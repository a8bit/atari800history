#include <stdio.h>
#include <stdlib.h>				/* for atoi() */
#include <string.h>

#define LCHOP 3					/* do not build lefmost 0..3 characters in wide mode */
#define RCHOP 3					/* do not build rightmost 0..3 characters in wide mode */

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
#include "sio.h"
#include "log.h"
#include "statesav.h"

#define FALSE 0
#define TRUE 1

UBYTE CHACTL;
UBYTE CHBASE;
UBYTE DMACTL;
UBYTE HSCROL;
UBYTE NMIEN;
UBYTE NMIST;
UBYTE PMBASE;
UBYTE VSCROL;
UWORD dlist;

int global_artif_mode;

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

#define PF2_COLPF0 0x0404
#define PF2_COLPF1 0x0505
#define PF2_COLPF2 0x0606
#define PF2_COLPF3 0x0707
#define PF2_COLBK 0x0808

#define PF4_COLPF0 0x04040404
#define PF4_COLPF1 0x05050505
#define PF4_COLPF2 0x06060606
#define PF4_COLPF3 0x07070707
#define PF4_COLBK 0x08080808

int ypos;

unsigned int cpu_clock;			/* Global tacts counter */
static unsigned int last_cpu_clock;/* local temporary conter for line sync */

#define CPUL	114				/* 114 CPU cycles for 1 screenline */
#define DMAR	9				/* 9 cycles for DMA refresh */
#define WSYNC	7				/* 7 cycles for wsync */
#define VCOUNTDELAY 3			/*delay for VCOUNT change after wsync */
#define BEGL	15				/* 15 cycles for begin of each screenline */

/* Table of Antic "beglinecycles" from real atari */
/* middle of sceenline */
static int realcyc[128] =
{58, 58, 58, 0, 58, 58, 58, 0,	/* blank lines */
 58, 58, 58, 0, 58, 58, 58, 0,	/* --- */
 27, 23, 15, 0, 41, 37, 31, 0,	/* antic2, gr.0 */
 27, 23, 15, 0, 41, 37, 31, 0,	/* antic3, gr.0(8x10) */
 27, 23, 15, 0, 41, 37, 31, 0,	/* antic4, gr.12 */
 27, 23, 15, 0, 41, 37, 31, 0,	/* antic5, gr.13 */
 40, 36, 32, 0, 49, 47, 45, 0,	/* antic6, gr.1 */
 40, 36, 32, 0, 49, 47, 45, 0,	/* antic7, gr.2 */

 53, 52, 51, 0, 58, 58, 58, 0,	/* antic8, gr.3 */
 53, 52, 51, 0, 58, 58, 58, 0,	/* antic9, gr.4 */
 49, 47, 45, 0, 58, 58, 58, 0,	/* antic10, gr.5 */
 49, 47, 45, 0, 58, 58, 58, 0,	/* antic11, gr.6 */
 49, 47, 45, 0, 58, 58, 58, 0,	/* antic12, gr.14 */
 40, 36, 32, 0, 58, 58, 58, 0,	/* antic13, gr.7 */
 40, 36, 32, 0, 58, 58, 58, 0,	/* antic14, gr.15 */
 40, 36, 32, 0, 58, 58, 58, 0	/* antic15, gr.8 */
};

/* Table of nlines for Antic modes */
/*
static int an_nline[16] =
{1, 1, 8, 10, 8, 16, 8, 16,
 8, 4, 4, 2, 1, 2, 1, 1};
*/

int dmaw;						/* DMA width = 4,5,6 */

/* have made these globals for now:PM */
int unc;						/* unused cycles (can be < 0 !) */
int dlisc;						/* screenline with horizontal blank interrupt */

int firstlinecycles;			/* DMA cycles for first line */
int nextlinecycles;				/* DMA cycles for next lines */
int pmgdmac;					/* DMA cycles for PMG */
int dldmac;						/* DMA cycles for read display-list */

int allc;						/* temporary (sum of cycles) */
int begcyc;						/* temporary (cycles for begin of screenline) */
int anticmode;					/* temporary (Antic mode) */
int anticm8;					/* temporary (anticmode<<3) */

/*
 * Pre-computed values for improved performance
 */

static UWORD chbase_40;			/* CHBASE for 40 character mode */
static UWORD chbase_20;			/* CHBASE for 20 character mode */
static int scroll_offset;

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
static UBYTE IR;				/* made a global */

static int hscrol_flag;
static int chars_read[6];
static int chars_displayed[6];
static int x_min[6];
static int ch_offset[6];
#define NORMAL0 0
#define NORMAL1 1
#define NORMAL2 2
#define SCROLL0 3
#define SCROLL1 4
#define SCROLL2 5
static int base_scroll_char_offset;
static int base_scroll_char_offset2;
static int base_scroll_char_offset3;
static int mode_type;

static int left_border_chars;
static int right_border_chars;
static int right_border_start;
extern UBYTE pm_scanline[ATARI_WIDTH];
extern UBYTE pf_colls[9];

static int normal_lastline;
int wsync_halt = 0;
int antic23f = 0;
int pmg_dma(void);
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

int old_new_pm_lookup[16] =
{
	L_PMNONE,					/* 0000 - None */
	L_PM0,						/* 0001 - Player 0 */
	L_PM1,						/* 0010 - Player 1 */
	L_PM0,
/* 0011 - Player 0 */ /**0OR1   3 */
	L_PM2,						/* 0100 - Player 2 */
	L_PM0,						/* 0101 - Player 0 */
	L_PM1,						/* 0110 - Player 1 */
	L_PM0,
/* 0111 - Player 0 */ /**0OR1   7 */
	L_PM3,						/* 1000 - Player 3 */
	L_PM0,						/* 1001 - Player 0 */
	L_PM1,						/* 1010 - Player 1 */
	L_PM0,
/* 1011 - Player 0 */ /**0OR1   11 */
	L_PM2,
/* 1100 - Player 2 */ /**2OR3   12 */
	L_PM0,						/* 1101 - Player 0 */
	L_PM1,						/* 1110 - Player 1 */
	L_PM0,
/* 1111 - Player 0 */ /**0OR1   15 */
};

signed char *new_pm_lookup;
signed char pm_lookup_normal[256];
signed char pm_lookup_multi[256];
signed char pm_lookup_5p[256];
signed char pm_lookup_multi_5p[256];

static UBYTE playfield_lookup[256];		/* what size should this be to be fastest? */

/*
   =============================================================
   Define screen as ULONG to ensure that it is Longword aligned.
   This allows special optimisations under certain conditions.
   -------------------------------------------------------------
   The extra 16 scanlines is used as an overflow buffer and has
   enough room for any extra mode line. It is needed on the
   occasions that a valid JVB instruction is not found in the
   display list - An automatic break out will occur when ypos
   is greater than the ATARI_HEIGHT, if its one less than this
   there must be enough space for another mode line.
   =============================================================
 */

ULONG *atari_screen = NULL;
#ifdef BITPL_SCR
ULONG *atari_screen_b = NULL;
ULONG *atari_screen1 = NULL;
ULONG *atari_screen2 = NULL;
#endif
UBYTE *scrn_ptr;
UBYTE prior_table[5*NUM_PLAYER_TYPES * 16];
UBYTE cur_prior[5*NUM_PLAYER_TYPES];
#define R_BLACK 23
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
UWORD cl_word[24];

/* please note Antic can address just 4kB block */
UWORD increment_Antic_counter(UWORD counter, int increment)
{
	UWORD result = counter & 0xF000;
	result |= ((counter + increment) & 0x0FFF);

	return result;
}

void initialize_prior_table(void)
{
	UBYTE player_colreg[] =
	{0, 1, 9, 2, 3, 10, 0};
	int i, j,jj, k,kk;
	for (i = 0; i <= 15; i++) {	/* prior setting */

		for (jj = 0; jj <= (NUM_PLAYER_TYPES-1); jj++) {	/* player */

			for (kk = 0; kk <= 4; kk++) {	/* playfield */
                                int c = 0;		/* colreg */
                                k=kk;
				j=jj;
				if (jj==13)
				   c=3+4; /*5th player by itself*/
                                else if (jj == 12)
					c = k + 4;	/* no player */
                                else
                                {
                                if (jj >= 6 && jj<= 11){
                                k=3; /*5th player with other players*/
				j=jj-6; /*other player that is with it*/
				}
				if (k == 4)
					c = player_colreg[j];
				else {
					if (k <= 1) {	/* playfields 0 and 1 */

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

							if ((i & 0x01) == 0) {
								c = k + 4;
							}
							else {
								c = player_colreg[j];
							}
						}
					}
					else {		/* playfields 2 and 3 */

						if (j <= 2) {	/* players 01 and 0OR1 */

							if ((i <= 3) || (i >= 8 && i <= 11)) {
								c = player_colreg[j];
							}
							else {
								c = k + 4;
							}
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
				}
				prior_table[i*NUM_PLAYER_TYPES*5 + jj * 5 + kk] = c;

			}
		}
	}
}

void init_pm_lookup(void){
	int i;
	int pm;
	signed char t;
	for(i=0;i<=255;i++){
		pm=( (i&0x0f)|(i>>4) );
		t=old_new_pm_lookup[pm];
		pm_lookup_normal[i]=t;
		if(pm==3 || pm==7 || pm==11 || pm==15) t=L_PM01;
		if(pm==12) t=L_PM23;
		pm_lookup_multi[i]=t;
		pm=(i&0x0f);
		t=old_new_pm_lookup[pm];
		pm_lookup_5p[i]=t;
		if(pm==3 || pm==7 || pm==11 || pm==15) t=L_PM01;
		if(pm==12) t=L_PM23;
		pm_lookup_multi_5p[i]=t;
		if(i>15) { /* 5th player adds 6*5 */
			if((i&0x0f)!=0) {
				pm_lookup_5p[i]+=6*5;
				pm_lookup_multi_5p[i]+=6*5;
			}
			else {
				pm_lookup_5p[i]=L_PM5PONLY;
				pm_lookup_multi_5p[i]=L_PM5PONLY;
			}
		}
	}
}

/* artifacting stuff */
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
static void setup_art_colours();
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

static void setup_art_colours()
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
			i++;
			artif_mode = atoi(argv[i]);
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
	initialize_prior_table();
	init_pm_lookup();
	GTIA_PutByte(_PRIOR, 0xff);
	GTIA_PutByte(_PRIOR, 0x00);

	player_dma_enabled = missile_dma_enabled = 0;
	player_gra_enabled = missile_gra_enabled = 0;
	player_flickering = missile_flickering = 0;
	GRAFP0 = GRAFP1 = GRAFP2 = GRAFP3 = GRAFM = 0;
}

/*
   *****************************************************************
   *                                *
   *    Section         :   Antic Display Modes *
   *    Original Author     :   David Firth     *
   *    Date Written        :   28th May 1995       *
   *    Version         :   1.0         *
   *                                *
   *                                *
   *   Description                          *
   *   -----------                          *
   *                                *
   *   Section that handles Antic display modes. Not required   *
   *   for BASIC version.                       *
   *                                *
   *****************************************************************
 */

int xmin;
int xmax;

int dmactl_xmin_noscroll;
int dmactl_xmax_noscroll;
static int dmactl_xmin_scroll;
static int dmactl_xmax_scroll;
static int char_delta;
static int flip_mask;
static int char_offset;
static int invert_mask;
static int blank_mask;

static UWORD screenaddr;
static UWORD lookup1[256];
static UWORD lookup2[256];
static ULONG lookup_gtia[16];

static int vskipbefore = 0;
static int vskipafter = 0;

#define DO_PMG pf_colls[colreg]|=pm_pixel;\
               colreg=cur_prior[new_pm_lookup[pm_pixel]+colreg];

void do_border(void)
{
	int kk;
	int temp_border_chars;
	int pass;
	UWORD *ptr = (UWORD *) & scrn_ptr[LCHOP * 8];
	ULONG *t_pm_scanline_ptr = (ULONG *) (&pm_scanline[LCHOP * 4]);
	ULONG COL_8_LONG;

	if ((PRIOR & 0xC0) == 0x80)                       /* ERU */
		COL_8_LONG = cl_word[0] | (cl_word[0] << 16); /* ERU */
	else                                              /* ERU */
		COL_8_LONG = cl_word[8] | (cl_word[8] << 16);
	temp_border_chars = left_border_chars;
	pass = 2;
	while (pass--) {
		for (kk = temp_border_chars; kk; kk--) {
			if (!(*t_pm_scanline_ptr)) {
				ULONG *l_ptr = (ULONG *) ptr;

				*l_ptr++ = COL_8_LONG;	/* PF4_COLPF2; */

				*l_ptr++ = COL_8_LONG;	/* PF4_COLPF2; */

				ptr = (UWORD *) l_ptr;
			}
			else {
				UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
				UBYTE pm_pixel;
				UBYTE colreg;
				int k;
				for (k = 1; k <= 4; k++) {
					pm_pixel = *c_pm_scanline_ptr++;
					colreg = 8;
					DO_PMG
						* ptr++ = cl_word[colreg];
				}
			}
			t_pm_scanline_ptr++;
		}
		ptr = (UWORD *) & scrn_ptr[right_border_start];
		t_pm_scanline_ptr = (ULONG *) (&pm_scanline[(right_border_start >> 1)]);
		temp_border_chars = right_border_chars;
	}
}

void draw_antic_2(int j, int nchars, UWORD t_screenaddr, char *ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	ULONG COL_6_LONG;
	int i;
	lookup1[0x00] = colour_lookup[6];
	lookup1[0x80] = lookup1[0x40] = lookup1[0x20] =
		lookup1[0x10] = lookup1[0x08] = lookup1[0x04] =
		lookup1[0x02] = lookup1[0x01] = (colour_lookup[6] & 0xf0) | (colour_lookup[5] & 0x0f);
	COL_6_LONG = cl_word[6] | (cl_word[6] << 16);
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = dGetByte(t_screenaddr++);
		UWORD chaddr;
		UBYTE invert;
		UBYTE blank;
		UBYTE chdata;

		chaddr = t_chbase + ((UWORD) (screendata & 0x7f) << 3);
		if (screendata & invert_mask)
			invert = 0xff;
		else
			invert = 0x00;
		if (screendata & blank_mask)
			blank = 0x00;
		else
			blank = 0xff;
		if ((j & 0x0e) == 0x08 && (screendata & 0x60) != 0x60)
			chdata = invert;
		else
			chdata = (dGetByte(chaddr) & blank) ^ invert;
		if (!(*t_pm_scanline_ptr)) {
			if (chdata) {
				*ptr++ = (char) lookup1[chdata & 0x80];
				*ptr++ = (char) lookup1[chdata & 0x40];
				*ptr++ = (char) lookup1[chdata & 0x20];
				*ptr++ = (char) lookup1[chdata & 0x10];
				*ptr++ = (char) lookup1[chdata & 0x08];
				*ptr++ = (char) lookup1[chdata & 0x04];
				*ptr++ = (char) lookup1[chdata & 0x02];
				*ptr++ = (char) lookup1[chdata & 0x01];
			}
			else {
#ifdef UNALIGNED_LONG_OK
				ULONG *l_ptr = (ULONG *) ptr;

				*l_ptr++ = COL_6_LONG;
				*l_ptr++ = COL_6_LONG;

				ptr = (UBYTE *) l_ptr;
#else
				UWORD *w_ptr = (UWORD *) ptr;

				*w_ptr++ = PF2_COLPF2;
				*w_ptr++ = PF2_COLPF2;
				*w_ptr++ = PF2_COLPF2;
				*w_ptr++ = PF2_COLPF2;

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
void draw_antic_2_artif(int j, int nchars, UWORD t_screenaddr, char *ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	ULONG COL_6_LONG;
	int i;
	ULONG screendata_tally;
        UBYTE screendata;
        UWORD chaddr;
	UBYTE invert;
	UBYTE blank;
	UBYTE chdata;
        lookup1[0x00] = colour_lookup[6];
	lookup1[0x80] = lookup1[0x40] = lookup1[0x20] =
		lookup1[0x10] = lookup1[0x08] = lookup1[0x04] =
		lookup1[0x02] = lookup1[0x01] = (colour_lookup[6] & 0xf0) | (colour_lookup[5] & 0x0f);

	COL_6_LONG = cl_word[6] | (cl_word[6] << 16);
	screendata=dGetByte(t_screenaddr++);
    /*   t_screenaddr++; for ART */
	chaddr = t_chbase + ((UWORD) (screendata & 0x7f) << 3);
	if (screendata & invert_mask)
		invert = 0xff;
	else
		invert = 0x00;
	if (screendata & blank_mask)
		blank = 0x00;
	else
		blank = 0xff;
	if ((j & 0x0e) == 0x08 && (screendata & 0x60) != 0x60)
		chdata = invert;
	else
		chdata = (dGetByte(chaddr) & blank) ^ invert;
	screendata_tally=chdata;
        setup_art_colours();
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = dGetByte(t_screenaddr++);
		UWORD chaddr;
		UBYTE invert;
		UBYTE blank;
		UBYTE chdata;

		chaddr = t_chbase + ((UWORD) (screendata & 0x7f) << 3);
		if (screendata & invert_mask)
			invert = 0xff;
		else
			invert = 0x00;
		if (screendata & blank_mask)
			blank = 0x00;
		else
			blank = 0xff;
		if ((j & 0x0e) == 0x08 && (screendata & 0x60) != 0x60)
			chdata = invert;
		else
			chdata = (dGetByte(chaddr) & blank) ^ invert;
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
   			/* screendata=dGetByte(t_screenaddr-2); */
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
void draw_antic_2_gtia9_11(int j, int nchars, UWORD t_screenaddr, char *t_ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	int i;
	ULONG *ptr = (ULONG *) (t_ptr);
	ULONG temp_count = 0;
	ULONG base_colour = colour_lookup[8];
	ULONG increment;
	base_colour = base_colour | (base_colour << 8);
	base_colour = base_colour | (base_colour << 16);
	if ((PRIOR & 0xC0) == 0x40)
		increment = 0x01010101;
	else
		increment = 0x10101010;
	for (i = 0; i <= 15; i++) {
		lookup_gtia[i] = base_colour | temp_count;
		temp_count += increment;
	}

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = dGetByte(t_screenaddr++);
		UWORD chaddr;
		UBYTE invert;
		UBYTE blank;
		UBYTE chdata;

		chaddr = t_chbase + ((UWORD) (screendata & 0x7f) << 3);
		if (screendata & invert_mask)
			invert = 0xff;
		else
			invert = 0x00;
		if (screendata & blank_mask)
			blank = 0x00;
		else
			blank = 0xff;
		if ((j & 0x0e) == 0x08 && (screendata & 0x60) != 0x60)
			chdata = invert;
		else
			chdata = (dGetByte(chaddr) & blank) ^ invert;
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
void draw_antic_3(int j, int nchars, UWORD t_screenaddr, char *ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	ULONG COL_6_LONG;
	int i;
	lookup1[0x00] = colour_lookup[6];
	lookup1[0x80] = lookup1[0x40] = lookup1[0x20] =
		lookup1[0x10] = lookup1[0x08] = lookup1[0x04] =
		lookup1[0x02] = lookup1[0x01] = (colour_lookup[6] & 0xf0) | (colour_lookup[5] & 0x0f);
	COL_6_LONG = cl_word[6] | (cl_word[6] << 16);
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = dGetByte(t_screenaddr++);
		UWORD chaddr;
		UBYTE invert;
		UBYTE blank;
		UBYTE chdata;

		chaddr = t_chbase + ((UWORD) (screendata & 0x7f) << 3);
		if (screendata & invert_mask)
			invert = 0xff;
		else
			invert = 0x00;
		if (screendata & blank_mask)
			blank = 0x00;
		else
			blank = 0xff;
/* only need to change this line from antic_2 vvvvvvvvv */
		/*     if((j&0x0e)==0x08 && (screendata&0x60)!=0x60) */
		if ((((screendata & 0x60) == 0x60) && ((j & 0x0e) == 0x00)) || (((screendata & 0x60) != 0x60) && ((j & 0x0e) == 0x08)))
			chdata = invert;
		else
			chdata = (dGetByte(chaddr) & blank) ^ invert;
		if (!(*t_pm_scanline_ptr)) {
			if (chdata) {
				*ptr++ = (char) lookup1[chdata & 0x80];
				*ptr++ = (char) lookup1[chdata & 0x40];
				*ptr++ = (char) lookup1[chdata & 0x20];
				*ptr++ = (char) lookup1[chdata & 0x10];
				*ptr++ = (char) lookup1[chdata & 0x08];
				*ptr++ = (char) lookup1[chdata & 0x04];
				*ptr++ = (char) lookup1[chdata & 0x02];
				*ptr++ = (char) lookup1[chdata & 0x01];
			}
			else {
#ifdef UNALIGNED_LONG_OK
				ULONG *l_ptr = (ULONG *) ptr;

				*l_ptr++ = COL_6_LONG;
				*l_ptr++ = COL_6_LONG;

				ptr = (UBYTE *) l_ptr;
#else
				UWORD *w_ptr = (UWORD *) ptr;

				*w_ptr++ = PF2_COLPF2;
				*w_ptr++ = PF2_COLPF2;
				*w_ptr++ = PF2_COLPF2;
				*w_ptr++ = PF2_COLPF2;

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

void draw_antic_4(int j, int nchars, UWORD t_screenaddr, char *t_ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	int i;
	UWORD *ptr = (UWORD *) (t_ptr);
#define LOOKUP1_4COL  lookup1[0x00] = cl_word[8];\
  lookup1[0x40] = lookup1[0x10] = lookup1[0x04] = lookup1[0x01] = cl_word[4];\
  lookup1[0x80] = lookup1[0x20] = lookup1[0x08] = lookup1[0x02] = cl_word[5];\
  lookup1[0xc0] = lookup1[0x30] = lookup1[0x0c] = lookup1[0x03] = cl_word[6];
	LOOKUP1_4COL
/*
   ==================================
   Pixel values when character >= 128
   ==================================
 */
		lookup2[0x00] = cl_word[8];
	lookup2[0x40] = lookup2[0x10] = lookup2[0x04] = lookup2[0x01] = cl_word[4];
	lookup2[0x80] = lookup2[0x20] = lookup2[0x08] = lookup2[0x02] = cl_word[5];
	lookup2[0xc0] = lookup2[0x30] = lookup2[0x0c] = lookup2[0x03] = cl_word[7];

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = dGetByte(t_screenaddr++);
		UWORD chaddr;
		UWORD *lookup;
		UBYTE chdata;
		chaddr = t_chbase + ((UWORD) (screendata & 0x7f) << 3);
		if (screendata & 0x80)
			lookup = lookup2;
		else
			lookup = lookup1;
		chdata = dGetByte(chaddr);
		if (!(*t_pm_scanline_ptr)) {
			if (chdata) {
				UWORD colour;

				colour = lookup[chdata & 0xc0];
				*ptr++ = colour;

				colour = lookup[chdata & 0x30];
				*ptr++ = colour;

				colour = lookup[chdata & 0x0c];
				*ptr++ = colour;

				colour = lookup[chdata & 0x03];
				*ptr++ = colour;
			}
			else {
				*ptr++ = lookup1[0];
				*ptr++ = lookup1[0];
				*ptr++ = lookup1[0];
				*ptr++ = lookup1[0];
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

void draw_antic_6(int j, int nchars, UWORD t_screenaddr, char *t_ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	int i;
	UWORD *ptr = (UWORD *) (t_ptr);
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = dGetByte(t_screenaddr++);
		UWORD chaddr;
		UBYTE chdata;
		UWORD colour = 0;
		int kk;
		chaddr = t_chbase + ((UWORD) (screendata & 0x3f) << 3);
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
		chdata = dGetByte(chaddr);
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
void draw_antic_8(int j, int nchars, UWORD t_screenaddr, char *t_ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	int i;
	UWORD *ptr = (UWORD *) (t_ptr);
	LOOKUP1_4COL
		for (i = 0; i < nchars; i++) {
		UBYTE screendata = dGetByte(t_screenaddr++);
		int kk;
		for (kk = 0; kk < 4; kk++) {
			if (!(*t_pm_scanline_ptr)) {
				*ptr++ = lookup1[screendata & 0xC0];
				*ptr++ = lookup1[screendata & 0xC0];
				*ptr++ = lookup1[screendata & 0xC0];
				*ptr++ = lookup1[screendata & 0xC0];
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
void draw_antic_9(int j, int nchars, UWORD t_screenaddr, char *t_ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	int i;
	UWORD *ptr = (UWORD *) (t_ptr);
	lookup1[0x00] = cl_word[8];
	lookup1[0x80] = lookup1[0x40] = lookup1[0x20] = lookup1[0x10] = cl_word[4];
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = dGetByte(t_screenaddr++);
		int kk;
		for (kk = 0; kk < 4; kk++) {
			if (!(*t_pm_scanline_ptr)) {
				*ptr++ = lookup1[screendata & 0x80];
				*ptr++ = lookup1[screendata & 0x80];
				*ptr++ = lookup1[screendata & 0x40];
				*ptr++ = lookup1[screendata & 0x40];
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

void draw_antic_a(int j, int nchars, UWORD t_screenaddr, char *t_ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	int i;
	UWORD *ptr = (UWORD *) (t_ptr);
	LOOKUP1_4COL
		for (i = 0; i < nchars; i++) {
		UBYTE screendata = dGetByte(t_screenaddr++);
		int kk;
		for (kk = 0; kk < 2; kk++) {
			if (!(*t_pm_scanline_ptr)) {
				*ptr++ = lookup1[screendata & 0xC0];
				*ptr++ = lookup1[screendata & 0xC0];
				*ptr++ = lookup1[screendata & 0x30];
				*ptr++ = lookup1[screendata & 0x30];
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

void draw_antic_c(int j, int nchars, UWORD t_screenaddr, char *t_ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	int i;
	UWORD *ptr = (UWORD *) (t_ptr);
	lookup1[0x00] = cl_word[8];
	lookup1[0x80] = lookup1[0x40] = lookup1[0x20] = lookup1[0x10] = cl_word[4];
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = dGetByte(t_screenaddr++);
		int kk;
		for (kk = 0; kk < 2; kk++) {
			if (!(*t_pm_scanline_ptr)) {
				*ptr++ = lookup1[screendata & 0x80];
				*ptr++ = lookup1[screendata & 0x40];
				*ptr++ = lookup1[screendata & 0x20];
				*ptr++ = lookup1[screendata & 0x10];
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

void draw_antic_e(int j, int nchars, UWORD t_screenaddr, char *t_ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	int i;
	UWORD *ptr = (UWORD *) (t_ptr);
#ifdef UNALIGNED_LONG_OK
	int background;
#endif
	LOOKUP1_4COL
#ifdef UNALIGNED_LONG_OK
		background = (lookup1[0x00] << 16) | lookup1[0x00];
#endif

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = dGetByte(t_screenaddr++);
		if (!(*t_pm_scanline_ptr)) {
			if (screendata) {
				*ptr++ = lookup1[screendata & 0xc0];
				*ptr++ = lookup1[screendata & 0x30];
				*ptr++ = lookup1[screendata & 0x0c];
				*ptr++ = lookup1[screendata & 0x03];
			}
			else {
#ifdef UNALIGNED_LONG_OK
				ULONG *l_ptr = (ULONG *) ptr;

				*l_ptr++ = background;
				*l_ptr++ = background;

				ptr = (UWORD *) l_ptr;
#else
				*ptr++ = lookup1[0x00];
				*ptr++ = lookup1[0x00];
				*ptr++ = lookup1[0x00];
				*ptr++ = lookup1[0x00];
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

void draw_antic_f(int j, int nchars, UWORD t_screenaddr, char *ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	ULONG COL_6_LONG;
	int i;
	lookup1[0x00] = colour_lookup[6];
	lookup1[0x80] = lookup1[0x40] = lookup1[0x20] =
		lookup1[0x10] = lookup1[0x08] = lookup1[0x04] =
		lookup1[0x02] = lookup1[0x01] = (colour_lookup[6] & 0xf0) | (colour_lookup[5] & 0x0f);
	COL_6_LONG = cl_word[6] | (cl_word[6] << 16);
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = dGetByte(t_screenaddr++);
		if (!(*t_pm_scanline_ptr)) {
			if (screendata) {
				*ptr++ = (char)lookup1[screendata & 0x80];
				*ptr++ = (char)lookup1[screendata & 0x40];
				*ptr++ = (char)lookup1[screendata & 0x20];
				*ptr++ = (char)lookup1[screendata & 0x10];
				*ptr++ = (char)lookup1[screendata & 0x08];
				*ptr++ = (char)lookup1[screendata & 0x04];
				*ptr++ = (char)lookup1[screendata & 0x02];
				*ptr++ = (char)lookup1[screendata & 0x01];
			}
			else {
#ifdef UNALIGNED_LONG_OK
				ULONG *l_ptr = (ULONG *) ptr;

				*l_ptr++ = COL_6_LONG;
				*l_ptr++ = COL_6_LONG;

				ptr = (UBYTE *) l_ptr;
#else
				UWORD *w_ptr = (UWORD *) ptr;

				*w_ptr++ = PF2_COLPF2;
				*w_ptr++ = PF2_COLPF2;
				*w_ptr++ = PF2_COLPF2;
				*w_ptr++ = PF2_COLPF2;

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
void draw_antic_f_artif(int j, int nchars, UWORD t_screenaddr, char *ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	ULONG COL_6_LONG;
	int i;
	ULONG screendata_tally;
	lookup1[0x00] = colour_lookup[6];
	lookup1[0x80] = lookup1[0x40] = lookup1[0x20] =
		lookup1[0x10] = lookup1[0x08] = lookup1[0x04] =
		lookup1[0x02] = lookup1[0x01] = (colour_lookup[6] & 0xf0) | (colour_lookup[5] & 0x0f);
	COL_6_LONG = cl_word[6] | (cl_word[6] << 16);
	t_screenaddr++;				/* for ART */
	screendata_tally = dGetByte(t_screenaddr - 1);
	setup_art_colours();
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = dGetByte(t_screenaddr++);
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
			screendata = dGetByte(t_screenaddr - 2);
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
void draw_antic_f_gtia9(int j, int nchars, UWORD t_screenaddr, char *t_ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	int i;
	ULONG *ptr = (ULONG *) (t_ptr);
	ULONG temp_count = 0;
	ULONG base_colour = colour_lookup[8];
	base_colour = base_colour | (base_colour << 8);
	base_colour = base_colour | (base_colour << 16);
	for (i = 0; i <= 15; i++) {
		lookup_gtia[i] = base_colour | temp_count;
		temp_count += 0x01010101;
	}

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = dGetByte(t_screenaddr++);
		*ptr++ = lookup_gtia[screendata >> 4];
		*ptr++ = lookup_gtia[screendata & 0x0f];
		if ((*t_pm_scanline_ptr)) {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			int k;
			signed char pm_reg;
			UWORD *w_ptr = (UWORD *) (ptr - 2);
			for (k = 1; k <= 4; k++) {
				pm_reg=new_pm_lookup[*c_pm_scanline_ptr++];
				if (pm_reg<L_PMNONE) {
				/* it is not blank or P5ONLY */
			             *w_ptr = cl_word[cur_prior[pm_reg+8]];
                                }else if(pm_reg==L_PM5PONLY){
                                        *w_ptr=(*w_ptr&0x0f0f) | cl_word[7];
                                        }
                                        /* otherwise it was L_PMNONE so do nothing */
				w_ptr++;
			}
		}
		t_pm_scanline_ptr++;
	}
}
/* note:border for mode 11 should be fixed as bkground colour is COLBK&0xf0*/
void draw_antic_f_gtia11(int j, int nchars, UWORD t_screenaddr, char *t_ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
{
	int i;
	ULONG *ptr = (ULONG *) (t_ptr);
	ULONG temp_count = 0;
	ULONG base_colour = colour_lookup[8];
	base_colour = base_colour | (base_colour << 8);
	base_colour = base_colour | (base_colour << 16);
	lookup_gtia[0]=base_colour&0xf0f0f0f0;
	/*gtia mode 11 background is always lum 0*/

	for (i = 1; i <= 15; i++) {
		lookup_gtia[i] = base_colour | temp_count;
		temp_count += 0x10101010;
	}

	for (i = 0; i < nchars; i++) {
		UBYTE screendata = dGetByte(t_screenaddr++);
		*ptr++ = lookup_gtia[screendata >> 4];
		*ptr++ = lookup_gtia[screendata & 0x0f];
		if ((*t_pm_scanline_ptr)) {
			UBYTE *c_pm_scanline_ptr = (char *) t_pm_scanline_ptr;
			int k;
			signed char pm_reg;
			UWORD *w_ptr = (UWORD *) (ptr - 2);
			for (k = 1; k <= 4; k++) {
				pm_reg=new_pm_lookup[*c_pm_scanline_ptr++];
				if (pm_reg<L_PMNONE) {
                                     *w_ptr = cl_word[cur_prior[pm_reg+8]];
                                }else if(pm_reg==L_PM5PONLY){
                                        if ((*w_ptr)&0xf0f0)  *w_ptr= (((*w_ptr)&0xf0f0) | cl_word[7]);
                                        else  *w_ptr =(cl_word[7]&0xf0f0);
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
void draw_antic_f_gtia10(int j, int nchars, UWORD t_screenaddr, char *t_ptr, ULONG * t_pm_scanline_ptr, UWORD t_chbase)
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
/* should prolly fix the border code instead..right border is still not right. */
	/* needs to be moved over 1 colour clock */
	t_pm_scanline_ptr = (ULONG *) (((UBYTE *) t_pm_scanline_ptr) + 1);
	for (i = 0; i < nchars; i++) {
		UBYTE screendata = dGetByte(t_screenaddr++);
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


void do_antic(void)
{
	UWORD t_screenaddr;
	int j;
	int lastline;
	int md = hscrol_flag + mode_type;
	int temp_chars_read = chars_read[md];
	int thislinecycles = firstlinecycles;
	int first_line_flag = -4;

	lastline = vskipafter < 16 ? vskipafter : normal_lastline;
	j = vskipbefore;
	do {
		int temp_left_border_chars;
		ULONG *t_pm_scanline_ptr;
		char *ptr;
		UWORD t_chbase = 0;
		extern void new_pm_scanline(void);
		int temp_xmin = x_min[md];

		j &= 0x0f;

		POKEY_Scanline();		/* check and generate IRQ */

		/* if (!wsync_halt) GO(107); */
		pmgdmac = pmg_dma();
		begcyc = BEGL - pmgdmac - dldmac;
		dldmac = 0;				/* subsequent lines have none; */
		/* PART 1 */

		cpu_clock+=BEGL-begcyc;
		unc = GO(begcyc + unc);	/* cycles for begin of each screen line */
		allc = realcyc[anticm8 + first_line_flag + dmaw] - BEGL;
		/* ^^^^^cycles for part 2; (realcyc is for parts 1+ 2. then -BEGL for 2 */
		/* ^^^ first line flag is either -4 or 0. when added to dmaw */
		/*  is 0-3 or 4-7.  (first and subsequent lines) */
		if ((IR & 0x80) && j == lastline) {
			NMIST = 0x9f;
			if( NMIEN & 0x80 ) {
				allc -= 8;
				NMI();
			}
		}
		/* cpu_clock+=58-BEGL-allc; */		/* mmm */
		unc = GO(allc + unc);
		/* ^^PART 2.(after the DLI) */
		if( !draw_display )	goto after_scanline;
		new_pm_scanline();
		temp_left_border_chars = left_border_chars;
		switch (IR & 0x0f) {
		case 2:
		case 3:
		case 4:
			t_chbase = chbase_40 + ((j & 0x07) ^ flip_mask);
			break;
		case 5:
			t_chbase = chbase_40 + ((j >> 1) ^ flip_mask);
			break;
		case 6:
			t_chbase = chbase_20 + ((j & 0x07) ^ flip_mask);
			break;
		case 7:
			t_chbase = chbase_20 + ((j >> 1) ^ flip_mask);
			break;
		}

		t_pm_scanline_ptr = (ULONG *) (&pm_scanline[temp_xmin >> 1]);
		ptr = &scrn_ptr[temp_xmin];

		t_screenaddr = screenaddr + ch_offset[md];
		switch (IR & 0x0f) {
		case 0:
			temp_chars_read = 0;
			left_border_chars = (48 - LCHOP - RCHOP) - right_border_chars;
			break;
		case 2:
			if ((PRIOR & 0x40) == 0x40)
				draw_antic_2_gtia9_11(j, chars_displayed[md], t_screenaddr, ptr, t_pm_scanline_ptr, t_chbase);
			else if (global_artif_mode!=0)
				draw_antic_2_artif(j, chars_displayed[md], t_screenaddr, ptr, t_pm_scanline_ptr, t_chbase);
			else
			        draw_antic_2(j, chars_displayed[md], t_screenaddr, ptr, t_pm_scanline_ptr, t_chbase);
			break;
		case 3:
			draw_antic_3(j, chars_displayed[md], t_screenaddr, ptr, t_pm_scanline_ptr, t_chbase);
			break;
		case 4:
		case 5:
			draw_antic_4(j, chars_displayed[md], t_screenaddr, ptr, t_pm_scanline_ptr, t_chbase);
			break;
		case 6:
		case 7:
			draw_antic_6(j, chars_displayed[md], t_screenaddr, ptr, t_pm_scanline_ptr, t_chbase);
			break;
		case 0x08:
			draw_antic_8(j, chars_displayed[md], t_screenaddr, ptr, t_pm_scanline_ptr, t_chbase);
			break;
		case 0x09:
			draw_antic_9(j, chars_displayed[md], t_screenaddr, ptr, t_pm_scanline_ptr, t_chbase);
			break;
		case 0x0a:
			draw_antic_a(j, chars_displayed[md], t_screenaddr, ptr, t_pm_scanline_ptr, t_chbase);
			break;
		case 0x0b:
		case 0x0c:
			draw_antic_c(j, chars_displayed[md], t_screenaddr, ptr, t_pm_scanline_ptr, t_chbase);
			break;
		case 0x0d:
		case 0x0e:
			draw_antic_e(j, chars_displayed[md], t_screenaddr, ptr, t_pm_scanline_ptr, t_chbase);
			break;
		case 0x0f:
			if ((PRIOR & 0xC0) == 0x40)
				draw_antic_f_gtia9(j, chars_displayed[md], t_screenaddr, ptr, t_pm_scanline_ptr, t_chbase);
			else if ((PRIOR & 0xC0) == 0x80)
				draw_antic_f_gtia10(j, chars_displayed[md], t_screenaddr, ptr, t_pm_scanline_ptr, t_chbase);
			else if ((PRIOR & 0xC0) == 0xC0)
				draw_antic_f_gtia11(j, chars_displayed[md], t_screenaddr, ptr, t_pm_scanline_ptr, t_chbase);
			else if (global_artif_mode != 0)
				draw_antic_f_artif(j, chars_displayed[md], t_screenaddr, ptr, t_pm_scanline_ptr, t_chbase);
			else
				draw_antic_f(j, chars_displayed[md], t_screenaddr, ptr, t_pm_scanline_ptr, t_chbase);
			break;
		default:
			break;
		}
		do_border();
		left_border_chars = temp_left_border_chars;
		/* memset(pm_scanline, 0, ATARI_WIDTH / 2); */ /* Perry disabled 98/03/14 */
after_scanline:
/* part 3 */
		allc = CPUL - WSYNC - realcyc[anticm8 + first_line_flag + dmaw] - thislinecycles;
		cpu_clock+=thislinecycles;
		unc = GO(allc + unc);
		thislinecycles = nextlinecycles;
		first_line_flag = 0;
/* ^^^^ adjust for subsequent lines; */
		wsync_halt = 0;
		unc = GO(VCOUNTDELAY + unc);
		ypos++;
		last_cpu_clock=cpu_clock=last_cpu_clock+CPUL;	/* sync */
		unc = GO(WSYNC - VCOUNTDELAY + unc);
/* ^^^^ part 4. */

/*if (wsync_halt)
   {
   if (!(--wsync_halt))
   {
   GO (7);
   }
   }
   else
   {
   GO (7);
   }
 */
		scrn_ptr += ATARI_WIDTH;
	} while ((j++) != lastline);
	screenaddr = increment_Antic_counter(screenaddr, temp_chars_read);
}

/*
   *****************************************************************
   *                                *
   *    Section         :   Display List        *
   *    Original Author     :   David Firth     *
   *    Date Written        :   28th May 1995       *
   *    Version         :   1.0         *
   *                                *
   *   Description                          *
   *   -----------                          *
   *                                *
   *   Section that handles Antic Display List. Not required for    *
   *   BASIC version.                       *
   *                                                               *
   *****************************************************************
 */

int pmg_dma(void)
{
	if (player_dma_enabled) {
		if (player_gra_enabled) {
			if (singleline) {
				GRAFP0 = dGetByte(p0addr_s + ypos);
				GRAFP1 = dGetByte(p1addr_s + ypos);
				GRAFP2 = dGetByte(p2addr_s + ypos);
				GRAFP3 = dGetByte(p3addr_s + ypos);
			}
			else {
				GRAFP0 = dGetByte(p0addr_d + (ypos >> 1));
				GRAFP1 = dGetByte(p1addr_d + (ypos >> 1));
				GRAFP2 = dGetByte(p2addr_d + (ypos >> 1));
				GRAFP3 = dGetByte(p3addr_d + (ypos >> 1));
			}
		}
		if (missile_gra_enabled)
			GRAFM = dGetByte( singleline
					? maddr_s + ypos
					: maddr_d + (ypos >> 1)
			);
		return 5;
	}
	if (missile_dma_enabled) {
		if (missile_gra_enabled)
			GRAFM = dGetByte( singleline
					? maddr_s + ypos
					: maddr_d + (ypos >> 1)
			);
		return 1;
	}
	return 0;
}

UBYTE get_DL_byte(void)
{
	UBYTE result;

	result=dGetByte(dlist);
	dlist++;
	if( (dlist & 0x3FF) == 0 )
		dlist -= 0x400;
	return result;
}

void ANTIC_RunDisplayList(void)
{
/*  UWORD dlist;  made a global */
	int JVB;
	int vscrol_flag;
	int nlines;
	int i;
	UBYTE lsb;

	wsync_halt = 0;
	unc = 0;

	/*
	 * VCOUNT must equal zero for some games but the first line starts
	 * when VCOUNT=4. This portion processes when VCOUNT=0, 1, 2 and 3
	 */

	ypos = 0;
	while (ypos < 8) {
		POKEY_Scanline();		/* check and generate IRQ */
		unc = GO(CPUL - DMAR - WSYNC + unc);
		wsync_halt = 0;
		unc = GO(VCOUNTDELAY + unc);
		ypos++;
		last_cpu_clock=cpu_clock=last_cpu_clock+CPUL;	/* sync */
		unc = GO(WSYNC - VCOUNTDELAY + unc);
		/* GO (114); */
	}

	scrn_ptr = (UBYTE *) atari_screen;

	last_cpu_clock=cpu_clock=last_cpu_clock+CPUL*(8-ypos);	/* sync */
	ypos = 8;
	vscrol_flag = FALSE;

	/*dlist = (DLISTH << 8) | DLISTL; */
	JVB = FALSE;

	while ((DMACTL & 0x20) && !JVB && (ypos < (ATARI_HEIGHT + 8))) {
		UBYTE colpf1;

		antic23f = FALSE;

		IR = get_DL_byte();

		colpf1 = COLPF1;

		/* PMG flickering :-) (Raster) */
		if (player_flickering) {
			GRAFP0 = GRAFP1 = GRAFP2 = GRAFP3 = rand();
		}
		if (missile_flickering) {
			GRAFM = rand();
		}


		firstlinecycles = nextlinecycles = DMAR;
		anticmode = (IR & 0x0f);
		anticm8 = (anticmode << 3);

		switch (anticmode) {
		case 0x00:
			{
				if (vscrol_flag) {
					vskipafter = VSCROL;
				}
				vscrol_flag = FALSE;
				/* apparently blank lines are
				   affected by a VSCROL on to off transition
				   and are shortened from the bottom.
				   they always turn vscrol_flag off however */
				/* should the blank line in case 0x01 (jump)
				   also be so affected? */
				normal_lastline = ((IR >> 4) & 0x07);
				dldmac = 1;
				/* IR=0; leave dli bit alone. */
				do_antic();
				nlines = 0;
			}
			break;
		case 0x01:
			vscrol_flag = FALSE;
			lsb = get_DL_byte();
			dlist = (get_DL_byte() << 8) | lsb;
			if (IR & 0x40) {
				nlines = 0;
				JVB = TRUE;
			}
			else {
				normal_lastline = 0;
				IR &= 0xf0;		/* important:must preserve DLI bit. */
				/* maybe should just add 0x01 as a case in antic code? */

				dldmac = 3;
				do_antic();
				nlines = 0;
				/* Jump aparently uses 1 scan line */
			}
			break;
		default:
			if (IR & 0x40) {
				lsb = get_DL_byte();
				screenaddr = (get_DL_byte() << 8) | lsb;

				dldmac = 3;
			}
			else
				dldmac = 1;

			if (IR & 0x20) {
				if (!vscrol_flag) {
					vskipbefore = VSCROL;
					vscrol_flag = TRUE;
				}
			}
			else if (vscrol_flag) {
				vskipafter = VSCROL;
				vscrol_flag = FALSE;
			}
			hscrol_flag = 0;
			if (IR & 0x10) {
				xmin = dmactl_xmin_scroll;
				xmax = dmactl_xmax_scroll;
				scroll_offset = HSCROL + HSCROL;
				hscrol_flag = 3;
			}
			else {
				xmin = dmactl_xmin_noscroll;
				xmax = dmactl_xmax_noscroll;
				scroll_offset = 0;
			}

			switch (IR & 0x0f) {
			case 0x02:
				nlines = 0;
				mode_type = 0;
				normal_lastline = 7;
				antic23f = TRUE;
				firstlinecycles = (dmaw << 4) + 6 - dmaw;	/* 0,1,2 */
				nextlinecycles = (dmaw << 3) + DMAR;
				do_antic();
				break;
			case 0x03:
				nlines = 0;
				mode_type = 0;
				normal_lastline = 9;
				antic23f = TRUE;
				firstlinecycles = (dmaw << 4) + 6 - dmaw;	/* 0,1,2 */
				nextlinecycles = (dmaw << 3) + DMAR;
				do_antic();
				break;
			case 0x04:
				mode_type = 0;
				normal_lastline = 7;
				nlines = 0;
				firstlinecycles = (dmaw << 4) + 6 - dmaw;	/* 0,1,2 */
				nextlinecycles = (dmaw << 3) + DMAR;
				do_antic();
				break;
			case 0x05:
				mode_type = 0;
				normal_lastline = 15;
				nlines = 0;
				firstlinecycles = (dmaw << 4) + 6 - dmaw;	/* 0,1,2 */
				nextlinecycles = (dmaw << 3) + DMAR;
				do_antic();
				break;
			case 0x06:
				mode_type = 1;
				normal_lastline = 7;
				nlines = 0;
				firstlinecycles = (dmaw << 3) + DMAR;
				nextlinecycles = (dmaw << 2) + DMAR;
				do_antic();
				break;
			case 0x07:
				mode_type = 1;
				normal_lastline = 15;
				nlines = 0;
				firstlinecycles = (dmaw << 3) + DMAR;
				nextlinecycles = (dmaw << 2) + DMAR;
				do_antic();
				break;
			case 0x08:
				mode_type = 2;
				normal_lastline = 7;
				nlines = 0;
				firstlinecycles = (dmaw << 1) + DMAR;
				do_antic();
				break;
			case 0x09:
				mode_type = 2;
				normal_lastline = 3;
				nlines = 0;
				firstlinecycles = (dmaw << 1) + DMAR;
				do_antic();
				break;
			case 0x0a:
				nlines = 0;
				mode_type = 1;
				normal_lastline = 3;
				firstlinecycles = (dmaw << 2) + DMAR;
				do_antic();
				break;
			case 0x0b:
				nlines = 0;
				mode_type = 1;
				normal_lastline = 1;
				firstlinecycles = (dmaw << 2) + DMAR;
				do_antic();
				break;
			case 0x0c:
				nlines = 0;
				mode_type = 1;
				normal_lastline = 0;
				firstlinecycles = (dmaw << 2) + DMAR;
				do_antic();
				break;
			case 0x0d:
				nlines = 0;
				mode_type = 0;
				normal_lastline = 1;
				firstlinecycles = (dmaw << 3) + DMAR;
				do_antic();
				break;
			case 0x0e:
				nlines = 0;
				mode_type = 0;
				normal_lastline = 0;
				firstlinecycles = (dmaw << 3) + DMAR;
				do_antic();
				break;
			case 0x0f:
				nlines = 0;
				mode_type = 0;
				normal_lastline = 0;
				antic23f = TRUE;
				firstlinecycles = (dmaw << 3) + DMAR;
				do_antic();
				break;
			default:
				nlines = 0;
				JVB = TRUE;
				break;
			}
			break;
		}
		vskipbefore = 0;
		vskipafter = 99;
	}
	IR = 0;
	normal_lastline = 0;
	for (i = (ATARI_HEIGHT + 8 - ypos); i > 0; i--) {
		firstlinecycles = nextlinecycles = DMAR;
		anticmode = (IR & 0x0f);
		anticm8 = (anticmode << 3);
		do_antic();
	}
	nlines = 0;
	NMIST = 0x5f;				/* Set VBLANK */
	if (NMIEN & 0x40) {
		GO(1);					/* Needed for programs that monitor NMIST (Spy's Demise) */
		NMI();
	}

	last_cpu_clock=cpu_clock=last_cpu_clock+CPUL*(248-ypos);/* sync */
	ypos = 248;
	while (ypos < (tv_mode == TV_PAL ? 312 : 262)) {
		POKEY_Scanline();		/* check and generate IRQ */
		unc = GO(CPUL - WSYNC + unc - DMAR);
		wsync_halt = 0;
		unc = GO(VCOUNTDELAY + unc);
		ypos++;
		last_cpu_clock=cpu_clock=last_cpu_clock+CPUL;	/* sync */
		unc = GO(WSYNC - VCOUNTDELAY + unc);
	}
}

UBYTE ANTIC_GetByte(UWORD addr)
{
	switch (addr&0xf) {
	case _VCOUNT:
		return ypos >> 1;
	case _PENH:
		return 0x00;
	case _NMIST:
		return NMIST;
	default:
		return 0xff;
	}
}

int ANTIC_PutByte(UWORD addr, UBYTE byte)
{
	int abort = FALSE;

	addr &= 0x0f;
	switch (addr) {
	case _CHBASE:
		CHBASE = byte;
		chbase_40 = (byte << 8) & 0xfc00;
		chbase_20 = (byte << 8) & 0xfe00;
		break;
	case _CHACTL:
		CHACTL = byte;
/*
   =================================================================
   Check for vertical reflect, video invert and character blank bits
   =================================================================
 */
		switch (CHACTL & 0x07) {
		case 0x00:
			char_offset = 0;
			char_delta = 1;
			flip_mask = 0x00;
			invert_mask = 0x00;
			blank_mask = 0x00;
			break;
		case 0x01:
			char_offset = 0;
			char_delta = 1;
			flip_mask = 0x00;
			invert_mask = 0x00;
			blank_mask = 0x80;
			break;
		case 0x02:
			char_offset = 0;
			char_delta = 1;
			flip_mask = 0x00;
			invert_mask = 0x80;
			blank_mask = 0x00;
			break;
		case 0x03:
			char_offset = 0;
			char_delta = 1;
			flip_mask = 0x00;
			invert_mask = 0x80;
			blank_mask = 0x80;
			break;
		case 0x04:
			char_offset = 7;
			char_delta = -1;
			flip_mask = 0x07;
			invert_mask = 0x00;
			blank_mask = 0x00;
			break;
		case 0x05:
			char_offset = 7;
			char_delta = -1;
			flip_mask = 0x07;
			invert_mask = 0x00;
			blank_mask = 0x80;
			break;
		case 0x06:
			char_offset = 7;
			char_delta = -1;
			flip_mask = 0x07;
			invert_mask = 0x80;
			blank_mask = 0x00;
			break;
		case 0x07:
			char_offset = 7;
			char_delta = -1;
			flip_mask = 0x07;
			invert_mask = 0x80;
			blank_mask = 0x80;
			break;
		}
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
			dmactl_xmin_noscroll = dmactl_xmax_noscroll = 0;
			dmactl_xmin_scroll = dmactl_xmax_scroll = 0;
			chars_read[NORMAL0] = 0;
			chars_read[NORMAL1] = 0;
			chars_read[NORMAL2] = 0;
			chars_read[SCROLL0] = 0;
			chars_read[SCROLL1] = 0;
			chars_read[SCROLL2] = 0;
			chars_displayed[NORMAL0] = 0;
			chars_displayed[NORMAL1] = 0;
			chars_displayed[NORMAL2] = 0;
			x_min[NORMAL0] = 0;
			x_min[NORMAL1] = 0;
			x_min[NORMAL2] = 0;
			ch_offset[NORMAL0] = 0;
			ch_offset[NORMAL1] = 0;
			ch_offset[NORMAL2] = 0;
			base_scroll_char_offset = 0;
			base_scroll_char_offset2 = 0;
			base_scroll_char_offset3 = 0;
			left_border_chars = 24 - LCHOP;
			right_border_chars = 24 - RCHOP;
			right_border_start = ATARI_WIDTH >> 1;
			dmaw = 5;
			break;
		case 0x01:
			dmactl_xmin_noscroll = 64;
			dmactl_xmax_noscroll = ATARI_WIDTH - 64;
			dmactl_xmin_scroll = 32;
			dmactl_xmax_scroll = ATARI_WIDTH - 32;
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
			base_scroll_char_offset = 4;
			base_scroll_char_offset2 = 2;
			base_scroll_char_offset3 = 1;
			left_border_chars = 8 - LCHOP;
			right_border_chars = 8 - RCHOP;
			right_border_start = ATARI_WIDTH - 64 /* - 1 */ ;	/* RS! */
			dmaw = 4;
			break;
		case 0x02:
			dmactl_xmin_noscroll = 32;
			dmactl_xmax_noscroll = ATARI_WIDTH - 32;
			dmactl_xmin_scroll = 0;
			dmactl_xmax_scroll = ATARI_WIDTH;
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
			base_scroll_char_offset = 4;
			base_scroll_char_offset2 = 2;
			base_scroll_char_offset3 = 1;
			left_border_chars = 4 - LCHOP;
			right_border_chars = 4 - RCHOP;
			right_border_start = ATARI_WIDTH - 32 /* - 1 */ ;	/* RS! */
			dmaw = 5;
			break;
		case 0x03:
			dmactl_xmin_noscroll = dmactl_xmin_scroll = 0;
			dmactl_xmax_noscroll = dmactl_xmax_scroll = ATARI_WIDTH;
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
			base_scroll_char_offset = 3;
			base_scroll_char_offset2 = 1;
			base_scroll_char_offset3 = 0;
			left_border_chars = 3 - LCHOP;
			right_border_chars = ((1 - LCHOP) < 0) ? (0) : (1 - LCHOP);
			right_border_start = ATARI_WIDTH - 8 /* - 1 */ ;	/* RS! */
			dmaw = 6;
			break;
		}

		missile_dma_enabled = (DMACTL & 0x0C);	/* no player dma without missile */
		player_dma_enabled = (DMACTL & 0x08);
		singleline = (DMACTL & 0x10);
		player_flickering = ((player_dma_enabled | player_gra_enabled) == 0x02);
		missile_flickering = ((missile_dma_enabled | missile_gra_enabled) == 0x01);

		ANTIC_PutByte(_HSCROL, HSCROL);		/* reset values in hscrol */

		break;
	case _HSCROL:

		{

			int char_whole, char_remainder;
			HSCROL = byte & 0x0f;

			if ((DMACTL & 0x03) == 0x00) {	/* no playfield */

				chars_displayed[SCROLL0] = 0;
				chars_displayed[SCROLL1] = 0;
				chars_displayed[SCROLL2] = 0;
				x_min[SCROLL0] = 0;
				x_min[SCROLL1] = 0;
				x_min[SCROLL2] = 0;
				ch_offset[SCROLL0] = 0;
				ch_offset[SCROLL1] = 0;
				ch_offset[SCROLL2] = 0;
			}
			else {
				char_whole = HSCROL >> 2;
				char_remainder = (HSCROL & 0x03) << 1;
				chars_displayed[SCROLL0] = chars_displayed[NORMAL0];
				ch_offset[SCROLL0] = base_scroll_char_offset - char_whole;
				if (char_remainder != 0) {
					char_remainder -= 8;
					chars_displayed[SCROLL0]++;
					ch_offset[SCROLL0]--;
				}
				x_min[SCROLL0] = x_min[NORMAL0] + char_remainder;

				if ((DMACTL & 0x03) == 0x03) {	/* wide playfield */

					if (HSCROL == 4 && HSCROL == 12)
						chars_displayed[SCROLL1] = 22;
					else
						chars_displayed[SCROLL1] = 23;
					char_whole = 0;
					if (HSCROL <= 4) {
						ch_offset[SCROLL1] = 1;
						x_min[SCROLL1] = 16 + (HSCROL << 1);
					}
					else if (HSCROL >= 5 && HSCROL <= 12) {
						ch_offset[SCROLL1] = 0;
						x_min[SCROLL1] = ((HSCROL - 4) << 1) + 8;
					}
					else {
						ch_offset[SCROLL1] = -1;
						x_min[SCROLL1] = ((HSCROL - 12) << 1) + 8;
					}
				}
				else {
					chars_displayed[SCROLL1] = chars_displayed[NORMAL1];
					char_whole = HSCROL >> 3;
					char_remainder = (HSCROL & 0x07) << 1;
					ch_offset[SCROLL1] = base_scroll_char_offset2 - char_whole;
					if (char_remainder != 0) {
						char_remainder -= 16;
						chars_displayed[SCROLL1]++;
						ch_offset[SCROLL1]--;
					}
					x_min[SCROLL1] = x_min[NORMAL0] + char_remainder;
				}
				ch_offset[SCROLL2] = base_scroll_char_offset3;
				if ((DMACTL & 0x03) == 0x03) {	/* wide playfield */

					chars_displayed[SCROLL2] = chars_displayed[NORMAL2];
					x_min[SCROLL2] = x_min[NORMAL2] + (HSCROL << 1);
				}
				else {
					if (HSCROL != 0) {
						x_min[SCROLL2] = x_min[NORMAL2] - 32 + (HSCROL << 1);
						chars_displayed[SCROLL2] = chars_displayed[NORMAL2] + 1;
						ch_offset[SCROLL2]--;
					}
					else {
						chars_displayed[SCROLL2] = chars_displayed[NORMAL2];
						x_min[SCROLL2] = x_min[NORMAL2];
					}
				}
			}
		}
		break;
	case _NMIEN:
		NMIEN = byte;
		break;
	case _NMIRES:
		NMIST = 0x1f;
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
	case _VSCROL:
		VSCROL = byte & 0x0f;
		break;
	case _WSYNC:
		wsync_halt++;
		abort = TRUE;
		break;
	}

	return abort;
}

void AnticStateSave( void )
{
	UBYTE	temp;

	SaveUBYTE( &CHACTL, 1 );
	SaveUBYTE( &CHBASE, 1 );
	SaveUBYTE( &DMACTL, 1 );
	SaveUBYTE( &HSCROL, 1 );
	SaveUBYTE( &NMIEN, 1 );
	SaveUBYTE( &NMIST, 1 );
	SaveUBYTE( &PMBASE, 1 );
	SaveUBYTE( &VSCROL, 1 );
	SaveUBYTE( &singleline, 1 );
	SaveUBYTE( &player_dma_enabled, 1 );
	SaveUBYTE( &player_gra_enabled, 1 );
	SaveUBYTE( &missile_dma_enabled, 1 );
	SaveUBYTE( &missile_gra_enabled, 1 );
	SaveUBYTE( &player_flickering, 1 );
	SaveUBYTE( &missile_flickering, 1 );
	SaveUBYTE( &IR, 1 );

	SaveUBYTE( &pm_scanline[0], ATARI_WIDTH );
	SaveUBYTE( &pf_colls[0], 9 );
	SaveUBYTE( &playfield_lookup[0], 256);
	SaveUBYTE( &prior_table[0], 5 * NUM_PLAYER_TYPES * 16 );
	SaveUBYTE( &cur_prior[0], 5 * NUM_PLAYER_TYPES);

	SaveUWORD( &dlist, 1 );
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

	SaveINT( &dmaw, 1 );
	SaveINT( &unc, 1 );
	SaveINT( &dlisc, 1 );
	SaveINT( &firstlinecycles, 1 );
	SaveINT( &nextlinecycles, 1 );
	SaveINT( &pmgdmac, 1 );
	SaveINT( &dldmac, 1 );
	SaveINT( &scroll_offset, 1 );
	SaveINT( &hscrol_flag, 1 );
	SaveINT( &base_scroll_char_offset, 1 );
	SaveINT( &base_scroll_char_offset2, 1 );
	SaveINT( &base_scroll_char_offset3, 1 );
	SaveINT( &mode_type, 1 );
	SaveINT( &left_border_chars, 1 );
	SaveINT( &right_border_chars, 1 );
	SaveINT( &right_border_start, 1 );
	SaveINT( &normal_lastline, 1 );

	SaveINT( &chars_read[0], 6 );
	SaveINT( &chars_displayed[0], 6 );
	SaveINT( &x_min[0], 6 );
	SaveINT( &ch_offset[0], 6 );

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

	ReadUBYTE( &CHACTL, 1 );
	ReadUBYTE( &CHBASE, 1 );
	ReadUBYTE( &DMACTL, 1 );
	ReadUBYTE( &HSCROL, 1 );
	ReadUBYTE( &NMIEN, 1 );
	ReadUBYTE( &NMIST, 1 );
	ReadUBYTE( &PMBASE, 1 );
	ReadUBYTE( &VSCROL, 1 );
	ReadUBYTE( &singleline, 1 );
	ReadUBYTE( &player_dma_enabled, 1 );
	ReadUBYTE( &player_gra_enabled, 1 );
	ReadUBYTE( &missile_dma_enabled, 1 );
	ReadUBYTE( &missile_gra_enabled, 1 );
	ReadUBYTE( &player_flickering, 1 );
	ReadUBYTE( &missile_flickering, 1 );
	ReadUBYTE( &IR, 1 );

	ReadUBYTE( &pm_scanline[0], ATARI_WIDTH );
	ReadUBYTE( &pf_colls[0], 9 );
	ReadUBYTE( &playfield_lookup[0], 256);
	ReadUBYTE( &prior_table[0], 5 * NUM_PLAYER_TYPES * 16 );
	ReadUBYTE( &cur_prior[0], 5 * NUM_PLAYER_TYPES);

	ReadUWORD( &dlist, 1 );
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

	ReadINT( &dmaw, 1 );
	ReadINT( &unc, 1 );
	ReadINT( &dlisc, 1 );
	ReadINT( &firstlinecycles, 1 );
	ReadINT( &nextlinecycles, 1 );
	ReadINT( &pmgdmac, 1 );
	ReadINT( &dldmac, 1 );
	ReadINT( &scroll_offset, 1 );
	ReadINT( &hscrol_flag, 1 );
	ReadINT( &base_scroll_char_offset, 1 );
	ReadINT( &base_scroll_char_offset2, 1 );
	ReadINT( &base_scroll_char_offset3, 1 );
	ReadINT( &mode_type, 1 );
	ReadINT( &left_border_chars, 1 );
	ReadINT( &right_border_chars, 1 );
	ReadINT( &right_border_start, 1 );
	ReadINT( &normal_lastline, 1 );

	ReadINT( &chars_read[0], 6 );
	ReadINT( &chars_displayed[0], 6 );
	ReadINT( &x_min[0], 6 );
	ReadINT( &ch_offset[0], 6 );

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
