/* -------------------------------------------------------------------------- */

/*
 * Falcon Backend for David Firth's Atari 800 emulator
 *
 * by Petr Stehlik & Karel Rous (C)1997-98  See file COPYRIGHT for policy.
 *
 */

/* -------------------------------------------------------------------------- */

#include <osbind.h>
#include <string.h>
#include <stdio.h>
#include <linea.h>
#include "cpu.h"
#include "colours.h"
#include "config.h"

/* -------------------------------------------------------------------------- */

#define FALSE 0
#define TRUE 1

int get_cookie(long cookie, long *value)
{
	long *cookiejar = (long *) Setexc(0x168, -1L);

	if (cookiejar) {
		while (*cookiejar) {
			if (*cookiejar == cookie) {
				if (value)
					*value = *++cookiejar;
				return (1);
			}
			cookiejar += 2;
		}
	}
	return (0);
}

#include "jclkcook.h"

int screensaver(int on)
{
	long adr;
	JCLKSTRUCT *jclk;
	int oldval;

	CHECK_CLOCKY_STRUCT;

	if (!get_cookie((long) 'JCLK', &adr))
		return 0;

	jclk = (JCLKSTRUCT *) adr;

	if (jclk->name != CLOCKY_IDENT) {
		return 0;
	}

	if (jclk->version != CLOCKY_VERSION) {
		return 0;
	}

	oldval = jclk->parametry.Saveron;
	jclk->parametry.Saveron = on;	/* turn the Clocky's screen saver on/off */

	return oldval;
}

static int screensaverval;		/* original value */

/* -------------------------------------------------------------------------- */

#ifdef BACKUP_MSG
extern char backup_msg_buf[300];
#endif

#define nodebug 1
static int consol;
static int trig0;
static int stick0;
static int joyswap = FALSE;

short screenw = ATARI_WIDTH;
short screenh = ATARI_HEIGHT;
UBYTE *odkud, *kam;
static int jen_nekdy = 0;
static int resolution = 0;

#ifdef CPUASS
#include "cpu_asm.h"
#endif
extern void init_kb(void);
extern void rem_kb(void);
extern char key_buf[128];
extern UBYTE joy0, joy1, buttons;
_KEYTAB *key_tab;
#define	KEYBUF_SIZE	256
unsigned char keybuf[KEYBUF_SIZE];
int kbhead = 0;

UBYTE *Log_base, *Phys_base;

UWORD stara_graf[25];
UWORD nova_graf_384[25] =
{0x0057, 0x0000, 0x0000, 0x0000, 0x0000, 0x0007, 0x0000, 0x0010, \
 0x00C0, 0x0186, 0x0005, 0x00D7, 0x00B2, 0x000B, 0x02A1, 0x00A0, \
 0x00B9, 0x0000, 0x0000, 0x0425, 0x03F3, 0x0033, 0x0033, 0x03F3, 0x0415};

UWORD nova_graf_352[25] =
{0x003b, 0x0001, 0x004a, 0x0160, 0x00F0, 0x0008, 0x0000, 0x0010, \
 0x00b0, 0x0186, 0x0005, 0x00D7, 0x00a2, 0x001B, 0x02b1, 0x0090, \
 0x00B9, 0x0000, 0x0000, 0x0425, 0x03F3, 0x0033, 0x0033, 0x03F3, 0x0415};

UWORD nova_graf_320[25] =
{0xc133, 0x0001, 0x2c00, 0x0140, 0x00f0, 0x0008, 0x0002, 0x0010, \
 0x00a0, 0x0186, 0x0005, 0x00c6, 0x008d, 0x0015, 0x029a, 0x007b, \
 0x0096, 0x0000, 0x0000, 0x0419, 0x03FF, 0x003f, 0x003f, 0x03Ff, 0x0415};

UBYTE *nova_vram;

extern void rplanes(void);
extern void rplanes_320(void);
extern void rplanes_352(void);
extern void load_r(void);
extern void save_r(void);
extern ULONG *p_str_p;

ULONG f030coltable[256];
ULONG f030coltable_zaloha[256];
ULONG *col_table;

void get_colors_on_f030(void)
{
	int i;
	ULONG *x = (ULONG *) 0xff9800;

	for (i = 0; i < 256; i++)
		col_table[i] = x[i];
}

void set_colors_on_f030(void)
{
	int i;
	ULONG *x = (ULONG *) 0xff9800;

	for (i = 0; i < 256; i++)
		x[i] = col_table[i];
}

/* -------------------------------------------------------------------------- */

void SetupFalconEnvironment(void)
{
	long int a, r, g, b;
	/* vypni kurzor */
	Cursconf(0, 0);

	/* switch to new graphics mode 384x240x256 */
#define DELKA_VIDEORAM	(384UL*256UL)
	nova_vram = (UBYTE *) Mxalloc((DELKA_VIDEORAM), 0);
	if (nova_vram == NULL) {
		printf("Error allocating video memory\n");
		exit(-1);
	}

#if nodebug
	memset(nova_vram, 0, DELKA_VIDEORAM);
	Setscreen(nova_vram, nova_vram, -1);

	/* check current resolution - if it is one of supported, do not touch VIDEL */
	linea0();
	if (VPLANES == 8 && V_Y_MAX == 240 && (
								   (resolution == 2 && V_X_MAX == 384) ||
								   (resolution == 1 && V_X_MAX == 352) ||
								   (resolution == 0 && V_X_MAX == 320)));	/* the current resolution is OK */
	else {
		switch (resolution) {
		case 2:
			p_str_p = (ULONG *) nova_graf_384;
			break;
		case 1:
			p_str_p = (ULONG *) nova_graf_352;
			break;
		case 0:
		default:
			p_str_p = (ULONG *) nova_graf_320;
		}
		Supexec(load_r);		/* set new video resolution by direct VIDEL programming */
	}

	/* nastav nove barvy */
	for (a = 0; a < 256; a++) {
		r = (colortable[a] >> 18) & 0x3f;
		g = (colortable[a] >> 10) & 0x3f;
		b = (colortable[a] >> 2) & 0x3f;
		f030coltable[a] = (r << 26) | (g << 18) | (b << 2);
	}
	col_table = f030coltable;
	Supexec(set_colors_on_f030);

	Supexec(init_kb);

	/* joystick init */
	Bconout(4, 0x14);
#endif
}

void Atari_Initialise(int *argc, char *argv[])
{
	int i;
	int j;
	long val;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-interlace") == 0) {
			sscanf(argv[++i], "%d", &jen_nekdy);
		}
		else if (strcmp(argv[i], "-resolution") == 0) {
			sscanf(argv[++i], "%d", &resolution);
		}
		else if (strcmp(argv[i], "-joyswap") == 0)
			joyswap = TRUE;
		else {
			if (strcmp(argv[i], "-help") == 0) {
				printf("\t-interlace number  Generate screen only every number-th frame\n");
				printf("\t-resolution X      0 => 320x240, 1 => 352x240, 2=> 384x240\n");
			}

			argv[j++] = argv[i];
		}
	}

	*argc = j;

	if (!get_cookie('_VDO', &val))
		val = 0;

	if (val != 0x30000) {
		printf("This Atari800 emulator requires Falcon video hardware\n");
		exit(-1);
	}

#ifdef DMASOUND
	Sound_Initialise(argc, argv);
#endif

	screensaverval = screensaver(0);	/* turn off screen saver */

#if nodebug
	/* uschovat stare barvy */
	col_table = f030coltable_zaloha;
	Supexec(get_colors_on_f030);

	/* uschovej starou grafiku */
	p_str_p = (ULONG *) stara_graf;
	Supexec(save_r);
#endif

	Log_base = Logbase();
	Phys_base = Physbase();

	key_tab = Keytbl(-1, -1, -1);
	consol = 7;
#ifdef CPUASS
	CPU_INIT();
#endif

	SetupFalconEnvironment();
}

/* -------------------------------------------------------------------------- */

int Atari_Exit(int run_monitor)
{

#if nodebug
	/* vratit puvodni graficky mod */
	p_str_p = (ULONG *) stara_graf;
	Supexec(load_r);
	Setscreen(Log_base, Phys_base, -1);

	/* obnovit stare barvy */
	col_table = f030coltable_zaloha;
	Supexec(set_colors_on_f030);

	Supexec(rem_kb);

	/* joystick disable */
	Bconout(4, 8);

#endif
	/* zapni kurzor */
	Cursconf(1, 0);

	/* smaz obraz */
	printf("\33E");

#ifdef BACKUP_MSG
	if (*backup_msg_buf) {		/* print the buffer */
		puts(backup_msg_buf);
		*backup_msg_buf = 0;
	}
#endif

	if (run_monitor)
		if (monitor()) {
			SetupFalconEnvironment();

			return 1;			/* go back to emulation */
		}

#ifdef DMASOUND
	Sound_Exit();
#endif

	screensaver(screensaverval);

	return 0;
}

/* -------------------------------------------------------------------------- */

void Atari_DisplayScreen(UBYTE * screen)
{
	static int i = 0;
	if (i >= jen_nekdy) {
		odkud = screen;
		kam = Logbase();
		switch (resolution) {
		case 2:
			rplanes();
			break;
		case 1:
			rplanes_352();
			break;
		case 0:
		default:
			rplanes_320();
		}
		i = 0;
	}
	else
		i++;

	consol = 7;
}

/* -------------------------------------------------------------------------- */

extern int SHIFT_KEY, KEYPRESSED;

int Atari_Keyboard(void)
{
	UBYTE control_key;
	int scancode, keycode;
	int i;

	consol = 7;

	trig0 = 1;
	stick0 = STICK_CENTRE;

	SHIFT_KEY = (key_buf[0x2a] || key_buf[0x36]);
	control_key = key_buf[0x1d];

	if (!SHIFT_KEY && !control_key) {
		if (key_buf[0x70])
			trig0 = 0;
		if (key_buf[0x6d] || key_buf[0x6e] || key_buf[0x6f])
			stick0 -= (STICK_CENTRE - STICK_BACK);
		if (key_buf[0x6f] || key_buf[0x6c] || key_buf[0x69])
			stick0 -= (STICK_CENTRE - STICK_RIGHT);
		if (key_buf[0x6d] || key_buf[0x6a] || key_buf[0x67])
			stick0 -= (STICK_CENTRE - STICK_LEFT);
		if (key_buf[0x67] || key_buf[0x68] || key_buf[0x69])
			stick0 -= (STICK_CENTRE - STICK_FORWARD);
	}

	scancode = 0;

	if (stick0 == STICK_CENTRE && trig0 == 1) {
		for (i = 1; i <= 0x72; i++)		/* test vsech klaves postupne */
			if (key_buf[i]) {
				if (i == 0x1d || i == 0x2a || i == 0x36)	/* prerazovace preskoc */
					continue;
				scancode = i;
				break;
			}
	}

	if (scancode) {

		/* precti stisknutou klavesu */
		if (SHIFT_KEY)
			keycode = *(UBYTE *) (key_tab->shift + scancode);
		else
			keycode = *(UBYTE *) (key_tab->unshift + scancode);

		if (control_key)
			keycode -= 64;

		switch (keycode) {
		case 0x01:
			keycode = AKEY_CTRL_a;
			break;
		case 0x02:
			keycode = AKEY_CTRL_b;
			break;
		case 0x03:
			keycode = AKEY_CTRL_c;
			break;
		case 0x04:
			keycode = AKEY_CTRL_d;
			break;
		case 0x05:
			keycode = AKEY_CTRL_e;
			break;
		case 0x06:
			keycode = AKEY_CTRL_f;
			break;
		case 0x07:
			keycode = AKEY_CTRL_g;
			break;
		case 0x08:
			if (scancode == 0x0e)
				keycode = AKEY_BACKSPACE;
			else
				keycode = AKEY_CTRL_h;
			break;
		case 0x09:
			if (scancode == 0x0f) {
				if (SHIFT_KEY)
					keycode = AKEY_SETTAB;
				else if (control_key)
					keycode = AKEY_CLRTAB;
				else
					keycode = AKEY_TAB;
			}
			else
				keycode = AKEY_CTRL_i;
			break;
		case 0x0a:
			keycode = AKEY_CTRL_j;
			break;
		case 0x0b:
			keycode = AKEY_CTRL_k;
			break;
		case 0x0c:
			keycode = AKEY_CTRL_l;
			break;
		case 0x0d:
			if (scancode == 0x1c || scancode == 0x72)
				keycode = AKEY_RETURN;
			else
				keycode = AKEY_CTRL_m;
			break;
		case 0x0e:
			keycode = AKEY_CTRL_n;
			break;
		case 0x0f:
			keycode = AKEY_CTRL_o;
			break;
		case 0x10:
			keycode = AKEY_CTRL_p;
			break;
		case 0x11:
			keycode = AKEY_CTRL_q;
			break;
		case 0x12:
			keycode = AKEY_CTRL_r;
			break;
		case 0x13:
			keycode = AKEY_CTRL_s;
			break;
		case 0x14:
			keycode = AKEY_CTRL_t;
			break;
		case 0x15:
			keycode = AKEY_CTRL_u;
			break;
		case 0x16:
			keycode = AKEY_CTRL_v;
			break;
		case 0x17:
			keycode = AKEY_CTRL_w;
			break;
		case 0x18:
			keycode = AKEY_CTRL_x;
			break;
		case 0x19:
			keycode = AKEY_CTRL_y;
			break;
		case 0x1a:
			keycode = AKEY_CTRL_z;
			break;
		case ' ':
			keycode = AKEY_SPACE;
			break;
		case '`':
			keycode = AKEY_CAPSTOGGLE;
			break;
		case '!':
			keycode = AKEY_EXCLAMATION;
			break;
		case '"':
			keycode = AKEY_DBLQUOTE;
			break;
		case '#':
			keycode = AKEY_HASH;
			break;
		case '$':
			keycode = AKEY_DOLLAR;
			break;
		case '%':
			keycode = AKEY_PERCENT;
			break;
		case '&':
			keycode = AKEY_AMPERSAND;
			break;
		case '\'':
			keycode = AKEY_QUOTE;
			break;
		case '@':
			keycode = AKEY_AT;
			break;
		case '(':
			keycode = AKEY_PARENLEFT;
			break;
		case ')':
			keycode = AKEY_PARENRIGHT;
			break;
		case '[':
			keycode = AKEY_BRACKETLEFT;
			break;
		case ']':
			keycode = AKEY_BRACKETRIGHT;
			break;
		case '<':
			keycode = AKEY_LESS;
			break;
		case '>':
			keycode = AKEY_GREATER;
			break;
		case '=':
			keycode = AKEY_EQUAL;
			break;
		case '?':
			keycode = AKEY_QUESTION;
			break;
		case '-':
			keycode = AKEY_MINUS;
			break;
		case '+':
			keycode = AKEY_PLUS;
			break;
		case '*':
			keycode = AKEY_ASTERISK;
			break;
		case '/':
			keycode = AKEY_SLASH;
			break;
		case ':':
			keycode = AKEY_COLON;
			break;
		case ';':
			keycode = AKEY_SEMICOLON;
			break;
		case ',':
			keycode = AKEY_COMMA;
			break;
		case '.':
			keycode = AKEY_FULLSTOP;
			break;
		case '_':
			keycode = AKEY_UNDERSCORE;
			break;
		case '^':
			keycode = AKEY_CIRCUMFLEX;
			break;
		case '\\':
			keycode = AKEY_BACKSLASH;
			break;
		case '|':
			keycode = AKEY_BAR;
			break;
		case '0':
			keycode = AKEY_0;
			break;
		case '1':
			keycode = AKEY_1;
			break;
		case '2':
			keycode = AKEY_2;
			break;
		case '3':
			keycode = AKEY_3;
			break;
		case '4':
			keycode = AKEY_4;
			break;
		case '5':
			keycode = AKEY_5;
			break;
		case '6':
			keycode = AKEY_6;
			break;
		case '7':
			keycode = AKEY_7;
			break;
		case '8':
			keycode = AKEY_8;
			break;
		case '9':
			keycode = AKEY_9;
			break;
		case 'a':
			keycode = AKEY_a;
			break;
		case 'b':
			keycode = AKEY_b;
			break;
		case 'c':
			keycode = AKEY_c;
			break;
		case 'd':
			keycode = AKEY_d;
			break;
		case 'e':
			keycode = AKEY_e;
			break;
		case 'f':
			keycode = AKEY_f;
			break;
		case 'g':
			keycode = AKEY_g;
			break;
		case 'h':
			keycode = AKEY_h;
			break;
		case 'i':
			keycode = AKEY_i;
			break;
		case 'j':
			keycode = AKEY_j;
			break;
		case 'k':
			keycode = AKEY_k;
			break;
		case 'l':
			keycode = AKEY_l;
			break;
		case 'm':
			keycode = AKEY_m;
			break;
		case 'n':
			keycode = AKEY_n;
			break;
		case 'o':
			keycode = AKEY_o;
			break;
		case 'p':
			keycode = AKEY_p;
			break;
		case 'q':
			keycode = AKEY_q;
			break;
		case 'r':
			keycode = AKEY_r;
			break;
		case 's':
			keycode = AKEY_s;
			break;
		case 't':
			keycode = AKEY_t;
			break;
		case 'u':
			keycode = AKEY_u;
			break;
		case 'v':
			keycode = AKEY_v;
			break;
		case 'w':
			keycode = AKEY_w;
			break;
		case 'x':
			keycode = AKEY_x;
			break;
		case 'y':
			keycode = AKEY_y;
			break;
		case 'z':
			keycode = AKEY_z;
			break;
		case 'A':
			keycode = AKEY_A;
			break;
		case 'B':
			keycode = AKEY_B;
			break;
		case 'C':
			keycode = AKEY_C;
			break;
		case 'D':
			keycode = AKEY_D;
			break;
		case 'E':
			keycode = AKEY_E;
			break;
		case 'F':
			keycode = AKEY_F;
			break;
		case 'G':
			keycode = AKEY_G;
			break;
		case 'H':
			keycode = AKEY_H;
			break;
		case 'I':
			keycode = AKEY_I;
			break;
		case 'J':
			keycode = AKEY_J;
			break;
		case 'K':
			keycode = AKEY_K;
			break;
		case 'L':
			keycode = AKEY_L;
			break;
		case 'M':
			keycode = AKEY_M;
			break;
		case 'N':
			keycode = AKEY_N;
			break;
		case 'O':
			keycode = AKEY_O;
			break;
		case 'P':
			keycode = AKEY_P;
			break;
		case 'Q':
			keycode = AKEY_Q;
			break;
		case 'R':
			keycode = AKEY_R;
			break;
		case 'S':
			keycode = AKEY_S;
			break;
		case 'T':
			keycode = AKEY_T;
			break;
		case 'U':
			keycode = AKEY_U;
			break;
		case 'V':
			keycode = AKEY_V;
			break;
		case 'W':
			keycode = AKEY_W;
			break;
		case 'X':
			keycode = AKEY_X;
			break;
		case 'Y':
			keycode = AKEY_Y;
			break;
		case 'Z':
			keycode = AKEY_Z;
			break;
		case 0x1b:
			keycode = AKEY_ESCAPE;
			break;
		case 0x00:
			switch (scancode) {
			case 0x3b:			/* F1 */
			case 0x61:			/* Undo */
				keycode = AKEY_UI;
				break;
			case 0x62:			/* Help */
				keycode = AKEY_HELP;
				break;
			case 0x3c:			/* F2 */
				consol &= 0x03;
				keycode = AKEY_NONE;
				break;
			case 0x3d:			/* F3 */
				consol &= 0x05;
				keycode = AKEY_NONE;
				break;
			case 0x3e:			/* F4 */
				consol &= 0x06;
				keycode = AKEY_NONE;
				break;
			case 0x3f:			/* F5 */
				keycode = SHIFT_KEY ? AKEY_COLDSTART : AKEY_WARMSTART;
				break;
			case 0x40:			/* F6 - used to be PILL mode switch */
				/* keycode = AKEY_PIL; */
				keycode = AKEY_NONE;
				break;
			case 0x41:			/* F7 */
				keycode = AKEY_BREAK;
				break;
			case 0x42:			/* F8 */
				keycode = Atari_Exit(1) ? AKEY_NONE : AKEY_EXIT;	/* invoke monitor */
				break;
			case 0x43:			/* F9 */
				keycode = AKEY_EXIT;
				break;
			case 0x50:
				keycode = AKEY_DOWN;
				break;
			case 0x4b:
				keycode = AKEY_LEFT;
				break;
			case 0x4d:
				keycode = AKEY_RIGHT;
				break;
			case 0x48:
				keycode = AKEY_UP;
				break;
			default:
				keycode = AKEY_NONE;
				break;
			}
			break;
		default:
			keycode = AKEY_NONE;
			break;
		}
	}
	else
		keycode = AKEY_NONE;

	KEYPRESSED = (keycode != AKEY_NONE);

	return keycode;
}

/* -------------------------------------------------------------------------- */

int Atari_PORT(int num)
{
	if (num == 0) {
		if (stick0 == STICK_CENTRE && trig0 == 1)
			return (((~joy1 << 4) & 0xf0) | ((~joy0) & 0x0f));
		else {
			if (joyswap)
				return ((stick0 << 4) | ((~joy0) & 0x0f));
			else
				return (((~joy0 << 4) & 0xf0) | stick0);
		}
	}
	else
		return 0xff;
}

/* -------------------------------------------------------------------------- */

int Atari_TRIG(int num)
{
	switch (num) {
	case 0:
		return (joy0 > 0x0f) ? 0 : joyswap ? 1 : trig0;
	case 1:
		return (joy1 > 0x0f) ? 0 : joyswap ? trig0 : 1;
	case 2:
	case 3:
	default:
		return 1;
	}
}

/* -------------------------------------------------------------------------- */

int Atari_POT(int num)
{
	return 228;
}

/* -------------------------------------------------------------------------- */

int Atari_CONSOL(void)
{
	return consol;
}

/* -------------------------------------------------------------------------- */
