#include	<stdio.h>
#include	<stdlib.h>

#ifdef X11
#include	<X11/Xlib.h>
#include	<X11/Xutil.h>
#include	<X11/keysym.h>
#endif

#ifdef SVGALIB
#include	<vga.h>
#include	<vgagl.h>
#endif

#ifdef INTUITION
#include	<intuition/intuition.h>
#endif

#ifdef CURSES
#ifdef NCURSES
#include	<ncurses.h>
#else
#include	<curses.h>
#endif
extern int	curses_mode;
#endif

#include	"system.h"
#include	"cpu.h"
#include	"atari_custom.h"

#define	FALSE	0
#define	TRUE	1
#define	TITLE	"Atari 800 Emulator, Version 0.1.5"

#ifndef BASIC
#define	ATARI_WIDTH	(384)
#define	ATARI_HEIGHT	(192 + 24 + 24)
#endif

#ifdef X11
static Display	*display;
static Screen	*screen;
static Window	window;
static Pixmap	pixmap;
static Visual	*visual;

static XComposeStatus	keyboard_status;

static GC	gc;
static GC	gc_colour[256];
#endif

#ifdef INTUITION
static struct IntuitionBase	*IntuitionBase = NULL;
static struct GfxBase		*GfxBase = NULL;
static struct LayersBase	*LayersBase = NULL;
static struct Window		*WindowMain = NULL;
#endif

static int colortable[128] =
{

/* 0: Grey */

	0x000000, 0x222222, 0x444444, 0x666666,
	0x888888, 0xaaaaaa, 0xcccccc, 0xeeeeee,

/* 1: Gold */

	0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,
	0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,

/* 2: Orange */

	0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,
	0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,

/* 3: Red-Orange */

	0xff0000, 0xff2222, 0xff4444, 0xff6666,
	0xff8888, 0xffaaaa, 0xffcccc, 0xffeeee,

/* 4: Pink */

	0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,
	0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,

/* 5: Purple */

	0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,
	0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,

/* 6: Purple-Blue */

	0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,
	0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,

/* 7: Blue */

	0x0000ff, 0x2222ff, 0x4444ff, 0x6666ff,
	0x8888ff, 0xaaaaff, 0xccccff, 0xeeeeff,

/* 8: Blue */

	0x0000ff, 0x2222ff, 0x4444ff, 0x6666ff,
	0x8888ff, 0xaaaaff, 0xccccff, 0xeeeeff,

/* 9: Light-Blue */

	0x0000ff, 0x2222ff, 0x4444ff, 0x6666ff,
	0x8888ff, 0xaaaaff, 0xccccff, 0xeeeeff,

/* A: Turquoise */

	0x00ff00, 0x22ff22, 0x24be4d, 0x66ff66,
	0x88ff88, 0xaaffaa, 0xccffcc, 0xeeffee,

/* B: Green-Blue */

	0x00ff00, 0x22ff22, 0x44ff44, 0x66ff66,
	0x88ff88, 0xaaffaa, 0xccffcc, 0xeeffee,

/* C: Green */

	0x00ff00, 0x22ff22, 0x44ff44, 0x66ff66,
	0x88ff88, 0xaaffaa, 0xccffcc, 0xeeffee,

/* D: Yellow-Green */

	0x00ff00, 0x22ff22, 0x44ff44, 0x66ff66,
	0x88ff88, 0xaaffaa, 0xccffcc, 0xeeffee,

/* E: Orange-Green */

	0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,
	0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,

/* F: Light-Orange */

	0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,
	0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff
};

static int	colours[256];

/*
	=========================================
	Allocate variables for Hardware Registers
	=========================================
*/

static UBYTE	ALLPOT;
static UBYTE	AUDC1;
static UBYTE	AUDC2;
static UBYTE	AUDC3;
static UBYTE	AUDC4;
static UBYTE	AUDCTL;
static UBYTE	AUDF1;
static UBYTE	AUDF2;
static UBYTE	AUDF3;
static UBYTE	AUDF4;
static UBYTE	CHACTL;
static UBYTE	CHBASE;
static UBYTE	COLBK;
static UBYTE	COLPF0;
static UBYTE	COLPF1;
static UBYTE	COLPF2;
static UBYTE	COLPF3;
static UBYTE	COLPM0;
static UBYTE	COLPM1;
static UBYTE	COLPM2;
static UBYTE	COLPM3;
static UBYTE	CONSOL;
static UBYTE	DLISTH;
static UBYTE	DLISTL;
static UBYTE	DMACTL;
static UBYTE	GRACTL;
static UBYTE	GRAFM;
static UBYTE	GRAFP0;
static UBYTE	GRAFP1;
static UBYTE	GRAFP2;
static UBYTE	GRAFP3;
static UBYTE	HITCLR;
static UBYTE	HPOSM0;
static UBYTE	HPOSM1;
static UBYTE	HPOSM2;
static UBYTE	HPOSM3;
static UBYTE	HPOSP0;
static UBYTE	HPOSP1;
static UBYTE	HPOSP2;
static UBYTE	HPOSP3;
static UBYTE	HSCROL;
static UBYTE	IRQEN;
static UBYTE	IRQST;
static UBYTE	KBCODE;
static UBYTE	M0PF;
static UBYTE	M0PL;
static UBYTE	M1PF;
static UBYTE	M1PL;
static UBYTE	M2PF;
static UBYTE	M2PL;
static UBYTE	M3PF;
static UBYTE	M3PL;
static UBYTE	NMIEN;
static UBYTE	NMIRES;
static UBYTE	NMIST;
static UBYTE	P0PF;
static UBYTE	P0PL;
static UBYTE	P1PF;
static UBYTE	P1PL;
static UBYTE	P2PF;
static UBYTE	P2PL;
static UBYTE	P3PF;
static UBYTE	P3PL;
static UBYTE	PACTL;
static UBYTE	PAL;
static UBYTE	PBCTL;
static UBYTE	PENH;
static UBYTE	PENV;
static UBYTE	PMBASE;
static UBYTE	PORTA;
static UBYTE	PORTB;
static UBYTE	POT0;
static UBYTE	POT1;
static UBYTE	POT2;
static UBYTE	POT3;
static UBYTE	POT4;
static UBYTE	POT5;
static UBYTE	POT6;
static UBYTE	POT7;
static UBYTE	POTGO;
static UBYTE	PRIOR;
static UBYTE	RANDOM;
static UBYTE	SERIN;
static UBYTE	SEROUT;
static UBYTE	SIZEM;
static UBYTE	SIZEP0;
static UBYTE	SIZEP1;
static UBYTE	SIZEP2;
static UBYTE	SIZEP3;
static UBYTE	SKCTL;
static UBYTE	SKREST;
static UBYTE	SKSTAT;
static UBYTE	STIMER;
static UBYTE	TRIG0;
static UBYTE	TRIG1;
static UBYTE	TRIG2;
static UBYTE	TRIG3;
static UBYTE	VCOUNT;
static UBYTE	VDELAY;
static UBYTE	VSCROL;
static UBYTE	WSYNC;

extern UBYTE	*super;

#ifndef BASIC
static UBYTE	*image_data;
static UBYTE	*scanline_ptr;
#endif

UBYTE Atari800_GetByte (UWORD addr);
void  Atari800_PutByte (UWORD addr, UBYTE byte);
void  Atari800_Hardware (void);

void Atari800_Initialise ()
{
	int	i;

#ifdef X11
	XSetWindowAttributes	xswda;

	XGCValues	xgcvl;

	int	depth;

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

	xswda.event_mask = KeyPress | KeyRelease;

#ifdef DOUBLE_SIZE
	window = XCreateWindow (display,
			XRootWindowOfScreen(screen),
			50, 50,
			ATARI_WIDTH*2, ATARI_HEIGHT*2, 3, depth,
			InputOutput, visual,
			CWEventMask | CWBackPixel,
			&xswda);

	pixmap = XCreatePixmap (display, window, ATARI_WIDTH*2, ATARI_HEIGHT*2, depth);
#else
	window = XCreateWindow (display,
			XRootWindowOfScreen(screen),
			50, 50,
			ATARI_WIDTH, ATARI_HEIGHT, 3, depth,
			InputOutput, visual,
			CWEventMask | CWBackPixel,
			&xswda);

	pixmap = XCreatePixmap (display, window, ATARI_WIDTH, ATARI_HEIGHT, depth);
#endif

	XStoreName (display, window, TITLE);

	for (i=0;i<128;i++)
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

		colours[i+i] = colour.pixel;
		colours[i+i+1] = colour.pixel;
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

#ifdef DOUBLE_SIZE
	XFillRectangle (display, pixmap, gc, 0, 0, ATARI_WIDTH*2, ATARI_HEIGHT*2);
#else
	XFillRectangle (display, pixmap, gc, 0, 0, ATARI_WIDTH, ATARI_HEIGHT);
#endif

	XMapWindow (display, window);

	XSync (display, False);
#endif

#ifdef SVGALIB
	int	VGAMODE = G320x200x256;

	printf ("Initialising SVGALIB\n");

	vga_init ();

	printf ("Checkpoint\n");
	if (!vga_hasmode(VGAMODE))
	{
		printf ("Mode not available\n");
		exit (1);
	}

	vga_setmode (VGAMODE);

	gl_setcontextvga (VGAMODE);

	for (i=0;i<128;i++)
	{
		int	rgb = colortable[i];
		int	red;
		int	green;
		int	blue;

		red = (rgb & 0x00ff0000) >> 18;
		green = (rgb & 0x0000ff00) >> 10;
		blue = (rgb & 0x000000ff) >> 2;

		gl_setpalettecolor (i+i, red, green, blue);
		gl_setpalettecolor (i+i+1, red, green, blue);
	}
#endif

#ifdef INTUITION
	struct NewWindow	NewWindow;

	IntuitionBase = (struct IntuitionBase*) OpenLibrary ("intuition.library", 0);
	if (!IntuitionBase)
	{
		printf ("Failed to open intuition.library\n");
		exit (1);
	}

	GfxBase = (struct GfxBase*) OpenLibrary ("graphics.library", 39);
	if (!GfxBase)
	{
		printf ("Failed to open graphics.library\n");
		exit (1);
	}

	LayersBase = (struct LayersBase*) OpenLibrary ("layers.library", 0);
	if (!LayersBase)
	{
		printf ("Failed to open layers.library\n");
		exit (1);
	}

	NewWindow.LeftEdge = 0;
	NewWindow.TopEdge = 11;
	NewWindow.Width = ATARI_WIDTH + 8;
	NewWindow.Height = ATARI_HEIGHT + 13;
	NewWindow.DetailPen = 1;
	NewWindow.BlockPen = 2;
	NewWindow.IDCMPFlags = MENUPICK | GADGETUP;
	NewWindow.Flags = GIMMEZEROZERO | WINDOWDRAG | WINDOWDEPTH | SMART_REFRESH | ACTIVATE;
	NewWindow.FirstGadget = NULL;
	NewWindow.CheckMark = NULL;
	NewWindow.Title = TITLE;
	NewWindow.Screen = NULL;
	NewWindow.Type = WBENCHSCREEN;
	NewWindow.BitMap = NULL;
	NewWindow.MinWidth = 92;
	NewWindow.MinHeight = 65;
	NewWindow.MaxWidth = 1280;
	NewWindow.MaxHeight = 512;

	WindowMain = (struct Window*) OpenWindowTags
	(
		&NewWindow,
		WA_NewLookMenus, TRUE,
		WA_MenuHelp, TRUE,
		WA_ScreenTitle, TITLE,
		TAG_DONE
	);

	if (!WindowMain)
	{
		printf ("Failed to create window\n");
		exit (1);
	}
#endif

#ifdef CURSES
	initscr ();
	noecho ();
	cbreak ();		/* Don't wait for carriage return */
	keypad (stdscr, TRUE);
	curs_set (0);		/* Disable Cursor */
	nodelay (stdscr, 1);	/* Don't block for keypress */
#endif

/*
	============================
	Storage for Atari 800 Screen
	============================
*/

#ifndef BASIC
	image_data = (UBYTE*) malloc (ATARI_WIDTH * ATARI_HEIGHT);
	if (!image_data)
	{
		printf ("Failed to allocate space for image\n");
		exit (1);
	}
#endif

	for (i=0;i<65536;i++)
	{
		memory[i] = 0;
		attrib[i] = RAM;
	}
/*
	=======================================
	Install functions for CPU memory access
	=======================================
*/
	XGetByte = Atari800_GetByte;
	XPutByte = Atari800_PutByte;
	Hardware = Atari800_Hardware;
/*
	=============================
	Initialise Hardware Registers
	=============================
*/
	CONSOL = 0x07;
	PORTA = 0xff;
	PORTB = 0xff;
	TRIG0 = 1;
	TRIG1 = 1;
	TRIG2 = 1;
	TRIG3 = 1;

#ifndef SVGALIB
	printf ("CUSTOM: Initialised\n");
#endif
}

void Atari800_Exit ()
{
#ifdef X11
	free (image_data);

	XSync (display, True);

	XFreePixmap (display, pixmap);
	XUnmapWindow (display, window);
	XDestroyWindow (display, window);
	XCloseDisplay (display);
#endif

#ifdef SVGALIB
	vga_setmode (TEXT);
#endif

#ifdef INTUITION
	if (WindowMain)
	{
		CloseWindow (WindowMain);
	}

	if (LayersBase)
	{
		CloseLibrary (LayersBase);
	}

	if (GfxBase)
	{
		CloseLibrary (GfxBase);
	}

	if (IntuitionBase)
	{
		CloseLibrary (IntuitionBase);
	}
#endif

#ifdef CURSES
	curs_set (1);
	endwin ();
#endif
}

void SetRAM (int addr1, int addr2)
{
	int	i;

	for (i=addr1;i<=addr2;i++)
	{
		attrib[i] = RAM;
	}
}

void SetROM (int addr1, int addr2)
{
	int	i;

	for (i=addr1;i<=addr2;i++)
	{
		attrib[i] = ROM;
	}
}

void SetHARDWARE (int addr1, int addr2)
{
	int	i;

	for (i=addr1;i<=addr2;i++)
	{
		attrib[i] = HARDWARE;
	}
}

/*
	*****************************************************************
	*								*
	*	Section			:	Scanline Generation	*
	*	Original Author		:	David Firth		*
	*	Date Written		:	28th May 1995		*
	*	Version			:	1.0			*
	*								*
	*   Description							*
	*   -----------							*
	*								*
	*   Generates screen from internal scan	line.			*
	*   Handles Player Missile Graphics Collision Detection.	*
	*								*
	*****************************************************************
*/

/*
	===========================
	ScanLine Generation Section
	===========================
*/

#ifndef BASIC
static UWORD	scanline[ATARI_WIDTH];
static int	ypos;

static int	last_colour = -1;
static int	modified = FALSE;

#ifdef SVGALIB
static UBYTE	*svga_ptr;
#endif

#ifdef X11
#define	NPOINTS	(4096/4)
#define	NRECTS	(4096/4)

static XPoint		points[NPOINTS];
static XRectangle	rectangles[NRECTS];

static int	npoints = 0;
static int	nrects = 0;

void ScanLine_Flush ()
{
#ifdef DOUBLE_SIZE
	if (nrects != 0)
	{
		XFillRectangles (display, pixmap, gc_colour[last_colour], rectangles, nrects);
		nrects = 0;
		modified = TRUE;
	}
#else
	if (npoints != 0)
	{
		XDrawPoints (display, pixmap, gc_colour[last_colour], points, npoints, CoordModeOrigin);
		npoints = 0;
		modified = TRUE;
	}
#endif

	last_colour = -1;
}
#endif

void ScanLine_Show ()
{
	UWORD	*pixel_ptr;
	int	xpos;

	UBYTE	temp[16];

#ifdef SVGALIB
	if ((ypos < 24) || (ypos >= (ATARI_HEIGHT-24))) return;
#endif

	temp[0] = COLBK;
	temp[1] = COLPF0;
	temp[2] = COLPF1;
	temp[3] = COLBK;
	temp[4] = COLPF2;
	temp[5] = COLBK;
	temp[6] = COLBK;
	temp[7] = COLBK;
	temp[8] = COLPF3;
	temp[9] = COLBK;
	temp[10] = COLBK;
	temp[11] = COLBK;
	temp[12] = COLBK;
	temp[13] = COLBK;
	temp[14] = COLBK;
	temp[15] = COLBK;

#ifdef SVGALIB
	pixel_ptr = &scanline[32];
	for (xpos=32;xpos<ATARI_WIDTH-32;xpos++)
#else
	pixel_ptr = scanline;
	for (xpos=0;xpos<ATARI_WIDTH;xpos++)
#endif
	{
		UWORD	pixel;
		UBYTE	playfield;
		UBYTE	colour;

		pixel = *pixel_ptr++;
		playfield = pixel & 0x000f;
		colour = temp[playfield];

		if (pixel & 0xff00)
		{
			UBYTE	player;

			player = (pixel >> 8) & 0x000f;

			if (pixel & 0xf000)
			{
				if (pixel & 0x8000)
				{
					M3PF |= playfield;
					M3PL |= player;
					colour = COLPM3;
				}

				if (pixel & 0x4000)
				{
					M2PF |= playfield;
					M2PL |= player;
					colour = COLPM2;
				}

				if (pixel & 0x2000)
				{
					M1PF |= playfield;
					M1PL |= player;
					colour = COLPM1;
				}

				if (pixel & 0x1000)
				{
					M0PF |= playfield;
					M0PL |= player;
					colour = COLPM0;
				}
/*
	==========================
	Handle fifth player enable
	==========================
*/
				if (PRIOR & 0x10)
				{
					colour = COLPF3;
				}
			}

			if (pixel & 0x0f00)
			{
				if (pixel & 0x0800)
				{
					P3PF |= playfield;
					P3PL |= player;
					colour = COLPM3;
				}

				if (pixel & 0x0400)
				{
					P2PF |= playfield;
					P2PL |= player;
					colour = COLPM2;
				}

				if (pixel & 0x0200)
				{
					P1PF |= playfield;
					P1PL |= player;
					colour = COLPM1;
				}

				if (pixel & 0x0100)
				{
					P0PF |= playfield;
					P0PL |= player;
					colour = COLPM0;
				}
			}
		}

#ifdef SVGALIB
		*svga_ptr++ = colour;
#else
		if (colour != *scanline_ptr)
		{
#ifdef X11
			int	flush = FALSE;

#ifdef DOUBLE_SIZE
			if (nrects == NRECTS) flush = TRUE;
#else
			if (npoints == NPOINTS) flush = TRUE;
#endif

			if (colour != last_colour) flush = TRUE;
			if (flush)
			{
				ScanLine_Flush ();
				last_colour = colour;
			}

#ifdef DOUBLE_SIZE
			rectangles[nrects].x = xpos << 1;
			rectangles[nrects].y = ypos << 1;
			rectangles[nrects].width = 2;
			rectangles[nrects].height = 2;
			nrects++;
#else
			points[npoints].x = xpos;
			points[npoints].y = ypos;
			npoints++;
#endif
#endif

#ifdef INTUITION
			if (colour != 0x94)
				SetAPen (WindowMain->RPort, 1);
			else
				SetAPen (WindowMain->RPort, 2);

			WritePixel (WindowMain->RPort, xpos, ypos);
#endif

			*scanline_ptr++ = colour;
		}
		else
		{
			scanline_ptr++;
		}
#endif
	}

#ifdef X11
	ScanLine_Flush ();
#endif
}
#endif

/*
	*****************************************************************
	*								*
	*	Section			:	Player Missile Graphics	*
	*	Original Author		:	David Firth		*
	*	Date Written		:	28th May 1995		*
	*	Version			:	1.0			*
	*								*
	*****************************************************************
*/

/*
	=========================================
	Width of each bit within a Player/Missile
	=========================================
*/

#ifndef BASIC
static UBYTE	PM_Width[4] = { 2, 4, 2, 8 };
static int	PM_XPos[256];

static UBYTE	singleline;
static UWORD	pl0adr;
static UWORD	pl1adr;
static UWORD	pl2adr;
static UWORD	pl3adr;
static UWORD	m0123adr;

void PM_InitFrame ()
{
	UWORD	pmbase = PMBASE << 8;

	static int	init = FALSE;

	if (!init)
	{
		int	i;

		for (i=0;i<256;i++)
		{
			PM_XPos[i] = (i - 0x20) * 2;
		}

		init = TRUE;
	}

	switch (DMACTL & 0x10)
	{
		case 0x00 :
			singleline = FALSE;
			m0123adr = pmbase + 384 + 4;
			pl0adr = pmbase + 512 + 4;
			pl1adr = pmbase + 640 + 4;
			pl2adr = pmbase + 768 + 4;
			pl3adr = pmbase + 896 + 4;
			break;
		case 0x10 :
			singleline = TRUE;
			m0123adr = pmbase + 768 + 8;
			pl0adr = pmbase + 1024 + 8;
			pl1adr = pmbase + 1280 + 8;
			pl2adr = pmbase + 1536 + 8;
			pl3adr = pmbase + 1792 + 8;
			break;
	}
}

void PM_ScanLine ()
{
	int	hposp0;
	int	hposp1;
	int	hposp2;
	int	hposp3;

	int	hposm0;
	int	hposm1;
	int	hposm2;
	int	hposm3;

	UBYTE	grafp0 = memory[pl0adr];
	UBYTE	grafp1 = memory[pl1adr];
	UBYTE	grafp2 = memory[pl2adr];
	UBYTE	grafp3 = memory[pl3adr];
	UBYTE	grafm = memory[m0123adr];

	UBYTE	s0;
	UBYTE	s1;
	UBYTE	s2;
	UBYTE	s3;
	UBYTE	sm;

	int	nextdata;
	int	i;

	hposp0 = PM_XPos[HPOSP0];
	hposp1 = PM_XPos[HPOSP1];
	hposp2 = PM_XPos[HPOSP2];
	hposp3 = PM_XPos[HPOSP3];

	hposm0 = PM_XPos[HPOSM0];
	hposm1 = PM_XPos[HPOSM1];
	hposm2 = PM_XPos[HPOSM2];
	hposm3 = PM_XPos[HPOSM3];

	s0 = PM_Width[SIZEP0 & 0x03];
	s1 = PM_Width[SIZEP1 & 0x03];
	s2 = PM_Width[SIZEP2 & 0x03];
	s3 = PM_Width[SIZEP3 & 0x03];
	sm = PM_Width[SIZEM & 0x03];

	for (i=0;i<8;i++)
	{
		int	j;

		if (grafp0 & 0x80)
		{
			for (j=0;j<s0;j++)
			{
				if ((hposp0 >= 0) && (hposp0 < ATARI_WIDTH))
					scanline[hposp0] |= 0x0100;
				hposp0++;
			}
		}
		else
		{
			hposp0 += s0;
		}

		if (grafp1 & 0x80)
		{
			for (j=0;j<s1;j++)
			{
				if ((hposp1 >= 0) && (hposp1 < ATARI_WIDTH))
					scanline[hposp1] |= 0x0200;
				hposp1++;
			}
		}
		else
		{
			hposp1 += s1;
		}

		if (grafp2 & 0x80)
		{
			for (j=0;j<s2;j++)
			{
				if ((hposp2 >= 0) && (hposp2 < ATARI_WIDTH))
					scanline[hposp2] |= 0x0400;
				hposp2++;
			}
		}
		else
		{
			hposp2 += s2;
		}

		if (grafp3 & 0x80)
		{
			for (j=0;j<s3;j++)
			{
				if ((hposp3 >= 0) && (hposp3 < ATARI_WIDTH))
					scanline[hposp3] |= 0x0800;
				hposp3++;
			}
		}
		else
		{
			hposp3 += s3;
		}

		if (grafm & 0x80)
		{
			for (j=0;j<sm;j++)
			{
				switch (i & 0x06)
				{
					case 0x00 :
						if ((hposm3 >= 0) && (hposm3 < ATARI_WIDTH))
							scanline[hposm3] |= 0x8000;
						hposm3++;
						break;
					case 0x02 :
						if ((hposm2 >= 0) && (hposm2 < ATARI_WIDTH))
							scanline[hposm2] |= 0x4000;
						hposm2++;
						break;
					case 0x04 :
						if ((hposm1 >= 0) && (hposm1 < ATARI_WIDTH))
							scanline[hposm1] |= 0x2000;
						hposm1++;
						break;
					case 0x06 :
						if ((hposm0 >= 0) && (hposm0 < ATARI_WIDTH))
							scanline[hposm0] |= 0x1000;
						hposm0++;
						break;
				}
			}
		}
		else
		{
			switch (i & 0x06)
			{
				case 0x00 :
					hposm3 += sm;
					break;
				case 0x02 :
					hposm2 += sm;
					break;
				case 0x04 :
					hposm1 += sm;
					break;
				case 0x06 :
					hposm0 += sm;
					break;
			}
		}

		grafp0 = grafp0 << 1;
		grafp1 = grafp1 << 1;
		grafp2 = grafp2 << 1;
		grafp3 = grafp3 << 1;
		grafm = grafm << 1;
	}

	if (singleline)
		nextdata = TRUE;
	else
		nextdata = (ypos & 0x01);

	if (nextdata)
	{
		pl0adr++;
		pl1adr++;
		pl2adr++;
		pl3adr++;
		m0123adr++;
	}
}
#endif

/*
	*****************************************************************
	*								*
	*	Section			:	Antic Display Modes	*
	*	Original Author		:	David Firth		*
	*	Date Written		:	28th May 1995		*
	*	Version			:	1.0			*
	*								*
	*								*
	*   Description							*
	*   -----------							*
	*								*
	*   Section that handles Antic display modes. Not required	*
	*   for BASIC version.						*
	*								*
	*****************************************************************
*/

#ifndef BASIC

static UWORD	screenaddr;
static UWORD	chbase;

static int	xmin;
static int	xmax;

void antic_blank (int nlines)
{
	while (nlines > 0)
	{
		int	i;

		for (i=0;i<ATARI_WIDTH;i++) scanline[i] = 0;

		if (DMACTL & 0x0c)
		{
			PM_ScanLine ();
		}

		ScanLine_Show ();

		ypos++;
		nlines--;
	}
}

void antic_2 ()
{
	UWORD	chadr[48];
	UBYTE	invert[48];
	UBYTE	blank[48];
	int	charoffset;
	int	chardelta;
	int	i;

	int	nchars;
	int	invert_mask;
	int	blank_mask;

#ifdef DEBUG
	fprintf (stderr, "ANTIC: mode 2\n");
#endif

	chbase = CHBASE << 8;

	nchars = (xmax - xmin) >> 3;
/*
	==========================
	Check for vertical reflect
	==========================
*/
	if (CHACTL & 0x04)
	{
		charoffset = 7;
		chardelta = -1;
	}
	else
	{
		charoffset = 0;
		chardelta = 1;
	}
/*
	======================
	Check for video invert
	======================
*/
	if (CHACTL & 0x02)
	{
		invert_mask = 0x80;
	}
	else
	{
		invert_mask = 0x00;
	}
/*
	=========================
	Check for character blank
	=========================
*/
	if (CHACTL & 0x01)
	{
		blank_mask = 0x00;
	}
	else
	{
		blank_mask = 0x80;
	}
/*
	==============================================
	Extract required characters from screen memory
	and locate start position in character set.
	==============================================
*/
	for (i=0;i<nchars;i++)
	{
		UBYTE	screendata;

		screendata = GetByte (screenaddr);
		screenaddr++;
		chadr[i] = chbase + ((UWORD)(screendata & 0x7f) << 3) + charoffset;

		if (screendata & invert_mask)
			invert[i] = 0xff;
		else
			invert[i] = 0x00;

		if (screendata & 0x80)
			blank[i] = screendata & blank_mask;
		else
			blank[i] = 0x80;
		if (blank[i]) blank[i] = 0xff;
	}

	for (i=0;i<8;i++)
	{
		int	j;
		int	xpos = xmin;

		for (j=0;j<xpos;j++) scanline[j] = 0;

		for (j=0;j<nchars;j++)
		{
			UBYTE	chdata;
			UWORD	addr;
			int	k;

			addr = chadr[j];
			chdata = memory[addr];
			chadr[j] = addr + chardelta;

			chdata = (chdata ^ invert[j]) & blank[j];

			if (chdata)
			{
				for (k=0;k<8;k++)
				{
					if (chdata & 0x80)
					{
						scanline[xpos++] = 0x0002;
					}
					else
					{
						scanline[xpos++] = 0x0004;
					}

					chdata = chdata << 1;
				}
			}
			else
			{
				scanline[xpos++] = 0x0004;
				scanline[xpos++] = 0x0004;
				scanline[xpos++] = 0x0004;
				scanline[xpos++] = 0x0004;
				scanline[xpos++] = 0x0004;
				scanline[xpos++] = 0x0004;
				scanline[xpos++] = 0x0004;
				scanline[xpos++] = 0x0004;
			}
		}

		while (xpos < ATARI_WIDTH) scanline[xpos++] = 0;

		if (DMACTL & 0x0c)
			PM_ScanLine ();

		ScanLine_Show ();

		ypos++;
	}
}

void antic_3 ()
{
	UWORD	chadr[48];
	UBYTE	invert[48];
	UBYTE	blank[48];
	UBYTE	lowercase[48];
	UBYTE	first[48];
	UBYTE	second[48];
	int	charoffset;
	int	chardelta;
	int	i;

	int	nchars;
	int	invert_mask;
	int	blank_mask;

#ifdef DEBUG
	fprintf (stderr, "ANTIC: mode 3\n");
#endif

	chbase = CHBASE << 8;

	nchars = (xmax - xmin) >> 3;
/*
	==========================
	Check for vertical reflect
	==========================
*/
	if (CHACTL & 0x04)
	{
		charoffset = 7;
		chardelta = -1;
	}
	else
	{
		charoffset = 0;
		chardelta = 1;
	}
/*
	======================
	Check for video invert
	======================
*/
	if (CHACTL & 0x02)
	{
		invert_mask = 0x80;
	}
	else
	{
		invert_mask = 0x00;
	}
/*
	=========================
	Check for character blank
	=========================
*/
	if (CHACTL & 0x01)
	{
		blank_mask = 0x00;
	}
	else
	{
		blank_mask = 0x80;
	}
/*
	==============================================
	Extract required characters from screen memory
	and locate start position in character set.
	==============================================
*/
	for (i=0;i<nchars;i++)
	{
		UBYTE	screendata;

		screendata = GetByte (screenaddr);
		screenaddr++;
		chadr[i] = chbase + ((UWORD)(screendata & 0x7f) << 3) + charoffset;
		invert[i] = screendata & invert_mask;

		if (screendata & 0x80)
			blank[i] = screendata & blank_mask;
		else
			blank[i] = 0x80;

		if ((screendata & 0x60) == 0x60)
		{
			lowercase[i] = TRUE;
		}
		else
		{
			lowercase[i] = FALSE;
		}
	}

	for (i=0;i<10;i++)
	{
		int	j;
		int	xpos = xmin;

		for (j=0;j<ATARI_WIDTH;j++) scanline[j] = 0;

		for (j=0;j<nchars;j++)
		{
			UBYTE	chdata;
			int	t_invert;
			int	t_blank;
			int	k;

			if (lowercase[j])
			{
				switch (i)
				{
					case 0 :
						first[j] = memory[chadr[j]];
						chdata = 0;
						break;
					case 1 :
						second[j] = memory[chadr[j]];
						chdata = 0;
						break;
					case 8 :
						chdata = first[j];
						break;
					case 9 :
						chdata = second[j];
						break;
					default :
						chdata = memory[chadr[j]];
						break;
				}
			}
			else if (i < 8)
			{
				chdata = memory[chadr[j]];
			}
			else
			{
				chdata = 0;
			}

			chadr[j] += chardelta;
			t_invert = invert[j];
			t_blank = blank[j];

			for (k=0;k<8;k++)
			{
				if (((chdata & 0x80) ^ t_invert) & t_blank)
				{
					scanline[xpos++] |= 0x0002;
				}
				else
				{
					scanline[xpos++] |= 0x0004;
				}

				chdata = chdata << 1;
			}
		}

		if (DMACTL & 0x0c)
			PM_ScanLine ();

		ScanLine_Show ();

		ypos++;
	}
}

void antic_4 ()
{
	UBYTE	screendata[48];
	UWORD	chadr[48];
	int	charoffset;
	int	chardelta;
	int	i;

	int	nchars;

#ifdef DEBUG
	fprintf (stderr, "ANTIC: mode 4\n");
#endif

	chbase = CHBASE << 8;

	nchars = (xmax - xmin) >> 3;
/*
	==========================
	Check for vertical reflect
	==========================
*/
	if (CHACTL & 0x04)
	{
		charoffset = 7;
		chardelta = -1;
	}
	else
	{
		charoffset = 0;
		chardelta = 1;
	}
/*
	==============================================
	Extract required characters from screen memory
	and locate start position in character set.
	==============================================
*/
	for (i=0;i<nchars;i++)
	{
		screendata[i] = GetByte (screenaddr);
		screenaddr++;
		chadr[i] = chbase + ((UWORD)(screendata[i] & 0x7f) << 3) + charoffset;
	}

	for (i=0;i<8;i++)
	{
		int	j;
		int	xpos = xmin;

		for (j=0;j<ATARI_WIDTH;j++) scanline[j] = 0;

		for (j=0;j<nchars;j++)
		{
			UBYTE	chdata;
			UBYTE	t_screendata;
			int	k;

			chdata = memory[chadr[j]];
			chadr[j] += chardelta;
			t_screendata = screendata[j];

			for (k=0;k<4;k++)
			{
				switch (chdata & 0xc0)
				{
					case 0x00 :
						xpos += 2;
						break;
					case 0x40 :
						scanline[xpos++] |= 0x0001;
						scanline[xpos++] |= 0x0001;
						break;
					case 0x80 :
						if (t_screendata & 0x80)
						{
							scanline[xpos++] |= 0x0004;
							scanline[xpos++] |= 0x0004;
						}
						else
						{
							scanline[xpos++] |= 0x0002;
							scanline[xpos++] |= 0x0002;
						}
						break;
					case 0xc0 :
						scanline[xpos++] |= 0x0008;
						scanline[xpos++] |= 0x0008;
						break;
				}

				chdata = chdata << 2;
			}
		}

		if (DMACTL & 0x0c)
			PM_ScanLine ();

		ScanLine_Show ();

		ypos++;
	}
}

void antic_5 ()
{
	UBYTE	screendata[48];
	UWORD	chadr[48];
	int	charoffset;
	int	chardelta;
	int	i;

	int	nchars;

#ifdef DEBUG
	fprintf (stderr, "ANTIC: mode 5\n");
#endif

	chbase = CHBASE << 8;

	nchars = (xmax - xmin) >> 3;
/*
	==========================
	Check for vertical reflect
	==========================
*/
	if (CHACTL & 0x04)
	{
		charoffset = 7;
		chardelta = -1;
	}
	else
	{
		charoffset = 0;
		chardelta = 1;
	}
/*
	==============================================
	Extract required characters from screen memory
	and locate start position in character set.
	==============================================
*/
	for (i=0;i<nchars;i++)
	{
		screendata[i] = GetByte (screenaddr);
		screenaddr++;
		chadr[i] = chbase + ((UWORD)(screendata[i] & 0x7f) << 3) + charoffset;
	}

	for (i=0;i<8;i++)
	{
		int	j;
		int	xpos = xmin;

		for (j=0;j<ATARI_WIDTH;j++) scanline[j] = 0;

		for (j=0;j<nchars;j++)
		{
			UBYTE	chdata;
			UBYTE	t_screendata;
			int	k;

			chdata = memory[chadr[j]];
			chadr[j] += chardelta;
			t_screendata = screendata[j];

			for (k=0;k<4;k++)
			{
				switch (chdata & 0xc0)
				{
					case 0x00 :
						xpos += 2;
						break;
					case 0x40 :
						scanline[xpos++] |= 0x0001;
						scanline[xpos++] |= 0x0001;
						break;
					case 0x80 :
						if (t_screendata & 0x80)
						{
							scanline[xpos++] |= 0x0004;
							scanline[xpos++] |= 0x0004;
						}
						else
						{
							scanline[xpos++] |= 0x0002;
							scanline[xpos++] |= 0x0002;
						}
						break;
					case 0xc0 :
						scanline[xpos++] |= 0x0008;
						scanline[xpos++] |= 0x0008;
						break;
				}

				chdata = chdata << 2;
			}
		}

		if (DMACTL & 0x0c)
		{
			PM_ScanLine ();
			ScanLine_Show ();
			ypos++;

			for (j=0;j<ATARI_WIDTH;j++) scanline[j] &= 0x000f;

			PM_ScanLine ();
			ScanLine_Show ();
			ypos++;
		}
		else
		{
			ScanLine_Show ();
			ypos++;
			ScanLine_Show ();
			ypos++;
		}
	}
}

void antic_6 ()
{
	UBYTE	screendata[24];
	UWORD	chadr[24];
	int	charoffset;
	int	chardelta;
	int	i;

	int	nchars;

#ifdef DEBUG
	fprintf (stderr, "ANTIC: mode 6\n");
#endif

	chbase = CHBASE << 8;

	nchars = (xmax - xmin) / 16;
/*
	==========================
	Check for vertical reflect
	==========================
*/
	if (CHACTL & 0x04)
	{
		charoffset = 7;
		chardelta = -1;
	}
	else
	{
		charoffset = 0;
		chardelta = 1;
	}
/*
	==============================================
	Extract required characters from screen memory
	and locate start position in character set.
	==============================================
*/
	for (i=0;i<nchars;i++)
	{
		screendata[i] = GetByte (screenaddr);
		screenaddr++;
		chadr[i] = chbase + ((UWORD)(screendata[i] & 0x3f) << 3) + charoffset;
	}

	for (i=0;i<8;i++)
	{
		int	j;
		int	xpos = xmin;

		for (j=0;j<ATARI_WIDTH;j++) scanline[j] = 0;

		for (j=0;j<nchars;j++)
		{
			UBYTE	chdata;
			UBYTE	t_screendata;
			int	k;

			chdata = memory[chadr[j]];
			chadr[j] += chardelta;
			t_screendata = screendata[j];

			for (k=0;k<8;k++)
			{
				if (chdata & 0x80)
				{
					switch (t_screendata & 0xc0)
					{
						case 0x00 :
							scanline[xpos++] |= 0x0001;
							scanline[xpos++] |= 0x0001;
							break;
						case 0x40 :
							scanline[xpos++] |= 0x0002;
							scanline[xpos++] |= 0x0002;
							break;
						case 0x80 :
							scanline[xpos++] |= 0x0004;
							scanline[xpos++] |= 0x0004;
							break;
						case 0xc0 :
							scanline[xpos++] |= 0x0008;
							scanline[xpos++] |= 0x0008;
							break;
					}
				}
				else
				{
					xpos += 2;
				}

				chdata = chdata << 1;
			}
		}

		if (DMACTL & 0x0c)
			PM_ScanLine ();

		ScanLine_Show ();

		ypos++;
	}
}

void antic_7 ()
{
	UBYTE	screendata[24];
	UWORD	chadr[24];
	int	charoffset;
	int	chardelta;
	int	i;

	int	nchars;

#ifdef DEBUG
	fprintf (stderr, "ANTIC: mode 7\n");
#endif

	chbase = CHBASE << 8;

	nchars = (xmax - xmin) / 16;
/*
	==========================
	Check for vertical reflect
	==========================
*/
	if (CHACTL & 0x04)
	{
		charoffset = 7;
		chardelta = -1;
	}
	else
	{
		charoffset = 0;
		chardelta = 1;
	}
/*
	==============================================
	Extract required characters from screen memory
	and locate start position in character set.
	==============================================
*/
	for (i=0;i<nchars;i++)
	{
		screendata[i] = GetByte (screenaddr);
		screenaddr++;
		chadr[i] = chbase + ((UWORD)(screendata[i] & 0x3f) << 3) + charoffset;
	}

	for (i=0;i<8;i++)
	{
		int	j;
		int	xpos = xmin;

		for (j=0;j<ATARI_WIDTH;j++) scanline[j] = 0;

		for (j=0;j<nchars;j++)
		{
			UBYTE	chdata;
			UBYTE	t_screendata;
			int	k;

			chdata = memory[chadr[j]];
			chadr[j] += chardelta;
			t_screendata = screendata[j];

			for (k=0;k<8;k++)
			{
				if (chdata & 0x80)
				{
					switch (t_screendata & 0xc0)
					{
						case 0x00 :
							scanline[xpos++] |= 0x0001;
							scanline[xpos++] |= 0x0001;
							break;
						case 0x40 :
							scanline[xpos++] |= 0x0002;
							scanline[xpos++] |= 0x0002;
							break;
						case 0x80 :
							scanline[xpos++] |= 0x0004;
							scanline[xpos++] |= 0x0004;
							break;
						case 0xc0 :
							scanline[xpos++] |= 0x0008;
							scanline[xpos++] |= 0x0008;
							break;
					}
				}
				else
				{
					xpos += 2;
				}

				chdata = chdata << 1;
			}
		}

		if (DMACTL & 0x0c)
		{
			PM_ScanLine ();
			ScanLine_Show ();
			ypos++;

			for (j=0;j<ATARI_WIDTH;j++) scanline[j] &= 0x000f;

			PM_ScanLine ();
			ScanLine_Show ();
			ypos++;
		}
		else
		{
			ScanLine_Show ();
			ypos++;
			ScanLine_Show ();
			ypos++;
		}
	}
}

void antic_8 ()
{
	int	xpos;
	int	nbytes;
	int	i;
	int	j;

#ifdef DEBUG
	fprintf (stderr, "ANTIC: mode 8\n");
#endif

	nbytes = (xmax - xmin) / 32;

	xpos = xmin;

	for (i=0;i<ATARI_WIDTH;i++) scanline[i] = 0;

	for (i=0;i<nbytes;i++)
	{
		UBYTE	screendata;

		screendata = GetByte (screenaddr);
		screenaddr++;

		for (j=0;j<4;j++)
		{
			switch (screendata & 0xc0)
			{
				case 0x00 :
					xpos += 8;
					break;
				case 0x40 :
					scanline[xpos++] |= 0x0001;
					scanline[xpos++] |= 0x0001;
					scanline[xpos++] |= 0x0001;
					scanline[xpos++] |= 0x0001;
					scanline[xpos++] |= 0x0001;
					scanline[xpos++] |= 0x0001;
					scanline[xpos++] |= 0x0001;
					scanline[xpos++] |= 0x0001;
					break;
				case 0x80 :
					scanline[xpos++] |= 0x0002;
					scanline[xpos++] |= 0x0002;
					scanline[xpos++] |= 0x0002;
					scanline[xpos++] |= 0x0002;
					scanline[xpos++] |= 0x0002;
					scanline[xpos++] |= 0x0002;
					scanline[xpos++] |= 0x0002;
					scanline[xpos++] |= 0x0002;
					break;
				case 0xc0 :
					scanline[xpos++] |= 0x0003;
					scanline[xpos++] |= 0x0003;
					scanline[xpos++] |= 0x0003;
					scanline[xpos++] |= 0x0003;
					scanline[xpos++] |= 0x0003;
					scanline[xpos++] |= 0x0003;
					scanline[xpos++] |= 0x0003;
					scanline[xpos++] |= 0x0003;
					break;
			}

			screendata = screendata << 2;
		}
	}

	if (DMACTL & 0x0c)
	{
		for (i=0;i<8;i++)
		{
			PM_ScanLine ();
			ScanLine_Show ();
			ypos++;

			if (i < 7)
				for (j=0;j<ATARI_WIDTH;j++) scanline[j] &= 0x000f;
		}
	}
	else
	{
		for (i=0;i<8;i++)
		{
			ScanLine_Show ();
			ypos++;
		}
	}
}

void antic_9 ()
{
	int	xpos;
	int	nbytes;
	int	i;
	int	j;

#ifdef DEBUG
	fprintf (stderr, "ANTIC: mode 9\n");
#endif

	nbytes = (xmax - xmin) / 32;

	xpos = xmin;

	for (i=0;i<ATARI_WIDTH;i++) scanline[i] = 0;

	for (i=0;i<nbytes;i++)
	{
		UBYTE	screendata;

		screendata = GetByte (screenaddr);
		screenaddr++;

		for (j=0;j<8;j++)
		{
			switch (screendata & 0x80)
			{
				case 0x00 :
					xpos += 4;
					break;
				case 0x80 :
					scanline[xpos++] |= 0x0001;
					scanline[xpos++] |= 0x0001;
					scanline[xpos++] |= 0x0001;
					scanline[xpos++] |= 0x0001;
					break;
			}

			screendata = screendata << 1;
		}
	}

	if (DMACTL & 0x0c)
	{
		for (i=0;i<4;i++)
		{
			PM_ScanLine ();
			ScanLine_Show ();
			ypos++;


			if (i < 3)
				for (j=0;j<ATARI_WIDTH;j++) scanline[j] &= 0x000f;
		}
	}
	else
	{
		for (i=0;i<4;i++)
		{
			ScanLine_Show ();
			ypos++;
		}
	}
}

void antic_a ()
{
	int	xpos;
	int	nbytes;
	int	i;
	int	j;

#ifdef DEBUG
	fprintf (stderr, "ANTIC: mode a\n");
#endif

	nbytes = (xmax - xmin) / 16;

	xpos = xmin;

	for (i=0;i<ATARI_WIDTH;i++) scanline[i] = 0;

	for (i=0;i<nbytes;i++)
	{
		UBYTE	screendata;

		screendata = GetByte (screenaddr);
		screenaddr++;

		for (j=0;j<4;j++)
		{
			switch (screendata & 0xc0)
			{
				case 0x00 :
					xpos += 4;
					break;
				case 0x40 :
					scanline[xpos++] |= 0x0001;
					scanline[xpos++] |= 0x0001;
					scanline[xpos++] |= 0x0001;
					scanline[xpos++] |= 0x0001;
					break;
				case 0x80 :
					scanline[xpos++] |= 0x0002;
					scanline[xpos++] |= 0x0002;
					scanline[xpos++] |= 0x0002;
					scanline[xpos++] |= 0x0002;
					break;
				case 0xc0 :
					scanline[xpos++] |= 0x0003;
					scanline[xpos++] |= 0x0003;
					scanline[xpos++] |= 0x0003;
					scanline[xpos++] |= 0x0003;
					break;
			}

			screendata = screendata << 2;
		}
	}

	if (DMACTL & 0x0c)
	{
		for (i=0;i<4;i++)
		{
			PM_ScanLine ();
			ScanLine_Show ();
			ypos++;

			if (i < 3)
				for (j=0;j<ATARI_WIDTH;j++) scanline[j] &= 0x000f;
		}
	}
	else
	{
		for (i=0;i<4;i++)
		{
			ScanLine_Show ();
			ypos++;
		}
	}
}

void antic_b ()
{
	int	xpos;
	int	nbytes;
	int	i;

#ifdef DEBUG
	fprintf (stderr, "ANTIC: mode b\n");
#endif

	nbytes = (xmax - xmin) / 16;

	xpos = xmin;

	for (i=0;i<ATARI_WIDTH;i++) scanline[i] = 0;

	for (i=0;i<nbytes;i++)
	{
		UBYTE	screendata;
		int	j;

		screendata = GetByte (screenaddr);
		screenaddr++;

		for (j=0;j<8;j++)
		{
			switch (screendata & 0x80)
			{
				case 0x00 :
					xpos += 2;
					break;
				case 0x80 :
					scanline[xpos++] |= 0x0001;
					scanline[xpos++] |= 0x0001;
					break;
			}

			screendata = screendata << 1;
		}
	}

	if (DMACTL & 0x0c)
	{
		PM_ScanLine ();
		ScanLine_Show ();
		ypos++;

		for (i=0;i<ATARI_WIDTH;i++) scanline[i] &= 0x000f;

		PM_ScanLine ();
		ScanLine_Show ();
		ypos++;
	}
	else
	{
		ScanLine_Show ();
		ypos++;
		ScanLine_Show ();
		ypos++;
	}
}

void antic_c ()
{
	int	xpos;
	int	nbytes;
	int	i;

#ifdef DEBUG
	fprintf (stderr, "ANTIC: mode c\n");
#endif

	nbytes = (xmax - xmin) / 16;

	xpos = xmin;

	for (i=0;i<ATARI_WIDTH;i++) scanline[i] = 0;

	for (i=0;i<nbytes;i++)
	{
		UBYTE	screendata;
		int	j;

		screendata = GetByte (screenaddr);
		screenaddr++;

		for (j=0;j<8;j++)
		{
			if (screendata & 0x80)
			{
				scanline[xpos++] |= 0x0001;
				scanline[xpos++] |= 0x0001;
			}
			else
			{
				xpos += 2;
			}

			screendata = screendata << 1;
		}
	}

	if (DMACTL & 0x0c)
		PM_ScanLine ();

	ScanLine_Show ();

	ypos++;
}

void antic_d ()
{
	int	xpos;
	int	nbytes;
	int	i;

#ifdef DEBUG
	fprintf (stderr, "ANTIC: mode d\n");
#endif

	nbytes = (xmax - xmin) >> 3;

	xpos = xmin;

	for (i=0;i<ATARI_WIDTH;i++) scanline[i] = 0;

	for (i=0;i<nbytes;i++)
	{
		UBYTE	screendata;
		int	j;

		screendata = GetByte (screenaddr);
		screenaddr++;

		for (j=0;j<4;j++)
		{
			switch (screendata & 0xc0)
			{
				case 0x00 :
					xpos += 2;
					break;
				case 0x40 :
					scanline[xpos++] |= 0x0001;
					scanline[xpos++] |= 0x0001;
					break;
				case 0x80 :
					scanline[xpos++] |= 0x0002;
					scanline[xpos++] |= 0x0002;
					break;
				case 0xc0 :
					scanline[xpos++] |= 0x0004;
					scanline[xpos++] |= 0x0004;
					break;
			}

			screendata = screendata << 2;
		}
	}

	if (DMACTL & 0x0c)
	{
		PM_ScanLine ();
		ScanLine_Show ();
		ypos++;

		for (i=0;i<ATARI_WIDTH;i++) scanline[i] &= 0x000f;

		PM_ScanLine ();
		ScanLine_Show ();
		ypos++;
	}
	else
	{
		ScanLine_Show ();
		ypos++;
		ScanLine_Show ();
		ypos++;
	}
}

void antic_e ()
{
	int	xpos;
	int	nbytes;
	int	i;

#ifdef DEBUG
	fprintf (stderr, "ANTIC: mode e\n");
#endif

	nbytes = (xmax - xmin) >> 3;

	xpos = xmin;

	for (i=0;i<ATARI_WIDTH;i++) scanline[i] = 0;

	for (i=0;i<nbytes;i++)
	{
		UBYTE	screendata;
		int	j;

		screendata = GetByte (screenaddr);
		screenaddr++;

		for (j=0;j<4;j++)
		{
			switch (screendata & 0xc0)
			{
				case 0x00 :
					xpos += 2;
					break;
				case 0x40 :
					scanline[xpos++] |= 0x0001;
					scanline[xpos++] |= 0x0001;
					break;
				case 0x80 :
					scanline[xpos++] |= 0x0002;
					scanline[xpos++] |= 0x0002;
					break;
				case 0xc0 :
					scanline[xpos++] |= 0x0004;
					scanline[xpos++] |= 0x0004;
					break;
			}

			screendata = screendata << 2;
		}
	}

	if (DMACTL & 0x0c)
		PM_ScanLine ();

	ScanLine_Show ();

	ypos++;
}

void antic_f ()
{
	int	xpos;
	int	nbytes;
	int	i;

#ifdef DEBUG
	fprintf (stderr, "ANTIC: mode f\n");
#endif

	nbytes = (xmax - xmin) >> 3;

	xpos = xmin;

	for (i=0;i<ATARI_WIDTH;i++) scanline[i] = 0;

	for (i=0;i<nbytes;i++)
	{
		UBYTE	screendata;
		int	j;

		screendata = GetByte (screenaddr);
		screenaddr++;

		for (j=0;j<8;j++)
		{
			if (screendata & 0x80)
				scanline[xpos++] |= 0x0002;
			else
				scanline[xpos++] |= 0x0004;

			screendata = screendata << 1;
		}
	}

	if (DMACTL & 0x0c)
		PM_ScanLine ();

	ScanLine_Show ();

	ypos++;
}

#endif

/*
	*****************************************************************
	*								*
	*	Section			:	Display List		*
	*	Original Author		:	David Firth		*
	*	Date Written		:	28th May 1995		*
	*	Version			:	1.0			*
	*								*
	*   Description							*
	*   -----------							*
	*								*
	*   Section that handles Antic Display List. Not required for	*
	*   BASIC version.						*
	*								*
	*****************************************************************
*/

#ifndef BASIC

void Atari800_UpdateScreen ()
{
	UWORD	dlist;

	int	JVB;
	int	abort_count = 300;

#ifdef SVGALIB
	svga_ptr = graph_mem;
#endif

	PM_InitFrame ();

	modified = FALSE;
	scanline_ptr = image_data;
	ypos = 0;

	if (DMACTL & 0x20)
	{
#ifdef DEBUG
		fprintf (stderr, "Atari800_UpdateScreen: started\n");
#endif

		dlist = (DLISTH << 8) | DLISTL;

		JVB = FALSE;

		do
		{
			UBYTE	IR;

			if (abort_count-- == 0)
			{
				fprintf (stderr, "DLIST: ABORT\n");
				break;
			}

			IR = GetByte (dlist);
			dlist++;

			switch (IR & 0x0f)
			{
				case 0x00 :
					{
						int	nlines;

						nlines = ((IR >> 4)  & 0x07) + 1;
						antic_blank (nlines);
					}
					break;
				case 0x01 :
					if (IR & 0x40)
					{
						JVB = TRUE;
					}
					else
					{
						dlist = (GetByte (dlist+1) << 8) | GetByte (dlist);
						antic_blank (1);	/* Jump aparently uses 1 scan line */
					}
					break;
				default :
					if (IR & 0x40)
					{
						screenaddr = (GetByte (dlist+1) << 8) | GetByte (dlist);
						dlist += 2;
					}

					if (IR & 0x20)
					{
						static int	flag = TRUE;

						if (flag)
						{
							fprintf (stderr, "DLIST: vertical scroll unsupported\n");
							flag = FALSE;
						}
					}

					if (IR & 0x10)
					{
						static int	flag = TRUE;

						if (flag)
						{
							fprintf (stderr, "DLIST: horizontal scroll unsupported\n");
							flag = FALSE;
						}
					}

					switch (DMACTL & 0x03)
					{
						case 0x00 :
							continue;
						case 0x01 :
							xmin = 64;
							xmax = 320;
							break;
						case 0x02 :
							xmin = 32;
							xmax = 352;
							break;
						case 0x03 :
							xmin = 0;
							xmax = 384;
							break;
					}

					switch (IR & 0x0f)
					{
						case 0x02 :
							antic_2 ();
							break;
						case 0x03 :
							antic_3 ();
							break;
						case 0x04 :
							antic_4 ();
							break;
						case 0x05 :
							antic_5 ();
							break;
						case 0x06 :
							antic_6 ();
							break;
						case 0x07 :
							antic_7 ();
							break;
						case 0x08 :
							antic_8 ();
							break;
						case 0x09 :
							antic_9 ();
							break;
						case 0x0a :
							antic_a ();
							break;
						case 0x0b :
							antic_b ();
							break;
						case 0x0c :
							antic_c ();
							break;
						case 0x0d :
							antic_d ();
							break;
						case 0x0e :
							antic_e ();
							break;
						case 0x0f :
							antic_f ();
							break;
						default :
							JVB = TRUE;
							break;
					}

					break;
			}

			if (IR & 0x80)
			{
				if (NMIEN & 0x80)
				{
					NMIST = NMIST | 0x80;

					NMI = TRUE;

					GO (-1);
				}
			}
		} while (!JVB && (ypos < ATARI_HEIGHT));
	}

	antic_blank (ATARI_HEIGHT - ypos);

#ifdef X11
	if (modified)
	{
#ifdef DOUBLE_SIZE
		XCopyArea (display, pixmap, window, gc, 0, 0, ATARI_WIDTH*2, ATARI_HEIGHT*2, 0, 0);
#else
		XCopyArea (display, pixmap, window, gc, 0, 0, ATARI_WIDTH, ATARI_HEIGHT, 0, 0);
#endif
	}
#endif

#ifdef DEBUG
	fprintf (stderr, "Atari800_UpdateScreen: finished\n");
#endif
}

#endif

static int	CONTROL = 0x00;
static int	SHIFT = 0x00;

static int	count = 0;

UBYTE Atari800_GetByte (UWORD addr)
{
	UBYTE	byte;

	count++;

	switch (addr)
	{
		case _CHBASE :
			byte = CHBASE;
			break;
		case _CHACTL :
			byte = CHACTL;
			break;
		case _CONSOL :
			byte = CONSOL;
			break;
		case _DLISTL :
			byte = DLISTL;
			break;
		case _DLISTH :
			byte = DLISTH;
			break;
		case _DMACTL :
			byte = DMACTL;
			break;
		case _KBCODE :
			byte = KBCODE;
			break;
		case _IRQST :
			byte = IRQST;
			break;
		case _M0PF :
			byte = M0PF;
			break;
		case _M1PF :
			byte = M1PF;
			break;
		case _M2PF :
			byte = M2PF;
			break;
		case _M3PF :
			byte = M3PF;
			break;
		case _M0PL :
			byte = M0PL;
			break;
		case _M1PL :
			byte = M1PL;
			break;
		case _M2PL :
			byte = M2PL;
			break;
		case _M3PL :
			byte = M3PL;
			break;
		case _P0PF :
			byte = P0PF;
			break;
		case _P1PF :
			byte = P1PF;
			break;
		case _P2PF :
			byte = P2PF;
			break;
		case _P3PF :
			byte = P3PF;
			break;
		case _P0PL :
			byte = P0PL;
			break;
		case _P1PL :
			byte = P1PL;
			break;
		case _P2PL :
			byte = P2PL;
			break;
		case _P3PL :
			byte = P3PL;
			break;
		case _PENH :
		case _PENV :
			byte = 0x00;
			break;
		case _PORTA :
			byte = PORTA;
			break;
		case _PORTB :
			byte = PORTB;
			break;
		case _POT0 :
#ifdef X11
			{
				Window	root_return;
				Window	child_return;
				int	root_x_return;
				int	root_y_return;
				int	win_x_return;
				int	win_y_return;
				int	mask_return;

				if (XQueryPointer (display, window, &root_return, &child_return, &root_x_return, &root_y_return, &win_x_return, &win_y_return, &mask_return))
				{
					byte = ((float)((ATARI_WIDTH * 2) - win_x_return) / (float)(ATARI_WIDTH*2)) * 228;
					if (mask_return)
					{
						PORTA &= 0xfb;
					}
					else
					{
						PORTA |= 0x04;
					}
				}
				else
				{
					byte = 0;
				}
			}
#else
			byte = 0x00;
#endif
			break;
		case _POT1 :
		case _POT2 :
		case _POT3 :
		case _POT4 :
		case _POT5 :
		case _POT6 :
		case _POT7 :
			byte = 0x00;
			break;
		case _RANDOM :
			byte = count & 0xff;
			break;
		case _TRIG0 :
			byte = TRIG0;
			break;
		case _TRIG1 :
			byte = TRIG1;
			break;
		case _TRIG2 :
			byte = TRIG2;
			break;
		case _TRIG3 :
			byte = TRIG3;
			break;
		case _VCOUNT :
			byte = VCOUNT++;
			break;
		case _NMIEN :
			byte = NMIEN;
			break;
		case _NMIST :
			byte = NMIST;
			break;
		case _SKSTAT :
			if (SHIFT)
				byte = 0x05;
			else
				byte = 0x0d;
			break;
		case _WSYNC :
			byte = 0;
			break;
		default :
			fprintf (stderr, "read from %04x\n", addr);
			byte = 0;
			break;
	}

	return byte;
}

void Atari800_PutByte (UWORD addr, UBYTE byte)
{
	count++;

	switch (addr)
	{
		case 0xD500:	/* Check for oss supercart switch */
			if (super) memcpy (memory+0xA000, super+0x0000, 0x1000);
			break;
		case 0xD504:
			if (super) memcpy (memory+0xA000, super+0x1000, 0x1000);
			break;
		case 0xD503:
		case 0xD507:
			if (super) memcpy (memory+0xA000, super+0x2000, 0x1000);
			break;
		case _AUDC1 :
		case _AUDC2 :
		case _AUDC3 :
		case _AUDC4 :
		case _AUDCTL :
		case _AUDF1 :
		case _AUDF2 :
		case _AUDF3 :
		case _AUDF4 :
			break;
		case _CHBASE :
			CHBASE = byte;
			break;
		case _CHACTL :
			CHACTL = byte;
			break;
		case _COLBK :
			COLBK = byte;
			break;
		case _COLPF0 :
			COLPF0 = byte;
			break;
		case _COLPF1 :
			COLPF1 = byte;
			break;
		case _COLPF2 :
			COLPF2 = byte;
			break;
		case _COLPF3 :
			COLPF3 = byte;
			break;
		case _COLPM0 :
			COLPM0 = byte;
			break;
		case _COLPM1 :
			COLPM1 = byte;
			break;
		case _COLPM2 :
		COLPM2 = byte;
			break;
		case _COLPM3 :
			COLPM3 = byte;
			break;
		case _CONSOL :
			break;
		case _DLISTL :
			DLISTL = byte;
			break;
		case _DLISTH :
			DLISTH = byte;
			break;
		case _DMACTL :
			DMACTL = byte;
			break;
		case _HITCLR :
			M0PF = M1PF = M2PF = M3PF = 0;
			P0PF = P1PF = P2PF = P3PF = 0; 
			M0PL = M1PL = M2PL = M3PL = 0; 
			P0PL = P1PL = P2PL = P3PL = 0;
			break;
		case _HPOSM0 :
			HPOSM0 = byte;
			break;
		case _HPOSM1 :
			HPOSM1 = byte;
			break;
		case _HPOSM2 :
			HPOSM2 = byte;
			break;
		case _HPOSM3 :
			HPOSM3 = byte;
			break;
		case _HPOSP0 :
			HPOSP0 = byte;
			break;
		case _HPOSP1 :
			HPOSP1 = byte;
			break;
		case _HPOSP2 :
			HPOSP2 = byte;
			break;
		case _HPOSP3 :
			HPOSP3 = byte;
			break;
		case _IRQEN :
			break;
		case _NMIEN :
			NMIEN = byte;
			break;
		case _NMIRES :
			NMIRES = 0;
			break;
		case _PMBASE :
			PMBASE = byte;
			break;
		case _POTGO :
			break;
		case _PRIOR :
			PRIOR = byte;
			break;
		case _SIZEM :
			SIZEM = byte;
			break;
		case _SIZEP0 :
			SIZEP0 = byte;
			break;
		case _SIZEP1 :
			SIZEP1 = byte;
			break;
		case _SIZEP2 :
			SIZEP2 = byte;
			break;
		case _SIZEP3 :
			SIZEP3 = byte;
			break;
		case _WSYNC :
			break;
		default :
			fprintf (stderr, "write %02x to %04x\n", byte, addr);
			break;
	}
}

void Atari800_Hardware (void)
{
	static int	pil_on = FALSE;

		int	keycode = -1;

		NMIST = 0x00;

#ifdef CURSES
		CONSOL = 0x07;

		keycode = getch ();
		switch (keycode)
		{
			case 0x1b :
				keycode = 0x1c;
				break;
			case '!' :
				keycode = 0x5f;
				break;
			case '"' :
				keycode = 0x5e;
				break;
			case '#' :
				keycode = 0x5a;
				break;
			case '$' :
				keycode = 0x58;
				break;
			case '%' :
				keycode = 0x5d;
				break;
			case '&' :
				keycode = 0x5b;
				break;
			case '\'' :
				keycode = 0x73;
				break;
			case '@' :
				keycode = 0x75;
				break;
			case '(' :
				keycode = 0x70;
				break;
			case ')' :
				keycode = 0x72;
				break;
			case '<' :
				keycode = 0x36;
				break;
			case '>' :
				keycode = 0x37;
				break;
			case '=' :
				keycode = 0x0f;
				break;
			case '?' :
				keycode = 0x66;
				break;
			case '-' :
				keycode = 0x0e;
				break;
			case '+' :
				keycode = 0x06;
				break;
			case '*' :
				keycode = 0x07;
				break;
			case '/' :
				keycode = 0x26;
				break;
			case ':' :
				keycode = 0x42;
				break;
			case ';' :
				keycode = 0x02;
				break;
			case ',' :
				keycode = 0x20;
				break;
			case '.' :
				keycode = 0x22;
				break;
			case '_' :
				keycode = 0x4e;
				break;
			case '{' :
				keycode = 0x60;
				break;
			case '}' :
				keycode = 0x62;
				break;
/*
			case XK_asciicircum :
*/
				keycode = 0x47;
				break;
			case '\\' :
				keycode = 0x46;
				break;
			case '|' :
				keycode = 0x4f;
				break;
			case '0' :
				keycode = 0x32;
				break;
			case '1' :
				keycode = 0x1f;
				break;
			case '2' :
				keycode = 0x1e;
				break;
			case '3' :
				keycode = 0x1a;
				break;
			case '4' :
				keycode = 0x18;
				break;
			case '5' :
				keycode = 0x1d;
				break;
			case '6' :
				keycode = 0x1b;
				break;
			case '7' :
				keycode = 0x33;
				break;
			case '8' :
				keycode = 0x35;
				break;
			case '9' :
				keycode = 0x30;
				break;
			case ' ' :
				keycode = 0x21;
				break;
			case '\n' :
				keycode = 0x0c;
				break;
			case 'a' :
			case 'A' :
				keycode = SHIFT | 0x3f;
				break;
			case 'b' :
			case 'B' :
				keycode = SHIFT | 0x15;
				break;
			case 'c' :
			case 'C' :
				keycode = SHIFT | 0x12;
				break;
			case 'd' :
			case 'D' :
				keycode = SHIFT | 0x3a;
				break;
			case 'e' :
			case 'E' :
				keycode = SHIFT | 0x2a;
				break;
			case 'f' :
			case 'F' :
				keycode = SHIFT | 0x38;
				break;
			case 'g' :
			case 'G' :
				keycode = SHIFT | 0x3d;
				break;
			case 'h' :
			case 'H' :
				keycode = SHIFT | 0x39;
				break;
			case 'i' :
			case 'I' :
				keycode = SHIFT | 0x0d;
				break;
			case 'j' :
			case 'J' :
				keycode = SHIFT | 0x01;
				break;
			case 'k' :
			case 'K' :
				keycode = SHIFT | 0x05;
				break;
			case 'l' :
			case 'L' :
				keycode = SHIFT | 0x00;
				break;
			case 'm' :
			case 'M' :
				keycode = SHIFT | 0x25;
				break;
			case 'n' :
			case 'N' :
				keycode = SHIFT | 0x23;
				break;
			case 'o' :
			case 'O' :
				keycode = SHIFT | 0x08;
				break;
			case 'p' :
			case 'P' :
				keycode = SHIFT | 0x0a;
				break;
			case 'q' :
			case 'Q' :
				keycode = SHIFT | 0x2f;
				break;
			case 'r' :
			case 'R' :
				keycode = SHIFT | 0x28;
				break;
			case 's' :
			case 'S' :
				keycode = SHIFT | 0x3e;
				break;
			case 't' :
			case 'T' :
				keycode = SHIFT | 0x2d;
				break;
			case 'u' :
			case 'U' :
				keycode = SHIFT | 0x0b;
				break;
			case 'v' :
			case 'V' :
				keycode = SHIFT | 0x10;
				break;
			case 'w' :
			case 'W' :
				keycode = SHIFT | 0x2e;
				break;
			case 'x' :
			case 'X' :
				keycode = SHIFT | 0x16;
				break;
			case 'y' :
			case 'Y' :
				keycode = SHIFT | 0x2b;
				break;
			case 'z' :
			case 'Z' :
				keycode = SHIFT | 0x17;
				break;
			case KEY_F0 + 1 :
				NMIST = 0x20;
				NMI = TRUE;
				keycode = -1;
				break;
			case KEY_F0 + 2 :
				CONSOL &= ~0x04;
				keycode = -1;
				break;
			case KEY_F0 + 3 :
				CONSOL &= ~0x02;
				keycode = -1;
				break;
			case KEY_F0 + 4 :
				CONSOL &= ~0x01;
				keycode = -1;
				break;
			case KEY_F0 + 5 :
				PutByte (0x244, 1);
				NMIST = 0x20;
				NMI = TRUE;
				keycode = -1;
				break;
			case KEY_F0 + 6 :
				if (pil_on)
				{
					printf ("PIL: disabled\n");
					SetRAM (0x8000, 0xbfff);
				}
				else
				{
					printf ("PIL: Memory 0x8000 - 0xbfff is ROM\n");
					SetROM (0x8000, 0xbfff);
				}
				pil_on = !pil_on;
				keycode = -1;
				break;
			case KEY_F0 + 7 :
				IRQST = 0x7f;
				IRQ = TRUE;
				keycode = -1;
				break;
			case KEY_F0 + 8 :
				keycode = -1;
				break;
			case KEY_DOWN :
				keycode = 0x8f;
				break;
			case KEY_LEFT :
				keycode = 0x86;
				break;
			case KEY_RIGHT :
				keycode = 0x87;
				break;
			case KEY_UP :
				keycode = 0x8e;
				break;
			case KEY_BACKSPACE :
				keycode = 0x74;
				keycode = 0x34;
				break;
			default :
				keycode = -1;
				break;
		}
#endif

#ifdef SVGALIB
		CONSOL = 0x07;
		TRIG0 = 1;

		keycode = vga_getkey();
		switch (keycode)
		{
			case '!' :
				keycode = 0x5f;
				break;
			case '"' :
				keycode = 0x5e;
				break;
			case '#' :
				keycode = 0x5a;
				break;
			case '$' :
				keycode = 0x58;
				break;
			case '%' :
				keycode = 0x5d;
				break;
			case '&' :
				keycode = 0x5b;
				break;
			case '\'' :
				keycode = 0x73;
				break;
			case '@' :
				keycode = 0x75;
				break;
			case '(' :
				keycode = 0x70;
				break;
			case ')' :
				keycode = 0x72;
				break;
			case '<' :
				keycode = 0x36;
				break;
			case '>' :
				keycode = 0x37;
				break;
			case '=' :
				keycode = 0x0f;
				break;
			case '?' :
				keycode = 0x66;
				break;
			case '-' :
				keycode = 0x0e;
				break;
			case '+' :
				keycode = 0x06;
				break;
			case '*' :
				keycode = 0x07;
				break;
			case '/' :
				keycode = 0x26;
				break;
			case ':' :
				keycode = 0x42;
				break;
			case ';' :
				keycode = 0x02;
				break;
			case ',' :
				keycode = 0x20;
				break;
			case '.' :
				keycode = 0x22;
				break;
			case '_' :
				keycode = 0x4e;
				break;
			case '{' :
				keycode = 0x60;
				break;
			case '}' :
				keycode = 0x62;
				break;
/*
			case XK_asciicircum :
*/
				keycode = 0x47;
				break;
			case '\\' :
				keycode = 0x46;
				break;
			case '|' :
				keycode = 0x4f;
				break;
			case '0' :
				keycode = 0x32;
				break;
			case '1' :
				keycode = 0x1f;
				break;
			case '2' :
				keycode = 0x1e;
				break;
			case '3' :
				keycode = 0x1a;
				break;
			case '4' :
				keycode = 0x18;
				break;
			case '5' :
				keycode = 0x1d;
				break;
			case '6' :
				keycode = 0x1b;
				break;
			case '7' :
				keycode = 0x33;
				break;
			case '8' :
				keycode = 0x35;
				break;
			case '9' :
				keycode = 0x30;
				break;
			case ' ' :
				keycode = 0x21;
				break;
			case '\n' :
				keycode = 0x0c;
				break;
			case 'a' :
				keycode = 0x40 | 0x3f;
				break;
			case 'A' :
				keycode = SHIFT | 0x3f;
				break;
			case 'b' :
			case 'B' :
				keycode = SHIFT | 0x15;
				break;
			case 'c' :
			case 'C' :
				keycode = SHIFT | 0x12;
				break;
			case 'd' :
			case 'D' :
				keycode = SHIFT | 0x3a;
				break;
			case 'e' :
			case 'E' :
				keycode = SHIFT | 0x2a;
				break;
			case 'f' :
			case 'F' :
				keycode = SHIFT | 0x38;
				break;
			case 'g' :
			case 'G' :
				keycode = SHIFT | 0x3d;
				break;
			case 'h' :
			case 'H' :
				keycode = SHIFT | 0x39;
				break;
			case 'i' :
			case 'I' :
				keycode = SHIFT | 0x0d;
				break;
			case 'j' :
			case 'J' :
				keycode = SHIFT | 0x01;
				break;
			case 'k' :
			case 'K' :
				keycode = SHIFT | 0x05;
				break;
			case 'l' :
			case 'L' :
				keycode = SHIFT | 0x00;
				break;
			case 'm' :
			case 'M' :
				keycode = SHIFT | 0x25;
				break;
			case 'n' :
			case 'N' :
				keycode = SHIFT | 0x23;
				break;
			case 'o' :
			case 'O' :
				keycode = SHIFT | 0x08;
				break;
			case 'p' :
			case 'P' :
				keycode = SHIFT | 0x0a;
				break;
			case 'q' :
			case 'Q' :
				keycode = SHIFT | 0x2f;
				break;
			case 'r' :
			case 'R' :
				keycode = SHIFT | 0x28;
				break;
			case 's' :
			case 'S' :
				keycode = SHIFT | 0x3e;
				break;
			case 't' :
			case 'T' :
				keycode = SHIFT | 0x2d;
				break;
			case 'u' :
			case 'U' :
				keycode = SHIFT | 0x0b;
				break;
			case 'v' :
			case 'V' :
				keycode = SHIFT | 0x10;
				break;
			case 'w' :
			case 'W' :
				keycode = SHIFT | 0x2e;
				break;
			case 'x' :
			case 'X' :
				keycode = SHIFT | 0x16;
				break;
			case 'y' :
			case 'Y' :
				keycode = SHIFT | 0x2b;
				break;
			case 'z' :
			case 'Z' :
				keycode = SHIFT | 0x17;
				break;
			case 0x7f :	/* Backspace */
				keycode = 0x74;
				keycode = 0x34;
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
						NMIST = 0x20;
						NMI = TRUE;
						keycode = -1;
					}
					else if (strcmp(buff, "\133\133\102") == 0)	/* F2 */
					{
						CONSOL &= ~0x04;
						keycode = -1;
					}
					else if (strcmp(buff, "\133\133\103") == 0)	/* F3 */
					{
						CONSOL &= ~0x02;
						keycode = -1;
					}
					else if (strcmp(buff, "\133\133\104") == 0)	/* F4 */
					{
						CONSOL &= ~0x01;
						keycode = -1;
					}
					else if (strcmp(buff, "\133\133\105") == 0)	/* F5 */
					{
						PutByte (0x244, 1);
						NMIST = 0x20;
						NMI = TRUE;
						keycode = -1;
					}
					else if (strcmp(buff, "\133\061\067\176") == 0)	/* F6 */
					{
						if (pil_on)
						{
							printf ("PIL: disabled\n");
							SetRAM (0x8000, 0xbfff);
						}
						else
						{
							printf ("PIL: Memory 0x8000 - 0xbfff is ROM\n");
							SetROM (0x8000, 0xbfff);
						}
						pil_on = !pil_on;
						keycode = -1;
					}
					else if (strcmp(buff, "\133\061\070\176") == 0)	/* F7 */
					{
						IRQST = 0x7f;
						IRQ = TRUE;
						keycode = -1;
					}
					else if (strcmp(buff, "\133\061\071\176") == 0)	/* F8 */
					{
						keycode = -1;
					}
					else if (strcmp(buff, "\133\062\176") == 0)	/* Keypad 0 */
					{
						TRIG0 = 0;
						keycode = -1;
					}
					else if (strcmp(buff, "\133\064\176") == 0)	/* Keypad 1 */
					{
						PORTA = 0xf9;
						keycode = -1;
					}
					else if (strcmp(buff, "\133\102") == 0)		/* Keypad 2 */
					{
						PORTA = 0xfd;
						keycode = 0x8f;
					}
					else if (strcmp(buff, "\133\066\176") == 0)	/* Keypad 3 */
					{
						PORTA = 0xf5;
						keycode = -1;
					}
					else if (strcmp(buff, "\133\104") == 0)		/* Keypad 4 */
					{
						PORTA = 0xfb;
						keycode = 0x86;
					}
					else if (strcmp(buff, "\133\107") == 0)		/* Keypad 5 */
					{
						PORTA = 0xff;
						keycode = -1;
					}
					else if (strcmp(buff, "\133\103") == 0)		/* Keypad 6 */
					{
						PORTA = 0xf7;
						keycode = 0x87;
					}
					else if (strcmp(buff, "\133\061\176") == 0)	/* Keypad 7 */
					{
						PORTA = 0xfa;
						keycode = -1;
					}
					else if (strcmp(buff, "\133\101") == 0)		/* Keypad 8 */
					{
						PORTA = 0xfe;
						keycode = 0x8e;
					}
					else if (strcmp(buff, "\133\065\176") == 0)	/* Keypad 9 */
					{
						PORTA = 0xf6;
						keycode = -1;
					}
					else
					{
						int	i;

						printf ("Unknown key: 0x1b ");
						for (i=0;i<nc;i++) printf ("0x%02x ", buff[i]);
						printf ("\n");

						keycode = -1;
					}
				}
				break;
			default :
				printf ("Unknown Keycode: %d\n", keycode);
			case 0 :
				keycode = -1;
				break;
		}
#endif

#ifdef X11
		if (XEventsQueued (display, QueuedAfterFlush) > 0)
		{
			XEvent	event;
			KeySym	keysym;
			char	buffer[128];

			XNextEvent (display, &event);
/*
			keysym = XLookupKeysym ((XKeyEvent*)&event, 0);
*/
			XLookupString ((XKeyEvent*)&event, buffer, 128, &keysym, &keyboard_status);

			switch (event.type)
			{
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
							if (SHIFT)
								keycode = 0x7c;
							else
								keycode = 0x3c;
							break;
						case XK_Shift_Lock :
							break;
						case XK_Alt_L :
						case XK_Alt_R :
							keycode = 0x27;
							break;
						case XK_F5 :
							PutByte (0x244, 1);
						case XK_F1 :
							NMIST = 0x20;
							NMI = TRUE;
							break;
						case XK_F2 :
							CONSOL &= ~0x04;
							break;
						case XK_F3 :
							CONSOL &= ~0x02;
							break;
						case XK_F4 :
							CONSOL &= ~0x01;
							break;
						case XK_F6 :
							if (pil_on)
							{
								printf ("PIL: disabled\n");
								SetRAM (0x8000, 0xbfff);
							}
							else
							{
								printf ("PIL: Memory 0x8000 - 0xbfff is ROM\n");
								SetROM (0x8000, 0xbfff);
							}
							pil_on = !pil_on;
							break;
						case XK_F7 :
							IRQST = 0x7f;
							IRQ = TRUE;
							break;
						case XK_F8 :
							{
								char	filename[128];

								printf ("Next Disk: ");
								scanf ("\n%s", filename);
								SIO_Dismount (1);
								if (!SIO_Mount (1, filename))
								{
									printf ("Failed to mount %s\n", filename);
								}
							}
							break;
						case XK_KP_0 :
							TRIG0 = 0;
							break;
						case XK_KP_1 :
							PORTA = 0xf9;
							break;
						case XK_KP_2 :
							PORTA = 0xfd;
							break;
						case XK_KP_3 :
							PORTA = 0xf5;
							break;
						case XK_KP_4 :
							PORTA = 0xfb;
							break;
						case XK_KP_5 :
							PORTA = 0xff;
							break;
						case XK_KP_6 :
							PORTA = 0xf7;
							break;
						case XK_KP_7 :
							PORTA = 0xfa;
							break;
						case XK_KP_8 :
							PORTA = 0xfe;
							break;
						case XK_KP_9 :
							PORTA = 0xf6;
							break;
						case XK_Home :
							keycode = 0x76;
							break;
						case XK_Insert :
							if (SHIFT)
								keycode = 0x77;
							else
								keycode = 0xb7;
							break;
						case XK_Delete :
							if (SHIFT)
								keycode = 0x74;
							else
								keycode = 0x34;
							break;
						case XK_Left :
							keycode = 0x86;
							break;
						case XK_Up :
							keycode = 0x8e;
							break;
						case XK_Right :
							keycode = 0x87;
							break;
						case XK_Down :
							keycode = 0x8f;
							break;
						case XK_Escape :
							keycode = 0x1c;
							break;
						case XK_Tab :
							if (SHIFT)
								keycode = 0x6c;
							else
								keycode = 0x2c;
							break;
						case XK_exclam :
							keycode = 0x5f;
							break;
						case XK_quotedbl :
							keycode = 0x5e;
							break;
						case XK_numbersign :
							keycode = 0x5a;
							break;
						case XK_dollar :
							keycode = 0x58;
							break;
						case XK_percent :
							keycode = 0x5d;
							break;
						case XK_ampersand :
							keycode = 0x5b;
							break;
						case XK_quoteright :
							keycode = 0x73;
							break;
						case XK_at :
							keycode = 0x75;
							break;
						case XK_parenleft :
							keycode = 0x70;
							break;
						case XK_parenright :
							keycode = 0x72;
							break;
						case XK_less :
							keycode = 0x36;
							break;
						case XK_greater :
							keycode = 0x37;
							break;
						case XK_equal :
							keycode = 0x0f;
							break;
						case XK_question :
							keycode = 0x66;
							break;
						case XK_minus :
							keycode = 0x0e;
							break;
						case XK_plus :
							keycode = 0x06;
							break;
						case XK_asterisk :
							keycode = 0x07;
							break;
						case XK_slash :
							keycode = 0x26;
							break;
						case XK_colon :
							keycode = 0x42;
							break;
						case XK_semicolon :
							keycode = 0x02;
							break;
						case XK_comma :
							keycode = 0x20;
							break;
						case XK_period :
							keycode = 0x22;
							break;
						case XK_underscore :
							keycode = 0x4e;
							break;
						case XK_bracketleft :
							keycode = 0x60;
							break;
						case XK_bracketright :
							keycode = 0x62;
							break;
						case XK_asciicircum :
							keycode = 0x47;
							break;
						case XK_backslash :
							keycode = 0x46;
							break;
						case XK_bar :
							keycode = 0x4f;
							break;
						case XK_0 :
							keycode = 0x32;
							break;
						case XK_1 :
							keycode = 0x1f;
							break;
						case XK_2 :
							keycode = 0x1e;
							break;
						case XK_3 :
							keycode = 0x1a;
							break;
						case XK_4 :
							keycode = 0x18;
							break;
						case XK_5 :
							keycode = 0x1d;
							break;
						case XK_6 :
							keycode = 0x1b;
							break;
						case XK_7 :
							keycode = 0x33;
							break;
						case XK_8 :
							keycode = 0x35;
							break;
						case XK_9 :
							keycode = 0x30;
							break;
						case XK_space :
							keycode = 0x21;
							break;
						case XK_Return :
							keycode = 0x0c;
							break;
						case XK_a :
						case XK_A :
							keycode = SHIFT | 0x3f;
							break;
						case XK_b :
						case XK_B :
							keycode = SHIFT | 0x15;
							break;
						case XK_c :
						case XK_C :
							keycode = SHIFT | 0x12;
							break;
						case XK_d :
						case XK_D :
							keycode = SHIFT | 0x3a;
							break;
						case XK_e :
						case XK_E :
							keycode = SHIFT | 0x2a;
							break;
						case XK_f :
						case XK_F :
							keycode = SHIFT | 0x38;
							break;
						case XK_g :
						case XK_G :
							keycode = SHIFT | 0x3d;
							break;
						case XK_h :
						case XK_H :
							keycode = SHIFT | 0x39;
							break;
						case XK_i :
						case XK_I :
							keycode = SHIFT | 0x0d;
							break;
						case XK_j :
						case XK_J :
							keycode = SHIFT | 0x01;
							break;
						case XK_k :
						case XK_K :
							keycode = SHIFT | 0x05;
							break;
						case XK_l :
						case XK_L :
							keycode = SHIFT | 0x00;
							break;
						case XK_m :
						case XK_M :
							keycode = SHIFT | 0x25;
							break;
						case XK_n :
						case XK_N :
							keycode = SHIFT | 0x23;
							break;
						case XK_o :
						case XK_O :
							keycode = SHIFT | 0x08;
							break;
						case XK_p :
						case XK_P :
							keycode = SHIFT | 0x0a;
							break;
						case XK_q :
						case XK_Q :
							keycode = SHIFT | 0x2f;
							break;
						case XK_r :
						case XK_R :
							keycode = SHIFT | 0x28;
							break;
						case XK_s :
						case XK_S :
							keycode = SHIFT | 0x3e;
							break;
						case XK_t :
						case XK_T :
							keycode = SHIFT | 0x2d;
							break;
						case XK_u :
						case XK_U :
							keycode = SHIFT | 0x0b;
							break;
						case XK_v :
						case XK_V :
							keycode = SHIFT | 0x10;
							break;
						case XK_w :
						case XK_W :
							keycode = SHIFT | 0x2e;
							break;
						case XK_x :
						case XK_X :
							keycode = SHIFT | 0x16;
							break;
						case XK_y :
						case XK_Y :
							keycode = SHIFT | 0x2b;
							break;
						case XK_z :
						case XK_Z :
							keycode = SHIFT | 0x17;
							break;
						default :
							keycode = -1;
							printf ("Pressed Keysym = %x\n", (int)keysym);
							break;
					}
					break;
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
						case XK_F2 :
							CONSOL |= 0x04;
							break;
						case XK_F3 :
							CONSOL |= 0x02;
							break;
						case XK_F4 :
							CONSOL |= 0x01;
							break;
						case XK_KP_0 :
							TRIG0 = 1;
							break;
						default :
							break;
					}
					break;
			}
		}
#endif

		if (keycode != -1)
		{
			KBCODE = CONTROL | keycode;
			IRQST = 0xbf;
			IRQ = TRUE;
		}
		else
		{
			KBCODE = 0xff;
		}

		if (NMIEN & 0x40)
		{
			static int	test_val = 0;

			NMIST = NMIST | 0x40;

#ifndef BASIC
#ifdef CURSES
{
	int	xpos;
	int	ypos;

	screenaddr = (GetByte (89) << 8) | GetByte (88);

	for (ypos=0;ypos<24;ypos++)
	{
		for (xpos=0;xpos<40;xpos++)
		{
			char	ch;

			ch = GetByte (screenaddr);

			switch (ch & 0x60)
			{
				case 0x00 :
					ch = 0x20 + ch;
					break;
				case 0x20 :
					ch = 0x40 + (ch - 0x20);
					break;
				case 0x40 :
					ch = (ch - 0x40);
					break;
				case 0x60 :
					break;
			}

			if (ch & 0x80)
			{
				attron (A_REVERSE);
				ch &= 0x7f;
			}
			else
			{
				attroff (A_REVERSE);
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
#else
			if (++test_val == 2)
			{
				Atari800_UpdateScreen ();
				test_val = 0;
			}
#endif
#endif

			NMI = TRUE;

			GO (-1);
		}
}
