#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/soundcard.h>

#include <vga.h>
#include <vgagl.h>

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

#include "atari.h"
#include "colours.h"

static int	sb = -1;

/*
 * /dev/sequencer only has 49 notes :-( and these notes
 * are in terms of musical notes (A,B,C etc) and not
 * frequency as on the atari. i.e. It is not possible
 * to play all 256 frequencies values (let alone
 * the 16 bit sound modes).
 */

#define	NUM_NOTES	49

static int	values[NUM_NOTES] =
{
  243, 230, 217, 204, 193, 182, 173, 162, 153, 144, 136, 128,
  121, 114, 108, 102, 96, 91, 85, 81, 76, 72, 68, 64,
  60, 57, 53, 50, 47, 45, 42, 40, 37, 35, 33, 31,
  29
};

static int	lookup[256];

#define FALSE 0
#define TRUE 1

static int sound_enabled = FALSE;

static int trig0;
static int stick0;
static int consol;

void Atari_Initialise (int *argc, char *argv[])
{
  int	VGAMODE = G320x200x256;

  struct sbi_instrument	instr;

  int fd;
  char buf[100];
  int note;
  int status;

  int i;
  int j;

  for (i=j=1;i<*argc;i++)
    {
      if (strcmp(argv[i],"-sound") == 0)
	{
	  printf ("SVGALIB sound enabled\n");
	  sound_enabled = TRUE;
	}
      else
	{
	  argv[j++] = argv[i];
	}
    }

  *argc = j;

#ifdef LINUX_JOYSTICK
  js0 = open ("/dev/js0", O_RDONLY, 0777);
  if (js0 != -1)
    {
      int status;

      status = read (js0, &js_data, JS_RETURN);
      if (status != JS_RETURN)
	{
	  perror ("/dev/js0");
	  exit (1);
	}

      js0_centre_x = js_data.x;
      js0_centre_y = js_data.y;
    }

  js1 = open ("/dev/js1", O_RDONLY, 0777);
  if (js1 != -1)
    {
      int status;

      status = read (js1, &js_data, JS_RETURN);
      if (status != JS_RETURN)
	{
	  perror ("/dev/js1");
	  exit (1);
	}

      js1_centre_x = js_data.x;
      js1_centre_y = js_data.y;
    }
#endif

  vga_init ();

  if (!vga_hasmode(VGAMODE))
    {
      printf ("Mode not available\n");
      exit (1);
    }

  vga_setmode (VGAMODE);

  gl_setcontextvga (VGAMODE);

  for (i=0;i<256;i++)
    {
      int	rgb = colortable[i];
      int	red;
      int	green;
      int	blue;

      red = (rgb & 0x00ff0000) >> 18;
      green = (rgb & 0x0000ff00) >> 10;
      blue = (rgb & 0x000000ff) >> 2;

      gl_setpalettecolor (i, red, green, blue);
    }

  if (sound_enabled)
    {
      sb = open ("/dev/sequencer", O_WRONLY, 0);
      if (sb == -1)
	{
	  perror ("/dev/sequencer");
	  exit (1);
	}

      fd = open ("piano1.sbi", O_RDONLY, 0);
      if (fd == -1)
	{
	  perror ("piano1.sbi");
	  exit (1);
	}

      read (fd, buf, 100);
      if (buf[0] != 'S' || buf[1] != 'B' || buf[2] != 'I')
	{
	  printf ("Not SBI file\n");
	  exit (1);
	}

      close (fd);

      instr.channel = 129;
      for (i=0;i<16;i++)
	instr.operators[i] = buf[i+0x24];

      status = ioctl (sb, SNDCTL_FM_LOAD_INSTR, &instr);
      if (status == -1)
	{
	  perror ("ioctl");
	  exit (1);
	}

      buf[0] = SEQ_FMPGMCHANGE;
      buf[1] = 0;
      buf[2] = 129;
      write (sb, buf, 4);

      buf[0] = SEQ_FMPGMCHANGE;
      buf[1] = 1;
      buf[2] = 129;
      write (sb, buf, 4);

      buf[0] = SEQ_FMPGMCHANGE;
      buf[1] = 2;
      buf[2] = 129;
      write (sb, buf, 4);

      buf[0] = SEQ_FMPGMCHANGE;
      buf[1] = 3;
      buf[2] = 129;
      write (sb, buf, 4);

      for (i=0;i<256;i++)
	lookup[i] = -1;

      for (i=0;i<NUM_NOTES;i++)
	{
	  int octave = i / 12;
	  int note = i - (octave * 12);
      
	  lookup[values[i]] = (octave + 2) * 12 + note;
	}

      note = -1;
      for (i=0;i<256;i++)
	{
	  if (lookup[i] != -1)
	    note = lookup[i];
	  else
	    lookup[i] = note;
	}
    }

  trig0 = 1;
  stick0 = 15;
  consol = 7;
}

int Atari_Exit (int run_monitor)
{
  int restart;

  vga_setmode (TEXT);

  if (sound_enabled && (sb != -1))
    close (sb);

  if (run_monitor)
    restart = monitor ();
  else
    restart = FALSE;

  if (restart)
    {
      int VGAMODE = G320x200x256;
      int status;
      int i;

      if (!vga_hasmode(VGAMODE))
	{
	  printf ("Mode not available\n");
	  exit (1);
	}

      vga_setmode (VGAMODE);

      gl_setcontextvga (VGAMODE);

      for (i=0;i<256;i++)
	{
	  int	rgb = colortable[i];
	  int	red;
	  int	green;
	  int	blue;

	  red = (rgb & 0x00ff0000) >> 18;
	  green = (rgb & 0x0000ff00) >> 10;
	  blue = (rgb & 0x000000ff) >> 2;

	  gl_setpalettecolor (i, red, green, blue);
	}
    }
#ifdef LINUX_JOYSTICK
  else
    {
      if (js0 != -1)
	close (js0);

      if (js1 != -1)
	close (js1);
    }
#endif

  return restart;
}

void Atari_DisplayScreen (UBYTE *screen)
{
  UBYTE *svga_ptr = graph_mem;
  UBYTE *scrn_ptr = &screen[24*ATARI_WIDTH+32];
  int ypos;

  consol = 7;

  for (ypos=0;ypos<192;ypos++)
    {
      memcpy (svga_ptr, scrn_ptr, 320);
      svga_ptr += 320;
      scrn_ptr += ATARI_WIDTH;
    }
}

static int special_keycode = -1;

int Atari_Keyboard (void)
{
  int	keycode;

  trig0 = 1;

  if (special_keycode != -1)
    {
      keycode = special_keycode;
      special_keycode = -1;
    }
  else
    keycode = vga_getkey();

  switch (keycode)
    {
    case 0x01 :
      keycode = AKEY_CTRL_A;
      break;
    case 0x02 :
      keycode = AKEY_CTRL_B;
      break;
/*
   case 0x03 :
   keycode = AKEY_CTRL_C;
   break;
*/
    case 0x04 :
      keycode = AKEY_CTRL_D;
      break;
    case 0x05 :
      keycode = AKEY_CTRL_E;
      break;
    case 0x06 :
      keycode = AKEY_CTRL_F;
      break;
    case 0x07 :
      keycode = AKEY_CTRL_G;
      break;
    case 0x08 :
      keycode = AKEY_CTRL_H;
      break;
    case 0x09 :
      keycode = AKEY_CTRL_I;
      break;
/*
   case 0x0a :
   keycode = AKEY_CTRL_J;
   break;
*/
    case 0x0b :
      keycode = AKEY_CTRL_K;
      break;
    case 0x0c :
      keycode = AKEY_CTRL_L;
      break;
/*
   case 0x0d :
   keycode = AKEY_CTRL_M;
   break;
*/
    case 0x0e :
      keycode = AKEY_CTRL_N;
      break;
    case 0x0f :
      keycode = AKEY_CTRL_O;
      break;
    case 0x10 :
      keycode = AKEY_CTRL_P;
      break;
    case 0x11 :
      keycode = AKEY_CTRL_Q;
      break;
    case 0x12 :
      keycode = AKEY_CTRL_R;
      break;
    case 0x13 :
      keycode = AKEY_CTRL_S;
      break;
    case 0x14 :
      keycode = AKEY_CTRL_T;
      break;
    case 0x15 :
      keycode = AKEY_CTRL_U;
      break;
    case 0x16 :
      keycode = AKEY_CTRL_V;
      break;
    case 0x17 :
      keycode = AKEY_CTRL_W;
      break;
    case 0x18 :
      keycode = AKEY_CTRL_X;
      break;
    case 0x19 :
      keycode = AKEY_CTRL_Y;
      break;
    case 0x1a :
      keycode = AKEY_CTRL_Z;
      break;
    case '`' :
      keycode = AKEY_CAPSTOGGLE;
      break;
    case '!' :
    case '"' :
    case '#' :
    case '$' :
    case '%' :
    case '&' :
    case '\'' :
    case '@' :
    case '(' :
    case ')' :
    case '<' :
    case '>' :
    case '=' :
    case '?' :
    case '-' :
    case '+' :
    case '*' :
    case '/' :
    case ':' :
    case ';' :
    case ',' :
    case '.' :
    case '_' :
    case '{' :
    case '}' :
    case '^' :
    case '\\' :
    case '|' :
    case '0' : case '1' : case '2' : case '3' : case '4' :
    case '5' : case '6' : case '7' : case '8' : case '9' :
    case ' ' :
    case 'a' : case 'A' :
    case 'b' : case 'B' :
    case 'c' : case 'C' :
    case 'd' : case 'D' :
    case 'e' : case 'E' :
    case 'f' : case 'F' :
    case 'g' : case 'G' :
    case 'h' : case 'H' :
    case 'i' : case 'I' :
    case 'j' : case 'J' :
    case 'k' : case 'K' :
    case 'l' : case 'L' :
    case 'm' : case 'M' :
    case 'n' : case 'N' :
    case 'o' : case 'O' :
    case 'p' : case 'P' :
    case 'q' : case 'Q' :
    case 'r' : case 'R' :
    case 's' : case 'S' :
    case 't' : case 'T' :
    case 'u' : case 'U' :
    case 'v' : case 'V' :
    case 'w' : case 'W' :
    case 'x' : case 'X' :
    case 'y' : case 'Y' :
    case 'z' : case 'Z' :
      break;
    case 0x7f :	/* Backspace */
      keycode = AKEY_BACKSPACE;
      break;
    case '\n' :
      keycode = AKEY_RETURN;
      break;
    case 0x1b :
      {
	char	buff[10];
	int	nc;
	
	nc = 0;
	while (((keycode = vga_getkey()) != 0) && (nc < 8))
	  buff[nc++] = keycode;
	buff[nc++] = '\0';

	if (strcmp(buff, "\133\133\101") == 0)		/* F1 */
	  {
	    keycode = AKEY_WARMSTART;
	  }
	else if (strcmp(buff, "\133\133\102") == 0)	/* F2 */
	  {
	    consol &= 0x03;
	    keycode = AKEY_NONE;
	  }
	else if (strcmp(buff, "\133\133\103") == 0)	/* F3 */
	  {
	    consol &= 0x05;
	    keycode = AKEY_NONE;
	  }
	else if (strcmp(buff, "\133\133\104") == 0)	/* F4 */
	  {
	    consol &= 0x06;
	    keycode = AKEY_NONE;
	  }
	else if (strcmp(buff, "\133\133\105") == 0)	/* F5 */
	  {
	    keycode = AKEY_COLDSTART;
	  }
	else if (strcmp(buff, "\133\061\067\176") == 0)	/* F6 */
	  {
	    keycode = AKEY_PIL;
	  }
	else if (strcmp(buff, "\133\061\070\176") == 0)	/* F7 */
	  {
	    keycode = AKEY_BREAK;
	  }
	else if (strcmp(buff, "\133\061\071\176") == 0)	/* F8 */
	  {
	    keycode = AKEY_NONE;
	  }
	else if (strcmp(buff, "\133\062\060\176") == 0)	/* F9 */
	  {
	    keycode = AKEY_EXIT;
	  }
	else if (strcmp(buff, "\133\062\061\176") == 0)	/* F10 */
	  {
	    keycode = AKEY_NONE;
	  }
	else if (strcmp(buff, "\133\062\063\176") == 0)	/* F11 */
	  {
	    keycode = AKEY_NONE;
	  }
	else if (strcmp(buff, "\133\062\064\176") == 0)	/* F12 */
	  {
	    keycode = AKEY_NONE;
	  }
	else if (strcmp(buff, "\141\000") == 0)
	  keycode = AKEY_SHFTCTRL_A;
	else if (strcmp(buff, "\142\000") == 0)
	  keycode = AKEY_SHFTCTRL_B;
	else if (strcmp(buff, "\143\000") == 0)
	  keycode = AKEY_SHFTCTRL_C;
	else if (strcmp(buff, "\144\000") == 0)
	  keycode = AKEY_SHFTCTRL_D;
	else if (strcmp(buff, "\145\000") == 0)
	  keycode = AKEY_SHFTCTRL_E;
	else if (strcmp(buff, "\146\000") == 0)
	  keycode = AKEY_SHFTCTRL_F;
	else if (strcmp(buff, "\147\000") == 0)
	  keycode = AKEY_SHFTCTRL_G;
	else if (strcmp(buff, "\150\000") == 0)
	  keycode = AKEY_SHFTCTRL_H;
	else if (strcmp(buff, "\151\000") == 0)
	  keycode = AKEY_SHFTCTRL_I;
	else if (strcmp(buff, "\152\000") == 0)
	  keycode = AKEY_SHFTCTRL_J;
	else if (strcmp(buff, "\153\000") == 0)
	  keycode = AKEY_SHFTCTRL_K;
	else if (strcmp(buff, "\154\000") == 0)
	  keycode = AKEY_SHFTCTRL_L;
	else if (strcmp(buff, "\155\000") == 0)
	  keycode = AKEY_SHFTCTRL_M;
	else if (strcmp(buff, "\156\000") == 0)
	  keycode = AKEY_SHFTCTRL_N;
	else if (strcmp(buff, "\157\000") == 0)
	  keycode = AKEY_SHFTCTRL_O;
	else if (strcmp(buff, "\160\000") == 0)
	  keycode = AKEY_SHFTCTRL_P;
	else if (strcmp(buff, "\161\000") == 0)
	  keycode = AKEY_SHFTCTRL_Q;
	else if (strcmp(buff, "\162\000") == 0)
	  keycode = AKEY_SHFTCTRL_R;
	else if (strcmp(buff, "\163\000") == 0)
	  keycode = AKEY_SHFTCTRL_S;
	else if (strcmp(buff, "\164\000") == 0)
	  keycode = AKEY_SHFTCTRL_T;
	else if (strcmp(buff, "\165\000") == 0)
	  keycode = AKEY_SHFTCTRL_U;
	else if (strcmp(buff, "\166\000") == 0)
	  keycode = AKEY_SHFTCTRL_V;
	else if (strcmp(buff, "\167\000") == 0)
	  keycode = AKEY_SHFTCTRL_W;
	else if (strcmp(buff, "\170\000") == 0)
	  keycode = AKEY_SHFTCTRL_X;
	else if (strcmp(buff, "\171\000") == 0)
	  keycode = AKEY_SHFTCTRL_Y;
	else if (strcmp(buff, "\172\000") == 0)
	  keycode = AKEY_SHFTCTRL_Z;
	else if (strcmp(buff, "\133\062\176") == 0)	/* Keypad 0 */
	  {
	    trig0 = 0;
	    keycode = AKEY_NONE;
	  }
	else if (strcmp(buff, "\133\064\176") == 0)	/* Keypad 1 */
	  {
	    stick0 = STICK_LL;
	    keycode = AKEY_NONE;
	  }
	else if (strcmp(buff, "\133\102") == 0)		/* Keypad 2 */
	  {
	    stick0 = STICK_BACK;
	    keycode = AKEY_NONE;
	  }
	else if (strcmp(buff, "\133\066\176") == 0)	/* Keypad 3 */
	  {
	    stick0 = STICK_LR;
	    keycode = AKEY_NONE;
	  }
	else if (strcmp(buff, "\133\104") == 0)		/* Keypad 4 */
	  {
	    stick0 = STICK_LEFT;
	    keycode = AKEY_NONE;
	  }
	else if (strcmp(buff, "\133\107") == 0)		/* Keypad 5 */
	  {
	    stick0 = STICK_CENTRE;
	    keycode = AKEY_NONE;
	  }
	else if (strcmp(buff, "\133\103") == 0)		/* Keypad 6 */
	  {
	    stick0 = STICK_RIGHT;
	    keycode = AKEY_NONE;
	  }
	else if (strcmp(buff, "\133\061\176") == 0)	/* Keypad 7 */
	  {
	    stick0 = STICK_UL;
	    keycode = AKEY_NONE;
	  }
	else if (strcmp(buff, "\133\101") == 0)		/* Keypad 8 */
	  {
	    stick0 = STICK_FORWARD;
	    keycode = AKEY_NONE;
	  }
	else if (strcmp(buff, "\133\065\176") == 0)	/* Keypad 9 */
	  {
	    stick0 = STICK_UR;
	    keycode = AKEY_NONE;
	  }
	else
	  {
	    int	i;
	    printf ("Unknown key: 0x1b ");
	    for (i=0;i<nc;i++) printf ("0x%02x ", buff[i]);
	    printf ("\n");
	    keycode = AKEY_NONE;
	  }
      }
      break;
    default :
      printf ("Unknown Keycode: %d\n", keycode);
    case 0 :
      keycode = AKEY_NONE;
      break;
    }
  
  return keycode;
}

#ifdef LINUX_JOYSTICK
void read_joystick (int js, int centre_x, int centre_y)
{
  const int threshold = 50;
  int status;

  stick0 = 0x0f;

  status = read (js, &js_data, JS_RETURN);
  if (status != JS_RETURN)
    {
      perror ("/dev/js");
      exit (1);
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

int Atari_PORT (int num)
{
  if (num == 0)
    {
#ifdef LINUX_JOYSTICK
      read_joystick (js0, js0_centre_x, js0_centre_y);
#endif
      return 0xf0 | stick0;
    }
  else
    return 0xff;
}

int Atari_TRIG (int num)
{
  if (num == 0)
    {
#ifdef LINUX_JOYSTICK
      int status;

      status = read (js0, &js_data, JS_RETURN);
      if (status != JS_RETURN)
	{
	  perror ("/dev/js0");
	  exit (1);
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

int Atari_POT (int num)
{
  return 228;
}

int Atari_CONSOL (void)
{
  return consol;
}

void Atari_AUDC (int channel, int byte)
{
}

void Atari_AUDF (int channel, int byte)
{
  if (sound_enabled)
    {
      static char buf[4];

      int pitch;
      int volume = 64;

/*
   ========================================
   Don't ask me why, but sound doesn't work
   if the following printf is missing!
   ========================================
*/
      printf ("\n");

      pitch = lookup[byte];
      if (pitch != -1)
	{
	  buf[0] = SEQ_FMNOTEON;
	  buf[1] = channel - 1;
	  buf[2] = pitch;
	  buf[3] = volume;

	  write (sb, buf, 4);
	}
    }
}

void Atari_AUDCTL (int byte)
{
}

