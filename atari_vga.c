/* -------------------------------------------------------------------------- */

/*
 * DJGPP - VGA Backend for David Firth's Atari 800 emulator
 *
 * by Ivo van Poorten (C)1996  See file COPYRIGHT for policy.
 *
 */

/* -------------------------------------------------------------------------- */

#include <stdio.h>
#include <dpmi.h>
#include <go32.h>
#include <pc.h>
#include <sys/farptr.h>
#include <dos.h>
#include "cpu.h"
#include "colours.h"

/* -------------------------------------------------------------------------- */

#define FALSE 0
#define TRUE 1

static int consol;
static int trig0;
static int stick0;
static int trig1;
static int stick1;
static int joyswap = FALSE;

static int first_lno = 24;
static int first_col = 32;
static int ypos_inc = 1;
static int vga_ptr_inc = 320;
static int scr_ptr_inc = ATARI_WIDTH;

#ifdef BACKUP_MSG
extern char backup_msg_buf[300];
#endif

static int joy_in = FALSE;
static int js0_centre_x;
static int js0_centre_y;

int joystick0(int *x, int *y)
{
	int jx, jy, jb;

	__asm__ __volatile__(
							"cli\n\t"
							"movw	$0x201,%%dx\n\t"
							"xorl	%%ebx,%%ebx\n\t"
							"xorl	%%ecx,%%ecx\n\t"
							"outb	%%al,%%dx\n\t"
							"__jloop:\n\t"
							"inb	%%dx,%%al\n\t"
							"testb	$1,%%al\n\t"
							"jz		x_ok\n\t"
							"incl	%%ecx\n\t"
							"x_ok:\n\t"
							"testb	$2,%%al\n\t"
							"jz		y_ok\n\t"
							"incl	%%ebx\n\t"
							"jmp	__jloop\n\t"
							"y_ok:\n\t"
							"testb	$1,%%al\n\t"
							"jnz	__jloop\n\t"
							"sti\n\t"
							"shrb	$4,%%al"
							:"=a"(jb), "=b"(jy), "=c"(jx)
							:	/* no input values */
							:"%al", "%ebx", "%ecx", "%dx"
	);

	*x = jx;
	*y = jy;

	return jb & 0x03;
}

void read_joystick(int centre_x, int centre_y)
{
	const int threshold = 50;
	int jsx, jsy;

	stick0 = STICK_CENTRE;

	trig0 = (joystick0(&jsx, &jsy) < 3) ? 0 : 1;

	if (jsx < (centre_x - threshold))
		stick0 &= 0xfb;
	if (jsx > (centre_x + threshold))
		stick0 &= 0xf7;
	if (jsy < (centre_y - threshold))
		stick0 &= 0xfe;
	if (jsy > (centre_y + threshold))
		stick0 &= 0xfd;
}

/* -------------------------------------------------------------------------- */

unsigned char raw_key;

_go32_dpmi_seginfo old_key_handler, new_key_handler;

static int key[10] =
{1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
extern int SHIFT_KEY, KEYPRESSED;
static int control = FALSE;
static int caps_lock = FALSE;
static int extended_key_follows = FALSE;

void key_handler(void)
{
	asm("cli; pusha");
	raw_key = inportb(0x60);

	if (!extended_key_follows) {	/* evaluate just numeric keypad! */
		switch (raw_key & 0x7f) {
		case 0x52:				/* 0 */
			key[0] = raw_key & 0x80;
			break;
		case 0x4f:				/* 1 */
			key[1] = raw_key & 0x80;
			break;
		case 0x50:				/* 2 */
			key[2] = raw_key & 0x80;
			break;
		case 0x51:				/* 3 */
			key[3] = raw_key & 0x80;
			break;
		case 0x4b:				/* 4 */
			key[4] = raw_key & 0x80;
			break;
		case 0x4c:				/* 5 */
			key[5] = raw_key & 0x80;
			break;
		case 0x4d:				/* 6 */
			key[6] = raw_key & 0x80;
			break;
		case 0x47:				/* 7 */
			key[7] = raw_key & 0x80;
			break;
		case 0x48:				/* 8 */
			key[8] = raw_key & 0x80;
			break;
		case 0x49:				/* 9 */
			key[9] = raw_key & 0x80;
			break;
		}
	}

	extended_key_follows = FALSE;

	switch (raw_key) {
	case 0x2a:
	case 0x36:
		SHIFT_KEY = TRUE;
		break;
	case 0xaa:
	case 0xb6:
		SHIFT_KEY = FALSE;
		break;
	case 0x1d:
		control = TRUE;
		break;
	case 0x9d:
		control = FALSE;
		break;
	case 0x3a:
		caps_lock = !caps_lock;
		break;
	case 0xe0:
		extended_key_follows = TRUE;
	}

	outportb(0x20, 0x20);
	asm("popa; sti");
}

void key_init(void)
{
	new_key_handler.pm_offset = (int) key_handler;
	new_key_handler.pm_selector = _go32_my_cs();
	_go32_dpmi_get_protected_mode_interrupt_vector(0x9, &old_key_handler);
	_go32_dpmi_allocate_iret_wrapper(&new_key_handler);
	_go32_dpmi_set_protected_mode_interrupt_vector(0x9, &new_key_handler);
}

void key_delete(void)
{
	_go32_dpmi_set_protected_mode_interrupt_vector(0x9, &old_key_handler);
}

void SetupVgaEnvironment()
{
	int a, r, g, b;
	union REGS rg;

	rg.x.ax = 0x0013;
	int86(0x10, &rg, &rg);

	for (a = 0; a < 256; a++) {
		r = (colortable[a] >> 18) & 0x3f;
		g = (colortable[a] >> 10) & 0x3f;
		b = (colortable[a] >> 2) & 0x3f;
		rg.x.ax = 0x1010;
		rg.x.bx = a;
		rg.h.dh = r;
		rg.h.ch = g;
		rg.h.cl = b;
		int86(0x10, &rg, &rg);
	}

	key_init();
}

void Atari_Initialise(int *argc, char *argv[])
{
	int i;
	int j;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-interlace") == 0) {
			ypos_inc = 2;
			vga_ptr_inc = 320 + 320;
			scr_ptr_inc = ATARI_WIDTH + ATARI_WIDTH;
		}
		else if (strcmp(argv[i], "-joyswap") == 0)
			joyswap = TRUE;
		else {
			if (strcmp(argv[i], "-help") == 0)
				printf("\t-interlace    Generate screen with interlace\n");
			argv[j++] = argv[i];
		}
	}

	*argc = j;

	/* check if joystick is connected */
	printf("Checking for joystick...");
	fflush(stdout);
	outportb(0x201, 0xff);
	usleep(100000UL);
	joy_in = (inportb(0x201) == 0xfc);
	if (joy_in)
		printf(" found!\n");
	else
		printf(" sorry, I see no joystick. Use numeric pad\n");

	Sound_Initialise(argc, argv);

	SetupVgaEnvironment();

	/* setup joystick */
	if (joy_in)
		joystick0(&js0_centre_x, &js0_centre_y);
	stick0 = stick1 = STICK_CENTRE;
	trig0 = trig1 = 1;

	consol = 7;
}

/* -------------------------------------------------------------------------- */

int Atari_Exit(int run_monitor)
{
	union REGS rg;

	/* restore to text mode */

	rg.x.ax = 0x0003;
	int86(0x10, &rg, &rg);

#ifdef BACKUP_MSG
	if (*backup_msg_buf) {		/* print the buffer */
		puts(backup_msg_buf);
		*backup_msg_buf = 0;
	}
#endif

	key_delete();				/* enable keyboard in monitor */

	if (run_monitor)
		if (monitor()) {
			SetupVgaEnvironment();

			return 1;			/* return to emulation */
		}

	Sound_Exit();

	return 0;
}

/* -------------------------------------------------------------------------- */

void Atari_DisplayScreen(UBYTE * screen)
{
	static int lace = 0;

	UBYTE *vga_ptr;
	UBYTE *scr_ptr;
	int ypos;
	int x;

	vga_ptr = (UBYTE *) 0xa0000;
	scr_ptr = &screen[first_lno * ATARI_WIDTH + first_col];

	if (ypos_inc == 2) {
		if (lace) {
			vga_ptr += 320;
			scr_ptr += ATARI_WIDTH;
		}
		lace = 1 - lace;
	}
	_farsetsel(_dos_ds);
	for (ypos = 0; ypos < 200; ypos += ypos_inc) {
/*
   for (x = 0; x < 320; x += 4) {
   _farnspokel((int) (vga_ptr + x), *(long *) (scr_ptr + x));
   }
 */
		_dosmemputl(scr_ptr, 320 / 4, (long) vga_ptr);
		vga_ptr += vga_ptr_inc;
		scr_ptr += scr_ptr_inc;
	}

	consol = 7;
}

/* -------------------------------------------------------------------------- */

int Atari_Keyboard(void)
{
	int keycode;

/*
 * Trigger, Joystick and Console handling should
 * be moved into the Keyboard Interrupt Routine
 */

	if (key[0])
		trig0 = 1;
	else
		trig0 = 0;

	stick0 = STICK_CENTRE;
	if (!key[1] || !key[4] || !key[7])
		stick0 &= 0xfb;
	if (!key[3] || !key[6] || !key[9])
		stick0 &= 0xf7;
	if (!key[1] || !key[2] || !key[3])
		stick0 &= 0xfd;
	if (!key[7] || !key[8] || !key[9])
		stick0 &= 0xfe;

	if (stick0 != STICK_CENTRE || trig0 != 1)
		raw_key = 0;			/* do not evaluate the pressed key */

	/* if joy_in then keyboard emulates stick1 instead of standard stick0 */
	if (joy_in) {
		stick1 = stick0;
		trig1 = trig0;
		read_joystick(js0_centre_x, js0_centre_y);	/* read real PC joystick */
	}

	/* if joyswap then joy0 and joy1 are exchanged */
	if (joyswap) {
		int stick = stick1;
		int trig = trig1;
		stick1 = stick0;
		trig1 = trig0;
		stick0 = stick;
		trig0 = trig;
	}

#ifdef STRANGE_SELECT			/* Sorry David, I couldn't play with this enabled. I pressed the '5' by mistake too many times */
	if (key[5])
		consol = 0x07;
	else
		consol = 0x05;			/* SELECT key (why?) */
#endif

/*
 * This needs a bit of tidying up - array lookup
 */

	switch (raw_key) {
	case 0x3b:
		keycode = AKEY_UI;
		break;
	case 0x3c:
		consol &= 0x03;			/* OPTION key */
		keycode = AKEY_NONE;
		break;
	case 0x3d:
		consol &= 0x05;			/* SELECT key */
		keycode = AKEY_NONE;
		break;
	case 0x3e:
		consol &= 0x06;			/* START key */
		keycode = AKEY_NONE;
		break;
	case 0x3f:					/* F5 */
		keycode = SHIFT_KEY ? AKEY_COLDSTART : AKEY_WARMSTART;
		break;
	case 0x40:
		keycode = AKEY_PIL;
		break;
	case 0x41:
		keycode = AKEY_BREAK;
		break;
	case 0x42:
		/* keycode = AKEY_NONE; */
		keycode = Atari_Exit(1) ? AKEY_NONE : AKEY_EXIT;	/* invoke monitor */
		break;
	case 0x43:
		keycode = AKEY_EXIT;
		break;
	case 0x44:					/* centre 320x200 VGA screen */
		if (control) {
			first_lno = 24;
			first_col = 32;
			keycode = AKEY_NONE;
		}
		else
			keycode = AKEY_HELP;	/* F10 = HELP key */
		break;
	case 0x01:
		keycode = AKEY_ESCAPE;
		break;
	case 0x50:
		if (control) {
			if (first_lno < 40)
				first_lno++;	/* move VGA screen down */
			keycode = AKEY_NONE;
		}
		else
			keycode = AKEY_DOWN;
		break;
	case 0x4b:
		if (control) {
			if (first_col)
				first_col--;	/* move VGA screen left */
			keycode = AKEY_NONE;
		}
		else
			keycode = AKEY_LEFT;
		break;
	case 0x4d:
		if (control) {
			if (first_col < 64)
				first_col++;	/* move VGA screen right */
			keycode = AKEY_NONE;
		}
		else
			keycode = AKEY_RIGHT;
		break;
	case 0x48:
		if (control) {
			if (first_lno)
				first_lno--;	/* move VGA screen up */
			keycode = AKEY_NONE;
		}
		else
			keycode = AKEY_UP;
		break;
	case 0x02:
		if (control)
			keycode = 159;		/* stops text scrolling */
		else if (SHIFT_KEY)
			keycode = AKEY_EXCLAMATION;
		else
			keycode = AKEY_1;
		break;
	case 0x03:
		if (control)
			keycode = 158;		/* bell */
		else if (SHIFT_KEY)
			keycode = AKEY_DBLQUOTE;
		else
			keycode = AKEY_2;
		break;
	case 0x04:
		if (control)
			keycode = 154;		/* EOF */
		else if (SHIFT_KEY)
			keycode = AKEY_HASH;
		else
			keycode = AKEY_3;
		break;
	case 0x05:
		if (SHIFT_KEY)
			keycode = AKEY_DOLLAR;
		else
			keycode = AKEY_4;
		break;
	case 0x06:
		if (SHIFT_KEY)
			keycode = AKEY_PERCENT;
		else
			keycode = AKEY_5;
		break;
	case 0x07:
		if (SHIFT_KEY)
			keycode = AKEY_CIRCUMFLEX;
		else
			keycode = AKEY_6;
		break;
	case 0x08:
		if (SHIFT_KEY)
			keycode = AKEY_AMPERSAND;
		else
			keycode = AKEY_7;
		break;
	case 0x09:
		if (SHIFT_KEY)
			keycode = AKEY_ASTERISK;
		else
			keycode = AKEY_8;
		break;
	case 0x0a:
		if (SHIFT_KEY)
			keycode = AKEY_PARENLEFT;
		else
			keycode = AKEY_9;
		break;
	case 0x0b:
		if (SHIFT_KEY)
			keycode = AKEY_PARENRIGHT;
		else
			keycode = AKEY_0;
		break;
	case 0x0c:
		if (SHIFT_KEY)
			keycode = AKEY_UNDERSCORE;
		else
			keycode = AKEY_MINUS;
		break;
	case 0x0d:
		if (SHIFT_KEY)
			keycode = AKEY_PLUS;
		else
			keycode = AKEY_EQUAL;
		break;
	case 0x0e:
		keycode = AKEY_BACKSPACE;
		break;
	case 0x0f:
		if (SHIFT_KEY)
			keycode = AKEY_SETTAB;
		else if (control)
			keycode = AKEY_CLRTAB;
		else
			keycode = AKEY_TAB;
		break;
	case 0x10:
		if (control)
			keycode = AKEY_CTRL_q;
		else if (caps_lock)
			keycode = AKEY_Q;
		else if (SHIFT_KEY)
			keycode = AKEY_Q;
		else
			keycode = AKEY_q;
		break;
	case 0x11:
		if (control)
			keycode = AKEY_CTRL_w;
		else if (caps_lock)
			keycode = AKEY_W;
		else if (SHIFT_KEY)
			keycode = AKEY_W;
		else
			keycode = AKEY_w;
		break;
	case 0x12:
		if (control)
			keycode = AKEY_CTRL_e;
		else if (caps_lock)
			keycode = AKEY_E;
		else if (SHIFT_KEY)
			keycode = AKEY_E;
		else
			keycode = AKEY_e;
		break;
	case 0x13:
		if (control)
			keycode = AKEY_CTRL_r;
		else if (caps_lock)
			keycode = AKEY_R;
		else if (SHIFT_KEY)
			keycode = AKEY_R;
		else
			keycode = AKEY_r;
		break;
	case 0x14:
		if (control)
			keycode = AKEY_CTRL_t;
		else if (caps_lock)
			keycode = AKEY_T;
		else if (SHIFT_KEY)
			keycode = AKEY_T;
		else
			keycode = AKEY_t;
		break;
	case 0x15:
		if (control)
			keycode = AKEY_CTRL_y;
		else if (caps_lock)
			keycode = AKEY_Y;
		else if (SHIFT_KEY)
			keycode = AKEY_Y;
		else
			keycode = AKEY_y;
		break;
	case 0x16:
		if (control)
			keycode = AKEY_CTRL_u;
		else if (caps_lock)
			keycode = AKEY_U;
		else if (SHIFT_KEY)
			keycode = AKEY_U;
		else
			keycode = AKEY_u;
		break;
	case 0x17:
		if (control)
			keycode = AKEY_CTRL_i;
		else if (caps_lock)
			keycode = AKEY_I;
		else if (SHIFT_KEY)
			keycode = AKEY_I;
		else
			keycode = AKEY_i;
		break;
	case 0x18:
		if (control)
			keycode = AKEY_CTRL_o;
		else if (caps_lock)
			keycode = AKEY_O;
		else if (SHIFT_KEY)
			keycode = AKEY_O;
		else
			keycode = AKEY_o;
		break;
	case 0x19:
		if (control)
			keycode = AKEY_CTRL_p;
		else if (caps_lock)
			keycode = AKEY_P;
		else if (SHIFT_KEY)
			keycode = AKEY_P;
		else
			keycode = AKEY_p;
		break;
	case 0x1a:
		keycode = AKEY_BRACKETLEFT;
		break;
	case 0x1b:
		keycode = AKEY_BRACKETRIGHT;
		break;
	case 0x1c:
		keycode = AKEY_RETURN;
		break;
	case 0x1e:
		if (control)
			keycode = AKEY_CTRL_a;
		else if (caps_lock)
			keycode = AKEY_A;
		else if (SHIFT_KEY)
			keycode = AKEY_A;
		else
			keycode = AKEY_a;
		break;
	case 0x1f:
		if (control)
			keycode = AKEY_CTRL_s;
		else if (caps_lock)
			keycode = AKEY_S;
		else if (SHIFT_KEY)
			keycode = AKEY_S;
		else
			keycode = AKEY_s;
		break;
	case 0x20:
		if (control)
			keycode = AKEY_CTRL_d;
		else if (caps_lock)
			keycode = AKEY_D;
		else if (SHIFT_KEY)
			keycode = AKEY_D;
		else
			keycode = AKEY_d;
		break;
	case 0x21:
		if (control)
			keycode = AKEY_CTRL_f;
		else if (caps_lock)
			keycode = AKEY_F;
		else if (SHIFT_KEY)
			keycode = AKEY_F;
		else
			keycode = AKEY_f;
		break;
	case 0x22:
		if (control)
			keycode = AKEY_CTRL_g;
		else if (caps_lock)
			keycode = AKEY_G;
		else if (SHIFT_KEY)
			keycode = AKEY_G;
		else
			keycode = AKEY_g;
		break;
	case 0x23:
		if (control)
			keycode = AKEY_CTRL_h;
		else if (caps_lock)
			keycode = AKEY_H;
		else if (SHIFT_KEY)
			keycode = AKEY_H;
		else
			keycode = AKEY_h;
		break;
	case 0x24:
		if (control)
			keycode = AKEY_CTRL_j;
		else if (caps_lock)
			keycode = AKEY_J;
		else if (SHIFT_KEY)
			keycode = AKEY_J;
		else
			keycode = AKEY_j;
		break;
	case 0x25:
		if (control)
			keycode = AKEY_CTRL_k;
		else if (caps_lock)
			keycode = AKEY_K;
		else if (SHIFT_KEY)
			keycode = AKEY_K;
		else
			keycode = AKEY_k;
		break;
	case 0x26:
		if (control)
			keycode = AKEY_CTRL_l;
		else if (caps_lock)
			keycode = AKEY_L;
		else if (SHIFT_KEY)
			keycode = AKEY_L;
		else
			keycode = AKEY_l;
		break;
	case 0x27:
		if (SHIFT_KEY)
			keycode = AKEY_COLON;
		else
			keycode = AKEY_SEMICOLON;
		break;
	case 0x28:
		if (SHIFT_KEY)
			keycode = AKEY_AT;
		else
			keycode = AKEY_QUOTE;
		break;
	case 0x2b:
		keycode = AKEY_HASH;
		break;
	case 0x2c:
		if (control)
			keycode = AKEY_CTRL_z;
		else if (caps_lock)
			keycode = AKEY_Z;
		else if (SHIFT_KEY)
			keycode = AKEY_Z;
		else
			keycode = AKEY_z;
		break;
	case 0x2d:
		if (control)
			keycode = AKEY_CTRL_x;
		else if (caps_lock)
			keycode = AKEY_X;
		else if (SHIFT_KEY)
			keycode = AKEY_X;
		else
			keycode = AKEY_x;
		break;
	case 0x2e:
		if (control)
			keycode = AKEY_CTRL_c;
		else if (caps_lock)
			keycode = AKEY_C;
		else if (SHIFT_KEY)
			keycode = AKEY_C;
		else
			keycode = AKEY_c;
		break;
	case 0x2f:
		if (control)
			keycode = AKEY_CTRL_v;
		else if (caps_lock)
			keycode = AKEY_V;
		else if (SHIFT_KEY)
			keycode = AKEY_V;
		else
			keycode = AKEY_v;
		break;
	case 0x30:
		if (control)
			keycode = AKEY_CTRL_b;
		else if (caps_lock)
			keycode = AKEY_B;
		else if (SHIFT_KEY)
			keycode = AKEY_B;
		else
			keycode = AKEY_b;
		break;
	case 0x31:
		if (control)
			keycode = AKEY_CTRL_n;
		else if (caps_lock)
			keycode = AKEY_N;
		else if (SHIFT_KEY)
			keycode = AKEY_N;
		else
			keycode = AKEY_n;
		break;
	case 0x32:
		if (control)
			keycode = AKEY_CTRL_m;
		else if (caps_lock)
			keycode = AKEY_M;
		else if (SHIFT_KEY)
			keycode = AKEY_M;
		else
			keycode = AKEY_m;
		break;
	case 0x33:
		if (SHIFT_KEY)
			keycode = AKEY_LESS;
		else
			keycode = AKEY_COMMA;
		break;
	case 0x34:
		if (SHIFT_KEY)
			keycode = AKEY_GREATER;
		else
			keycode = AKEY_FULLSTOP;
		break;
	case 0x35:
		if (SHIFT_KEY)
			keycode = AKEY_QUESTION;
		else
			keycode = AKEY_SLASH;
		break;
	case 0x39:
		keycode = AKEY_SPACE;
		break;
	case 0x3a:
		keycode = AKEY_CAPSTOGGLE;
		break;
	case 0x56:
		if (SHIFT_KEY)
			keycode = AKEY_BAR;
		else
			keycode = AKEY_BACKSLASH;
		break;
	case 71:	/* HOME key */
		keycode = 118;		/* clear screen */
		break;
	case 79:	/* END key */
		keycode = AKEY_BREAK;
		break;
	case 82:	/* INSERT key */
		if (SHIFT_KEY)
			keycode = AKEY_INSERT_LINE;
		else
			keycode = AKEY_INSERT_CHAR;
		break;
	case 83:	/* DELETE key */
		if (SHIFT_KEY)
			keycode = AKEY_DELETE_LINE;
		else
			keycode = AKEY_DELETE_CHAR;
		break;
	default:
		keycode = AKEY_NONE;
		break;
	}

	KEYPRESSED = (keycode != AKEY_NONE);	/* for POKEY */

	return keycode;
}

/* -------------------------------------------------------------------------- */

int Atari_PORT(int num)
{
	if (num == 0)
		return (stick1 << 4) | stick0;
	else
		return 0xff;
}

/* -------------------------------------------------------------------------- */

int Atari_TRIG(int num)
{
	switch (num) {
	case 0:
		return trig0;
	case 1:
		return trig1;
	case 2:
	case 3:
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
