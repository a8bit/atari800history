#ifndef __ATARI__
#define	__ATARI__

#include	"system.h"

#define	ATARI_WIDTH	(384)
#define	ATARI_HEIGHT	(192 + 24 + 24)
#define	ATARI_TITLE	"Atari 800 Emulator, Version 0.2.1"

#define	NORMAL8_CART	1
#define	NORMAL16_CART	2
#define	OSS_SUPERCART	3
#define	DB_SUPERCART	4

enum ESCAPE
{
	ESC_SIO,
	ESC_E_OPEN,
	ESC_E_CLOSE,
	ESC_E_READ,
	ESC_E_WRITE,
	ESC_E_STATUS,
	ESC_E_SPECIAL,
	ESC_K_OPEN,
	ESC_K_CLOSE,
	ESC_K_READ,
	ESC_K_WRITE,
	ESC_K_STATUS,
	ESC_K_SPECIAL,
	ESC_H_OPEN,
	ESC_H_CLOSE,
	ESC_H_READ,
	ESC_H_WRITE,
	ESC_H_STATUS,
	ESC_H_SPECIAL
};

/*
	==================
	Hardware Registers
	==================
*/

extern UBYTE	ALLPOT;
extern UBYTE	CHACTL;
extern UBYTE	CHBASE;
extern UBYTE	COLBK;
extern UBYTE	DLISTH;
extern UBYTE	DLISTL;
extern UBYTE	DMACTL;
extern UBYTE	GRACTL;
extern UBYTE	GRAFM;
extern UBYTE	GRAFP0;
extern UBYTE	GRAFP1;
extern UBYTE	GRAFP2;
extern UBYTE	GRAFP3;
extern UBYTE	HSCROL;
extern UBYTE	IRQEN;
extern UBYTE	IRQST;
extern UBYTE	KBCODE;
extern UBYTE	M0PF;
extern UBYTE	M0PL;
extern UBYTE	M1PF;
extern UBYTE	M1PL;
extern UBYTE	M2PF;
extern UBYTE	M2PL;
extern UBYTE	M3PF;
extern UBYTE	M3PL;
extern UBYTE	NMIEN;
extern UBYTE	NMIRES;
extern UBYTE	NMIST;
extern UBYTE	P0PF;
extern UBYTE	P0PL;
extern UBYTE	P1PF;
extern UBYTE	P1PL;
extern UBYTE	P2PF;
extern UBYTE	P2PL;
extern UBYTE	P3PF;
extern UBYTE	P3PL;
extern UBYTE	PACTL;
extern UBYTE	PAL;
extern UBYTE	PBCTL;
extern UBYTE	PENH;
extern UBYTE	PENV;
extern UBYTE	PMBASE;
extern UBYTE	PORTA;
extern UBYTE	PORTB;
extern UBYTE	POTGO;
extern UBYTE	PRIOR;
extern UBYTE	SERIN;
extern UBYTE	SEROUT;
extern UBYTE	SKCTL;
extern UBYTE	SKREST;
extern UBYTE	SKSTAT;
extern UBYTE	STIMER;
extern UBYTE	VCOUNT;
extern UBYTE	VDELAY;
extern UBYTE	VSCROL;

/*
	=================
	Joystick Position
	=================
*/

#define	STICK_LL	0x09;
#define	STICK_BACK	0x0d;
#define	STICK_LR	0x05;
#define	STICK_LEFT	0x0b;
#define	STICK_CENTRE	0x0f;
#define	STICK_RIGHT	0x07;
#define	STICK_UL	0x0a;
#define	STICK_FORWARD	0x0e;
#define	STICK_UR	0x06;

/*
	=================================================================
	Defines return values to be used by the various keyboard handlers
	=================================================================
*/

typedef enum
{
	AKEY_NONE = -1,
	AKEY_WARMSTART = 256,
	AKEY_COLDSTART,
	AKEY_EXIT,
	AKEY_HELP,
	AKEY_BREAK,
	AKEY_PIL,
	AKEY_DOWN,
	AKEY_LEFT,
	AKEY_RIGHT,
	AKEY_UP,
	AKEY_BACKSPACE,
	AKEY_DELETE_CHAR,
	AKEY_DELETE_LINE,
	AKEY_INSERT_CHAR,
	AKEY_INSERT_LINE,
	AKEY_ESCAPE,
	AKEY_ATARI,
	AKEY_CAPSLOCK,
	AKEY_CAPSTOGGLE,
	AKEY_TAB,
	AKEY_SETTAB,
	AKEY_CLRTAB,
	AKEY_RETURN,

	AKEY_CTRL_A,
	AKEY_CTRL_B,
	AKEY_CTRL_C,
	AKEY_CTRL_D,
	AKEY_CTRL_E,
	AKEY_CTRL_F,
	AKEY_CTRL_G,
	AKEY_CTRL_H,
	AKEY_CTRL_I,
	AKEY_CTRL_J,
	AKEY_CTRL_K,
	AKEY_CTRL_L,
	AKEY_CTRL_M,
	AKEY_CTRL_N,
	AKEY_CTRL_O,
	AKEY_CTRL_P,
	AKEY_CTRL_Q,
	AKEY_CTRL_R,
	AKEY_CTRL_S,
	AKEY_CTRL_T,
	AKEY_CTRL_U,
	AKEY_CTRL_V,
	AKEY_CTRL_W,
	AKEY_CTRL_X,
	AKEY_CTRL_Y,
	AKEY_CTRL_Z,

	AKEY_CTRL_0,
	AKEY_CTRL_1,
	AKEY_CTRL_2,
	AKEY_CTRL_3,
	AKEY_CTRL_4,
	AKEY_CTRL_5,
	AKEY_CTRL_6,
	AKEY_CTRL_7,
	AKEY_CTRL_8,
	AKEY_CTRL_9,

	AKEY_SHFTCTRL_A,
	AKEY_SHFTCTRL_B,
	AKEY_SHFTCTRL_C,
	AKEY_SHFTCTRL_D,
	AKEY_SHFTCTRL_E,
	AKEY_SHFTCTRL_F,
	AKEY_SHFTCTRL_G,
	AKEY_SHFTCTRL_H,
	AKEY_SHFTCTRL_I,
	AKEY_SHFTCTRL_J,
	AKEY_SHFTCTRL_K,
	AKEY_SHFTCTRL_L,
	AKEY_SHFTCTRL_M,
	AKEY_SHFTCTRL_N,
	AKEY_SHFTCTRL_O,
	AKEY_SHFTCTRL_P,
	AKEY_SHFTCTRL_Q,
	AKEY_SHFTCTRL_R,
	AKEY_SHFTCTRL_S,
	AKEY_SHFTCTRL_T,
	AKEY_SHFTCTRL_U,
	AKEY_SHFTCTRL_V,
	AKEY_SHFTCTRL_W,
	AKEY_SHFTCTRL_X,
	AKEY_SHFTCTRL_Y,
	AKEY_SHFTCTRL_Z
} AtariKey;

/*
	==========================
	Define Function Prototypes
	==========================
*/

void	initialise_os ();
void	add_esc (UWORD address, UBYTE esc_code);
void	A800_RUN (void);
void	ESC (UBYTE esc_code);

#endif
