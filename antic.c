/* ANTIC emulation -------------------------------- */
/* Original Author:                                 */
/*              David Firth                         */
/* Correct timing, internal memory and other fixes: */
/*              Piotr Fusik <pfusik@elka.pw.edu.pl> */
/* Last changes: 2nd April 2000                     */
/* ------------------------------------------------ */

#include <stdio.h>
#include <string.h>

#define LCHOP 3			/* do not build lefmost 0..3 characters in wide mode */
#define RCHOP 3			/* do not build rightmost 0..3 characters in wide mode */

#ifdef WIN32
#include <windows.h>
#include "winatari.h"
#else
#include "config.h"
#endif

#include "atari.h"
#include "rt-config.h"
#include "cpu.h"
#include "memory.h"
#include "gtia.h"
#include "antic.h"
#include "pokey.h"
#include "log.h"
#include "statesav.h"
#include "platform.h"

/* ANTIC Registers --------------------------------------------------------- */

UBYTE DMACTL;
UBYTE CHACTL;
UWORD dlist;
UBYTE HSCROL;
UBYTE VSCROL;
UBYTE PMBASE;
UBYTE CHBASE;
UBYTE NMIEN;
UBYTE NMIST;

/* ANTIC Memory ------------------------------------------------------------ */

UBYTE ANTIC_memory[52];
#define ANTIC_margin 4
/* It's number of bytes in ANTIC_memory, which are newer loaded, but may be
   read in wide playfield mode. These bytes are uninitialized, because on
   real computer there's some kind of 'garbage'. Possibly 1 is enough, but
   4 bytes surely won't cause negative indexes. :) */

/* Screen -----------------------------------------------------------------
   Define screen as ULONG to ensure that it is Longword aligned.
   This allows special optimisations under certain conditions.
   ------------------------------------------------------------------------ */
/* Possibly we no longer need 16 extra lines */

ULONG *atari_screen = NULL;
#ifdef BITPL_SCR
ULONG *atari_screen_b = NULL;
ULONG *atari_screen1 = NULL;
ULONG *atari_screen2 = NULL;
#endif
UBYTE *scrn_ptr;

/* ANTIC Timing --------------------------------------------------------------

I've introduced global variable xpos, which contains current number of cycle
in a line. This simplifies ANTIC/CPU timing much. The GO() function which
emulates CPU is now void and is called with xpos limit, below which CPU can go.

All strange variables holding 'unused cycles', 'DMA cycles', 'allocated cycles'
etc. are removed. Simply whenever ANTIC fetches a byte, it takes single cycle,
which can be done now with xpos++. There's only one exception: in text modes
2-5 ANTIC takes more bytes than cycles, probably fetching bytes in DMAR cycles.

Now emulation is really screenline-oriented. We do ypos++ after a line,
not inside it.

This diagram shows when what is done in a line:

MDPPPPDD..............(------R/S/F------)..........
^  ^     ^      ^     ^                     ^    ^ ^        ---> time/xpos
0  |  NMIST_C NMI_C SCR_C                 WSYNC_C|LINE_C
VSCON_C                                        VSCOF_C

M - fetch Missiles
D - fetch DL
P - fetch Players
S - fetch Screen
F - fetch Font (in text modes)
R - refresh Memory (DMAR cycles)

Only Memory Refresh happens in every line, other tasks are optional.

I think some simplifications can be made:

* There's no need to emulate exact cycle in which a byte of screen/font is
  taken. Actually I haven't even tested it.
  That's why I've drawn (--R/S/F--). :)
  It represents a range, when refresh and Screen/Font fetches are done.

* I've tested exact moments when DL and P/MG are fetched. As you see it is:
  - a byte of Missiles
  - a byte of DL (instruction)
  - four bytes of Players
  - two bytes of DL argument (jump or screen address)
  I think it is not necessary to emulate it 100%, we can fetch things we
  need continuosly at the beginning of the line.

There are few constants representing following events:

* VSCON_C - in first VSC line dctr is loaded with VSCROL

* NMIST_C - NMIST is updated (set to 0x9f on DLI, set to 0x5f on VBLKI)

* NMI_C - If NMIEN permits, NMI interrupt is generated

* SCR_C - We draw whole line of screen. On a real computer you can change
  ANTIC/GTIA registers while displaying screen, however this emulator
  isn't that accurate.

* WSYNC_C - ANTIC holds CPU until this moment, when WSYNC is written

* VSCOF_C - in last VSC line dctr is compared with VSCROL

* LINE_C - simply end of line (this used to be called CPUL)

All constants are determined by tests on real Atari computer. It is assumed,
that ANTIC registers are read with LDA, LDX, LDY and written with STA, STX,
STY, all in absolute addressing mode. All these instructions last 4 cycles
and perform read/write operation in last cycle. The CPU emulation should
correctly emulate WSYNC and add cycles for current instruction BEFORE
executing it. That's why VSCOF_C > LINE_C is correct.

How WSYNC is now implemented:

* On writing WSYNC:
  - if xpos <= WSYNC_C && xpos_limit >= WSYNC_C,
    we only change xpos to WSYNC_C - that's all
  - otherwise we set wsync_halt and change xpos to xpos_limit causing GO()
    to return

* At the beginning of GO() (CPU emulation), when wsync_halt is set:
  - if xpos_limit < WSYNC_C we return
  - else we set xpos to WSYNC_C, reset wsync_halt and emulate some cycles

We don't emulate NMIST_C, NMI_C and SCR_C if it is unnecessary.
These are all cases:

* Common overscreen line
  Nothing happens except that ANTIC gets DMAR cycles:
  xpos += DMAR; GOEOL;

* First overscreen line - start of vertical blank
  - CPU goes until NMIST_C
  - ANTIC sets NMIST to 0x5f
  if (NMIEN & 0x40) {
	  - CPU goes until NMI_C
	  - ANTIC forces NMI
  }
  - ANTIC gets DMAR cycles
  - CPU goes until LINE_C

* Screen line without DLI
  - ANTIC fetches DL and P/MG
  - CPU goes until SCR_C
  - ANTIC draws whole line fetching Screen/Font and refreshing memory
  - CPU goes until LINE_C

* Screen line with DLI
  - ANTIC fetches DL and P/MG
  - CPU goes until NMIST_C
  - ANTIC sets NMIST to 0x9f
  if (NMIEN & 0x80) {
	  - CPU goes until NMI_C
	  - ANTIC forces NMI
  }
  - CPU goes until SCR_C
  - ANTIC draws line with DMAR
  - CPU goes until LINE_C

  -------------------------------------------------------------------------- */

#define VSCON_C	5
#define NMIST_C	10
#define	NMI_C	16
#define	SCR_C	32
#define WSYNC_C	110	/* defined also in atari.h */
#define LINE_C	114	/* defined also in atari.h */
#define VSCOF_C	116

#define DMAR	9

#ifndef NO_VOL_ONLY
unsigned int screenline_cpu_clock = 0;
#define GOEOL	GO(LINE_C); xpos -= LINE_C; screenline_cpu_clock += LINE_C; ypos++
#else
#define GOEOL	GO(LINE_C); xpos -= LINE_C; ypos++
#endif
#define OVERSCREEN_LINE	xpos += DMAR; GOEOL

int xpos = 0;
int xpos_limit;
UBYTE wsync_halt = FALSE;

int ypos;						/* Line number - lines 8..247 are on screen */

/* Timing in first line of modes 2-5
In these modes ANTIC takes more bytes than cycles. Despite this, it would be
possible that SCR_C + cycles_taken > WSYNC_C. To avoid this we must take some
cycles before SCR_C. before_cycles contains number of them, while extra_cycles
contains difference between bytes taken and cycles taken plus before_cycles. */

#define BEFORE_CYCLES (SCR_C - 32)
/* It's number of cycles taken before SCR_C for not scrolled, narrow playfield.
   It wasn't tested, but should be ok. ;) */

/* Internal ANTIC registers ------------------------------------------------ */

static UBYTE IR;				/* Instruction Register */
static UWORD screenaddr;		/* Screen Pointer */
static int dctr;				/* Delta Counter */
static int lastline;			/* dctr limit */
static UBYTE anticmode;			/* Antic mode */
static UBYTE need_dl;			/* 0x20: fetch DL next line, 0x00: don't */
static UBYTE vscrol_off;		/* boolean: displaying line ending VSC */

/* Pre-computed values for improved performance ---------------------------- */

#define NORMAL0 0				/* modes 2,3,4,5,0xd,0xe,0xf */
#define NORMAL1 1				/* modes 6,7,0xa,0xb,0xc */
#define NORMAL2 2				/* modes 8,9 */
#define SCROLL0 3				/* modes 2,3,4,5,0xd,0xe,0xf with HSC */
#define SCROLL1 4				/* modes 6,7,0xa,0xb,0xc with HSC */
#define SCROLL2 5				/* modes 8,9 with HSC */
static int md;					/* current mode NORMAL0..SCROLL2 */
/* tables for modes NORMAL0..SCROLL2 */
static int chars_read[6];
static int chars_displayed[6];
static int x_min[6];
static int ch_offset[6];
static int load_cycles[6];
static int font_cycles[6];
static int before_cycles[6];
static int extra_cycles[6];

/* border parameters for current display width */
static int left_border_chars;
static int right_border_chars;
static int right_border_start;

/* set with CHBASE */
static UWORD chbase_40;			/* CHBASE for 40 character mode */
static UWORD chbase_20;			/* CHBASE for 20 character mode */

/* set with CHACTL */
static UBYTE flip_mask;
static UBYTE invert_mask;
static UBYTE blank_mask;

/* lookup tables */
static UBYTE blank_lookup[256];
static UBYTE lookup1[256];
static UWORD lookup2[256];
static ULONG lookup_gtia[16];
static UBYTE playfield_lookup[256];

extern UBYTE pf_colls[9];		/* playfield collisions */

/*
 * These are defined for Word (2 Byte) memory accesses. I have not
 * defined Longword (4 Byte) values since the Sparc architecture
 * requires that Longword are on a longword boundry.
 *
 * Words accesses don't appear to be a problem because the first
 * pixel plotted on a line will always be at an even offset, and
 * hence on a word boundry.
 *
 * Note: HSCROL is in colour clocks whereas the pixels are emulated
 *       down to a half colour clock - the first pixel plotted is
 *       moved across by 2*HSCROL
 */
UWORD cl_word[24];

/* Player/Missile Graphics ------------------------------------------------- */

static UBYTE singleline;
UBYTE player_dma_enabled;
UBYTE player_gra_enabled;
UBYTE missile_dma_enabled;
UBYTE missile_gra_enabled;
UBYTE player_flickering;
UBYTE missile_flickering;

static UWORD maddr_s;			/* Address of Missiles - Single Line Resolution */
static UWORD p0addr_s;			/* Address of Player0 - Single Line Resolution */
static UWORD p1addr_s;			/* Address of Player1 - Single Line Resolution */
static UWORD p2addr_s;			/* Address of Player2 - Single Line Resolution */
static UWORD p3addr_s;			/* Address of Player3 - Single Line Resolution */
static UWORD maddr_d;			/* Address of Missiles - Double Line Resolution */
static UWORD p0addr_d;			/* Address of Player0 - Double Line Resolution */
static UWORD p1addr_d;			/* Address of Player1 - Double Line Resolution */
static UWORD p2addr_d;			/* Address of Player2 - Double Line Resolution */
static UWORD p3addr_d;			/* Address of Player3 - Double Line Resolution */

extern UBYTE pm_scanline[ATARI_WIDTH];

#define L_PM0 (0*5-4)
#define L_PM1 (1*5-4)
#define L_PM01 (2*5-4)
#define L_PM2 (3*5-4)
#define L_PM3 (4*5-4)
#define L_PM23 (5*5-4)
#define L_PMNONE (12*5-4)
#define L_PM5PONLY (13*5-4) /*these should be the last two*/
#define NUM_PLAYER_TYPES 14
/*these defines also in gtia.c*/

signed char *new_pm_lookup;
signed char pm_lookup_normal[256];
signed char pm_lookup_multi[256];
signed char pm_lookup_5p[256];
signed char pm_lookup_multi_5p[256];

UBYTE prior_table[5*NUM_PLAYER_TYPES * 16];
UBYTE cur_prior[5*NUM_PLAYER_TYPES];
#define R_COLPM0OR1 9
#define R_COLPM2OR3 10
#define R_COLPM0_OR_PF0 11
#define R_COLPM1_OR_PF0 12
#define R_COLPM0OR1_OR_PF0 13
#define R_COLPM0_OR_PF1 14
#define R_COLPM1_OR_PF1 15
#define R_COLPM0OR1_OR_PF1 16
#define R_COLPM2_OR_PF2 17
#define R_COLPM3_OR_PF2 18
#define R_COLPM2OR3_OR_PF2 19
#define R_COLPM2_OR_PF3 20
#define R_COLPM3_OR_PF3 21
#define R_COLPM2OR3_OR_PF3 22
#define R_BLACK 23

void initialize_prior_table(void)
{
	static UBYTE player_colreg[] =
	{0, 1, 9, 2, 3, 10, 0};
	int i, j, jj, k, kk;
	for (i = 0; i <= 15; i++) {	/* prior setting */

		for (jj = 0; jj < NUM_PLAYER_TYPES; jj++) {	/* player */

			for (kk = 0; kk < 5; kk++) {	/* playfield */
				int c = 0;					/* colreg */
				k = kk;
				j = jj;
				if (jj == 13)
					c = 3 + 4;				/*5th player by itself*/
				else if (jj == 12)
					c = k + 4;				/* no player */
				else {
					if (jj >= 6 && jj <= 11) {
						k = 3; 				/*5th player with other players*/
						j = jj - 6;			/*other player that is with it*/
					}
					if (k == 4)
						c = player_colreg[j];
					else if (k <= 1) {		/* playfields 0 and 1 */

						if (j <= 2) {	/* players01 and 2=0+1 */

							switch (i) {
							case 0:
								if (k == 0) {
									if (j == 0)
										c = R_COLPM0_OR_PF0;
									if (j == 1)
										c = R_COLPM1_OR_PF0;
									if (j == 2)
										c = R_COLPM0OR1_OR_PF0;
								}
								else {
									if (j == 0)
										c = R_COLPM0_OR_PF1;
									if (j == 1)
										c = R_COLPM1_OR_PF1;
									if (j == 2)
										c = R_COLPM0OR1_OR_PF1;
								}
								break;
							case 1:
							case 2:
							case 3:
								c = player_colreg[j];
								break;
							case 4:
							case 8:
							case 12:
								c = k + 4;
								break;
							default:
								c = R_BLACK;
								break;
							}
						}
						else {	/* pf01, players23 and 2OR3 */

							if ((i & 0x01) == 0)
								c = k + 4;
							else
								c = player_colreg[j];
						}
					}
					else {		/* playfields 2 and 3 */

						if (j <= 2) {	/* players 01 and 0OR1 */

							if ((i <= 3) || (i >= 8 && i <= 11))
								c = player_colreg[j];
							else
								c = k + 4;
						}
						else {	/* pf23, players23 and 2OR3 */

							switch (i) {
							case 0:
								if (k == 2) {
									if (j == 3)
										c = R_COLPM2_OR_PF2;
									if (j == 4)
										c = R_COLPM3_OR_PF2;
									if (j == 5)
										c = R_COLPM2OR3_OR_PF2;
								}
								else {
									if (j == 3)
										c = R_COLPM2_OR_PF3;
									if (j == 4)
										c = R_COLPM3_OR_PF3;
									if (j == 5)
										c = R_COLPM2OR3_OR_PF3;
								}
								break;
							case 1:
							case 8:
							case 9:
								c = player_colreg[j];
								break;
							case 2:
							case 4:
							case 6:
								c = k + 4;
								break;
							default:
								c = R_BLACK;
								break;
							}
						}
					}
				}
				prior_table[i * NUM_PLAYER_TYPES * 5 + jj * 5 + kk] = c;
			}
		}
	}
}

void init_pm_lookup(void)
{
	static signed char old_new_pm_lookup[16] = {
		L_PMNONE,	/* 0000 - None */
		L_PM0,		/* 0001 - Player 0 */
		L_PM1,		/* 0010 - Player 1 */
		L_PM0,		/* 0011 - Player 0 */
		L_PM2,		/* 0100 - Player 2 */
		L_PM0,		/* 0101 - Player 0 */
		L_PM1,		/* 0110 - Player 1 */
		L_PM0,		/* 0111 - Player 0 */
		L_PM3,		/* 1000 - Player 3 */
		L_PM0,		/* 1001 - Player 0 */
		L_PM1,		/* 1010 - Player 1 */
		L_PM0,		/* 1011 - Player 0 */
		L_PM2,		/* 1100 - Player 2 */
		L_PM0,		/* 1101 - Player 0 */
		L_PM1,		/* 1110 - Player 1 */
		L_PM0,		/* 1111 - Player 0 */
	};
	static signed char old_new_pm_lookup_multi[16] = {
		L_PMNONE,	/* 0000 - None */
		L_PM0,		/* 0001 - Player 0 */
		L_PM1,		/* 0010 - Player 1 */
		L_PM01,		/* 0011 - Player 0 OR 1 */
		L_PM2,		/* 0100 - Player 2 */
		L_PM0,		/* 0101 - Player 0 */
		L_PM1,		/* 0110 - Player 1 */
		L_PM01,		/* 0111 - Player 0 OR 1 */
		L_PM3,		/* 1000 - Player 3 */
		L_PM0,		/* 1001 - Player 0 */
		L_PM1,		/* 1010 - Player 1 */
		L_PM01,		/* 1011 - Player 0 OR 1 */
		L_PM23,		/* 1100 - Player 2 OR 3 */
		L_PM0,		/* 1101 - Player 0 */
		L_PM1,		/* 1110 - Player 1 */
		L_PM01,		/* 1111 - Player 0 OR 1 */
	};

	int i;
	int pm;
	for(i = 0; i <= 255; i++) {
		pm = (i & 0x0f) | (i >> 4);
		pm_lookup_normal[i] = old_new_pm_lookup[pm];
		pm_lookup_multi[i] = old_new_pm_lookup_multi[pm];
		pm = i & 0x0f;
		pm_lookup_5p[i] = old_new_pm_lookup[pm];
		pm_lookup_multi_5p[i] = old_new_pm_lookup_multi[pm];
		if (i > 15) { /* 5th player adds 6*5 */
			if (pm) {
				pm_lookup_5p[i] += 6*5;
				pm_lookup_multi_5p[i] += 6*5;
			}
			else {
				pm_lookup_5p[i] = L_PM5PONLY;
				pm_lookup_multi_5p[i] = L_PM5PONLY;
			}
		}
	}
}

void pmg_dma(void)
{
	static UBYTE hold_missiles_tab[16] = {
		0x00,0x03,0x0c,0x0f,0x30,0x33,0x3c,0x3f,
		0xc0,0xc3,0xcc,0xcf,0xf0,0xf3,0xfc,0xff};
	if (player_dma_enabled) {
		if (player_gra_enabled) {
			if (singleline) {
				if (ypos & 1) {
					GRAFP0 = dGetByte(p0addr_s + ypos);
					GRAFP1 = dGetByte(p1addr_s + ypos);
					GRAFP2 = dGetByte(p2addr_s + ypos);
					GRAFP3 = dGetByte(p3addr_s + ypos);
				}
				else {
					if ( (VDELAY&0x10)==0 )
						GRAFP0 = dGetByte(p0addr_s + ypos);
					if ( (VDELAY&0x20)==0 )
						GRAFP1 = dGetByte(p1addr_s + ypos);
					if ( (VDELAY&0x40)==0 )
						GRAFP2 = dGetByte(p2addr_s + ypos);
					if ( (VDELAY&0x80)==0 )
						GRAFP3 = dGetByte(p3addr_s + ypos);
				}
			}
			else {
				if (ypos & 1) {
					GRAFP0 = dGetByte(p0addr_d + (ypos >> 1));
					GRAFP1 = dGetByte(p1addr_d + (ypos >> 1));
					GRAFP2 = dGetByte(p2addr_d + (ypos >> 1));
					GRAFP3 = dGetByte(p3addr_d + (ypos >> 1));
				}
				else {
					if ( (VDELAY&0x10)==0 )
						GRAFP0 = dGetByte(p0addr_d + (ypos >> 1));
					if ( (VDELAY&0x20)==0 )
						GRAFP1 = dGetByte(p1addr_d + (ypos >> 1));
					if ( (VDELAY&0x40)==0 )
						GRAFP2 = dGetByte(p2addr_d + (ypos >> 1));
					if ( (VDELAY&0x80)==0 )
						GRAFP3 = dGetByte(p3addr_d + (ypos >> 1));
				}
			}
		}
		xpos += 4;
	}
	if (missile_dma_enabled) {
		if (missile_gra_enabled) {
			UBYTE hold_missiles;
			hold_missiles = ypos & 1 ? 0 : hold_missiles_tab[VDELAY&0xf];
			GRAFM = (dGetByte(singleline ? maddr_s + ypos : maddr_d + (ypos >> 1)) & ~hold_missiles )
					| (GRAFM & hold_missiles);
		}
		xpos++;
	}
}

/* Artifacting ------------------------------------------------------------ */

int global_artif_mode;

static ULONG art_lookup_normal[256];
static ULONG art_lookup_reverse[256];
static ULONG art_bkmask_normal[256];
static ULONG art_lummask_normal[256];
static ULONG art_bkmask_reverse[256];
static ULONG art_lummask_reverse[256];

UBYTE art_redgreen_table[] =
{
/* ART_BROWN */
	13 * 16 + 6,
/* ART_DARK_BROWN */
	13 * 16 + 6,
/* ART_BLUE */
	4 * 16 + 6,
/* ART_DARK_BLUE */
	4 * 16 + 6,
/* ART_BRIGHT_BROWN */
	13 * 16 + 15,
/* ART_RED */
	4 * 16 + 15,
/* ART_BRIGHT_BLUE */
	4 * 16 + 10,
/* ART_GREEN */
	10 * 16 + 12,
};

/* I'm trying to create a table here for CTIA - some older games use this,
   basically like the redgreen table but with everything one color clock out
   of phase. This is just a guess, feel free to correct if necessary - REL	*/
UBYTE art_greenred_table[] =
{
/* ART_BLUE */
	4 * 16 + 6,
/* ART_DARK_BLUE */
	4 * 16 + 6,
/* ART_BROWN */
	13 * 16 + 6,
/* ART_DARK_BROWN */
	13 * 16 + 6,
/* ART_BRIGHT_BLUE */
	4 * 16 + 10,
/* ART_GREEN */
	10 * 16 + 12,
/* ART_BRIGHT_BROWN */
	13 * 16 + 15,
/* ART_RED */
	4 * 16 + 15,
};

UBYTE art_bluebrown_table[] =
{
/* ART_BROWN */
	1 * 16 + 4,
/* ART_DARK_BROWN */
	1 * 16 + 4,
/* ART_BLUE */
	8 * 16 + 8,
/* ART_DARK_BLUE */
	8 * 16 + 8,
/* ART_BRIGHT_BROWN */
	1 * 16 + 15,
/* ART_RED */
	5 * 16 + 15,
/* ART_BRIGHT_BLUE */
	8 * 16 + 15,
/* ART_GREEN */
	11 * 16 + 11,
};

static void setup_art_colours(void);
static ULONG art_normal_colpf1_save, art_normal_colpf2_save;
static ULONG art_reverse_colpf1_save, art_reverse_colpf2_save;

void art_initialise(int gtia_flag, int init_flag, UBYTE * art_colour_table)
{
#define ART_GTIA gtia_flag
#define ART_BROWN art_colour_table[0]
#define ART_DARK_BROWN art_colour_table[1]
#define ART_BLUE art_colour_table[2]
#define ART_DARK_BLUE art_colour_table[3]
#define ART_BRIGHT_BROWN art_colour_table[4]
#define ART_RED art_colour_table[5]
#define ART_BRIGHT_BLUE art_colour_table[6]
#define ART_GREEN art_colour_table[7]
#define ART_BLACK 0
#define ART_WHITE 0

	int i;
	int j;
	int q;
	int c;
	int mask_state;
	int art_black, art_white;
	if (init_flag == 1) {
		art_black = 0;
		art_white = 0;
		art_normal_colpf1_save = 0;
		art_normal_colpf1_save = 0;
		art_reverse_colpf2_save = 0;
		art_reverse_colpf2_save = 0;
	}
	else {
		art_black = COLPF2;
		art_white = ((COLPF2 & 0xf0) | (COLPF1 & 0x0f));
		art_normal_colpf1_save = ((ULONG) cl_word[5] | (((ULONG) cl_word[5]) << 16));
		art_normal_colpf2_save = ((ULONG) cl_word[6] | (((ULONG) cl_word[6]) << 16));
		art_reverse_colpf1_save = ((ULONG) cl_word[5] | (((ULONG) cl_word[5]) << 16));
		art_reverse_colpf2_save = ((ULONG) cl_word[6] | (((ULONG) cl_word[6]) << 16));
	}

	for (i = 0; i <= 255; i++) {
		art_lookup_normal[i] = 0;
		art_lookup_reverse[255 - i] = 0;
		art_bkmask_normal[i] = 0;
		art_lummask_normal[i] = 0;
		art_bkmask_reverse[255 - i] = 0;
		art_lummask_reverse[255 - i] = 0;

		for (j = 0; j <= 3; j++) {
			mask_state = 0;
			q = (i << j);
			if (!(q & 0x20)) {	/*not on */
				if ((q & 0xF8) == 0x50)
					c = ((j + ART_GTIA) % 2 ? ART_BROWN : ART_BLUE);
				else if ((q & 0xF8) == 0xD8)
					c = ((j + ART_GTIA) % 2 ? ART_DARK_BROWN : ART_DARK_BLUE);
				else {
					c = ART_BLACK;
					mask_state = 1;
				}
			}
/*on */
			else if (q & 0x40) {	/*left on */
				if (q & 0x10) {	/*right on as well */
					if ((q & 0xF8) == 0x70) {
						c = ART_WHITE;	/* ((j+ART_GTIA)%2 ? ART_BRIGHT_RED : ART_BRIGHT_BLUE); */
						mask_state = 2;
					}
/*^^^ middle three on only or */
					else {
						c = ART_WHITE;	/* more than middle three, so white */
						mask_state = 2;
					}
				}				/*right not on */
				else if (q & 0x80) {	/*far left on */
					if (q & 0x08)
						c = ((j + ART_GTIA) % 2 ? ART_BRIGHT_BLUE : ART_BRIGHT_BROWN);
					else {
						c = ART_WHITE;	/*adjust if far right on ***.*  */
						mask_state = 2;
					}
				}
				else
					c = ((j + ART_GTIA) % 2 ? ART_RED : ART_GREEN);		/*left only on */
			}
			else if (q & 0x10) {	/*right on, left not on */
				if (q & 0x08) {	/*far right on */
					if (q & 0x80)
						c = ((j + ART_GTIA) % 2 ? ART_BRIGHT_BLUE : ART_BRIGHT_BROWN);
					else {
						c = ART_WHITE;	/*adjust if far left on *.***  */
						mask_state = 2;
					}
				}
				else
					c = ((j + ART_GTIA) % 2 ? ART_GREEN : ART_RED);		/*right only on */
			}
			else
				c = ((j + ART_GTIA) % 2 ? ART_BLUE : ART_BROWN);	/*right and left both off */

			art_lookup_normal[i] |= (c << (j * 8));
			art_lookup_reverse[255 - i] |= (c << (j * 8));

			if (mask_state == 1) {	/* black for normal white for reverse */
				art_lookup_normal[i] |= (art_black << (j * 8));
				art_lookup_reverse[255 - i] |= (art_white << (j * 8));
				art_bkmask_normal[i] |= (0xff << (j * 8));
				art_lummask_reverse[255 - i] |= (0x0f << (j * 8));
				art_bkmask_reverse[255 - i] |= (0xf0 << (j * 8));
			}
			if (mask_state == 2) {	/*white for normal, black for reverse */
				art_lookup_normal[i] |= (art_white << (j * 8));
				art_lookup_reverse[255 - i] |= (art_black << (j * 8));
				art_bkmask_reverse[255 - i] |= (0xff << (j * 8));
				art_lummask_normal[i] |= (0x0f << (j * 8));
				art_bkmask_normal[i] |= (0xf0 << (j * 8));
			}
		}
	}
	if (init_flag == 1) {
		cl_word[5] = 00;
		cl_word[6] = 0xffff;	/* something different */
		COLPF1 = 0x00;
		COLPF2 = 0xff;
		setup_art_colours();
	}
}

ULONG *art_curtable;
ULONG *art_curlummask, *art_curbkmask;

static void setup_art_colours(void)
{
	static ULONG *art_colpf1_save = &art_normal_colpf1_save,
				*art_colpf2_save = &art_normal_colpf2_save;
	UBYTE curlum;

	/* This prevents seg faults with the windows version, which may adjust artifacting on the fly,
	   which can result in art_curtable = NULL at bad times */
	art_curtable = art_lookup_normal;

	curlum = (COLPF1 & 0x0e);
	if (cl_word[6] != *art_colpf2_save || curlum != (0x0e & *art_colpf1_save)) {
		if (curlum < (COLPF2 & 0x0e)) {
			art_colpf1_save = &art_reverse_colpf1_save;
			art_colpf2_save = &art_reverse_colpf2_save;
			art_curtable = art_lookup_reverse;
			art_curlummask = art_lummask_reverse;
			art_curbkmask = art_bkmask_reverse;
		}
		else {
			art_colpf1_save = &art_normal_colpf1_save;
			art_colpf2_save = &art_normal_colpf2_save;
			art_curtable = art_lookup_normal;
			art_curlummask = art_lummask_normal;
			art_curbkmask = art_bkmask_normal;
		}
		if (curlum != ((*art_colpf1_save) & 0x0e)) {
			ULONG temp_new_colour = ((ULONG) cl_word[5] | (((ULONG) cl_word[5]) << 16));
			ULONG new_colour = temp_new_colour ^ *art_colpf1_save;
			int i;
			*art_colpf1_save = temp_new_colour;
			for (i = 0; i <= 255; i++) {
				art_curtable[i] ^= art_curlummask[i] & new_colour;
			}
		}
		if ((COLPF2) != ((*art_colpf2_save) & 0xff)) {
			ULONG temp_new_colour = ((ULONG) cl_word[6] | (((ULONG) cl_word[6]) << 16));
			ULONG new_colour = temp_new_colour ^ *art_colpf2_save;
			int i;
			*art_colpf2_save = temp_new_colour;
			for (i = 0; i <= 255; i++) {
				art_curtable[i] ^= art_curbkmask[i] & new_colour;
			}
		}

	}
}

void art_main_init(int init_flag, int mode)
{
	global_artif_mode = mode;
	switch (mode) {
	case 0:
		break;
	case 1:
		art_initialise(1, init_flag, art_bluebrown_table);
		break;
	case 2:
		art_initialise(0, init_flag, art_bluebrown_table);
		break;
	case 3:
		art_initialise(0, init_flag, art_redgreen_table);
		break;
	case 4:
		art_initialise(0, init_flag, art_greenred_table);
		break;
	default:
		art_initialise(0, init_flag, art_redgreen_table);
		break;
	}
}

/* Initialization ---------------------------------------------------------- */

void ANTIC_Initialise(int *argc, char *argv[])
{
	int i, j;
#ifdef WIN32
	BOOL	bInit = FALSE;
#else
	art_main_init(0, 0);
#endif
	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-artif") == 0) {
			int artif_mode;
			artif_mode = argv[++i][0]-'0';
#ifdef WIN32
			bInit = TRUE;
#endif
			if (artif_mode < 0 || artif_mode > 4) {
				Aprint("Invalid artifacting mode, using default.");
				artif_mode = 0;
			}
			art_main_init(0, artif_mode);
		}
		else
			argv[j++] = argv[i];
	}
	*argc = j;

#ifdef WIN32
	if( !bInit )
		art_main_init( 0, global_artif_mode );
#endif

	playfield_lookup[0x00] = 8;
	playfield_lookup[0x40] = 4;
	playfield_lookup[0x80] = 5;
	playfield_lookup[0xc0] = 6;
	blank_lookup[0x80] = blank_lookup[0xa0] = blank_lookup[0xc0] = blank_lookup[0xe0] = 0x00;
	initialize_prior_table();
	init_pm_lookup();
	GTIA_PutByte(_PRIOR, 0xff);
	GTIA_PutByte(_PRIOR, 0x00);

	player_dma_enabled = missile_dma_enabled = 0;
	player_gra_enabled = missile_gra_enabled = 0;
	player_flickering = missile_flickering = 0;
	GRAFP0 = GRAFP1 = GRAFP2 = GRAFP3 = GRAFM = 0;
}

void ANTIC_Reset(void) {
	NMIEN = 0x00;
	NMIST = 0x3f;	/* Probably bit 5 is Reset flag */
	ANTIC_PutByte(_DMACTL, 0);
}

/* Border ------------------------------------------------------------------ */

#define DO_PMG pf_colls[colreg]|=pm_pixel;\
				colreg=cur_prior[new_pm_lookup[pm_pixel]+colreg];

#define DO_BORDER_CHAR	if (!(*t_pm_scanline_ptr)) {\
						ULONG *l_ptr = (ULONG *) ptr;\
						*l_ptr++ = COL_8_LONG;\
						*l_ptr++ = COL_8_LONG;\
						ptr = (UWORD *) l_ptr;\
					} else

#define DO_GTIA10_BORDER P0PL |= pm_pixel = *c_pm_scanline_ptr++;\
						 *ptr++ = cl_word[cur_prior[new_pm_lookup[pm_pixel|1]+8]];
				
#define BAK_PMG cur_prior[new_pm_lookup[*c_pm_scanline_ptr++]+8]

void do_border(int left_border_chars, int right_border_chars)
{
	int kk;
	UWORD *ptr = (UWORD *) & scrn_ptr[LCHOP * 8];
	ULONG *t_pm_scanline_ptr = (ULONG *) (&pm_scanline[LCHOP * 4]);
	ULONG COL_8_LONG;

	switch (PRIOR & 0xc0) {		/* ERU sux */
	case 0x80:	/* GTIA mode 10 */
		COL_8_LONG = cl_word[0] | (cl_word[0] << 16);
		/* left border */
		for (kk = left_border_chars; kk; kk--) {
			DO_BORDER_CHAR {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				UBYTE pm_pixel;
				int k;
				for (k = 1; k <= 4; k++) {
					DO_GTIA10_BORDER
				}
			}
			t_pm_scanline_ptr++;
		}
		/* right border */
		if ( (kk = right_border_chars)!=0 ) {
			UBYTE *c_pm_scanline_ptr = & pm_scanline[(right_border_start >> 1) + 1];
			UBYTE pm_pixel;
			ptr = (UWORD *) & scrn_ptr[right_border_start+2];
			DO_GTIA10_BORDER
			DO_GTIA10_BORDER
			DO_GTIA10_BORDER
			t_pm_scanline_ptr = (ULONG *) c_pm_scanline_ptr;
			while(--kk) {
				DO_BORDER_CHAR {
					UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
					UBYTE pm_pixel;
					int k;
					for (k = 1; k <= 4; k++) {
						DO_GTIA10_BORDER
					}
				}
				t_pm_scanline_ptr++;
			}
		}
		break;
	case 0xc0:	/* GTIA mode 11 */
		COL_8_LONG = ( cl_word[8] | (cl_word[8] << 16) ) & 0xf0f0f0f0;
		/* left border */
		for (kk = left_border_chars; kk; kk--) {
			DO_BORDER_CHAR {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				UBYTE colreg;
				int k;
				for (k = 1; k <= 4; k++) {
					colreg = BAK_PMG;
					*ptr++ = colreg == 7 || colreg == 8 ? cl_word[colreg]&0xf0f0 : cl_word[colreg];
				}
			}
			t_pm_scanline_ptr++;
		}
		/* right border */
		ptr = (UWORD *) & scrn_ptr[right_border_start];
		t_pm_scanline_ptr = (ULONG *) (&pm_scanline[(right_border_start >> 1)]);
		for (kk = right_border_chars; kk; kk--) {
			DO_BORDER_CHAR {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				UBYTE colreg;
				int k;
				for (k = 1; k <= 4; k++) {
					colreg = BAK_PMG;
					*ptr++ = colreg == 7 || colreg == 8 ? cl_word[colreg]&0xf0f0 : cl_word[colreg];
				}
			}
			t_pm_scanline_ptr++;
		}
		break;
	default:	/* normal and GTIA mode 9 */
		COL_8_LONG = cl_word[8] | (cl_word[8] << 16);
		/* left border */
		for (kk = left_border_chars; kk; kk--) {
			DO_BORDER_CHAR {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				int k;
				for (k = 1; k <= 4; k++)
					*ptr++ = cl_word[BAK_PMG];
			}
			t_pm_scanline_ptr++;
		}
		/* right border */
		ptr = (UWORD *) & scrn_ptr[right_border_start];
		t_pm_scanline_ptr = (ULONG *) (&pm_scanline[(right_border_start >> 1)]);
		for (kk = right_border_chars; kk; kk--) {
			DO_BORDER_CHAR {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				int k;
				for (k = 1; k <= 4; k++)
					*ptr++ = cl_word[BAK_PMG];
			}
			t_pm_scanline_ptr++;
		}
		break;
	}
}

/* ANTIC modes ------------------------------------------------------------- */

void draw_antic_2(int nchars, UBYTE *ANTIC_memptr, UBYTE *ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
#ifdef UNALIGNED_LONG_OK
	ULONG COL_6_LONG = cl_word[6] | (cl_word[6] << 16);
#endif
	int i;
	lookup1[0x00] = colour_lookup[6];
	lookup1[0x80] = lookup1[0x40] = lookup1[0x20] =
		lookup1[0x10] = lookup1[0x08] = lookup1[0x04] =
		lookup1[0x02] = lookup1[0x01] = (colour_lookup[6] & 0xf0) | (colour_lookup[5] & 0x0f);
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		UBYTE chdata;

		chdata = (screendata & invert_mask) ? 0xff : 0;
		if (blank_lookup[screendata & blank_mask])
			chdata ^= dGetByte(t_chbase + ((UWORD) (screendata & 0x7f) << 3));
		if (!(*t_pm_scanline_ptr)) {
			if (chdata) {
				*ptr++ = lookup1[chdata & 0x80];
				*ptr++ = lookup1[chdata & 0x40];
				*ptr++ = lookup1[chdata & 0x20];
				*ptr++ = lookup1[chdata & 0x10];
				*ptr++ = lookup1[chdata & 0x08];
				*ptr++ = lookup1[chdata & 0x04];
				*ptr++ = lookup1[chdata & 0x02];
				*ptr++ = lookup1[chdata & 0x01];
			}
			else {
#ifdef UNALIGNED_LONG_OK
				ULONG *l_ptr = (ULONG *) ptr;

				*l_ptr++ = COL_6_LONG;
				*l_ptr++ = COL_6_LONG;

				ptr = (UBYTE *) l_ptr;
#else
				UWORD *w_ptr = (UWORD *) ptr;

				*w_ptr++ = cl_word[6];
				*w_ptr++ = cl_word[6];
				*w_ptr++ = cl_word[6];
				*w_ptr++ = cl_word[6];

				ptr = (UBYTE *) w_ptr;
#endif
			}
		}
		else {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			UBYTE pm_pixel;
			UBYTE colreg;
			int k;
			for (k = 1; k <= 4; k++) {
				pm_pixel = *c_pm_scanline_ptr++;
				colreg = 6;
				DO_PMG
				if (chdata & 0x80)
					*ptr++ = (cl_word[colreg] & 0xf0) | (COLPF1 & 0x0f);
				else
					*ptr++ = (UBYTE) cl_word[colreg];
				if (chdata & 0x40)
					*ptr++ = (cl_word[colreg] & 0xf0) | (COLPF1 & 0x0f);
				else
					*ptr++ = (UBYTE) cl_word[colreg];
				chdata <<= 2;
			}

		}
		t_pm_scanline_ptr++;
	}
}

void draw_antic_2_artif(int nchars, UBYTE *ANTIC_memptr, UBYTE *ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	int i;
	ULONG screendata_tally;
	UBYTE screendata = *ANTIC_memptr++;
	UBYTE chdata;
	chdata = (screendata & invert_mask) ? 0xff : 0;
	if (blank_lookup[screendata & blank_mask])
		chdata ^= dGetByte(t_chbase + ((UWORD) (screendata & 0x7f) << 3));
	screendata_tally=chdata;
	setup_art_colours();
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		UBYTE chdata;

		chdata = (screendata & invert_mask) ? 0xff : 0;
		if (blank_lookup[screendata & blank_mask])
			chdata ^= dGetByte(t_chbase + ((UWORD) (screendata & 0x7f) << 3));
		screendata_tally<<=8;
		screendata_tally|=chdata;
		if (!(*t_pm_scanline_ptr)) {
			*((ULONG*)ptr)=art_curtable[((UBYTE)(screendata_tally>>10))];
			*((ULONG*)(ptr+4))=art_curtable[((UBYTE)(screendata_tally>>6))];
			ptr+=8;
		}
		else {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			UBYTE pm_pixel;
			UBYTE colreg;
			int k;
   			chdata=(UBYTE)(screendata_tally>>8);
			for (k = 1; k <= 4; k++) {
				pm_pixel = *c_pm_scanline_ptr++;
				colreg = 6;
				DO_PMG
				if (chdata & 0x80)
					*ptr++ = (cl_word[colreg] & 0xf0) | (COLPF1 & 0x0f);
				else
					*ptr++ = (UBYTE)cl_word[colreg];
				if (chdata & 0x40)
					*ptr++ = (cl_word[colreg] & 0xf0) | (COLPF1 & 0x0f);
				else
					*ptr++ = (UBYTE)cl_word[colreg];
				chdata <<= 2;
			}
		}
		t_pm_scanline_ptr++;
	}
}

void draw_antic_2_gtia9_11(int nchars, UBYTE *ANTIC_memptr, char *t_ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	int i;
	ULONG *ptr = (ULONG *) (t_ptr);
	ULONG temp_count = 0;
	ULONG base_colour = cl_word[8] | (cl_word[8] << 16);
	ULONG increment;
	if (PRIOR & 0x80)
		increment = 0x10101010;
	else
		increment = 0x01010101;
	for (i = 0; i <= 15; i++) {
		lookup_gtia[i] = base_colour | temp_count;
		temp_count += increment;
	}

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		UBYTE chdata;

		chdata = (screendata & invert_mask) ? 0xff : 0;
		if (blank_lookup[screendata & blank_mask])
			chdata ^= dGetByte(t_chbase + ((UWORD) (screendata & 0x7f) << 3));
		*ptr++ = lookup_gtia[chdata >> 4];
		*ptr++ = lookup_gtia[chdata & 0x0f];
		if ((*t_pm_scanline_ptr)) {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			UBYTE pm_pixel;
			int k;
			UWORD *w_ptr = (UWORD *) (ptr - 2);
			for (k = 1; k <= 4; k++) {
				pm_pixel = *c_pm_scanline_ptr++;
				if (pm_pixel) {
					*w_ptr = cl_word[cur_prior[new_pm_lookup[pm_pixel]+8]];
				}
				w_ptr++;
			}
		}
		t_pm_scanline_ptr++;
	}
}

void draw_antic_4(int nchars, UBYTE *ANTIC_memptr, char *t_ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	int i;
	UWORD *ptr = (UWORD *) (t_ptr);

	lookup2[0x0f] = lookup2[0x00] = cl_word[8];
	lookup2[0x4f] = lookup2[0x1f] = lookup2[0x13] =
	lookup2[0x40] = lookup2[0x10] = lookup2[0x04] = lookup2[0x01] = cl_word[4];
	lookup2[0x8f] = lookup2[0x2f] = lookup2[0x17] = lookup2[0x11] =
	lookup2[0x80] = lookup2[0x20] = lookup2[0x08] = lookup2[0x02] = cl_word[5];
	lookup2[0xc0] = lookup2[0x30] = lookup2[0x0c] = lookup2[0x03] = cl_word[6];
	lookup2[0xcf] = lookup2[0x3f] = lookup2[0x1b] = lookup2[0x12] = cl_word[7];

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		UWORD *lookup;
		UBYTE chdata;
		if (screendata & 0x80)
			lookup = lookup2 + 0xf;
		else
			lookup = lookup2;
		chdata = dGetByte(t_chbase + ((UWORD) (screendata & 0x7f) << 3));
		if (!(*t_pm_scanline_ptr)) {
			if (chdata) {
				*ptr++ = lookup[chdata & 0xc0];
				*ptr++ = lookup[chdata & 0x30];
				*ptr++ = lookup[chdata & 0x0c];
				*ptr++ = lookup[chdata & 0x03];
			}
			else {
				*ptr++ = lookup2[0];
				*ptr++ = lookup2[0];
				*ptr++ = lookup2[0];
				*ptr++ = lookup2[0];
			}
		}
		else {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			UBYTE pm_pixel;
			UBYTE colreg;
			int k;
			for (k = 1; k <= 4; k++) {
				pm_pixel = *c_pm_scanline_ptr++;
				colreg = playfield_lookup[chdata & 0xc0];
				if ((screendata & 0x80) && (colreg == 6))
					colreg = 7;
				DO_PMG
				* ptr++ = cl_word[colreg];
				chdata <<= 2;
			}

		}
		t_pm_scanline_ptr++;
	}
}

void draw_antic_6(int nchars, UBYTE *ANTIC_memptr, char *t_ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	int i;
	UWORD *ptr = (UWORD *) (t_ptr);
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		UBYTE chdata;
		UWORD colour = 0;
		int kk;
		switch (screendata & 0xc0) {
		case 0x00:
			colour = cl_word[4];
			break;
		case 0x40:
			colour = cl_word[5];
			break;
		case 0x80:
			colour = cl_word[6];
			break;
		case 0xc0:
			colour = cl_word[7];
			break;
		}
		chdata = dGetByte(t_chbase + ((UWORD) (screendata & 0x3f) << 3));
		for (kk = 0; kk < 2; kk++) {
			if (!(*t_pm_scanline_ptr)) {
				if (chdata & 0xf0) {
					if (chdata & 0x80)
						*ptr++ = colour;
					else
						*ptr++ = cl_word[8];
					if (chdata & 0x40)
						*ptr++ = colour;
					else
						*ptr++ = cl_word[8];
					if (chdata & 0x20)
						*ptr++ = colour;
					else
						*ptr++ = cl_word[8];
					if (chdata & 0x10)
						*ptr++ = colour;
					else
						*ptr++ = cl_word[8];
				}
				else {
					*ptr++ = cl_word[8];
					*ptr++ = cl_word[8];
					*ptr++ = cl_word[8];
					*ptr++ = cl_word[8];
				}
				chdata = chdata << 4;
			}
			else {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				UBYTE pm_pixel;
				UBYTE colreg;
				int k;
				for (k = 1; k <= 4; k++) {
					pm_pixel = *c_pm_scanline_ptr++;
					colreg = (chdata & 0x80) ? ((screendata & 0xc0) >> 6) + 4 : 8;

					DO_PMG
						* ptr++ = cl_word[colreg];
					chdata <<= 1;
				}

			}
			t_pm_scanline_ptr++;
		}
	}
}

void draw_antic_8(int nchars, UBYTE *ANTIC_memptr, char *t_ptr, ULONG * t_pm_scanline_ptr)
{
	int i;
	UWORD *ptr = (UWORD *) (t_ptr);
	lookup2[0x00] = cl_word[8];
	lookup2[0x40] = cl_word[4];
	lookup2[0x80] = cl_word[5];
	lookup2[0xc0] = cl_word[6];
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		int kk;
		for (kk = 0; kk < 4; kk++) {
			if (!(*t_pm_scanline_ptr)) {
				*ptr++ = lookup2[screendata & 0xc0];
				*ptr++ = lookup2[screendata & 0xc0];
				*ptr++ = lookup2[screendata & 0xc0];
				*ptr++ = lookup2[screendata & 0xc0];
				screendata <<= 2;
			}
			else {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				UBYTE pm_pixel;
				UBYTE colreg;
				int k;
				for (k = 0; k <= 3; k++) {
					pm_pixel = *c_pm_scanline_ptr++;
					colreg = playfield_lookup[screendata & 0xc0];
					DO_PMG
					* ptr++ = cl_word[colreg];
				}
				screendata <<= 2;
			}
			t_pm_scanline_ptr++;
		}
	}
}

void draw_antic_9(int nchars, UBYTE *ANTIC_memptr, char *t_ptr, ULONG * t_pm_scanline_ptr)
{
	int i;
	UWORD *ptr = (UWORD *) (t_ptr);
	lookup2[0x00] = cl_word[8];
	lookup2[0x80] = lookup2[0x40] = cl_word[4];
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		int kk;
		for (kk = 0; kk < 4; kk++) {
			if (!(*t_pm_scanline_ptr)) {
				*ptr++ = lookup2[screendata & 0x80];
				*ptr++ = lookup2[screendata & 0x80];
				*ptr++ = lookup2[screendata & 0x40];
				*ptr++ = lookup2[screendata & 0x40];
				screendata <<= 2;
			}
			else {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				UBYTE pm_pixel;
				UBYTE colreg;
				int k;
				for (k = 0; k <= 3; k++) {
					pm_pixel = *c_pm_scanline_ptr++;
					colreg = (screendata & 0x80) ? 4 : 8;

					DO_PMG
					* ptr++ = cl_word[colreg];
					if (k & 0x01)
						screendata <<= 1;
				}
			}
			t_pm_scanline_ptr++;
		}
	}
}

void draw_antic_a(int nchars, UBYTE *ANTIC_memptr, char *t_ptr, ULONG * t_pm_scanline_ptr)
{
	int i;
	UWORD *ptr = (UWORD *) (t_ptr);
	lookup2[0x00] = cl_word[8];
	lookup2[0x40] = lookup2[0x10] = cl_word[4];
	lookup2[0x80] = lookup2[0x20] = cl_word[5];
	lookup2[0xc0] = lookup2[0x30] = cl_word[6];
		for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		int kk;
		for (kk = 0; kk < 2; kk++) {
			if (!(*t_pm_scanline_ptr)) {
				*ptr++ = lookup2[screendata & 0xc0];
				*ptr++ = lookup2[screendata & 0xc0];
				*ptr++ = lookup2[screendata & 0x30];
				*ptr++ = lookup2[screendata & 0x30];
				screendata <<= 4;
			}
			else {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				UBYTE pm_pixel;
				UBYTE colreg;
				int k;
				for (k = 0; k <= 3; k++) {
					pm_pixel = *c_pm_scanline_ptr++;
					colreg = playfield_lookup[screendata & 0xc0];
					DO_PMG
						* ptr++ = cl_word[colreg];
					if (k & 0x01)
						screendata <<= 2;
				}
			}
			t_pm_scanline_ptr++;
		}
	}
}

void draw_antic_c(int nchars, UBYTE *ANTIC_memptr, char *t_ptr, ULONG * t_pm_scanline_ptr)
{
	int i;
	UWORD *ptr = (UWORD *) (t_ptr);
	lookup2[0x00] = cl_word[8];
	lookup2[0x80] = lookup2[0x40] = lookup2[0x20] = lookup2[0x10] = cl_word[4];
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		int kk;
		for (kk = 0; kk < 2; kk++) {
			if (!(*t_pm_scanline_ptr)) {
				*ptr++ = lookup2[screendata & 0x80];
				*ptr++ = lookup2[screendata & 0x40];
				*ptr++ = lookup2[screendata & 0x20];
				*ptr++ = lookup2[screendata & 0x10];
				screendata <<= 4;
			}
			else {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				UBYTE pm_pixel;
				UBYTE colreg;
				int k;
				for (k = 1; k <= 4; k++) {
					pm_pixel = *c_pm_scanline_ptr++;
					colreg = (screendata & 0x80) ? 4 : 8;
					DO_PMG
					* ptr++ = cl_word[colreg];
					screendata <<= 1;
				}
			}
			t_pm_scanline_ptr++;
		}
	}
}

void draw_antic_e(int nchars, UBYTE *ANTIC_memptr, char *t_ptr, ULONG * t_pm_scanline_ptr)
{
	int i;
	UWORD *ptr = (UWORD *) (t_ptr);
#ifdef UNALIGNED_LONG_OK
	int background;
#endif
	lookup2[0x00] = cl_word[8];
	lookup2[0x40] = lookup2[0x10] = lookup2[0x04] = lookup2[0x01] = cl_word[4];
	lookup2[0x80] = lookup2[0x20] = lookup2[0x08] = lookup2[0x02] = cl_word[5];
	lookup2[0xc0] = lookup2[0x30] = lookup2[0x0c] = lookup2[0x03] = cl_word[6];
#ifdef UNALIGNED_LONG_OK
	background = (lookup2[0x00] << 16) | lookup2[0x00];
#endif

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		if (!(*t_pm_scanline_ptr)) {
			if (screendata) {
				*ptr++ = lookup2[screendata & 0xc0];
				*ptr++ = lookup2[screendata & 0x30];
				*ptr++ = lookup2[screendata & 0x0c];
				*ptr++ = lookup2[screendata & 0x03];
			}
			else {
#ifdef UNALIGNED_LONG_OK
				ULONG *l_ptr = (ULONG *) ptr;

				*l_ptr++ = background;
				*l_ptr++ = background;

				ptr = (UWORD *) l_ptr;
#else
				*ptr++ = lookup2[0x00];
				*ptr++ = lookup2[0x00];
				*ptr++ = lookup2[0x00];
				*ptr++ = lookup2[0x00];
#endif
			}
		}
		else {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			UBYTE pm_pixel;
			UBYTE colreg;
			int k;
			for (k = 1; k <= 4; k++) {
				pm_pixel = *c_pm_scanline_ptr++;
				colreg = playfield_lookup[screendata & 0xc0];
				DO_PMG
				* ptr++ = cl_word[colreg];
				screendata <<= 2;
			}

		}
		t_pm_scanline_ptr++;
	}
}

void draw_antic_f(int nchars, UBYTE *ANTIC_memptr, UBYTE *ptr, ULONG * t_pm_scanline_ptr)
{
#ifdef UNALIGNED_LONG_OK
	ULONG COL_6_LONG = cl_word[6] | (cl_word[6] << 16);
#endif
	int i;
	lookup1[0x00] = colour_lookup[6];
	lookup1[0x80] = lookup1[0x40] = lookup1[0x20] =
		lookup1[0x10] = lookup1[0x08] = lookup1[0x04] =
		lookup1[0x02] = lookup1[0x01] = (colour_lookup[6] & 0xf0) | (colour_lookup[5] & 0x0f);
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		if (!(*t_pm_scanline_ptr)) {
			if (screendata) {
				*ptr++ = lookup1[screendata & 0x80];
				*ptr++ = lookup1[screendata & 0x40];
				*ptr++ = lookup1[screendata & 0x20];
				*ptr++ = lookup1[screendata & 0x10];
				*ptr++ = lookup1[screendata & 0x08];
				*ptr++ = lookup1[screendata & 0x04];
				*ptr++ = lookup1[screendata & 0x02];
				*ptr++ = lookup1[screendata & 0x01];
			}
			else {
#ifdef UNALIGNED_LONG_OK
				ULONG *l_ptr = (ULONG *) ptr;

				*l_ptr++ = COL_6_LONG;
				*l_ptr++ = COL_6_LONG;

				ptr = (UBYTE *) l_ptr;
#else
				UWORD *w_ptr = (UWORD *) ptr;

				*w_ptr++ = cl_word[6];
				*w_ptr++ = cl_word[6];
				*w_ptr++ = cl_word[6];
				*w_ptr++ = cl_word[6];

				ptr = (UBYTE *) w_ptr;
#endif
			}
		}
		else {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			UBYTE pm_pixel;
			UBYTE colreg;
			int k;
			for (k = 1; k <= 4; k++) {
				pm_pixel = *c_pm_scanline_ptr++;
				colreg = 6;
				DO_PMG
				if (screendata & 0x80)
					*ptr++ = (cl_word[colreg] & 0xf0) | (COLPF1 & 0x0f);
				else
					*ptr++ = (UBYTE) cl_word[colreg];
				if (screendata & 0x40)
					*ptr++ = (cl_word[colreg] & 0xf0) | (COLPF1 & 0x0f);
				else
					*ptr++ = (UBYTE) cl_word[colreg];
				screendata <<= 2;
			}
		}
		t_pm_scanline_ptr++;
	}
}

void draw_antic_f_artif(int nchars, UBYTE *ANTIC_memptr, UBYTE *ptr, ULONG * t_pm_scanline_ptr)
{
	int i;
	ULONG screendata_tally = *ANTIC_memptr++;

	setup_art_colours();
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		screendata_tally <<= 8;
		screendata_tally |= screendata;
		if (!(*t_pm_scanline_ptr)) {
			*((ULONG *) ptr) = art_curtable[((UBYTE) (screendata_tally >> 10))];
			*((ULONG *) (ptr + 4)) = art_curtable[((UBYTE) (screendata_tally >> 6))];
			ptr += 8;
		}
		else {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			UBYTE pm_pixel;
			UBYTE colreg;
			int k;
			screendata = ANTIC_memptr[-2];
			for (k = 1; k <= 4; k++) {
				pm_pixel = *c_pm_scanline_ptr++;
				colreg = 6;
				DO_PMG
				if (screendata & 0x80)
					*ptr++ = (cl_word[colreg] & 0xf0) | (COLPF1 & 0x0f);
				else
					*ptr++ = (UBYTE) cl_word[colreg];
				if (screendata & 0x40)
					*ptr++ = (cl_word[colreg] & 0xf0) | (COLPF1 & 0x0f);
				else
					*ptr++ = (UBYTE) cl_word[colreg];
				screendata <<= 2;
			}
		}
		t_pm_scanline_ptr++;
	}
}

void draw_antic_f_gtia9(int nchars, UBYTE *ANTIC_memptr, char *t_ptr, ULONG * t_pm_scanline_ptr)
{
	int i;
	ULONG *ptr = (ULONG *) (t_ptr);
	ULONG temp_count = 0;
	ULONG base_colour = cl_word[8] | (cl_word[8] << 16);
	for (i = 0; i <= 15; i++) {
		lookup_gtia[i] = base_colour | temp_count;
		temp_count += 0x01010101;
	}

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		*ptr++ = lookup_gtia[screendata >> 4];
		*ptr++ = lookup_gtia[screendata & 0x0f];
		if ((*t_pm_scanline_ptr)) {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			int k;
			signed char pm_reg;
			UWORD *w_ptr = (UWORD *) (ptr - 2);
			for (k = 0; k < 4; k++) {
				pm_reg=new_pm_lookup[*c_pm_scanline_ptr++];
				if (pm_reg < L_PMNONE)
				/* it is not blank or P5ONLY */
					*w_ptr = cl_word[cur_prior[pm_reg + 8]];
				else if(pm_reg == L_PM5PONLY) {
					UWORD tmp = k < 2 ? screendata >> 4 : screendata & 0xf;
					*w_ptr = tmp | (tmp << 8) | cl_word[7];
				}
				/* otherwise it was L_PMNONE so do nothing */
				w_ptr++;
			}
		}
		t_pm_scanline_ptr++;
	}
}

void draw_antic_f_gtia11(int nchars, UBYTE *ANTIC_memptr, char *t_ptr, ULONG * t_pm_scanline_ptr)
{
	int i;
	ULONG *ptr = (ULONG *) (t_ptr);
	ULONG temp_count = 0;
	ULONG base_colour = cl_word[8] | (cl_word[8] << 16);
	lookup_gtia[0]=base_colour&0xf0f0f0f0;
	/*gtia mode 11 background is always lum 0*/

	for (i = 1; i <= 15; i++) {
		lookup_gtia[i] = base_colour | temp_count;
		temp_count += 0x10101010;
	}

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		*ptr++ = lookup_gtia[screendata >> 4];
		*ptr++ = lookup_gtia[screendata & 0x0f];
		if ((*t_pm_scanline_ptr)) {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			int k;
			signed char pm_reg;
			UWORD *w_ptr = (UWORD *) (ptr - 2);
			for (k = 0; k < 4; k++) {
				pm_reg=new_pm_lookup[*c_pm_scanline_ptr++];
				if (pm_reg < L_PMNONE)
					*w_ptr = cl_word[cur_prior[pm_reg+8]];
				else if(pm_reg == L_PM5PONLY) {
					if ((*w_ptr) & 0xf0f) {
						UWORD tmp = k < 2 ? screendata & 0xf0 : screendata << 4;
						*w_ptr= tmp | (tmp << 8) | cl_word[7];
					}
					else
						*w_ptr =(cl_word[7] & 0xf0f0);
				}
				w_ptr++;
			}
		}
		t_pm_scanline_ptr++;
	}
}

static UBYTE gtia_10_lookup[] =
{8, 8, 8, 8, 4, 5, 6, 7, 8, 8, 8, 8, 4, 5, 6, 7};
static UBYTE gtia_10_pm[] =
{0x01, 0x02, 0x04, 0x08, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
void draw_antic_f_gtia10(int nchars, UBYTE *ANTIC_memptr, char *t_ptr, ULONG * t_pm_scanline_ptr)
{
	int i;
	ULONG *ptr = (ULONG *) (t_ptr + 2);
	lookup_gtia[0] = cl_word[0] | (cl_word[0] << 16);
	lookup_gtia[1] = cl_word[1] | (cl_word[1] << 16);
	lookup_gtia[2] = cl_word[2] | (cl_word[2] << 16);
	lookup_gtia[3] = cl_word[3] | (cl_word[3] << 16);
	lookup_gtia[12] = lookup_gtia[4] = cl_word[4] | (cl_word[4] << 16);
	lookup_gtia[13] = lookup_gtia[5] = cl_word[5] | (cl_word[5] << 16);
	lookup_gtia[14] = lookup_gtia[6] = cl_word[6] | (cl_word[6] << 16);
	lookup_gtia[15] = lookup_gtia[7] = cl_word[7] | (cl_word[7] << 16);
	lookup_gtia[8] = lookup_gtia[9] = lookup_gtia[10] = lookup_gtia[11] = cl_word[8] | (cl_word[8] << 16);
	if (!(*((char *) (t_pm_scanline_ptr)))) {
		*((UWORD *) t_ptr) = cl_word[0];
	}
	else {
		UBYTE pm_pixel = *((char *) t_pm_scanline_ptr);
		pm_pixel |= 0x01;
		*((UWORD *) t_ptr) = cl_word[cur_prior[new_pm_lookup[pm_pixel] + 8]];
	}
	t_pm_scanline_ptr = (ULONG *) (((UBYTE *) t_pm_scanline_ptr) + 1);
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		*ptr++ = lookup_gtia[screendata >> 4];
		*ptr++ = lookup_gtia[screendata & 0x0f];
		if ((*t_pm_scanline_ptr)) {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			UBYTE pm_pixel;
			UBYTE colreg;
			int k;
			UWORD *w_ptr = (UWORD *) (ptr - 2);
			UBYTE t_screendata = screendata >> 4;
			for (k = 0; k <= 3; k++) {
				pm_pixel = *c_pm_scanline_ptr++;
				colreg = gtia_10_lookup[t_screendata];
				pf_colls[colreg] |= pm_pixel;
				pm_pixel |= gtia_10_pm[t_screendata];
				*w_ptr = cl_word[cur_prior[new_pm_lookup[pm_pixel] + colreg]];
				w_ptr++;
				if (k & 0x01)
					t_screendata = screendata & 0x0f;
			}
		}
		t_pm_scanline_ptr++;
	}
}

/* Display List ------------------------------------------------------------ */

UBYTE get_DL_byte(void)
{
	UBYTE result = dGetByte(dlist);
	dlist++;
	if( (dlist & 0x3FF) == 0 )
		dlist -= 0x400;
	xpos++;
	return result;
}

UWORD get_DL_word(void)
{
	UWORD lsb = get_DL_byte();
	return (get_DL_byte() << 8) | lsb;
}

/* Real ANTIC doesn't fetch beginning bytes in HSC
   nor screen+47 in wide playfield. This function does. */
void ANTIC_load(void) {
	UBYTE *ANTIC_memptr = ANTIC_memory + ANTIC_margin;
	UWORD new_screenaddr = screenaddr + chars_read[md];
	if ((screenaddr ^ new_screenaddr) & 0xf000) {
		do
			*ANTIC_memptr++ = dGetByte(screenaddr++);
		while (screenaddr & 0xfff);
		screenaddr -= 0x1000;
		new_screenaddr -= 0x1000;
	}
	while (screenaddr < new_screenaddr)
		*ANTIC_memptr++ = dGetByte(screenaddr++);
}

/* This function emulates one frame drawing screen at atari_screen */
void ANTIC_RunDisplayList(void)
{
	static int mode_type[32] = {
	NORMAL0, NORMAL0, NORMAL0, NORMAL0, NORMAL0, NORMAL0, NORMAL1, NORMAL1,
	NORMAL2, NORMAL2, NORMAL1, NORMAL1, NORMAL1, NORMAL0, NORMAL0, NORMAL0,
	SCROLL0, SCROLL0, SCROLL0, SCROLL0, SCROLL0, SCROLL0, SCROLL1, SCROLL1,
	SCROLL2, SCROLL2, SCROLL1, SCROLL1, SCROLL1, SCROLL0, SCROLL0, SCROLL0};
	static int normal_lastline[16] =
	{ 0, 0, 7, 9, 7, 15, 7, 15, 7, 3, 3, 1, 0, 1, 0, 0 };
	UBYTE vscrol_flag = FALSE;
	UBYTE no_jvb = TRUE;
	UBYTE need_load = TRUE;

	ypos = 0;
	do {
		POKEY_Scanline();		/* check and generate IRQ */
		OVERSCREEN_LINE;
	} while (ypos < 8);

	scrn_ptr = (UBYTE *) atari_screen;
	need_dl = 0x20;
	do {
		POKEY_Scanline();		/* check and generate IRQ */
		pmg_dma();

		if (DMACTL & need_dl) {
			/* PMG flickering :-) */
			if (missile_flickering)
				GRAFM = dGetByte(regPC);
			if (player_flickering) {
				GRAFP0 = dGetByte(regPC+4);
				GRAFP1 = dGetByte(regPC+5);
				GRAFP2 = dGetByte(regPC+6);
				GRAFP3 = dGetByte(regPC+7);
			}

			dctr = 0;
			need_dl = 0;
			vscrol_off = FALSE;
			IR = get_DL_byte();
			anticmode = IR & 0xf;

			switch (anticmode) {
			case 0x00:
				lastline = (IR >> 4) & 7;
				if (vscrol_flag) {
					lastline = VSCROL;
					vscrol_flag = FALSE;
					vscrol_off = TRUE;
				}
				break;
			case 0x01:
				lastline = 0;
				if (IR & 0x40) {
					dlist = get_DL_word();
					anticmode = 0;
					no_jvb = FALSE;
				}
				else
					if (vscrol_flag) {
						lastline = VSCROL;
						vscrol_flag = FALSE;
						vscrol_off = TRUE;
					}
				break;
			default:
				lastline = normal_lastline[anticmode];
				if (IR & 0x20) {
					if (!vscrol_flag) {
						GO(VSCON_C);
						dctr = VSCROL;
						vscrol_flag = TRUE;
					}
				}
				else if (vscrol_flag) {
					lastline = VSCROL;
					vscrol_flag = FALSE;
					vscrol_off = TRUE;
				}
				if (IR & 0x40)
					screenaddr = get_DL_word();
				md = mode_type[IR & 0x1f];
				need_load = TRUE;
				break;
			}
		}

		if (anticmode == 1 && DMACTL & 0x20)
			dlist = get_DL_word();

		if (dctr == lastline) {
			if (no_jvb)
				need_dl = 0x20;
			if (IR & 0x80) {
				GO(NMIST_C);
				NMIST = 0x9f;
				if(NMIEN & 0x80) {
					GO(NMI_C);
					NMI();
				}
			}
		}

		if (need_load && anticmode >=2 && anticmode <= 5 && (DMACTL & 3))
			xpos += before_cycles[md];

		if (!draw_display) {
			xpos += DMAR;
			if (anticmode < 2 || (DMACTL & 3) == 0)
				goto after_scanline;
			if (need_load) {
				need_load = FALSE;
				xpos += load_cycles[md];
				if (anticmode <= 5)	/* extra cycles in font modes */
					xpos -= extra_cycles[md];
			}
			if (anticmode < 8)
				xpos += font_cycles[md];
			goto after_scanline;
		}

		GO(SCR_C);
		new_pm_scanline();

		xpos += DMAR;

		if (anticmode < 2 || (DMACTL & 3) == 0) {
			do_border(48-LCHOP-RCHOP, 0);
			goto after_scanline;
		}

		if (need_load) {
			need_load = FALSE;
			ANTIC_load();
			xpos += load_cycles[md];
			if (anticmode <= 5)	/* extra cycles in font modes */
				xpos -= extra_cycles[md];
		}

		switch (anticmode) {
#define CHARS_MEM_PTR_PM	chars_displayed[md], \
							ANTIC_memory + ANTIC_margin + ch_offset[md], \
							scrn_ptr + x_min[md], \
							(ULONG *) (&pm_scanline[x_min[md] >> 1])
		case 2:
		case 3:
			xpos += font_cycles[md];
			blank_lookup[0x60] = (anticmode == 2 || dctr & 0xe) ? 0xff : 0;
			blank_lookup[0x00] = blank_lookup[0x20] = blank_lookup[0x40] = (dctr & 0xe) == 8 ? 0 : 0xff;
			if (PRIOR & 0x40)
				draw_antic_2_gtia9_11(CHARS_MEM_PTR_PM, chbase_40 + ((dctr & 0x07) ^ flip_mask) );
			else if (global_artif_mode != 0)
				draw_antic_2_artif(CHARS_MEM_PTR_PM, chbase_40 + ((dctr & 0x07) ^ flip_mask) );
			else
			        draw_antic_2(CHARS_MEM_PTR_PM, chbase_40 + ((dctr & 0x07) ^ flip_mask) );
			break;
		case 4:
			xpos += font_cycles[md];
			draw_antic_4(CHARS_MEM_PTR_PM, chbase_40 + ((dctr & 0x07) ^ flip_mask) );
			break;
		case 5:
			xpos += font_cycles[md];
			draw_antic_4(CHARS_MEM_PTR_PM, chbase_40 + ((dctr >> 1) ^ flip_mask) );
			break;
		case 6:
			xpos += font_cycles[md];
			draw_antic_6(CHARS_MEM_PTR_PM, chbase_20 + ((dctr & 7) ^ flip_mask) );
			break;
		case 7:
			xpos += font_cycles[md];
			draw_antic_6(CHARS_MEM_PTR_PM, chbase_20 + ((dctr >> 1) ^ flip_mask) );
			break;
		case 8:
			draw_antic_8(CHARS_MEM_PTR_PM);
			break;
		case 9:
			draw_antic_9(CHARS_MEM_PTR_PM);
			break;
		case 0xa:
			draw_antic_a(CHARS_MEM_PTR_PM);
			break;
		case 0xb:
		case 0xc:
			draw_antic_c(CHARS_MEM_PTR_PM);
			break;
		case 0xd:
		case 0xe:
			draw_antic_e(CHARS_MEM_PTR_PM);
			break;
		case 0xf:
			if ((PRIOR & 0xC0) == 0x40)
				draw_antic_f_gtia9(CHARS_MEM_PTR_PM);
			else if ((PRIOR & 0xC0) == 0x80)
				draw_antic_f_gtia10(CHARS_MEM_PTR_PM);
			else if ((PRIOR & 0xC0) == 0xC0)
				draw_antic_f_gtia11(CHARS_MEM_PTR_PM);
			else if (global_artif_mode != 0)
				draw_antic_f_artif(CHARS_MEM_PTR_PM);
			else
				draw_antic_f(CHARS_MEM_PTR_PM);
			break;
		}
		do_border(left_border_chars, right_border_chars);
	after_scanline:
		GOEOL;
		scrn_ptr += ATARI_WIDTH;
		if (no_jvb) {
			dctr++;
			dctr &= 0xf;
		}
	} while (ypos < (ATARI_HEIGHT + 8));

	POKEY_Scanline();		/* check and generate IRQ */
	GO(NMIST_C);
	NMIST = 0x5f;				/* Set VBLANK */
	if (NMIEN & 0x40) {
		GO(NMI_C);
		NMI();
	}
	xpos += DMAR;
	GOEOL;

	while (ypos < max_ypos) {
		POKEY_Scanline();		/* check and generate IRQ */
		OVERSCREEN_LINE;
	}
}

/* ANTIC registers --------------------------------------------------------- */

UBYTE ANTIC_GetByte(UWORD addr)
{
	switch (addr & 0xf) {
	case _VCOUNT:
		return ypos >> 1;
	case _PENH:
		return Atari_PEN(0);
	case _PENV:
		return Atari_PEN(1);
	case _NMIST:
		return NMIST;
	default:
		return 0xff;
	}
}

void ANTIC_PutByte(UWORD addr, UBYTE byte)
{
	switch (addr & 0xf) {
	case _CHACTL:
		CHACTL = byte;
		flip_mask = byte & 4 ? 7 : 0;
		invert_mask = byte & 2 ? 0x80 : 0;
		blank_mask = byte & 1 ? 0xe0 : 0x60;
		break;
	case _DLISTL:
		dlist = (dlist & 0xff00) | byte;
		break;
	case _DLISTH:
		dlist = (dlist & 0x00ff) | (byte << 8);
		break;
	case _DMACTL:
		DMACTL = byte;
		switch (DMACTL & 0x03) {
		case 0x00:
			chars_read[NORMAL0] = 0;
			chars_read[NORMAL1] = 0;
			chars_read[NORMAL2] = 0;
			chars_read[SCROLL0] = 0;
			chars_read[SCROLL1] = 0;
			chars_read[SCROLL2] = 0;
			chars_displayed[NORMAL0] = 0;
			chars_displayed[NORMAL1] = 0;
			chars_displayed[NORMAL2] = 0;
			chars_displayed[SCROLL0] = 0;
			chars_displayed[SCROLL1] = 0;
			chars_displayed[SCROLL2] = 0;
			x_min[NORMAL0] = 0;
			x_min[NORMAL1] = 0;
			x_min[NORMAL2] = 0;
			x_min[SCROLL0] = 0;
			x_min[SCROLL1] = 0;
			x_min[SCROLL2] = 0;
			ch_offset[NORMAL0] = 0;
			ch_offset[NORMAL1] = 0;
			ch_offset[NORMAL2] = 0;
			ch_offset[SCROLL0] = 0;
			ch_offset[SCROLL1] = 0;
			ch_offset[SCROLL2] = 0;
			left_border_chars = 48 - LCHOP - RCHOP;
			right_border_chars = 0;
			right_border_start = 0;
			break;
		case 0x01:
			chars_read[NORMAL0] = 32;
			chars_read[NORMAL1] = 16;
			chars_read[NORMAL2] = 8;
			chars_read[SCROLL0] = 40;
			chars_read[SCROLL1] = 20;
			chars_read[SCROLL2] = 10;
			chars_displayed[NORMAL0] = 32;
			chars_displayed[NORMAL1] = 16;
			chars_displayed[NORMAL2] = 8;
			x_min[NORMAL0] = 64;
			x_min[NORMAL1] = 64;
			x_min[NORMAL2] = 64;
			ch_offset[NORMAL0] = 0;
			ch_offset[NORMAL1] = 0;
			ch_offset[NORMAL2] = 0;
			ch_offset[SCROLL2] = 1;
			font_cycles[NORMAL0] = load_cycles[NORMAL0] = 32;
			font_cycles[NORMAL1] = load_cycles[NORMAL1] = 16;
			load_cycles[NORMAL2] = 8;
			before_cycles[NORMAL0] = BEFORE_CYCLES;
			before_cycles[SCROLL0] = BEFORE_CYCLES + 8;
			extra_cycles[NORMAL0] = 7 + BEFORE_CYCLES;
			extra_cycles[SCROLL0] = 8 + BEFORE_CYCLES + 8;
			left_border_chars = 8 - LCHOP;
			right_border_chars = 8 - RCHOP;
			right_border_start = ATARI_WIDTH - 64 /* - 1 */ ;	/* RS! */
			break;
		case 0x02:
			chars_read[NORMAL0] = 40;
			chars_read[NORMAL1] = 20;
			chars_read[NORMAL2] = 10;
			chars_read[SCROLL0] = 48;
			chars_read[SCROLL1] = 24;
			chars_read[SCROLL2] = 12;
			chars_displayed[NORMAL0] = 40;
			chars_displayed[NORMAL1] = 20;
			chars_displayed[NORMAL2] = 10;
			x_min[NORMAL0] = 32;
			x_min[NORMAL1] = 32;
			x_min[NORMAL2] = 32;
			ch_offset[NORMAL0] = 0;
			ch_offset[NORMAL1] = 0;
			ch_offset[NORMAL2] = 0;
			ch_offset[SCROLL2] = 1;
			font_cycles[NORMAL0] = load_cycles[NORMAL0] = 40;
			font_cycles[NORMAL1] = load_cycles[NORMAL1] = 20;
			load_cycles[NORMAL2] = 10;
			before_cycles[NORMAL0] = BEFORE_CYCLES + 8;
			before_cycles[SCROLL0] = BEFORE_CYCLES + 16;
			extra_cycles[NORMAL0] = 8 + BEFORE_CYCLES + 8;
			extra_cycles[SCROLL0] = 7 + BEFORE_CYCLES + 16;
			left_border_chars = 4 - LCHOP;
			right_border_chars = 4 - RCHOP;
			right_border_start = ATARI_WIDTH - 32 /* - 1 */ ;	/* RS! */
			break;
		case 0x03:
			chars_read[NORMAL0] = 48;
			chars_read[NORMAL1] = 24;
			chars_read[NORMAL2] = 12;
			chars_read[SCROLL0] = 48;
			chars_read[SCROLL1] = 24;
			chars_read[SCROLL2] = 12;
			chars_displayed[NORMAL0] = 44;
			chars_displayed[NORMAL1] = 23;
			chars_displayed[NORMAL2] = 12;
			x_min[NORMAL0] = 24;
			x_min[NORMAL1] = 16;
			x_min[NORMAL2] = 0;
			ch_offset[NORMAL0] = 3;
			ch_offset[NORMAL1] = 1;
			ch_offset[NORMAL2] = 0;
			ch_offset[SCROLL2] = 0;
			font_cycles[NORMAL0] = load_cycles[NORMAL0] = 47;
			font_cycles[NORMAL1] = load_cycles[NORMAL1] = 24;
			load_cycles[NORMAL2] = 12;
			before_cycles[NORMAL0] = BEFORE_CYCLES + 16;
			before_cycles[SCROLL0] = BEFORE_CYCLES + 16;
			extra_cycles[NORMAL0] = 7 + BEFORE_CYCLES + 16;
			extra_cycles[SCROLL0] = 7 + BEFORE_CYCLES + 16;
			left_border_chars = 3 - LCHOP;
			right_border_chars = ((1 - LCHOP) < 0) ? (0) : (1 - LCHOP);
			right_border_start = ATARI_WIDTH - 8 /* - 1 */ ;	/* RS! */
			break;
		}

		missile_dma_enabled = (DMACTL & 0x0c);	/* no player dma without missile */
		player_dma_enabled = (DMACTL & 0x08);
		singleline = (DMACTL & 0x10);
		player_flickering = ((player_dma_enabled | player_gra_enabled) == 0x02);
		missile_flickering = ((missile_dma_enabled | missile_gra_enabled) == 0x01);

		if ((DMACTL & 0x20) == 0) {
			anticmode = IR = 0;
			lastline = 0;
		}

		byte = HSCROL;	/* update horizontal scroll data */

	case _HSCROL:
		HSCROL = byte & 0x0f;
		if (DMACTL & 3) {
			chars_displayed[SCROLL0] = chars_displayed[NORMAL0];
			ch_offset[SCROLL0] = 4 - (HSCROL >> 2);
			x_min[SCROLL0] = x_min[NORMAL0];
			if (HSCROL & 3) {
				x_min[SCROLL0] += ((HSCROL & 3) << 1) - 8;
				chars_displayed[SCROLL0]++;
				ch_offset[SCROLL0]--;
			}

			if ((DMACTL & 3) == 3) {	/* wide playfield */
				ch_offset[SCROLL0]--;
				if (HSCROL == 4 || HSCROL == 12)
					chars_displayed[SCROLL1] = 22;
				else
					chars_displayed[SCROLL1] = 23;
				if (HSCROL <= 4) {
					x_min[SCROLL1] = (HSCROL << 1) + 16;
					ch_offset[SCROLL1] = 1;
				}
				else if (HSCROL <= 12) {
					x_min[SCROLL1] = HSCROL << 1;
					ch_offset[SCROLL1] = 0;
				}
				else {
					x_min[SCROLL1] = (HSCROL << 1) - 16;
					ch_offset[SCROLL1] = -1;
				}
			}
			else {
				chars_displayed[SCROLL1] = chars_displayed[NORMAL1];
				ch_offset[SCROLL1] = 2 - (HSCROL >> 3);
				x_min[SCROLL1] = x_min[NORMAL0];
				if (HSCROL & 7) {
					x_min[SCROLL1] += ((HSCROL & 7) << 1) - 16;
					chars_displayed[SCROLL1]++;
					ch_offset[SCROLL1]--;
				}
			}

			chars_displayed[SCROLL2] = chars_displayed[NORMAL2];
			if ((DMACTL & 3) == 3)	/* wide playfield */
				x_min[SCROLL2] = x_min[NORMAL2] + (HSCROL << 1);
			else {
				if (HSCROL) {
					x_min[SCROLL2] = x_min[NORMAL2] - 32 + (HSCROL << 1);
					chars_displayed[SCROLL2]++;
					ch_offset[SCROLL2]--;
				}
				else
					x_min[SCROLL2] = x_min[NORMAL2];
			}

			if (DMACTL & 2) {		/* normal & wide playfield */
				load_cycles[SCROLL0] = 47 - (HSCROL >> 2);
				font_cycles[SCROLL0] = (47 * 4 + 1 - HSCROL) >> 2;
				load_cycles[SCROLL1] = (24 * 8 + 3 - HSCROL) >> 3;
				font_cycles[SCROLL1] = (24 * 8 + 1 - HSCROL) >> 3;
				load_cycles[SCROLL2] = HSCROL < 0xc ? 12 : 11;
			}
			else {					/* narrow playfield */
				font_cycles[SCROLL0] = load_cycles[SCROLL0] = 40;
				font_cycles[SCROLL1] = load_cycles[SCROLL1] = 20;
				load_cycles[SCROLL2] = 16;
			}
		}
		break;
	case _VSCROL:
		VSCROL = byte & 0x0f;
		if (vscrol_off) {
			lastline = VSCROL;
			if (xpos < VSCOF_C)
				need_dl = dctr == lastline ? 0x20 : 0x00;
		}
		break;
	case _PMBASE:
		{
			UWORD pmbase_s;
			UWORD pmbase_d;

			PMBASE = byte;

			pmbase_s = (PMBASE & 0xf8) << 8;
			pmbase_d = (PMBASE & 0xfc) << 8;

			maddr_s = pmbase_s + 768;
			p0addr_s = pmbase_s + 1024;
			p1addr_s = pmbase_s + 1280;
			p2addr_s = pmbase_s + 1536;
			p3addr_s = pmbase_s + 1792;

			maddr_d = pmbase_d + 384;
			p0addr_d = pmbase_d + 512;
			p1addr_d = pmbase_d + 640;
			p2addr_d = pmbase_d + 768;
			p3addr_d = pmbase_d + 896;
		}
		break;
	case _CHBASE:
		CHBASE = byte;
		chbase_40 = (byte << 8) & 0xfc00;
		chbase_20 = (byte << 8) & 0xfe00;
		break;
	case _WSYNC:
		if (xpos <= WSYNC_C && xpos_limit >= WSYNC_C)
			xpos = WSYNC_C;
		else {
			wsync_halt = TRUE;
			xpos = xpos_limit;
		}
		break;
	case _NMIEN:
		NMIEN = byte;
		break;
	case _NMIRES:
		NMIST = 0x1f;
		break;
	}
}

/* State ------------------------------------------------------------------- */

void AnticStateSave( void )
{
	UBYTE	temp;

	SaveUBYTE( &DMACTL, 1 );
	SaveUBYTE( &CHACTL, 1 );
	SaveUBYTE( &HSCROL, 1 );
	SaveUBYTE( &VSCROL, 1 );
	SaveUBYTE( &PMBASE, 1 );
	SaveUBYTE( &CHBASE, 1 );
	SaveUBYTE( &NMIEN, 1 );
	SaveUBYTE( &NMIST, 1 );
	SaveUBYTE( &singleline, 1 );
	SaveUBYTE( &player_dma_enabled, 1 );
	SaveUBYTE( &player_gra_enabled, 1 );
	SaveUBYTE( &missile_dma_enabled, 1 );
	SaveUBYTE( &missile_gra_enabled, 1 );
	SaveUBYTE( &player_flickering, 1 );
	SaveUBYTE( &missile_flickering, 1 );
	SaveUBYTE( &IR, 1 );
	SaveUBYTE( &anticmode, 1 );
	SaveUBYTE( &flip_mask, 1 );
	SaveUBYTE( &invert_mask, 1 );
	SaveUBYTE( &blank_mask, 1 );

	SaveUBYTE( &pm_scanline[0], ATARI_WIDTH );
	SaveUBYTE( &pf_colls[0], 9 );
	SaveUBYTE( &prior_table[0], 5 * NUM_PLAYER_TYPES * 16 );
	SaveUBYTE( &cur_prior[0], 5 * NUM_PLAYER_TYPES);

	SaveUWORD( &dlist, 1 );
	SaveUWORD( &screenaddr, 1 );
	SaveUWORD( &chbase_40, 1 );
	SaveUWORD( &chbase_20, 1 );
	SaveUWORD( &maddr_s, 1 );
	SaveUWORD( &p0addr_s, 1 );
	SaveUWORD( &p1addr_s, 1 );
	SaveUWORD( &p2addr_s, 1 );
	SaveUWORD( &p3addr_s, 1 );
	SaveUWORD( &maddr_d, 1 );
	SaveUWORD( &p0addr_d, 1 );
	SaveUWORD( &p1addr_d, 1 );
	SaveUWORD( &p2addr_d, 1 );
	SaveUWORD( &p3addr_d, 1 );
	
	SaveUWORD( &cl_word[0], 24 );

	SaveINT( &dctr, 1 );
	SaveINT( &lastline, 1 );
	SaveINT( &left_border_chars, 1 );
	SaveINT( &right_border_chars, 1 );
	SaveINT( &right_border_start, 1 );

	SaveINT( &chars_read[0], 6 );
	SaveINT( &chars_displayed[0], 6 );
	SaveINT( &x_min[0], 6 );
	SaveINT( &ch_offset[0], 6 );
	SaveINT( &load_cycles[0], 6 );
	SaveINT( &font_cycles[0], 6 );
	SaveINT( &before_cycles[0], 6 );
	SaveINT( &extra_cycles[0], 6 );

	if( new_pm_lookup == NULL )
		temp = 0;
	if( new_pm_lookup == pm_lookup_normal )
		temp = 1;
	if( new_pm_lookup == pm_lookup_5p )
		temp = 2;
	if( new_pm_lookup == pm_lookup_multi )
		temp = 3;
	if( new_pm_lookup == pm_lookup_multi_5p )
		temp = 4;
	SaveUBYTE( &temp, 1 );
}

void AnticStateRead( void )
{
	UBYTE	temp;

	ReadUBYTE( &DMACTL, 1 );
	ReadUBYTE( &CHACTL, 1 );
	ReadUBYTE( &HSCROL, 1 );
	ReadUBYTE( &VSCROL, 1 );
	ReadUBYTE( &PMBASE, 1 );
	ReadUBYTE( &CHBASE, 1 );
	ReadUBYTE( &NMIEN, 1 );
	ReadUBYTE( &NMIST, 1 );
	ReadUBYTE( &singleline, 1 );
	ReadUBYTE( &player_dma_enabled, 1 );
	ReadUBYTE( &player_gra_enabled, 1 );
	ReadUBYTE( &missile_dma_enabled, 1 );
	ReadUBYTE( &missile_gra_enabled, 1 );
	ReadUBYTE( &player_flickering, 1 );
	ReadUBYTE( &missile_flickering, 1 );
	ReadUBYTE( &IR, 1 );
	ReadUBYTE( &anticmode, 1 );
	ReadUBYTE( &flip_mask, 1 );
	ReadUBYTE( &invert_mask, 1 );
	ReadUBYTE( &blank_mask, 1 );

	ReadUBYTE( &pm_scanline[0], ATARI_WIDTH );
	ReadUBYTE( &pf_colls[0], 9 );
	ReadUBYTE( &prior_table[0], 5 * NUM_PLAYER_TYPES * 16 );
	ReadUBYTE( &cur_prior[0], 5 * NUM_PLAYER_TYPES);

	ReadUWORD( &dlist, 1 );
	ReadUWORD( &screenaddr, 1 );
	ReadUWORD( &chbase_40, 1 );
	ReadUWORD( &chbase_20, 1 );
	ReadUWORD( &maddr_s, 1 );
	ReadUWORD( &p0addr_s, 1 );
	ReadUWORD( &p1addr_s, 1 );
	ReadUWORD( &p2addr_s, 1 );
	ReadUWORD( &p3addr_s, 1 );
	ReadUWORD( &maddr_d, 1 );
	ReadUWORD( &p0addr_d, 1 );
	ReadUWORD( &p1addr_d, 1 );
	ReadUWORD( &p2addr_d, 1 );
	ReadUWORD( &p3addr_d, 1 );
	
	ReadUWORD( &cl_word[0], 24 );

	ReadINT( &dctr, 1 );
	ReadINT( &lastline, 1 );
	ReadINT( &left_border_chars, 1 );
	ReadINT( &right_border_chars, 1 );
	ReadINT( &right_border_start, 1 );

	ReadINT( &chars_read[0], 6 );
	ReadINT( &chars_displayed[0], 6 );
	ReadINT( &x_min[0], 6 );
	ReadINT( &ch_offset[0], 6 );
	ReadINT( &load_cycles[0], 6 );
	ReadINT( &font_cycles[0], 6 );
	ReadINT( &before_cycles[0], 6 );
	ReadINT( &extra_cycles[0], 6 );

	ReadUBYTE( &temp, 1 );

	if( temp == 0 )
		new_pm_lookup = NULL;
	if( temp == 1 )
		new_pm_lookup = pm_lookup_normal;
	if( temp == 2 )
		new_pm_lookup = pm_lookup_5p;
	if( temp == 3 )
		new_pm_lookup = pm_lookup_multi;
	if( temp == 4 )
		new_pm_lookup = pm_lookup_multi_5p;
}
