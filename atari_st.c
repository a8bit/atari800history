/* --------------------------------------------------------------------------*/

/*
 * DJGPP - VGA Backend for David Firth's Atari 800 emulator
 *
 * by Ivo van Poorten (C)1996  See file COPYRIGHT for policy.
 *
 */

/* --------------------------------------------------------------------------*/

#include <osbind.h>
#include "cpu.h"
#include "colours.h"

/* --------------------------------------------------------------------------*/

static int consol;
static int trig0;
static int stick0;

short screen_w = ATARI_WIDTH;
short screen_h = ATARI_HEIGHT;
UBYTE *odkud, *kam;

UBYTE scancode;
UBYTE control_key = 0;
UBYTE shift_key = 0;

extern double deltatime;

UBYTE *Log_base, *Phys_base;

UWORD stara_graf[25];
UWORD nova_graf[25]={0x0057,0x0000,0x0000,0x0000,0x0000,0x0007,0x0000,0x0010,\
					0x00C0,0x0186,0x0005,0x00D7,0x00B2,0x000B,0x02A1,0x00A0,\
					0x00B9,0x0000,0x0000,0x0425,0x03F3,0x0033,0x0033,0x03F3,0x0415};

UBYTE *nova_vram;

extern void rplanes(void);
extern void load_registers(void);
extern void save_registers(void);
extern ULONG *patch_struct_ptr;

ULONG f030coltable[256];
ULONG f030coltable_zaloha[256];
ULONG *col_table;

void get_colors_on_f030(void)
{
	int i;
	ULONG *x = (ULONG *)0xff9800;

	for(i=0; i<256; i++)
		col_table[i] = x[i];
}

void set_colors_on_f030(void)
{
	int i;
	ULONG *x = (ULONG *)0xff9800;

	for(i=0; i<256; i++)
		x[i] = col_table[i];
}

/* --------------------------------------------------------------------------*/

void Atari_Initialise(int *argc, char *argv[])
{
  int a,r,g,b;

  /* vypni kurzor */
  Cursconf(0,0);

  /* uschovat stare barvy */
  col_table = f030coltable_zaloha;
  Supexec(get_colors_on_f030);

  /* uschovej starou grafiku */
  patch_struct_ptr = (ULONG *)stara_graf;
  Supexec(save_registers);

  Log_base = Logbase();
  Phys_base = Physbase();

  /* zapni graficky mod 384x240x256 */
  nova_vram = (UBYTE *)Mxalloc((ULONG)(384UL*256UL),0);
  Setscreen(nova_vram,nova_vram,-1);
  patch_struct_ptr = (ULONG *)nova_graf;
  Supexec(load_registers);

  /* nastav nove barvy */
  for(a=0; a<256; a++) {
    r = ( colortable[a] >> 18 ) & 0x3f;
    g = ( colortable[a] >> 10 ) & 0x3f;
    b = ( colortable[a] >>  2 ) & 0x3f;
	f030coltable[a] = (r << 26) | (g << 18) | (b << 2);
  }
  col_table = f030coltable;
  Supexec(set_colors_on_f030);

  consol = 7;
}

/* --------------------------------------------------------------------------*/

int Atari_Exit(int run_monitor)
{

  /* vratit puvodni graficky mod */
  patch_struct_ptr = (ULONG *)stara_graf;
  Supexec(load_registers);
  Setscreen(Log_base,Phys_base,-1);

  /* obnovit stare barvy */
  col_table = f030coltable_zaloha;
  Supexec(set_colors_on_f030);

  /* zahodit znaky z klavesoveho bufru */
  while(Cconis())
  	Crawcin();

  return 0;
}

/* --------------------------------------------------------------------------*/

void Atari_DisplayScreen(UBYTE *screen)
{
  odkud = screen;
  kam = Logbase();

  rplanes();

  consol = 7;
}

/* --------------------------------------------------------------------------*/

void read_scancode(void)
{
	scancode=*(UBYTE *)0xfffffc02;
}

int Atari_Keyboard(void)
{
  int keycode;

  trig0 = 1;
/*  stick0 = STICK_CENTRE */

  consol = 7;

  deltatime = 0.0;	/* zachran rychlost! */

  /* mrkni se byla-li stlacena klavesa */

#if 1
  Supexec(read_scancode);

  if ((scancode & 0x7f) == 0x1d)
		control_key = (scancode < 0x80);
  if ((scancode & 0x7f) == 0x2a || (scancode & 0x7f) == 0x36)
		shift_key = (scancode < 0x80);

  if (scancode > 0x7f)
        return AKEY_NONE;

  /* precti stisknutou klavesu */
  keycode = *(UBYTE *)(*(ULONG *)Keytbl(-1,-1,-1) + (shift_key?128:0) + scancode);

  if (control_key)
    keycode -= 64;
#else
  if (! Cconis())
        return AKEY_NONE;
  keycode = Crawcin();
  scancode = (keycode >> 16) & 0xff;
#endif
 
  switch (keycode)
    {
    case 0x01 :
      keycode = AKEY_CTRL_a;
      break;
    case 0x02 :
      keycode = AKEY_CTRL_b;
      break;
   case 0x03 :
      keycode = AKEY_CTRL_c;
      break;
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
      if(scancode==0x70)        /* keypad 0 == fire button */
	{
	  keycode = AKEY_NONE;
	  trig0=0;
	}
      break;
    case '1' :
      keycode = AKEY_1;
      if(scancode==0x6d)        /* keypad 1 */
	{
	  stick0=STICK_LL;
	  keycode = AKEY_NONE;
	}
      break;
    case '2' :
      keycode = AKEY_2;
      if(scancode==0x6e)        /* keypad 2 */
	{
	  stick0=STICK_BACK;
	  keycode = AKEY_NONE;
	}
      break;
    case '3' :
      keycode = AKEY_3;
      if(scancode==0x6f)        /* keypad 3 */
	{
	  stick0=STICK_LR;
	  keycode = AKEY_NONE;
	}
      break;
    case '4' :
      keycode = AKEY_4;
      if(scancode==0x6a)        /* keypad 4 */
	{
	  stick0=STICK_LEFT;
	  keycode = AKEY_NONE;
	}
      break;
    case '5' :
      keycode = AKEY_5;        
      if(scancode==0x6b)        /* keypad 5 */
	{
	  stick0=STICK_CENTRE;
	  keycode = AKEY_NONE;
	}
      break;
    case '6' :
      keycode = AKEY_6;
      if(scancode==0x6c)        /* keypad 6 */
	{
	  stick0=STICK_RIGHT;
	  keycode = AKEY_NONE;
	}
      break;
    case '7' :
      keycode = AKEY_7;
      if(scancode==0x67)        /* keypad 7 */
	{
	  stick0=STICK_UL;
	  keycode = AKEY_NONE;
	}
      break;
    case '8' :
      keycode = AKEY_8;
      if(scancode==0x68)        /* keypad 8 */
	{
	  stick0=STICK_FORWARD;
	  keycode = AKEY_NONE;
	}
      break;
    case '9' :
      keycode = AKEY_9;
      if(scancode==0x69)        /* keypad 9 */
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
        case 0x61 :
          keycode = AKEY_UI;
          break;
        case 0x62 :
          keycode = AKEY_HELP;
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

