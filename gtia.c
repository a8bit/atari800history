
/*
 * Scanline Generation
 * Player Missile Graphics
 * Collision Detection
 * Playfield Priorities
 * Issues cpu cycles during frame redraw.
 */

#include <stdio.h>
#include <string.h>

#ifndef AMIGA
#include "config.h"
#endif

#include "atari.h"
#include "cpu.h"
#include "pia.h"
#include "pokey.h"
#include "gtia.h"
#include "antic.h"
#include "platform.h"

#define FALSE 0
#define TRUE 1

static char *rcsid = "$Id: gtia.c,v 1.20 1998/02/21 14:57:10 david Exp $";

extern int DELAYED_SERIN_IRQ;
extern int DELAYED_SEROUT_IRQ;
extern int DELAYED_XMTDONE_IRQ;
extern int rom_inserted;

#ifdef CPUASS
extern void color_scanline(UBYTE *, UWORD *);
#endif

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
static int global_sizem0 = 2;
static int global_sizem1 = 2;
static int global_sizem2 = 2;
static int global_sizem3 = 2;

#ifdef DIRECT_VIDEO
static UBYTE colour_to_pf[256];
#endif

/*
   =========================================
   Width of each bit within a Player/Missile
   =========================================
 */

static UBYTE pm_scanline[ATARI_WIDTH];

UBYTE colour_lookup[9];
UWORD colour_lookup16[9<<8];
int colour_translation_table[256];

int next_console_value = 7;

UWORD pl0adr;
UWORD pl1adr;
UWORD pl2adr;
UWORD pl3adr;
UWORD m0123adr;

static int PM_XPos[256];
static UBYTE PM_Width[4] =
{2, 4, 2, 8};

void GTIA_Initialise(int *argc, char *argv[])
{
	int i;

	for (i = 0; i < 256; i++)
		PM_XPos[i] = (i - 0x20) << 1;

#ifdef DIRECT_VIDEO
	printf("GTIA: Screen Generation compiled with DIRECT_VIDEO\n");

	for (i = 0; i < 256; i++)
		colour_to_pf[i] = 0x00;
#endif

	for (i = 0; i < 9; i++)
		colour_lookup[i] = 0x00;

	for (i = 0; i < (9<<8); i++)
		colour_lookup16[i] = 0x0000;

	PRIOR = 0x00;
}

UBYTE WhichColourReg(int pf_pixel, int player, int missile)
{
	UBYTE colreg;

	switch (PRIOR & 0x1f) {
	case 0x01:					/* RIVER RAID */
		if (missile < player)
			colreg = missile;
		else
			colreg = player;
		break;
	case 0x02:					/* MOUNTAIN KING */
		if (player < 2) {
			colreg = player;
		}
		else if (pf_pixel < 7) {
			colreg = pf_pixel;
		}
		else if (pf_pixel == 7) {
			if (missile != 0xff)
				colreg = missile;
			else
				colreg = pf_pixel;
		}
		else if (player < 4) {
			colreg = player;
		}
		else {
			colreg = pf_pixel;
		}
		break;
	case 0x04:
		if (pf_pixel > 7) {
			if (missile < player)
				colreg = missile;
			else
				colreg = player;
		}
		else {
			colreg = pf_pixel;
		}
		break;
	case 0x08:					/* RALLY SPEEDWAY */
		if (pf_pixel > 5) {
			if (missile < player)
				colreg = missile;
			else
				colreg = player;
		}
		else {
			colreg = pf_pixel;
		}
		break;
	case 0x11:					/* PAC-MAN, STAR RAIDERS, ASTEROIDS, FROGGER DELUXE, KANGAROO */
		if (player != 0xff)
			colreg = player;
		else if (pf_pixel > 7)
			colreg = 7;			/* Missile using COLPF3 */
		else
			colreg = pf_pixel;
		break;
	case 0x12:					/* ZONE RANGER */
		if (player < 2) {
			colreg = player;
		}
		else if (pf_pixel < 7) {
			colreg = pf_pixel;
		}
		else if (pf_pixel == 7) {
			if (missile != 0xff)
				colreg = missile;
			else
				colreg = pf_pixel;
		}
		else if (player < 4) {
			colreg = player;
		}
		else {
			colreg = pf_pixel;
		}
		break;
	case 0x14:					/* ATARI CHESS, FORT APOCALYPSE */
		if (missile != 0xff) {
			if (pf_pixel > 7)
				colreg = 7;		/* Missile using COLPF3 */
			else
				colreg = pf_pixel;
		}
		else {
			if (pf_pixel > 7)
				colreg = player;
			else
				colreg = pf_pixel;
		}
		break;
	case 0x18:					/* THE LAST STARTFIGHTER */
		if (pf_pixel > 5) {
			if (missile < player)
				colreg = 7;		/* Missile using COLPF3 */
			else
				colreg = player;
		}
		else {
			colreg = pf_pixel;
		}
		break;
	default:
		printf("Unsupported PRIOR = %02x\n", PRIOR);
	case 0x00:
		if (missile < player)
			colreg = missile;
		else
			colreg = player;
		break;
	}

	return colreg;
}

void PM_ScanLine(void)
{
	static UBYTE pf_collision_bit[9] =
	{
		0x00, 0x00, 0x00, 0x00,
		0x01, 0x02, 0x04, 0x08,
		0x00
	};

	int dirty = FALSE;

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
					if ((hposp0 >= 0) && (hposp0 < ATARI_WIDTH)) {
						UBYTE playfield = scrn_ptr[hposp0];

#ifdef DIRECT_VIDEO
						playfield = colour_to_pf[playfield];
#endif

						pm_scanline[hposp0] |= 0x01;

						P0PF |= pf_collision_bit[playfield];
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
					if ((hposp1 >= 0) && (hposp1 < ATARI_WIDTH)) {
						UBYTE playfield = scrn_ptr[hposp1];

#ifdef DIRECT_VIDEO
						playfield = colour_to_pf[playfield];
#endif

						pm_scanline[hposp1] |= 0x02;

						P1PF |= pf_collision_bit[playfield];
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
					if ((hposp2 >= 0) && (hposp2 < ATARI_WIDTH)) {
						UBYTE playfield = scrn_ptr[hposp2];

#ifdef DIRECT_VIDEO
						playfield = colour_to_pf[playfield];
#endif

						pm_scanline[hposp2] |= 0x04;

						P2PF |= pf_collision_bit[playfield];
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
					if ((hposp3 >= 0) && (hposp3 < ATARI_WIDTH)) {
						UBYTE playfield = scrn_ptr[hposp3];

#ifdef DIRECT_VIDEO
						playfield = colour_to_pf[playfield];
#endif

						pm_scanline[hposp3] |= 0x08;

						P3PF |= pf_collision_bit[playfield];
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
		int hposm0 = global_hposm0;
		int hposm1 = global_hposm1;
		int hposm2 = global_hposm2;
		int hposm3 = global_hposm3;
		int i, mask;

		dirty = TRUE;

		for (i = 0, mask = 0x80; i < 8; i++) {
			if (GRAFM & mask) {
				int j;

/*        for (j=0;j<sizem;j++) */
				{
					switch (i & 0x06) {
					case 0x00:
						for (j = 0; j < global_sizem3; j++) {
							if ((hposm3 >= 0) && (hposm3 < ATARI_WIDTH)) {
								UBYTE playfield = scrn_ptr[hposm3];
								UBYTE player = pm_scanline[hposm3];

#ifdef DIRECT_VIDEO
								playfield = colour_to_pf[playfield];
#endif

								pm_scanline[hposm3] |= 0x80;

								M3PF |= pf_collision_bit[playfield];
								M3PL |= player;
							}
							hposm3++;
						}
						break;
					case 0x02:
						for (j = 0; j < global_sizem2; j++) {
							if ((hposm2 >= 0) && (hposm2 < ATARI_WIDTH)) {
								UBYTE playfield = scrn_ptr[hposm2];
								UBYTE player = pm_scanline[hposm2];

#ifdef DIRECT_VIDEO
								playfield = colour_to_pf[playfield];
#endif

								pm_scanline[hposm2] |= 0x40;

								M2PF |= pf_collision_bit[playfield];
								M2PL |= player;
							}
							hposm2++;
						}
						break;
					case 0x04:
						for (j = 0; j < global_sizem1; j++) {
							if ((hposm1 >= 0) && (hposm1 < ATARI_WIDTH)) {
								UBYTE playfield = scrn_ptr[hposm1];
								UBYTE player = pm_scanline[hposm1];

#ifdef DIRECT_VIDEO
								playfield = colour_to_pf[playfield];
#endif

								pm_scanline[hposm1] |= 0x20;

								M1PF |= pf_collision_bit[playfield];
								M1PL |= player;
							}
							hposm1++;
						}
						break;
					case 0x06:
						for (j = 0; j < global_sizem0; j++) {
							if ((hposm0 >= 0) && (hposm0 < ATARI_WIDTH)) {
								UBYTE playfield = scrn_ptr[hposm0];
								UBYTE player = pm_scanline[hposm0];

#ifdef DIRECT_VIDEO
								playfield = colour_to_pf[playfield];
#endif

								pm_scanline[hposm0] |= 0x10;

								M0PF |= pf_collision_bit[playfield];
								M0PL |= player;
							}
							hposm0++;
						}
						break;
					}
				}
			}
			else {
				switch (i & 0x06) {
				case 0x00:
					hposm3 += global_sizem3;
					break;
				case 0x02:
					hposm2 += global_sizem2;
					break;
				case 0x04:
					hposm1 += global_sizem1;
					break;
				case 0x06:
					hposm0 += global_sizem0;
					break;
				}
			}

			mask = mask >> 1;
		}
	}
/*
   ======================================
   Plot Player/Missile Data onto Scanline
   ======================================
 */
	if (dirty) {
		static int which_pm_lookup[16] =
		{
			0xff,				/* 0000 - None */
			0x00,				/* 0001 - Player 0 */
			0x01,				/* 0010 - Player 1 */
			0x00,				/* 0011 - Player 0 */
			0x02,				/* 0100 - Player 2 */
			0x00,				/* 0101 - Player 0 */
			0x01,				/* 0110 - Player 1 */
			0x00,				/* 0111 - Player 0 */
			0x03,				/* 1000 - Player 3 */
			0x00,				/* 1001 - Player 0 */
			0x01,				/* 1010 - Player 1 */
			0x00,				/* 1011 - Player 0 */
			0x02,				/* 1100 - Player 2 */
			0x00,				/* 1101 - Player 0 */
			0x01,				/* 1110 - Player 1 */
			0x00				/* 1111 - Player 0 */
		};

		int xpos;

		for (xpos = 0; xpos < ATARI_WIDTH; xpos++) {
			UBYTE pm_pixel;

			pm_pixel = pm_scanline[xpos];
			if (pm_pixel) {
				UBYTE pf_pixel;
				UBYTE colreg;
				UBYTE player;
				int which_player;
				int which_missile;

				pf_pixel = scrn_ptr[xpos];
#ifdef DIRECT_VIDEO
				pf_pixel = colour_to_pf[pf_pixel];
#endif
				player = pm_pixel & 0x0f;

				if (player & 0x01)
					P0PL |= player;
				if (player & 0x02)
					P1PL |= player;
				if (player & 0x04)
					P2PL |= player;
				if (player & 0x08)
					P3PL |= player;

				which_player = which_pm_lookup[pm_pixel & 0x0f];
				which_missile = which_pm_lookup[(pm_pixel >> 4) & 0x0f];

				colreg = WhichColourReg(pf_pixel, which_player, which_missile);

#ifdef DIRECT_VIDEO
				scrn_ptr[xpos] = colour_lookup[colreg];
#else
				scrn_ptr[xpos] = colreg;
#endif
			}
		}

		memset(pm_scanline, 0, ATARI_WIDTH);
	}
}

void Atari_ScanLine(void)
{
	int i;

	if (DELAYED_SERIN_IRQ > 0) {
		if (--DELAYED_SERIN_IRQ == 0) {
			if (IRQEN & 0x20) {
#ifdef DEBUG2
				printf("SERIO: SERIN Interrupt triggered\n");
#endif
				IRQST &= 0xdf;
				IRQ = 1;
			}
#ifdef DEBUG2
			else {
				printf("SERIO: SERIN Interrupt missed\n");
			}
#endif
		}
	}

	if (DELAYED_SEROUT_IRQ > 0) {
		if (--DELAYED_SEROUT_IRQ == 0) {
			if (IRQEN & 0x10) {
#ifdef DEBUG2
				printf("SERIO: SEROUT Interrupt triggered\n");
#endif
				IRQST &= 0xef;
				IRQ = 1;
			}
#ifdef DEBUG2
			else {
				printf("SERIO: SEROUT Interrupt missed\n");
			}
#endif
		}
	}

	if (DELAYED_XMTDONE_IRQ > 0) {
		if (--DELAYED_XMTDONE_IRQ == 0) {
			if (IRQEN & 0x08) {
#ifdef DEBUG2
				printf("SERIO: XMTDONE Interrupt triggered\n");
#endif
				IRQST &= 0xf7;
				IRQ = 1;
			}
#ifdef DEBUG2
			else {
				printf("SERIO: XMTDONE Interrupt missed\n");
			}
#endif
		}
	}

#ifdef DIRECT_VIDEO
	for (i = 0; i < dmactl_xmin_noscroll; i++)
		scrn_ptr[i] = colour_lookup[8];

	for (i = dmactl_xmax_noscroll; i < ATARI_WIDTH; i++)
		scrn_ptr[i] = colour_lookup[8];
#else
	for (i = 0; i < dmactl_xmin_noscroll; i++)
		scrn_ptr[i] = PF_COLBK;

	for (i = dmactl_xmax_noscroll; i < ATARI_WIDTH; i++)
		scrn_ptr[i] = PF_COLBK;
#endif

	PM_ScanLine();

	if (PRIOR & 0xc0) {
		UBYTE *t_scrn_ptr1;		/* Input Pointer */
		UBYTE *t_scrn_ptr2;		/* Output Pointer */
		int xpos;
		int nibble = 0;			/* initialise value, just for sure */

		t_scrn_ptr1 = scrn_ptr;
		t_scrn_ptr2 = scrn_ptr;

		for (xpos = 0; xpos < ATARI_WIDTH; xpos++) {
			UBYTE pf_pixel;
			UBYTE colour = 0;	/* initialise value, just for sure */

			pf_pixel = *t_scrn_ptr1++;
#ifdef DIRECT_VIDEO
			pf_pixel = colour_to_pf[pf_pixel];
#endif

			if ((xpos & 0x03) == 0x00)
				nibble = 0;

			nibble <<= 1;

			if (pf_pixel == PF_COLPF1)
				nibble += 1;

			if ((xpos & 0x03) == 0x03) {
				switch (PRIOR & 0xc0) {
				case 0x40:
					colour = colour_translation_table[(COLBK & 0xf0) | nibble];
					break;
				case 0x80:
					if (nibble >= 8)
						nibble -= 9;
					colour = colour_lookup[nibble];
					break;
				case 0xc0:
					colour = colour_translation_table[(nibble << 4) | (COLBK & 0x0f)];
					break;
				}

				*t_scrn_ptr2++ = colour;
				*t_scrn_ptr2++ = colour;
				*t_scrn_ptr2++ = colour;
				*t_scrn_ptr2++ = colour;
			}
		}

		scrn_ptr = t_scrn_ptr2;
	}
#ifndef DIRECT_VIDEO
	else {
#ifdef CPUASS
		color_scanline(scrn_ptr, colour_lookup16);
#else
/* original, slow code 
		UBYTE *t_scrn_ptr;
		int xpos;

		t_scrn_ptr = scrn_ptr;

		for (xpos = 0; xpos < ATARI_WIDTH; xpos += 8) {
			UBYTE colreg;
			UBYTE colour;

			colreg = *t_scrn_ptr;
			colour = colour_lookup[colreg];
			*t_scrn_ptr++ = colour;

			colreg = *t_scrn_ptr;
			colour = colour_lookup[colreg];
			*t_scrn_ptr++ = colour;

			colreg = *t_scrn_ptr;
			colour = colour_lookup[colreg];
			*t_scrn_ptr++ = colour;

			colreg = *t_scrn_ptr;
			colour = colour_lookup[colreg];
			*t_scrn_ptr++ = colour;

			colreg = *t_scrn_ptr;
			colour = colour_lookup[colreg];
			*t_scrn_ptr++ = colour;

			colreg = *t_scrn_ptr;
			colour = colour_lookup[colreg];
			*t_scrn_ptr++ = colour;

			colreg = *t_scrn_ptr;
			colour = colour_lookup[colreg];
			*t_scrn_ptr++ = colour;

			colreg = *t_scrn_ptr;
			colour = colour_lookup[colreg];
			*t_scrn_ptr++ = colour;
		}
*/
		UWORD *t_scrn_ptr;
		int xpos;

		t_scrn_ptr = (UWORD *)scrn_ptr;

		for (xpos = 0; xpos < ATARI_WIDTH; xpos += 8) {
			UWORD colreg16;
			UWORD colour16;

			colreg16 = *t_scrn_ptr;
			colour16 = colour_lookup16[colreg16];
			*t_scrn_ptr++ = colour16;

			colreg16 = *t_scrn_ptr;
			colour16 = colour_lookup16[colreg16];
			*t_scrn_ptr++ = colour16;

			colreg16 = *t_scrn_ptr;
			colour16 = colour_lookup16[colreg16];
			*t_scrn_ptr++ = colour16;

			colreg16 = *t_scrn_ptr;
			colour16 = colour_lookup16[colreg16];
			*t_scrn_ptr++ = colour16;
		}
#endif	/* CPUASS */
#endif	/* !DIRECT_VIDEO */
		scrn_ptr += ATARI_WIDTH;
	}
}

UBYTE GTIA_GetByte(UWORD addr)
{
	UBYTE byte;

	addr &= 0xff1f;
	switch (addr) {
	case _CONSOL:
		if (next_console_value != 7) {
			byte = next_console_value;
			next_console_value = 0x07;
		}
		else {
			byte = Atari_CONSOL();
		}
		break;
	case _M0PF:
		byte = M0PF;
		break;
	case _M1PF:
		byte = M1PF;
		break;
	case _M2PF:
		byte = M2PF;
		break;
	case _M3PF:
		byte = M3PF;
		break;
	case _M0PL:
		byte = M0PL;
		break;
	case _M1PL:
		byte = M1PL;
		break;
	case _M2PL:
		byte = M2PL;
		break;
	case _M3PL:
		byte = M3PL;
		break;
	case _P0PF:
		byte = P0PF;
		break;
	case _P1PF:
		byte = P1PF;
		break;
	case _P2PF:
		byte = P2PF;
		break;
	case _P3PF:
		byte = P3PF;
		break;
	case _P0PL:
		byte = P0PL & 0xfe;
		break;
	case _P1PL:
		byte = P1PL & 0xfd;
		break;
	case _P2PL:
		byte = P2PL & 0xfb;
		break;
	case _P3PL:
		byte = P3PL & 0xf7;
		break;
	case _PAL:
		if (tv_mode == PAL)
			byte = 0x00;
		else
			byte = 0x0e;
		break;
	case _TRIG0:
		byte = Atari_TRIG(0);
		break;
	case _TRIG1:
		byte = Atari_TRIG(1);
		break;
	case _TRIG2:
		if (machine == Atari)
			byte = Atari_TRIG(2);
		else
			byte = 0;
		break;
	case _TRIG3:
		if (machine == Atari)
			byte = Atari_TRIG(3);
		else
			/* extremely important patch - thanks to this hundred of games start running (BruceLee) */
			byte = rom_inserted;
		break;
	case _GRACTL:
		byte = 0x0f;			/* according to XL-it! this helps some games */
		break;
	default:
		byte = 0;				/* just for sure */
	}

	return byte;
}

void setcolreg(int pf, UBYTE colour)
{
#ifdef DIRECT_VIDEO
	colour_to_pf[colour_lookup[pf]] = 0x00;

	colour_lookup[pf] = colour_translation_table[colour];
	while (colour_to_pf[colour_lookup[pf]] != 0) {
		colour++;
		colour_lookup[pf] = colour_translation_table[colour];
	}

	colour_to_pf[colour_lookup[pf]] = pf;
#else
	int i;

	colour_lookup[pf] = colour_translation_table[colour];

	for (i = 0; i < 9; i++) {
		colour_lookup16[(pf << 8) + i] &= 0x00ff;
		colour_lookup16[(pf << 8) + i] |= colour_translation_table[colour] << 8;
		colour_lookup16[pf + (i << 8)] &= 0xff00;
		colour_lookup16[pf + (i << 8)] |= colour_translation_table[colour];
	}
#endif
}

int GTIA_PutByte(UWORD addr, UBYTE byte)
{
	addr &= 0xff1f;
	switch (addr) {
	case _COLBK:
		COLBK = byte;
		setcolreg(8, byte);
		break;
	case _COLPF0:
		COLPF0 = byte;
		setcolreg(4, byte);
		break;
	case _COLPF1:
		COLPF1 = byte;
		setcolreg(5, byte);
		break;
	case _COLPF2:
		COLPF2 = byte;
		setcolreg(6, byte);
		break;
	case _COLPF3:
		COLPF3 = byte;
		setcolreg(7, byte);
		break;
	case _COLPM0:
		COLPM0 = byte;
#ifndef DIRECT_VIDEO
		setcolreg(0, byte);
#endif
		break;
	case _COLPM1:
		COLPM1 = byte;
#ifndef DIRECT_VIDEO
		setcolreg(1, byte);
#endif
		break;
	case _COLPM2:
		COLPM2 = byte;
#ifndef DIRECT_VIDEO
		setcolreg(2, byte);
#endif
		break;
	case _COLPM3:
		COLPM3 = byte;
#ifndef DIRECT_VIDEO
		setcolreg(3, byte);
#endif
		break;
	case _CONSOL:
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
		global_sizem0 = PM_Width[byte & 0x03];
		if (PRIOR & 0x10) {
			global_sizem1 = global_sizem2 = global_sizem3 = global_sizem0;
		}
		else {
			global_sizem1 = PM_Width[(byte >> 2) & 0x03];
			global_sizem2 = PM_Width[(byte >> 4) & 0x03];
			global_sizem3 = PM_Width[(byte >> 6) & 0x03];
		}
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
		PRIOR = byte;
		break;
	case _GRACTL:
		GRACTL = byte;
		break;
	}

	return FALSE;
}
