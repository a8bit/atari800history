/* -------------------------------------------------------------------------- */

/*
 * Atari Falcon/TT/Nova Backend for David Firth's Atari 800 emulator
 *
 * by Petr Stehlik & Karel Rous (C)1997-98  See file COPYRIGHT for policy.
 *
 */

/* -------------------------------------------------------------------------- */

#include <osbind.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>		/* for free */
// #include <linea.h>
#include <falcon.h>		/* for VsetRGB */

#include "cpu.h"
#include "colours.h"
#include "config.h"
#include "platform.h"
#include "monitor.h"
#include "log.h"

#include "gemfast.h"
#include "aesbind.h"
#include "vdibind.h"

/* -------------------------------------------------------------------------- */

#define FALSE 0
#define TRUE 1

int get_cookie(long cookie, long *value)
{
	long *cookiejar = (long *) Setexc(0x168, -1L);

	if (cookiejar) {
		while (*cookiejar) {
			if (*cookiejar == cookie) {
				if (value)
					*value = *++cookiejar;
				return (1);
			}
			cookiejar += 2;
		}
	}
	return (0);
}

/* -------------------------------------------------------------------------- */

#ifdef CLOCKY_SCREENSAVER

#include "jclkcook.h"

int screensaver(int on)
{
	long adr;
	JCLKSTRUCT *jclk;
	int oldval;

/*	CHECK_CLOCKY_STRUCT; */

	if (!get_cookie((long) 'JCLK', &adr))
		return 0;

	jclk = (JCLKSTRUCT *) adr;

	if (jclk->name != CLOCKY_IDENT) {
		return 0;
	}

	if (jclk->version != CLOCKY_VERSION) {
		return 0;
	}

	oldval = jclk->parametry.Saveron;
	jclk->parametry.Saveron = on;	/* turn the Clocky's screen saver on/off */

	return oldval;
}

static int screensaverval;		/* original value */
#endif	/* CLOCKY_SCREENSAVER */

/* -------------------------------------------------------------------------- */

typedef enum {
	UNKNOWN,
	NOVA,
	F030,
	TT030
} Video_HW;

static Video_HW video_hw = UNKNOWN;
static int gl_vdi_handle;
static int NOVA_double_size = 0;
static int HOST_WIDTH, HOST_HEIGHT, HOST_PLANES;
#define EMUL_WIDTH	(NOVA_double_size ? 2*336 : 336)
#define EMUL_HEIGHT	(NOVA_double_size ? 2*240 : 240)
#undef QUAD
#define CENTER_X	((HOST_WIDTH - EMUL_WIDTH) / 2)
#define CENTER_Y	((HOST_HEIGHT - EMUL_HEIGHT) / 2)
#define CENTER		(CENTER_X + CENTER_Y * HOST_WIDTH)

#ifdef SET_LED
static int LED_timeout = 0;
#endif

/* -------------------------------------------------------------------------- */

static int consol;
static int trig0;
static int stick0;
static int joyswap = FALSE;

/* parameters for c2p_uni */
UWORD screenw, screenh, vramw, vramh;
UBYTE *odkud, *kam;

/* parameters for DisplayScreen */
static int skip_N_frames = 0;

#ifdef CPUASS
#include "cpu_asm.h"
#endif
extern void init_kb(void);
extern void rem_kb(void);
extern char key_buf[128];
extern UBYTE joy0, joy1, buttons;
_KEYTAB *key_tab;
#define	KEYBUF_SIZE	256
unsigned char keybuf[KEYBUF_SIZE];
int kbhead = 0;

UBYTE *Original_Log_base, *Original_Phys_base;

UWORD original_videl_settings[25];
UWORD mode336x240_videl_settings[25]=
{0x0133, 0x0001, 0x3b00, 0x0150, 0x00f0, 0x0008, 0x0002, 0x0010, \
 0x00a8, 0x0186, 0x0005, 0x00c6, 0x0095, 0x000d, 0x0292, 0x0083, \
 0x0096, 0x0000, 0x0000, 0x0419, 0x03ff, 0x003f, 0x003f, 0x03ff, 0x0415};

static int new_videl_mode_valid = 0;
static int reprogram_VIDEL = 0;
UBYTE *new_videoram = NULL;

extern void rplanes(void);
extern void load_r(void);
extern void save_r(void);
extern ULONG *p_str_p;

long RGBcoltable[256], RGBcoltable_backup[256];
int coltable[256][3], coltable_backup[256][3];

void set_colors(int new)
{
	int i;

	switch(video_hw) {
		case F030:
//			VsetRGB(0, 256, new ? RGBcoltable : RGBcoltable_backup);
//			break;
		case TT030:
//			Esetpalette();
//			break;
		default:
			for(i=0; i<256; i++) {
				vs_color(gl_vdi_handle, i, new ? coltable[i] : coltable_backup[i]);
		}
	}
}

void save_original_colors(void)
{
	int i;

	switch(video_hw) {
		case F030:
//			VgetRGB(0, 256, RGBcoltable_backup);
//			break;
		case TT030:
			// Egetpalette
//			break;
		default:
			for(i=0; i<256; i++) {
				vq_color(gl_vdi_handle, i, 1, coltable_backup[i]);
		}
	}
}

void set_new_colors(void) { set_colors(1); }

void restore_original_colors(void) { set_colors(0); }

/* -------------------------------------------------------------------------- */

void SetupEmulatedEnvironment(void)
{
	if (reprogram_VIDEL) {
		/* set new video resolution by direct VIDEL programming */
		(void)Vsetscreen(new_videoram, new_videoram, -1, -1);
		p_str_p = (ULONG *)mode336x240_videl_settings;
		Supexec(load_r);
		new_videl_mode_valid = 1;
	}

	set_new_colors();	/* setup new color palette */

	Supexec(init_kb);	/* our keyboard routine */

	Bconout(4, 0x14);	/* joystick init */
}

void ShutdownEmulatedEnvironment(void)
{
	if (new_videl_mode_valid) {
		/* restore original VIDEL mode */
		p_str_p = (ULONG *) original_videl_settings;
		Supexec(load_r);
		new_videl_mode_valid = 0;
		(void)Vsetscreen(Original_Log_base, Original_Phys_base, -1, -1);
	}

	restore_original_colors();

	Supexec(rem_kb);	/* original keyboard routine */

	Bconout(4, 8);		/* joystick disable */
}

void Atari_Initialise(int *argc, char *argv[])
{
	int i;
	int j;

	int work_in[11], work_out[57];
	int maxx, maxy, maxw, maxh, wcell, hcell, wbox, hbox;
	int video_hardware;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-interlace") == 0) {
			sscanf(argv[++i], "%d", &skip_N_frames);
		}
		else if (strcmp(argv[i], "-joyswap") == 0)
			joyswap = TRUE;
		else {
			if (strcmp(argv[i], "-help") == 0) {
				printf("\t-interlace x  Generate screen only every number-th frame\n");
			}

			argv[j++] = argv[i];
		}
	}

	*argc = j;

	/* recalculate color tables */
	for (i = 0; i < 256; i++) {
		int r = (colortable[i] >> 18) & 0x3f;
		int g = (colortable[i] >> 10) & 0x3f;
		int b = (colortable[i] >> 2) & 0x3f;
		RGBcoltable[i] = (r << 16) | (g << 8) | b;
		coltable[i][0] = r * 1000 / 64;
		coltable[i][1] = g * 1000 / 64;
		coltable[i][2] = b * 1000 / 64;
	}

	/* check for VIDEL hardware */
	if (!get_cookie('_VDO', &video_hardware))
		video_hardware = 0;
	switch(video_hardware >> 16) {
		case 2:
			video_hw = TT030;
			break;
		case 3:
			video_hw = F030;
			break;
		default:
			video_hw = UNKNOWN;
	}

	/* check for NOVA graphics card */
	if (get_cookie('NOVA', NULL))
		video_hw = NOVA;

	/* GEM init */
	appl_init();
	graf_mouse(M_OFF, NULL);
	wind_get(0, WF_WORKXYWH, &maxx, &maxy, &maxw, &maxh);

	gl_vdi_handle = graf_handle(&wcell, &hcell, &wbox, &hbox);

	work_in[0] = Getrez() + 2;
	for(i = 1;i < 10;work_in[i++] = 1);
	work_in[10] = 2;
	v_opnvwk(work_in, &gl_vdi_handle, work_out);

	/* get current screen size and color depth */
	HOST_WIDTH = work_out[0] + 1;
	HOST_HEIGHT = work_out[1] + 1;

	vq_extnd(gl_vdi_handle, 1, work_out);
	HOST_PLANES = work_out[4];

	if (HOST_PLANES == 8 && HOST_WIDTH >= 320 && HOST_HEIGHT >= ATARI_HEIGHT) {
		/* current resolution is OK */
		vramw = HOST_WIDTH;
		vramh = HOST_HEIGHT;

/*
		if (vramw > 336)
			screenw = 336;
		else
*/
			screenw = 320;
		screenh = ATARI_HEIGHT;
	}
	else {
		if (video_hw == F030) {	/* we may switch VIDEL directly */
			/* save original VIDEL settings */
			p_str_p = (ULONG *) original_videl_settings;
			Supexec(save_r);

			if ((new_videoram = (UBYTE *)Mxalloc((336UL*ATARI_HEIGHT), 0)) == NULL) {
				form_alert(1, "[1][Error allocating video memory ][ OK ]");
				exit(-1);
			}

			/* create new graphics mode 336x240 in 256 colors */
			reprogram_VIDEL = 1;
			vramw = screenw = 336;
			vramh = screenh = ATARI_HEIGHT;
		}
		else { /* non-Falcon hardware */
			form_alert(1, "[1][Atari800 emulator needs |320x240 or higher res|in 256 colors][ OK ]");
			exit(-1);
		}
	}

	/* lock GEM */
	v_clrwk(gl_vdi_handle);		/* clear whole screen */
	wind_update(BEG_UPDATE);

	save_original_colors();

#ifdef CLOCKY_SCREENSAVER
	screensaverval = screensaver(0);	/* turn off Clocky's screen saver */
#endif

	Original_Log_base = Logbase();
	Original_Phys_base = Physbase();

	key_tab = Keytbl(-1, -1, -1);

	consol = 7;

#ifdef CPUASS
	CPU_INIT();
#endif

#ifdef DMASOUND
	Sound_Initialise(argc, argv);
#endif

	SetupEmulatedEnvironment();
}

/* -------------------------------------------------------------------------- */

int Atari_Exit(int run_monitor)
{
	ShutdownEmulatedEnvironment();

#ifdef BUFFERED_LOG
	Aflushlog();
#endif

	if (run_monitor) {
		if (monitor()) {
			SetupEmulatedEnvironment();

			return 1;			/* go back to emulation */
		}
	}

#ifdef DMASOUND
	Sound_Exit();
#endif

	if (new_videoram)
		free(new_videoram);

	/* unlock GEM */
	wind_update(END_UPDATE);
	form_dial(FMD_FINISH, 0, 0, 0, 0, 0, 0, HOST_WIDTH, HOST_HEIGHT);	/* redraw screen */
	graf_mouse(M_ON, NULL);
	/* GEM exit */
	appl_exit();

#ifdef CLOCKY_SCREENSAVER
	screensaver(screensaverval);
#endif

	return 0;
}

/* -------------------------------------------------------------------------- */

void Atari_DisplayScreen(UBYTE *screen)
{
	static int i = 0;

#ifdef SET_LED
	if (LED_timeout)
		if (--LED_timeout == 0)
			Atari_Set_LED(0);
#endif

	if (i < skip_N_frames) {
		i++;
		return;
	}
	i = 0;

	odkud = screen;
	kam = Logbase();

	if (video_hw == NOVA) {
		UBYTE *ptr_from = screen + 24;
		UBYTE *ptr_dest = kam + CENTER;
		int j;

		for(j=0; j<ATARI_HEIGHT; j++) {
			int cycles;
			long tmp=0, tmp2=0;

			if (NOVA_double_size) {
				cycles = 167;

				__asm__ __volatile__("\n\t\
				1:\n\t\
					movew	%3@+,%4\n\t\
					movew	%4,%5\n\t\
					swap	%4\n\t\
					movew	%5,%4\n\t\
					rorw	#8,%4\n\t\
					rorl	#8,%4\n\t\
					movel	%4,%2@+\n\t\

					dbra	%0,1b"
					: "=d" (cycles)
					: "0" (cycles), "a" (ptr_dest), "a" (ptr_from), "d" (tmp), "d" (tmp2)
				);
				ptr_dest += HOST_WIDTH;	/* odd lines only */
			}
			else {
				cycles = 20;
				__asm__ __volatile__("\n\t\
				1:\n\t\
					movel	%3@+,%2@+\n\t\
					movel	%3@+,%2@+\n\t\
					movel	%3@+,%2@+\n\t\
					movel	%3@+,%2@+\n\t\
					dbra	%0,1b"
					: "=d" (cycles)
					: "0" (cycles), "a" (ptr_dest), "a" (ptr_from)
				);
			}
			ptr_from += ATARI_WIDTH-336;
			ptr_dest += HOST_WIDTH-EMUL_WIDTH;
		}
	}
	else {
		rplanes();
	}
}

#ifdef SET_LED
void Atari_Set_LED(int how)
{
	if (how) {
		Offgibit(~0x02);
		LED_timeout = 8;
	}
	else {
		Ongibit(0x02);
		LED_timeout = 0;
	}
}
#endif

/* -------------------------------------------------------------------------- */

extern int SHIFT_KEY, KEYPRESSED;

int Atari_Keyboard(void)
{
	UBYTE control_key;
	int scancode, keycode;
	int i;

	trig0 = 1;
	stick0 = STICK_CENTRE;

	SHIFT_KEY = (key_buf[0x2a] || key_buf[0x36]);
	control_key = key_buf[0x1d];

	if (!SHIFT_KEY && !control_key) {
		if (key_buf[0x70])
			trig0 = 0;
		if (key_buf[0x6d] || key_buf[0x6e] || key_buf[0x6f])
			stick0 -= (STICK_CENTRE - STICK_BACK);
		if (key_buf[0x6f] || key_buf[0x6c] || key_buf[0x69])
			stick0 -= (STICK_CENTRE - STICK_RIGHT);
		if (key_buf[0x6d] || key_buf[0x6a] || key_buf[0x67])
			stick0 -= (STICK_CENTRE - STICK_LEFT);
		if (key_buf[0x67] || key_buf[0x68] || key_buf[0x69])
			stick0 -= (STICK_CENTRE - STICK_FORWARD);
	}

	scancode = 0;

	if (stick0 == STICK_CENTRE && trig0 == 1) {
		for (i = 1; i <= 0x72; i++)		/* test vsech klaves postupne */
			if (key_buf[i]) {
				if (i == 0x1d || i == 0x2a || i == 0x36)	/* prerazovace preskoc */
					continue;
				scancode = i;
				break;
			}
	}

	if (scancode) {

		/* precti stisknutou klavesu */
		if (SHIFT_KEY)
			keycode = *(UBYTE *) (key_tab->shift + scancode);
		else
			keycode = *(UBYTE *) (key_tab->unshift + scancode);

		if (control_key)
			keycode -= 64;

		switch (keycode) {
		case 0x01:
			keycode = AKEY_CTRL_a;
			break;
		case 0x02:
			keycode = AKEY_CTRL_b;
			break;
		case 0x03:
			keycode = AKEY_CTRL_c;
			break;
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
			if (scancode == 0x0e)
				keycode = AKEY_BACKSPACE;
			else
				keycode = AKEY_CTRL_h;
			break;
		case 0x09:
			if (scancode == 0x0f) {
				if (SHIFT_KEY)
					keycode = AKEY_SETTAB;
				else if (control_key)
					keycode = AKEY_CLRTAB;
				else
					keycode = AKEY_TAB;
			}
			else
				keycode = AKEY_CTRL_i;
			break;
		case 0x0a:
			keycode = AKEY_CTRL_j;
			break;
		case 0x0b:
			keycode = AKEY_CTRL_k;
			break;
		case 0x0c:
			keycode = AKEY_CTRL_l;
			break;
		case 0x0d:
			if (scancode == 0x1c || scancode == 0x72)
				keycode = AKEY_RETURN;
			else
				keycode = AKEY_CTRL_m;
			break;
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
		case 0x1b:
			keycode = AKEY_ESCAPE;
			break;
		case 0x00:
			switch (scancode) {
			case 0x3b:			/* F1 */
			case 0x61:			/* Undo */
				keycode = AKEY_UI;
				break;
			case 0x62:			/* Help */
				keycode = AKEY_HELP;
				break;
			case 0x3c:			/* F2 */
				consol &= 0x03;
				keycode = AKEY_NONE;
				break;
			case 0x3d:			/* F3 */
				consol &= 0x05;
				keycode = AKEY_NONE;
				break;
			case 0x3e:			/* F4 */
				consol &= 0x06;
				keycode = AKEY_NONE;
				break;
			case 0x3f:			/* F5 */
				keycode = SHIFT_KEY ? AKEY_COLDSTART : AKEY_WARMSTART;
				break;
			case 0x40:			/* F6 - used to be PILL mode switch */
				/* keycode = AKEY_PIL; */
				keycode = AKEY_SCREENSHOT;
				break;
			case 0x41:			/* F7 */
				keycode = AKEY_BREAK;
				break;
			case 0x42:			/* F8 */
				keycode = Atari_Exit(1) ? AKEY_NONE : AKEY_EXIT;	/* invoke monitor */
				break;
			case 0x43:			/* F9 */
				keycode = AKEY_EXIT;
				break;
			case 0x50:
				keycode = AKEY_DOWN;
				break;
			case 0x4b:
				keycode = AKEY_LEFT;
				break;
			case 0x4d:
				keycode = AKEY_RIGHT;
				break;
			case 0x48:
				keycode = AKEY_UP;
				break;
			default:
				keycode = AKEY_NONE;
				break;
			}
			break;
		default:
			keycode = AKEY_NONE;
			break;
		}
	}
	else
		keycode = AKEY_NONE;

	KEYPRESSED = (keycode != AKEY_NONE);

	return keycode;
}

/* -------------------------------------------------------------------------- */

int Atari_PORT(int num)
{
	if (num == 0) {
		if (stick0 == STICK_CENTRE && trig0 == 1)
			return (((~joy1 << 4) & 0xf0) | ((~joy0) & 0x0f));
		else {
			if (joyswap)
				return ((stick0 << 4) | ((~joy0) & 0x0f));
			else
				return (((~joy0 << 4) & 0xf0) | stick0);
		}
	}
	else
		return 0xff;
}

/* -------------------------------------------------------------------------- */

int Atari_TRIG(int num)
{
	switch (num) {
	case 0:
		return (joy0 > 0x0f) ? 0 : joyswap ? 1 : trig0;
	case 1:
		return (joy1 > 0x0f) ? 0 : joyswap ? trig0 : 1;
	case 2:
	case 3:
	default:
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
