

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <vga.h>
#include <vgagl.h>

static char *rcsid = "$Id: atari_svgalib.c,v 1.11 1998/02/21 14:53:17 david Exp $";

#include "config.h"
#include "atari.h"
#include "colours.h"
#include "monitor.h"
#include "nas.h"
#include "platform.h"

#ifdef JOYMOUSE
#include <vgamouse.h>

#define CENTER_X 16384
#define CENTER_Y 16384
#define THRESHOLD 15
#endif

#ifdef LINUX_JOYSTICK
#include <linux/joystick.h>

static int js0;
static int js1;

static int js0_centre_x;
static int js0_centre_y;
static int js1_centre_x;
static int js1_centre_y;

static struct JS_DATA_TYPE js_data;
#endif

static int lookup[256];

#define FALSE 0
#define TRUE 1

static int vgamouse_stick;
static int vgamouse_strig;
static int trig0;
static int stick0;
static int consol;

extern double deltatime;

/*
   Interlace variables
 */

static int first_lno = 24;
static int ypos_inc = 1;
static int svga_ptr_inc = 320;
static int scrn_ptr_inc = ATARI_WIDTH;

void Atari_Initialise(int *argc, char *argv[])
{
	int VGAMODE = G320x200x256;

	int fd;
	int status;

	int i;
	int j;

#ifdef NAS
	NAS_Initialise(argc, argv);
#endif

#ifdef VOXWARE
	Voxware_Initialise(argc, argv);
#endif

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-interlace") == 0) {
			ypos_inc = 2;
			svga_ptr_inc = 320 + 320;
			scrn_ptr_inc = ATARI_WIDTH + ATARI_WIDTH;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				printf("\t-interlace    Generate screen with interlace\n");
			}
			argv[j++] = argv[i];
		}
	}

	*argc = j;

#ifdef JOYMOUSE
	if (mouse_init("/dev/mouse", MOUSE_PS2, MOUSE_DEFAULTSAMPLERATE) == -1) {
		perror("/dev/mouse");
		exit(1);
	}
	mouse_setposition(CENTER_X, CENTER_Y);
#endif

#ifdef LINUX_JOYSTICK
	js0 = open("/dev/js0", O_RDONLY, 0777);
	if (js0 != -1) {
		int status;

		status = read(js0, &js_data, JS_RETURN);
		if (status != JS_RETURN) {
			perror("/dev/js0");
			exit(1);
		}
		js0_centre_x = js_data.x;
		js0_centre_y = js_data.y;
	}
	js1 = open("/dev/js1", O_RDONLY, 0777);
	if (js1 != -1) {
		int status;

		status = read(js1, &js_data, JS_RETURN);
		if (status != JS_RETURN) {
			perror("/dev/js1");
			exit(1);
		}
		js1_centre_x = js_data.x;
		js1_centre_y = js_data.y;
	}
#endif

	vga_init();

	if (!vga_hasmode(VGAMODE)) {
		printf("Mode not available\n");
		exit(1);
	}
	vga_setmode(VGAMODE);

	gl_setcontextvga(VGAMODE);

	for (i = 0; i < 256; i++) {
		int rgb = colortable[i];
		int red;
		int green;
		int blue;

		red = (rgb & 0x00ff0000) >> 18;
		green = (rgb & 0x0000ff00) >> 10;
		blue = (rgb & 0x000000ff) >> 2;

		gl_setpalettecolor(i, red, green, blue);
	}

	trig0 = 1;
	stick0 = 15;
	consol = 7;
}

int Atari_Exit(int run_monitor)
{
	int restart;

	vga_setmode(TEXT);

	if (run_monitor)
		restart = monitor();
	else
		restart = FALSE;

	if (restart) {
		int VGAMODE = G320x200x256;
		int status;
		int i;

		if (!vga_hasmode(VGAMODE)) {
			printf("Mode not available\n");
			exit(1);
		}
		vga_setmode(VGAMODE);

		gl_setcontextvga(VGAMODE);

		for (i = 0; i < 256; i++) {
			int rgb = colortable[i];
			int red;
			int green;
			int blue;

			red = (rgb & 0x00ff0000) >> 18;
			green = (rgb & 0x0000ff00) >> 10;
			blue = (rgb & 0x000000ff) >> 2;

			gl_setpalettecolor(i, red, green, blue);
		}
	}
	else {
#ifdef JOYMOUSE
		mouse_close();
#endif

#ifdef LINUX_JOYSTICK
		if (js0 != -1)
			close(js0);

		if (js1 != -1)
			close(js1);
#endif

#ifdef NAS
		NAS_Exit();
#endif

#ifdef VOXWARE
		Voxware_Exit();
#endif
	}

	return restart;
}

void Atari_DisplayScreen(UBYTE * screen)
{
	static int lace = 0;

	UBYTE *svga_ptr;
	UBYTE *scrn_ptr;
	int ypos;

/*
 * This single command will replace all this function but doesn't allow
 * the interlace option - maybe the interlace option should dropped.
 *
 * gl_putboxpart (0, 0, 320, 200, 384, 240, screen, 32, first_lno);
 */

	svga_ptr = graph_mem;
	scrn_ptr = &screen[first_lno * ATARI_WIDTH + 32];

	if (ypos_inc == 2) {
		if (lace) {
			svga_ptr += 320;
			scrn_ptr += ATARI_WIDTH;
		}
		lace = 1 - lace;
	}
	for (ypos = 0; ypos < 200; ypos += ypos_inc) {
		memcpy(svga_ptr, scrn_ptr, 320);
		svga_ptr += svga_ptr_inc;
		scrn_ptr += scrn_ptr_inc;
	}

#ifdef JOYMOUSE
	vgamouse_stick = 0xff;

	if (mouse_update() != 0) {
		int x;
		int y;

		x = mouse_getx();
		y = mouse_gety();

		if (x < (CENTER_X - THRESHOLD))
			vgamouse_stick &= 0xfb;
		else if (x > (CENTER_X + THRESHOLD))
			vgamouse_stick &= 0xf7;

		if (y < (CENTER_Y - THRESHOLD))
			vgamouse_stick &= 0xfe;
		else if (y > (CENTER_Y + THRESHOLD))
			vgamouse_stick &= 0xfd;

		mouse_setposition(CENTER_X, CENTER_Y);
	}
	if (mouse_getbutton())
		vgamouse_strig = 0;
	else
		vgamouse_strig = 1;
#endif

#ifdef NAS
	NAS_UpdateSound();
#endif

#ifdef VOXWARE
	Voxware_UpdateSound();
#endif
}

static int special_keycode = -1;

int Atari_Keyboard(void)
{
	int keycode;

	trig0 = 1;

	if (special_keycode != -1) {
		keycode = special_keycode;
		special_keycode = -1;
	}
	else
		keycode = vga_getkey();

	switch (keycode) {
	case 0x01:
		keycode = AKEY_CTRL_a;
		break;
	case 0x02:
		keycode = AKEY_CTRL_b;
		break;
/*
   case 0x03 :
   keycode = AKEY_CTRL_c;
   break;
 */
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
		keycode = AKEY_CTRL_h;
		break;
	case 0x09:
		keycode = AKEY_CTRL_i;
		break;
/*
   case 0x0a :
   keycode = AKEY_CTRL_j;
   break;
 */
	case 0x0b:
		keycode = AKEY_CTRL_k;
		break;
	case 0x0c:
		keycode = AKEY_CTRL_l;
		break;
/*
   case 0x0d :
   keycode = AKEY_CTRL_m;
   break;
 */
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
	case '~':
		keycode = AKEY_CAPSLOCK;
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
	case '{':
		keycode = AKEY_NONE;
		break;
	case '}':
		keycode = AKEY_NONE;
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
	case 0x7f:					/* Backspace */
		keycode = AKEY_BACKSPACE;
		break;
	case '\n':
		keycode = AKEY_RETURN;
		break;
	case 0x1b:
		{
			char buff[10];
			int nc;

			nc = 0;
			while (((keycode = vga_getkey()) != 0) && (nc < 8))
				buff[nc++] = keycode;
			buff[nc++] = '\0';

			if (nc == 1) {
				keycode = AKEY_ESCAPE;
			}
			else if (strcmp(buff, "\133\133\101") == 0)		/* F1 */
				keycode = AKEY_UI;
			else if (strcmp(buff, "\133\133\102") == 0) {	/* F2 */
				consol &= 0x03;
				keycode = AKEY_NONE;
			}
			else if (strcmp(buff, "\133\133\103") == 0) {	/* F3 */
				consol &= 0x05;
				keycode = AKEY_NONE;
			}
			else if (strcmp(buff, "\133\133\104") == 0) {	/* F4 */
				consol &= 0x06;
				keycode = AKEY_NONE;
			}
			else if (strcmp(buff, "[28~") == 0)		/* Shift F5 */
				keycode = AKEY_COLDSTART;
			else if (strcmp(buff, "\133\133\105") == 0)		/* F5 */
				keycode = AKEY_WARMSTART;
			else if (strcmp(buff, "\133\061\067\176") == 0) {	/* F6 */
				keycode = AKEY_PIL;
			}
			else if (strcmp(buff, "\133\061\070\176") == 0) {	/* F7 */
				keycode = AKEY_BREAK;
			}
			else if (strcmp(buff, "\133\061\071\176") == 0) {	/* F8 */
				keycode = AKEY_NONE;
			}
			else if (strcmp(buff, "\133\062\060\176") == 0) {	/* F9 */
				keycode = AKEY_EXIT;
			}
			else if (strcmp(buff, "\133\062\061\176") == 0) {	/* F10 */
				keycode = AKEY_NONE;
				if (deltatime == 0.0)
					deltatime = (1.0 / 8.0);
				else
					deltatime = 0.0;
			}
			else if (strcmp(buff, "\133\062\063\176") == 0) {	/* F11 */
				if (first_lno > 0)
					first_lno--;
				keycode = AKEY_NONE;
			}
			else if (strcmp(buff, "\133\062\064\176") == 0) {	/* F12 */
				if (first_lno < (ATARI_HEIGHT - 200))
					first_lno++;
				keycode = AKEY_NONE;
			}
			else if (strcmp(buff, "\141\000") == 0)
				keycode = AKEY_CTRL_A;
			else if (strcmp(buff, "\142\000") == 0)
				keycode = AKEY_CTRL_B;
			else if (strcmp(buff, "\143\000") == 0)
				keycode = AKEY_CTRL_C;
			else if (strcmp(buff, "\144\000") == 0)
				keycode = AKEY_CTRL_D;
			else if (strcmp(buff, "\145\000") == 0)
				keycode = AKEY_CTRL_E;
			else if (strcmp(buff, "\146\000") == 0)
				keycode = AKEY_CTRL_F;
			else if (strcmp(buff, "\147\000") == 0)
				keycode = AKEY_CTRL_G;
			else if (strcmp(buff, "\150\000") == 0)
				keycode = AKEY_CTRL_H;
			else if (strcmp(buff, "\151\000") == 0)
				keycode = AKEY_CTRL_I;
			else if (strcmp(buff, "\152\000") == 0)
				keycode = AKEY_CTRL_J;
			else if (strcmp(buff, "\153\000") == 0)
				keycode = AKEY_CTRL_K;
			else if (strcmp(buff, "\154\000") == 0)
				keycode = AKEY_CTRL_L;
			else if (strcmp(buff, "\155\000") == 0)
				keycode = AKEY_CTRL_M;
			else if (strcmp(buff, "\156\000") == 0)
				keycode = AKEY_CTRL_N;
			else if (strcmp(buff, "\157\000") == 0)
				keycode = AKEY_CTRL_O;
			else if (strcmp(buff, "\160\000") == 0)
				keycode = AKEY_CTRL_P;
			else if (strcmp(buff, "\161\000") == 0)
				keycode = AKEY_CTRL_Q;
			else if (strcmp(buff, "\162\000") == 0)
				keycode = AKEY_CTRL_R;
			else if (strcmp(buff, "\163\000") == 0)
				keycode = AKEY_CTRL_S;
			else if (strcmp(buff, "\164\000") == 0)
				keycode = AKEY_CTRL_T;
			else if (strcmp(buff, "\165\000") == 0)
				keycode = AKEY_CTRL_U;
			else if (strcmp(buff, "\166\000") == 0)
				keycode = AKEY_CTRL_V;
			else if (strcmp(buff, "\167\000") == 0)
				keycode = AKEY_CTRL_W;
			else if (strcmp(buff, "\170\000") == 0)
				keycode = AKEY_CTRL_X;
			else if (strcmp(buff, "\171\000") == 0)
				keycode = AKEY_CTRL_Y;
			else if (strcmp(buff, "\172\000") == 0)
				keycode = AKEY_CTRL_Z;
			else if (strcmp(buff, "\133\062\176") == 0) {	/* Keypad 0 */
				trig0 = 0;
				keycode = AKEY_NONE;
			}
			else if (strcmp(buff, "\133\064\176") == 0) {	/* Keypad 1 */
				stick0 = STICK_LL;
				keycode = AKEY_NONE;
			}
			else if (strcmp(buff, "\133\102") == 0) {	/* Keypad 2 */
				stick0 = STICK_BACK;
				keycode = AKEY_DOWN;
			}
			else if (strcmp(buff, "\133\066\176") == 0) {	/* Keypad 3 */
				stick0 = STICK_LR;
				keycode = AKEY_NONE;
			}
			else if (strcmp(buff, "\133\104") == 0) {	/* Keypad 4 */
				stick0 = STICK_LEFT;
				keycode = AKEY_LEFT;
			}
			else if (strcmp(buff, "\133\107") == 0) {	/* Keypad 5 */
				stick0 = STICK_CENTRE;
				keycode = AKEY_NONE;
			}
			else if (strcmp(buff, "\133\103") == 0) {	/* Keypad 6 */
				stick0 = STICK_RIGHT;
				keycode = AKEY_RIGHT;
			}
			else if (strcmp(buff, "\133\061\176") == 0) {	/* Keypad 7 */
				stick0 = STICK_UL;
				keycode = AKEY_NONE;
			}
			else if (strcmp(buff, "\133\101") == 0) {	/* Keypad 8 */
				stick0 = STICK_FORWARD;
				keycode = AKEY_UP;
			}
			else if (strcmp(buff, "\133\065\176") == 0) {	/* Keypad 9 */
				stick0 = STICK_UR;
				keycode = AKEY_NONE;
			}
			else {
				int i;
				printf("Unknown key (%s): 0x1b ", buff);
				for (i = 0; i < nc; i++)
					printf("0x%02x ", buff[i]);
				printf("\n");
				keycode = AKEY_NONE;
			}
		}
		break;
	default:
		printf("Unknown Keycode: %d\n", keycode);
	case 0:
		keycode = AKEY_NONE;
		break;
	}

	return keycode;
}

#ifdef LINUX_JOYSTICK
void read_joystick(int js, int centre_x, int centre_y)
{
	const int threshold = 50;
	int status;

	stick0 = 0x0f;

	status = read(js, &js_data, JS_RETURN);
	if (status != JS_RETURN) {
		perror("/dev/js");
		exit(1);
	}
	if (js_data.x < (centre_x - threshold))
		stick0 &= 0xfb;
	if (js_data.x > (centre_x + threshold))
		stick0 &= 0xf7;
	if (js_data.y < (centre_y - threshold))
		stick0 &= 0xfe;
	if (js_data.y > (centre_y + threshold))
		stick0 &= 0xfd;
}
#endif

int Atari_PORT(int num)
{
	if (num == 0) {
#ifdef JOYMOUSE
		stick0 = vgamouse_stick;
#endif

#ifdef LINUX_JOYSTICK
		read_joystick(js0, js0_centre_x, js0_centre_y);
#endif
		return 0xf0 | stick0;
	}
	else
		return 0xff;
}

int Atari_TRIG(int num)
{
	if (num == 0) {
#ifdef JOYMOUSE
		trig0 = vgamouse_strig;
#endif

#ifdef LINUX_JOYSTICK
		int status;

		status = read(js0, &js_data, JS_RETURN);
		if (status != JS_RETURN) {
			perror("/dev/js0");
			exit(1);
		}
		if (js_data.buttons & 0x01)
			trig0 = 0;
		else
			trig0 = 1;

		if (js_data.buttons & 0x02)
			special_keycode = ' ';
/*
   trig0 = (js_data.buttons & 0x0f) ? 0 : 1;
 */
#endif
		return trig0;
	}
	else
		return 1;
}

int Atari_POT(int num)
{
	return 228;
}

int Atari_CONSOL(void)
{
	return consol;
}
