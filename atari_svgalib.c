
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <vga.h>
#include <vgagl.h>

#include <rawkey.h>

#include "config.h"
#include "atari.h"
#include "colours.h"
#include "monitor.h"
#include "nas.h"
#include "platform.h"

#define ikp(k) is_key_pressed(scancode_trans(k))

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

/* static int lookup[256]; */

#define FALSE 0
#define TRUE 1

#ifdef JOYMOUSE
static int vgamouse_stick;
static int vgamouse_strig;
#endif

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

/* define some functions */
/*
void Atari_DisplayScreen(UBYTE * screen);
void LeaveVGAMode(void);
*/

void Atari_Initialise(int *argc, char *argv[])
{
	int VGAMODE = G320x200x256;

	/* int fd; */
	/* int status; */

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
	if (mouse_init("/dev/mouse", vga_getmousetype(), MOUSE_DEFAULTSAMPLERATE) == -1) {
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
		fprintf(stderr,"Mode not available\n");
		exit(1);
	}
	vga_setmode(VGAMODE);

	gl_setcontextvga(VGAMODE);

	if (!rawmode_init()) {
	  fprintf(stderr,"Error entering rawmode\n");
	  vga_setmode(TEXT);
	  exit(1);
	}

/*
        set_switch_functions(LeaveVGAMode,Atari_DisplayScreen);
        allow_switch(1);
*/

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

	rawmode_exit();
	vga_setmode(TEXT);

	if (run_monitor)
		restart = monitor();
	else
		restart = FALSE;

	if (restart) {
		int VGAMODE = G320x200x256;
		/* int status; */
		int i;

		if (!vga_hasmode(VGAMODE)) {
			fprintf(stderr,"Mode not available\n");
			exit(1);
		}
		vga_setmode(VGAMODE);

		gl_setcontextvga(VGAMODE);

		if (!rawmode_init()) {
		  fprintf(stderr,"Error entering rawmode\n");
		  vga_setmode(TEXT);
		  exit(1);
		}

/*
		set_switch_functions(LeaveVGAMode,Atari_DisplayScreen);
		allow_switch(1);
*/
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

/* static int special_keycode = -1; */
static int GCK = 1;  /* Generate_Cursor_Keys */

int Atari_Keyboard(void)
{
	int keycode = AKEY_NONE;

	consol = 7;
	trig0 = 1;
 
	scan_keyboard();

/* First of all: cursor arrows and emulating joystick on keyboard */

	if ( is_key_pressed(INSERT_KEY) || is_key_pressed(RIGHT_SHIFT) )
	  trig0 = 0;

	stick0 = STICK_CENTRE;
	if ( is_key_pressed(CURSOR_DOWN) ) {
	  stick0 &= 0xfd;    /* 1101 */
	  if (GCK) keycode = AKEY_DOWN;
	}
	if ( is_key_pressed(CURSOR_UP) ) {
	  stick0 &= 0xfe;    /* 1110 */
	  if (GCK) keycode = AKEY_UP;
	}
	if ( is_key_pressed(CURSOR_LEFT) ) {
	  stick0 &= 0xfb;    /* 1011 */
	  if (GCK) keycode = AKEY_LEFT;
	}
	if ( is_key_pressed(CURSOR_RIGHT) ) {
	  stick0 &= 0xf7;    /* 0111 */
	  if (GCK) keycode = AKEY_RIGHT;
	}


/* Now testing function keys and some specials */

	if ( is_key_pressed(FUNC_KEY(1)) ) keycode = AKEY_UI;

	else if ( is_key_pressed(FUNC_KEY(2)) ) {
		consol &= 0x03;
		keycode = AKEY_NONE;
	}

	else if ( is_key_pressed(FUNC_KEY(3)) ) {
		consol &= 0x05;
		keycode = AKEY_NONE;
	}

	else if ( is_key_pressed(FUNC_KEY(4)) ) {
		consol &= 0x06;
		keycode = AKEY_NONE;
	}

	else if ( is_key_pressed(FUNC_KEY(5)) ) {
	  if ( is_key_pressed(LEFT_SHIFT) )
	    keycode = AKEY_COLDSTART;           /* Shift F5 */
	  else
	    keycode = AKEY_WARMSTART;           /* F5 */
	}

	else if ( is_key_pressed(FUNC_KEY(6)) ) keycode = AKEY_PIL;
	      
	else if ( is_key_pressed(FUNC_KEY(7)) ) keycode = AKEY_BREAK;

	else if ( is_key_pressed(FUNC_KEY(8)) ) {
	  keycode = AKEY_NONE;
	  GCK = !GCK ;
	}
	else if ( is_key_pressed(FUNC_KEY(9)) ) keycode = AKEY_EXIT;

	else if ( is_key_pressed(FUNC_KEY(10)) ) keycode = AKEY_NONE;

	else if ( is_key_pressed(FUNC_KEY(11)) ) keycode = AKEY_NONE;

	else if ( is_key_pressed(FUNC_KEY(12)) ) keycode = AKEY_NONE;

	if ( keycode != AKEY_NONE ) return keycode;
/* To prevent these keycodes from modifying by SHIFT or CTRL */


	if ( ikp('a') ) keycode = AKEY_a;

	else if ( ikp('b') ) keycode = AKEY_b;

	else if ( ikp('c') ) keycode = AKEY_c;

	else if ( ikp('d') ) keycode = AKEY_d;

	else if ( ikp('e') ) keycode = AKEY_e;

	else if ( ikp('f') ) keycode = AKEY_f;

	else if ( ikp('g') ) keycode = AKEY_g;

	else if ( ikp('h') ) keycode = AKEY_h;

	else if ( ikp('i') ) keycode = AKEY_i;

	else if ( ikp('j') ) keycode = AKEY_j;

	else if ( ikp('k') ) keycode = AKEY_k;

	else if ( ikp('l') ) keycode = AKEY_l;

	else if ( ikp('m') ) keycode = AKEY_m;

	else if ( ikp('n') ) keycode = AKEY_n;

	else if ( ikp('o') ) keycode = AKEY_o;

	else if ( ikp('p') ) keycode = AKEY_p;

	else if ( ikp('q') ) keycode = AKEY_q;

	else if ( ikp('r') ) keycode = AKEY_r;

	else if ( ikp('s') ) keycode = AKEY_s;

	else if ( ikp('t') ) keycode = AKEY_t;

	else if ( ikp('u') ) keycode = AKEY_u;

	else if ( ikp('v') ) keycode = AKEY_v;

	else if ( ikp('w') ) keycode = AKEY_w;

	else if ( ikp('x') ) keycode = AKEY_x;

	else if ( ikp('y') ) keycode = AKEY_y;

	else if ( ikp('z') ) keycode = AKEY_z;

	else if ( ikp(' ') ) keycode = AKEY_SPACE;

	else if ( ikp('`') ) keycode = AKEY_CAPSTOGGLE;

	else if ( ikp('~') ) keycode = AKEY_CAPSLOCK;

	else if ( ikp('!') ) keycode = AKEY_EXCLAMATION;

	else if ( ikp('"') ) keycode = AKEY_DBLQUOTE;

	else if ( ikp('#') ) keycode = AKEY_HASH;

	else if ( ikp('$') ) keycode = AKEY_DOLLAR;

	else if ( ikp('%') ) keycode = AKEY_PERCENT;

	else if ( ikp('&') ) keycode = AKEY_AMPERSAND;

	else if ( ikp('\'') ) keycode = AKEY_QUOTE;

	else if ( ikp('@') ) keycode = AKEY_AT;

	else if ( ikp('(') ) keycode = AKEY_PARENLEFT;

	else if ( ikp(')') ) keycode = AKEY_PARENRIGHT;

	else if ( ikp('[') ) keycode = AKEY_BRACKETLEFT;

	else if ( ikp(']') ) keycode = AKEY_BRACKETRIGHT;

	else if ( ikp('<') ) keycode = AKEY_LESS;

	else if ( ikp('>') ) keycode = AKEY_GREATER;

	else if ( ikp('=') ) keycode = AKEY_EQUAL;

	else if ( ikp('?') ) keycode = AKEY_QUESTION;

	else if ( ikp('-') ) keycode = AKEY_MINUS;

	else if ( ikp('+') ) keycode = AKEY_PLUS;

	else if ( ikp('*') ) keycode = AKEY_ASTERISK;

	else if ( ikp('/') ) keycode = AKEY_SLASH;

	else if ( ikp(':') ) keycode = AKEY_COLON;

	else if ( ikp(';') ) keycode = AKEY_SEMICOLON;

	else if ( ikp(',') ) keycode = AKEY_COMMA;

	else if ( ikp('.') ) keycode = AKEY_FULLSTOP;

	else if ( ikp('_') ) keycode = AKEY_UNDERSCORE;

	else if ( ikp('{') ) keycode = AKEY_NONE;

	else if ( ikp('}') ) keycode = AKEY_NONE;

	else if ( ikp('^') ) keycode = AKEY_CIRCUMFLEX;

	else if ( ikp('\\') ) keycode = AKEY_BACKSLASH;

	else if ( ikp('|') ) keycode = AKEY_BAR;

	else if ( ikp('0') ) keycode = AKEY_0;

	else if ( ikp('1') ) keycode = AKEY_1;

	else if ( ikp('2') ) keycode = AKEY_2;

	else if ( ikp('3') ) keycode = AKEY_3;

	else if ( ikp('4') ) keycode = AKEY_4;

	else if ( ikp('5') ) keycode = AKEY_5;

	else if ( ikp('6') ) keycode = AKEY_6;

	else if ( ikp('7') ) keycode = AKEY_7;

	else if ( ikp('8') ) keycode = AKEY_8;

	else if ( ikp('9') ) keycode = AKEY_9;

	else if ( is_key_pressed(BACKSPACE) ) keycode = AKEY_BACKSPACE;

	else if ( is_key_pressed(ENTER_KEY) ) keycode = AKEY_RETURN;

	else if ( is_key_pressed(ESCAPE_KEY) ) keycode = AKEY_ESCAPE;

/* Keys are tested, now comes SHIFT and CTRL */

	if ( is_key_pressed(LEFT_SHIFT) ) keycode |= AKEY_SHFT;
	if ( is_key_pressed(LEFT_CTRL) ) keycode |= AKEY_CTRL;

/* printf("Unknown Keycode: %d\n", keycode); */

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

/*
void LeaveVGAMode(void)
{
  printf("Leaving VGA mode\n");
}
*/








