/*
 * Scanline Generation
 * Player Missile Graphics
 * Collision Detection
 * Playfield Priorities
 * Issues cpu cycles during frame redraw.
 */

#include <stdio.h>
#include <string.h>

#ifdef WIN32
#include "windows.h"
#else
#include "config.h"
#endif

#include "atari.h"
#include "cpu.h"
#include "pia.h"
#include "pokey.h"
#include "gtia.h"
#include "antic.h"
#include "platform.h"
#include "sio.h"
#include "statesav.h"

#define FALSE 0
#define TRUE 1

#ifndef NO_CONSOL_SOUND
void Update_consol_sound( int set );
#endif /* NO_CONSOL_SOUND */

int atari_speaker;
int consol_mask;
extern int DELAYED_SERIN_IRQ;
extern int DELAYED_SEROUT_IRQ;
extern int DELAYED_XMTDONE_IRQ;
extern int rom_inserted;
extern int mach_xlxe;

UBYTE COLBK;
UBYTE COLPF0;
UBYTE COLPF1;
UBYTE COLPF2;
UBYTE COLPF3;
UBYTE COLPM0;
UBYTE COLPM1;
UBYTE COLPM2;
UBYTE COLPM3;
UBYTE GRAFM;
UBYTE GRAFP0;
UBYTE GRAFP1;
UBYTE GRAFP2;
UBYTE GRAFP3;
UBYTE HPOSP0;
UBYTE HPOSP1;
UBYTE HPOSP2;
UBYTE HPOSP3;
UBYTE HPOSM0;
UBYTE HPOSM1;
UBYTE HPOSM2;
UBYTE HPOSM3;
UBYTE SIZEP0;
UBYTE SIZEP1;
UBYTE SIZEP2;
UBYTE SIZEP3;
UBYTE SIZEM;
UBYTE M0PF;
UBYTE M1PF;
UBYTE M2PF;
UBYTE M3PF;
UBYTE M0PL;
UBYTE M1PL;
UBYTE M2PL;
UBYTE M3PL;
UBYTE P0PF;
UBYTE P1PF;
UBYTE P2PF;
UBYTE P3PF;
UBYTE P0PL;
UBYTE P1PL;
UBYTE P2PL;
UBYTE P3PL;
UBYTE PRIOR;
UBYTE GRACTL;

extern UBYTE player_dma_enabled;
extern UBYTE missile_dma_enabled;
extern UBYTE player_gra_enabled;
extern UBYTE missile_gra_enabled;
extern UBYTE player_flickering;
extern UBYTE missile_flickering;

/*
   *****************************************************************
   *                                *
   *    Section         :   Player Missile Graphics *
   *    Original Author     :   David Firth     *
   *    Date Written        :   28th May 1995       *
   *    Version         :   1.0         *
   *                                *
   *****************************************************************
 */

static int global_hposp0;
static int global_hposp1;
static int global_hposp2;
static int global_hposp3;
static int global_hposm0;
static int global_hposm1;
static int global_hposm2;
static int global_hposm3;
static int global_sizep0 = 2;
static int global_sizep1 = 2;
static int global_sizep2 = 2;
static int global_sizep3 = 2;
static int global_sizem[] =
{2, 2, 2, 2};


/*
   =========================================
   Width of each bit within a Player/Missile
   =========================================
 */

UBYTE pm_scanline[ATARI_WIDTH];

UBYTE colour_lookup[9];
UBYTE pf_colls[9];
int colour_translation_table[256];
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
extern UWORD cl_word[24];
#define L_PM0 (0*5-4)
#define L_PM1 (1*5-4)
#define L_PM01 (2*5-4)
#define L_PM2 (3*5-4)
#define L_PM3 (4*5-4)
#define L_PM23 (5*5-4)
/*6-11 are 0-5 +6 to mean 5th player as well*/
#define L_PMNONE (12*5-4)
#define L_PM5PONLY (13*5-4) /*these should be the last two*/
#define NUM_PLAYER_TYPES 14
extern UBYTE prior_table[5*NUM_PLAYER_TYPES * 16];
extern UBYTE cur_prior[5*NUM_PLAYER_TYPES];
extern signed char *new_pm_lookup;
extern signed char pm_lookup_normal[256];
extern signed char pm_lookup_multi[256];
extern signed char pm_lookup_5p[256];
extern signed char pm_lookup_multi_5p[256];

int next_console_value = 7;

UWORD pl0adr;
UWORD pl1adr;
UWORD pl2adr;
UWORD pl3adr;
UWORD m0123adr;

static int PM_XPos[256];
static UBYTE PM_Width[4] =
{1, 2, 1, 4};					/*{ 2, 4, 2, 8}; */ /* 1/2 size pm scanline */

void GTIA_Initialise(int *argc, char *argv[])
{
	int i;

	for (i = 0; i < 256; i++)
		PM_XPos[i] = (i - 0x20);	/*<< 1 */ /* for 1/2 size pmscanline */

	for (i = 0; i < 9; i++)
		colour_lookup[i] = 0x00;

	PRIOR = 0x00;
}

void new_pm_scanline(void)
{
	static int dirty;
	if (dirty)
		memset(pm_scanline, 0, ATARI_WIDTH / 2);
	dirty = FALSE;

/*
   =============================
   Display graphics for Player 0
   =============================
 */
	if (GRAFP0) {
		UBYTE grafp0 = GRAFP0;
		int sizep0 = global_sizep0;
		int hposp0 = global_hposp0;
		int i;

		dirty = TRUE;

		for (i = 0; i < 8; i++) {
			if (grafp0 & 0x80) {
				int j;

				for (j = 0; j < sizep0; j++) {
					if ((hposp0 >= 0) && (hposp0 < ATARI_WIDTH / 2)) {
						/* UBYTE playfield = scrn_ptr[hposp0 << 1]; */
						pm_scanline[hposp0] |= 0x01;
						P0PL |= pm_scanline[hposp0];
					}
					hposp0++;
				}
			}
			else {
				hposp0 += sizep0;
			}

			grafp0 = grafp0 << 1;
		}
	}
/*
   =============================
   Display graphics for Player 1
   =============================
 */
	if (GRAFP1) {
		UBYTE grafp1 = GRAFP1;
		int sizep1 = global_sizep1;
		int hposp1 = global_hposp1;
		int i;

		dirty = TRUE;

		for (i = 0; i < 8; i++) {
			if (grafp1 & 0x80) {
				int j;

				for (j = 0; j < sizep1; j++) {
					if ((hposp1 >= 0) && (hposp1 < ATARI_WIDTH / 2)) {
						/* UBYTE playfield = scrn_ptr[hposp1 << 1]; */
						pm_scanline[hposp1] |= 0x02;
						P1PL |= pm_scanline[hposp1];
					}
					hposp1++;
				}
			}
			else {
				hposp1 += sizep1;
			}

			grafp1 = grafp1 << 1;
		}
	}
/*
   =============================
   Display graphics for Player 2
   =============================
 */
	if (GRAFP2) {
		UBYTE grafp2 = GRAFP2;
		int sizep2 = global_sizep2;
		int hposp2 = global_hposp2;
		int i;

		dirty = TRUE;

		for (i = 0; i < 8; i++) {
			if (grafp2 & 0x80) {
				int j;

				for (j = 0; j < sizep2; j++) {
					if ((hposp2 >= 0) && (hposp2 < ATARI_WIDTH / 2)) {
						/* UBYTE playfield = scrn_ptr[hposp2 << 1]; */
						pm_scanline[hposp2] |= 0x04;
						P2PL |= pm_scanline[hposp2];
					}
					hposp2++;
				}
			}
			else {
				hposp2 += sizep2;
			}

			grafp2 = grafp2 << 1;
		}
	}
/*
   =============================
   Display graphics for Player 3
   =============================
 */
	if (GRAFP3) {
		UBYTE grafp3 = GRAFP3;
		int sizep3 = global_sizep3;
		int hposp3 = global_hposp3;
		int i;

		dirty = TRUE;

		for (i = 0; i < 8; i++) {
			if (grafp3 & 0x80) {
				int j;

				for (j = 0; j < sizep3; j++) {
					if ((hposp3 >= 0) && (hposp3 < ATARI_WIDTH / 2)) {
						/* UBYTE playfield = scrn_ptr[hposp3 << 1]; */
						pm_scanline[hposp3] |= 0x08;
						P3PL |= pm_scanline[hposp3];
					}
					hposp3++;
				}
			}
			else {
				hposp3 += sizep3;
			}

			grafp3 = grafp3 << 1;
		}
	}

/*
   =============================
   Display graphics for Missiles
   =============================
 */
	if (GRAFM) {
		UBYTE grafm = GRAFM;
		int hposm0 = global_hposm0;
		int hposm1 = global_hposm1;
		int hposm2 = global_hposm2;
		int hposm3 = global_hposm3;
		int i;

		dirty = TRUE;

		for (i = 0; i < 8; i++) {
			if (grafm & 0x80) {
				int j;

				for (j = 0; j < global_sizem[3 - (i >> 1)]; j++) {
					switch (i & 0x06) {
					case 0x00:
						if ((hposm3 >= 0) && (hposm3 < ATARI_WIDTH / 2)) {
							/* UBYTE playfield = scrn_ptr[hposm3 << 1]; */
							UBYTE player = pm_scanline[hposm3];
							pm_scanline[hposm3] |= 0x80;
							M3PL |= player;
						}
						hposm3++;
						break;
					case 0x02:
						if ((hposm2 >= 0) && (hposm2 < ATARI_WIDTH / 2)) {
							/* UBYTE playfield = scrn_ptr[hposm2 << 1]; */
							UBYTE player = pm_scanline[hposm2];
							pm_scanline[hposm2] |= 0x40;
							M2PL |= player;
						}
						hposm2++;
						break;
					case 0x04:
						if ((hposm1 >= 0) && (hposm1 < ATARI_WIDTH / 2)) {
							/* UBYTE playfield = scrn_ptr[hposm1 << 1]; */
							UBYTE player = pm_scanline[hposm1];
							pm_scanline[hposm1] |= 0x20;
							M1PL |= player;
						}
						hposm1++;
						break;
					case 0x06:
						if ((hposm0 >= 0) && (hposm0 < ATARI_WIDTH / 2)) {
							/* UBYTE playfield = scrn_ptr[hposm0 << 1]; */
							UBYTE player = pm_scanline[hposm0];
							pm_scanline[hposm0] |= 0x10;
							M0PL |= player;
						}
						hposm0++;
						break;
					}
				}
			}
			else {
				switch (i & 0x06) {
				case 0x00:
					hposm3 += global_sizem[3];
					break;
				case 0x02:
					hposm2 += global_sizem[2];
					break;
				case 0x04:
					hposm1 += global_sizem[1];
					break;
				case 0x06:
					hposm0 += global_sizem[0];
					break;
				}
			}

			grafm = grafm << 1;
		}
	}
}


UBYTE GTIA_GetByte(UWORD addr)
{
	UBYTE byte = 0xff;

	addr &= 0x1f;
	switch (addr) {
	case _CONSOL:
		if (next_console_value != 7) {
			byte = (next_console_value|0x08)&consol_mask;
			next_console_value = 0x07;
		}
		else {
                        /* 0x08 is because 'speaker is always 'on' '
                           consol_mask is set by CONSOL (write) !PM! */
			byte = (Atari_CONSOL()|0x08)&consol_mask;
		}
		break;
	case _M0PF:
		byte = (pf_colls[4] & 0x10) >> 4;
		byte |= (pf_colls[5] & 0x10) >> 3;
		byte |= (pf_colls[6] & 0x10) >> 2;
		byte |= (pf_colls[7] & 0x10) >> 1;
		break;
	case _M1PF:
		byte = (pf_colls[4] & 0x20) >> 5;
		byte |= (pf_colls[5] & 0x20) >> 4;
		byte |= (pf_colls[6] & 0x20) >> 3;
		byte |= (pf_colls[7] & 0x20) >> 2;
		break;
	case _M2PF:
		byte = (pf_colls[4] & 0x40) >> 6;
		byte |= (pf_colls[5] & 0x40) >> 5;
		byte |= (pf_colls[6] & 0x40) >> 4;
		byte |= (pf_colls[7] & 0x40) >> 3;
		break;
	case _M3PF:
		byte = (pf_colls[4] & 0x80) >> 7;
		byte |= (pf_colls[5] & 0x80) >> 6;
		byte |= (pf_colls[6] & 0x80) >> 5;
		byte |= (pf_colls[7] & 0x80) >> 4;
		break;
	case _M0PL:
		byte = M0PL & 0x0f;		/* AAA fix for galaxian. easier to do it here. */

		break;
	case _M1PL:
		byte = M1PL & 0x0f;
		break;
	case _M2PL:
		byte = M2PL & 0x0f;
		break;
	case _M3PL:
		byte = M3PL & 0x0f;
		break;
	case _P0PF:
		byte = (pf_colls[4] & 0x01);
		byte |= (pf_colls[5] & 0x01) << 1;
		byte |= (pf_colls[6] & 0x01) << 2;
		byte |= (pf_colls[7] & 0x01) << 3;
		break;
	case _P1PF:
		byte = (pf_colls[4] & 0x02) >> 1;
		byte |= (pf_colls[5] & 0x02);
		byte |= (pf_colls[6] & 0x02) << 1;
		byte |= (pf_colls[7] & 0x02) << 2;
		break;
	case _P2PF:
		byte = (pf_colls[4] & 0x04) >> 2;
		byte |= (pf_colls[5] & 0x04) >> 1;
		byte |= (pf_colls[6] & 0x04);
		byte |= (pf_colls[7] & 0x04) << 1;
		break;
	case _P3PF:
		byte = (pf_colls[4] & 0x08) >> 3;
		byte |= (pf_colls[5] & 0x08) >> 2;
		byte |= (pf_colls[6] & 0x08) >> 1;
		byte |= (pf_colls[7] & 0x08);
		break;
	case _P0PL:
		byte = (P1PL & 0x01) << 1;	/* mask in player 1 */
		byte |= (P2PL & 0x01) << 2;		/* mask in player 2 */
		byte |= (P3PL & 0x01) << 3;		/* mask in player 3 */
		break;
	case _P1PL:
		byte = (P1PL & 0x01);	/* mask in player 0 */
		byte |= (P2PL & 0x02) << 1;		/* mask in player 2 */
		byte |= (P3PL & 0x02) << 2;		/* mask in player 3 */
		break;
	case _P2PL:
		byte = (P2PL & 0x03);	/*mask in player 0 and 1 */
		byte |= (P3PL & 0x04) << 1;		/*mask in player 3 */
		break;
	case _P3PL:
		byte = P3PL & 0x07;		/* mask in player 0,1, and 2 */
		break;
	case _PAL:
		if (tv_mode == TV_PAL)
			byte = 0x00;
		else
			byte = 0x0f;
		break;
	case _TRIG0:
		byte = Atari_TRIG(0);
		break;
	case _TRIG1:
		byte = Atari_TRIG(1);
		break;
	case _TRIG2:
		if (!mach_xlxe)
			byte = Atari_TRIG(2);
		else
			byte = 0;
		break;
	case _TRIG3:
		if (!mach_xlxe)
			byte = Atari_TRIG(3);
		else
			/* extremely important patch - thanks to this hundred of games start running (BruceLee) */
			byte = rom_inserted;
		break;
	case _GRACTL:
		byte = 0x0f;			/* according to XL-it! this helps some games */
		break;
	}

	return byte;
}

int GTIA_PutByte(UWORD addr, UBYTE byte)
{
	UWORD cword;
	addr &= 0x1f;
	switch (addr) {
	case _COLBK:
		byte &= 0xfe;			/* clip lowest bit. 16 lum only in gtia 9! */

		COLBK = byte;
		cword = colour_lookup[8] = colour_translation_table[byte];
		cword = cword | (cword << 8);
		cl_word[8] = cword;
		break;
	case _COLPF0:
		byte &= 0xfe;			/* clip lowest bit. 16 lum only in gtia 9! */

		COLPF0 = byte;
		cword = colour_lookup[4] = colour_translation_table[byte];
		cword = cword | (cword << 8);
		cl_word[4] = cword;
		cl_word[R_COLPM0_OR_PF0] = cl_word[0] | cword;
		cl_word[R_COLPM1_OR_PF0] = cl_word[1] | cword;
		cl_word[R_COLPM0OR1_OR_PF0] = cl_word[R_COLPM0OR1] | cword;
		break;
	case _COLPF1:
		byte &= 0xfe;			/* clip lowest bit. 16 lum only in gtia 9! */

		COLPF1 = byte;
		cword = colour_lookup[5] = colour_translation_table[byte];
		cword = cword | (cword << 8);
		cl_word[5] = cword;
		cl_word[R_COLPM0_OR_PF1] = cl_word[0] | cword;
		cl_word[R_COLPM1_OR_PF1] = cl_word[1] | cword;
		cl_word[R_COLPM0OR1_OR_PF1] = cl_word[R_COLPM0OR1] | cword;
		break;
	case _COLPF2:
		byte &= 0xfe;			/* clip lowest bit. 16 lum only in gtia 9! */

		COLPF2 = byte;
		cword = colour_lookup[6] = colour_translation_table[byte];
		cword = cword | (cword << 8);
		cl_word[6] = cword;
		cl_word[R_COLPM2_OR_PF2] = cl_word[0] | cword;
		cl_word[R_COLPM3_OR_PF2] = cl_word[1] | cword;
		cl_word[R_COLPM2OR3_OR_PF2] = cl_word[R_COLPM2OR3] | cword;
		break;
	case _COLPF3:
		byte &= 0xfe;			/* clip lowest bit. 16 lum only in gtia 9! */

		COLPF3 = byte;
		cword = colour_lookup[7] = colour_translation_table[byte];
		cword = cword | (cword << 8);
		cl_word[7] = cword;
		cl_word[R_COLPM2_OR_PF3] = cl_word[0] | cword;
		cl_word[R_COLPM3_OR_PF3] = cl_word[1] | cword;
		cl_word[R_COLPM2OR3_OR_PF3] = cl_word[R_COLPM2OR3] | cword;
		break;
	case _COLPM0:
		byte &= 0xfe;			/* clip lowest bit. 16 lum only in gtia 9! */

		COLPM0 = byte;
		cword = colour_lookup[0] = colour_translation_table[byte];
		cword = cword | (cword << 8);
		cl_word[0] = cword;
		cl_word[R_COLPM0OR1] = cl_word[1] | cword;
		cl_word[R_COLPM0_OR_PF0] = cl_word[4] | cword;
		cl_word[R_COLPM0_OR_PF1] = cl_word[5] | cword;
		cl_word[R_COLPM0OR1_OR_PF0] = cl_word[R_COLPM0OR1] | cl_word[4];
		cl_word[R_COLPM0OR1_OR_PF1] = cl_word[R_COLPM0OR1] | cl_word[5];
		break;
	case _COLPM1:
		byte &= 0xfe;			/* clip lowest bit. 16 lum only in gtia 9! */

		COLPM1 = byte;
		cword = colour_lookup[1] = colour_translation_table[byte];
		cword = cword | (cword << 8);
		cl_word[1] = cword;
		cl_word[R_COLPM0OR1] = cl_word[0] | cword;
		cl_word[R_COLPM1_OR_PF0] = cl_word[4] | cword;
		cl_word[R_COLPM1_OR_PF1] = cl_word[5] | cword;
		cl_word[R_COLPM0OR1_OR_PF0] = cl_word[R_COLPM0OR1] | cl_word[4];
		cl_word[R_COLPM0OR1_OR_PF1] = cl_word[R_COLPM0OR1] | cl_word[5];
		break;
	case _COLPM2:
		byte &= 0xfe;			/* clip lowest bit. 16 lum only in gtia 9! */

		COLPM2 = byte;
		cword = colour_lookup[2] = colour_translation_table[byte];
		cword = cword | (cword << 8);
		cl_word[2] = cword;
		cl_word[R_COLPM2OR3] = cl_word[3] | cword;
		cl_word[R_COLPM2_OR_PF2] = cl_word[6] | cword;
		cl_word[R_COLPM2_OR_PF3] = cl_word[7] | cword;
		cl_word[R_COLPM2OR3_OR_PF2] = cl_word[R_COLPM2OR3] | cl_word[6];
		cl_word[R_COLPM2OR3_OR_PF3] = cl_word[R_COLPM2OR3] | cl_word[7];
		break;
	case _COLPM3:
		byte &= 0xfe;			/* clip lowest bit. 16 lum only in gtia 9! */

		COLPM3 = byte;
		cword = colour_lookup[3] = colour_translation_table[byte];
		cword = cword | (cword << 8);
		cl_word[3] = cword;
		cl_word[R_COLPM2OR3] = cl_word[2] | cword;
		cl_word[R_COLPM3_OR_PF2] = cl_word[6] | cword;
		cl_word[R_COLPM3_OR_PF3] = cl_word[7] | cword;
		cl_word[R_COLPM2OR3_OR_PF2] = cl_word[R_COLPM2OR3] | cl_word[6];
		cl_word[R_COLPM2OR3_OR_PF3] = cl_word[R_COLPM2OR3] | cl_word[7];
		break;
	case _CONSOL:
		atari_speaker = !(byte & 0x08);
#ifndef NO_CONSOL_SOUND
		Update_consol_sound(1);
#endif /* NO_CONSOL_SOUND */
		consol_mask = (~byte)&0x0f;
		break;
	case _GRAFM:
		GRAFM = byte;
		break;
	case _GRAFP0:
		GRAFP0 = byte;
		break;
	case _GRAFP1:
		GRAFP1 = byte;
		break;
	case _GRAFP2:
		GRAFP2 = byte;
		break;
	case _GRAFP3:
		GRAFP3 = byte;
		break;
	case _HITCLR:
		M0PF = M1PF = M2PF = M3PF = 0;
		P0PF = P1PF = P2PF = P3PF = 0;
		M0PL = M1PL = M2PL = M3PL = 0;
		P0PL = P1PL = P2PL = P3PL = 0;
		pf_colls[4] = pf_colls[5] = pf_colls[6] = pf_colls[7] = 0;
		break;
	case _HPOSM0:
		HPOSM0 = byte;
		global_hposm0 = PM_XPos[byte];
		break;
	case _HPOSM1:
		HPOSM1 = byte;
		global_hposm1 = PM_XPos[byte];
		break;
	case _HPOSM2:
		HPOSM2 = byte;
		global_hposm2 = PM_XPos[byte];
		break;
	case _HPOSM3:
		HPOSM3 = byte;
		global_hposm3 = PM_XPos[byte];
		break;
	case _HPOSP0:
		HPOSP0 = byte;
		global_hposp0 = PM_XPos[byte];
		break;
	case _HPOSP1:
		HPOSP1 = byte;
		global_hposp1 = PM_XPos[byte];
		break;
	case _HPOSP2:
		HPOSP2 = byte;
		global_hposp2 = PM_XPos[byte];
		break;
	case _HPOSP3:
		HPOSP3 = byte;
		global_hposp3 = PM_XPos[byte];
		break;
	case _SIZEM:
		SIZEM = byte;
		global_sizem[0] = PM_Width[byte & 0x03];
		global_sizem[1] = PM_Width[(byte & 0x0C) >> 2];
		global_sizem[2] = PM_Width[(byte & 0x30) >> 4];
		global_sizem[3] = PM_Width[(byte & 0xC0) >> 6];
		break;
	case _SIZEP0:
		SIZEP0 = byte;
		global_sizep0 = PM_Width[byte & 0x03];
		break;
	case _SIZEP1:
		SIZEP1 = byte;
		global_sizep1 = PM_Width[byte & 0x03];
		break;
	case _SIZEP2:
		SIZEP2 = byte;
		global_sizep2 = PM_Width[byte & 0x03];
		break;
	case _SIZEP3:
		SIZEP3 = byte;
		global_sizep3 = PM_Width[byte & 0x03];
		break;
	case _PRIOR:
		if ((byte & 0x30) != (PRIOR & 0x30)) {
		switch(byte&0x30){
		case 0x00:
                new_pm_lookup=pm_lookup_normal;
		break;
		case 0x10:
                new_pm_lookup=pm_lookup_5p;
		break;
		case 0x20:
                new_pm_lookup=pm_lookup_multi;
		break;
		case 0x30:
                new_pm_lookup=pm_lookup_multi_5p;
		break;
		}
		}
		if ((byte & 0x0f) != (PRIOR & 0x0f)) {
			memcpy(&cur_prior, &prior_table[( (byte&0x0f)*(NUM_PLAYER_TYPES*5) )], (NUM_PLAYER_TYPES*5));
                }
		PRIOR = byte;
		break;
	case _GRACTL:
		GRACTL = byte;
		missile_gra_enabled = (byte & 0x01);
		player_gra_enabled = (byte & 0x02);
		player_flickering = ((player_dma_enabled | player_gra_enabled) == 0x02);
		missile_flickering = ((missile_dma_enabled | missile_gra_enabled) == 0x01);
		break;
	}

	return FALSE;
}

void GTIAStateSave( void )
{
	SaveUBYTE( &COLBK, 1 );
	SaveUBYTE( &COLPF0, 1 );
	SaveUBYTE( &COLPF1, 1 );
	SaveUBYTE( &COLPF2, 1 );
	SaveUBYTE( &COLPF3, 1 );
	SaveUBYTE( &COLPM0, 1 );
	SaveUBYTE( &COLPM1, 1 );
	SaveUBYTE( &COLPM2, 1 );
	SaveUBYTE( &COLPM3, 1 );
	SaveUBYTE( &GRAFM, 1 );
	SaveUBYTE( &GRAFP0, 1 );
	SaveUBYTE( &GRAFP1, 1 );
	SaveUBYTE( &GRAFP2, 1 );
	SaveUBYTE( &GRAFP3, 1 );
	SaveUBYTE( &HPOSP0, 1 );
	SaveUBYTE( &HPOSP1, 1 );
	SaveUBYTE( &HPOSP2, 1 );
	SaveUBYTE( &HPOSP3, 1 );
	SaveUBYTE( &HPOSM0, 1 );
	SaveUBYTE( &HPOSM1, 1 );
	SaveUBYTE( &HPOSM2, 1 );
	SaveUBYTE( &HPOSM3, 1 );
	SaveUBYTE( &SIZEP0, 1 );
	SaveUBYTE( &SIZEP1, 1 );
	SaveUBYTE( &SIZEP2, 1 );
	SaveUBYTE( &SIZEP3, 1 );
	SaveUBYTE( &SIZEM, 1 );
	SaveUBYTE( &M0PF, 1 );
	SaveUBYTE( &M1PF, 1 );
	SaveUBYTE( &M2PF, 1 );
	SaveUBYTE( &M3PF, 1 );
	SaveUBYTE( &M0PL, 1 );
	SaveUBYTE( &M1PL, 1 );
	SaveUBYTE( &M2PL, 1 );
	SaveUBYTE( &M3PL, 1 );
	SaveUBYTE( &P0PF, 1 );
	SaveUBYTE( &P1PF, 1 );
	SaveUBYTE( &P2PF, 1 );
	SaveUBYTE( &P3PF, 1 );
	SaveUBYTE( &P0PL, 1 );
	SaveUBYTE( &P1PL, 1 );
	SaveUBYTE( &P2PL, 1 );
	SaveUBYTE( &P3PL, 1 );
	SaveUBYTE( &PRIOR, 1 );
	SaveUBYTE( &GRACTL, 1 );

	SaveINT( &global_hposp0, 1 );
	SaveINT( &global_hposp1, 1 );
	SaveINT( &global_hposp2, 1 );
	SaveINT( &global_hposp3, 1 );
	SaveINT( &global_hposm0, 1 );
	SaveINT( &global_hposm1, 1 );
	SaveINT( &global_hposm2, 1 );
	SaveINT( &global_hposm3, 1 );
	SaveINT( &global_sizep0, 1 );
	SaveINT( &global_sizep1, 1 );
	SaveINT( &global_sizep2, 1 );
	SaveINT( &global_sizep3, 1 );
	SaveINT( &global_sizem[0], 4 );
	SaveINT( &next_console_value, 1 );

	SaveUWORD( &pl0adr, 24 );
	SaveUWORD( &pl1adr, 24 );
	SaveUWORD( &pl2adr, 24 );
	SaveUWORD( &pl3adr, 24 );
	SaveUWORD( &m0123adr, 24 );


	SaveINT( &PM_XPos[0], 256 );
	SaveUBYTE( &PM_Width[0], 4 );
}

void GTIAStateRead( void )
{
	ReadUBYTE( &COLBK, 1 );
	ReadUBYTE( &COLPF0, 1 );
	ReadUBYTE( &COLPF1, 1 );
	ReadUBYTE( &COLPF2, 1 );
	ReadUBYTE( &COLPF3, 1 );
	ReadUBYTE( &COLPM0, 1 );
	ReadUBYTE( &COLPM1, 1 );
	ReadUBYTE( &COLPM2, 1 );
	ReadUBYTE( &COLPM3, 1 );
	ReadUBYTE( &GRAFM, 1 );
	ReadUBYTE( &GRAFP0, 1 );
	ReadUBYTE( &GRAFP1, 1 );
	ReadUBYTE( &GRAFP2, 1 );
	ReadUBYTE( &GRAFP3, 1 );
	ReadUBYTE( &HPOSP0, 1 );
	ReadUBYTE( &HPOSP1, 1 );
	ReadUBYTE( &HPOSP2, 1 );
	ReadUBYTE( &HPOSP3, 1 );
	ReadUBYTE( &HPOSM0, 1 );
	ReadUBYTE( &HPOSM1, 1 );
	ReadUBYTE( &HPOSM2, 1 );
	ReadUBYTE( &HPOSM3, 1 );
	ReadUBYTE( &SIZEP0, 1 );
	ReadUBYTE( &SIZEP1, 1 );
	ReadUBYTE( &SIZEP2, 1 );
	ReadUBYTE( &SIZEP3, 1 );
	ReadUBYTE( &SIZEM, 1 );
	ReadUBYTE( &M0PF, 1 );
	ReadUBYTE( &M1PF, 1 );
	ReadUBYTE( &M2PF, 1 );
	ReadUBYTE( &M3PF, 1 );
	ReadUBYTE( &M0PL, 1 );
	ReadUBYTE( &M1PL, 1 );
	ReadUBYTE( &M2PL, 1 );
	ReadUBYTE( &M3PL, 1 );
	ReadUBYTE( &P0PF, 1 );
	ReadUBYTE( &P1PF, 1 );
	ReadUBYTE( &P2PF, 1 );
	ReadUBYTE( &P3PF, 1 );
	ReadUBYTE( &P0PL, 1 );
	ReadUBYTE( &P1PL, 1 );
	ReadUBYTE( &P2PL, 1 );
	ReadUBYTE( &P3PL, 1 );
	ReadUBYTE( &PRIOR, 1 );
	ReadUBYTE( &GRACTL, 1 );

	ReadINT( &global_hposp0, 1 );
	ReadINT( &global_hposp1, 1 );
	ReadINT( &global_hposp2, 1 );
	ReadINT( &global_hposp3, 1 );
	ReadINT( &global_hposm0, 1 );
	ReadINT( &global_hposm1, 1 );
	ReadINT( &global_hposm2, 1 );
	ReadINT( &global_hposm3, 1 );
	ReadINT( &global_sizep0, 1 );
	ReadINT( &global_sizep1, 1 );
	ReadINT( &global_sizep2, 1 );
	ReadINT( &global_sizep3, 1 );
	ReadINT( &global_sizem[0], 4 );
	ReadINT( &next_console_value, 1 );

	ReadUWORD( &pl0adr, 24 );
	ReadUWORD( &pl1adr, 24 );
	ReadUWORD( &pl2adr, 24 );
	ReadUWORD( &pl3adr, 24 );
	ReadUWORD( &m0123adr, 24 );


	ReadINT( &PM_XPos[0], 256 );
	ReadUBYTE( &PM_Width[0], 4 );
}
