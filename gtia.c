/* GTIA emulation --------------------------------- */
/* Original Author:                                 */
/*              David Firth                         */
/* Clean ups and optimizations:                     */
/*              Piotr Fusik <pfusik@elka.pw.edu.pl> */
/* Last changes: 26th April 2000                    */
/* ------------------------------------------------ */

#include <string.h>

#include "antic.h"
#include "config.h"
#include "gtia.h"
#include "platform.h"
#include "statesav.h"

#ifndef NO_CONSOL_SOUND
void Update_consol_sound(int set);
#endif

/* GTIA Registers ---------------------------------------------------------- */

UBYTE M0PL;
UBYTE M1PL;
UBYTE M2PL;
UBYTE M3PL;
UBYTE P0PL;
UBYTE P1PL;
UBYTE P2PL;
UBYTE P3PL;
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
UBYTE GRAFP0;
UBYTE GRAFP1;
UBYTE GRAFP2;
UBYTE GRAFP3;
UBYTE GRAFM;
UBYTE COLPM0;
UBYTE COLPM1;
UBYTE COLPM2;
UBYTE COLPM3;
UBYTE COLPF0;
UBYTE COLPF1;
UBYTE COLPF2;
UBYTE COLPF3;
UBYTE COLBK;
UBYTE PRIOR;
UBYTE VDELAY;
UBYTE GRACTL;

/* Internal GTIA state ----------------------------------------------------- */

int atari_speaker;
int next_console_value = 7;			/* for 'hold OPTION during reboot' */
UBYTE consol_mask;
extern int rom_inserted;
extern int mach_xlxe;
void set_prior(UBYTE byte);			/* in antic.c */

/* Player/Missile stuff ---------------------------------------------------- */

extern UBYTE player_dma_enabled;
extern UBYTE missile_dma_enabled;
extern UBYTE player_gra_enabled;
extern UBYTE missile_gra_enabled;
extern UBYTE player_flickering;
extern UBYTE missile_flickering;

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
static int global_sizem[4] =
{2, 2, 2, 2};

static UBYTE PM_Width[4] =
{1, 2, 1, 4};

/* Meaning of bits in pm_scanline and pf_colls:
bit 0 - Player 0
bit 1 - Player 1
bit 2 - Player 2
bit 3 - Player 3
bit 4 - Missile 0
bit 5 - Missile 1
bit 6 - Missile 2
bit 7 - Missile 3
*/

UBYTE pm_scanline[ATARI_WIDTH / 2];	/* there's a byte for every *pair* of pixels */
UBYTE pm_dirty = TRUE;

UBYTE pf_colls[9];					/* collisions with playfield */

/* Colours ----------------------------------------------------------------- */

int colour_translation_table[256];

/* Indexes in cl_word - 0..8 are COLPM0..COLBK */
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

extern UWORD hires_lookup_l[128];
extern ULONG lookup_gtia9[16];
extern ULONG lookup_gtia11[16];
extern UWORD cl_word[24];

void setup_gtia9_11(void) {
	int i;
	ULONG count9 = 0;
	ULONG count11 = 0;
	lookup_gtia11[0] = lookup_gtia9[0] & 0xf0f0f0f0;
	for (i = 1; i < 16; i++) {
		lookup_gtia9[i] = lookup_gtia9[0] | (count9 += 0x01010101);
		lookup_gtia11[i] = lookup_gtia9[0] | (count11 += 0x10101010);
	}
}

/* Initialization ---------------------------------------------------------- */

void GTIA_Initialise(int *argc, char *argv[])
{
	PRIOR = 0x00;
	memset(cl_word, 0, sizeof(cl_word));
	lookup_gtia9[0] = 0;
	setup_gtia9_11();
}

/* Prepare PMG scanline ---------------------------------------------------- */

void new_pm_scanline(void)
{
/* Clear if necessary */
	if (pm_dirty) {
		memset(pm_scanline, 0, ATARI_WIDTH / 2);
		pm_dirty = FALSE;
	}

/* Draw Players */

#define DO_PLAYER(n)	if (GRAFP##n) {				\
	UBYTE grafp = GRAFP##n;							\
	int sizep = global_sizep##n;					\
	unsigned hposp = global_hposp##n;				\
	pm_dirty = TRUE;								\
	do {											\
		if (grafp & 0x80) {							\
			int j = sizep;							\
			do {									\
				if (hposp < ATARI_WIDTH / 2)		\
					P##n##PL |= pm_scanline[hposp] |= 1 << n;	\
				hposp++;							\
			} while (--j);							\
		}											\
		else										\
			hposp += sizep;							\
		grafp <<= 1;								\
	} while (grafp);								\
}

	DO_PLAYER(0)
	DO_PLAYER(1)
	DO_PLAYER(2)
	DO_PLAYER(3)

/* Draw Missiles */

#define DO_MISSILE(n,p,m,r,l)	if (GRAFM & m) {	\
	if (GRAFM & r) {								\
		int j = global_sizem[n];					\
		unsigned hposm = global_hposm##n;			\
		if (GRAFM & l)								\
			j <<= 1;								\
		do {										\
			if (hposm < ATARI_WIDTH / 2)			\
				M##n##PL |= pm_scanline[hposm] |= p;\
			hposm++;								\
		} while (--j);								\
	}												\
	else {											\
		int j = global_sizem[n];					\
		unsigned hposm = global_hposm##n + j;		\
		do {										\
			if (hposm < ATARI_WIDTH / 2)			\
				M##n##PL |= pm_scanline[hposm] |= p;\
			hposm++;								\
		} while (--j);								\
	}												\
}

	if (GRAFM) {
		pm_dirty = TRUE;
		DO_MISSILE(3,0x80,0xc0,0x80,0x40)
		DO_MISSILE(2,0x40,0x30,0x20,0x10)
		DO_MISSILE(1,0x20,0x0c,0x08,0x04)
		DO_MISSILE(0,0x10,0x03,0x02,0x01)
	}
}

/* GTIA registers ---------------------------------------------------------- */

UBYTE GTIA_GetByte(UWORD addr)
{
	UBYTE byte = 0x0f;	/* write-only registers return 0x0f */

	switch (addr & 0x1f) {
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
	case _P0PL:
		byte = (P1PL & 0x01) << 1;	/* mask in player 1 */
		byte |= (P2PL & 0x01) << 2;	/* mask in player 2 */
		byte |= (P3PL & 0x01) << 3;	/* mask in player 3 */
		break;
	case _P1PL:
		byte = (P1PL & 0x01);		/* mask in player 0 */
		byte |= (P2PL & 0x02) << 1;	/* mask in player 2 */
		byte |= (P3PL & 0x02) << 2;	/* mask in player 3 */
		break;
	case _P2PL:
		byte = (P2PL & 0x03);		/* mask in player 0 and 1 */
		byte |= (P3PL & 0x04) << 1;	/* mask in player 3 */
		break;
	case _P3PL:
		byte = P3PL & 0x07;			/* mask in player 0,1, and 2 */
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
			byte = 1;
		break;
	case _TRIG3:
		if (!mach_xlxe)
			byte = Atari_TRIG(3);
		else
			/* extremely important patch - thanks to this hundred of games start running (BruceLee) */
			byte = rom_inserted;
		break;
	case _PAL:
		if (tv_mode == TV_PAL)
			byte = 0x01;
		/* 0x0f is default */
		/* else
			byte = 0x0f; */
		break;
	case _CONSOL:
		if (next_console_value != 7) {
			byte = (next_console_value | 0x08) & consol_mask;
			next_console_value = 0x07;
		}
		else {
			/* 0x08 is because 'speaker is always 'on' '
			consol_mask is set by CONSOL (write) !PM! */
			byte = (Atari_CONSOL() | 0x08) & consol_mask;
		}
		break;
	}

	return byte;
}

void GTIA_PutByte(UWORD addr, UBYTE byte)
{
	UWORD cword;
	switch (addr & 0x1f) {
	case _COLBK:
		byte &= 0xfe;			/* clip lowest bit. 16 lum only in gtia 9! */

		COLBK = byte;
		cword = colour_translation_table[byte];
		cword |= cword << 8;
		cl_word[8] = cword;
		if (cword != (UWORD) (lookup_gtia9[0]) ) {
			lookup_gtia9[0] = cword | (cword << 16);
			if (PRIOR & 0x40)
				setup_gtia9_11();
		}
		break;
	case _COLPF0:
		byte &= 0xfe;			/* clip lowest bit. 16 lum only in gtia 9! */

		COLPF0 = byte;
		cword = colour_translation_table[byte];
		cword |= cword << 8;
		cl_word[4] = cword;
		cl_word[R_COLPM0_OR_PF0] = cl_word[0] | cword;
		cl_word[R_COLPM1_OR_PF0] = cl_word[1] | cword;
		cl_word[R_COLPM0OR1_OR_PF0] = cl_word[R_COLPM0OR1] | cword;
		break;
	case _COLPF1:
		byte &= 0xfe;			/* clip lowest bit. 16 lum only in gtia 9! */

		COLPF1 = byte;
		cword = colour_translation_table[byte];
		cword |= cword << 8;
		cl_word[5] = cword;
		cl_word[R_COLPM0_OR_PF1] = cl_word[0] | cword;
		cl_word[R_COLPM1_OR_PF1] = cl_word[1] | cword;
		cl_word[R_COLPM0OR1_OR_PF1] = cl_word[R_COLPM0OR1] | cword;
		((UBYTE *)hires_lookup_l)[0x80] = ((UBYTE *)hires_lookup_l)[0x41] = byte & 0x0f;
		hires_lookup_l[0x60] = cword & 0xf0f;
		break;
	case _COLPF2:
		byte &= 0xfe;			/* clip lowest bit. 16 lum only in gtia 9! */

		COLPF2 = byte;
		cword = colour_translation_table[byte];
		cword |= cword << 8;
		cl_word[6] = cword;
		cl_word[R_COLPM2_OR_PF2] = cl_word[0] | cword;
		cl_word[R_COLPM3_OR_PF2] = cl_word[1] | cword;
		cl_word[R_COLPM2OR3_OR_PF2] = cl_word[R_COLPM2OR3] | cword;
		break;
	case _COLPF3:
		byte &= 0xfe;			/* clip lowest bit. 16 lum only in gtia 9! */

		COLPF3 = byte;
		cword = colour_translation_table[byte];
		cword |= cword << 8;
		cl_word[7] = cword;
		cl_word[R_COLPM2_OR_PF3] = cl_word[0] | cword;
		cl_word[R_COLPM3_OR_PF3] = cl_word[1] | cword;
		cl_word[R_COLPM2OR3_OR_PF3] = cl_word[R_COLPM2OR3] | cword;
		break;
	case _COLPM0:
		byte &= 0xfe;			/* clip lowest bit. 16 lum only in gtia 9! */

		COLPM0 = byte;
		cword = colour_translation_table[byte];
		cword |= cword << 8;
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
		cword = colour_translation_table[byte];
		cword |= cword << 8;
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
		cword = colour_translation_table[byte];
		cword |= cword << 8;
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
		cword = colour_translation_table[byte];
		cword |= cword << 8;
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
#endif
		consol_mask = (~byte) & 0x0f;
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
		M0PL = M1PL = M2PL = M3PL = 0;
		P0PL = P1PL = P2PL = P3PL = 0;
		pf_colls[4] = pf_colls[5] = pf_colls[6] = pf_colls[7] = 0;
		break;
	case _HPOSM0:
		HPOSM0 = byte;
		global_hposm0 = byte - 0x20;
		break;
	case _HPOSM1:
		HPOSM1 = byte;
		global_hposm1 = byte - 0x20;
		break;
	case _HPOSM2:
		HPOSM2 = byte;
		global_hposm2 = byte - 0x20;
		break;
	case _HPOSM3:
		HPOSM3 = byte;
		global_hposm3 = byte - 0x20;
		break;
	case _HPOSP0:
		HPOSP0 = byte;
		global_hposp0 = byte - 0x20;
		break;
	case _HPOSP1:
		HPOSP1 = byte;
		global_hposp1 = byte - 0x20;
		break;
	case _HPOSP2:
		HPOSP2 = byte;
		global_hposp2 = byte - 0x20;
		break;
	case _HPOSP3:
		HPOSP3 = byte;
		global_hposp3 = byte - 0x20;
		break;
	case _SIZEM:
		SIZEM = byte;
		global_sizem[0] = PM_Width[byte & 0x03];
		global_sizem[1] = PM_Width[(byte & 0x0c) >> 2];
		global_sizem[2] = PM_Width[(byte & 0x30) >> 4];
		global_sizem[3] = PM_Width[(byte & 0xc0) >> 6];
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
		set_prior(byte);
		PRIOR = byte;
		if (PRIOR & 0x40)
			setup_gtia9_11();
		break;
	case _VDELAY:
		VDELAY = byte;
		break;
	case _GRACTL:
		GRACTL = byte;
		missile_gra_enabled = (byte & 0x01);
		player_gra_enabled = (byte & 0x02);
		player_flickering = ((player_dma_enabled | player_gra_enabled) == 0x02);
		missile_flickering = ((missile_dma_enabled | missile_gra_enabled) == 0x01);
		break;
	}
}

/* State ------------------------------------------------------------------- */

void GTIAStateSave( void )
{
	SaveUBYTE( &HPOSP0, 1 );
	SaveUBYTE( &HPOSP1, 1 );
	SaveUBYTE( &HPOSP2, 1 );
	SaveUBYTE( &HPOSP3, 1 );
	SaveUBYTE( &HPOSM0, 1 );
	SaveUBYTE( &HPOSM1, 1 );
	SaveUBYTE( &HPOSM2, 1 );
	SaveUBYTE( &HPOSM3, 1 );
	SaveUBYTE( &M0PL, 1 );
	SaveUBYTE( &M1PL, 1 );
	SaveUBYTE( &M2PL, 1 );
	SaveUBYTE( &M3PL, 1 );
	SaveUBYTE( &P0PL, 1 );
	SaveUBYTE( &P1PL, 1 );
	SaveUBYTE( &P2PL, 1 );
	SaveUBYTE( &P3PL, 1 );
	SaveUBYTE( &SIZEP0, 1 );
	SaveUBYTE( &SIZEP1, 1 );
	SaveUBYTE( &SIZEP2, 1 );
	SaveUBYTE( &SIZEP3, 1 );
	SaveUBYTE( &SIZEM, 1 );
	SaveUBYTE( &GRAFP0, 1 );
	SaveUBYTE( &GRAFP1, 1 );
	SaveUBYTE( &GRAFP2, 1 );
	SaveUBYTE( &GRAFP3, 1 );
	SaveUBYTE( &GRAFM, 1 );
	SaveUBYTE( &COLPM0, 1 );
	SaveUBYTE( &COLPM1, 1 );
	SaveUBYTE( &COLPM2, 1 );
	SaveUBYTE( &COLPM3, 1 );
	SaveUBYTE( &COLPF0, 1 );
	SaveUBYTE( &COLPF1, 1 );
	SaveUBYTE( &COLPF2, 1 );
	SaveUBYTE( &COLPF3, 1 );
	SaveUBYTE( &COLBK, 1 );
	SaveUBYTE( &PRIOR, 1 );
	SaveUBYTE( &VDELAY, 1 );
	SaveUBYTE( &GRACTL, 1 );

	SaveUBYTE( &consol_mask, 1 );
	SaveUBYTE( &pf_colls[0], 9 );

	SaveINT( &atari_speaker, 1 );
	SaveINT( &next_console_value, 1 );
}

void GTIAStateRead( void )
{
	ReadUBYTE( &HPOSP0, 1 );
	ReadUBYTE( &HPOSP1, 1 );
	ReadUBYTE( &HPOSP2, 1 );
	ReadUBYTE( &HPOSP3, 1 );
	ReadUBYTE( &HPOSM0, 1 );
	ReadUBYTE( &HPOSM1, 1 );
	ReadUBYTE( &HPOSM2, 1 );
	ReadUBYTE( &HPOSM3, 1 );
	ReadUBYTE( &M0PL, 1 );
	ReadUBYTE( &M1PL, 1 );
	ReadUBYTE( &M2PL, 1 );
	ReadUBYTE( &M3PL, 1 );
	ReadUBYTE( &P0PL, 1 );
	ReadUBYTE( &P1PL, 1 );
	ReadUBYTE( &P2PL, 1 );
	ReadUBYTE( &P3PL, 1 );
	ReadUBYTE( &SIZEP0, 1 );
	ReadUBYTE( &SIZEP1, 1 );
	ReadUBYTE( &SIZEP2, 1 );
	ReadUBYTE( &SIZEP3, 1 );
	ReadUBYTE( &SIZEM, 1 );
	ReadUBYTE( &GRAFP0, 1 );
	ReadUBYTE( &GRAFP1, 1 );
	ReadUBYTE( &GRAFP2, 1 );
	ReadUBYTE( &GRAFP3, 1 );
	ReadUBYTE( &GRAFM, 1 );
	ReadUBYTE( &COLPM0, 1 );
	ReadUBYTE( &COLPM1, 1 );
	ReadUBYTE( &COLPM2, 1 );
	ReadUBYTE( &COLPM3, 1 );
	ReadUBYTE( &COLPF0, 1 );
	ReadUBYTE( &COLPF1, 1 );
	ReadUBYTE( &COLPF2, 1 );
	ReadUBYTE( &COLPF3, 1 );
	ReadUBYTE( &COLBK, 1 );
	ReadUBYTE( &PRIOR, 1 );
	ReadUBYTE( &VDELAY, 1 );
	ReadUBYTE( &GRACTL, 1 );

	ReadUBYTE( &consol_mask, 1 );
	ReadUBYTE( &pf_colls[0], 9 );

	ReadINT( &atari_speaker, 1 );
	ReadINT( &next_console_value, 1 );

	GTIA_PutByte(_HPOSP0, HPOSP0);
	GTIA_PutByte(_HPOSP1, HPOSP1);
	GTIA_PutByte(_HPOSP2, HPOSP2);
	GTIA_PutByte(_HPOSP3, HPOSP3);
	GTIA_PutByte(_HPOSM0, HPOSM0);
	GTIA_PutByte(_HPOSM1, HPOSM1);
	GTIA_PutByte(_HPOSM2, HPOSM2);
	GTIA_PutByte(_HPOSM3, HPOSM3);
	GTIA_PutByte(_SIZEP0, SIZEP0);
	GTIA_PutByte(_SIZEP1, SIZEP1);
	GTIA_PutByte(_SIZEP2, SIZEP2);
	GTIA_PutByte(_SIZEP3, SIZEP3);
	GTIA_PutByte(_SIZEM, SIZEM);
	GTIA_PutByte(_GRAFP0, GRAFP0);
	GTIA_PutByte(_GRAFP1, GRAFP1);
	GTIA_PutByte(_GRAFP2, GRAFP2);
	GTIA_PutByte(_GRAFP3, GRAFP3);
	GTIA_PutByte(_GRAFM, GRAFM);
	GTIA_PutByte(_COLPM0, COLPM0);
	GTIA_PutByte(_COLPM1, COLPM1);
	GTIA_PutByte(_COLPM2, COLPM2);
	GTIA_PutByte(_COLPM3, COLPM3);
	GTIA_PutByte(_COLPF0, COLPF0);
	GTIA_PutByte(_COLPF1, COLPF1);
	GTIA_PutByte(_COLPF2, COLPF2);
	GTIA_PutByte(_COLPF3, COLPF3);
	GTIA_PutByte(_COLBK, COLBK);
	GTIA_PutByte(_PRIOR, PRIOR);
	GTIA_PutByte(_GRACTL, GRACTL);
	pm_dirty = TRUE;
}
