#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <vga.h>

#include "config.h"
#include "atari.h"
#include "colours.h"
#include "monitor.h"
#include "nas.h"
#include "sound.h"
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

#include <linux/keyboard.h>
#include <vgakeyboard.h>

extern int SHIFT_KEY, KEYPRESSED;
static int pause_hit = 0;
static UBYTE kbhits[NR_KEYS];
static int kbcode = 0;
static int kbjoy = 1;
static UBYTE joydefs[9] =
{
  SCANCODE_KEYPAD0,	/* fire */
  SCANCODE_KEYPAD7,	/* up/left */
  SCANCODE_KEYPAD8,	/* up */
  SCANCODE_KEYPAD9,	/* up/right */
  SCANCODE_KEYPAD4,	/* left */
  SCANCODE_KEYPAD6,	/* right */
  SCANCODE_KEYPAD1,	/* down/left */
  SCANCODE_KEYPAD2,	/* down */
  SCANCODE_KEYPAD3,	/* down/right */
};

static UBYTE joymask[9] =
{
  0, 		/* not used */
  ~1 & ~4,	/* up/left */
  ~1, 		/* up */
  ~1 & ~8, 	/* up/right */
  ~4,		/* left */
  ~8,		/* right */
  ~2 & ~4,	/* down/left */
  ~2,		/* down */
  ~2 & ~8,	/* down/right */
};


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

static int first_lno = 0;
static int ypos_inc = 1;
static int svga_ptr_inc = 320;
static int scrn_ptr_inc = ATARI_WIDTH;

/* define some functions */
/*
void Atari_DisplayScreen(UBYTE * screen);
void LeaveVGAMode(void);
*/


/****************************************************************************/
/* Linux raw keyboard handler  				    		    */
/* by Krzysztof Nikiel, 1999                                                */
/****************************************************************************/
static void kbhandler(int code, int state)
{
  if (code < 0 || code >= NR_KEYS)
    {
      fprintf(stderr, "keyboard code out of range (0x%x).\n", code);
      return;
    }
#if 0
  printf("code: %x(%x) - %s\n", code, state, keynames[code]);
#endif
  if ((code == SCANCODE_BREAK)
       || (code == SCANCODE_BREAK_ALTERNATIVE))
    {
      if (state)
	pause_hit = 1;
      return;
    }
  kbhits[kbcode = code] = state;
  if (!state)
    kbcode |= 0x100;
}

static void initkb(void)
{
  int i;
  if (keyboard_init())
  {
    fprintf(stderr,"unable to initialize keyboard\n");
    exit(1);
  }
  for (i = 0; i < sizeof(kbhits) / sizeof(kbhits[0]); i++)
    kbhits[i] = 0;
  keyboard_seteventhandler(kbhandler);
}

int Atari_Keyboard(void)
{
	int keycode;
        int i;

        keyboard_update();

        if (kbjoy)
        {
          /* fire */
          trig0 = kbhits[joydefs[0]] ? 0 : 1;
          
          stick0 |= 0xf;
          for (i = 1; i < sizeof(joydefs) / sizeof(joydefs[0]); i++)
            if (kbhits[joydefs[i]])
              stick0 &= joymask[i];
        }

        consol = (consol & ~7)
        	| (kbhits[SCANCODE_F2] ? 0 : 4)
        	| (kbhits[SCANCODE_F3] ? 0 : 2)
        	| (kbhits[SCANCODE_F4] ? 0 : 1);

        if (pause_hit)
        {
          pause_hit = 0;
          return AKEY_BREAK;
        }
    
        SHIFT_KEY = (kbhits[SCANCODE_LEFTSHIFT]
                   | kbhits[SCANCODE_RIGHTSHIFT]) ? 1 : 0;

        /* need to set shift mask here to aviod conflict with PC layout */
        keycode = (SHIFT_KEY ? 0x40 : 0)
        	| ((kbhits[SCANCODE_LEFTCONTROL]
                   | kbhits[SCANCODE_RIGHTCONTROL]) ? 0x80 : 0);
                
        switch (kbcode)
        {
          case SCANCODE_F9:
            return AKEY_EXIT;
            break;
          case SCANCODE_F8:
            keycode = Atari_Exit(1) ? AKEY_NONE : AKEY_EXIT;
            kbcode = 0;
            break;
          case SCANCODE_SCROLLLOCK: /* pause */
            while (kbhits[SCANCODE_SCROLLLOCK])
              keyboard_update();
            while (!kbhits[SCANCODE_SCROLLLOCK])
              keyboard_update();
            kbcode = 0;
            break;
          case SCANCODE_PRINTSCREEN:
            keycode = AKEY_SCREENSHOT;
            kbcode = 0;
            break;
          case SCANCODE_F1:
            return AKEY_UI;
            break;
          case SCANCODE_F5:
            return SHIFT_KEY ? AKEY_COLDSTART : AKEY_WARMSTART;
            break;
#define KBSCAN(name) \
          case SCANCODE_##name: \
            keycode |= (AKEY_##name & ~(AKEY_CTRL | AKEY_SHFT)); \
            break;
	  KBSCAN(ESCAPE)
	  KBSCAN(1)
	  case SCANCODE_2:
            if (SHIFT_KEY)
              keycode = AKEY_AT;
            else
              keycode |= AKEY_2;
            break;
	  KBSCAN(3)
	  KBSCAN(4)
	  KBSCAN(5)
	  case SCANCODE_6:
            if (SHIFT_KEY)
              keycode = AKEY_CARET;
            else
              keycode |= AKEY_6;
            break;
	  case SCANCODE_7:
            if (SHIFT_KEY)
              keycode = AKEY_AMPERSAND;
            else
              keycode |= AKEY_7;
            break;
	  case SCANCODE_8:
            if (SHIFT_KEY)
              keycode = AKEY_ASTERISK;
            else
              keycode |= AKEY_8;
            break;
	  KBSCAN(9)
	  KBSCAN(0)
          KBSCAN(MINUS)
          case SCANCODE_EQUAL:
            if (SHIFT_KEY)
              keycode = AKEY_PLUS;
            else
              keycode |= AKEY_EQUAL;
            break;
          KBSCAN(BACKSPACE)
          KBSCAN(TAB)
	  KBSCAN(Q)
	  KBSCAN(W)
	  KBSCAN(E)
	  KBSCAN(R)
	  KBSCAN(T)
	  KBSCAN(Y)
	  KBSCAN(U)
	  KBSCAN(I)
	  KBSCAN(O)
	  KBSCAN(P)
	  case SCANCODE_BRACKET_LEFT:
            keycode |= AKEY_BRACKETLEFT;
            break;
	  case SCANCODE_BRACKET_RIGHT:
            keycode |= AKEY_BRACKETRIGHT;
            break;
          case SCANCODE_ENTER:
            keycode |= AKEY_RETURN;
            break;
	  KBSCAN(A)
	  KBSCAN(S)
	  KBSCAN(D)
	  KBSCAN(F)
	  KBSCAN(G)
	  KBSCAN(H)
	  KBSCAN(J)
	  KBSCAN(K)
	  KBSCAN(L)
	  KBSCAN(SEMICOLON)
	  case SCANCODE_APOSTROPHE:
            if (SHIFT_KEY)
              keycode = AKEY_DBLQUOTE;
            else
              keycode |= AKEY_QUOTE;
            break;
	  case SCANCODE_GRAVE:
            keycode |= AKEY_ATARI;
            break;
	  case SCANCODE_BACKSLASH:
            if (SHIFT_KEY)
              keycode = AKEY_EQUAL | AKEY_SHFT;
            else
              keycode |= AKEY_BACKSLASH;
            break;
	  KBSCAN(Z)
	  KBSCAN(X)
	  KBSCAN(C)
	  KBSCAN(V)
	  KBSCAN(B)
	  KBSCAN(N)
	  KBSCAN(M)
	  case SCANCODE_COMMA:
            if (SHIFT_KEY)
              keycode = AKEY_LESS;
            else
              keycode |= AKEY_COMMA;
            break;
	  case SCANCODE_PERIOD:
            if (SHIFT_KEY)
              keycode = AKEY_GREATER;
            else
              keycode |= AKEY_FULLSTOP;
            break;
	  KBSCAN(SLASH)
	  KBSCAN(SPACE)
	  KBSCAN(CAPSLOCK)
          case SCANCODE_CURSORBLOCKUP:
            keycode = AKEY_UP;
            break;
          case SCANCODE_CURSORBLOCKDOWN:
            keycode = AKEY_DOWN;
            break;
          case SCANCODE_CURSORBLOCKLEFT:
            keycode = AKEY_LEFT;
            break;
          case SCANCODE_CURSORBLOCKRIGHT:
            keycode = AKEY_RIGHT;
            break;
          case SCANCODE_REMOVE:
            if (SHIFT_KEY)
              keycode = AKEY_DELETE_LINE;
            else
              keycode |= AKEY_DELETE_CHAR;
            break;
          case SCANCODE_INSERT:
            if (SHIFT_KEY)
              keycode = AKEY_INSERT_LINE;
            else
              keycode |= AKEY_INSERT_CHAR;
            break;
          case SCANCODE_HOME:
              keycode = AKEY_CLEAR;
            break;
          case SCANCODE_END:
              keycode = AKEY_HELP;
            break;
          default:
            keycode = AKEY_NONE;
        }

        KEYPRESSED = (keycode != AKEY_NONE);
	return keycode;
}
/****************************************************************************/
/* end of keyboard handler                                                  */
/****************************************************************************/



void Atari_Initialise(int *argc, char *argv[])
{
	int VGAMODE = G320x240x256;

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

        initkb();

	for (i = 0; i < 256; i++) {
		int rgb = colortable[i];
		int red;
		int green;
		int blue;

		red = (rgb & 0x00ff0000) >> 18;
		green = (rgb & 0x0000ff00) >> 10;
		blue = (rgb & 0x000000ff) >> 2;

                vga_setpalette(i, red, green, blue);
	}

	trig0 = 1;
	stick0 = 15;
	consol = 7;
}

int Atari_Exit(int run_monitor)
{
	int restart;

        keyboard_close();
	vga_setmode(TEXT);

	if (run_monitor)
		restart = monitor();
	else
		restart = FALSE;

	if (restart) {
		int VGAMODE = G320x240x256;
		/* int status; */
		int i;

		if (!vga_hasmode(VGAMODE)) {
			fprintf(stderr,"Mode not available\n");
			exit(1);
		}
		vga_setmode(VGAMODE);

                initkb();
                
		for (i = 0; i < 256; i++) {
			int rgb = colortable[i];
			int red;
			int green;
			int blue;

			red = (rgb & 0x00ff0000) >> 18;
			green = (rgb & 0x0000ff00) >> 10;
			blue = (rgb & 0x000000ff) >> 2;

                        vga_setpalette(i, red, green, blue);
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

#ifdef SET_LED
static UBYTE LEDstatus = 0; /*status of disk LED*/
void Atari_Set_LED(int how)
{
  LEDstatus = 8 * how;
}
#endif

void Atari_DisplayScreen(UBYTE * screen)
{
  static int writepage = 0;
  UBYTE *vbuf = screen + first_lno * ATARI_WIDTH + 32;
  
#ifdef SET_LED
  if (LEDstatus)  /*draw disk LED*/
  {
    int i;
    for (i = 0; i < 6; i++)
    {
      vbuf[i] = 0xba;
      vbuf[i + ATARI_WIDTH] = 0xba;
    }
    LEDstatus--;
  }
#endif

  vga_copytoplanar256(vbuf, ATARI_WIDTH,
		      ((320 * 240) >> 2) * writepage
		      , 320 >> 2, 320,
		      240);
  vga_setdisplaystart(320 * 240 * writepage);
  vga_setpage(0);
  writepage ^= 1;

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

