/* --------------------------------------------------------------------------*/

/*
 * DJGPP - VGA Backend for David Firth's Atari 800 emulator
 *
 * by Ivo van Poorten (C)1996  See file COPYRIGHT for policy.
 *
 */

/* --------------------------------------------------------------------------*/

#include <dpmi.h>
#include <go32.h>
#include <pc.h>
#include <sys/farptr.h>
#include <dos.h>
#include "cpu.h"
#include "colours.h"

/* --------------------------------------------------------------------------*/

static int consol;
static int trig0;
static int stick0;

#define FALSE 0
#define TRUE 1

static int first_lno = 24;
static int ypos_inc = 1;
static int vga_ptr_inc = 320;
static int scr_ptr_inc = ATARI_WIDTH;

/* --------------------------------------------------------------------------*/

unsigned char raw_key;

_go32_dpmi_seginfo old_key_handler, new_key_handler;

static int key[10] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static int shift = FALSE;
static int control = FALSE;
static int caps_lock = TRUE;

void key_handler(void)
{
  asm("cli; pusha");
  raw_key = inportb(0x60);

  switch (raw_key & 0x7f)
    {
      case 0x52 : /* 0 */
        key[0] = raw_key & 0x80;
        break;
      case 0x4f : /* 1 */
        key[1] = raw_key & 0x80;
        break;
      case 0x50 : /* 2 */
        key[2] = raw_key & 0x80;
        break;
      case 0x51 : /* 3 */
        key[3] = raw_key & 0x80;
        break;
      case 0x4b : /* 4 */
        key[4] = raw_key & 0x80;
        break;
      case 0x4c : /* 5 */
        key[5] = raw_key & 0x80;
        break;
      case 0x4d : /* 6 */
        key[6] = raw_key & 0x80;
        break;
      case 0x47 : /* 7 */
        key[7] = raw_key & 0x80;
        break;
      case 0x48 : /* 8 */
        key[8] = raw_key & 0x80;
        break;
      case 0x49 : /* 9 */
        key[9] = raw_key & 0x80;
        break;
    }

  switch (raw_key)
    {
      case 0x2a :
      case 0x36 :
        shift = TRUE;
        break;
      case 0xaa :
      case 0xb6 :
        shift = FALSE;
        break;
      case 0x1d :
        control = TRUE;
        break;
      case 0x9d :
        control = FALSE;
        break;
      case 0x3a :
        caps_lock = !caps_lock;
        break;
    }

  outportb(0x20, 0x20);
  asm("popa; sti");
}

void key_init(void)
{
  new_key_handler.pm_offset   = (int)key_handler;
  new_key_handler.pm_selector = _go32_my_cs();
  _go32_dpmi_get_protected_mode_interrupt_vector(0x9, &old_key_handler);
  _go32_dpmi_allocate_iret_wrapper(&new_key_handler);
  _go32_dpmi_set_protected_mode_interrupt_vector(0x9,&new_key_handler);
}

void key_delete(void)
{
  _go32_dpmi_set_protected_mode_interrupt_vector(0x9,&old_key_handler);
}

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

  stick0 = STICK_CENTRE;
  consol = 7;

  key_init();
}

/* --------------------------------------------------------------------------*/

int Atari_Exit(int run_monitor)
{
  union REGS rg;

  /* restore to text mode */

  rg.x.ax = 0x0003;
  int86 (0x10, &rg, &rg);

  /* cleanupSB(); */

  key_delete();

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
  static int last_keycode = AKEY_NONE;
  int   keycode;

/*
 * Trigger, Joystick and Console handling should
 * be moved into the Keyboard Interrupt Routine
 */

  if (key[0])
    trig0 = 1;
  else
    trig0 = 0;

  stick0 = 0x0f;
  if (!key[1] || !key[4] || !key[7])
    stick0 &= 0xfb;
  if (!key[3] || !key[6] || !key[9])
    stick0 &= 0xf7;
  if (!key[1] || !key[2] || !key[3])
    stick0 &= 0xfd;
  if (!key[7] || !key[8] || !key[9])
    stick0 &= 0xfe;

  if (key[5])
    consol = 0x07;
  else
    consol = 0x05;

/*
 * This needs a bit of tidying up - array lookup
 */

  switch (raw_key)
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
        if (shift)
          keycode = AKEY_COLDSTART;
        else
          keycode = AKEY_WARMSTART;
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
      case 0x01 :
        keycode = AKEY_ESCAPE;
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
      case 0x02 :
        if (shift)
          keycode = AKEY_EXCLAMATION;
        else
          keycode = AKEY_1;
        break;
      case 0x03 :
        if (shift)
          keycode = AKEY_DBLQUOTE;
        else
          keycode = AKEY_2;
        break;
      case 0x04 :
        keycode = AKEY_3;
        break;
      case 0x05 :
        if (shift)
          keycode = AKEY_DOLLAR;
        else
          keycode = AKEY_4;
        break;
      case 0x06 :
        if (shift)
          keycode = AKEY_PERCENT;
        else
          keycode = AKEY_5;
        break;
      case 0x07 :
        if (shift)
          keycode = AKEY_CIRCUMFLEX;
        else
          keycode = AKEY_6;
        break;
      case 0x08 :
        if (shift)
          keycode = AKEY_AMPERSAND;
        else
          keycode = AKEY_7;
        break;
      case 0x09 :
        if (shift)
          keycode = AKEY_ASTERISK;
        else
          keycode = AKEY_8;
        break;
      case 0x0a :
        if (shift)
          keycode = AKEY_PARENLEFT;
        else
          keycode = AKEY_9;
        break;
      case 0x0b :
        if (shift)
          keycode = AKEY_PARENRIGHT;
        else
          keycode = AKEY_0;
        break;
      case 0x0c :
        if (shift)
          keycode = AKEY_UNDERSCORE;
        else
          keycode = AKEY_MINUS;
        break;
      case 0x0d :
        if (shift)
          keycode = AKEY_PLUS;
        else
          keycode = AKEY_EQUAL;
        break;
      case 0x0e :
        keycode = AKEY_BACKSPACE;
        break;
      case 0x10 :
        if (control)
          keycode = AKEY_CTRL_q;
        else if (caps_lock)
          keycode = AKEY_Q;
        else if (shift)
          keycode = AKEY_Q;
        else
          keycode = AKEY_q;
        break;
      case 0x11 :
        if (control)
          keycode = AKEY_CTRL_w;
        else if (caps_lock)
          keycode = AKEY_W;
        else if (shift)
          keycode = AKEY_W;
        else
          keycode = AKEY_w;
        break;
      case 0x12 :
        if (control)
          keycode = AKEY_CTRL_e;
        else if (caps_lock)
          keycode = AKEY_E;
        else if (shift)
          keycode = AKEY_E;
        else
          keycode = AKEY_e;
        break;
      case 0x13 :
        if (control)
          keycode = AKEY_CTRL_r;
        else if (caps_lock)
          keycode = AKEY_R;
        else if (shift)
          keycode = AKEY_R;
        else
          keycode = AKEY_r;
        break;
      case 0x14 :
        if (control)
          keycode = AKEY_CTRL_t;
        else if (caps_lock)
          keycode = AKEY_T;
        else if (shift)
          keycode = AKEY_T;
        else
          keycode = AKEY_t;
        break;
      case 0x15 :
        if (control)
          keycode = AKEY_CTRL_y;
        else if (caps_lock)
          keycode = AKEY_Y;
        else if (shift)
          keycode = AKEY_Y;
        else
          keycode = AKEY_y;
        break;
      case 0x16 :
        if (control)
          keycode = AKEY_CTRL_u;
        else if (caps_lock)
          keycode = AKEY_U;
        else if (shift)
          keycode = AKEY_U;
        else
          keycode = AKEY_u;
        break;
      case 0x17 :
        if (control)
          keycode = AKEY_CTRL_i;
        else if (caps_lock)
          keycode = AKEY_I;
        else if (shift)
          keycode = AKEY_I;
        else
          keycode = AKEY_i;
        break;
      case 0x18 :
        if (control)
          keycode = AKEY_CTRL_o;
        else if (caps_lock)
          keycode = AKEY_O;
        else if (shift)
          keycode = AKEY_O;
        else
          keycode = AKEY_o;
        break;
      case 0x19 :
        if (control)
          keycode = AKEY_CTRL_p;
        else if (caps_lock)
          keycode = AKEY_P;
        else if (shift)
          keycode = AKEY_P;
        else
          keycode = AKEY_p;
        break;
      case 0x1a :
        keycode = AKEY_BRACKETLEFT;
        break;
      case 0x1b :
        keycode = AKEY_BRACKETRIGHT;
        break;
      case 0x1c :
        keycode = AKEY_RETURN;
        break;
      case 0x1e :
        if (control)
          keycode = AKEY_CTRL_a;
        else if (caps_lock)
          keycode = AKEY_A;
        else if (shift)
          keycode = AKEY_A;
        else
          keycode = AKEY_a;
        break;
      case 0x1f :
        if (control)
          keycode = AKEY_CTRL_s;
        else if (caps_lock)
          keycode = AKEY_S;
        else if (shift)
          keycode = AKEY_S;
        else
          keycode = AKEY_s;
        break;
      case 0x20 :
        if (control)
          keycode = AKEY_CTRL_d;
        else if (caps_lock)
          keycode = AKEY_D;
        else if (shift)
          keycode = AKEY_D;
        else
          keycode = AKEY_d;
        break;
      case 0x21 :
        if (control)
          keycode = AKEY_CTRL_f;
        else if (caps_lock)
          keycode = AKEY_F;
        else if (shift)
          keycode = AKEY_F;
        else
          keycode = AKEY_f;
        break;
      case 0x22 :
        if (control)
          keycode = AKEY_CTRL_g;
        else if (caps_lock)
          keycode = AKEY_G;
        else if (shift)
          keycode = AKEY_G;
        else
          keycode = AKEY_g;
        break;
      case 0x23 :
        if (control)
          keycode = AKEY_CTRL_h;
        else if (caps_lock)
          keycode = AKEY_H;
        else if (shift)
          keycode = AKEY_H;
        else
          keycode = AKEY_h;
        break;
      case 0x24 :
        if (control)
          keycode = AKEY_CTRL_j;
        else if (caps_lock)
          keycode = AKEY_J;
        else if (shift)
          keycode = AKEY_J;
        else
          keycode = AKEY_j;
        break;
      case 0x25 :
        if (control)
          keycode = AKEY_CTRL_k;
        else if (caps_lock)
          keycode = AKEY_K;
        else if (shift)
          keycode = AKEY_K;
        else
          keycode = AKEY_k;
        break;
      case 0x26 :
        if (control)
          keycode = AKEY_CTRL_l;
        else if (caps_lock)
          keycode = AKEY_L;
        else if (shift)
          keycode = AKEY_L;
        else
          keycode = AKEY_l;
        break;
      case 0x27 :
        if (shift)
          keycode = AKEY_COLON;
        else
          keycode = AKEY_SEMICOLON;
        break;
      case 0x28 :
        if (shift)
          keycode = AKEY_AT;
        else
          keycode = AKEY_QUOTE;
        break;
      case 0x2b :
        keycode = AKEY_HASH;
        break;
      case 0x2c :
        if (control)
          keycode = AKEY_CTRL_z;
        else if (caps_lock)
          keycode = AKEY_Z;
        else if (shift)
          keycode = AKEY_Z;
        else
          keycode = AKEY_z;
        break;
      case 0x2d :
        if (control)
          keycode = AKEY_CTRL_x;
        else if (caps_lock)
          keycode = AKEY_X;
        else if (shift)
          keycode = AKEY_X;
        else
          keycode = AKEY_x;
        break;
      case 0x2e :
        if (control)
          keycode = AKEY_CTRL_c;
        else if (caps_lock)
          keycode = AKEY_C;
        else if (shift)
          keycode = AKEY_C;
        else
          keycode = AKEY_c;
        break;
      case 0x2f :
        if (control)
          keycode = AKEY_CTRL_v;
        else if (caps_lock)
          keycode = AKEY_V;
        else if (shift)
          keycode = AKEY_V;
        else
          keycode = AKEY_v;
        break;
      case 0x30 :
        if (control)
          keycode = AKEY_CTRL_b;
        else if (caps_lock)
          keycode = AKEY_B;
        else if (shift)
          keycode = AKEY_B;
        else
          keycode = AKEY_b;
        break;
      case 0x31 :
        if (control)
          keycode = AKEY_CTRL_n;
        else if (caps_lock)
          keycode = AKEY_N;
        else if (shift)
          keycode = AKEY_N;
        else
          keycode = AKEY_n;
        break;
      case 0x32 :
        if (control)
          keycode = AKEY_CTRL_m;
        else if (caps_lock)
          keycode = AKEY_M;
        else if (shift)
          keycode = AKEY_M;
        else
          keycode = AKEY_m;
        break;
      case 0x33 :
        if (shift)
          keycode = AKEY_LESS;
        else
          keycode = AKEY_COMMA;
        break;
      case 0x34 :
        if (shift)
          keycode = AKEY_GREATER;
        else
          keycode = AKEY_FULLSTOP;
        break;
      case 0x35 :
        if (shift)
          keycode = AKEY_QUESTION;
        else
          keycode = AKEY_SLASH;
        break;
      case 0x39 :
        keycode = AKEY_SPACE;
        break;
      case 0x3a :
        keycode = AKEY_CAPSTOGGLE;
        break;
      case 0x56 :
        if (shift)
          keycode = AKEY_BAR;
        else
          keycode = AKEY_BACKSLASH;
        break;
      default :
        keycode = AKEY_NONE;
        break;
    }

/*
 * Add a crude keyboard debounce - this
 * won't allow repeating characters yet
 */

  if (keycode == last_keycode)
    keycode = AKEY_NONE;
  else
    last_keycode = keycode;

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

