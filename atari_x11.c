#include <stdio.h>

#ifdef XVIEW
#include <xview/xview.h>
#include <xview/frame.h>
#include <xview/panel.h>
#include <xview/canvas.h>
#include <xview/notice.h>
#include <xview/file_chsr.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#ifdef LINUX_JOYSTICK
#include <linux/joystick.h>
#include <fcntl.h>

static int js0;
static int js1;

static int js0_centre_x;
static int js0_centre_y;
static int js1_centre_x;
static int js1_centre_y;

static struct JS_DATA_TYPE js_data;
#endif

#include "system.h"
#include "atari.h"
#include "colours.h"

#define	FALSE	0
#define	TRUE	1

typedef enum
{
  Small,
  Large,
  Huge
} WindowSize;

static WindowSize windowsize = Large;

static int window_width;
static int window_height;

static Display	*display;
static Screen	*screen;
static Window	window;
static Pixmap	pixmap;
static Visual	*visual;

static GC	gc;
static GC	gc_colour[256];

static XComposeStatus	keyboard_status;

#ifdef XVIEW
static Frame frame;
static Panel panel;
static Canvas canvas;
static Menu disk_menu;
static Menu consol_menu;
static Menu options_menu;
static Frame disk_change_chooser;

static Frame controllers_frame;
static Panel controllers_panel;
static Panel_item keypad_item;
static Panel_item mouse_item;

#ifdef LINUX_JOYSTICK
static Panel_item js0_item;
static Panel_item js1_item;
#endif

static Frame performance_frame;
static Panel performance_panel;
static Panel_item refresh_slider;
static Panel_item countdown_slider;
#endif

static int	SHIFT = 0x00;
static int	CONTROL = 0x00;
static UBYTE	*image_data;
static int	modified;

static int keypad_mode = -1; /* Joystick */
static int keypad_trig = 1; /* Keypad Trigger Position */
static int keypad_stick = 0x0f; /* Keypad Joystick Position */

static int mouse_mode = -1; /* Joystick, Paddle and Light Pen */
static int mouse_stick; /* Mouse Joystick Position */

static int js0_mode = -1;
static int js1_mode = -1;

static int	last_colour = -1;

#define	NPOINTS	(4096/4)
#define	NRECTS	(4096/4)

static int	nrects = 0;
static int	npoints = 0;

static XPoint		points[NPOINTS];
static XRectangle	rectangles[NRECTS];

static int consol;

/*
   ==========================================
   Import a few variables from atari_custom.c
   ==========================================
*/

extern int refresh_rate;
extern int countdown_rate;

int GetKeyCode (XEvent *event)
{
  KeySym keysym;
  char buffer[128];
  int keycode = AKEY_NONE;

  XLookupString ((XKeyEvent*)event, buffer, 128,
		 &keysym, &keyboard_status);

  switch (event->type)
    {
    case Expose :
      XCopyArea (display, pixmap, window, gc,
		 0, 0,
		 window_width, window_height,
		 0, 0);
      break;
    case KeyPress :
      switch (keysym)
	{
	case XK_Shift_L :
	case XK_Shift_R :
	  SHIFT = 0x40;
	  break;
	case XK_Control_L :
	case XK_Control_R :
	  CONTROL = 0x80;
	  break;
	case XK_Caps_Lock :
	  keycode = AKEY_CAPSTOGGLE;
	  break;
	case XK_Shift_Lock :
	  break;
	case XK_Alt_L :
	case XK_Alt_R :
	  keycode = AKEY_ATARI;
	  break;
	case XK_F1 :
	  keycode = AKEY_WARMSTART;
	  break;
	case XK_F2 :
	  consol &= 0x03;
	  keycode = AKEY_NONE;
	  break;
	case XK_F3 :
	  consol &= 0x05;
	  keycode = AKEY_NONE;
	  break;
	case XK_F4 :
	  consol &= 0x6;
	  keycode = AKEY_NONE;
	  break;
	case XK_F5 :
	  keycode = AKEY_COLDSTART;
	  break;
	case XK_F6 :
	  keycode = AKEY_PIL;
	  break;
	case XK_F7 :
	  keycode = AKEY_BREAK;
	  break;
	case XK_F9 :
	  keycode = AKEY_EXIT;
	  break;
	case XK_F10 :
	  keycode = AKEY_NONE;
	  break;
	case XK_Home :
	  keycode = 0x76;
	  break;
	case XK_Insert :
	  if (SHIFT)
	    keycode = AKEY_INSERT_LINE;
	  else
	    keycode = AKEY_INSERT_CHAR;
	  break;
	case XK_Delete :
	  if (CONTROL)
	    keycode = AKEY_DELETE_CHAR;
	  else if (SHIFT)
	    keycode = AKEY_DELETE_LINE;
	  else
	    keycode = AKEY_BACKSPACE;
	  break;
	case XK_Left :
	  keycode = AKEY_LEFT;
	  keypad_stick = STICK_LEFT;
	  break;
	case XK_Up :
	  keycode = AKEY_UP;
	  keypad_stick = STICK_FORWARD;
	  break;
	case XK_Right :
	  keycode = AKEY_RIGHT;
	  keypad_stick = STICK_RIGHT;
	  break;
	case XK_Down :
	  keycode = AKEY_DOWN;
	  keypad_stick = STICK_BACK;
	  break;
	case XK_Escape :
	  keycode = AKEY_ESCAPE;
	  break;
	case XK_Tab :
	  if (CONTROL)
	    keycode = AKEY_CLRTAB;
	  else if (SHIFT)
	    keycode = AKEY_SETTAB;
	  else
	    keycode = AKEY_TAB;
	  break;
	case XK_exclam :
	  keycode = '!';
	  break;
	case XK_quotedbl :
	  keycode = '"';
	  break;
	case XK_numbersign :
	  keycode = '#';
	  break;
	case XK_dollar :
	  keycode = '$';
	  break;
	case XK_percent :
	  keycode = '%';
	  break;
	case XK_ampersand :
	  keycode = '&';
	  break;
	case XK_quoteright :
	  keycode = '\'';
	  break;
	case XK_at :
	  keycode = '@';
	  break;
	case XK_parenleft :
	  keycode = '(';
	  break;
	case XK_parenright :
	  keycode = ')';
	  break;
	case XK_less :
	  keycode = '<';
	  break;
	case XK_greater :
	  keycode = '>';
	  break;
	case XK_equal :
	  keycode = '=';
	  break;
	case XK_question :
	  keycode = '?';
	  break;
	case XK_minus :
	  keycode = '-';
	  break;
	case XK_plus :
	  keycode = '+';
	  break;
	case XK_asterisk :
	  keycode = '*';
	  break;
	case XK_slash :
	  keycode = '/';
	  break;
	case XK_colon :
	  keycode = ':';
	  break;
	case XK_semicolon :
	  keycode = ';';
	  break;
	case XK_comma :
	  keycode = ',';
	  break;
	case XK_period :
	  keycode = '.';
	  break;
	case XK_underscore :
	  keycode = '_';
	  break;
	case XK_bracketleft :
	  keycode = '[';
	  break;
	case XK_bracketright :
	  keycode = ']';
	  break;
	case XK_asciicircum :
	  keycode = '^';
	  break;
	case XK_backslash :
	  keycode = '\\';
	  break;
	case XK_bar :
	  keycode = '|';
	  break;
	case XK_space :
	  keycode = ' ';
	  keypad_trig = 0;
	  break;
	case XK_Return :
	  keycode = AKEY_RETURN;
	  keypad_stick = STICK_CENTRE;
	  break;
	case XK_0 :
	  if (CONTROL)
	    keycode = AKEY_CTRL_0;
	  else
	    keycode = '0';
	  break;
	case XK_1 :
	  if (CONTROL)
	    keycode = AKEY_CTRL_1;
	  else
	    keycode = '1';
	  break;
	case XK_2 :
	  if (CONTROL)
	    keycode = AKEY_CTRL_2;
	  else
	    keycode = '2';
	  break;
	case XK_3 :
	  if (CONTROL)
	    keycode = AKEY_CTRL_3;
	  else
	    keycode = '3';
	  break;
	case XK_4 :
	  if (CONTROL)
	    keycode = AKEY_CTRL_4;
	  else
	    keycode = '4';
	  break;
	case XK_5 :
	  if (CONTROL)
	    keycode = AKEY_CTRL_5;
	  else
	    keycode = '5';
	  break;
	case XK_6 :
	  if (CONTROL)
	    keycode = AKEY_CTRL_6;
	  else
	    keycode = '6';
	  break;
	case XK_7 :
	  if (CONTROL)
	    keycode = AKEY_CTRL_7;
	  else
	    keycode = '7';
	  break;
	case XK_8 :
	  if (CONTROL)
	    keycode = AKEY_CTRL_8;
	  else
	    keycode = '8';
	  break;
	case XK_9 :
	  if (CONTROL)
	    keycode = AKEY_CTRL_9;
	  else
	    keycode = '9';
	  break;
	case XK_a :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_A;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_A;
	  else
	    keycode = 'a';
	  break;
	case XK_b :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_B;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_B;
	  else
	    keycode = 'b';
	  break;
	case XK_c :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_C;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_C;
	  else
	    keycode = 'c';
	  break;
	case XK_d :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_D;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_D;
	  else
	    keycode = 'd';
	  break;
	case XK_e :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_E;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_E;
	  else
	    keycode = 'e';
	  break;
	case XK_f :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_F;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_F;
	  else
	    keycode = 'f';
	  break;
	case XK_g :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_G;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_G;
	  else
	    keycode = 'g';
	  break;
	case XK_h :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_H;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_H;
	  else
	    keycode = 'h';
	  break;
	case XK_i :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_I;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_I;
	  else
	    keycode = 'i';
	  break;
	case XK_j :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_J;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_J;
	  else
	    keycode = 'j';
	  break;
	case XK_k :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_K;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_K;
	  else
	    keycode = 'k';
	  break;
	case XK_l :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_L;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_L;
	  else
	    keycode = 'l';
	  break;
	case XK_m :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_M;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_M;
	  else
	    keycode = 'm';
	  break;
	case XK_n :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_N;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_N;
	  else
	    keycode = 'n';
	  break;
	case XK_o :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_O;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_O;
	  else
	    keycode = 'o';
	  break;
	case XK_p :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_P;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_P;
	  else
	    keycode = 'p';
	  break;
	case XK_q :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_Q;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_Q;
	  else
	    keycode = 'q';
	  break;
	case XK_r :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_R;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_R;
	  else
	    keycode = 'r';
	  break;
	case XK_s :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_S;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_S;
	  else
	    keycode = 's';
	  break;
	case XK_t :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_T;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_T;
	  else
	    keycode = 't';
	  break;
	case XK_u :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_U;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_U;
	  else
	    keycode = 'u';
	  break;
	case XK_v :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_V;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_V;
	  else
	    keycode = 'v';
	  break;
	case XK_w :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_W;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_W;
	  else
	    keycode = 'w';
	  break;
	case XK_x :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_X;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_X;
	  else
	    keycode = 'x';
	  break;
	case XK_y :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_Y;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_Y;
	  else
	    keycode = 'y';
	  break;
	case XK_z :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_Z;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_Z;
	  else
	    keycode = 'z';
	  break;
	case XK_A :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_A;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_A;
	  else
	    keycode = 'A';
	  break;
	case XK_B :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_B;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_B;
	  else
	    keycode = 'B';
	  break;
	case XK_C :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_C;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_C;
	  else
	    keycode = 'C';
	  break;
	case XK_D :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_D;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_D;
	  else
	    keycode = 'D';
	  break;
	case XK_E :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_E;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_E;
	  else
	    keycode = 'E';
	  break;
	case XK_F :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_F;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_F;
	  else
	    keycode = 'F';
	  break;
	case XK_G :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_G;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_G;
	  else
	    keycode = 'G';
	  break;
	case XK_H :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_H;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_H;
	  else
	    keycode = 'H';
	  break;
	case XK_I :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_I;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_I;
	  else
	    keycode = 'I';
	  break;
	case XK_J :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_J;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_J;
	  else
	    keycode = 'J';
	  break;
	case XK_K :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_K;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_K;
	  else
	    keycode = 'K';
	  break;
	case XK_L :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_L;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_L;
	  else
	    keycode = 'L';
	  break;
	case XK_M :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_M;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_M;
	  else
	    keycode = 'M';
	  break;
	case XK_N :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_N;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_N;
	  else
	    keycode = 'N';
	  break;
	case XK_O :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_O;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_O;
	  else
	    keycode = 'O';
	  break;
	case XK_P :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_P;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_P;
	  else
	    keycode = 'P';
	  break;
	case XK_Q :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_Q;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_Q;
	  else
	    keycode = 'Q';
	  break;
	case XK_R :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_R;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_R;
	  else
	    keycode = 'R';
	  break;
	case XK_S :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_S;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_S;
	  else
	    keycode = 'S';
	  break;
	case XK_T :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_T;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_T;
	  else
	    keycode = 'T';
	  break;
	case XK_U :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_U;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_U;
	  else
	    keycode = 'U';
	  break;
	case XK_V :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_V;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_V;
	  else
	    keycode = 'V';
	  break;
	case XK_W :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_W;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_W;
	  else
	    keycode = 'W';
	  break;
	case XK_X :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_X;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_X;
	  else
	    keycode = 'X';
	  break;
	case XK_Y :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_Y;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_Y;
	  else
	    keycode = 'Y';
	  break;
	case XK_Z :
	  if (SHIFT && CONTROL)
	    keycode = AKEY_SHFTCTRL_Z;
	  else if (CONTROL)
	    keycode = AKEY_CTRL_Z;
	  else
	    keycode = 'Z';
	  break;
	case XK_KP_0 :
	  keypad_trig = 0;
	  break;
	case XK_KP_1 :
	  keypad_stick = STICK_LL;
	  break;
	case XK_KP_2 :
	  keypad_stick = STICK_BACK;
	  break;
	case XK_KP_3 :
	  keypad_stick = STICK_LR;
	  break;
	case XK_KP_4 :
	  keypad_stick = STICK_LEFT;
	  break;
	case XK_KP_5 :
	  keypad_stick = STICK_CENTRE;
	  break;
	case XK_KP_6 :
	  keypad_stick = STICK_RIGHT;
	  break;
	case XK_KP_7 :
	  keypad_stick = STICK_UL;
	  break;
	case XK_KP_8 :
	  keypad_stick = STICK_FORWARD;
	  break;
	case XK_KP_9 :
	  keypad_stick = STICK_UR;
	  break;
	default :
	  printf ("Pressed Keysym = %x\n", (int)keysym);
	  break;
	}
    case KeyRelease :
      switch (keysym)
	{
	case XK_Shift_L :
	case XK_Shift_R :
	  SHIFT = 0x00;
	  break;
	case XK_Control_L :
	case XK_Control_R :
	  CONTROL = 0x00;
	  break;
	default :
	  break;
	}
      break;
    }

  return keycode;
}

static int xview_keycode;

#ifdef XVIEW

void event_proc (Xv_Window window, Event *event, Notify_arg arg)
{
  int keycode;

  keycode = GetKeyCode (event->ie_xevent);
  if (keycode != AKEY_NONE)
    xview_keycode = keycode;
}

static int auto_reboot;

int disk_change (char *a, char *full_filename, char *filename)
{
  int diskno;
  int status;

  diskno = 1;

  if (!auto_reboot)
    diskno = notice_prompt (panel, NULL,
			    NOTICE_MESSAGE_STRINGS,
			      "Insert Disk into which drive?",
			      NULL,
			    NOTICE_BUTTON, "1", 1,
			    NOTICE_BUTTON, "2", 2,
			    NOTICE_BUTTON, "3", 3,
			    NOTICE_BUTTON, "4", 4,
			    NOTICE_BUTTON, "5", 5,
			    NOTICE_BUTTON, "6", 6,
			    NOTICE_BUTTON, "7", 7,
			    NOTICE_BUTTON, "8", 8,
			    NULL);

  if ((diskno < 1) || (diskno > 8))
    {
      printf ("Invalid diskno: %d\n", diskno);
      exit (1);
    }

  SIO_Dismount(diskno);
  if (!SIO_Mount (diskno,full_filename))
    status = XV_ERROR;
  else
    {
      if (auto_reboot)
	xview_keycode = AKEY_COLDSTART;
      status = XV_OK;
    }

  return status;
}

boot_callback ()
{
  auto_reboot = TRUE;

  xv_set (disk_change_chooser,
	  XV_SHOW, TRUE,
	  NULL);
}

insert_callback ()
{
  auto_reboot = FALSE;

  xv_set (disk_change_chooser,
	  XV_SHOW, TRUE,
	  NULL);
}

eject_callback ()
{
  int diskno;

  diskno = notice_prompt (panel, NULL,
			  NOTICE_MESSAGE_STRINGS,
			    "Eject Disk from drive?",
			    NULL,
			  NOTICE_BUTTON, "1", 1,
			  NOTICE_BUTTON, "2", 2,
			  NOTICE_BUTTON, "3", 3,
			  NOTICE_BUTTON, "4", 4,
			  NOTICE_BUTTON, "5", 5,
			  NOTICE_BUTTON, "6", 6,
			  NOTICE_BUTTON, "7", 7,
			  NOTICE_BUTTON, "8", 8,
			  NULL);

  if ((diskno < 1) || (diskno > 8))
    {
      printf ("Invalid diskno: %d\n", diskno);
      exit (1);
    }

  SIO_Dismount(diskno);
}

option_callback ()
{
  consol &= 0x03;
}

select_callback ()
{
  consol &= 0x05;
}

start_callback ()
{
  consol &= 0x6;
}

help_callback ()
{
  xview_keycode = AKEY_HELP;
}

break_callback ()
{
  xview_keycode = AKEY_BREAK;
}

reset_callback ()
{
  xview_keycode = AKEY_WARMSTART;
}

coldstart_callback ()
{
  xview_keycode = AKEY_COLDSTART;
}

controllers_callback ()
{
  xv_set (controllers_frame,
	  XV_SHOW, TRUE,
	  NULL);
}

void sorry_message ()
{
    notice_prompt (panel, NULL,
		   NOTICE_MESSAGE_STRINGS,
		     "Sorry, controller already assign",
		     "to another device",
		     NULL,
		   NOTICE_BUTTON, "Cancel", 1,
		   NULL);
  }

keypad_callback ()
{
  int new_mode;

  new_mode = xv_get (keypad_item, PANEL_VALUE);

  if ((new_mode != mouse_mode) &&
      (new_mode != js0_mode) &&
      (new_mode != js1_mode))
    {
      keypad_mode = new_mode;
    }
  else
    {
      sorry_message ();
      xv_set (keypad_item,
	      PANEL_VALUE, keypad_mode,
	      NULL);
    }
}

mouse_callback ()
{
  int new_mode;

  new_mode = xv_get (mouse_item, PANEL_VALUE);

  if ((new_mode != keypad_mode) &&
      (new_mode != js0_mode) &&
      (new_mode != js1_mode))
    {
      mouse_mode = new_mode;
    }
  else
    {
      sorry_message ();
      xv_set (mouse_item,
	      PANEL_VALUE, mouse_mode,
	      NULL);
    }
}

#ifdef LINUX_JOYSTICK
js0_callback ()
{
  int new_mode;

  new_mode = xv_get (js0_item, PANEL_VALUE);

  if ((new_mode != keypad_mode) &&
      (new_mode != mouse_mode) &&
      (new_mode != js1_mode))
    {
      js0_mode = new_mode;
    }
  else
    {
      sorry_message ();
      xv_set (js0_item,
	      PANEL_VALUE, js0_mode,
	      NULL);
    }
}

js1_callback ()
{
  int new_mode;

  new_mode = xv_get (js1_item, PANEL_VALUE);

  if ((new_mode != keypad_mode) &&
      (new_mode != mouse_mode) &&
      (new_mode != js0_mode))
    {
      js1_mode = new_mode;
    }
  else
    {
      sorry_message ();
      xv_set (js1_item,
	      PANEL_VALUE, js1_mode,
	      NULL);
    }
}
#endif

performance_callback ()
{
  xv_set (performance_frame,
	  XV_SHOW, TRUE,
	  NULL);
}

refresh_callback (Panel_item item, int value, Event *event)
{
  refresh_rate = value;
}

countdown_callback (Panel_item item, int value, Event *event)
{
  countdown_rate = value;
}
#endif

void Atari_WhatIs (int mode)
{
  switch (mode)
    {
    case 0 :
      printf ("Joystick 0");
      break;
    case 1 :
      printf ("Joystick 1");
      break;
    case 2 :
      printf ("Joystick 2");
      break;
    case 3 :
      printf ("Joystick 3");
      break;
    default :
      printf ("not available");
      break;
    }
}

void Atari_Initialise (int *argc, char *argv[])
{
  XSetWindowAttributes	xswda;

  XGCValues	xgcvl;

  int colours[256];
  int depth;
  int i, j;
  int mode = 0;

#ifdef XVIEW
  xv_init (XV_INIT_ARGC_PTR_ARGV, argc, argv, NULL);
#endif

  for (i=j=1;i<*argc;i++)
    {
      if (strcmp(argv[i],"-small") == 0)
	windowsize = Small;
      else if (strcmp(argv[i],"-large") == 0)
	windowsize = Large;
      else if (strcmp(argv[i],"-huge") == 0)
	windowsize = Huge;
      else
	argv[j++] = argv[i];
    }

  *argc = j;

  switch (windowsize)
    {
    case Small :
      window_width = ATARI_WIDTH;
      window_height = ATARI_HEIGHT;
      break;
    case Large :
      window_width = ATARI_WIDTH * 2;
      window_height = ATARI_HEIGHT * 2;
      break;
    case Huge :
      window_width = ATARI_WIDTH * 3;
      window_height = ATARI_HEIGHT * 3;
      break;
    }

#ifdef LINUX_JOYSTICK
  js0 = open ("/dev/js0", O_RDONLY, 0777);
  if (js0 != -1)
    {
      int status;

      printf ("/dev/js0 is available\n");
      status = read (js0, &js_data, JS_RETURN);
      if (status != JS_RETURN)
	{
	  perror ("/dev/js0");
	  exit (1);
	}

      js0_centre_x = js_data.x;
      js0_centre_y = js_data.y;

      printf ("\tcentre_x = %d, centry_y = %d\n",
	      js0_centre_x, js0_centre_y);

      js0_mode = mode++;
    }

  js1 = open ("/dev/js1", O_RDONLY, 0777);
  if (js1 != -1)
    {
      int status;

      printf ("/dev/js1 is available\n");
      status = read (js1, &js_data, JS_RETURN);
      if (status != JS_RETURN)
	{
	  perror ("/dev/js1");
	  exit (1);
	}

      js1_centre_x = js_data.x;
      js1_centre_y = js_data.y;

      printf ("\tcentre_x = %d, centry_y = %d\n",
	      js1_centre_x, js1_centre_y);

      js1_mode = mode++;
    }
#endif

  mouse_mode = mode++;
  keypad_mode = mode++;

#ifdef XVIEW
  frame = (Frame)xv_create (NULL, FRAME,
			    FRAME_LABEL, ATARI_TITLE,
			    FRAME_SHOW_RESIZE_CORNER, FALSE,
			    XV_WIDTH, window_width,
			    XV_HEIGHT, window_height + 27,
			    XV_SHOW, TRUE,
			    NULL);

  panel = (Panel)xv_create (frame, PANEL,
			    XV_HEIGHT, 25,
			    XV_SHOW, TRUE,
			    NULL);

  disk_menu = xv_create (NULL, MENU,
			 MENU_ITEM,
			   MENU_STRING, "Boot Disk",
			   MENU_NOTIFY_PROC, boot_callback,
			   NULL,
			 MENU_ITEM,
			   MENU_STRING, "Insert Disk",
			   MENU_NOTIFY_PROC, insert_callback,
			   NULL,
			 MENU_ITEM,
			   MENU_STRING, "Eject Disk",
			   MENU_NOTIFY_PROC, eject_callback,
			   NULL,
			 NULL);

  xv_create (panel, PANEL_BUTTON,
	     PANEL_LABEL_STRING, "Disk",
	     PANEL_ITEM_MENU, disk_menu,
	     NULL);

  consol_menu = (Menu)xv_create (NULL, MENU,
				 MENU_ITEM,
				   MENU_STRING, "Option",
				   MENU_NOTIFY_PROC, option_callback,
				   NULL,
				 MENU_ITEM,
				   MENU_STRING, "Select",
				   MENU_NOTIFY_PROC, select_callback,
				   NULL,
				 MENU_ITEM,
				   MENU_STRING, "Start",
				   MENU_NOTIFY_PROC, start_callback,
				   NULL,
				 MENU_ITEM,
				   MENU_STRING, "Help",
				   MENU_NOTIFY_PROC, help_callback,
				   MENU_INACTIVE, (machine == Atari),
				   NULL,
				 MENU_ITEM,
				   MENU_STRING, "Break",
				   MENU_NOTIFY_PROC, break_callback,
				   NULL,
				 MENU_ITEM,
				   MENU_STRING, "Reset",
				   MENU_NOTIFY_PROC, reset_callback,
				   NULL,
				 MENU_ITEM,
				   MENU_STRING, "Coldstart",
				   MENU_NOTIFY_PROC, coldstart_callback,
				   NULL,
				 NULL);

  xv_create (panel, PANEL_BUTTON,
	     PANEL_LABEL_STRING, "Console",
	     PANEL_ITEM_MENU, consol_menu,
	     NULL);

  options_menu = (Menu)xv_create (NULL, MENU,
				  MENU_ITEM,
				    MENU_STRING, "Controllers",
				    MENU_NOTIFY_PROC, controllers_callback,
				    NULL,
				  MENU_ITEM,
				    MENU_STRING, "Performance",
				    MENU_NOTIFY_PROC, performance_callback,
				    NULL,
				  NULL);

  xv_create (panel, PANEL_BUTTON,
	     PANEL_LABEL_STRING, "Options",
	     PANEL_ITEM_MENU, options_menu,
	     NULL);

  canvas = (Canvas)xv_create (frame, CANVAS,
			      CANVAS_WIDTH, ATARI_WIDTH,
			      CANVAS_HEIGHT, ATARI_HEIGHT,
			      NULL);
/*
   =====================================
   Create Controller Configuration Frame
   =====================================
*/
  controllers_frame = (Frame)xv_create (frame, FRAME_CMD,
				       FRAME_LABEL, "Controller Configuration",
				       XV_WIDTH, 300,
				       XV_HEIGHT, 150,
				       NULL);

  controllers_panel = (Panel)xv_get (controllers_frame, FRAME_CMD_PANEL,
				     NULL);

  keypad_item = (Panel_item)xv_create (controllers_panel, PANEL_CHOICE_STACK,
				       PANEL_LAYOUT, PANEL_HORIZONTAL,
				       PANEL_LABEL_STRING, "Numeric Keypad",
				       PANEL_CHOICE_STRINGS,
				         "Joystick 1",
				         "Joystick 2",
				         "Joystick 3",
				         "Joystick 4",
				         NULL,
				       PANEL_VALUE, keypad_mode,
				       PANEL_NOTIFY_PROC, keypad_callback,
				       NULL);

  mouse_item = (Panel_item)xv_create (controllers_panel, PANEL_CHOICE_STACK,
				      PANEL_LAYOUT, PANEL_HORIZONTAL,
				      PANEL_LABEL_STRING, "Mouse",
				      PANEL_CHOICE_STRINGS,
				        "Joystick 1",
				        "Joystick 2",
				        "Joystick 3",
				        "Joystick 4",
				        "Paddle 1",
				        "Paddle 2",
				        "Paddle 3",
				        "Paddle 4",
				        "Paddle 5",
				        "Paddle 6",
				        "Paddle 7",
				        "Paddle 8",
				        "Light Pen",
				        NULL,
				      PANEL_VALUE, mouse_mode,
				      PANEL_NOTIFY_PROC, mouse_callback,
				      NULL);

#ifdef LINUX_JOYSTICK
  if (js0 != -1)
    {
      js0_item = (Panel_item)xv_create (controllers_panel, PANEL_CHOICE_STACK,
					PANEL_LAYOUT, PANEL_HORIZONTAL,
					PANEL_LABEL_STRING, "/dev/js0",
					PANEL_CHOICE_STRINGS,
					  "Joystick 1",
					  "Joystick 2",
					  "Joystick 3",
					  "Joystick 4",
					NULL,
					PANEL_VALUE, js0_mode,
					PANEL_NOTIFY_PROC, js0_callback,
					NULL);
    }

  if (js1 != -1)
    {
      js1_item = (Panel_item)xv_create (controllers_panel, PANEL_CHOICE_STACK,
					PANEL_LAYOUT, PANEL_HORIZONTAL,
					PANEL_LABEL_STRING, "/dev/js1",
					PANEL_CHOICE_STRINGS,
					  "Joystick 1",
					  "Joystick 2",
					  "Joystick 3",
					  "Joystick 4",
					NULL,
					PANEL_VALUE, js1_mode,
					PANEL_NOTIFY_PROC, js1_callback,
					NULL);
    }
#endif
/*
   ======================================
   Create Performance Configuration Frame
   ======================================
*/
  performance_frame = (Frame)xv_create (frame, FRAME_CMD,
					FRAME_LABEL, "Performance Configuration",
					XV_WIDTH, 500,
					XV_HEIGHT, 75,
					NULL);

  performance_panel = (Panel)xv_get (performance_frame, FRAME_CMD_PANEL,
				     NULL);

  refresh_slider = (Panel_item)xv_create (performance_panel, PANEL_SLIDER,
					  PANEL_LAYOUT, PANEL_HORIZONTAL,
					  PANEL_LABEL_STRING, "Screen Refresh Rate",
					  PANEL_VALUE, refresh_rate,
					  PANEL_MIN_VALUE, 1,
					  PANEL_MAX_VALUE, 32,
					  PANEL_SLIDER_WIDTH, 100,
					  PANEL_TICKS, 32,
					  PANEL_NOTIFY_PROC, refresh_callback,
					  NULL);

  countdown_slider = (Panel_item)xv_create (performance_panel, PANEL_SLIDER,
					    PANEL_LAYOUT, PANEL_HORIZONTAL,
					    PANEL_LABEL_STRING, "Instructions during Vertical Blank",
					    PANEL_VALUE, countdown_rate,
					    PANEL_MIN_VALUE, 1000,
					    PANEL_MAX_VALUE, 10000,
					    PANEL_SLIDER_WIDTH, 100,
					    PANEL_TICKS, 10,
					    PANEL_NOTIFY_PROC, countdown_callback,
					    NULL);
/*
   ====================
   Get X Window Objects
   ====================
*/
  display = (Display*)xv_get(frame, XV_DISPLAY);
  screen = XDefaultScreenOfDisplay (display);
  window = (Window)xv_get(canvas_paint_window(canvas), XV_XID);
  depth = XDefaultDepthOfScreen (screen);
  pixmap = XCreatePixmap (display, window,
			  window_width, window_height, depth);

  disk_change_chooser = (Frame)xv_create (frame, FILE_CHOOSER,
					  FILE_CHOOSER_DIRECTORY, "/usr/local/lib/atari",
					  FILE_CHOOSER_DIRECTORY, "/home/david/a800/disks",
					  FILE_CHOOSER_TYPE, FILE_CHOOSER_OPEN,
					  FILE_CHOOSER_NOTIFY_FUNC, disk_change,
					  NULL);

  xv_set (disk_change_chooser,
	  FRAME_LABEL, "Disk Selector",
	  NULL);

  xv_set (canvas_paint_window(canvas),
	  WIN_EVENT_PROC, event_proc,
	  WIN_CONSUME_EVENTS, WIN_ASCII_EVENTS, WIN_MOUSE_BUTTONS, NULL,
	  NULL);
#endif

#ifndef XVIEW
  display = XOpenDisplay (NULL);
  if (!display)
    {
      printf ("Failed to open display\n");
      exit (1);
    }

  screen = XDefaultScreenOfDisplay (display);
  if (!screen)
    {
      printf ("Unable to get screen\n");
      exit (1);
    }

  depth = XDefaultDepthOfScreen (screen);

  xswda.event_mask = KeyPressMask | KeyReleaseMask | ExposureMask;

  window = XCreateWindow (display,
			  XRootWindowOfScreen(screen),
			  50, 50,
			  window_width, window_height, 3, depth,
			  InputOutput, visual,
			  CWEventMask | CWBackPixel,
			  &xswda);

  pixmap = XCreatePixmap (display, window,
			  window_width, window_height, depth);

  XStoreName (display, window, ATARI_TITLE);
#endif

  for (i=0;i<256;i+=2)
    {
      XColor	colour;

      int	rgb = colortable[i];
      int	status;

      colour.red = (rgb & 0x00ff0000) >> 8;
      colour.green = (rgb & 0x0000ff00);
      colour.blue = (rgb & 0x000000ff) << 8;

      status = XAllocColor (display,
			    XDefaultColormapOfScreen(screen),
			    &colour);

      colours[i] = colour.pixel;
      colours[i+1] = colour.pixel;
    }

  for (i=0;i<256;i++)
    {
      xgcvl.background = colours[0];
      xgcvl.foreground = colours[i];

      gc_colour[i] = XCreateGC (display, window,
				GCForeground | GCBackground,
				&xgcvl);
    }

  xgcvl.background = colours[0];
  xgcvl.foreground = colours[0];

  gc = XCreateGC (display, window,
		  GCForeground | GCBackground,
		  &xgcvl);

  XFillRectangle (display, pixmap, gc, 0, 0,
		  window_width, window_height);

  XMapWindow (display, window);

  XSync (display, False);
/*
   ============================
   Storage for Atari 800 Screen
   ============================
*/
  image_data = (UBYTE*) malloc (ATARI_WIDTH * ATARI_HEIGHT);
  if (!image_data)
    {
      printf ("Failed to allocate space for image\n");
      exit (1);
    }

  consol = 7;

  printf ("Initial X11 controller configuration\n");
  printf ("------------------------------------\n\n");
  printf ("Keypad is "); Atari_WhatIs (keypad_mode); printf ("\n");
  printf ("Mouse is "); Atari_WhatIs (mouse_mode); printf ("\n");
  printf ("/dev/js0 is "); Atari_WhatIs (js0_mode); printf ("\n");
  printf ("/dev/js1 is "); Atari_WhatIs (js1_mode); printf ("\n");
}

int Atari_Exit (int run_monitor)
{
  int restart;

  if (run_monitor)
    restart = monitor();
  else
    restart = FALSE;

  if (!restart)
    {
      free (image_data);

      XSync (display, True);

      XFreePixmap (display, pixmap);
      XUnmapWindow (display, window);
      XDestroyWindow (display, window);
      XCloseDisplay (display);

#ifdef LINUX_JOYSTICK
      if (js0 != -1)
	close (js0);

      if (js1 != -1)
	close (js1);
#endif
    }

  return restart;
}

void Atari_ScanLine_Flush ()
{
  if (windowsize == Small)
    {
      if (npoints != 0)
	{
	  XDrawPoints (display, pixmap, gc_colour[last_colour],
		       points, npoints, CoordModeOrigin);
	  npoints = 0;
	  modified = TRUE;
	}
    }
  else
    {
      if (nrects != 0)
	{
	  XFillRectangles (display, pixmap, gc_colour[last_colour],
			   rectangles, nrects);
	  nrects = 0;
	  modified = TRUE;
	}
    }

  last_colour = -1;
}

void Atari_DisplayScreen (UBYTE *screen)
{
  UBYTE *scanline_ptr;

  int xpos;
  int ypos;

  consol = 7;
  keypad_trig = 1;
  scanline_ptr = image_data;
  modified = FALSE;

  for (ypos=0;ypos<ATARI_HEIGHT;ypos++)
    {
      for (xpos=0;xpos<ATARI_WIDTH;xpos++)
	{
	  UBYTE colour;

	  colour = *screen++;
	  if (colour != *scanline_ptr)
	    {
	      int flush = FALSE;

	      if (windowsize == Small)
		{
		  if (npoints == NPOINTS)
		    flush = TRUE;
		}
	      else
		{
		  if (nrects == NRECTS)
		    flush = TRUE;
		}

	      if (colour != last_colour)
		flush = TRUE;

	      if (flush)
		{
		  Atari_ScanLine_Flush ();
		  last_colour = colour;
		}

	      if (windowsize == Small)
		{
		  points[npoints].x = xpos;
		  points[npoints].y = ypos;
		  npoints++;
		}
	      else if (windowsize == Large)
		{
		  rectangles[nrects].x = xpos << 1;
		  rectangles[nrects].y = ypos << 1;
		  rectangles[nrects].width = 2;
		  rectangles[nrects].height = 2;
		  nrects++;
		}
	      else
		{
		  rectangles[nrects].x = xpos + xpos + xpos;
		  rectangles[nrects].y = ypos + ypos + ypos;
		  rectangles[nrects].width = 3;
		  rectangles[nrects].height = 3;
		  nrects++;
		}

	      *scanline_ptr++ = colour;
	    }
	  else
	    {
	      scanline_ptr++;
	    }
	}
    }

  Atari_ScanLine_Flush ();

  if (modified)
    {
      XCopyArea (display, pixmap, window, gc, 0, 0,
		 window_width, window_height, 0, 0);
    }

#ifdef XVIEW
  notify_dispatch ();
  XFlush (display);
#endif
}

int Atari_Keyboard (void)
{
  int	keycode = AKEY_NONE;

#ifdef XVIEW
  keycode = xview_keycode;
  xview_keycode = AKEY_NONE;
#else
  if (XEventsQueued (display, QueuedAfterFlush) > 0)
    {
      XEvent	event;

      XNextEvent (display, &event);
      keycode = GetKeyCode (&event);
    }
#endif

  return keycode;
}

mouse_joystick (int mode)
{
  Window root_return;
  Window child_return;
  int root_x_return;
  int root_y_return;
  int win_x_return;
  int win_y_return;
  int mask_return;

  mouse_stick = 0x0f;

  XQueryPointer (display, window, &root_return, &child_return,
		 &root_x_return, &root_y_return,
		 &win_x_return, &win_y_return,
		 &mask_return);

  if (mode < 5)
    {
      int center_x;
      int center_y;
      int threshold;

      if (windowsize == Small)
	{
	  center_x = ATARI_WIDTH / 2;
	  center_y = ATARI_HEIGHT / 2;
	  threshold = 32;
	}
      else if (windowsize == Large)
	{
	  center_x = (ATARI_WIDTH * 2) / 2;
	  center_y = (ATARI_HEIGHT * 2) / 2;
	  threshold = 64;
	}
      else
	{
	  center_x = (ATARI_WIDTH * 3) / 2;
	  center_y = (ATARI_HEIGHT * 3) / 2;
	  threshold = 96;
	}

      if (win_x_return < (center_x - threshold))
	mouse_stick &= 0xfb;
      if (win_x_return > (center_x + threshold))
	mouse_stick &= 0xf7;
      if (win_y_return < (center_y - threshold))
	mouse_stick &= 0xfe;
      if (win_y_return > (center_y + threshold))
	mouse_stick &= 0xfd;
    }
  else
    {
      if (mask_return)
	mouse_stick &= 0xfb;
    }
}

#ifdef LINUX_JOYSTICK
void read_joystick (int js, int centre_x, int centre_y)
{
  const int threshold = 50;
  int status;

  mouse_stick = 0x0f;

  status = read (js, &js_data, JS_RETURN);
  if (status != JS_RETURN)
    {
      perror ("/dev/js");
      exit (1);
    }

  if (js_data.x < (centre_x - threshold))
    mouse_stick &= 0xfb;
  if (js_data.x > (centre_x + threshold))
    mouse_stick &= 0xf7;
  if (js_data.y < (centre_y - threshold))
    mouse_stick &= 0xfe;
  if (js_data.y > (centre_y + threshold))
    mouse_stick &= 0xfd;
}
#endif

int Atari_PORT (int num)
{
  int nibble_0 = 0x0f;
  int nibble_1 = 0x0f;

  if (num == 0)
    {
      if (keypad_mode == 0)
	nibble_0 = keypad_stick;
      else if (keypad_mode == 1)
	nibble_1 = keypad_stick;

      if (mouse_mode == 0)
	{
	  mouse_joystick (mouse_mode);
	  nibble_0 = mouse_stick;
	}
      else if (mouse_mode == 1)
	{
	  mouse_joystick (mouse_mode);
	  nibble_1 = mouse_stick;
	}

#ifdef LINUX_JOYSTICK
      if (js0_mode == 0)
	{
	  read_joystick (js0, js0_centre_x, js0_centre_y);
	  nibble_0 = mouse_stick;
	}
      else if (js0_mode == 1)
	{
	  read_joystick (js0, js0_centre_x, js0_centre_y);
	  nibble_1 = mouse_stick;
	}

      if (js1_mode == 0)
	{
	  read_joystick (js1, js1_centre_x, js1_centre_y);
	  nibble_0 = mouse_stick;
	}
      else if (js1_mode == 1)
	{
	  read_joystick (js1, js1_centre_x, js1_centre_y);
	  nibble_1 = mouse_stick;
	}
#endif
    }
  else
    {
      if (keypad_mode == 2)
	nibble_0 = keypad_stick;
      else if (keypad_mode == 3)
	nibble_1 = keypad_stick;

      if (mouse_mode == 2)
	{
	  mouse_joystick (mouse_mode);
	  nibble_0 = mouse_stick;
	}
      else if (mouse_mode == 3)
	{
	  mouse_joystick (mouse_mode);
	  nibble_1 = mouse_stick;
	}

#ifdef LINUX_JOYSTICK
      if (js0_mode == 2)
	{
	  read_joystick (js0, js0_centre_x, js0_centre_y);
	  nibble_0 = mouse_stick;
	}
      else if (js0_mode == 3)
	{
	  read_joystick (js0, js0_centre_x, js0_centre_y);
	  nibble_1 = mouse_stick;
	}

      if (js1_mode == 2)
	{
	  read_joystick (js1, js1_centre_x, js1_centre_y);
	  nibble_0 = mouse_stick;
	}
      else if (js1_mode == 3)
	{
	  read_joystick (js1, js1_centre_x, js1_centre_y);
	  nibble_1 = mouse_stick;
	}
#endif
    }

  return (nibble_1 << 4) | nibble_0;
}

int Atari_TRIG (int num)
{
  int	trig = 1;	/* Trigger not pressed */

  if (num == keypad_mode)
    {
      trig = keypad_trig;
    }

  if (num == mouse_mode)
    {
      Window	root_return;
      Window	child_return;
      int	root_x_return;
      int	root_y_return;
      int	win_x_return;
      int	win_y_return;
      int	mask_return;

      if (XQueryPointer (display, window, &root_return, &child_return,
			 &root_x_return, &root_y_return,
			 &win_x_return, &win_y_return,
			 &mask_return))
	{
	  if (mask_return)
	    trig = 0;
	}
    }

#ifdef LINUX_JOYSTICK
  if (num == js0_mode)
    {
      int status;

      status = read (js0, &js_data, JS_RETURN);
      if (status != JS_RETURN)
	{
	  perror ("/dev/js0");
	  exit (1);
	}

      if (js_data.buttons & 0x01)
	trig = 0;
      else
	trig = 1;

      if (js_data.buttons & 0x02)
	xview_keycode = ' ';
    }

  if (num == js1_mode)
    {
      int status;

      status = read (js1, &js_data, JS_RETURN);
      if (status != JS_RETURN)
	{
	  perror ("/dev/js1");
	  exit (1);
	}

      trig = (js_data.buttons & 0x0f) ? 0 : 1;
    }
#endif

  return trig;
}

int Atari_POT (int num)
{
  int	pot;

  if (num == (mouse_mode - 4))
    {
      Window	root_return;
      Window	child_return;
      int	root_x_return;
      int	root_y_return;
      int	win_x_return;
      int	win_y_return;
      int	mask_return;

      if (XQueryPointer (display, window, &root_return,
			 &child_return, &root_x_return, &root_y_return,
			 &win_x_return, &win_y_return, &mask_return))
	{
	  if (windowsize == Small)
	    {
	      pot = ((float)((ATARI_WIDTH) - win_x_return) /
		     (float)(ATARI_WIDTH)) * 228;
	    }
	  else if (windowsize == Large)
	    {
	      pot = ((float)((ATARI_WIDTH * 2) - win_x_return) /
		     (float)(ATARI_WIDTH * 2)) * 228;
	    }
	  else
	    {
	      pot = ((float)((ATARI_WIDTH * 3) - win_x_return) /
		     (float)(ATARI_WIDTH * 3)) * 228;
	    }
	}
    }
  else
    {
      pot = 228;
    }

  return pot;
}

int Atari_CONSOL (void)
{
  return consol;
}

int Atari_AUDC (int channel, int byte)
{
}

int Atari_AUDF (int channel, int byte)
{
}

int Atari_AUDCTL (int byte)
{
}
