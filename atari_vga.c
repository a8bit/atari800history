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

#ifdef AT_USE_ALLEGRO
#include <allegro.h>
#endif

/* -------------------------------------------------------------------------- */

#define FALSE 0
#define TRUE 1

static int consol;
static int trig0;
static int stick0;
static int trig1;
static int stick1;
static int LPTjoy_addr[3] =
{0, 0, 0};
static int LPTjoy_num = 0;
static int joyswap = FALSE;

static int first_lno = 24;
static int first_col = 32;
static int ypos_inc = 1;
static int vga_ptr_inc = 320;
static int scr_ptr_inc = ATARI_WIDTH;

static int video_mode=0;
#ifdef AT_USE_ALLEGRO
  BITMAP *scr_bitmap;
  UBYTE *dark_memory;
  void *scr_data=NULL;
#endif

static int vga_started = 0;		/*AAA needed for DOS to see text */
UBYTE POT[2];
extern int SHIFT;
extern int ATKEYPRESSED;

static int SHIFT_LEFT = FALSE;
static int SHIFT_RIGHT = FALSE;
static int norepkey = FALSE;	/* for "no repeat" of some keys !RS! */

static int PC_keyboard = TRUE;

#ifdef BACKUP_MSG
extern char backup_msg_buf[300];
#endif

static int joy_in = FALSE;

#ifndef AT_USE_ALLEGRO_JOY
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
#endif							/* AT_USE_ALLEGRO_JOY */

int test_LPTjoy(int portno)
{
	int addr;

	if (portno < 1 || portno > 3)
		return FALSE;
	addr = _farpeekw(_dos_ds, (portno - 1) * 2 + 0x408);
	if (addr == 0)
		return FALSE;

	if (LPTjoy_num >= 3)
		return FALSE;
	LPTjoy_addr[LPTjoy_num++] = addr;

	return TRUE;
}

void read_LPTjoy(int LPTidx, int joyport)
{
	int state = STICK_CENTRE, trigger = 1, val;

	val = inportb(LPTjoy_addr[LPTidx] + 1);
	if (!(val & (1 << 4)))
		state &= STICK_FORWARD;
	else if (!(val & (1 << 5)))
		state &= STICK_BACK;
	if (!(val & (1 << 6)))
		state &= STICK_RIGHT;
	else if ((val & (1 << 7)))
		state &= STICK_LEFT;

	if (!(val & (1 << 3)))
		trigger = 0;

	if (joyport == 0) {
		stick0 = state;
		trig0 = trigger;
	}
	else if (joyport == 1) {
		stick1 = state;
		trig1 = trigger;
	}
}

/* -------------------------------------------------------------------------- */
#ifdef AT_USE_ALLEGRO
void Map_bitmap(BITMAP *bitmap,void *memory,int width,int height)
{
  int i;
  
  bitmap->dat=memory;
  bitmap->line[0]=memory;
  for (i=1;i<height;i++)
    bitmap->line[i]=bitmap->line[i-1]+width;
}
void Map_i_bitmap(BITMAP *bitmap,void *memory,void *memory2,int width,int height)
{
  int i;
  
  bitmap->dat=memory;
  bitmap->line[0]=memory;
  bitmap->line[1]=memory2;
  for (i=2;i<height;i+=2)
  {
    bitmap->line[i]=bitmap->line[i-2]+width;
    bitmap->line[i+1]=bitmap->line[i-1]+width;
  }
}
void make_darker(void *target,void *source,int bytes)
{
  __asm__ ("
          movl %0,%%edi
          movl %1,%%esi
          movl %2,%%ecx
          subl $4,%%edi
          shr $2,%%ecx
0:
          movl (%%esi),%%eax
          movl %%eax,%%ebx
          andl  $0xf0f0f0f0,%%eax
          shrl $1,%%ebx
          addl $4,%%esi
          addl $4,%%edi
          andl $0x07070707,%%ebx
          orl %%ebx,%%eax
          decl %%ecx
          movl %%eax,(%%edi)
          jne 0b
    "
    :
    : "g" (target), "g" (source), "g" (bytes)
    : "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi"
  );
}
#else
/* Procedure for initializing X-mode. Possible modes:
 *		0	320x175			6	360x175
 *		1	320x200			7	360x200
 *		2	320x240			8	360x240
 *		3	320x350			9	360x350
 *		4	320x400			a	360x400
 *		5	320x480			b	360x480
 *
 * This procedure is based on Guru mode by Per Ole Klemetsrud
 */
UWORD x_regtab[]={0x0e11,0x0014,0xe317,0x6b00,0x5901,
                  0x5a02,0x8e03,0x5e04,0x8a05,0x2d13};
UWORD x_vert480[]={0x4009,0x3e07,0x0b06,0xea10,0x8c11,
                   0xe715,0x0416,0xdf12,0xdf12};
UWORD x_vert350[]={0x4009,0x1f07,0xbf06,0x8310,0x8511,
                   0x6315,0xba16,0x5d12,0x5d12};
UBYTE x_clocktab[]={0xa3,0x0,0xe3,0xa3,0x0,0xe3,0xa7,0x67,
                    0xe7,0xa7,0x67,0xe7};
UWORD *x_verttab[]={x_vert350+1,(void*)0,x_vert480+1,x_vert350,(void*)1,x_vert480,
                   x_vert350+1,(void*)0,x_vert480+1,x_vert350,(void*)1,x_vert480};

UBYTE x_open(UWORD mode)
{
  __asm__ __volatile__("
     pushw %%es
     movw $0x1a00,%%ax
     int $0x10
     cmpb $0x1a,%%al
     je x_VGApresent
     movb $0,%%al
     jmp x_exit
x_VGApresent:
     movb $0,%%al
     movzwl %0,%%ebx
     cmpw $0xb,%%bx
     jg x_exit

     movw $0x0013,%%ax
     int $0x10

     movw %1,%%es
     movw $0x03c4,%%dx
     movb _x_clocktab(%%ebx),%%cl
     orb %%cl,%%cl
     je x_Skip_Reset

     movw $0x100,%%ax
     outw %%ax,%%dx

     movw $0x3c2,%%dx
     movb %%cl,%%al
     outb %%al,%%dx

     movw $0x3c4,%%dx
     movw $0x300,%%ax
     outw %%ax,%%dx
x_Skip_Reset:
     movw $0x604,%%ax
     outw %%ax,%%dx
     
     cld
     movl $0xa0000,%%di
     xorl %%eax,%%eax
     mov $0x8000,%%cx
     rep
     stosw

     movw $0x3d4,%%dx
     xorl %%esi,%%esi
     movl %%esi,%%ecx
     movw $3,%%cx
     cmpb $5,%%bl
     jbe x_CRT1
     movw $10,%%cx
x_CRT1:
     movw _x_regtab(,%%esi,2),%%ax
     outw %%ax,%%dx
     incl %%esi
     decw %%cx
     jne x_CRT1

     movl _x_verttab(,%%ebx,4),%%esi
     cmpl $0,%%esi
     je x_noerr
     cmpl $1,%%esi
     je x_Set400

     movw $8,%%cx
x_CRT2:
     movw (%%esi),%%ax
     outw %%ax,%%dx
     incl %%esi
     incl %%esi
     decl %%ecx
     jne x_CRT2
     jmp x_noerr

x_Set400:
     mov $0x4009,%%ax
     out %%ax,%%dx
x_noerr:
     xorb %%al,%%al
x_exit:
     popw %%es
     "
     :
     : "g" (mode), "c" (_dos_ds)
     : "%esi", "%eax", "%ebx", "%ecx", "%edx"
     );
}
/* Procedure for copying atari screen to x-mode videomemory */
void x_blit(void *mem,UBYTE lines,ULONG change,ULONG videoline)
{ 
  asm volatile("
     movl %0,%%esi
     pushw %%es
     movw %1,%%es
     subl $4,%3	

     movw $0x102,%%bx
     movw $0x3c4,%%dx
     cld
     
     movb %2,%%ch 
     movl $0xa0000,%%edi
.ALIGN 4
__xb_lloop:
     movb $0x11,%%bh
__xb_mloop:
     movw %%bx,%%ax
     outw %%ax,%%dx 	
     movb $20,%%cl
.ALIGN 4     
__xb_loop:
     movb (%%esi),%%al
     movb 4(%%esi),%%ah
     shrd $16,%%eax,%%eax
     movb 8(%%esi),%%al
     movb 12(%%esi),%%ah
     shrd $16,%%eax,%%eax
     addl $16,%%esi
     stosl		

     dec %%cl
     jne __xb_loop

     subl $319,%%esi
     subl $80,%%edi
     shlb $1,%%bh
     cmpb $0x10,%%bh
     jne __xb_mloop

     addl %3,%%esi 
     addl %4,%%edi
     dec %%ch
     jne __xb_lloop

     popw %%es
    "
    :
    : "b" (mem), "a"(_dos_ds), "g" (lines), "m" (change), "m" (videoline)
    : "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi"
  );
}

void x_blit_i2(void *mem,UBYTE lines,ULONG change,ULONG videoline)
{ 
  asm volatile("
     movl %0,%%esi
     pushw %%es
     movw %1,%%es
     subl $4,%3	

     movw $0x102,%%bx
     movw $0x3c4,%%dx
     cld
     
     movb %2,%%ch 
     movl $0xa0000,%%edi
.ALIGN 4
__x2_lloop:
     movb $0x11,%%bh
__x2_mloop:
     movw %%bx,%%ax
     movw $0x3c4,%%dx
     outw %%ax,%%dx 	
     movb $20,%%cl
.ALIGN 4     
__x2_loop:
     movb (%%esi),%%al
     movb 4(%%esi),%%ah
     shrd $16,%%eax,%%eax
     movb 8(%%esi),%%al
     movb 12(%%esi),%%ah
     shrd $16,%%eax,%%eax
     addl $16,%%esi
     movl %%eax,%%edx
     stosl
     shr $1,%%edx 
     and $0xf0f0f0f0,%%eax
     and $0x07070707,%%edx
     or %%edx,%%eax
     .BYTE 0x26
     movl %%eax,76(%%edi)

     dec %%cl
     jne __x2_loop

     subl $319,%%esi
     subl $80,%%edi
     shlb $1,%%bh
     cmpb $0x10,%%bh
     jne __x2_mloop

     addl %3,%%esi 
     addl %4,%%edi
     dec %%ch
     jne __x2_lloop

     popw %%es
    "
    :
    : "b" (mem), "a"(_dos_ds), "g" (lines), "m" (change), "m" (videoline)
    : "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi"
  );
}
#endif

/* -------------------------------------------------------------------------- */

unsigned char raw_key;

_go32_dpmi_seginfo old_key_handler, new_key_handler;

static int key_status[10] =
{1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
extern int SHIFT_KEY, KEYPRESSED;
static int control = FALSE;
static int extended_key_follows = FALSE;

void key_handler(void)
{
	asm("cli; pusha");
	raw_key = inportb(0x60);

	if (!extended_key_follows) {	/* evaluate just numeric keypad! */
		switch (raw_key & 0x7f) {
		case 0x52:				/* 0 */
			key_status[0] = raw_key & 0x80;
			break;
		case 0x4f:				/* 1 */
			key_status[1] = raw_key & 0x80;
			break;
		case 0x50:				/* 2 */
			if (control) {
				if ((raw_key < 0x80) && (first_lno < 40))
					first_lno++;	/* move VGA screen down */
				raw_key = 0;
			}
			else
				key_status[2] = raw_key & 0x80;
			break;
		case 0x51:				/* 3 */
			key_status[3] = raw_key & 0x80;
			break;
		case 0x4b:				/* 4 */
			if (control) {
				if ((raw_key < 0x80) && (first_col > 24))
					first_col--;	/* move VGA screen left */
				raw_key = 0;
			}
			else
				key_status[4] = raw_key & 0x80;
			break;
		case 0x4c:				/* 5 */
			if (control && (raw_key < 0x80)) {
				first_lno = 24;	/* center of 320x200 VGA screen */
				first_col = 32;
				raw_key = 0;
			}
			else
				key_status[5] = raw_key & 0x80;
			break;
		case 0x4d:				/* 6 */
			if (control) {
				if ((raw_key < 0x80) && (first_col < 40))
					first_col++;	/* move VGA screen right */
				raw_key = 0;
			}
			else
				key_status[6] = raw_key & 0x80;
			break;
		case 0x47:				/* 7 */
			key_status[7] = raw_key & 0x80;
			break;
		case 0x48:				/* 8 */
			if (control) {
				if ((raw_key < 0x80) && (first_lno))
					first_lno--;	/* move VGA screen up */
				raw_key = 0;
			}
			else
				key_status[8] = raw_key & 0x80;
			break;
		case 0x49:				/* 9 */
			key_status[9] = raw_key & 0x80;
			break;
		}
	}

	extended_key_follows = FALSE;

	switch (raw_key) {
	case 0x2a:
		SHIFT_LEFT = TRUE;
		break;
	case 0x36:
		SHIFT_RIGHT = TRUE;
		break;
	case 0xaa:
		SHIFT_LEFT = FALSE;
		break;
	case 0xb6:
		SHIFT_RIGHT = FALSE;
		break;
	case 0x1d:
		control = TRUE;
		break;
	case 0x9d:
		control = FALSE;
		break;
	case 0x3c:
		consol &= 0x03;			/* OPTION key ON */
		break;
	case 0xbc:					/* OPTION key OFF */
		consol |= 0x04;
		break;
	case 0x3d:
		consol &= 0x05;			/* SELECT key ON */
		break;
	case 0xbd:					/* SELECT key OFF */
		consol |= 0x02;
		break;
	case 0x3e:
		consol &= 0x06;			/* START key ON */
		break;
	case 0xbe:					/* START key OFF */
		consol |= 0x01;
		break;
	case 0xe0:
		extended_key_follows = TRUE;
	}

	SHIFT_KEY = SHIFT_LEFT | SHIFT_RIGHT;

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

#ifdef AT_USE_ALLEGRO
        switch(video_mode)
        {
	  case 0:
	    set_gfx_mode(GFX_VGA, 320, 200, 0, 0);
            break;
          case 1:
            set_gfx_mode(GFX_AUTODETECT,320,240,0,0);
 	    scr_bitmap=create_bitmap(ATARI_WIDTH,ATARI_HEIGHT);
	    scr_data=scr_bitmap->dat;
            break;
          case 2:
            set_gfx_mode(GFX_AUTODETECT,320,480,0,0);
 	    scr_bitmap=create_bitmap(ATARI_WIDTH,ATARI_HEIGHT*2);
	    scr_data=scr_bitmap->dat;
            dark_memory=(UBYTE*)malloc(ATARI_WIDTH*ATARI_HEIGHT);
            memset(dark_memory,0,ATARI_WIDTH*ATARI_HEIGHT);
            break;
          case 3:
            set_gfx_mode(GFX_AUTODETECT,320,480,0,0);
 	    scr_bitmap=create_bitmap(ATARI_WIDTH,ATARI_HEIGHT*2);
	    scr_data=scr_bitmap->dat;
            dark_memory=(UBYTE*)malloc(ATARI_WIDTH*ATARI_HEIGHT);
            break;
        }
#else
        switch(video_mode)
        {
          case 0:
	    rg.x.ax = 0x0013;
	    int86(0x10, &rg, &rg);
            break;
          case 1:
            x_open(2);
            break;
          case 2:
          case 3:
            x_open(5);
            break;
        }
#endif
	vga_started = 1;

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

void ShutdownVgaEnvironment()
{
	union REGS rg;

	if (vga_started) {
		rg.x.ax = 0x0003;
		int86(0x10, &rg, &rg);
#ifdef AT_USE_ALLEGRO
		switch(video_mode)
		{
		  case 1:
 	            Map_bitmap(scr_bitmap,scr_data,ATARI_WIDTH,ATARI_HEIGHT);
	            destroy_bitmap(scr_bitmap);
	            break;
	          case 2:
	          case 3:
 	            Map_bitmap(scr_bitmap,scr_data,ATARI_WIDTH,ATARI_HEIGHT*2);
	            destroy_bitmap(scr_bitmap);
                    free(dark_memory);
	            break;
                }
#endif
	}
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
		else if (strcmp(argv[i], "-LPTjoy1") == 0)
			test_LPTjoy(1);
		else if (strcmp(argv[i], "-LPTjoy2") == 0)
			test_LPTjoy(2);
		else if (strcmp(argv[i], "-LPTjoy3") == 0)
			test_LPTjoy(3);

		else if (strcmp(argv[i], "-joyswap") == 0)
			joyswap = TRUE;
			
		else if (strcmp(argv[i], "-video") == 0)	
		{
		  i++;
		  video_mode=atoi(argv[i]);
		  if (video_mode<0 || video_mode>3) 
		  {
		    printf("Invalid video mode, using default.\n");
		    getchar();
		    video_mode=0;
		  }
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				printf("\t-interlace    Generate screen with interlace\n");
				printf("\t-LPTjoy1      Read joystick connected to LPT1\n");
				printf("\t-LPTjoy2      Read joystick connected to LPT2\n");
				printf("\t-LPTjoy3      Read joystick connected to LPT3\n");
				printf("\t-joyswap      Swap joysticks\n");
				printf("\t-video x          set video mode:\n");
				printf("\t\t0 - 320x200\n\t\t1 - 320x240\n");
				printf("\t\t2 - 320x240, interlaced with black lines\n");
				printf("\t\t3 - 320x240, interlaced with darker lines (slower!)\n");
				printf("\nPress Return/Enter to continue...");
				getchar();
				printf("\r                                 \n");
			}
			argv[j++] = argv[i];
		}
	}

	*argc = j;

#ifdef AT_USE_ALLEGRO
	allegro_init();
#endif

	/* initialise sound routines */
#ifndef USE_DOSSOUND
	Sound_Initialise(argc, argv);
#else
	if (dossound_Initialise(argc, argv))
		exit(1);
#endif

	/* check if joystick is connected */
	printf("Joystick is checked...");
	fflush(stdout);
#ifndef AT_USE_ALLEGRO_JOY
	outportb(0x201, 0xff);
	usleep(100000UL);
	joy_in = (inportb(0x201) == 0xfc);
	if (joy_in)
		joystick0(&js0_centre_x, &js0_centre_y);
#else
	joy_in = (initialise_joystick() == 0 ? TRUE : FALSE);
#endif
	if (joy_in)
		printf(" found!\n");
	else
		printf("\n\nSorry, I see no joystick. Use numeric pad\n");

#ifndef DELAYED_VGAINIT
	SetupVgaEnvironment();
#endif

	/* setup joystick */
	stick0 = stick1 = STICK_CENTRE;
	trig0 = trig1 = 1;

	consol = 0x07;				/* OPTION, SELECT, START - OFF */
}

/* -------------------------------------------------------------------------- */

int Atari_Exit(int run_monitor)
{
	/* restore to text mode */
        ShutdownVgaEnvironment();
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

#ifndef USE_DOSSOUND
	Sound_Exit();
#endif

	return 0;
}

/* -------------------------------------------------------------------------- */

void Atari_DisplayScreen(UBYTE * ascreen)
 {
	static int lace = 0;
	unsigned long vga_ptr;

	UBYTE *scr_ptr;
	int ypos;
	int x;
#ifdef AT_USE_ALLEGRO_COUNTER
	static char speedmessage[200];
	extern int speed_count;
#endif
	vga_ptr = 0xa0000;
	scr_ptr = &ascreen[first_lno * ATARI_WIDTH + first_col];

	if (ypos_inc == 2) {
		if (lace) {
			vga_ptr += 320;
			scr_ptr += ATARI_WIDTH;
		}
		lace = 1 - lace;
	}
#ifdef AT_USE_ALLEGRO
        switch(video_mode)
        {
          case 0:
	     /*_farsetsel(_dos_ds);*/
	    for (ypos = 0; ypos < 200; ypos += ypos_inc) {
		_dosmemputl(scr_ptr, 320 / 4, vga_ptr);

		vga_ptr += vga_ptr_inc;
		scr_ptr += scr_ptr_inc;
	    }
            break;
          case 1:
            if (screen!=scr_bitmap->dat)
              Map_bitmap(scr_bitmap,ascreen,ATARI_WIDTH,ATARI_HEIGHT);
            blit(scr_bitmap,screen,first_col,0,0,0,320,240);
            break;
          case 2:
            if (screen!=scr_bitmap->dat)
            Map_i_bitmap(scr_bitmap,ascreen,dark_memory,ATARI_WIDTH,ATARI_HEIGHT*2);
            blit(scr_bitmap,screen,first_col,0,0,0,320,480);
            break;
          case 3:
            if (screen!=scr_bitmap->dat)
              Map_i_bitmap(scr_bitmap,ascreen,dark_memory,ATARI_WIDTH,ATARI_HEIGHT*2);
            make_darker(dark_memory,ascreen,ATARI_WIDTH*ATARI_HEIGHT);
            blit(scr_bitmap,screen,first_col,0,0,0,320,480);
            break;
        }
#else
        switch(video_mode)
        {
          case 0:
	     /*_farsetsel(_dos_ds);*/
	    for (ypos = 0; ypos < 200; ypos += ypos_inc) {
		_dosmemputl(scr_ptr, 320 / 4, vga_ptr);

		vga_ptr += vga_ptr_inc;
		scr_ptr += scr_ptr_inc;
	    }
            break;
          case 1:
            x_blit(ascreen+first_col,240,ATARI_WIDTH,80);
            break;
          case 2:
            x_blit(ascreen+first_col,240,ATARI_WIDTH,160);
            break;
          case 3:
            x_blit_i2(ascreen+first_col,240,ATARI_WIDTH,160);
            break;
        }
#endif

#ifdef AT_USE_ALLEGRO_COUNTER
	sprintf(speedmessage, "%d", speed_count);
	textout((BITMAP *) screen, font, speedmessage, 1, 1, 10);
	speed_count = 0;
#endif
#ifdef USE_DOSSOUND
	dossound_UpdateSound();
#endif
	/* consol = 7; */
}

/* -------------------------------------------------------------------------- */

int Atari_Keyboard(void)
{
	int keycode;
	int shift_mask;
	static int stillpressed;	/* is 5200 button 2 still pressed */

	extern int ui_is_active;	/* to workaround a problem in 5200 stuff */

#ifdef AT_USE_ALLEGRO_JOY
	extern volatile int joy_left, joy_right, joy_up, joy_down;
	extern volatile int joy_b1, joy_b2, joy_b3, joy_b4;
#endif

/*
 * Trigger and Joystick handling should
 * be moved into the Keyboard Interrupt Routine
 */

	if (key_status[0])
		trig0 = 1;
	else
		trig0 = 0;

	stick0 = STICK_CENTRE;
	if (!key_status[1] || !key_status[4] || !key_status[7])
		stick0 &= 0xfb;
	if (!key_status[3] || !key_status[6] || !key_status[9])
		stick0 &= 0xf7;
	if (!key_status[1] || !key_status[2] || !key_status[3])
		stick0 &= 0xfd;
	if (!key_status[7] || !key_status[8] || !key_status[9])
		stick0 &= 0xfe;

	if (stick0 != STICK_CENTRE || trig0 != 1)
		raw_key = 0;			/* do not evaluate the pressed key */

	/* if joy_in then keyboard emulates stick1 instead of standard stick0 */
	if (joy_in) {
		stick1 = stick0;
		trig1 = trig0;
#ifndef AT_USE_ALLEGRO_JOY
		read_joystick(js0_centre_x, js0_centre_y);	/* read real PC joystick */
#else
		poll_joystick();
		stick0 = ((joy_right << 3) | (joy_left << 2) | (joy_down << 1) | joy_up) ^ 0x0f;
		trig0 = !joy_b1;
#endif
	}

	/* read LPT joysticks */
	if (LPTjoy_num) {
		if (joy_in)
			read_LPTjoy(0, 1);
		else {
			read_LPTjoy(0, 0);
			if (LPTjoy_num >= 2)
				read_LPTjoy(1, 1);
		}
	}

	/* if '-joyswap' then joy0 and joy1 are exchanged */
	if (joyswap) {
		int stick = stick1;
		int trig = trig1;
		stick1 = stick0;
		trig1 = trig0;
		stick0 = stick;
		trig0 = trig;
	}

/*
 * This needs a bit of tidying up - array lookup
 */

/* Atari5200 stuff */
	if (machine == Atari5200 && !ui_is_active) {
		POT[0] = 120;
		POT[1] = 120;
		if (!(stick0 & 0x04))
			POT[0] = 15;
		if (!(stick0 & 0x01))
			POT[1] = 15;
		if (!(stick0 & 0x08))
			POT[0] = 220;
		if (!(stick0 & 0x02))
			POT[1] = 220;

		switch (raw_key) {
		case 0x3b:
			keycode = AKEY_UI;
			break;
		case 0x3f:				/* F5 */
			/* if (SHIFT_KEY) */
			keycode = AKEY_COLDSTART;	/* 5200 has no warmstart */
			/*  else
			   keycode = AKEY_WARMSTART;
			 */

			break;
		case 0x43:
			keycode = AKEY_EXIT;
			break;
		case 0x02:
			keycode = 0x3f;
			break;
		case 0x03:
			keycode = 0x3d;
			break;
		case 0x04:
			keycode = 0x3b;
			break;
		case 0x0D:
			keycode = 0x23;		/* = = * */
			break;
		case 0x05:
			keycode = 0x37;
			break;
		case 0x06:
			keycode = 0x35;
			break;
		case 0x07:
			keycode = 0x33;
			break;
		case 0x08:
			keycode = 0x2f;
			break;
		case 0x09:
			keycode = 0x2d;
			break;
		case 0x0C:
			keycode = 0x27;		/* - = * */
			break;
		case 0x0a:
			keycode = 0x2b;
			break;
		case 0x0b:
			keycode = 0x25;
			break;
		case 0x3e:				/* 1f : */
			keycode = 0x39;		/* start */
			break;
		case 0x19:
			keycode = 0x31;		/* pause */
			break;
		case 0x13:
			keycode = 0x29;		/* reset */
			break;
		case 0x42:				/* F8 */
			if (!norepkey) {
				keycode = Atari_Exit(1) ? AKEY_NONE : AKEY_EXIT;	/* invoke monitor */
				norepkey = TRUE;
			}
			else
				keycode = AKEY_NONE;
			break;
		case 0x44:				/* centre 320x200 VGA screen */
			if (control) {
				first_lno = 24;
				first_col = 32;
				keycode = AKEY_NONE;
			}
		case 0x50:
			if (control) {
				if (first_lno < 40)
					first_lno++;	/* move VGA screen down */
				keycode = AKEY_NONE;
			}
		case 0x4b:
			if (control) {
				if (first_col)
					first_col--;	/* move VGA screen left */
				keycode = AKEY_NONE;
			}
		case 0x4d:
			if (control) {
				if (first_col < 64)
					first_col++;	/* move VGA screen right */
				keycode = AKEY_NONE;
			}
		case 0x48:
			if (control) {
				if (first_lno)
					first_lno--;	/* move VGA screen up */
				keycode = AKEY_NONE;
			}
		default:
			keycode = AKEY_NONE;
			norepkey = FALSE;
			break;
		}
		KEYPRESSED = (keycode != AKEY_NONE);	/* for POKEY */
#ifdef AT_USE_ALLEGRO_JOY
		if (joy_b2) {
			if (stillpressed) {
				SHIFT_KEY = 1;
			}
			else {
				keycode = AKEY_BREAK;
				stillpressed = 1;
				SHIFT_KEY = 1;
				return keycode;
			}
		}
		else {
			stillpressed = 0;
			SHIFT_KEY = 0;
		}
#endif
		return keycode;
	}

	/* preinitialize of keycode */
	shift_mask = SHIFT_KEY ? 0x40 : 0;
	keycode = shift_mask | (control ? 0x80 : 0);

	switch (raw_key) {
	case 0x3b:					/* F1 */
		if (control) {
			PC_keyboard = TRUE;	/* PC keyboard mode (default) */
			keycode = AKEY_NONE;
		}
		else if (SHIFT_KEY) {
			PC_keyboard = FALSE;	/* Atari keyboard mode */
			keycode = AKEY_NONE;
		}
		else
			keycode = AKEY_UI;
		break;
	case 0x3f:					/* F5 */
		keycode = SHIFT_KEY ? AKEY_COLDSTART : AKEY_WARMSTART;
		break;
	case 0x40:					/* F6 */
		keycode = AKEY_PIL;
		break;
	case 0x41:					/* F7 */
	case 0x4F:					/* END key */
		if (!norepkey) {
			keycode = AKEY_BREAK;
			norepkey = TRUE;
		}
		else
			keycode = AKEY_NONE;
		break;
	case 0x42:					/* F8 */
		if (!norepkey) {
			keycode = Atari_Exit(1) ? AKEY_NONE : AKEY_EXIT;	/* invoke monitor */
			norepkey = TRUE;
		}
		else
			keycode = AKEY_NONE;
		break;
	case 0x43:					/* F9 */
		keycode = AKEY_EXIT;
		break;
	case 0x44:					/* F10 = HELP key */
		keycode |= AKEY_HELP;
		break;
	case 0x01:
		keycode |= AKEY_ESCAPE;
		break;
	case 0x50:
		if (PC_keyboard)
			keycode = AKEY_DOWN;
		else {
			keycode |= AKEY_EQUAL;
			keycode ^= shift_mask;
		}
		break;
	case 0x4b:
		if (PC_keyboard)
			keycode = AKEY_LEFT;
		else {
			keycode |= AKEY_PLUS;
			keycode ^= shift_mask;
		}
		break;
	case 0x4d:
		if (PC_keyboard)
			keycode = AKEY_RIGHT;
		else {
			keycode |= AKEY_ASTERISK;
			keycode ^= shift_mask;
		}
		break;
	case 0x48:
		if (PC_keyboard)
			keycode = AKEY_UP;
		else {
			keycode |= AKEY_MINUS;
			keycode ^= shift_mask;
		}
		break;
	case 0x29:					/* "`" key on top-left */
		keycode |= AKEY_ATARI;	/* Atari key (inverse video) */
		break;
	case 0x3a:
		keycode |= AKEY_CAPSTOGGLE;		/* Caps key */
		break;
	case 0x02:
		keycode |= AKEY_1;		/* 1 */
		break;
	case 0x03:
		if (!PC_keyboard)
			keycode |= AKEY_2;
		else if (SHIFT_KEY)
			keycode = AKEY_AT;
		else
			keycode |= AKEY_2;	/* 2,@ */
		break;
	case 0x04:
		keycode |= AKEY_3;		/* 3 */
		break;
	case 0x05:
		keycode |= AKEY_4;		/* 4 */
		break;
	case 0x06:
		keycode |= AKEY_5;		/* 5 */
		break;
	case 0x07:
		if (!PC_keyboard)
			keycode |= AKEY_6;
		else if (SHIFT_KEY)
			keycode = AKEY_CIRCUMFLEX;	/* 6,^ */
		else
			keycode |= AKEY_6;
		break;
	case 0x08:
		if (!PC_keyboard)
			keycode |= AKEY_7;
		else if (SHIFT_KEY)
			keycode = AKEY_AMPERSAND;	/* 7,& */
		else
			keycode |= AKEY_7;
		break;
	case 0x09:
		if (!PC_keyboard)
			keycode |= AKEY_8;
		else if (SHIFT_KEY)
			keycode = AKEY_ASTERISK;	/* 8,* */
		else
			keycode |= AKEY_8;
		break;
	case 0x0a:
		if (!PC_keyboard)
			keycode |= AKEY_9;
		else if (SHIFT_KEY)
			keycode = AKEY_PARENLEFT;
		else
			keycode |= AKEY_9;	/* 9,( */
		break;
	case 0x0b:
		if (!PC_keyboard)
			keycode |= AKEY_0;
		else if (SHIFT_KEY)
			keycode = AKEY_PARENRIGHT;	/* 0,) */
		else
			keycode |= AKEY_0;
		break;
	case 0x0c:
		if (!PC_keyboard)
			keycode |= AKEY_LESS;
		else if (SHIFT_KEY)
			keycode = AKEY_UNDERSCORE;
		else
			keycode |= AKEY_MINUS;
		break;
	case 0x0d:
		if (!PC_keyboard)
			keycode |= AKEY_GREATER;
		else if (SHIFT_KEY)
			keycode = AKEY_PLUS;
		else
			keycode |= AKEY_EQUAL;
		break;
	case 0x0e:
		keycode |= AKEY_BACKSPACE;
		break;


	case 0x0f:
		keycode |= AKEY_TAB;
		break;
	case 0x10:
		keycode |= AKEY_q;
		break;
	case 0x11:
		keycode |= AKEY_w;
		break;
	case 0x12:
		keycode |= AKEY_e;
		break;
	case 0x13:
		keycode |= AKEY_r;
		break;
	case 0x14:
		keycode |= AKEY_t;
		break;
	case 0x15:
		keycode |= AKEY_y;
		break;
	case 0x16:
		keycode |= AKEY_u;
		break;
	case 0x17:
		keycode |= AKEY_i;
		break;
	case 0x18:
		keycode |= AKEY_o;
		break;
	case 0x19:
		keycode |= AKEY_p;
		break;
	case 0x1a:
		if (!PC_keyboard)
			keycode |= AKEY_MINUS;
		else if (control | SHIFT_KEY)
			keycode |= AKEY_UP;
		else
			keycode = AKEY_BRACKETLEFT;
		break;
	case 0x1b:
		if (!PC_keyboard)
			keycode |= AKEY_EQUAL;
		else if (control | SHIFT_KEY)
			keycode |= AKEY_DOWN;
		else
			keycode = AKEY_BRACKETRIGHT;
		break;
	case 0x1c:
		keycode |= AKEY_RETURN;
		break;

	case 0x1e:
		keycode |= AKEY_a;
		break;
	case 0x1f:
		keycode |= AKEY_s;
		break;
	case 0x20:
		keycode |= AKEY_d;
		break;
	case 0x21:
		keycode |= AKEY_f;
		break;
	case 0x22:
		keycode |= AKEY_g;
		break;
	case 0x23:
		keycode |= AKEY_h;
		break;
	case 0x24:
		keycode |= AKEY_j;
		break;
	case 0x25:
		keycode |= AKEY_k;
		break;
	case 0x26:
		keycode |= AKEY_l;
		break;
	case 0x27:
		keycode |= AKEY_SEMICOLON;
		break;
	case 0x28:
		if (!PC_keyboard)
			keycode |= AKEY_PLUS;
		else if (SHIFT_KEY)
			keycode = AKEY_DBLQUOTE;
		else
			keycode = AKEY_QUOTE;
		break;
	case 0x2b:					/* PC key "\,|" */
	case 0x56:					/* PC key "\,|" */
		if (!PC_keyboard)
			keycode |= AKEY_ASTERISK;
		else if (SHIFT_KEY)
			keycode = AKEY_BAR;
		else
			keycode |= AKEY_BACKSLASH;
		break;


	case 0x2c:
		keycode |= AKEY_z;
		break;
	case 0x2d:
		keycode |= AKEY_x;
		break;
	case 0x2e:
		keycode |= AKEY_c;
		break;
	case 0x2f:
		keycode |= AKEY_v;
		break;
	case 0x30:
		keycode |= AKEY_b;
		break;
	case 0x31:
		keycode |= AKEY_n;
		break;
	case 0x32:
		keycode |= AKEY_m;
		break;
	case 0x33:
		if (!PC_keyboard)
			keycode |= AKEY_COMMA;
		else if (SHIFT_KEY && !control)
			keycode = AKEY_LESS;
		else
			keycode |= AKEY_COMMA;
		break;
	case 0x34:
		if (!PC_keyboard)
			keycode |= AKEY_FULLSTOP;
		else if (SHIFT_KEY && !control)
			keycode = AKEY_GREATER;
		else
			keycode |= AKEY_FULLSTOP;
		break;
	case 0x35:
		keycode |= AKEY_SLASH;
		break;
	case 0x39:
		keycode |= AKEY_SPACE;
		break;


	case 0x47:					/* HOME key */
		keycode |= 118;			/* clear screen */
		break;
	case 0x52:					/* INSERT key */
		if (SHIFT_KEY)
			keycode = AKEY_INSERT_LINE;
		else
			keycode = AKEY_INSERT_CHAR;
		break;
	case 0x53:					/* DELETE key */
		if (SHIFT_KEY)
			keycode = AKEY_DELETE_LINE;
		else
			keycode = AKEY_DELETE_CHAR;
		break;
	default:
		keycode = AKEY_NONE;
		norepkey = FALSE;
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
	if (machine == Atari5200) {
		if (num >= 0 && num < 2)
			return POT[num];
	}

	return 228;
}

/* -------------------------------------------------------------------------- */

int Atari_CONSOL(void)
{
	return consol;
}

/* -------------------------------------------------------------------------- */
