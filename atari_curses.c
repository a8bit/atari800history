#ifdef NCURSES
#include	<ncurses.h>
#else
#include	<curses.h>
#endif

#include	"cpu.h"
#include	"atari.h"

#define CURSES_LEFT 0
#define CURSES_CENTRAL 1
#define CURSES_RIGHT 2
#define CURSES_WIDE_1 3
#define CURSES_WIDE_2 4

static int curses_mode;
static int consol;

void Atari_Initialise (int *argc, char *argv[])
{
  int i;
  int j;

  for (i=j=1;i<*argc;i++)
    {
      if (strcmp(argv[i],"-left") == 0)
	curses_mode = CURSES_LEFT;
      else if (strcmp(argv[i],"-central") == 0)
	curses_mode = CURSES_CENTRAL;
      else if (strcmp(argv[i],"-right") == 0)
	curses_mode = CURSES_RIGHT;
      else if (strcmp(argv[i],"-wide1") == 0)
	curses_mode = CURSES_WIDE_1;
      else if (strcmp(argv[i],"-wide2") == 0)
	curses_mode = CURSES_WIDE_2;
      else
	argv[j++] = argv[i];
    }

  *argc = j;

  initscr ();
  noecho ();
  cbreak ();		/* Don't wait for carriage return */
  keypad (stdscr, TRUE);
  curs_set (0);		/* Disable Cursor */
  nodelay (stdscr, 1);	/* Don't block for keypress */
  
  consol = 7;
}

int Atari_Exit (int run_monitor)
{
  curs_set (1);
  endwin ();

  if (run_monitor)
    monitor ();

  return 0;
}

void Atari_DisplayScreen ()
{
  UWORD	screenaddr;

  int	xpos;
  int	ypos;

  consol = 7;

  screenaddr = (GetByte (89) << 8) | GetByte (88);

  for (ypos=0;ypos<24;ypos++)
    {
      for (xpos=0;xpos<40;xpos++)
	{
	  char	ch;

	  ch = GetByte (screenaddr);

	  switch (ch & 0xe0)
	    {
	    case 0x00 :	/* Numbers + !"$% etc. */
	      ch = 0x20 + ch;
	      attroff (A_REVERSE);
	      attroff (A_BOLD);
	      break;
	    case 0x20 :	/* Upper Case Characters */
	      ch = 0x40 + (ch - 0x20);
	      attroff (A_REVERSE);
	      attroff (A_BOLD);
	      break;
	    case 0x40 :	/* Control Characters */
	      attroff (A_REVERSE);
	      attron (A_BOLD);
	      break;
	    case 0x60 :	/* Lower Case Characters */
	      attroff (A_REVERSE);
	      attroff (A_BOLD);
	      break;
	    case 0x80 :	/* Number, !"$% etc. */
	      ch = 0x20 + (ch & 0x7f);
	      attron (A_REVERSE);
	      attroff (A_BOLD);
	      break;
	    case 0xa0 :	/* Upper Case Characters */
	      ch = 0x40 + ((ch & 0x7f) - 0x20);
	      attron (A_REVERSE);
	      attroff (A_BOLD);
	      break;
	    case 0xc0 :	/* Control Characters */
	      ch = ch & 0x7f;
	      attron (A_REVERSE);
	      attron (A_BOLD);
	      break;
	    case 0xe0 :	/* Lower Case Characters */
	      ch = ch & 0x7f;
	      attron (A_REVERSE);
	      attroff (A_BOLD);
	      break;
	    }

	  switch (curses_mode & 0x0f)
	    {
	    default :
	    case CURSES_LEFT :
	      move (ypos, xpos);
	    break;
	  case CURSES_CENTRAL :
	    move (ypos, 20+xpos);
	    break;
	  case CURSES_RIGHT :
	    move (ypos, 40+xpos);
	    break;
	  case CURSES_WIDE_1 :
	    move (ypos, xpos+xpos);
	    break;
	  case CURSES_WIDE_2 :
	    move (ypos, xpos+xpos);
	    addch (ch);
	    ch = (ch & 0x80) | 0x20;
	    break;
	  }

	  addch (ch);

	  screenaddr++;
	}
    }

  refresh ();
}

int Atari_Keyboard (void)
{
  int	keycode;

  keycode = getch ();

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
#ifndef SOLARIS2
		case 0x08 :
			keycode = AKEY_CTRL_H;
			break;
#endif
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
    case '~' :
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
    case ' ' :
    case '0' : case '1' : case '2' : case '3' : case '4' :
    case '5' : case '6' : case '7' : case '8' : case '9' :
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
    case 0x1b :
      keycode = AKEY_ESCAPE;
      break;
    case KEY_F0 + 1 :
      keycode = AKEY_WARMSTART;
      break;
    case KEY_F0 + 2 :
      consol &= 0x03;
      keycode = AKEY_NONE;
      break;
    case KEY_F0 + 3 :
      consol &= 0x05;
      keycode = AKEY_NONE;
      break;
    case KEY_F0 + 4 :
      consol &= 0x06;
      keycode = AKEY_NONE;
      break;
    case KEY_F0 + 5 :
      keycode = AKEY_COLDSTART;
      break;
    case KEY_F0 + 6 :
      keycode = AKEY_PIL;
      break;
    case KEY_F0 + 7 :
      keycode = AKEY_BREAK;
      break;
    case KEY_F0 + 8 :
      keycode = AKEY_NONE;
      break;
    case KEY_F0 + 9 :
      keycode = AKEY_EXIT;
      break;
    case KEY_DOWN :
      keycode = AKEY_DOWN;
      break;
    case KEY_LEFT :
      keycode = AKEY_LEFT;
      break;
    case KEY_RIGHT :
      keycode = AKEY_RIGHT;
      break;
    case KEY_UP :
      keycode = AKEY_UP;
      break;
#ifdef SOLARIS2
		case 8 :
		case 127 :
#else
		case KEY_BACKSPACE :
#endif
		  keycode = AKEY_BACKSPACE;
      break;
    case '\n' :
      keycode = AKEY_RETURN;
      break;
    default :
      keycode = AKEY_NONE;
      break;
    }

  return keycode;
}

int Atari_PORT (int num)
{
  return 0xff;
}

int Atari_TRIG (int num)
{
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
}

void Atari_AUDCTL (int byte)
{
}
