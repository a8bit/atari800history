#include	<stdio.h>
#include	<stdlib.h>

#include	"system.h"
#include	"cpu.h"
#include	"atari.h"
#include	"atari_custom.h"

#define	FALSE	0
#define	TRUE	1

static int dlicount;
static int scroll_offset;

int countdown_rate = 4000;
int refresh_rate = 4;

#define	Atari_ScanLine()	{ \
				    dlicount--; \
				    if (dlicount == 0) \
				    { \
				      NMIST |= 0x80; \
				      INTERRUPT |= NMI_MASK; \
				    } \
				    VCOUNT=ypos>>1; \
				    ncycles=48; \
				    GO(); \
				    PM_ScanLine (); \
				    Atari_ScanLine2(pf_scanline); \
				}

static UBYTE screen[ATARI_HEIGHT * ATARI_WIDTH];
static UBYTE *scrn_ptr;

/*
	=========================================
	Allocate variables for Hardware Registers
	=========================================
*/

UBYTE	ALLPOT;
UBYTE	CHACTL;
UBYTE	CHBASE;
UBYTE	COLBK;
UBYTE	DLISTH;
UBYTE	DLISTL;
UBYTE	DMACTL;
UBYTE	GRACTL;
UBYTE	GRAFM;
UBYTE	GRAFP0;
UBYTE	GRAFP1;
UBYTE	GRAFP2;
UBYTE	GRAFP3;
UBYTE	HSCROL;
UBYTE	IRQEN;
UBYTE	IRQST;
UBYTE	KBCODE;
UBYTE	M0PF;
UBYTE	M0PL;
UBYTE	M1PF;
UBYTE	M1PL;
UBYTE	M2PF;
UBYTE	M2PL;
UBYTE	M3PF;
UBYTE	M3PL;
UBYTE	NMIEN;
UBYTE	NMIST;
UBYTE	P0PF;
UBYTE	P0PL;
UBYTE	P1PF;
UBYTE	P1PL;
UBYTE	P2PF;
UBYTE	P2PL;
UBYTE	P3PF;
UBYTE	P3PL;
UBYTE	PACTL;
UBYTE	PAL;
UBYTE	PBCTL;
UBYTE	PENH;
UBYTE	PENV;
UBYTE	PMBASE;
UBYTE	PORTA;
UBYTE	PORTB;
UBYTE	POTGO;
UBYTE	PRIOR;
UBYTE	SERIN;
UBYTE	SEROUT;
UBYTE	SKCTL;
UBYTE	SKREST;
UBYTE	SKSTAT;
UBYTE	STIMER;
UBYTE	VCOUNT;
UBYTE	VDELAY;
UBYTE	VSCROL;

extern UBYTE	*cart_image;
extern int	cart_type;

/*
	============================================================
	atari_basic and under_basic are required for XL/XE emulation
	============================================================
*/

int	rom_inserted;
UBYTE	atari_basic[8192];
UBYTE	atarixl_os[16384];
UBYTE	under_atari_basic[8192];
UBYTE	under_atarixl_os[16384];

static int	PM_XPos[256];
static UBYTE	PM_Width[4] = { 2, 4, 2, 8 };

#ifndef BASIC
static UBYTE pf_scroll_scanline[ATARI_WIDTH+30];
static UBYTE *pf_scanline = &pf_scroll_scanline[30];
static UBYTE pm_scanline[ATARI_WIDTH];
int   ypos;
#endif

UBYTE Atari800_GetByte (UWORD addr);
void  Atari800_PutByte (UWORD addr, UBYTE byte);

void Atari800_Initialise (int *argc, char *argv[])
{
  int	i, j;

  Atari_Initialise (argc, argv);

  for (i=j=1;i<*argc;i++)
    {
      if (strcmp(argv[i],"-refresh") == 0)
	sscanf (argv[++i],"%d", &refresh_rate);
      else if (strcmp(argv[i],"-countdown") == 0)
	sscanf (argv[++i],"%d", &countdown_rate);
      else
	argv[j++] = argv[i];
    }

  *argc = j;

  if (refresh_rate < 1)
    refresh_rate = 1;

  if (countdown_rate < 1000)
    countdown_rate = 1000;

  for (i=0;i<65536;i++)
    {
      memory[i] = 0;
      attrib[i] = RAM;
    }
/*
	=========================================================
	Initialise tables for Player Missile Horizontal Positions
	=========================================================
*/
#ifndef BASIC
  for (i=0;i<256;i++)
    {
      PM_XPos[i] = (i - 0x20) << 1;
    }
#endif

/*
	=============================
	Initialise Hardware Registers
	=============================
*/

  PORTA = 0xff;
  PORTB = 0xff;

#ifndef BASIC
  SetPrior (0);
#endif
}

void Atari800_Exit (int run_monitor)
{
  Atari_Exit (run_monitor);
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
	*	Section			:	Player Missile Graphics	*
	*	Original Author		:	David Firth		*
	*	Date Written		:	28th May 1995		*
	*	Version			:	1.0			*
	*								*
	*****************************************************************
*/

static int global_hposp0;
static int global_hposp1;
static int global_hposp2;
static int global_hposp3;
static int global_hposm0;
static int global_hposm1;
static int global_hposm2;
static int global_hposm3;
static int global_sizep0;
static int global_sizep1;
static int global_sizep2;
static int global_sizep3;
static int global_sizem;

/*
	=========================================
	Width of each bit within a Player/Missile
	=========================================
*/

#ifndef BASIC

static UBYTE	singleline;
static UWORD	pl0adr;
static UWORD	pl1adr;
static UWORD	pl2adr;
static UWORD	pl3adr;
static UWORD	m0123adr;

void PM_InitFrame ()
{
  UWORD	pmbase = PMBASE << 8;

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

void PM_DMA ()
{
  int	nextdata;

  GRAFP0 = memory[pl0adr];
  GRAFP1 = memory[pl1adr];
  GRAFP2 = memory[pl2adr];
  GRAFP3 = memory[pl3adr];
  GRAFM = memory[m0123adr];

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

void PM_ScanLine ()
{
/*
   ==================
   Clear PMG Scanline
   ==================
*/
  memset (pm_scanline, 0, ATARI_WIDTH);
/*
   =============================
   Display graphics for Player 0
   =============================
*/
  if (GRAFP0)
    {
      UBYTE grafp0 = GRAFP0;
      int sizep0 = global_sizep0;
      int hposp0 = global_hposp0;
      int i;

      for (i=0;i<8;i++)
	{
	  if (grafp0 & 0x80)
	    {
	      int j;

	      for (j=0;j<sizep0;j++)
		{
		  if ((hposp0 >= 0) && (hposp0 < ATARI_WIDTH))
		    {
		      UBYTE playfield = pf_scanline[hposp0];

		      pm_scanline[hposp0] |= 0x01;

		      P0PF |= playfield;
		    }
		  hposp0++;
		}
	    }
	  else
	    {
	      hposp0 += sizep0;
	    }

	  grafp0 = grafp0 << 1;
	}
    }
/*
   =============================
   Display graphics for Player 1
   =============================
*/
  if (GRAFP1)
    {
      UBYTE grafp1 = GRAFP1;
      int sizep1 = global_sizep1;
      int hposp1 = global_hposp1;
      int i;

      for (i=0;i<8;i++)
	{
	  if (grafp1 & 0x80)
	    {
	      int j;

	      for (j=0;j<sizep1;j++)
		{
		  if ((hposp1 >= 0) && (hposp1 < ATARI_WIDTH))
		    {
		      UBYTE playfield = pf_scanline[hposp1];
		      UBYTE player = pm_scanline[hposp1];

		      pm_scanline[hposp1] |= 0x02;

		      P1PF |= playfield;
		      P1PL |= player;

		      if (player & 0x01)
			P0PL |= 0x02;
		    }
		  hposp1++;
		}
	    }
	  else
	    {
	      hposp1 += sizep1;
	    }

	  grafp1 = grafp1 << 1;
	}
    }
/*
   =============================
   Display graphics for Player 2
   =============================
*/
  if (GRAFP2)
    {
      UBYTE grafp2 = GRAFP2;
      int sizep2 = global_sizep2;
      int hposp2 = global_hposp2;
      int i;

      for (i=0;i<8;i++)
	{
	  if (grafp2 & 0x80)
	    {
	      int j;

	      for (j=0;j<sizep2;j++)
		{
		  if ((hposp2 >= 0) && (hposp2 < ATARI_WIDTH))
		    {
		      UBYTE playfield = pf_scanline[hposp2];
		      UBYTE player = pm_scanline[hposp2];

		      pm_scanline[hposp2] |= 0x04;

		      P2PF |= playfield;
		      P2PL |= player;

		      if (player & 0x01)
			P0PL |= 0x04;
		      if (player & 0x02)
			P1PL |= 0x04;
		    }
		  hposp2++;
		}
	    }
	  else
	    {
	      hposp2 += sizep2;
	    }

	  grafp2 = grafp2 << 1;
	}
    }
/*
   =============================
   Display graphics for Player 3
   =============================
*/
  if (GRAFP3)
    {
      UBYTE grafp3 = GRAFP3;
      int sizep3 = global_sizep3;
      int hposp3 = global_hposp3;
      int i;

      for (i=0;i<8;i++)
	{
	  if (grafp3 & 0x80)
	    {
	      int j;

	      for (j=0;j<sizep3;j++)
		{
		  if ((hposp3 >= 0) && (hposp3 < ATARI_WIDTH))
		    {
		      UBYTE playfield = pf_scanline[hposp3];
		      UBYTE player = pm_scanline[hposp3];

		      pm_scanline[hposp3] |= 0x08;

		      P3PF |= playfield;
		      P3PL |= player;

		      if (player & 0x01)
			P0PL |= 0x08;
		      if (player & 0x02)
			P1PL |= 0x08;
		      if (player & 0x04)
			P2PL |= 0x08;
		    }
		  hposp3++;
		}
	    }
	  else
	    {
	      hposp3 += sizep3;
	    }

	  grafp3 = grafp3 << 1;
	}
    }
/*
   =============================
   Display graphics for Missiles
   =============================
*/
  if (GRAFM)
    {
      UBYTE grafm = GRAFM;
      int hposm0 = global_hposm0;
      int hposm1 = global_hposm1;
      int hposm2 = global_hposm2;
      int hposm3 = global_hposm3;
      int sizem = global_sizem;
      int i;

      for (i=0;i<8;i++)
	{
	  if (grafm & 0x80)
	    {
	      int j;

	      for (j=0;j<sizem;j++)
		{
		  switch (i & 0x06)
		    {
		    case 0x00 :
		      if ((hposm3 >= 0) && (hposm3 < ATARI_WIDTH))
			{
			  UBYTE playfield = pf_scanline[hposm3];
			  UBYTE player = pm_scanline[hposm3];

			  pm_scanline[hposm3] |= 0x80;

			  M3PF |= playfield;
			  M3PL |= player;
			}
		      hposm3++;
		      break;
		    case 0x02 :
		      if ((hposm2 >= 0) && (hposm2 < ATARI_WIDTH))
			{
			  UBYTE playfield = pf_scanline[hposm2];
			  UBYTE player = pm_scanline[hposm2];

			  pm_scanline[hposm2] |= 0x40;

			  M2PF |= playfield;
			  M2PL |= player;
			}
		      hposm2++;
		      break;
		    case 0x04 :
		      if ((hposm1 >= 0) && (hposm1 < ATARI_WIDTH))
			{
			  UBYTE playfield = pf_scanline[hposm1];
			  UBYTE player = pm_scanline[hposm1];

			  pm_scanline[hposm1] |= 0x20;

			  M1PF |= playfield;
			  M1PL |= player;
			}
		      hposm1++;
		      break;
		    case 0x06 :
		      if ((hposm0 >= 0) && (hposm0 < ATARI_WIDTH))
			{
			  UBYTE playfield = pf_scanline[hposm0];
			  UBYTE player = pm_scanline[hposm0];

			  pm_scanline[hposm0] |= 0x10;

			  M0PF |= playfield;
			  M0PL |= player;
			}
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
		  hposm3 += sizem;
		  break;
		case 0x02 :
		  hposm2 += sizem;
		  break;
		case 0x04 :
		  hposm1 += sizem;
		  break;
		case 0x06 :
		  hposm0 += sizem;
		  break;
		}
	    }
      
	  grafm = grafm << 1;
	}
    }
}

static UBYTE colreg_lookup[4096];
static UBYTE colour_lookup[9];

SetPrior (int new_prior)
{
  UBYTE prior[9];
  int pixel;

  PRIOR = new_prior;

  switch (PRIOR & 0x0f)
    {
    default :
    case 0x01 :
      prior[0] = 8; prior[1] = 7; prior[2] = 6; prior[3] = 5;
      prior[4] = 4; prior[5] = 3; prior[6] = 2; prior[7] = 1;
      prior[8] = 0;
      break;
    case 0x02 :
      prior[0] = 8; prior[1] = 7; prior[2] = 2; prior[3] = 1;
      prior[4] = 6; prior[5] = 5; prior[6] = 4; prior[7] = 3;
      prior[8] = 0;
      break;
    case 0x04 :
      prior[0] = 4; prior[1] = 3; prior[2] = 2; prior[3] = 1;
      prior[4] = 8; prior[5] = 7; prior[6] = 6; prior[7] = 5;
      prior[8] = 0;
      break;
    case 0x08 :
      prior[0] = 6; prior[1] = 5; prior[2] = 4; prior[3] = 3;
      prior[4] = 8; prior[5] = 7; prior[6] = 2; prior[7] = 1;
      prior[8] = 0;
      break;
    }
      
  for (pixel=0;pixel<4096;pixel++)
    {
      UBYTE colreg;
      UBYTE curpri;
	  
      colreg = 8;
      curpri = prior[8];

      if ((pixel & 0x800) && (prior[7] > curpri)) /* COLPF3 */
	{
	  colreg = 7;
	  curpri = prior[7];
	}

      if ((pixel & 0x400) && (prior[6] > curpri)) /* COLPF2 */
	{
	  colreg = 6;
	  curpri = prior[6];
	}

      if ((pixel & 0x200) && (prior[5] > curpri)) /* COLPF1 */
	{
	  colreg = 5;
	  curpri = prior[5];
	}

      if ((pixel & 0x100) && (prior[4] > curpri)) /* COLPF0 */
	{
	  colreg = 4;
	  curpri = prior[4];
	}

      if (pixel & 0xf0) /* Check for Missile Pixels */
	{
	  if ((pixel & 0x80) && (prior[3] > curpri)) /* COLPM3 */
	    {
	      colreg = 3;
	      curpri = prior[3];
	    }

	  if ((pixel & 0x40) && (prior[2] > curpri)) /* COLPM2 */
	    {
	      colreg = 2;
	      curpri = prior[2];
	    }

	  if ((pixel & 0x20) && (prior[1] > curpri)) /* COLPM1 */
	    {
	      colreg = 1;
	      curpri = prior[1];
	    }

	  if ((pixel & 0x10) && (prior[0] > curpri)) /* COLPM0 */
	    {
	      colreg = 0;
	      curpri = prior[0];
	    }

	  if (PRIOR & 0x10)
	    colreg = 7;
	}

      if ((pixel & 0x08) && (prior[3] > curpri))
	{
	  colreg = 3;
	  curpri = prior[3];
	}

      if ((pixel & 0x04) && (prior[2] > curpri))
	{
	  colreg = 2;
	  curpri = prior[2];
	}

      if ((pixel & 0x02) && (prior[1] > curpri))
	{
	  colreg = 1;
	  curpri = prior[1];
	}

      if ((pixel & 0x01) && (prior[0] > curpri))
	{
	  colreg = 0;
	  curpri = prior[0];
	}
      
      colreg_lookup[pixel] = colreg;
    }
}

void Atari_ScanLine2 (UBYTE *pf_scanline)
{
  int xpos;
  UBYTE *pf_pixel_ptr;
  UBYTE *pm_pixel_ptr;

  UBYTE *t_scrn_ptr;

  t_scrn_ptr = scrn_ptr;

  if (PRIOR & 0xc0)
    {
      int nibble;

      pf_pixel_ptr = pf_scanline;
      pm_pixel_ptr = pm_scanline;

      for (xpos=0;xpos<ATARI_WIDTH;xpos++)
	{
	  UBYTE pf_pixel;
	  UBYTE pm_pixel;
	  UBYTE colreg;
	  UBYTE colour;

	  pf_pixel = *pf_pixel_ptr++;
	  pm_pixel = *pm_pixel_ptr++;

	  colreg = colreg_lookup[(pf_pixel << 8) | pm_pixel];
	  colour = colour_lookup[colreg];

	  if ((xpos & 0x03) == 0x00)
	    nibble = 0;

	  nibble = (nibble << 1) | ((pf_pixel >> 1) & 0x01);

	  if ((xpos & 0x03) == 0x03)
	    {
	      switch (PRIOR & 0xc0)
		{
		case 0x40 :
		  colour = (COLBK & 0xf0) | nibble;
		  break;
		case 0x80 :
		  if (nibble >= 8)
		    nibble -= 9;
		  colour = colour_lookup[nibble];
		  break;
		case 0xc0 :
		  colour = (nibble << 4) | (COLBK & 0x0f);
		  break;
		}

	      *t_scrn_ptr++ = colour;
	      *t_scrn_ptr++ = colour;
	      *t_scrn_ptr++ = colour;
	      *t_scrn_ptr++ = colour;
	    }
	}
    }
  else
    {
      pf_pixel_ptr = &pf_scanline[-scroll_offset];
      pm_pixel_ptr = pm_scanline;

      for (xpos=0;xpos<ATARI_WIDTH;xpos+=8)
	{
	  UBYTE pf_pixel;
	  UBYTE pm_pixel;
	  UBYTE colreg;
	  UBYTE colour;
	  
	  pf_pixel = *pf_pixel_ptr++;
	  pm_pixel = *pm_pixel_ptr++;
	  colreg = colreg_lookup[(pf_pixel << 8) | pm_pixel];
	  colour = colour_lookup[colreg];
	  *t_scrn_ptr++ = colour;

	  pf_pixel = *pf_pixel_ptr++;
	  pm_pixel = *pm_pixel_ptr++;
	  colreg = colreg_lookup[(pf_pixel << 8) | pm_pixel];
	  colour = colour_lookup[colreg];
	  *t_scrn_ptr++ = colour;

	  pf_pixel = *pf_pixel_ptr++;
	  pm_pixel = *pm_pixel_ptr++;
	  colreg = colreg_lookup[(pf_pixel << 8) | pm_pixel];
	  colour = colour_lookup[colreg];
	  *t_scrn_ptr++ = colour;

	  pf_pixel = *pf_pixel_ptr++;
	  pm_pixel = *pm_pixel_ptr++;
	  colreg = colreg_lookup[(pf_pixel << 8) | pm_pixel];
	  colour = colour_lookup[colreg];
	  *t_scrn_ptr++ = colour;

	  pf_pixel = *pf_pixel_ptr++;
	  pm_pixel = *pm_pixel_ptr++;
	  colreg = colreg_lookup[(pf_pixel << 8) | pm_pixel];
	  colour = colour_lookup[colreg];
	  *t_scrn_ptr++ = colour;

	  pf_pixel = *pf_pixel_ptr++;
	  pm_pixel = *pm_pixel_ptr++;
	  colreg = colreg_lookup[(pf_pixel << 8) | pm_pixel];
	  colour = colour_lookup[colreg];
	  *t_scrn_ptr++ = colour;

	  pf_pixel = *pf_pixel_ptr++;
	  pm_pixel = *pm_pixel_ptr++;
	  colreg = colreg_lookup[(pf_pixel << 8) | pm_pixel];
	  colour = colour_lookup[colreg];
	  *t_scrn_ptr++ = colour;

	  pf_pixel = *pf_pixel_ptr++;
	  pm_pixel = *pm_pixel_ptr++;
	  colreg = colreg_lookup[(pf_pixel << 8) | pm_pixel];
	  colour = colour_lookup[colreg];
	  *t_scrn_ptr++ = colour;
	}
    }

  scrn_ptr = t_scrn_ptr;
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

static UWORD chbase_40; /* CHBASE for 40 character mode */
static UWORD chbase_20; /* CHBASE for 20 character mode */

static int dmactl_xmin_noscroll;
static int dmactl_xmax_noscroll;
static int dmactl_xmin_scroll;
static int dmactl_xmax_scroll;

static int char_delta;
static int char_offset;
static int invert_mask;
static int blank_mask;

#ifndef BASIC
#ifndef CURSES

static UWORD	screenaddr;

static UWORD lookup1[256];
static UWORD lookup2[256];

static int xmin;
static int xmax;

void antic_blank (int nlines)
{
  if (dlicount)
    dlicount = nlines;

  memset (pf_scanline, 0, ATARI_WIDTH);

  while (nlines > 0)
  {
    if (DMACTL & 0x0c)
      PM_DMA ();

    Atari_ScanLine ();

    ypos++;
    nlines--;
  }
}

/*
#define TEST
*/

void antic_2 ()
{
  UWORD	chadr[48];
  UBYTE	invert[48];
  UBYTE	blank[48];
  int	i;

  int	nchars;

#ifdef TEST
  UBYTE *t_scrn_ptr;

  t_scrn_ptr = scrn_ptr;
#endif

  if (dlicount)
    dlicount = 8;

  lookup1[0x00] = 0x0004;
  lookup1[0x80] = lookup1[0x40] = lookup1[0x20] =
    lookup1[0x10] = lookup1[0x08] = lookup1[0x04] =
      lookup1[0x02] = lookup1[0x01] = 0x0002;

  nchars = (xmax - xmin) >> 3;

/*
   ==============================================
   Extract required characters from screen memory
   and locate start position in character set.
   ==============================================
*/
  for (i=0;i<nchars;i++)
    {
      UBYTE	screendata = memory[screenaddr++];

      chadr[i] = chbase_40 + char_offset + ((UWORD)(screendata & 0x7f) << 3);

      if (screendata & invert_mask)
	invert[i] = 0xff;
      else
	invert[i] = 0x00;

      if (screendata & blank_mask)
	blank[i] = 0x00;
      else
	blank[i] = 0xff;
    }

#ifndef TEST
  for (i=0;i<xmin;i++)
    pf_scanline[i] = 0;
  for (i=xmax;i<ATARI_WIDTH;i++)
    pf_scanline[i++] = 0;
#endif

  for (i=0;i<8;i++)
    {
      int	j;
#ifndef TEST
      int	xpos = xmin;
#endif

#ifdef TEST
      for (j=0;j<xmin;j++)
	*t_scrn_ptr++ = 0;
#endif

      for (j=0;j<nchars;j++)
	{
	  UBYTE	chdata;
	  UWORD	addr;

#ifdef TEST
	  int pf_pixel;
	  int colreg;
	  int colour;
#endif

	  addr = chadr[j];
	  chdata = memory[addr];
	  chadr[j] = addr + char_delta;

	  chdata = (chdata ^ invert[j]) & blank[j];

#ifdef TEST
	  pf_pixel = lookup1[chdata & 0x80];
	  colreg = colreg_lookup[pf_pixel << 8];
	  colour = colour_lookup[colreg];
	  *t_scrn_ptr++ = colour;

	  pf_pixel = lookup1[chdata & 0x40];
	  colreg = colreg_lookup[pf_pixel << 8];
	  colour = colour_lookup[colreg];
	  *t_scrn_ptr++ = colour;

	  pf_pixel = lookup1[chdata & 0x20];
	  colreg = colreg_lookup[pf_pixel << 8];
	  colour = colour_lookup[colreg];
	  *t_scrn_ptr++ = colour;

	  pf_pixel = lookup1[chdata & 0x10];
	  colreg = colreg_lookup[pf_pixel << 8];
	  colour = colour_lookup[colreg];
	  *t_scrn_ptr++ = colour;

	  pf_pixel = lookup1[chdata & 0x08];
	  colreg = colreg_lookup[pf_pixel << 8];
	  colour = colour_lookup[colreg];
	  *t_scrn_ptr++ = colour;

	  pf_pixel = lookup1[chdata & 0x04];
	  colreg = colreg_lookup[pf_pixel << 8];
	  colour = colour_lookup[colreg];
	  *t_scrn_ptr++ = colour;

	  pf_pixel = lookup1[chdata & 0x02];
	  colreg = colreg_lookup[pf_pixel << 8];
	  colour = colour_lookup[colreg];
	  *t_scrn_ptr++ = colour;

	  pf_pixel = lookup1[chdata & 0x01];
	  colreg = colreg_lookup[pf_pixel << 8];
	  colour = colour_lookup[colreg];
	  *t_scrn_ptr++ = colour;
#else
	  if (chdata)
	    {
	      char *ptr = &pf_scanline[xpos];

	      *ptr++ = lookup1[chdata & 0x80];
	      *ptr++ = lookup1[chdata & 0x40];
	      *ptr++ = lookup1[chdata & 0x20];
	      *ptr++ = lookup1[chdata & 0x10];
	      *ptr++ = lookup1[chdata & 0x08];
	      *ptr++ = lookup1[chdata & 0x04];
	      *ptr++ = lookup1[chdata & 0x02];
	      *ptr++ = lookup1[chdata & 0x01];
	    }
	  else
	    {
	      char *ptr = &pf_scanline[xpos];

	      *ptr++ = 0x04;
	      *ptr++ = 0x04;
	      *ptr++ = 0x04;
	      *ptr++ = 0x04;
	      *ptr++ = 0x04;
	      *ptr++ = 0x04;
	      *ptr++ = 0x04;
	      *ptr++ = 0x04;
	    }
#endif

#ifndef TEST
	  xpos += 8;
#endif
	}

#ifdef TEST
      for (j=xmax;j<ATARI_WIDTH;j++)
	*t_scrn_ptr++ = 0;

      dlicount--;
      if (dlicount == 0)
	{
	  NMIST |= 0x80;
	  INTERRUPT |= NMI_MASK;
	}

      VCOUNT = ypos>>1;
      ncycles = 48;
      GO ();
#else
      if (DMACTL & 0x0c)
	PM_DMA ();

      Atari_ScanLine ();
#endif

      ypos++;
    }

#ifdef TEST
  scrn_ptr = t_scrn_ptr;
#endif
}

void antic_3 ()
{
  UWORD	chadr[48];
  UBYTE	invert[48];
  UBYTE	blank[48];
  UBYTE	lowercase[48];
  UBYTE	first[48];
  UBYTE	second[48];
  int	i;

  int	nchars;

  if (dlicount)
    dlicount = 10;

  lookup1[0x00] = 0x0004;
  lookup1[0x80] = lookup1[0x40] = lookup1[0x20] =
    lookup1[0x10] = lookup1[0x08] = lookup1[0x04] =
      lookup1[0x02] = lookup1[0x01] = 0x0002;

  nchars = (xmax - xmin) >> 3;
/*
   ==============================================
   Extract required characters from screen memory
   and locate start position in character set.
   ==============================================
*/
  for (i=0;i<nchars;i++)
    {
      UBYTE	screendata;

      screendata = memory[screenaddr++];

      chadr[i] = chbase_40 + ((UWORD)(screendata & 0x7f) << 3) + char_offset;
      invert[i] = screendata & invert_mask;
/*
      if (screendata & 0x80)
	blank[i] = screendata & blank_mask;
      else
	blank[i] = 0x80;
*/
      if (screendata & blank_mask)
	blank[i] = 0x00;
      else
	blank[i] = 0xff;

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

      for (j=0;j<xpos;j++) pf_scanline[j] = 0;

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

	  chadr[j] += char_delta;
	  t_invert = invert[j];
	  t_blank = blank[j];
	  
	  for (k=0;k<8;k++)
	    {
	      if (((chdata & 0x80) ^ t_invert) & t_blank)
		{
		  pf_scanline[xpos++] = 0x02;
		}
	      else
		{
		  pf_scanline[xpos++] = 0x04;
		}
	      
	      chdata = chdata << 1;
	    }
	}

      while (xpos < ATARI_WIDTH) pf_scanline[xpos++] = 0;

      if (DMACTL & 0x0c)
	PM_DMA ();
      
      Atari_ScanLine ();

      ypos++;
    }
}

void antic_4 ()
{
  UBYTE	screendata[48];
  UWORD	chadr[48];
  int	i;

  int	nchars;

  if (dlicount)
    dlicount = 8;
/*
   =================================
   Pixel values when character < 128
   =================================
*/
  lookup1[0x00] = 0x0000;
  lookup1[0x40] = lookup1[0x10] = lookup1[0x04] =
    lookup1[0x01] = 0x0001;
  lookup1[0x80] = lookup1[0x20] = lookup1[0x08] =
    lookup1[0x02] = 0x0002;
  lookup1[0xc0] = lookup1[0x30] = lookup1[0x0c] =
    lookup1[0x03] = 0x0008;
/*
   ==================================
   Pixel values when character >= 128
   ==================================
*/
  lookup2[0x00] = 0x0000;
  lookup2[0x40] = lookup2[0x10] = lookup2[0x04] =
    lookup2[0x01] = 0x0001;
  lookup2[0x80] = lookup2[0x20] = lookup2[0x08] =
    lookup2[0x02] = 0x0004;
  lookup2[0xc0] = lookup2[0x30] = lookup2[0x0c] =
    lookup2[0x03] = 0x0008;

  nchars = (xmax - xmin) >> 3;
/*
   ==============================================
   Extract required characters from screen memory
   and locate start position in character set.
   ==============================================
*/
  for (i=0;i<nchars;i++)
    {
      screendata[i] = memory[screenaddr++];
      chadr[i] = chbase_40 + ((UWORD)(screendata[i] & 0x7f) << 3) + char_offset;
    }

  for (i=0;i<8;i++)
    {
      int	j;
      int	xpos = xmin;

      for (j=0;j<xpos;j++) pf_scanline[j] = 0;

      for (j=0;j<nchars;j++)
	{
	  UWORD *lookup;
	  UBYTE	chdata;

	  chdata = memory[chadr[j]];
	  chadr[j] += char_delta;
	  if (screendata[j] & 0x80)
	    lookup = lookup2;
	  else
	    lookup = lookup1;

	  if (chdata)
	    {
	      UWORD colour;

	      colour = lookup[chdata & 0xc0];
	      pf_scanline[xpos++] = colour;
	      pf_scanline[xpos++] = colour;

	      colour = lookup[chdata & 0x30];
	      pf_scanline[xpos++] = colour;
	      pf_scanline[xpos++] = colour;

	      colour = lookup[chdata & 0x0c];
	      pf_scanline[xpos++] = colour;
	      pf_scanline[xpos++] = colour;

	      colour = lookup[chdata & 0x03];
	      pf_scanline[xpos++] = colour;
	      pf_scanline[xpos++] = colour;
	    }
	  else
	    {
	      char *ptr = &pf_scanline[xpos];

	      *ptr++ = 0x00;
	      *ptr++ = 0x00;
	      *ptr++ = 0x00;
	      *ptr++ = 0x00;
	      *ptr++ = 0x00;
	      *ptr++ = 0x00;
	      *ptr++ = 0x00;
	      *ptr++ = 0x00;

	      xpos += 8;
	    }
	}

      while (xpos < ATARI_WIDTH) pf_scanline[xpos++] = 0;

      if (DMACTL & 0x0c)
	PM_DMA ();

      Atari_ScanLine ();

      ypos++;
    }
}

void antic_5 ()
{
  UBYTE	screendata[48];
  UWORD	chadr[48];
  int	i;

  int	nchars;

  if (dlicount)
    dlicount = 16;

  nchars = (xmax - xmin) >> 3;
/*
   ==============================================
   Extract required characters from screen memory
   and locate start position in character set.
   ==============================================
*/
  for (i=0;i<nchars;i++)
    {
      screendata[i] = memory[screenaddr++];
      chadr[i] = chbase_40 + ((UWORD)(screendata[i] & 0x7f) << 3) + char_offset;
    }

  for (i=0;i<8;i++)
    {
      int	j;
      int	xpos = xmin;

      for (j=0;j<xpos;j++) pf_scanline[j] = 0;

      for (j=0;j<nchars;j++)
	{
	  UBYTE	chdata;
	  UBYTE	t_screendata;
	  int	k;

	  chdata = memory[chadr[j]];
	  chadr[j] += char_delta;
	  t_screendata = screendata[j];

	  if (chdata)
	    {
	      for (k=0;k<4;k++)
		{
		  switch (chdata & 0xc0)
		    {
		    case 0x00 :
		      pf_scanline[xpos++] = 0x00;
		      pf_scanline[xpos++] = 0x00;
		      break;
		    case 0x40 :
		      pf_scanline[xpos++] = 0x01;
		      pf_scanline[xpos++] = 0x01;
		      break;
		    case 0x80 :
		      if (t_screendata & 0x80)
			{
			  pf_scanline[xpos++] = 0x04;
			  pf_scanline[xpos++] = 0x04;
			}
		      else
			{
			  pf_scanline[xpos++] = 0x02;
			  pf_scanline[xpos++] = 0x02;
			}
		      break;
		    case 0xc0 :
		      pf_scanline[xpos++] = 0x08;
		      pf_scanline[xpos++] = 0x08;
		      break;
		    }
		  
		  chdata = chdata << 2;
		}
	    }
	  else
	    {
	      char *ptr = &pf_scanline[xpos];

	      *ptr++ = 0x00;
	      *ptr++ = 0x00;
	      *ptr++ = 0x00;
	      *ptr++ = 0x00;
	      *ptr++ = 0x00;
	      *ptr++ = 0x00;
	      *ptr++ = 0x00;
	      *ptr++ = 0x00;

	      xpos += 8;
	    }
	}
      
      while (xpos < ATARI_WIDTH) pf_scanline[xpos++] = 0;

      if (DMACTL & 0x0c)
	{
	  PM_DMA ();
	  Atari_ScanLine ();
	  ypos++;
	  PM_DMA ();
	  Atari_ScanLine ();
	  ypos++;
	}
      else
	{
	  Atari_ScanLine ();
	  ypos++;
	  Atari_ScanLine ();
	  ypos++;
	}
    }
}

void antic_6 ()
{
  UBYTE	screendata[24];
  UWORD	chadr[24];
  int	i;

  int	nchars;

  if (dlicount)
    dlicount = 8;

  nchars = (xmax - xmin) >> 4;	/* Divide by 16 */
/*
   ==============================================
   Extract required characters from screen memory
   and locate start position in character set.
   ==============================================
*/
  for (i=0;i<nchars;i++)
    {
      screendata[i] = memory[screenaddr++];
      chadr[i] = chbase_20 + ((UWORD)(screendata[i] & 0x3f) << 3) + char_offset;
    }

  for (i=0;i<8;i++)
    {
      int	j;
      int	xpos = xmin;

      for (j=0;j<xpos;j++) pf_scanline[j] = 0;

      for (j=0;j<nchars;j++)
	{
	  UBYTE	chdata;
	  UBYTE	t_screendata;
	  int	k;

	  chdata = memory[chadr[j]];
	  chadr[j] += char_delta;
	  t_screendata = screendata[j];

	  for (k=0;k<8;k++)
	    {
	      if (chdata & 0x80)
		{
		  switch (t_screendata & 0xc0)
		    {
		    case 0x00 :
		      pf_scanline[xpos++] = 0x01;
		      pf_scanline[xpos++] = 0x01;
		      break;
		    case 0x40 :
		      pf_scanline[xpos++] = 0x02;
		      pf_scanline[xpos++] = 0x02;
		      break;
		    case 0x80 :
		      pf_scanline[xpos++] = 0x04;
		      pf_scanline[xpos++] = 0x04;
		      break;
		    case 0xc0 :
		      pf_scanline[xpos++] = 0x08;
		      pf_scanline[xpos++] = 0x08;
		      break;
		    }
		}
	      else
		{
		  pf_scanline[xpos++] = 0x00;
		  pf_scanline[xpos++] = 0x00;
		}
	      
	      chdata = chdata << 1;
	    }
	}

      while (xpos < ATARI_WIDTH) pf_scanline[xpos++] = 0;

      if (DMACTL & 0x0c)
	PM_DMA ();

      Atari_ScanLine ();

      ypos++;
    }
}

void antic_7 ()
{
  UBYTE	screendata[24];
  UWORD	chadr[24];
  int	i;

  int	nchars;

  if (dlicount)
    dlicount = 16;

  nchars = (xmax - xmin) >> 4;	/* Divide by 16 */
/*
   ==============================================
   Extract required characters from screen memory
   and locate start position in character set.
   ==============================================
*/
  for (i=0;i<nchars;i++)
    {
      screendata[i] = memory[screenaddr++];
      chadr[i] = chbase_20 + ((UWORD)(screendata[i] & 0x3f) << 3) + char_offset;
    }

  for (i=0;i<8;i++)
    {
      int	j;
      int	xpos = xmin;

      for (j=0;j<xpos;j++) pf_scanline[j] = 0;

      for (j=0;j<nchars;j++)
	{
	  UBYTE	chdata;
	  UBYTE	t_screendata;
	  int	k;

	  chdata = memory[chadr[j]];
	  chadr[j] += char_delta;
	  t_screendata = screendata[j];
	  
	  for (k=0;k<8;k++)
	    {
	      if (chdata & 0x80)
		{
		  switch (t_screendata & 0xc0)
		    {
		    case 0x00 :
		      pf_scanline[xpos++] = 0x01;
		      pf_scanline[xpos++] = 0x01;
		      break;
		    case 0x40 :
		      pf_scanline[xpos++] = 0x02;
		      pf_scanline[xpos++] = 0x02;
		      break;
		    case 0x80 :
		      pf_scanline[xpos++] = 0x04;
		      pf_scanline[xpos++] = 0x04;
		      break;
		    case 0xc0 :
		      pf_scanline[xpos++] = 0x08;
		      pf_scanline[xpos++] = 0x08;
		      break;
		    }
		}
	      else
		{
		  pf_scanline[xpos++] = 0x00;
		  pf_scanline[xpos++] = 0x00;
		}
	      
	      chdata = chdata << 1;
	    }
	}

      while (xpos < ATARI_WIDTH) pf_scanline[xpos++] = 0;

      if (DMACTL & 0x0c)
	{
	  PM_DMA ();
	  Atari_ScanLine ();
	  ypos++;
	  PM_DMA ();
	  Atari_ScanLine ();
	  ypos++;
	}
      else
	{
	  Atari_ScanLine ();
	  ypos++;
	  Atari_ScanLine ();
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

  if (dlicount)
    dlicount = 8;

  nbytes = (xmax - xmin) >> 5;	/* Divide by 32 */

  xpos = xmin;

  for (i=0;i<xpos;i++) pf_scanline[i] = 0;

  for (i=0;i<nbytes;i++)
    {
      UBYTE	screendata;

      screendata = memory[screenaddr++];

      for (j=0;j<4;j++)
	{
	  switch (screendata & 0xc0)
	    {
	    case 0x00 :
	      pf_scanline[xpos++] = 0x00;
	      pf_scanline[xpos++] = 0x00;
	      pf_scanline[xpos++] = 0x00;
	      pf_scanline[xpos++] = 0x00;
	      pf_scanline[xpos++] = 0x00;
	      pf_scanline[xpos++] = 0x00;
	      pf_scanline[xpos++] = 0x00;
	      pf_scanline[xpos++] = 0x00;
	      break;
	    case 0x40 :
	      pf_scanline[xpos++] = 0x01;
	      pf_scanline[xpos++] = 0x01;
	      pf_scanline[xpos++] = 0x01;
	      pf_scanline[xpos++] = 0x01;
	      pf_scanline[xpos++] = 0x01;
	      pf_scanline[xpos++] = 0x01;
	      pf_scanline[xpos++] = 0x01;
	      pf_scanline[xpos++] = 0x01;
	      break;
	    case 0x80 :
	      pf_scanline[xpos++] = 0x02;
	      pf_scanline[xpos++] = 0x02;
	      pf_scanline[xpos++] = 0x02;
	      pf_scanline[xpos++] = 0x02;
	      pf_scanline[xpos++] = 0x02;
	      pf_scanline[xpos++] = 0x02;
	      pf_scanline[xpos++] = 0x02;
	      pf_scanline[xpos++] = 0x02;
	      break;
	    case 0xc0 :
	      pf_scanline[xpos++] = 0x04;
	      pf_scanline[xpos++] = 0x04;
	      pf_scanline[xpos++] = 0x04;
	      pf_scanline[xpos++] = 0x04;
	      pf_scanline[xpos++] = 0x04;
	      pf_scanline[xpos++] = 0x04;
	      pf_scanline[xpos++] = 0x04;
	      pf_scanline[xpos++] = 0x04;
	      break;
	    }

	  screendata = screendata << 2;
	}
    }

  while (xpos < ATARI_WIDTH) pf_scanline[xpos++] = 0;

  if (DMACTL & 0x0c)
    {
      for (i=0;i<8;i++)
	{
	  PM_DMA ();
	  Atari_ScanLine ();
	  ypos++;
	}
    }
  else
    {
      for (i=0;i<8;i++)
	{
	  Atari_ScanLine ();
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

  if (dlicount)
    dlicount = 4;

  nbytes = (xmax - xmin) >> 5;	/* Divide by 32 */

  xpos = xmin;

  for (i=0;i<xpos;i++) pf_scanline[i] = 0;

  for (i=0;i<nbytes;i++)
    {
      UBYTE	screendata;

      screendata = memory[screenaddr++];

      for (j=0;j<8;j++)
	{
	  switch (screendata & 0x80)
	    {
	    case 0x00 :
	      pf_scanline[xpos++] = 0x00;
	      pf_scanline[xpos++] = 0x00;
	      pf_scanline[xpos++] = 0x00;
	      pf_scanline[xpos++] = 0x00;
	      break;
	    case 0x80 :
	      pf_scanline[xpos++] = 0x01;
	      pf_scanline[xpos++] = 0x01;
	      pf_scanline[xpos++] = 0x01;
	      pf_scanline[xpos++] = 0x01;
	      break;
	    }

	  screendata = screendata << 1;
	}
    }

  while (xpos < ATARI_WIDTH) pf_scanline[xpos++] = 0;

  if (DMACTL & 0x0c)
    {
      for (i=0;i<4;i++)
	{
	  PM_DMA ();
	  Atari_ScanLine ();
	  ypos++;
	}
    }
  else
    {
      for (i=0;i<4;i++)
	{
	  Atari_ScanLine ();
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
  
  if (dlicount)
    dlicount = 4;

  nbytes = (xmax - xmin) >> 4;	/* Divide by 16 */

  xpos = xmin;
  
  for (i=0;i<xpos;i++) pf_scanline[i] = 0;

  for (i=0;i<nbytes;i++)
    {
      UBYTE	screendata;

      screendata = memory[screenaddr++];

      for (j=0;j<4;j++)
	{
	  switch (screendata & 0xc0)
	    {
	    case 0x00 :
	      pf_scanline[xpos++] = 0x00;
	      pf_scanline[xpos++] = 0x00;
	      pf_scanline[xpos++] = 0x00;
	      pf_scanline[xpos++] = 0x00;
	      break;
	    case 0x40 :
	      pf_scanline[xpos++] = 0x01;
	      pf_scanline[xpos++] = 0x01;
	      pf_scanline[xpos++] = 0x01;
	      pf_scanline[xpos++] = 0x01;
	      break;
	    case 0x80 :
	      pf_scanline[xpos++] = 0x02;
	      pf_scanline[xpos++] = 0x02;
	      pf_scanline[xpos++] = 0x02;
	      pf_scanline[xpos++] = 0x02;
	      break;
	    case 0xc0 :
	      pf_scanline[xpos++] = 0x04;
	      pf_scanline[xpos++] = 0x04;
	      pf_scanline[xpos++] = 0x04;
	      pf_scanline[xpos++] = 0x04;
	      break;
	    }

	  screendata = screendata << 2;
	}
    }

  while (xpos < ATARI_WIDTH) pf_scanline[xpos++] = 0;

  if (DMACTL & 0x0c)
    {
      for (i=0;i<4;i++)
	{
	  PM_DMA ();
	  Atari_ScanLine ();
	  ypos++;
	}
    }
  else
    {
      for (i=0;i<4;i++)
	{
	  Atari_ScanLine ();
	  ypos++;
	}
    }
}

void antic_b ()
{
  int	xpos;
  int	nbytes;
  int	i;

  if (dlicount)
    dlicount = 2;

  nbytes = (xmax - xmin) >> 4;	/* Divide by 16 */

  xpos = xmin;

  for (i=0;i<xpos;i++) pf_scanline[i] = 0;

  for (i=0;i<nbytes;i++)
    {
      UBYTE	screendata;
      int	j;

      screendata = memory[screenaddr++];

      for (j=0;j<8;j++)
	{
	  if (screendata & 0x80)
	    {
	      pf_scanline[xpos++] = 0x01;
	      pf_scanline[xpos++] = 0x01;
	    }
	  else
	    {
	      pf_scanline[xpos++] = 0x00;
	      pf_scanline[xpos++] = 0x00;
	    }

	  screendata = screendata << 1;
	}
    }

  while (xpos < ATARI_WIDTH) pf_scanline[xpos++] = 0;

  if (DMACTL & 0x0c)
    {
      PM_DMA ();
      Atari_ScanLine ();
      ypos++;
      PM_DMA ();
      Atari_ScanLine ();
      ypos++;
    }
  else
    {
      Atari_ScanLine ();
      ypos++;
      Atari_ScanLine ();
      ypos++;
    }
}

void antic_c ()
{
  int	xpos;
  int	nbytes;
  int	i;

  if (dlicount)
    dlicount = 1;

  nbytes = (xmax - xmin) >> 4;	/* Divide by 16 */

  xpos = xmin;
  
  for (i=0;i<xpos;i++) pf_scanline[i] = 0;

  for (i=0;i<nbytes;i++)
    {
      UBYTE	screendata;
      int	j;

      screendata = memory[screenaddr++];

      for (j=0;j<8;j++)
	{
	  if (screendata & 0x80)
	    {
	      pf_scanline[xpos++] = 0x01;
	      pf_scanline[xpos++] = 0x01;
	    }
	  else
	    {
	      pf_scanline[xpos++] = 0x00;
	      pf_scanline[xpos++] = 0x00;
	    }

	  screendata = screendata << 1;
	}
    }

  while (xpos < ATARI_WIDTH) pf_scanline[xpos++] = 0;

  if (DMACTL & 0x0c)
    PM_DMA ();

  Atari_ScanLine ();

  ypos++;
}

void antic_d ()
{
  unsigned short *ptr = (unsigned short*)pf_scanline;
  int	nbytes;
  int	i;

  if (dlicount)
    dlicount = 2;

  nbytes = (xmax - xmin) >> 3;

  for (i=0;i<xmin;i+=2)
    *ptr++ = 0x0000;

  for (i=0;i<nbytes;i++)
    {
      UBYTE	screendata;
      
      screendata = memory[screenaddr++];

      if (screendata)
	{
	  int j;

	  for (j=0;j<4;j++)
	    {
	      switch (screendata & 0xc0)
		{
		case 0x00 :
		  *ptr++ = 0x0000;
		  break;
		case 0x40 :
		  *ptr++ = 0x0101;
		  break;
		case 0x80 :
		  *ptr++ = 0x0202;
		  break;
		case 0xc0 :
		  *ptr++ = 0x0404;
		  break;
		}
	  
	      screendata = screendata << 2;
	    }
	}
      else
	{
	  *ptr++ = 0x0000;
	  *ptr++ = 0x0000;
	  *ptr++ = 0x0000;
	  *ptr++ = 0x0000;
	}
    }

  for (i=xmax;i<ATARI_WIDTH;i+=2)
    *ptr++ = 0x0000;

  if (DMACTL & 0x0c)
    {
      PM_DMA ();
      Atari_ScanLine ();
      ypos++;
      PM_DMA ();
      Atari_ScanLine ();
      ypos++;
    }
  else
    {
      Atari_ScanLine ();
      ypos++;
      Atari_ScanLine ();
      ypos++;
    }
}

void antic_e ()
{
  char *ptr = pf_scanline;
  int nbytes;
  int i;

#ifdef TEST
  UBYTE *t_scrn_ptr;

  t_scrn_ptr = scrn_ptr;
#endif

  if (dlicount)
    dlicount = 1;

#ifdef TEST
#define O_COLBK 8
#define O_COLPF3 7
#define O_COLPF2 6
#define O_COLPF1 5
#define O_COLPF0 4
  lookup1[0x00] = colour_lookup[O_COLBK];
  lookup1[0x40] = lookup1[0x10] = lookup1[0x04] =
    lookup1[0x01] = colour_lookup[O_COLPF0];
  lookup1[0x80] = lookup1[0x20] = lookup1[0x08] =
    lookup1[0x02] = colour_lookup[O_COLPF1];
  lookup1[0xc0] = lookup1[0x30] = lookup1[0x0c] =
    lookup1[0x03] = colour_lookup[O_COLPF2];
#else
  lookup1[0x00] = 0x0000;
  lookup1[0x40] = lookup1[0x10] = lookup1[0x04] =
    lookup1[0x01] = 0x0001;
  lookup1[0x80] = lookup1[0x20] = lookup1[0x08] =
    lookup1[0x02] = 0x0002;
  lookup1[0xc0] = lookup1[0x30] = lookup1[0x0c] =
    lookup1[0x03] = 0x0004;
#endif

  nbytes = (xmax - xmin) >> 3;

#ifndef TEST
  for (i=0;i<xmin;i++)
    *ptr++ = 0x00;
#endif

#ifdef TEST
  for (i=0;i<xmin;i++)
    *t_scrn_ptr++ = 0x00;
#endif

  for (i=0;i<nbytes;i++)
    {
      UBYTE screendata = memory[screenaddr++];

#ifdef TEST
      int colour;

      colour = lookup1[screendata & 0xc0];
      *t_scrn_ptr++ = colour;
      *t_scrn_ptr++ = colour;

      colour = lookup1[screendata & 0x30];
      *t_scrn_ptr++ = colour;
      *t_scrn_ptr++ = colour;

      colour = lookup1[screendata & 0x0c];
      *t_scrn_ptr++ = colour;
      *t_scrn_ptr++ = colour;

      colour = lookup1[screendata & 0x03];
      *t_scrn_ptr++ = colour;
      *t_scrn_ptr++ = colour;
#else
      if (screendata)
	{
	  UWORD colour;

	  colour = lookup1[screendata & 0xc0];
	  *ptr++ = colour;
	  *ptr++ = colour;

	  colour = lookup1[screendata & 0x30];
	  *ptr++ = colour;
	  *ptr++ = colour;

	  colour = lookup1[screendata & 0x0c];
	  *ptr++ = colour;
	  *ptr++ = colour;

	  colour = lookup1[screendata & 0x03];
	  *ptr++ = colour;
	  *ptr++ = colour;
	}
      else
	{
	  *ptr++ = 0x00;
	  *ptr++ = 0x00;
	  *ptr++ = 0x00;
	  *ptr++ = 0x00;
	  *ptr++ = 0x00;
	  *ptr++ = 0x00;
	  *ptr++ = 0x00;
	  *ptr++ = 0x00;
	}
#endif
    }

#ifdef TEST
  for (i=xmax;i<ATARI_WIDTH;i++)
    *t_scrn_ptr++ = 0x00;

  dlicount--;
  if (dlicount == 0)
    {
      NMIST |= 0x80;
      INTERRUPT |= NMI_MASK;
    }

  VCOUNT = ypos >> 1;
  ncycles = 48;
  GO ();

  scrn_ptr = t_scrn_ptr;
#else
  for (i=xmax;i<ATARI_WIDTH;i++)
    *ptr++ = 0x00;

  if (DMACTL & 0x0c)
    PM_DMA ();
  
  Atari_ScanLine ();
#endif

  ypos++;
}

void antic_f ()
{
  char *ptr = pf_scanline;
  int nbytes;
  int i;

  if (dlicount)
    dlicount = 1;

  lookup1[0x00] = 0x0004;
  lookup1[0x80] = lookup1[0x40] = lookup1[0x20] =
    lookup1[0x10] = lookup1[0x08] = lookup1[0x04] =
      lookup1[0x02] = lookup1[0x01] = 0x0002;

  nbytes = (xmax - xmin) >> 3;

  for (i=0;i<xmin;i++)
    *ptr++ = 0x00;

  for (i=0;i<nbytes;i++)
    {
      UBYTE	screendata = memory[screenaddr++];

      if (screendata)
	{
	  *ptr++ = lookup1[screendata & 0x80];
	  *ptr++ = lookup1[screendata & 0x40];
	  *ptr++ = lookup1[screendata & 0x20];
	  *ptr++ = lookup1[screendata & 0x10];
	  *ptr++ = lookup1[screendata & 0x08];
	  *ptr++ = lookup1[screendata & 0x04];
	  *ptr++ = lookup1[screendata & 0x02];
	  *ptr++ = lookup1[screendata & 0x01];
	}
      else
	{
	  *ptr++ = 0x04;
	  *ptr++ = 0x04;
	  *ptr++ = 0x04;
	  *ptr++ = 0x04;
	  *ptr++ = 0x04;
	  *ptr++ = 0x04;
	  *ptr++ = 0x04;
	  *ptr++ = 0x04;
	}
    }

  for (i=xmax;i<ATARI_WIDTH;i++)
    *ptr++ = 0x00;

  if (DMACTL & 0x0c)
    PM_DMA ();

  Atari_ScanLine ();

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
	*   Description							*
	*   -----------							*
	*								*
	*   Section that handles Antic Display List. Not required for	*
	*   BASIC version.						*
	*                                                               *
	*   VCOUNT is equal to 8 at the start of the first mode line,   *
	*   and is compensated for in the Atari_ScanLine macro.         *
	*								*
	*****************************************************************
*/

void Atari800_UpdateScreen ()
{
  UWORD	dlist;
  int	JVB;

  scrn_ptr = screen;
  PM_InitFrame ();

  ypos = 0;

  if (DMACTL & 0x20)
    {
      dlist = (DLISTH << 8) | DLISTL;
      JVB = FALSE;

      do
	{
	  UBYTE	IR;

	  IR = memory[dlist++];

	  dlicount = IR & NMIEN & 0x80;

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
		  dlist = (memory[dlist+1] << 8) | memory[dlist];
		  antic_blank (1);	/* Jump aparently uses 1 scan line */
		}
	      break;
	    default :
	      if (IR & 0x40)
		{
		  screenaddr = (memory[dlist+1] << 8) | memory[dlist];
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
		  xmin = dmactl_xmin_scroll;
		  xmax = dmactl_xmax_scroll;
		  scroll_offset = HSCROL + HSCROL;
		}
	      else
		{
		  xmin = dmactl_xmin_noscroll;
		  xmax = dmactl_xmax_noscroll;
		  scroll_offset = 0;
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
	} while (!JVB && (ypos < ATARI_HEIGHT));
    }

  dlicount = 0;
  antic_blank (ATARI_HEIGHT - ypos);

  Atari_DisplayScreen (screen);
}

#endif
#endif

static int	SHIFT = 0x00;

UBYTE Atari800_GetByte (UWORD addr)
{
  UBYTE	byte;
/*
	============================================================
	GTIA, POKEY, PIA and ANTIC do not fully decode their address
	------------------------------------------------------------
	PIA (At least) is fully decoded when emulating the XL/XE
	============================================================
*/
  switch (addr & 0x0f00)
    {
    case 0xd000:        /* GTIA */
      addr &= 0xff1f;
      break;
    case 0xd200:        /* POKEY */
      addr &= 0xff0f;
      break;
    case 0xd300:        /* PIA */
      if (machine == Atari)
	addr &= 0xff03;
      break;
    case 0xd400:        /* ANTIC */
      addr &= 0xff0f;
      break;
    default:
      break;
    }

  switch (addr)
    {
    case _CHBASE :
      byte = CHBASE;
      break;
    case _CHACTL :
      byte = CHACTL;
      break;
    case _CONSOL :
      byte = Atari_CONSOL ();
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
      byte = P0PL & 0xfe;
      break;
    case _P1PL :
      byte = P1PL & 0xfd;
      break;
    case _P2PL :
      byte = P2PL & 0xfb;
      break;
    case _P3PL :
      byte = P3PL & 0xf7;
      break;
    case _PACTL :
      byte = PACTL;
      break;
    case _PBCTL :
      byte = PBCTL;
      break;
    case _PENH :
    case _PENV :
      byte = 0x00;
      break;
    case _PORTA :
      byte = Atari_PORT (0);
      break;
    case _PORTB :
      switch (machine)
	{
	case Atari :
	  byte = Atari_PORT (1);
	  break;
	case AtariXL :
	case AtariXE :
	  byte = PORTB;
	  break;
	}
      break;
    case _POT0 :
      byte = Atari_POT (0);
      break;
    case _POT1 :
      byte = Atari_POT (1);
      break;
    case _POT2 :
      byte = Atari_POT (2);
      break;
    case _POT3 :
      byte = Atari_POT (3); 
      break;
    case _POT4 :
      byte = Atari_POT (4);
      break;
    case _POT5 :
      byte = Atari_POT (5);
      break;
    case _POT6 :
      byte = Atari_POT (6);
      break;
    case _POT7 :
      byte = Atari_POT (7);
      break;
    case _RANDOM :
      byte = rand();
      break;
    case _TRIG0 :
      byte = Atari_TRIG (0);
      break;
    case _TRIG1 :
      byte = Atari_TRIG (1);
      break;
    case _TRIG2 :
      byte = Atari_TRIG (2);
      break;
    case _TRIG3 :
      byte = Atari_TRIG (3);
      break;
    case _VCOUNT :
      byte = VCOUNT++ + 4; /* VCOUNT starts 8 lines before playfield */
      break;
    case _NMIEN :
      byte = NMIEN;
      break;
    case _NMIST :
      byte = NMIST;
      break;
    case _SERIN :
      byte = SIO_SERIN ();
      break;
    case _SKSTAT :
      byte = 0xff;
      if (SHIFT)
	byte &= 0xf5;
      else
	byte &= 0xfd;
      break;
    case _WSYNC :
      ncycles = 3;
      byte = 0;
      break;
    default :
#ifdef DEBUG
      fprintf (stderr, "read from %04x\n", addr);
#endif
      byte = 0;
      break;
    }

  return byte;
}

void Atari800_PutByte (UWORD addr, UBYTE byte)
{
/*
	============================================================
	GTIA, POKEY, PIA and ANTIC do not fully decode their address
	------------------------------------------------------------
	PIA (At least) is fully decoded when emulating the XL/XE
	============================================================
*/
  switch (addr & 0x0f00)
    {
    case 0xd000:        /* GTIA */
      addr &= 0xff1f;
      break;
    case 0xd200:        /* POKEY */
      addr &= 0xff0f;
      break;
    case 0xd300:        /* PIA */
      if (machine == Atari)
	addr &= 0xff03;
      break;
    case 0xd400:        /* ANTIC */
      addr &= 0xff0f;
      break;
    default:
      break;
    }

  switch (cart_type)
    {
    case OSS_SUPERCART :
      switch (addr & 0xff0f)
	{
	case 0xd500 :
	  if (cart_image)
	    memcpy (memory+0xa000, cart_image, 0x1000);
	  break;
	case 0xd504 :
	  if (cart_image)
	    memcpy (memory+0xa000, cart_image+0x1000, 0x1000);
	  break;
	case 0xd503 :
	case 0xd507 :
	  if (cart_image)
	    memcpy (memory+0xa000, cart_image+0x2000, 0x1000);
	  break;
	}
      break;
    case DB_SUPERCART :
      switch (addr & 0xff07)
	{
	case 0xd500 :
	  if (cart_image)
	    memcpy (memory+0x8000, cart_image, 0x2000);
	  break;
	case 0xd501 :
	  if (cart_image)
	    memcpy (memory+0x8000, cart_image+0x2000, 0x2000);
	  break;
	case 0xd506 :
	  if (cart_image)
	    memcpy (memory+0x8000, cart_image+0x4000, 0x2000);
	  break;
	}
      break;
    default :
      break;
    }

  switch (addr)
    {
    case _AUDC1 :
      Atari_AUDC (1, byte);
      break;
    case _AUDC2 :
      Atari_AUDC (2, byte);
      break;
    case _AUDC3 :
      Atari_AUDC (3, byte);
      break;
    case _AUDC4 :
      Atari_AUDC (4, byte);
      break;
    case _AUDCTL :
      Atari_AUDCTL (byte);
      break;
    case _AUDF1 :
      Atari_AUDF (1, byte);
      break;
    case _AUDF2 :
      Atari_AUDF (2, byte);
      break;
    case _AUDF3 :
      Atari_AUDF (3, byte);
      break;
    case _AUDF4 :
      Atari_AUDF (4, byte);
      break;
    case _CHBASE :
      CHBASE = byte;
      chbase_40 = (byte << 8) & 0xfc00;
      chbase_20 = (byte << 8) & 0xfe00;
      break;
    case _CHACTL :
      CHACTL = byte;
/*
   =================================================================
   Check for vertical reflect, video invert and character blank bits
   =================================================================
*/
	switch (CHACTL & 0x07)
	  {
	  case 0x00 :
	    char_offset = 0;
	    char_delta = 1;
	    invert_mask = 0x00;
	    blank_mask = 0x00;
	    break;
	  case 0x01 :
	    char_offset = 0;
	    char_delta = 1;
	    invert_mask = 0x00;
	    blank_mask = 0x80;
	    break;
	  case 0x02 :
	    char_offset = 0;
	    char_delta = 1;
	    invert_mask = 0x80;
	    blank_mask = 0x00;
	    break;
	  case 0x03 :
	    char_offset = 0;
	    char_delta = 1;
	    invert_mask = 0x80;
	    blank_mask = 0x80;
	    break;
	  case 0x04 :
	    char_offset = 7;
	    char_delta = -1;
	    invert_mask = 0x00;
	    blank_mask = 0x00;
	    break;
	  case 0x05 :
	    char_offset = 7;
	    char_delta = -1;
	    invert_mask = 0x00;
	    blank_mask = 0x80;
	    break;
	  case 0x06 :
	    char_offset = 7;
	    char_delta = -1;
	    invert_mask = 0x80;
	    blank_mask = 0x00;
	    break;
	  case 0x07 :
	    char_offset = 7;
	    char_delta = -1;
	    invert_mask = 0x80;
	    blank_mask = 0x80;
	    break;
	  }
	break;
    case _COLBK :
      COLBK = byte;
#ifndef BASIC
      colour_lookup[8] = byte;
#endif
      break;
    case _COLPF0 :
#ifndef BASIC
      colour_lookup[4] = byte;
#endif
      break;
    case _COLPF1 :
#ifndef BASIC
      colour_lookup[5] = byte;
#endif
      break;
    case _COLPF2 :
#ifndef BASIC
      colour_lookup[6] = byte;
#endif
      break;
    case _COLPF3 :
#ifndef BASIC
      colour_lookup[7] = byte;
#endif
      break;
    case _COLPM0 :
#ifndef BASIC
      colour_lookup[0] = byte;
#endif
      break;
    case _COLPM1 :
#ifndef BASIC
      colour_lookup[1] = byte;
#endif
      break;
    case _COLPM2 :
#ifndef BASIC
      colour_lookup[2] = byte;
#endif
      break;
    case _COLPM3 :
#ifndef BASIC
      colour_lookup[3] = byte;
#endif
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
	switch (DMACTL & 0x03)
	  {
	  case 0x00 :
	    dmactl_xmin_noscroll = dmactl_xmax_noscroll = 0;
	    dmactl_xmin_scroll = dmactl_xmax_scroll = 0;
	    break;
	  case 0x01 :
	    dmactl_xmin_noscroll = 64;
	    dmactl_xmax_noscroll = ATARI_WIDTH - 64;
	    dmactl_xmin_scroll = 32;
	    dmactl_xmax_scroll = ATARI_WIDTH - 32;
	    break;
	  case 0x02 :
	    dmactl_xmin_noscroll = 32;
	    dmactl_xmax_noscroll = ATARI_WIDTH - 32;
	    dmactl_xmin_scroll = 0;
	    dmactl_xmax_scroll = ATARI_WIDTH;
	    break;
	  case 0x03 :
	    dmactl_xmin_noscroll = dmactl_xmin_scroll = 0;
	    dmactl_xmax_noscroll = dmactl_xmax_scroll = ATARI_WIDTH;
	    break;
	  }
	break;
    case _GRAFM :
      GRAFM = byte;
      break;
    case _GRAFP0 :
      GRAFP0 = byte;
      break;
    case _GRAFP1 :
      GRAFP1 = byte;
      break;
    case _GRAFP2 :
      GRAFP2 = byte;
      break;
    case _GRAFP3 :
      GRAFP3 = byte;
      break;
    case _HITCLR :
      M0PF = M1PF = M2PF = M3PF = 0;
      P0PF = P1PF = P2PF = P3PF = 0; 
      M0PL = M1PL = M2PL = M3PL = 0; 
      P0PL = P1PL = P2PL = P3PL = 0;
      break;
    case _HPOSM0 :
      global_hposm0 = PM_XPos[byte];
      break;
    case _HPOSM1 :
      global_hposm1 = PM_XPos[byte];
      break;
    case _HPOSM2 :
      global_hposm2 = PM_XPos[byte];
      break;
    case _HPOSM3 :
      global_hposm3 = PM_XPos[byte];
      break;
    case _HPOSP0 :
      global_hposp0 = PM_XPos[byte];
      break;
    case _HPOSP1 :
      global_hposp1 = PM_XPos[byte];
      break;
    case _HPOSP2 :
      global_hposp2 = PM_XPos[byte];
      break;
    case _HPOSP3 :
      global_hposp3 = PM_XPos[byte];
      break;
    case _HSCROL :
      HSCROL = byte & 0x0f;
      break;
    case _IRQEN :
      IRQEN = byte;
      IRQST |= (~byte);
      if (IRQEN & 0x08)
	{
	  IRQST &= 0xf7;
	  INTERRUPT |= IRQ_MASK;
	}
      if (IRQEN &0x20)
	{
	  IRQST &= 0xdf;
	  INTERRUPT |= IRQ_MASK;
	}
      break;
    case _NMIEN :
      NMIEN = byte;
      break;
    case _NMIRES :
      NMIST = 0x00;
      break;
    case _PACTL :
      PACTL = byte;
      break;
    case _PBCTL :
      PBCTL = byte;
      break;
    case _PMBASE :
      PMBASE = byte;
      break;
    case _PORTB :
      switch (machine)
	{
	case Atari :
	  break;
	case AtariXL :
	case AtariXE :
#ifdef DEBUG
	  printf ("Storing %x to PORTB\n", byte);
#endif
	  if ((byte ^ PORTB) & 0x01)
	    {
	      if (byte & 0x01)
		{
#ifdef DEBUG
		  printf ("\tEnable ROM at $c000-$cfff and $d800-$ffff\n");
#endif
		  memcpy (under_atarixl_os, memory+0xc000, 0x1000);
		  memcpy (under_atarixl_os+0x1800, memory+0xd800, 0x2800);
		  memcpy (memory+0xc000, atarixl_os, 0x1000);
		  memcpy (memory+0xd800, atarixl_os+0x1800, 0x2800);
		  SetROM (0xc000, 0xcfff);
		  SetROM (0xd800, 0xffff);
		}
	      else
		{
#ifdef DEBUG
		  printf ("\tEnable RAM at $c000-$cfff and $d800-$ffff\n");
#endif
		  memcpy (memory+0xc000, under_atarixl_os, 0x1000);
		  memcpy (memory+0xd800, under_atarixl_os+0x1800, 0x2800);
		  SetRAM (0xc000, 0xcfff);
		  SetRAM (0xd800, 0xffff);
		}
	    }
/*
	=====================================
	An Atari XL/XE can only disable Basic
	Other cartridge cannot be disable
	=====================================
*/
	  if (!rom_inserted)
	    {
	      if ((byte ^ PORTB) & 0x02)
		{
		  if (byte & 0x02)
		    {
#ifdef DEBUG
		      printf ("\tDisable BASIC\n");
#endif
		      memcpy (memory+0xa000, under_atari_basic, 0x2000);
		      SetRAM (0xa000, 0xbfff);
		    }
		  else
		    {
#ifdef DEBUG
		      printf ("\tEnable BASIC at $a000-$bfff\n");
#endif
		      memcpy (under_atari_basic, memory+0xa000, 0x2000);
		      memcpy (memory+0xa000, atari_basic, 0x2000);
		      SetROM (0xa000, 0xbfff);
		    }
		}
	    }

	  if ((byte ^ PORTB) & 0x80)
	    {
	      if (byte & 0x80)
		{
#ifdef DEBUG
		  printf ("\tEnable RAM at $5000-$57ff (Self Test)\n");
#endif
		  memcpy (memory+0x5000, under_atarixl_os+0x1000, 0x0800);
		  SetRAM (0x5000, 0x57ff);
		}
	      else
		{
#ifdef DEBUG
		  printf ("\tEnable ROM at $5000-$57ff (Self Test)\n");
#endif
		  memcpy (under_atarixl_os+0x1000, memory+0x5000, 0x800);
		  memcpy (memory+0x5000, atarixl_os+0x1000, 0x800);
		  SetROM (0x5000, 0x57ff);
		}
	    }
	  PORTB = byte;
	  break;
	}
      break;
    case _POTGO :
      break;
#ifndef BASIC
    case _PRIOR :
      if (byte != PRIOR)
	{
	  SetPrior (byte);
	}
      break;
#endif
    case _SEROUT :
      {
	int cmd_flag = (PBCTL & 0x08) ? 0 : 1;

	SIO_SEROUT (byte, cmd_flag);
      }
      break;
    case _SIZEM :
      global_sizem = PM_Width[byte & 0x03];
      break;
    case _SIZEP0 :
      global_sizep0 = PM_Width[byte & 0x03];
      break;
    case _SIZEP1 :
      global_sizep1 = PM_Width[byte & 0x03];
      break;
    case _SIZEP2 :
      global_sizep2 = PM_Width[byte & 0x03];
      break;
    case _SIZEP3 :
      global_sizep3 = PM_Width[byte & 0x03];
      break;
    case _WSYNC :
      ncycles = 3;
      break;
    default :
#ifdef DEBUG
      fprintf (stderr, "write %02x to %04x\n", byte, addr);
#endif
      break;
    }
}

void Atari800_Hardware (void)
{
  static int	pil_on = FALSE;

  while (TRUE)
    {
      static int	test_val = 0;

      int	keycode;

      NMIST = 0x00;

#ifndef BASIC
      keycode = Atari_Keyboard ();
#endif

      switch (keycode)
	{
	case AKEY_WARMSTART :
	  NMIST |= 0x20;
	  INTERRUPT |= NMI_MASK;
	  keycode = AKEY_NONE;
	  break;
	case AKEY_COLDSTART :
	  PutByte (0x244, 1);
	  NMIST |= 0x20;
	  INTERRUPT |= NMI_MASK;
	  keycode = AKEY_NONE;
	  break;
	case AKEY_EXIT :
	  Atari800_Exit (FALSE);
	  exit (1);
	case AKEY_HELP :
	  keycode = 0x11;
	  break;
	case AKEY_BREAK :
	  IRQST &= 0x7f;
	  INTERRUPT |= IRQ_MASK;
	  keycode = AKEY_NONE;
	  break;
	case AKEY_PIL :
	  if (pil_on)
	    {
	      SetRAM (0x8000, 0xbfff);
	      pil_on = FALSE;
	    }
	  else
	    {
	      SetROM (0x8000, 0xbfff);
	      pil_on = TRUE;
	    }
	  keycode = AKEY_NONE;
	  break;
	case AKEY_DOWN :
	  keycode = 0x8f;
	  break;
	case AKEY_LEFT :
	  keycode = 0x86;
	  break;
	case AKEY_RIGHT :
	  keycode = 0x87;
	  break;
	case AKEY_UP :
	  keycode = 0x8e;
	  break;
	case AKEY_BACKSPACE :
	  keycode = 0x34;
	  break;
	case AKEY_DELETE_CHAR :
	  keycode = 0xb4;
	  break;
	case AKEY_DELETE_LINE :
	  keycode = 0x74;
	  break;
	case AKEY_INSERT_CHAR :
	  keycode = 0xb7;
	  break;
	case AKEY_INSERT_LINE :
	  keycode = 0x77;
	  break;
	case AKEY_ESCAPE :
	  keycode = 0x1c;
	  break;
	case AKEY_ATARI :
	  keycode = 0x27;
	  break;
	case AKEY_CAPSLOCK :
	  keycode = 0x7c;
	  break;
	case AKEY_CAPSTOGGLE :
	  keycode = 0x3c;
	  break;
	case AKEY_TAB :
	  keycode = 0x2c;
	  break;
	case AKEY_SETTAB :
	  keycode = 0x6c;
	  break;
	case AKEY_CLRTAB :
	  keycode = 0xac;
	  break;
	case AKEY_RETURN :
	  keycode = 0x0c;
	  break;
	case ' ' :
	  keycode = 0x21;
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
	case '[' :
	  keycode = 0x60;
	  break;
	case ']' :
	  keycode = 0x62;
	  break;
	case '^' :
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
	case 'a' :
	  keycode = 0x3f;
	  break;
	case 'b' :
	  keycode = 0x15;
	  break;
	case 'c' :
	  keycode = 0x12;
	  break;
	case 'd' :
	  keycode = 0x3a;
	  break;
	case 'e' :
	  keycode = 0x2a;
	  break;
	case 'f' :
	  keycode = 0x38;
	  break;
	case 'g' :
	  keycode = 0x3d;
	  break;
	case 'h' :
	  keycode = 0x39;
	  break;
	case 'i' :
	  keycode = 0x0d;
	  break;
	case 'j' :
	  keycode = 0x01;
	  break;
	case 'k' :
	  keycode = 0x05;
	  break;
	case 'l' :
	  keycode = 0x00;
	  break;
	case 'm' :
	  keycode = 0x25;
	  break;
	case 'n' :
	  keycode = 0x23;
	  break;
	case 'o' :
	  keycode = 0x08;
	  break;
	case 'p' :
	  keycode = 0x0a;
	  break;
	case 'q' :
	  keycode = 0x2f;
	  break;
	case 'r' :
	  keycode = 0x28;
	  break;
	case 's' :
	  keycode = 0x3e;
	  break;
	case 't' :
	  keycode = 0x2d;
	  break;
	case 'u' :
	  keycode = 0x0b;
	  break;
	case 'v' :
	  keycode = 0x10;
	  break;
	case 'w' :
	  keycode = 0x2e;
	  break;
	case 'x' :
	  keycode = 0x16;
	  break;
	case 'y' :
	  keycode = 0x2b;
	  break;
	case 'z' :
	  keycode = 0x17;
	  break;
	case 'A' :
	  keycode = 0x40 | 0x3f;
	  break;
	case 'B' :
	  keycode = 0x40 | 0x15;
	  break;
	case 'C' :
	  keycode = 0x40 | 0x12;
	  break;
	case 'D' :
	  keycode = 0x40 | 0x3a;
	  break;
	case 'E' :
	  keycode = 0x40 | 0x2a;
	  break;
	case 'F' :
	  keycode = 0x40 | 0x38;
	  break;
	case 'G' :
	  keycode = 0x40 | 0x3d;
	  break;
	case 'H' :
	  keycode = 0x40 | 0x39;
	  break;
	case 'I' :
	  keycode = 0x40 | 0x0d;
	  break;
	case 'J' :
	  keycode = 0x40 | 0x01;
	  break;
	case 'K' :
	  keycode = 0x40 | 0x05;
	  break;
	case 'L' :
	  keycode = 0x40 | 0x00;
	  break;
	case 'M' :
	  keycode = 0x40 | 0x25;
	  break;
	case 'N' :
	  keycode = 0x40 | 0x23;
	  break;
	case 'O' :
	  keycode = 0x40 | 0x08;
	  break;
	case 'P' :
	  keycode = 0x40 | 0x0a;
	  break;
	case 'Q' :
	  keycode = 0x40 | 0x2f;
	  break;
	case 'R' :
	  keycode = 0x40 | 0x28;
	  break;
	case 'S' :
	  keycode = 0x40 | 0x3e;
	  break;
	case 'T' :
	  keycode = 0x40 | 0x2d;
	  break;
	case 'U' :
	  keycode = 0x40 | 0x0b;
	  break;
	case 'V' :
	  keycode = 0x40 | 0x10;
	  break;
	case 'W' :
	  keycode = 0x40 | 0x2e;
	  break;
	case 'X' :
	  keycode = 0x40 | 0x16;
	  break;
	case 'Y' :
	  keycode = 0x40 | 0x2b;
	  break;
	case 'Z' :
	  keycode = 0x40 | 0x17;
	  break;
	case AKEY_CTRL_0 :
	  keycode = 0x80 | 0x32;
	  break;
	case AKEY_CTRL_1 :
	  keycode = 0x80 | 0x1f;
	  break;
	case AKEY_CTRL_2 :
	  keycode = 0x80 | 0x1e;
	  break;
	case AKEY_CTRL_3 :
	  keycode = 0x80 | 0x1a;
	  break;
	case AKEY_CTRL_4 :
	  keycode = 0x80 | 0x18;
	  break;
	case AKEY_CTRL_5 :
	  keycode = 0x80 | 0x1d;
	  break;
	case AKEY_CTRL_6 :
	  keycode = 0x80 | 0x1b;
	  break;
	case AKEY_CTRL_7 :
	  keycode = 0x80 | 0x33;
	  break;
	case AKEY_CTRL_8 :
	  keycode = 0x80 | 0x35;
	  break;
	case AKEY_CTRL_9 :
	  keycode = 0x80 | 0x30;
	  break;
	case AKEY_CTRL_A :
	  keycode = 0x80 | 0x3f;
	  break;
	case AKEY_CTRL_B :
	  keycode = 0x80 | 0x15;
	  break;
	case AKEY_CTRL_C :
	  keycode = 0x80 | 0x12;
	  break;
	case AKEY_CTRL_D :
	  keycode = 0x80 | 0x3a;
	  break;
	case AKEY_CTRL_E :
	  keycode = 0x80 | 0x2a;
	  break;
	case AKEY_CTRL_F :
	  keycode = 0x80 | 0x38;
	  break;
	case AKEY_CTRL_G :
	  keycode = 0x80 | 0x3d;
	  break;
	case AKEY_CTRL_H :
	  keycode = 0x80 | 0x39;
	  break;
	case AKEY_CTRL_I :
	  keycode = 0x80 | 0x0d;
	  break;
	case AKEY_CTRL_J :
	  keycode = 0x80 | 0x01;
	  break;
	case AKEY_CTRL_K :
	  keycode = 0x80 | 0x05;
	  break;
	case AKEY_CTRL_L :
	  keycode = 0x80 | 0x00;
	  break;
	case AKEY_CTRL_M :
	  keycode = 0x80 | 0x25;
	  break;
	case AKEY_CTRL_N :
	  keycode = 0x80 | 0x23;
	  break;
	case AKEY_CTRL_O :
	  keycode = 0x80 | 0x08;
	  break;
	case AKEY_CTRL_P :
	  keycode = 0x80 | 0x0a;
	  break;
	case AKEY_CTRL_Q :
	  keycode = 0x80 | 0x2f;
	  break;
	case AKEY_CTRL_R :
	  keycode = 0x80 | 0x28;
	  break;
	case AKEY_CTRL_S :
	  keycode = 0x80 | 0x3e;
	  break;
	case AKEY_CTRL_T :
	  keycode = 0x80 | 0x2d;
	  break;
	case AKEY_CTRL_U :
	  keycode = 0x80 | 0x0b;
	  break;
	case AKEY_CTRL_V :
	  keycode = 0x80 | 0x10;
	  break;
	case AKEY_CTRL_W :
	  keycode = 0x80 | 0x2e;
	  break;
	case AKEY_CTRL_X :
	  keycode = 0x80 | 0x16;
	  break;
	case AKEY_CTRL_Y :
	  keycode = 0x80 | 0x2b;
	  break;
	case AKEY_CTRL_Z :
	  keycode = 0x80 | 0x17;
	  break;
	case AKEY_SHFTCTRL_A :
	  keycode = 0xc0 | 0x3f;
	  break;
	case AKEY_SHFTCTRL_B :
	  keycode = 0xc0 | 0x15;
	  break;
	case AKEY_SHFTCTRL_C :
	  keycode = 0xc0 | 0x12;
	  break;
	case AKEY_SHFTCTRL_D :
	  keycode = 0xc0 | 0x3a;
	  break;
	case AKEY_SHFTCTRL_E :
	  keycode = 0xc0 | 0x2a;
	  break;
	case AKEY_SHFTCTRL_F :
	  keycode = 0xc0 | 0x38;
	  break;
	case AKEY_SHFTCTRL_G :
	  keycode = 0xc0 | 0x3d;
	  break;
	case AKEY_SHFTCTRL_H :
	  keycode = 0xc0 | 0x39;
	  break;
	case AKEY_SHFTCTRL_I :
	  keycode = 0xc0 | 0x0d;
	  break;
	case AKEY_SHFTCTRL_J :
	  keycode = 0xc0 | 0x01;
	  break;
	case AKEY_SHFTCTRL_K :
	  keycode = 0xc0 | 0x05;
	  break;
	case AKEY_SHFTCTRL_L :
	  keycode = 0xc0 | 0x00;
	  break;
	case AKEY_SHFTCTRL_M :
	  keycode = 0xc0 | 0x25;
	  break;
	case AKEY_SHFTCTRL_N :
	  keycode = 0xc0 | 0x23;
	  break;
	case AKEY_SHFTCTRL_O :
	  keycode = 0xc0 | 0x08;
	  break;
	case AKEY_SHFTCTRL_P :
	  keycode = 0xc0 | 0x0a;
	  break;
	case AKEY_SHFTCTRL_Q :
	  keycode = 0xc0 | 0x2f;
	  break;
	case AKEY_SHFTCTRL_R :
	  keycode = 0xc0 | 0x28;
	  break;
	case AKEY_SHFTCTRL_S :
	  keycode = 0xc0 | 0x3e;
	  break;
	case AKEY_SHFTCTRL_T :
	  keycode = 0xc0 | 0x2d;
	  break;
	case AKEY_SHFTCTRL_U :
	  keycode = 0xc0 | 0x0b;
	  break;
	case AKEY_SHFTCTRL_V :
	  keycode = 0xc0 | 0x10;
	  break;
	case AKEY_SHFTCTRL_W :
	  keycode = 0xc0 | 0x2e;
	  break;
	case AKEY_SHFTCTRL_X :
	  keycode = 0xc0 | 0x16;
	  break;
	case AKEY_SHFTCTRL_Y :
	  keycode = 0xc0 | 0x2b;
	  break;
	case AKEY_SHFTCTRL_Z :
	  keycode = 0xc0 | 0x17;
	default :
	  keycode = AKEY_NONE;
	  break;
	}

      if (keycode != AKEY_NONE)
	{
	  KBCODE = keycode;
	  IRQST &= 0xbf;
	  INTERRUPT |= IRQ_MASK;
	}

      if (NMIEN & 0x40)
	{
	  NMIST |= 0x40;
	  INTERRUPT |= NMI_MASK;
	}
/*
	=========================
	Execute Some Instructions
	=========================
*/
      ncycles = countdown_rate;
      GO ();
/*
	=================
	Regenerate Screen
	=================
*/

#ifndef BASIC
#ifdef CURSES
      {
	int i;

	Atari_DisplayScreen ();

	for (i=0;i<ATARI_HEIGHT;i++)
	  {
	    ncycles = 48;
	    GO ();
	  }
      }
#else
      if (++test_val == refresh_rate)
	{
	  Atari800_UpdateScreen ();
	  test_val = 0;
	}
      else
	{
	  int i;

	  for (i=0;i<ATARI_HEIGHT;i++)
	    {
	      ncycles = 48;
	      GO ();
	    }
	}
#endif
#endif

    }
}

