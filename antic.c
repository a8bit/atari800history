/* ANTIC emulation -------------------------------- */
/* Original Author:                                 */
/*              David Firth                         */
/* Correct timing, internal memory and other fixes: */
/*              Piotr Fusik <pfusik@elka.pw.edu.pl> */
/* Last changes: 27th April 2000                    */
/* ------------------------------------------------ */

#include <string.h>

#define LCHOP 3			/* do not build lefmost 0..3 characters in wide mode */
#define RCHOP 3			/* do not build rightmost 0..3 characters in wide mode */

#include "antic.h"
#include "atari.h"
#include "config.h"
#include "cpu.h"
#include "log.h"
#include "gtia.h"
#include "memory.h"
#include "platform.h"
#include "pokey.h"
#include "rt-config.h"
#include "statesav.h"

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
/* We no longer need 16 extra lines */

ULONG *atari_screen = NULL;
#ifdef BITPL_SCR
ULONG *atari_screen_b = NULL;
ULONG *atari_screen1 = NULL;
ULONG *atari_screen2 = NULL;
#endif
static UBYTE *scrn_ptr;

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

#define DMAR	9	/* defined also in atari.h */

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

static UWORD screenaddr;		/* Screen Pointer */
static UBYTE IR;				/* Instruction Register */
static UBYTE anticmode;			/* Antic mode */
static UBYTE dctr;				/* Delta Counter */
static UBYTE lastline;			/* dctr limit */
static UBYTE need_dl;			/* boolean: fetch DL next line */
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

/* set with CHBASE *and* CHACTL - bits 0..2 set if flip on */
static UWORD chbase_20;			/* CHBASE for 20 character mode */

/* set with CHACTL */
static UBYTE invert_mask;
static UBYTE blank_mask;

/* lookup tables */
static UBYTE blank_lookup[256];
static UWORD lookup2[256];
ULONG lookup_gtia9[16];
ULONG lookup_gtia11[16];
static UBYTE playfield_lookup[257];

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

#ifdef UNALIGNED_LONG_OK

#define INIT_BACKGROUND_6 ULONG background = cl_word[6] | (((ULONG) cl_word[6]) << 16);
#define INIT_BACKGROUND_8 ULONG background = lookup_gtia9[0];
#define DRAW_BACKGROUND(colreg) {\
		ULONG *l_ptr = (ULONG *) ptr;\
		*l_ptr++ = background;\
		*l_ptr++ = background;\
		ptr = (UWORD *) l_ptr;\
	}
#define DRAW_ARTIF {\
		*(ULONG *) ptr = art_curtable[(UBYTE) (screendata_tally >> 10)];\
		((ULONG *) ptr)[1] = art_curtable[(UBYTE) (screendata_tally >> 6)];\
		ptr += 4;\
	}

#else

#define INIT_BACKGROUND_6
#define INIT_BACKGROUND_8
#define DRAW_BACKGROUND(colreg) {\
		*ptr++ = cl_word[colreg];\
		*ptr++ = cl_word[colreg];\
		*ptr++ = cl_word[colreg];\
		*ptr++ = cl_word[colreg];\
	}
#define DRAW_ARTIF {\
		*ptr++ = ((UWORD *) art_curtable)[(screendata_tally & 0x04fc00) >> 9];\
		*ptr++ = ((UWORD *) art_curtable)[((screendata_tally & 0x04fc00) >> 9) + 1];\
		*ptr++ = ((UWORD *) art_curtable)[(screendata_tally & 0x004fc0) >> 5];\
		*ptr++ = ((UWORD *) art_curtable)[((screendata_tally & 0x004fc0) >> 5) + 1];\
	}

#endif /* UNALIGNED_LONG_OK */

/* Hi-res modes optimizations
   Now hi-res modes are drawn with words, not bytes. Endianess defaults
   to little-endian. Use -DANTIC_BIG_ENDIAN when compiling on
   big-endian machine. */

#ifdef ANTIC_BIG_ENDIAN
#define HIRES_MASK_01	0xfff0
#define HIRES_MASK_10	0xf0ff
#define HIRES_LUM_01	0x000f
#define HIRES_LUM_10	0x0f00
#else
#define HIRES_MASK_01	0xf0ff
#define HIRES_MASK_10	0xfff0
#define HIRES_LUM_01	0x0f00
#define HIRES_LUM_10	0x000f
#endif

static UWORD hires_lookup_n[128];
static UWORD hires_lookup_m[128];
UWORD hires_lookup_l[128];	/* accessed in gtia.c */

#define hires_norm(x)	hires_lookup_n[(x) >> 1]
#define hires_mask(x)	hires_lookup_m[(x) >> 1]
#define hires_lum(x)	hires_lookup_l[(x) >> 1]

/* Player/Missile Graphics ------------------------------------------------- */

extern UBYTE pf_colls[9];		/* playfield collisions */

static UBYTE singleline;
UBYTE player_dma_enabled;
UBYTE player_gra_enabled;
UBYTE missile_dma_enabled;
UBYTE missile_gra_enabled;
UBYTE player_flickering;
UBYTE missile_flickering;

static UWORD pmbase_s;
static UWORD pmbase_d;

extern UBYTE pm_scanline[ATARI_WIDTH / 2];
extern UBYTE pm_dirty;

#define L_PM0 (0*5-4)
#define L_PM1 (1*5-4)
#define L_PM01 (2*5-4)
#define L_PM2 (3*5-4)
#define L_PM3 (4*5-4)
#define L_PM23 (5*5-4)
#define L_PMNONE (12*5-4)
#define L_PM5PONLY (13*5-4) 	/* these should be the last two */
#define NUM_PLAYER_TYPES 14

/* PMG lookup tables: 0 - normal, 1 - PM5, 2 - multi, 3 - multi/PM5 */
signed char pm_lookup_table[4][256];
/* current PMG lookup table */
signed char *pm_lookup_ptr;

UBYTE prior_table[5*NUM_PLAYER_TYPES * 16];
UBYTE cur_prior[5*NUM_PLAYER_TYPES];

/* these defines also in gtia.c */
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
	{0, 1, 9, 2, 3, 10};
	int i, j;
	UBYTE *ptr = prior_table;
	for (i = 0; i <= 15; i++) {	 /* prior setting */
		for (j = 0; j < 6; j++) {	/* player 0,1,0OR1,2,3,2OR3 */
			UBYTE c;
			if (j <= 2) {
				switch (i) {	/* playfields 0,1 */
				case 0:
					*ptr++ = j + R_COLPM0_OR_PF0;
					*ptr++ = j + R_COLPM0_OR_PF1;
					break;
				case 1:
				case 2:
				case 3:
					*ptr++ = player_colreg[j];
					*ptr++ = player_colreg[j];
					break;
				case 4:
				case 8:
				case 12:
					*ptr++ = 4;
					*ptr++ = 5;
					break;
				default:
					*ptr++ = R_BLACK;
					*ptr++ = R_BLACK;
					break;
				}
				if (i & 4) {	/* playfields 2,3 */
					*ptr++ = 6;
					c = 7;
				}
				else
					c = *ptr++ = player_colreg[j];
			}
			else {
				if (i & 1) {	/* playfields 0,1 */
					*ptr++ = player_colreg[j];
					*ptr++ = player_colreg[j];
				}
				else {
					*ptr++ = 4;
					*ptr++ = 5;
				}
				switch (i) {	/* playfields 2,3 */
				case 0:
					*ptr++ = j + R_COLPM2_OR_PF2 - 3;
					c = j + R_COLPM2_OR_PF3 - 3;
					break;
				case 1:
				case 8:
				case 9:
					*ptr++ = player_colreg[j];
					c = player_colreg[j];
					break;
				case 2:
				case 4:
				case 6:
					*ptr++ = 6;
					c = 7;
					break;
				default:
					c = *ptr++ = R_BLACK;
					break;
				}
			}
			/* playfield 3 = PM5 */
			ptr[27] = ptr[28] = ptr[29] = ptr[30] = ptr[31] = *ptr = c;
			/* player on background */
			ptr[1] = player_colreg[j];
			ptr += 2;
		}
		ptr += 30;
		ptr[0] = 4; ptr[1] = 5; ptr[2] = 6; ptr[3] = 7; ptr[4] = 8; /* PMNONE */
		ptr[5] = ptr[6] = ptr[7] = ptr[8] = ptr[9] = 7;			 /* PM5ONLY */
		ptr += 10;
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
		pm_lookup_table[0][i] = old_new_pm_lookup[pm];
		pm_lookup_table[2][i] = old_new_pm_lookup_multi[pm];
		pm = i & 0x0f;
		pm_lookup_table[1][i] = old_new_pm_lookup[pm];
		pm_lookup_table[3][i] = old_new_pm_lookup_multi[pm];
		if (i > 15) { /* 5th player adds 6*5 */
			if (pm) {
				pm_lookup_table[1][i] += 6*5;
				pm_lookup_table[3][i] += 6*5;
			}
			else {
				pm_lookup_table[1][i] = L_PM5PONLY;
				pm_lookup_table[3][i] = L_PM5PONLY;
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
					GRAFP0 = dGetByte(pmbase_s + ypos + 0x400);
					GRAFP1 = dGetByte(pmbase_s + ypos + 0x500);
					GRAFP2 = dGetByte(pmbase_s + ypos + 0x600);
					GRAFP3 = dGetByte(pmbase_s + ypos + 0x700);
				}
				else {
					if ((VDELAY & 0x10) == 0)
						GRAFP0 = dGetByte(pmbase_s + ypos + 0x400);
					if ((VDELAY & 0x20) == 0)
						GRAFP1 = dGetByte(pmbase_s + ypos + 0x500);
					if ((VDELAY & 0x40) == 0)
						GRAFP2 = dGetByte(pmbase_s + ypos + 0x600);
					if ((VDELAY & 0x80) == 0)
						GRAFP3 = dGetByte(pmbase_s + ypos + 0x700);
				}
			}
			else {
				if (ypos & 1) {
					GRAFP0 = dGetByte(pmbase_d + (ypos >> 1) + 0x200);
					GRAFP1 = dGetByte(pmbase_d + (ypos >> 1) + 0x280);
					GRAFP2 = dGetByte(pmbase_d + (ypos >> 1) + 0x300);
					GRAFP3 = dGetByte(pmbase_d + (ypos >> 1) + 0x380);
				}
				else {
					if ((VDELAY & 0x10) == 0)
						GRAFP0 = dGetByte(pmbase_d + (ypos >> 1) + 0x200);
					if ((VDELAY & 0x20) == 0)
						GRAFP1 = dGetByte(pmbase_d + (ypos >> 1) + 0x280);
					if ((VDELAY & 0x40) == 0)
						GRAFP2 = dGetByte(pmbase_d + (ypos >> 1) + 0x300);
					if ((VDELAY & 0x80) == 0)
						GRAFP3 = dGetByte(pmbase_d + (ypos >> 1) + 0x380);
				}
			}
		}
		xpos += 4;
	}
	if (missile_dma_enabled) {
		if (missile_gra_enabled) {
			UBYTE hold_missiles;
			hold_missiles = ypos & 1 ? 0 : hold_missiles_tab[VDELAY & 0xf];
			GRAFM = (dGetByte(singleline ? pmbase_s + ypos + 0x300 : pmbase_d + (ypos >> 1) + 0x180) & ~hold_missiles )
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

static ULONG *art_curtable = art_lookup_normal;
static ULONG *art_curbkmask = art_bkmask_normal;
static ULONG *art_curlummask = art_lummask_normal;

static UWORD art_normal_colpf1_save;
static UWORD art_normal_colpf2_save;
static UWORD art_reverse_colpf1_save;
static UWORD art_reverse_colpf2_save;

void setup_art_colours(void)
{
	static UWORD *art_colpf1_save = &art_normal_colpf1_save;
	static UWORD *art_colpf2_save = &art_normal_colpf2_save;
	UWORD curlum = cl_word[5] & 0x0f0f;

	if (curlum != *art_colpf1_save || cl_word[6] != *art_colpf2_save) {
		if (curlum < (COLPF2 & 0x0f0f)) {
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
		if (curlum ^ *art_colpf1_save) {
			int i;
			ULONG new_colour = curlum ^ *art_colpf1_save;
			new_colour |= new_colour << 16;
			*art_colpf1_save = curlum;
			for (i = 0; i <= 255; i++)
				art_curtable[i] ^= art_curlummask[i] & new_colour;
		}
		if (cl_word[6] ^ *art_colpf2_save) {
			int i;
			ULONG new_colour = cl_word[6] ^ *art_colpf2_save;
			new_colour |= new_colour << 16;
			*art_colpf2_save = cl_word[6];
			for (i = 0; i <= 255; i++)
				art_curtable[i] ^= art_curbkmask[i] & new_colour;
		}

	}
}

/* Initialization ---------------------------------------------------------- */

void ANTIC_Initialise(int *argc, char *argv[])
{
	int i, j;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-artif") == 0) {
			global_artif_mode = argv[++i][0] - '0';
			if (global_artif_mode < 0 || global_artif_mode > 4) {
				Aprint("Invalid artifacting mode, using default.");
				global_artif_mode = 0;
			}
		}
		else
			argv[j++] = argv[i];
	}
	*argc = j;

	artif_init();

	playfield_lookup[0x00] = 8;
	playfield_lookup[0x40] = 4;
	playfield_lookup[0x80] = 5;
	playfield_lookup[0xc0] = 6;
	playfield_lookup[0x100] = 7;
	blank_lookup[0x80] = blank_lookup[0xa0] = blank_lookup[0xc0] = blank_lookup[0xe0] = 0x00;
	hires_mask(0x00) = 0xffff;
	hires_mask(0x40) = HIRES_MASK_01;
	hires_mask(0x80) = HIRES_MASK_10;
	hires_mask(0xc0) = 0xf0f0;
	hires_lum(0x00) = hires_lum(0x40) = hires_lum(0x80) = hires_lum(0xc0) = 0;
	initialize_prior_table();
	init_pm_lookup();
	pm_lookup_ptr = pm_lookup_table[0];
	memcpy(cur_prior, prior_table, NUM_PLAYER_TYPES*5);

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

#define DO_PMG_LORES pf_colls[colreg] |= pm_pixel = *c_pm_scanline_ptr++;\
	*ptr++ = cl_word[cur_prior[pm_lookup_ptr[pm_pixel] + colreg]];

#define DO_PMG_HIRES(data) {\
	UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;\
	UBYTE pm_pixel;\
	int k = 4;\
	do {\
		pm_pixel = *c_pm_scanline_ptr++;\
		if (data & 0xc0)\
			pf_colls[6] |= pm_pixel;\
		*ptr++ = (cl_word[cur_prior[pm_lookup_ptr[pm_pixel] + 6]] & hires_mask(data & 0xc0)) | hires_lum(data & 0xc0);\
		data <<= 2;\
	} while (--k);\
}

#define DO_BORDER_CHAR	if (!(*t_pm_scanline_ptr)) {\
						ULONG *l_ptr = (ULONG *) ptr;\
						*l_ptr++ = background;\
						*l_ptr++ = background;\
						ptr = (UWORD *) l_ptr;\
					} else

#define DO_GTIA10_BORDER pm_pixel = *c_pm_scanline_ptr++;\
						 *ptr++ = cl_word[cur_prior[pm_lookup_ptr[pm_pixel | 1] + 8]];
				
#define BAK_PMG cur_prior[pm_lookup_ptr[*c_pm_scanline_ptr++] + 8]

void do_border(int left_border_chars, int right_border_chars)
{
	int kk;
	UWORD *ptr = (UWORD *) & scrn_ptr[LCHOP * 8];
	ULONG *t_pm_scanline_ptr = (ULONG *) (&pm_scanline[LCHOP * 4]);
	ULONG background;

	switch (PRIOR & 0xc0) {		/* ERU sux */
	case 0x80:	/* GTIA mode 10 */
		background = cl_word[0] | (cl_word[0] << 16);
		/* left border */
		for (kk = left_border_chars; kk; kk--) {
			DO_BORDER_CHAR {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				UBYTE pm_pixel;
				int k = 4;
				do {
					DO_GTIA10_BORDER
				} while (--k);
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
					int k = 4;
					do {
						DO_GTIA10_BORDER
					} while (--k);
				}
				t_pm_scanline_ptr++;
			}
		}
		break;
	case 0xc0:	/* GTIA mode 11 */
		background = lookup_gtia11[0];
		/* left border */
		for (kk = left_border_chars; kk; kk--) {
			DO_BORDER_CHAR {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				UBYTE colreg;
				int k = 4;
				do {
					colreg = BAK_PMG;
					*ptr++ = colreg == 7 || colreg == 8 ? cl_word[colreg] & 0xf0f0 : cl_word[colreg];
				} while (--k);
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
				int k = 4;
				do {
					colreg = BAK_PMG;
					*ptr++ = colreg == 7 || colreg == 8 ? cl_word[colreg] & 0xf0f0 : cl_word[colreg];
				} while (--k);
			}
			t_pm_scanline_ptr++;
		}
		break;
	default:	/* normal and GTIA mode 9 */
		background = lookup_gtia9[0];
		/* left border */
		for (kk = left_border_chars; kk; kk--) {
			DO_BORDER_CHAR {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				int k = 4;
				do {
					*ptr++ = cl_word[BAK_PMG];
				} while (--k);
			}
			t_pm_scanline_ptr++;
		}
		/* right border */
		ptr = (UWORD *) & scrn_ptr[right_border_start];
		t_pm_scanline_ptr = (ULONG *) (&pm_scanline[(right_border_start >> 1)]);
		for (kk = right_border_chars; kk; kk--) {
			DO_BORDER_CHAR {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				int k = 4;
				do {
					*ptr++ = cl_word[BAK_PMG];
				} while (--k);
			}
			t_pm_scanline_ptr++;
		}
		break;
	}
}

/* ANTIC modes ------------------------------------------------------------- */

static UBYTE gtia_10_lookup[] =
{8, 8, 8, 8, 4, 5, 6, 7, 8, 8, 8, 8, 4, 5, 6, 7};
static UBYTE gtia_10_pm[] =
{0x01, 0x02, 0x04, 0x08, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

#define INIT_ANTIC_2	UWORD t_chbase = (dctr ^ chbase_20) & 0xfc07;\
	xpos += font_cycles[md];\
	blank_lookup[0x60] = (anticmode == 2 || dctr & 0xe) ? 0xff : 0;\
	blank_lookup[0x00] = blank_lookup[0x20] = blank_lookup[0x40] = (dctr & 0xe) == 8 ? 0 : 0xff;

void draw_antic_2(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	INIT_BACKGROUND_6
	int i;
	INIT_ANTIC_2
	hires_norm(0x00) = cl_word[6];
	hires_norm(0x40) = hires_norm(0x10) = hires_norm(0x04) = (cl_word[6] & HIRES_MASK_01) | hires_lum(0x40);
	hires_norm(0x80) = hires_norm(0x20) = hires_norm(0x08) = (cl_word[6] & HIRES_MASK_10) | hires_lum(0x80);
	hires_norm(0xc0) = hires_norm(0x30) = hires_norm(0x0c) = (cl_word[6] & 0xf0f0) | hires_lum(0xc0);

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		UBYTE chdata;

		chdata = (screendata & invert_mask) ? 0xff : 0;
		if (blank_lookup[screendata & blank_mask])
			chdata ^= dGetByte(t_chbase + ((UWORD) (screendata & 0x7f) << 3));
		if (!(*t_pm_scanline_ptr)) {
			if (chdata) {
				*ptr++ = hires_norm(chdata & 0xc0);
				*ptr++ = hires_norm(chdata & 0x30);
				*ptr++ = hires_norm(chdata & 0x0c);
				*ptr++ = hires_norm((chdata & 0x03) << 2);
			}
			else
				DRAW_BACKGROUND(6)
		}
		else
			DO_PMG_HIRES(chdata)
		t_pm_scanline_ptr++;
	}
}

void draw_antic_2_artif(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	int i;
	ULONG screendata_tally;
	UBYTE screendata = *ANTIC_memptr++;
	UBYTE chdata;
	INIT_ANTIC_2
	chdata = (screendata & invert_mask) ? 0xff : 0;
	if (blank_lookup[screendata & blank_mask])
		chdata ^= dGetByte(t_chbase + ((UWORD) (screendata & 0x7f) << 3));
	screendata_tally = chdata;
	setup_art_colours();

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		UBYTE chdata;

		chdata = (screendata & invert_mask) ? 0xff : 0;
		if (blank_lookup[screendata & blank_mask])
			chdata ^= dGetByte(t_chbase + ((UWORD) (screendata & 0x7f) << 3));
		screendata_tally <<= 8;
		screendata_tally |= chdata;
		if (!(*t_pm_scanline_ptr))
			DRAW_ARTIF
		else {
			chdata = screendata_tally >> 8;
			DO_PMG_HIRES(chdata)
		}
		t_pm_scanline_ptr++;
	}
}

void draw_antic_2_gtia9(int nchars, UBYTE *ANTIC_memptr, UWORD *t_ptr, ULONG *t_pm_scanline_ptr)
{
	int i;
	ULONG *ptr = (ULONG *) t_ptr;
	INIT_ANTIC_2

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		UBYTE chdata;

		chdata = (screendata & invert_mask) ? 0xff : 0;
		if (blank_lookup[screendata & blank_mask])
			chdata ^= dGetByte(t_chbase + ((UWORD) (screendata & 0x7f) << 3));
		*ptr++ = lookup_gtia9[chdata >> 4];
		*ptr++ = lookup_gtia9[chdata & 0x0f];
		if (*t_pm_scanline_ptr) {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			int k = 4;
			signed char pm_reg;
			UWORD *w_ptr = (UWORD *) (ptr - 2);
			do {
				pm_reg = pm_lookup_ptr[*c_pm_scanline_ptr++];
				if (pm_reg < L_PMNONE)
				/* it is not blank or P5ONLY */
					*w_ptr = cl_word[cur_prior[pm_reg + 8]];
				else if (pm_reg == L_PM5PONLY) {
					UBYTE tmp = k > 2 ? chdata >> 4 : chdata & 0xf;
					*w_ptr = tmp | ((UWORD)tmp << 8) | cl_word[7];
				}
				/* otherwise it was L_PMNONE so do nothing */
				w_ptr++;
			} while (--k);
		}
		t_pm_scanline_ptr++;
	}
}

void draw_antic_2_gtia10(int nchars, UBYTE *ANTIC_memptr, UWORD *t_ptr, ULONG *t_pm_scanline_ptr)
{
	int i;
	ULONG *ptr = (ULONG *) (t_ptr + 1);
	ULONG lookup_gtia10[16];
	INIT_ANTIC_2

	lookup_gtia10[0] = cl_word[0] | (cl_word[0] << 16);
	lookup_gtia10[1] = cl_word[1] | (cl_word[1] << 16);
	lookup_gtia10[2] = cl_word[2] | (cl_word[2] << 16);
	lookup_gtia10[3] = cl_word[3] | (cl_word[3] << 16);
	lookup_gtia10[12] = lookup_gtia10[4] = cl_word[4] | (cl_word[4] << 16);
	lookup_gtia10[13] = lookup_gtia10[5] = cl_word[5] | (cl_word[5] << 16);
	lookup_gtia10[14] = lookup_gtia10[6] = cl_word[6] | (cl_word[6] << 16);
	lookup_gtia10[15] = lookup_gtia10[7] = cl_word[7] | (cl_word[7] << 16);
	lookup_gtia10[8] = lookup_gtia10[9] = lookup_gtia10[10] = lookup_gtia10[11] = lookup_gtia9[0];
	*t_ptr = cl_word[cur_prior[pm_lookup_ptr[(*(char *) t_pm_scanline_ptr) | 1] + 8]];
	t_pm_scanline_ptr = (ULONG *) (((UBYTE *) t_pm_scanline_ptr) + 1);
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		UBYTE chdata;

		chdata = (screendata & invert_mask) ? 0xff : 0;
		if (blank_lookup[screendata & blank_mask])
			chdata ^= dGetByte(t_chbase + ((UWORD) (screendata & 0x7f) << 3));
		*ptr++ = lookup_gtia10[chdata >> 4];
		*ptr++ = lookup_gtia10[chdata & 0x0f];
		if (*t_pm_scanline_ptr) {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			UBYTE pm_pixel;
			UBYTE colreg;
			int k = 4;
			UWORD *w_ptr = (UWORD *) (ptr - 2);
			UBYTE t_screendata = chdata >> 4;
			do {
				colreg = gtia_10_lookup[t_screendata];
				pf_colls[colreg] |= pm_pixel = *c_pm_scanline_ptr++;
				pm_pixel |= gtia_10_pm[t_screendata];
				*w_ptr++ = cl_word[cur_prior[pm_lookup_ptr[pm_pixel] + colreg]];
				if (k == 3)
					t_screendata = chdata & 0x0f;
			} while (--k);
		}
		t_pm_scanline_ptr++;
	}
}

void draw_antic_2_gtia11(int nchars, UBYTE *ANTIC_memptr, UWORD *t_ptr, ULONG *t_pm_scanline_ptr)
{
	int i;
	ULONG *ptr = (ULONG *) t_ptr;
	INIT_ANTIC_2

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		UBYTE chdata;

		chdata = (screendata & invert_mask) ? 0xff : 0;
		if (blank_lookup[screendata & blank_mask])
			chdata ^= dGetByte(t_chbase + ((UWORD) (screendata & 0x7f) << 3));
		*ptr++ = lookup_gtia11[chdata >> 4];
		*ptr++ = lookup_gtia11[chdata & 0x0f];
		if (*t_pm_scanline_ptr) {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			int k = 4;
			signed char pm_reg;
			UWORD *w_ptr = (UWORD *) (ptr - 2);
			do {
				pm_reg = pm_lookup_ptr[*c_pm_scanline_ptr++];
				if (pm_reg < L_PMNONE)
					*w_ptr = cl_word[cur_prior[pm_reg + 8]];
				else if (pm_reg == L_PM5PONLY) {
					UBYTE tmp = k > 2 ? chdata & 0xf0 : chdata << 4;
					*w_ptr = tmp ? cl_word[7] | tmp | ((UWORD)tmp << 8) : cl_word[7] & 0xf0f0;
				}
				w_ptr++;
			} while (--k);
		}
		t_pm_scanline_ptr++;
	}
}

void draw_antic_4(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	INIT_BACKGROUND_8
	int i;
	UWORD t_chbase = ((anticmode == 4 ? dctr : dctr >> 1) ^ chbase_20) & 0xfc07;

	xpos += font_cycles[md];
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
			else
				DRAW_BACKGROUND(8)
		}
		else {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			UBYTE pm_pixel;
			UBYTE colreg;
			int k = 4;
			playfield_lookup[0xc0] = screendata & 0x80 ? 7 : 6;
			do {
				colreg = playfield_lookup[chdata & 0xc0];
				DO_PMG_LORES
				chdata <<= 2;
			} while (--k);
			playfield_lookup[0xc0] = 6;
		}
		t_pm_scanline_ptr++;
	}
}

void draw_antic_6(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	int i;
	UWORD t_chbase = (anticmode == 6 ? dctr & 7 : dctr >> 1) ^ chbase_20;

	xpos += font_cycles[md];
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		UBYTE chdata;
		UWORD colour;
		int kk;
		colour = cl_word[(playfield_lookup + 0x40)[screendata & 0xc0]];
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
				chdata <<= 4;
			}
			else {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				UBYTE pm_pixel;
				UBYTE setcol = (playfield_lookup + 0x40)[screendata & 0xc0];
				UBYTE colreg;
				int k = 4;
				do {
					colreg = chdata & 0x80 ? setcol : 8;
					DO_PMG_LORES
					chdata <<= 1;
				} while (--k);

			}
			t_pm_scanline_ptr++;
		}
	}
}

void draw_antic_8(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	int i;
	lookup2[0x00] = cl_word[8];
	lookup2[0x40] = cl_word[4];
	lookup2[0x80] = cl_word[5];
	lookup2[0xc0] = cl_word[6];
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		int kk = 4;
		do {
			if (!(*t_pm_scanline_ptr)) {
				ptr[3] = ptr[2] = ptr[1] = ptr[0] = lookup2[screendata & 0xc0];
				ptr += 4;
			}
			else {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				UBYTE pm_pixel;
				UBYTE colreg = playfield_lookup[screendata & 0xc0];
				int k = 4;
				do {
					DO_PMG_LORES
				} while (--k);
			}
			screendata <<= 2;
			t_pm_scanline_ptr++;
		} while (--kk);
	}
}

void draw_antic_9(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	int i;
	lookup2[0x00] = cl_word[8];
	lookup2[0x80] = lookup2[0x40] = cl_word[4];
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		int kk = 4;
		do {
			if (!(*t_pm_scanline_ptr)) {
				ptr[1] = ptr[0] = lookup2[screendata & 0x80];
				ptr[3] = ptr[2] = lookup2[screendata & 0x40];
				ptr += 4;
				screendata <<= 2;
			}
			else {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				UBYTE pm_pixel;
				UBYTE colreg;
				int k = 4;
				do {
					colreg = (screendata & 0x80) ? 4 : 8;
					DO_PMG_LORES
					if (k & 0x01)
						screendata <<= 1;
				} while (--k);
			}
			t_pm_scanline_ptr++;
		} while (--kk);
	}
}

void draw_antic_a(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	int i;
	lookup2[0x00] = cl_word[8];
	lookup2[0x40] = lookup2[0x10] = cl_word[4];
	lookup2[0x80] = lookup2[0x20] = cl_word[5];
	lookup2[0xc0] = lookup2[0x30] = cl_word[6];
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		int kk = 2;
		do {
			if (!(*t_pm_scanline_ptr)) {
				ptr[1] = ptr[0] = lookup2[screendata & 0xc0];
				ptr[3] = ptr[2] = lookup2[screendata & 0x30];
				ptr += 4;
				screendata <<= 4;
			}
			else {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				UBYTE pm_pixel;
				UBYTE colreg;
				int k = 4;
				do {
					colreg = playfield_lookup[screendata & 0xc0];
					DO_PMG_LORES
					if (k & 0x01)
						screendata <<= 2;
				} while (--k);
			}
			t_pm_scanline_ptr++;
		} while (--kk);
	}
}

void draw_antic_c(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	int i;
	lookup2[0x00] = cl_word[8];
	lookup2[0x80] = lookup2[0x40] = lookup2[0x20] = lookup2[0x10] = cl_word[4];
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		int kk = 2;
		do {
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
				int k = 4;
				do {
					colreg = (screendata & 0x80) ? 4 : 8;
					DO_PMG_LORES
					screendata <<= 1;
				} while (--k);
			}
			t_pm_scanline_ptr++;
		} while (--kk);
	}
}

void draw_antic_e(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	INIT_BACKGROUND_8
	int i;
	lookup2[0x00] = cl_word[8];
	lookup2[0x40] = lookup2[0x10] = lookup2[0x04] = lookup2[0x01] = cl_word[4];
	lookup2[0x80] = lookup2[0x20] = lookup2[0x08] = lookup2[0x02] = cl_word[5];
	lookup2[0xc0] = lookup2[0x30] = lookup2[0x0c] = lookup2[0x03] = cl_word[6];

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		if (!(*t_pm_scanline_ptr)) {
			if (screendata) {
				*ptr++ = lookup2[screendata & 0xc0];
				*ptr++ = lookup2[screendata & 0x30];
				*ptr++ = lookup2[screendata & 0x0c];
				*ptr++ = lookup2[screendata & 0x03];
			}
			else
				DRAW_BACKGROUND(8)
		}
		else {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			UBYTE pm_pixel;
			UBYTE colreg;
			int k = 4;
			do {
				colreg = playfield_lookup[screendata & 0xc0];
				DO_PMG_LORES
				screendata <<= 2;
			} while (--k);

		}
		t_pm_scanline_ptr++;
	}
}

void draw_antic_f(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	INIT_BACKGROUND_6
	int i;

	hires_norm(0x00) = cl_word[6];
	hires_norm(0x40) = hires_norm(0x10) = hires_norm(0x04) = (cl_word[6] & HIRES_MASK_01) | hires_lum(0x40);
	hires_norm(0x80) = hires_norm(0x20) = hires_norm(0x08) = (cl_word[6] & HIRES_MASK_10) | hires_lum(0x80);
	hires_norm(0xc0) = hires_norm(0x30) = hires_norm(0x0c) = (cl_word[6] & 0xf0f0) | hires_lum(0xc0);

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		if (!(*t_pm_scanline_ptr)) {
			if (screendata) {
				*ptr++ = hires_norm(screendata & 0xc0);
				*ptr++ = hires_norm(screendata & 0x30);
				*ptr++ = hires_norm(screendata & 0x0c);
				*ptr++ = hires_norm((screendata & 0x03) << 2);
			}
			else
				DRAW_BACKGROUND(6)
		}
		else
			DO_PMG_HIRES(screendata)
		t_pm_scanline_ptr++;
	}
}

void draw_antic_f_artif(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	int i;
	ULONG screendata_tally = *ANTIC_memptr++;

	setup_art_colours();
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		screendata_tally <<= 8;
		screendata_tally |= screendata;
		if (!(*t_pm_scanline_ptr))
			DRAW_ARTIF
		else {
			screendata = ANTIC_memptr[-2];
			DO_PMG_HIRES(screendata)
		}
		t_pm_scanline_ptr++;
	}
}

void draw_antic_f_gtia9(int nchars, UBYTE *ANTIC_memptr, UWORD *t_ptr, ULONG *t_pm_scanline_ptr)
{
	int i;
	ULONG *ptr = (ULONG *) t_ptr;

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		*ptr++ = lookup_gtia9[screendata >> 4];
		*ptr++ = lookup_gtia9[screendata & 0x0f];
		if (*t_pm_scanline_ptr) {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			int k = 4;
			signed char pm_reg;
			UWORD *w_ptr = (UWORD *) (ptr - 2);
			do {
				pm_reg = pm_lookup_ptr[*c_pm_scanline_ptr++];
				if (pm_reg < L_PMNONE)
				/* it is not blank or P5ONLY */
					*w_ptr = cl_word[cur_prior[pm_reg + 8]];
				else if (pm_reg == L_PM5PONLY) {
					UBYTE tmp = k > 2 ? screendata >> 4 : screendata & 0xf;
					*w_ptr = tmp | ((UWORD)tmp << 8) | cl_word[7];
				}
				/* otherwise it was L_PMNONE so do nothing */
				w_ptr++;
			} while (--k);
		}
		t_pm_scanline_ptr++;
	}
}

void draw_antic_f_gtia10(int nchars, UBYTE *ANTIC_memptr, UWORD *t_ptr, ULONG *t_pm_scanline_ptr)
{
	int i;
	ULONG *ptr = (ULONG *) (t_ptr + 1);
	ULONG lookup_gtia10[16];
	lookup_gtia10[0] = cl_word[0] | (cl_word[0] << 16);
	lookup_gtia10[1] = cl_word[1] | (cl_word[1] << 16);
	lookup_gtia10[2] = cl_word[2] | (cl_word[2] << 16);
	lookup_gtia10[3] = cl_word[3] | (cl_word[3] << 16);
	lookup_gtia10[12] = lookup_gtia10[4] = cl_word[4] | (cl_word[4] << 16);
	lookup_gtia10[13] = lookup_gtia10[5] = cl_word[5] | (cl_word[5] << 16);
	lookup_gtia10[14] = lookup_gtia10[6] = cl_word[6] | (cl_word[6] << 16);
	lookup_gtia10[15] = lookup_gtia10[7] = cl_word[7] | (cl_word[7] << 16);
	lookup_gtia10[8] = lookup_gtia10[9] = lookup_gtia10[10] = lookup_gtia10[11] = lookup_gtia9[0];
	*t_ptr = cl_word[cur_prior[pm_lookup_ptr[(*(char *) t_pm_scanline_ptr) | 1] + 8]];
	t_pm_scanline_ptr = (ULONG *) (((UBYTE *) t_pm_scanline_ptr) + 1);
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		*ptr++ = lookup_gtia10[screendata >> 4];
		*ptr++ = lookup_gtia10[screendata & 0x0f];
		if (*t_pm_scanline_ptr) {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			UBYTE pm_pixel;
			UBYTE colreg;
			int k = 4;
			UWORD *w_ptr = (UWORD *) (ptr - 2);
			UBYTE t_screendata = screendata >> 4;
			do {
				colreg = gtia_10_lookup[t_screendata];
				pf_colls[colreg] |= pm_pixel = *c_pm_scanline_ptr++;
				pm_pixel |= gtia_10_pm[t_screendata];
				*w_ptr++ = cl_word[cur_prior[pm_lookup_ptr[pm_pixel] + colreg]];
				if (k == 3)
					t_screendata = screendata & 0x0f;
			} while (--k);
		}
		t_pm_scanline_ptr++;
	}
}

void draw_antic_f_gtia11(int nchars, UBYTE *ANTIC_memptr, UWORD *t_ptr, ULONG *t_pm_scanline_ptr)
{
	int i;
	ULONG *ptr = (ULONG *) t_ptr;

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		*ptr++ = lookup_gtia11[screendata >> 4];
		*ptr++ = lookup_gtia11[screendata & 0x0f];
		if (*t_pm_scanline_ptr) {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			int k = 4;
			signed char pm_reg;
			UWORD *w_ptr = (UWORD *) (ptr - 2);
			do {
				pm_reg = pm_lookup_ptr[*c_pm_scanline_ptr++];
				if (pm_reg < L_PMNONE)
					*w_ptr = cl_word[cur_prior[pm_reg+8]];
				else if (pm_reg == L_PM5PONLY) {
					UBYTE tmp = k > 2 ? screendata & 0xf0 : screendata << 4;
					*w_ptr = tmp ? cl_word[7] | tmp | ((UWORD)tmp << 8) : cl_word[7] & 0xf0f0;
				}
				w_ptr++;
			} while (--k);
		}
		t_pm_scanline_ptr++;
	}
}

/* GTIA-switch-to-mode-00 bug
If while drawing line in hi-res mode PRIOR is changed from 0x40..0xff to
0x00..0x3f, GTIA doesn't back to hi-res, but starts generating mode similar
to ANTIC's 0xe, but with colours PF0, PF1, PF2, PF3. */

void draw_antic_f_gtia_bug(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr)
{
	int i;
	lookup2[0x00] = cl_word[4];
	lookup2[0x40] = lookup2[0x10] = lookup2[0x04] = lookup2[0x01] = cl_word[5];
	lookup2[0x80] = lookup2[0x20] = lookup2[0x08] = lookup2[0x02] = cl_word[6];
	lookup2[0xc0] = lookup2[0x30] = lookup2[0x0c] = lookup2[0x03] = cl_word[7];

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = *ANTIC_memptr++;
		if (!(*t_pm_scanline_ptr)) {
			*ptr++ = lookup2[screendata & 0xc0];
			*ptr++ = lookup2[screendata & 0x30];
			*ptr++ = lookup2[screendata & 0x0c];
			*ptr++ = lookup2[screendata & 0x03];
		}
		else {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			UBYTE pm_pixel;
			UBYTE colreg;
			int k = 4;
			do {
				colreg = (playfield_lookup + 0x40)[screendata & 0xc0];
				DO_PMG_LORES
				screendata <<= 2;
			} while (--k);
		}
		t_pm_scanline_ptr++;
	}
}

/* pointer to a function drawing single line of graphics */
typedef void (*draw_antic_function)(int nchars, UBYTE *ANTIC_memptr, UWORD *ptr, ULONG *t_pm_scanline_ptr);

/* tables for all GTIA and ANTIC modes */
draw_antic_function draw_antic_table[4][16] = {
/* normal */
		{ NULL,			NULL,			draw_antic_2,	draw_antic_2,
		draw_antic_4,	draw_antic_4,	draw_antic_6,	draw_antic_6,
		draw_antic_8,	draw_antic_9,	draw_antic_a,	draw_antic_c,
		draw_antic_c,	draw_antic_e,	draw_antic_e,	draw_antic_f},
/* GTIA 9 */
		{ NULL,			NULL,			draw_antic_2_gtia9,	draw_antic_2_gtia9,
/* Only few of below are right... A lot of proper functions must be implemented */
		draw_antic_4,	draw_antic_4,	draw_antic_6,	draw_antic_6,
		draw_antic_8,	draw_antic_9,	draw_antic_a,	draw_antic_c,
		draw_antic_c,	draw_antic_e,	draw_antic_e,	draw_antic_f_gtia9},
/* GTIA 10 */
		{ NULL,			NULL,			draw_antic_2_gtia10,	draw_antic_2_gtia10,
		draw_antic_4,	draw_antic_4,	draw_antic_6,	draw_antic_6,
		draw_antic_8,	draw_antic_9,	draw_antic_a,	draw_antic_c,
		draw_antic_c,	draw_antic_e,	draw_antic_e,	draw_antic_f_gtia10},
/* GTIA 11 */
		{ NULL,			NULL,			draw_antic_2_gtia11,	draw_antic_2_gtia11,
		draw_antic_4,	draw_antic_4,	draw_antic_6,	draw_antic_6,
		draw_antic_8,	draw_antic_9,	draw_antic_a,	draw_antic_c,
		draw_antic_c,	draw_antic_e,	draw_antic_e,	draw_antic_f_gtia11}};

/* pointer to current GTIA/ANTIC mode */
draw_antic_function draw_antic_ptr = draw_antic_8;

/* Artifacting ------------------------------------------------------------ */

void artif_init(void)
{
#define ART_BROWN 0
#define ART_BLUE 1
#define ART_DARK_BROWN 2
#define ART_DARK_BLUE 3
#define ART_BRIGHT_BROWN 4
#define ART_BRIGHT_BLUE 5
#define ART_RED 6
#define ART_GREEN 7
	static UBYTE art_colour_table[4][8] = {
	{ 0x88, 0x14, 0x88, 0x14, 0x8f, 0x1f, 0xbb, 0x5f },	/* brownblue */
	{ 0x14, 0x88, 0x14, 0x88, 0x1f, 0x8f, 0x5f, 0xbb },	/* bluebrown */
	{ 0x46, 0xd6, 0x46, 0xd6, 0x4a, 0xdf, 0xac, 0x4f },	/* greenred */
	{ 0xd6, 0x46, 0xd6, 0x46, 0xdf, 0x4a, 0x4f, 0xac }	/* redgreen */
	};

	int i;
	int j;
	int c;
	UBYTE *art_colours;
	UBYTE q;
	UBYTE art_white;

	if (global_artif_mode == 0) {
		draw_antic_table[0][2] = draw_antic_table[0][3] = draw_antic_2;
		draw_antic_table[0][0xf] = draw_antic_f;
		return;
	}

	draw_antic_table[0][2] = draw_antic_table[0][3] = draw_antic_2_artif;
	draw_antic_table[0][0xf] = draw_antic_f_artif;

	art_colours = (global_artif_mode <= 4 ? art_colour_table[global_artif_mode - 1] : art_colour_table[2]);

	art_white = (COLPF2 & 0xf0) | (COLPF1 & 0x0f);
	art_reverse_colpf1_save = art_normal_colpf1_save = cl_word[5] & 0x0f0f;
	art_reverse_colpf2_save = art_normal_colpf2_save = cl_word[6];

	for (i = 0; i <= 255; i++) {
		art_bkmask_normal[i] = 0;
		art_lummask_normal[i] = 0;
		art_bkmask_reverse[255 - i] = 0;
		art_lummask_reverse[255 - i] = 0;

		for (j = 0; j <= 3; j++) {
			q = i << j;
			if (!(q & 0x20)) {
				if ((q & 0xf8) == 0x50)
					c = ART_BLUE;				/* 01010 */
				else if ((q & 0xf8) == 0xD8)
					c = ART_DARK_BLUE;			/* 11011 */
				else {							/* xx0xx */
					((UBYTE *)art_lookup_normal)[(i << 2) + j] = COLPF2;
					((UBYTE *)art_lookup_reverse)[((255 - i) << 2) + j] = art_white;
					((UBYTE *)art_bkmask_normal)[(i << 2) + j] = 0xff;
					((UBYTE *)art_lummask_reverse)[((255 - i) << 2) + j] = 0x0f;
					((UBYTE *)art_bkmask_reverse)[((255 - i) << 2) + j] = 0xf0;
					continue;
				}
			}
			else if (q & 0x40) {
				if (q & 0x10)
					goto colpf1_pixel;			/* x111x */
				else if (q & 0x80) {
					if (q & 0x08)
						c = ART_BRIGHT_BROWN;	/* 11101 */
					else
						goto colpf1_pixel;		/* 11100 */
				}
				else
					c = ART_GREEN;				/* 0110x */
			}
			else if (q & 0x10) {
				if (q & 0x08) {
					if (q & 0x80)
						c = ART_BRIGHT_BROWN;	/* 00111 */
					else
						goto colpf1_pixel;		/* 10111 */
				}
				else
					c = ART_RED;				/* x0110 */
			}
			else
				c = ART_BROWN;					/* x010x */


			((UBYTE *)art_lookup_reverse)[((255 - i) << 2) + j] =
			((UBYTE *)art_lookup_normal)[(i << 2) + j] = art_colours[(j & 1) ^ c];
			continue;

			colpf1_pixel:
			((UBYTE *)art_lookup_normal)[(i << 2) + j] = art_white;
			((UBYTE *)art_lookup_reverse)[((255 - i) << 2) + j] = COLPF2;
			((UBYTE *)art_bkmask_reverse)[((255 - i) << 2) + j] = 0xff;
			((UBYTE *)art_lummask_normal)[(i << 2) + j] = 0x0f;
			((UBYTE *)art_bkmask_normal)[(i << 2) + j] = 0xf0;
		}
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
	static UBYTE mode_type[32] = {
	NORMAL0, NORMAL0, NORMAL0, NORMAL0, NORMAL0, NORMAL0, NORMAL1, NORMAL1,
	NORMAL2, NORMAL2, NORMAL1, NORMAL1, NORMAL1, NORMAL0, NORMAL0, NORMAL0,
	SCROLL0, SCROLL0, SCROLL0, SCROLL0, SCROLL0, SCROLL0, SCROLL1, SCROLL1,
	SCROLL2, SCROLL2, SCROLL1, SCROLL1, SCROLL1, SCROLL0, SCROLL0, SCROLL0};
	static UBYTE normal_lastline[16] =
	{ 0, 0, 7, 9, 7, 15, 7, 15, 7, 3, 3, 1, 0, 1, 0, 0 };
	UBYTE vscrol_flag = FALSE;
	UBYTE no_jvb = TRUE;
	UBYTE need_load = FALSE;
#ifndef NO_GTIA11_DELAY
	int delayed_gtia11 = 250;
#endif

	ypos = 0;
	do {
		POKEY_Scanline();		/* check and generate IRQ */
		OVERSCREEN_LINE;
	} while (ypos < 8);

	scrn_ptr = (UBYTE *) atari_screen;
	need_dl = TRUE;
	do {
		POKEY_Scanline();		/* check and generate IRQ */
		pmg_dma();

		if (need_dl) {
			if (DMACTL & 0x20) {
				/* PMG flickering :-) */
				if (missile_flickering)
					GRAFM = dGetByte((UWORD)(regPC - xpos + 1));
				if (player_flickering) {
					GRAFP0 = dGetByte((UWORD)(regPC - xpos + 3));
					GRAFP1 = dGetByte((UWORD)(regPC - xpos + 4));
					GRAFP2 = dGetByte((UWORD)(regPC - xpos + 5));
					GRAFP3 = dGetByte((UWORD)(regPC - xpos + 6));
				}
				IR = get_DL_byte();
				anticmode = IR & 0xf;
			}
			else
				IR &= 0x7f;	/* repeat last instruction, but don't generate DLI */

			dctr = 0;
			need_dl = FALSE;
			vscrol_off = FALSE;

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
				if (IR & 0x40 && DMACTL & 0x20) {
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
				if (IR & 0x40 && DMACTL & 0x20)
					screenaddr = get_DL_word();
				md = mode_type[IR & 0x1f];
				need_load = TRUE;
				draw_antic_ptr = draw_antic_table[(PRIOR & 0xc0) >> 6][anticmode];
				break;
			}
		}

		if (anticmode == 1 && DMACTL & 0x20)
			dlist = get_DL_word();

		if (dctr == lastline) {
			if (no_jvb)
				need_dl = TRUE;
			if (IR & 0x80) {
				GO(NMIST_C);
				NMIST = 0x9f;
				if(NMIEN & 0x80) {
					GO(NMI_C);
					NMI();
				}
			}
		}

		if (!draw_display) {
			xpos += DMAR;
			if (anticmode < 2 || (DMACTL & 3) == 0) {
				GOEOL;
				if (no_jvb) {
					dctr++;
					dctr &= 0xf;
				}
				continue;
			}
			if (need_load) {
				need_load = FALSE;
				xpos += load_cycles[md];
				if (anticmode <= 5)	/* extra cycles in font modes */
					xpos += before_cycles[md] - extra_cycles[md];
			}
			if (anticmode < 8)
				xpos += font_cycles[md];
			GOEOL;
			dctr++;
			dctr &= 0xf;
			continue;
		}

		if (need_load && anticmode >=2 && anticmode <= 5 && DMACTL & 3)
			xpos += before_cycles[md];

		GO(SCR_C);
		new_pm_scanline();

		xpos += DMAR;

		if (anticmode < 2 || (DMACTL & 3) == 0) {
			if (pm_dirty)
				do_border(48 - LCHOP - RCHOP, 0);
			else
				memset(scrn_ptr + 8 * LCHOP,
				PRIOR < 0x80 ? COLBK : PRIOR < 0xc0 ? COLPM0 : COLBK & 0xf0,
				(48 - LCHOP - RCHOP) * 8);
			GOEOL;
			scrn_ptr += ATARI_WIDTH;
			if (no_jvb) {
				dctr++;
				dctr &= 0xf;
			}
			continue;
		}

		if (need_load) {
			need_load = FALSE;
			ANTIC_load();
			xpos += load_cycles[md];
			if (anticmode <= 5)	/* extra cycles in font modes */
				xpos -= extra_cycles[md];
		}

		draw_antic_ptr(chars_displayed[md],
			ANTIC_memory + ANTIC_margin + ch_offset[md],
			(UWORD *) (scrn_ptr + x_min[md]),
			(ULONG *) (&pm_scanline[x_min[md] >> 1]));
		do_border(left_border_chars, right_border_chars);
#ifndef NO_GTIA11_DELAY
		if (PRIOR >= 0xc0)
			delayed_gtia11 = ypos + 1;
		else
			if (ypos == delayed_gtia11) {
				ULONG *ptr = (ULONG *)(scrn_ptr + 8 * LCHOP);
				int k = 2 * (48 - LCHOP - RCHOP);
				do {
					*ptr |= ptr[-ATARI_WIDTH / 4];
					ptr++;
				} while (--k);
			}
#endif
		GOEOL;
		scrn_ptr += ATARI_WIDTH;
		dctr++;
		dctr &= 0xf;
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

	do {
		POKEY_Scanline();		/* check and generate IRQ */
		OVERSCREEN_LINE;
	} while (ypos < max_ypos);
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

/* GTIA calls it on write to PRIOR */
void set_prior(UBYTE byte) {
	if ((byte ^ PRIOR) & 0x0f)
		memcpy(cur_prior, &prior_table[(byte & 0x0f) * (NUM_PLAYER_TYPES * 5)], NUM_PLAYER_TYPES * 5);
	pm_lookup_ptr = pm_lookup_table[(byte & 0x30) >> 4];
	if (byte < 0x40 && PRIOR >= 0x40 && anticmode == 0xf && xpos >= ((DMACTL & 3) == 3 ? 20 : 22))
		draw_antic_ptr = draw_antic_f_gtia_bug;
	else
		draw_antic_ptr = draw_antic_table[(byte & 0xc0) >> 6][anticmode];
}

void ANTIC_PutByte(UWORD addr, UBYTE byte)
{
	switch (addr & 0xf) {
	case _CHACTL:
		if ((CHACTL ^ byte) & 4)
			chbase_20 ^= 7;
		CHACTL = byte;
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
			/* no ANTIC_load when screen off */
			/* chars_read[NORMAL0] = 0;
			chars_read[NORMAL1] = 0;
			chars_read[NORMAL2] = 0;
			chars_read[SCROLL0] = 0;
			chars_read[SCROLL1] = 0;
			chars_read[SCROLL2] = 0; */
			/* no draw_antic_* when screen off */
			/* chars_displayed[NORMAL0] = 0;
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
			ch_offset[SCROLL2] = 0; */
			/* no borders when screen off, only background */
			/* left_border_chars = 48 - LCHOP - RCHOP;
			right_border_chars = 0;
			right_border_start = 0; */
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
			font_cycles[NORMAL0] = load_cycles[NORMAL0] = 32;
			font_cycles[NORMAL1] = load_cycles[NORMAL1] = 16;
			load_cycles[NORMAL2] = 8;
			before_cycles[NORMAL0] = BEFORE_CYCLES;
			before_cycles[SCROLL0] = BEFORE_CYCLES + 8;
			extra_cycles[NORMAL0] = 7 + BEFORE_CYCLES;
			extra_cycles[SCROLL0] = 8 + BEFORE_CYCLES + 8;
			left_border_chars = 8 - LCHOP;
			right_border_chars = 8 - RCHOP;
			right_border_start = ATARI_WIDTH - 64;
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
			font_cycles[NORMAL0] = load_cycles[NORMAL0] = 40;
			font_cycles[NORMAL1] = load_cycles[NORMAL1] = 20;
			load_cycles[NORMAL2] = 10;
			before_cycles[NORMAL0] = BEFORE_CYCLES + 8;
			before_cycles[SCROLL0] = BEFORE_CYCLES + 16;
			extra_cycles[NORMAL0] = 8 + BEFORE_CYCLES + 8;
			extra_cycles[SCROLL0] = 7 + BEFORE_CYCLES + 16;
			left_border_chars = 4 - LCHOP;
			right_border_chars = 4 - RCHOP;
			right_border_start = ATARI_WIDTH - 32;
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
			font_cycles[NORMAL0] = load_cycles[NORMAL0] = 47;
			font_cycles[NORMAL1] = load_cycles[NORMAL1] = 24;
			load_cycles[NORMAL2] = 12;
			before_cycles[NORMAL0] = BEFORE_CYCLES + 16;
			before_cycles[SCROLL0] = BEFORE_CYCLES + 16;
			extra_cycles[NORMAL0] = 7 + BEFORE_CYCLES + 16;
			extra_cycles[SCROLL0] = 7 + BEFORE_CYCLES + 16;
			left_border_chars = 3 - LCHOP;
			right_border_chars = ((1 - LCHOP) < 0) ? (0) : (1 - LCHOP);
			right_border_start = ATARI_WIDTH - 8;
			break;
		}

		missile_dma_enabled = (DMACTL & 0x0c);	/* no player dma without missile */
		player_dma_enabled = (DMACTL & 0x08);
		singleline = (DMACTL & 0x10);
		player_flickering = ((player_dma_enabled | player_gra_enabled) == 0x02);
		missile_flickering = ((missile_dma_enabled | missile_gra_enabled) == 0x01);

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
			chars_displayed[SCROLL2] = chars_displayed[NORMAL2];
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
				x_min[SCROLL2] = x_min[NORMAL2] + (HSCROL << 1);
				ch_offset[SCROLL2] = 0;
			}
			else {
				chars_displayed[SCROLL1] = chars_displayed[NORMAL1];
				ch_offset[SCROLL1] = 2 - (HSCROL >> 3);
				x_min[SCROLL1] = x_min[NORMAL0];
				if (HSCROL) {
					if (HSCROL & 7) {
						x_min[SCROLL1] += ((HSCROL & 7) << 1) - 16;
						chars_displayed[SCROLL1]++;
						ch_offset[SCROLL1]--;
					}
					x_min[SCROLL2] = x_min[NORMAL2] + (HSCROL << 1) - 32;
					chars_displayed[SCROLL2]++;
					ch_offset[SCROLL2] = 0;
				}
				else {
					x_min[SCROLL2] = x_min[NORMAL2];
					ch_offset[SCROLL2] = 1;
				}
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
				need_dl = dctr == lastline;
		}
		break;
	case _PMBASE:
		PMBASE = byte;
		pmbase_d = (byte & 0xfc) << 8;
		pmbase_s = pmbase_d & 0xf8ff;
		break;
	case _CHBASE:
		CHBASE = byte;
		chbase_20 = (byte & 0xfe) << 8;
		if (CHACTL & 4)
			chbase_20 ^= 7;
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
	SaveUBYTE( &DMACTL, 1 );
	SaveUBYTE( &CHACTL, 1 );
	SaveUBYTE( &HSCROL, 1 );
	SaveUBYTE( &VSCROL, 1 );
	SaveUBYTE( &PMBASE, 1 );
	SaveUBYTE( &CHBASE, 1 );
	SaveUBYTE( &NMIEN, 1 );
	SaveUBYTE( &NMIST, 1 );
	SaveUBYTE( &IR, 1 );
	SaveUBYTE( &anticmode, 1 );
	SaveUBYTE( &dctr, 1 );
	SaveUBYTE( &lastline, 1 );
	SaveUBYTE( &need_dl, 1 );
	SaveUBYTE( &vscrol_off, 1 );

	SaveUWORD( &dlist, 1 );
	SaveUWORD( &screenaddr, 1 );
	
	SaveINT( &xpos, 1 );
	SaveINT( &xpos_limit, 1 );
	SaveINT( &ypos, 1 );
}

void AnticStateRead( void )
{
	ReadUBYTE( &DMACTL, 1 );
	ReadUBYTE( &CHACTL, 1 );
	ReadUBYTE( &HSCROL, 1 );
	ReadUBYTE( &VSCROL, 1 );
	ReadUBYTE( &PMBASE, 1 );
	ReadUBYTE( &CHBASE, 1 );
	ReadUBYTE( &NMIEN, 1 );
	ReadUBYTE( &NMIST, 1 );
	ReadUBYTE( &IR, 1 );
	ReadUBYTE( &anticmode, 1 );
	ReadUBYTE( &dctr, 1 );
	ReadUBYTE( &lastline, 1 );
	ReadUBYTE( &need_dl, 1 );
	ReadUBYTE( &vscrol_off, 1 );

	ReadUWORD( &dlist, 1 );
	ReadUWORD( &screenaddr, 1 );
	
	ReadINT( &xpos, 1 );
	ReadINT( &xpos_limit, 1 );
	ReadINT( &ypos, 1 );

	ANTIC_PutByte(_DMACTL, DMACTL);
	ANTIC_PutByte(_CHACTL, CHACTL);
	ANTIC_PutByte(_PMBASE, PMBASE);
	ANTIC_PutByte(_CHBASE, CHBASE);
}
