/* --------------------------------------------------------------------------*/

/*
 * DJGPP - VGA Backend for David Firth's Atari 800 emulator
 *
 * by Ivo van Poorten (C)1996  See file COPYRIGHT for policy.
 *
 */

/* --------------------------------------------------------------------------*/

#include <go32.h>
#include <sys/farptr.h>
#include <dos.h>
#include "cpu.h"
#include "colours.h"

/* --------------------------------------------------------------------------*/

static int consol;
static int trig0;
static int stick0;

static int first_lno = 24;
static int ypos_inc = 1;
static int vga_ptr_inc = 320;
static int scr_ptr_inc = ATARI_WIDTH;

/* --------------------------------------------------------------------------*/

void Atari_Initialise(int *argc, char *argv[])
{
  int a,r,g,b;
  union REGS rg;

  int i;
  int j;

  for (i=j=1;i<*argc;i++)
    {
      if (strcmp(argv[i],"-interlace") == 0)
        {
          ypos_inc = 2;
          vga_ptr_inc = 320 + 320;
          scr_ptr_inc = ATARI_WIDTH + ATARI_WIDTH;
        }
      else
        {
          if (strcmp(argv[i],"-help") == 0)
            {
              printf ("\t-interlace    Generate screen with interlace\n");
            }

          argv[j++] = argv[i];
        }
    }

  *argc = j;

  /* setupSB(); */

  rg.x.ax = 0x0013;
  int86(0x10, &rg, &rg);

  for(a=0; a<256; a++) {
    r = ( colortable[a] >> 18 ) & 0x3f;
    g = ( colortable[a] >> 10 ) & 0x3f;
    b = ( colortable[a] >>  2 ) & 0x3f;
    rg.x.ax = 0x1010;
    rg.x.bx = a;
    rg.h.dh = r;
    rg.h.ch = g;
    rg.h.cl = b;
    int86(0x10, &rg, &rg);
  }

  consol = 7;
}

/* --------------------------------------------------------------------------*/

int Atari_Exit(int run_monitor)
{
  union REGS rg;

  /* restore to text mode */

  rg.x.ax = 0x0003;
  int86 (0x10, &rg, &rg);

  /* cleanupSB(); */

  return 0;
}

/* --------------------------------------------------------------------------*/

void Atari_DisplayScreen(UBYTE *screen)
{
  static int lace = 0;

  UBYTE *vga_ptr;
  UBYTE *scr_ptr;
  int ypos;
  int x;

  vga_ptr = (UBYTE *) 0xa0000;
  scr_ptr = &screen[first_lno*ATARI_WIDTH+32];

  if (ypos_inc == 2)
    {
      if (lace)
        {
          vga_ptr += 320;
          scr_ptr += ATARI_WIDTH;
        }

      lace = 1 - lace;
    }

  _farsetsel(_dos_ds);
  for (ypos=0;ypos<200;ypos+=ypos_inc)
    {
      for(x=0; x<320; x+=4) {
        _farnspokel ( (int) (vga_ptr + x),  *(long*)(scr_ptr+x) );
      }
      vga_ptr += vga_ptr_inc;
      scr_ptr += scr_ptr_inc;
    }

  consol = 7;
}

/* --------------------------------------------------------------------------*/

int Atari_Keyboard(void)
{
  int   keycode,scancode;
  union REGS rg;

  trig0 = 1;
/*  stick0 = STICK_CENTRE */

  consol = 7;

  rg.h.ah = 0x01;
  int86(0x16, &rg, &rg);

  if(rg.h.flags & 0x40)
        return AKEY_NONE;

  rg.h.ah = 0x00;
  int86(0x16, &rg, &rg);
  keycode = rg.h.al;
  scancode = rg.h.ah;

/* Control-C can be typed as ALT-C */
 
  switch (keycode)
    {
    case 0x01 :
      keycode = AKEY_CTRL_a;
      break;
    case 0x02 :
      keycode = AKEY_CTRL_b;
      break;
/*
   case 0x03 :
   keycode = AKEY_CTRL_c;
   break;
*/
    case 0x04 :
      keycode = AKEY_CTRL_d;
      break;
    case 0x05 :
      keycode = AKEY_CTRL_e;
      break;
    case 0x06 :
      keycode = AKEY_CTRL_f;
      break;
    case 0x07 :
      keycode = AKEY_CTRL_g;
      break;
    case 0x08 :
      if (scancode == 0x0e)
        keycode = AKEY_BACKSPACE;
      else
        keycode = AKEY_CTRL_h;
      break;
    case 0x09 :
      keycode = AKEY_CTRL_i;
      break;
     case 0x0a :
       keycode = AKEY_CTRL_j;
       break;
    case 0x0b :
      keycode = AKEY_CTRL_k;
      break;
    case 0x0c :
      keycode = AKEY_CTRL_l;
      break;
    case 0x0d :
      if (scancode == 0x1c)
        keycode = AKEY_RETURN;
      else
        keycode = AKEY_CTRL_m;
      break;
    case 0x0e :
      keycode = AKEY_CTRL_n;
      break;
    case 0x0f :
      keycode = AKEY_CTRL_o;
      break;
    case 0x10 :
      keycode = AKEY_CTRL_p;
      break;
    case 0x11 :
      keycode = AKEY_CTRL_q;
      break;
    case 0x12 :
      keycode = AKEY_CTRL_r;
      break;
    case 0x13 :
      keycode = AKEY_CTRL_s;
      break;
    case 0x14 :
      keycode = AKEY_CTRL_t;
      break;
    case 0x15 :
      keycode = AKEY_CTRL_u;
      break;
    case 0x16 :
      keycode = AKEY_CTRL_v;
      break;
    case 0x17 :
      keycode = AKEY_CTRL_w;
      break;
    case 0x18 :
      keycode = AKEY_CTRL_x;
      break;
    case 0x19 :
      keycode = AKEY_CTRL_y;
      break;
    case 0x1a :
      keycode = AKEY_CTRL_z;
      break;
    case ' ' :
      keycode = AKEY_SPACE;
      break;
    case '`' :
      keycode = AKEY_CAPSTOGGLE;
      break;
    case '!' :
      keycode = AKEY_EXCLAMATION;
      break;
    case '"' :
      keycode = AKEY_DBLQUOTE;
      break;
    case '#' :
      keycode = AKEY_HASH;
      break;
    case '$' :
      keycode = AKEY_DOLLAR;
      break;
    case '%' :
      keycode = AKEY_PERCENT;
      break;
    case '&' :
      keycode = AKEY_AMPERSAND;
      break;
    case '\'' :
      keycode = AKEY_QUOTE;
      break;
    case '@' :
      keycode = AKEY_AT;
      break;
    case '(' :
      keycode = AKEY_PARENLEFT;
      break;
    case ')' :
      keycode = AKEY_PARENRIGHT;
      break;
    case '[' :
      keycode = AKEY_BRACKETLEFT;
      break;
    case ']' :
      keycode = AKEY_BRACKETRIGHT;
      break;
    case '<' :
      keycode = AKEY_LESS;
      break;
    case '>' :
      keycode = AKEY_GREATER;
      break;
    case '=' :
      keycode = AKEY_EQUAL;
      break;
    case '?' :
      keycode = AKEY_QUESTION;
      break;
    case '-' :
      keycode = AKEY_MINUS;
      break;
    case '+' :
      keycode = AKEY_PLUS;
      break;
    case '*' :
      keycode = AKEY_ASTERISK;
      break;
    case '/' :
      keycode = AKEY_SLASH;
      break;
    case ':' :
      keycode = AKEY_COLON;
      break;
    case ';' :
      keycode = AKEY_SEMICOLON;
      break;
    case ',' :
      keycode = AKEY_COMMA;
      break;
    case '.' :
      keycode = AKEY_FULLSTOP;
      break;
    case '_' :
      keycode = AKEY_UNDERSCORE;
      break;
    case '^' :
      keycode = AKEY_CIRCUMFLEX;
      break;
    case '\\' :
      keycode = AKEY_BACKSLASH;
      break;
    case '|' :
      keycode = AKEY_BAR;
      break;
    case '0' :
      keycode = AKEY_0;
      if(scancode==0x52)        /* keypad 0 == fire button */
	{
	  keycode = AKEY_NONE;
	  trig0=0;
	}
      break;
    case '1' :
      keycode = AKEY_1;
      if(scancode==0x4f)        /* keypad 1 */
	{
	  stick0=STICK_LL;
	  keycode = AKEY_NONE;
	}
      break;
    case '2' :
      keycode = AKEY_2;
      if(scancode==0x50)        /* keypad 2 */
	{
	  stick0=STICK_BACK;
	  keycode = AKEY_NONE;
	}
      break;
    case '3' :
      keycode = AKEY_3;
      if(scancode==0x51)        /* keypad 3 */
	{
	  stick0=STICK_LR;
	  keycode = AKEY_NONE;
	}
      break;
    case '4' :
      keycode = AKEY_4;
      if(scancode==0x4b)        /* keypad 4 */
	{
	  stick0=STICK_LEFT;
	  keycode = AKEY_NONE;
	}
      break;
    case '5' :
      keycode = AKEY_5;        
      if(scancode==0x4c)        /* keypad 5 */
	{
	  stick0=STICK_CENTRE;
	  keycode = AKEY_NONE;
	}
      break;
    case '6' :
      keycode = AKEY_6;
      if(scancode==0x4d)        /* keypad 6 */
	{
	  stick0=STICK_RIGHT;
	  keycode = AKEY_NONE;
	}
      break;
    case '7' :
      keycode = AKEY_7;
      if(scancode==0x47)        /* keypad 7 */
	{
	  stick0=STICK_UL;
	  keycode = AKEY_NONE;
	}
      break;
    case '8' :
      keycode = AKEY_8;
      if(scancode==0x48)        /* keypad 8 */
	{
	  stick0=STICK_FORWARD;
	  keycode = AKEY_NONE;
	}
      break;
    case '9' :
      keycode = AKEY_9;
      if(scancode==0x49)        /* keypad 9 */
	{
	  stick0=STICK_UR;
	  keycode = AKEY_NONE;
	}
      break;
    case 'a' :
      keycode = AKEY_a;
      break;
    case 'b' :
      keycode = AKEY_b;
      break;
    case 'c' :
      keycode = AKEY_c;
      break;
    case 'd' :
      keycode = AKEY_d;
      break;
    case 'e' :
      keycode = AKEY_e;
      break;
    case 'f' :
      keycode = AKEY_f;
      break;
    case 'g' :
      keycode = AKEY_g;
      break;
    case 'h' :
      keycode = AKEY_h;
      break;
    case 'i' :
      keycode = AKEY_i;
      break;
    case 'j' :
      keycode = AKEY_j;
      break;
    case 'k' :
      keycode = AKEY_k;
      break;
    case 'l' :
      keycode = AKEY_l;
      break;
    case 'm' :
      keycode = AKEY_m;
      break;
    case 'n' :
      keycode = AKEY_n;
      break;
    case 'o' :
      keycode = AKEY_o;
      break;
    case 'p' :
      keycode = AKEY_p;
      break;
    case 'q' :
      keycode = AKEY_q;
      break;
    case 'r' :
      keycode = AKEY_r;
      break;
    case 's' :
      keycode = AKEY_s;
      break;
    case 't' :
      keycode = AKEY_t;
      break;
    case 'u' :
      keycode = AKEY_u;
      break;
    case 'v' :
      keycode = AKEY_v;
      break;
    case 'w' :
      keycode = AKEY_w;
      break;
    case 'x' :
      keycode = AKEY_x;
      break;
    case 'y' :
      keycode = AKEY_y;
      break;
    case 'z' :
      keycode = AKEY_z;
      break;
    case 'A' :
      keycode = AKEY_A;
      break;
    case 'B' :
      keycode = AKEY_B;
      break;
    case 'C' :
      keycode = AKEY_C;
      break;
    case 'D' :
      keycode = AKEY_D;
      break;
    case 'E' :
      keycode = AKEY_E;
      break;
    case 'F' :
      keycode = AKEY_F;
      break;
    case 'G' :
      keycode = AKEY_G;
      break;
    case 'H' :
      keycode = AKEY_H;
      break;
    case 'I' :
      keycode = AKEY_I;
      break;
    case 'J' :
      keycode = AKEY_J;
      break;
    case 'K' :
      keycode = AKEY_K;
      break;
    case 'L' :
      keycode = AKEY_L;
      break;
    case 'M' :
      keycode = AKEY_M;
      break;
    case 'N' :
      keycode = AKEY_N;
      break;
    case 'O' :
      keycode = AKEY_O;
      break;
    case 'P' :
      keycode = AKEY_P;
      break;
    case 'Q' :
      keycode = AKEY_Q;
      break;
    case 'R' :
      keycode = AKEY_R;
      break;
    case 'S' :
      keycode = AKEY_S;
      break;
    case 'T' :
      keycode = AKEY_T;
      break;
    case 'U' :
      keycode = AKEY_U;
      break;
    case 'V' :
      keycode = AKEY_V;
      break;
    case 'W' :
      keycode = AKEY_W;
      break;
    case 'X' :
      keycode = AKEY_X;
      break;
    case 'Y' :
      keycode = AKEY_Y;
      break;
    case 'Z' :
      keycode = AKEY_Z;
      break;
    case 0x1b :
      keycode = AKEY_ESCAPE;
      break;
    case 0x00 :
    switch(scancode)
      {
        case 0x3b :
          keycode = AKEY_UI;
          break;
        case 0x3c :
          consol &= 0x03;
          keycode = AKEY_NONE;
          break;
        case 0x3d :
          consol &= 0x05;
          keycode = AKEY_NONE;
          break;
        case 0x3e :
          consol &= 0x06;
          keycode = AKEY_NONE;
          break;
        case 0x3f : /* F5 */
          keycode = AKEY_WARMSTART;
          break;
        case 0x58 : /* Shift F5 */
          keycode = AKEY_COLDSTART;
          break;
        case 0x40 :
          keycode = AKEY_PIL;
          break;
        case 0x41 :
          keycode = AKEY_BREAK;
          break;
        case 0x42 :
          keycode = AKEY_NONE;
          break;
        case 0x43 :
          keycode = AKEY_EXIT;
          break;
        case 0x50 :
          keycode = AKEY_DOWN;
          break;
        case 0x4b :
          keycode = AKEY_LEFT;
          break;
        case 0x4d :
          keycode = AKEY_RIGHT;
          break;
        case 0x48 :
          keycode = AKEY_UP;
          break;
        case 0x2e :
          keycode = AKEY_CTRL_c;
          break;
        default :
          keycode = AKEY_NONE;
          break;
      }
      break;
    default :
      keycode = AKEY_NONE;
      break;
    }

  return keycode;
}

/* --------------------------------------------------------------------------*/

int Atari_PORT(int num)
{
        if ( num == 0 )
                return 0xf0 | stick0;
        else
                return 0xff;
}

/* --------------------------------------------------------------------------*/

int Atari_TRIG(int num)
{
        if ( num == 0 )
                return trig0;
        else
                return 1;
}

/* --------------------------------------------------------------------------*/

int Atari_POT(int num)
{
        return 228;
}

/* --------------------------------------------------------------------------*/

int Atari_CONSOL(void)
{
        return consol;
}

/* --------------------------------------------------------------------------*/

void Atari_AUDC(int channel, int byte)
{
}

/* --------------------------------------------------------------------------*/

void Atari_AUDF(int channel, int byte)
{
}

/* --------------------------------------------------------------------------*/

void Atari_AUDCTL(int byte)
{
}

/* --------------------------------------------------------------------------*/

