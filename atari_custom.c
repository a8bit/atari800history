#include	<stdio.h>

#ifdef X11
#include	<X11/Xlib.h>
#include	<X11/Xutil.h>
#include	<X11/keysym.h>
#endif

#include	"system.h"
#include	"cpu.h"
#include	"atari_custom.h"

#define	FALSE	0
#define	TRUE	1
#define	TITLE	"Atari 800 Emulator, Version 0.1.1"

#ifdef BASIC
#define	WIDTH	320
#define	HEIGHT	192
#endif

#ifdef X11
#define	WIDTH	(384)
#define	HEIGHT	(192 + 24 + 24)
#endif

#ifdef X11
static Display	*display;
static Screen	*screen;
static Window	window;
static Visual	*visual;

static XComposeStatus	keyboard_status;

static XImage	*image;
static GC	gc;
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

static int	COLBK_pixel;
static int	COLPF0_pixel;
static int	COLPF1_pixel;
static int	COLPF2_pixel;
static int	COLPF3_pixel;
static int	COLPM0_pixel;
static int	COLPM1_pixel;
static int	COLPM2_pixel;
static int	COLPM3_pixel;

/*
	=============================
	Collision detection registers
	=============================
*/

static UBYTE	m0pf;
static UBYTE	m1pf;
static UBYTE	m2pf;
static UBYTE	m3pf;

static UBYTE	p0pf;
static UBYTE	p1pf;
static UBYTE	p2pf;
static UBYTE	p3pf;

static UBYTE	m0pl;
static UBYTE	m1pl;
static UBYTE	m2pl;
static UBYTE	m3pl;

static UBYTE	p0pl;
static UBYTE	p1pl;
static UBYTE	p2pl;
static UBYTE	p3pl;

#define	RAM		0
#define	ROM		1
#define	HARDWARE	2

UBYTE	memory[65536];

static UBYTE	attrib[65536];

int	countdown = 15000;

UBYTE Atari800_GetByte (UWORD addr);
void  Atari800_PutByte (UWORD addr, UBYTE byte);
UWORD Atari800_GetWord (UWORD addr);
void  Atari800_Hardware (void);

Atari800_Initialise ()
{
	int	i;

#ifdef X11
	XSetWindowAttributes	xswda;

	XGCValues	xgcvl;

	XColor	screen_colour;
	XColor	exact_colour;
	int	depth;

	char	*data;
	int	offset = 0;
	int	bitmap_pad = 8;
	int	bytes_per_line = 0;

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

	window = XCreateWindow (display,
			XRootWindowOfScreen(screen),
			50, 50,
			WIDTH, HEIGHT, 3, depth,
			InputOutput, visual,
			CWEventMask | CWBackPixel,
			&xswda);

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

	xgcvl.background = colours[0];
	xgcvl.foreground = colours[7];

	gc = XCreateGC (display, window,
		GCForeground | GCBackground,
		&xgcvl);

	XMapWindow (display, window);

	XSync (display, False);

	data = (char*) malloc (WIDTH * HEIGHT);
	if (!data)
	{
		printf ("Failed to allocate space for image\n");
		exit (1);
	}

	image = XCreateImage (display,
			visual,
			depth,
			ZPixmap,
			offset,
			data,
			WIDTH,
			HEIGHT,
			bitmap_pad,
			bytes_per_line);
	if (!image)
	{
		printf ("Failed to allocate image\n");
		exit (1);
	}
#endif

	for (i=0;i<65536;i++)
	{
		memory[i] = 0;
		attrib[i] = RAM;
	}

	memory[CONSOL] = 0x07;
/*
	=======================================
	Install functions for CPU memory access
	=======================================
*/
	GetByte = Atari800_GetByte;
	GetWord = Atari800_GetWord;
	PutByte = Atari800_PutByte;
	Hardware = Atari800_Hardware;

	printf ("CUSTOM: Initialised\n");
}

Atari800_Exit ()
{
#ifdef X11
	XSync (display, True);

	XDestroyImage (image);
	XUnmapWindow (display, window);
	XDestroyWindow (display, window);
	XCloseDisplay (display);
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
	=======================================================
	This routine will need to be modified to cope with bank
	switching. At the moment it will work since it is only
	used to obtain an address. If the CPU gets an address
	from the hardware registers it would crash anyway!
	=======================================================
*/

UWORD Atari800_GetWord (UWORD addr)
{
	return (memory[addr+1] << 8) | memory[addr];
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

static UWORD	scanline[WIDTH];
static int	ypos;

ScanLine_Show ()
{
	int	xpos;

	for (xpos=0;xpos<WIDTH;xpos++)
	{
		UWORD	pixel;
		UBYTE	playfield;
		UBYTE	player;
		int	colour = COLBK_pixel;

		pixel = scanline[xpos];
		playfield = pixel & 0x000f;
		player = (pixel >> 8) & 0x000f;

		switch (playfield)
		{
			case 0x01 :
				colour = COLPF0_pixel;
				break;
			case 0x02 :
				colour = COLPF1_pixel;
				break;
			case 0x04 :
				colour = COLPF2_pixel;
				break;
			case 0x08 :
				colour = COLPF3_pixel;
				break;
			default :
				break;
		}

		if (pixel & 0x8000)
		{
			m3pf |= playfield;
			m3pl |= player;
			colour = COLPM3_pixel;
		}

		if (pixel & 0x4000)
		{
			m2pf |= playfield;
			m2pl |= player;
			colour = COLPM2_pixel;
		}

		if (pixel & 0x2000)
		{
			m1pf |= playfield;
			m1pl |= player;
			colour = COLPM1_pixel;
		}

		if (pixel & 0x1000)
		{
			m0pf |= playfield;
			m0pl |= player;
			colour = COLPM0_pixel;
		}

		if (pixel & 0x0800)
		{
			p3pf |= playfield;
			p3pl |= player;
			colour = COLPM3_pixel;
		}

		if (pixel & 0x0400)
		{
			p2pf |= playfield;
			p2pl |= player;
			colour = COLPM2_pixel;
		}

		if (pixel & 0x0200)
		{
			p1pf |= playfield;
			p1pl |= player;
			colour = COLPM1_pixel;
		}

		if (pixel & 0x0100)
		{
			p0pf |= playfield;
			p0pl |= player;
			colour = COLPM0_pixel;
		}

		XPutPixel (image, xpos, ypos, colour);
	}
}

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

static UBYTE	PM_Width[4] = { 2, 4, 2, 8 };

static UBYTE	singleline;
static UWORD	pl0adr;
static UWORD	pl1adr;
static UWORD	pl2adr;
static UWORD	pl3adr;
static UWORD	m0123adr;

PM_InitFrame ()
{
	UWORD	pmbase = memory[PMBASE] << 8;

	switch (memory[DMACTL] & 0x10)
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

PM_ScanLine ()
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

	hposp0 = (memory[HPOSP0] - 0x20) << 1;
	hposp1 = (memory[HPOSP1] - 0x20) << 1;
	hposp2 = (memory[HPOSP2] - 0x20) << 1;
	hposp3 = (memory[HPOSP3] - 0x20) << 1;

	hposm0 = (memory[HPOSM0] - 0x20) << 1;
	hposm1 = (memory[HPOSM1] - 0x20) << 1;
	hposm2 = (memory[HPOSM2] - 0x20) << 1;
	hposm3 = (memory[HPOSM3] - 0x20) << 1;

	s0 = PM_Width[memory[SIZEP0] & 0x03];
	s1 = PM_Width[memory[SIZEP1] & 0x03];
	s2 = PM_Width[memory[SIZEP2] & 0x03];
	s3 = PM_Width[memory[SIZEP3] & 0x03];
	sm = PM_Width[memory[SIZEM] & 0x03];

	for (i=0;i<8;i++)
	{
		int	j;

		if (grafp0 & 0x80)
		{
			for (j=0;j<s0;j++)
			{
				if ((hposp0 >= 0) && (hposp0 < WIDTH))
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
				if ((hposp1 >= 0) && (hposp1 < WIDTH))
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
				if ((hposp2 >= 0) && (hposp2 < WIDTH))
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
				if ((hposp3 >= 0) && (hposp3 < WIDTH))
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
						if ((hposm3 >= 0) && (hposm3 < WIDTH))
							scanline[hposm3] |= 0x8000;
						hposm3++;
						break;
					case 0x02 :
						if ((hposm2 >= 0) && (hposm2 < WIDTH))
							scanline[hposm2] |= 0x4000;
						hposm2++;
						break;
					case 0x04 :
						if ((hposm1 >= 0) && (hposm1 < WIDTH))
							scanline[hposm1] |= 0x2000;
						hposm1++;
						break;
					case 0x06 :
						if ((hposm0 >= 0) && (hposm0 < WIDTH))
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

/*
	*****************************************************************
	*								*
	*	Section			:	Antic Display Modes	*
	*	Original Author		:	David Firth		*
	*	Date Written		:	28th May 1995		*
	*	Version			:	1.0			*
	*								*
	*****************************************************************
*/

static UWORD	screenaddr;
static UWORD	chbase;

static int	xmin;
static int	xmax;

antic_blank (int nlines)
{
	while (nlines > 0)
	{
		int	i;

		for (i=0;i<WIDTH;i++) scanline[i] = 0;

		if (memory[DMACTL] & 0x0c)
		{
			PM_ScanLine ();
		}

		ScanLine_Show ();

		ypos++;
		nlines--;
	}
}

antic_2 ()
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

	fprintf (stderr, "ANTIC: mode 2\n");

	nchars = (xmax - xmin) / 8;
/*
	==========================
	Check for vertical reflect
	==========================
*/
	if (memory[CHACTL] & 0x04)
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
	if (memory[CHACTL] & 0x02)
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
	if (memory[CHACTL] & 0x01)
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

		screendata = GetByte (screenaddr++);
		chadr[i] = chbase + (UWORD)(screendata & 0x7f) * 8 + charoffset;
		invert[i] = screendata & invert_mask;

		if (screendata & 0x80)
			blank[i] = screendata & blank_mask;
		else
			blank[i] = 0x80;
	}

	for (i=0;i<8;i++)
	{
		int	j;
		int	xpos = xmin;

		for (j=0;j<WIDTH;j++) scanline[j] = 0;

		for (j=0;j<nchars;j++)
		{
			UBYTE	chdata;
			int	t_invert;
			int	t_blank;
			int	k;

			chdata = memory[chadr[j]];
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

		if (memory[DMACTL] & 0x0c)
			PM_ScanLine ();

		ScanLine_Show ();

		ypos++;
	}
}

antic_3 ()
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

	fprintf (stderr, "ANTIC: mode 3\n");

	nchars = (xmax - xmin) / 8;
/*
	==========================
	Check for vertical reflect
	==========================
*/
	if (memory[CHACTL] & 0x04)
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
	if (memory[CHACTL] & 0x02)
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
	if (memory[CHACTL] & 0x01)
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

		screendata = GetByte (screenaddr++);
		chadr[i] = chbase + (UWORD)(screendata & 0x7f) * 8 + charoffset;
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

		for (j=0;j<WIDTH;j++) scanline[j] = 0;

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

		if (memory[DMACTL] & 0x0c)
			PM_ScanLine ();

		ScanLine_Show ();

		ypos++;
	}
}

antic_4 ()
{
	UBYTE	screendata[48];
	UWORD	chadr[48];
	int	charoffset;
	int	chardelta;
	int	i;

	int	nchars;

	fprintf (stderr, "ANTIC: mode 4\n");

	nchars = (xmax - xmin) / 8;
/*
	==========================
	Check for vertical reflect
	==========================
*/
	if (memory[CHACTL] & 0x04)
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
		screendata[i] = GetByte (screenaddr++);
		chadr[i] = chbase + (UWORD)(screendata[i] & 0x7f) * 8 + charoffset;
	}

	for (i=0;i<8;i++)
	{
		int	j;
		int	xpos = xmin;

		for (j=0;j<WIDTH;j++) scanline[j] = 0;

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

		if (memory[DMACTL] & 0x0c)
			PM_ScanLine ();

		ScanLine_Show ();

		ypos++;
	}
}

antic_5 ()
{
	UBYTE	screendata[48];
	UWORD	chadr[48];
	int	charoffset;
	int	chardelta;
	int	i;

	int	nchars;

	fprintf (stderr, "ANTIC: mode 5\n");

	nchars = (xmax - xmin) / 8;
/*
	==========================
	Check for vertical reflect
	==========================
*/
	if (memory[CHACTL] & 0x04)
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
		screendata[i] = GetByte (screenaddr++);
		chadr[i] = chbase + (UWORD)(screendata[i] & 0x7f) * 8 + charoffset;
	}

	for (i=0;i<8;i++)
	{
		int	j;
		int	xpos = xmin;

		for (j=0;j<WIDTH;j++) scanline[j] = 0;

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

		if (memory[DMACTL] & 0x0c)
		{
			PM_ScanLine ();
			ScanLine_Show ();
			ypos++;

			for (j=0;j<WIDTH;j++) scanline[j] &= 0x000f;

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

antic_6 ()
{
	UBYTE	screendata[24];
	UWORD	chadr[24];
	int	charoffset;
	int	chardelta;
	int	i;

	int	nchars;

	fprintf (stderr, "ANTIC: mode 6\n");

	nchars = (xmax - xmin) / 16;
/*
	==========================
	Check for vertical reflect
	==========================
*/
	if (memory[CHACTL] & 0x04)
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
		screendata[i] = GetByte (screenaddr++);
		chadr[i] = chbase + (UWORD)(screendata[i] & 0x3f) * 8 + charoffset;
	}

	for (i=0;i<8;i++)
	{
		int	j;
		int	xpos = xmin;

		for (j=0;j<WIDTH;j++) scanline[j] = 0;

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

		if (memory[DMACTL] & 0x0c)
			PM_ScanLine ();

		ScanLine_Show ();

		ypos++;
	}
}

antic_7 ()
{
	UBYTE	screendata[24];
	UWORD	chadr[24];
	int	charoffset;
	int	chardelta;
	int	i;

	int	nchars;

	fprintf (stderr, "ANTIC: mode 7\n");

	nchars = (xmax - xmin) / 16;
/*
	==========================
	Check for vertical reflect
	==========================
*/
	if (memory[CHACTL] & 0x04)
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
		screendata[i] = GetByte (screenaddr++);
		chadr[i] = chbase + (UWORD)(screendata[i] & 0x3f) * 8 + charoffset;
	}

	for (i=0;i<8;i++)
	{
		int	j;
		int	xpos = xmin;

		for (j=0;j<WIDTH;j++) scanline[j] = 0;

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

		if (memory[DMACTL] & 0x0c)
		{
			PM_ScanLine ();
			ScanLine_Show ();
			ypos++;

			for (j=0;j<WIDTH;j++) scanline[j] &= 0x000f;

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

antic_8 ()
{
	int	xpos;
	int	nbytes;
	int	i;
	int	j;

	fprintf (stderr, "ANTIC: mode 8\n");

	nbytes = (xmax - xmin) / 32;

	xpos = xmin;

	for (i=0;i<WIDTH;i++) scanline[i] = 0;

	for (i=0;i<nbytes;i++)
	{
		UBYTE	screendata;

		screendata = GetByte (screenaddr++);

		for (j=0;j<4;j++)
		{
			int	colour;

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

	if (memory[DMACTL] & 0x0c)
	{
		for (i=0;i<8;i++)
		{
			PM_ScanLine ();
			ScanLine_Show ();
			ypos++;

			if (i < 7)
				for (j=0;j<WIDTH;j++) scanline[j] &= 0x000f;
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

antic_9 ()
{
	int	xpos;
	int	nbytes;
	int	i;
	int	j;

	fprintf (stderr, "ANTIC: mode 9\n");

	nbytes = (xmax - xmin) / 32;

	xpos = xmin;

	for (i=0;i<WIDTH;i++) scanline[i] = 0;

	for (i=0;i<nbytes;i++)
	{
		UBYTE	screendata;

		screendata = GetByte (screenaddr++);

		for (j=0;j<8;j++)
		{
			int	colour;

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

	if (memory[DMACTL] & 0x0c)
	{
		for (i=0;i<4;i++)
		{
			PM_ScanLine ();
			ScanLine_Show ();
			ypos++;


			if (i < 3)
				for (j=0;j<WIDTH;j++) scanline[j] &= 0x000f;
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

antic_a ()
{
	int	xpos;
	int	nbytes;
	int	i;
	int	j;

	fprintf (stderr, "ANTIC: mode a\n");

	nbytes = (xmax - xmin) / 16;

	xpos = xmin;

	for (i=0;i<WIDTH;i++) scanline[i] = 0;

	for (i=0;i<nbytes;i++)
	{
		UBYTE	screendata;

		screendata = GetByte (screenaddr++);

		for (j=0;j<4;j++)
		{
			int	colour;

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

	if (memory[DMACTL] & 0x0c)
	{
		for (i=0;i<4;i++)
		{
			PM_ScanLine ();
			ScanLine_Show ();
			ypos++;

			if (i < 3)
				for (j=0;j<WIDTH;j++) scanline[j] &= 0x000f;
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

antic_b ()
{
	int	xpos;
	int	nbytes;
	int	i;

	fprintf (stderr, "ANTIC: mode b\n");

	nbytes = (xmax - xmin) / 16;

	xpos = xmin;

	for (i=0;i<WIDTH;i++) scanline[i] = 0;

	for (i=0;i<nbytes;i++)
	{
		UBYTE	screendata;
		int	j;

		screendata = GetByte (screenaddr++);

		for (j=0;j<8;j++)
		{
			int	colour;

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

	if (memory[DMACTL] & 0x0c)
	{
		PM_ScanLine ();
		ScanLine_Show ();
		ypos++;

		for (i=0;i<WIDTH;i++) scanline[i] &= 0x000f;

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

antic_c ()
{
	int	xpos;
	int	nbytes;
	int	i;

	fprintf (stderr, "ANTIC: mode c\n");

	nbytes = (xmax - xmin) / 16;

	xpos = xmin;

	for (i=0;i<WIDTH;i++) scanline[i] = 0;

	for (i=0;i<nbytes;i++)
	{
		UBYTE	screendata;
		int	j;

		screendata = GetByte (screenaddr++);

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

	if (memory[DMACTL] & 0x0c)
		PM_ScanLine ();

	ScanLine_Show ();

	ypos++;
}

antic_d ()
{
	int	xpos;
	int	nbytes;
	int	i;

	fprintf (stderr, "ANTIC: mode d\n");

	nbytes = (xmax - xmin) / 8;

	xpos = xmin;

	for (i=0;i<WIDTH;i++) scanline[i] = 0;

	for (i=0;i<nbytes;i++)
	{
		UBYTE	screendata;
		int	j;

		screendata = GetByte (screenaddr++);

		for (j=0;j<4;j++)
		{
			int	colour;

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

	if (memory[DMACTL] & 0x0c)
	{
		PM_ScanLine ();
		ScanLine_Show ();
		ypos++;

		for (i=0;i<WIDTH;i++) scanline[i] &= 0x000f;

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

antic_e ()
{
	int	xpos;
	int	nbytes;
	int	i;

	fprintf (stderr, "ANTIC: mode e\n");

	nbytes = (xmax - xmin) / 8;

	xpos = xmin;

	for (i=0;i<WIDTH;i++) scanline[i] = 0;

	for (i=0;i<nbytes;i++)
	{
		UBYTE	screendata;
		int	j;

		screendata = GetByte (screenaddr++);

		for (j=0;j<4;j++)
		{
			int	colour;

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

	if (memory[DMACTL] & 0x0c)
		PM_ScanLine ();

	ScanLine_Show ();

	ypos++;
}

antic_f ()
{
	int	xpos;
	int	nbytes;
	int	i;

	fprintf (stderr, "ANTIC: mode f\n");

	nbytes = (xmax - xmin) / 8;

	xpos = xmin;

	for (i=0;i<WIDTH;i++) scanline[i] = 0;

	for (i=0;i<nbytes;i++)
	{
		UBYTE	screendata;
		int	j;

		screendata = GetByte (screenaddr++);

		for (j=0;j<8;j++)
		{
			if (screendata & 0x80)
				scanline[xpos++] |= 0x0002;
			else
				scanline[xpos++] |= 0x0004;

			screendata = screendata << 1;
		}
	}

	if (memory[DMACTL] & 0x0c)
		PM_ScanLine ();

	ScanLine_Show ();

	ypos++;
}

/*
	*****************************************************************
	*								*
	*	Section			:	Display List		*
	*	Original Author		:	David Firth		*
	*	Date Written		:	28th May 1995		*
	*	Version			:	1.0			*
	*								*
	*****************************************************************
*/

Atari800_UpdateScreen ()
{
	UWORD	dlist;

	int	JVB;
	int	xpos;
	int	abort_count = 300;

	PM_InitFrame ();

	ypos = 0;

	if (memory[DMACTL] & 0x20)
	{
		fprintf (stderr, "Atari800_UpdateScreen: started\n");

		dlist = (GetByte (DLISTH) << 8) | GetByte (DLISTL);

		JVB = FALSE;

		do
		{
			UBYTE	IR;
			UBYTE	screendata;

			int	i;

			if (abort_count-- == 0)
			{
				fprintf (stderr, "DLIST: ABORT\n");
				break;
			}

			IR = GetByte (dlist++);

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
						dlist = (GetByte (DLISTH) << 8) | GetByte (DLISTL);
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

					chbase = GetByte (CHBASE) << 8;

					switch (memory[DMACTL] & 0x03)
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
				if (memory[NMIEN] & 0x80)
				{
					memory[NMIST] = memory[NMIST] | 0x80;

					NMI = TRUE;

					GO (-1);
				}
			}
		} while (!JVB && (ypos < HEIGHT));
	}

	antic_blank (HEIGHT - ypos);

#ifdef X11
	XPutImage (display, window, gc, image, 0, 0, 0, 0, WIDTH, HEIGHT);
#endif

	fprintf (stderr, "Atari800_UpdateScreen: finished\n");
}

static int	CONTROL = 0x00;
static int	SHIFT = 0x00;

static int	count = 0;

UBYTE Atari800_GetByte (UWORD addr)
{
	UBYTE	byte;

	if (attrib[addr] == HARDWARE)
	{
		switch (addr)
		{
			case CHBASE :
				byte = memory[addr];
				break;
			case CHACTL :
				byte = memory[addr];
				break;
			case CONSOL :
				byte = memory[CONSOL];
				break;
			case DLISTL :
				byte = memory[addr];
				break;
			case DLISTH :
				byte = memory[addr];
				break;
			case DMACTL :
				byte = memory[addr];
				break;
			case KBCODE :
				byte = memory[addr];
				break;
			case IRQST :
				byte = memory[addr];
				break;
			case M0PF :
				byte = m0pf;
				break;
			case M1PF :
				byte = m1pf;
				break;
			case M2PF :
				byte = m2pf;
				break;
			case M3PF :
				byte = m3pf;
				break;
			case M0PL :
				byte = m0pl;
				break;
			case M1PL :
				byte = m1pl;
				break;
			case M2PL :
				byte = m2pl;
				break;
			case M3PL :
				byte = m3pl;
				break;
			case P0PF :
				byte = p0pf;
				break;
			case P1PF :
				byte = p1pf;
				break;
			case P2PF :
				byte = p2pf;
				break;
			case P3PF :
				byte = p3pf;
				break;
			case P0PL :
				byte = p0pl;
				break;
			case P1PL :
				byte = p1pl;
				break;
			case P2PL :
				byte = p2pl;
				break;
			case P3PL :
				byte = p3pl;
				break;
			case PORTA :
			case PORTB :
				byte = memory[addr];
				break;
			case RANDOM :
				byte = count & 0xff;
				break;
			case TRIG0 :
			case TRIG1 :
			case TRIG2 :
			case TRIG3 :
				byte = memory[addr];
				break;
			case VCOUNT :
				byte = memory[addr]++;
				break;
			case NMIEN :
			case NMIST :
				byte = memory[addr];
				break;
			case SKSTAT :
				if (SHIFT)
					byte = 0x05;
				else
					byte = 0x0d;
				break;
			case WSYNC :
				break;
			default :
				fprintf (stderr, "read from %04x\n", addr);
				byte = 0;
				break;
		}
	}
	else
	{
		byte = memory[addr];
	}

	return byte;
}

void Atari800_PutByte (UWORD addr, UBYTE byte)
{
	if (attrib[addr] == RAM)
	{
		memory[addr] = byte;
	}
	else if (attrib[addr] == HARDWARE)
	{
		switch (addr)
		{
			case AUDC1 :
			case AUDC2 :
			case AUDC3 :
			case AUDC4 :
			case AUDCTL :
			case AUDF1 :
			case AUDF2 :
			case AUDF3 :
			case AUDF4 :
				break;
			case CHBASE :
				memory[addr] = byte;
				break;
			case CHACTL :
				memory[addr] = byte;
				break;
			case COLBK :
#ifdef X11
				COLBK_pixel = colours[byte];
#endif
				break;
			case COLPF0 :
#ifdef X11
				COLPF0_pixel = colours[byte];
#endif
				break;
			case COLPF1 :
#ifdef X11
				COLPF1_pixel = colours[byte];
#endif
				break;
			case COLPF2 :
#ifdef X11
				COLPF2_pixel = colours[byte];
#endif
				break;
			case COLPF3 :
#ifdef X11
				COLPF3_pixel = colours[byte];
#endif
				break;
			case COLPM0 :
#ifdef X11
				COLPM0_pixel = colours[byte];
#endif
				break;
			case COLPM1 :
#ifdef X11
				COLPM1_pixel = colours[byte];
#endif
				break;
			case COLPM2 :
#ifdef X11
				COLPM2_pixel = colours[byte];
#endif
				break;
			case COLPM3 :
#ifdef X11
				COLPM3_pixel = colours[byte];
#endif
				break;
			case CONSOL :
				break;
			case DLISTL :
				memory[addr] = byte;
				break;
			case DLISTH :
				memory[addr] = byte;
				break;
			case DMACTL :
				memory[addr] = byte;
				break;
			case HITCLR :
				m0pf = m1pf = m2pf = m3pf = 0;
				p0pf = p1pf = p2pf = p3pf = 0; 
				m0pl = m1pl = m2pl = m3pl = 0; 
				p0pl = p1pl = p2pl = p3pl = 0;
				break;
			case HPOSM0 :
			case HPOSM1 :
			case HPOSM2 :
			case HPOSM3 :
			case HPOSP0 :
			case HPOSP1 :
			case HPOSP2 :
			case HPOSP3 :
				memory[addr] = byte;
				break;
			case NMIEN :
				memory[addr] = byte;
				break;
			case NMIRES :
				memory[addr] = 0;
				break;
			case PMBASE :
				memory[addr] = byte;
				break;
			case SIZEM :
			case SIZEP0 :
			case SIZEP1 :
			case SIZEP2 :
			case SIZEP3 :
				memory[addr] = byte;
				break;
			case WSYNC :
				break;
			default :
				fprintf (stderr, "write %02x to %04x\n", byte, addr);
				break;
		}
	}
}

void Atari800_Hardware (void)
{
	static int	pil_on = FALSE;

	if (!count--)
	{
		int	keycode = -1;

		memory[NMIST] = 0x00;

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
							memory[0x244] = 1;
						case XK_F1 :
							memory[NMIST] = 0x20;
							NMI = TRUE;
							break;
						case XK_F2 :
							memory[CONSOL] &= ~0x04;
							break;
						case XK_F3 :
							memory[CONSOL] &= ~0x02;
							break;
						case XK_F4 :
							memory[CONSOL] &= ~0x01;
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
							memory[IRQST] = 0x7f;
							IRQ = TRUE;
							break;
						case XK_KP_0 :
							memory[TRIG0] = 0;
							break;
						case XK_KP_1 :
							memory[PORTA] = 0xf9;
							break;
						case XK_KP_2 :
							memory[PORTA] = 0xfd;
							break;
						case XK_KP_3 :
							memory[PORTA] = 0xf5;
							break;
						case XK_KP_4 :
							memory[PORTA] = 0xfb;
							break;
						case XK_KP_5 :
							memory[PORTA] = 0xff;
							break;
						case XK_KP_6 :
							memory[PORTA] = 0xf7;
							break;
						case XK_KP_7 :
							memory[PORTA] = 0xfa;
							break;
						case XK_KP_8 :
							memory[PORTA] = 0xfe;
							break;
						case XK_KP_9 :
							memory[PORTA] = 0xf6;
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
							printf ("Pressed Keysym = %x (%0x, %c)\n", keysym, keycode);
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
							memory[CONSOL] |= 0x04;
							break;
						case XK_F3 :
							memory[CONSOL] |= 0x02;
							break;
						case XK_F4 :
							memory[CONSOL] |= 0x01;
							break;
						case XK_KP_0 :
							memory[TRIG0] = 1;
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
			memory[KBCODE] = CONTROL | keycode;
			memory[IRQST] = 0xbf;
			IRQ = TRUE;
		}
		else
		{
			memory[KBCODE] = 0xff;
		}

		if (memory[NMIEN] & 0x40)
		{
			static int	test_val = 0;

			memory[NMIST] = memory[NMIST] | 0x40;

			if (!(test_val++ & 0x0f))
				Atari800_UpdateScreen ();

			NMI = TRUE;

			GO (-1);
		}

		count = countdown;
	}
}
