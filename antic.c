#include <stdio.h>
#include <string.h>

#ifndef AMIGA
#include "config.h"
#endif

#include "atari.h"
#include "rt-config.h"
#include "cpu.h"
#include "gtia.h"
#include "antic.h"

#define FALSE 0
#define TRUE 1

static char *rcsid = "$Id: antic.c,v 1.18 1997/03/22 21:48:27 david Exp $";

UBYTE CHACTL;
UBYTE CHBASE;
UBYTE DLISTH;
UBYTE DLISTL;
UBYTE DMACTL;
UBYTE HSCROL;
UBYTE NMIEN;
UBYTE NMIST;
UBYTE PMBASE;
UBYTE VSCROL;

/*
 * These are defined for Word (2 Byte) memory accesses. I have not
 * defined Longword (4 Byte) values since the Sparc architecture
 * requires that Longword are on a longword boundry.
 *
 * Words accesses don't appear to be a problem because the first
 * pixel plotted on a line will always be at an even offset, and
 * hence on a word boundry.
 *
 * Note: HSCROL is in colour clocks whereas the pixels are emulated
 *       down to a half colour clock - the first pixel plotted is
 *       moved across by 2*HSCROL
 */

#define PF2_COLPF0 0x0404
#define PF2_COLPF1 0x0505
#define PF2_COLPF2 0x0606
#define PF2_COLPF3 0x0707
#define PF2_COLBK 0x0808

#define PF4_COLPF0 0x04040404
#define PF4_COLPF1 0x05050505
#define PF4_COLPF2 0x06060606
#define PF4_COLPF3 0x07070707
#define PF4_COLBK 0x08080808

int ypos;

/*
 * Pre-computed values for improved performance
 */

static UWORD chbase_40; /* CHBASE for 40 character mode */
static UWORD chbase_20; /* CHBASE for 20 character mode */
static int scroll_offset;
static UBYTE singleline;
static UBYTE player_dma_enabled;
static UBYTE missile_dma_enabled;
static UWORD maddr_s; /* Address of Missiles - Single Line Resolution */
static UWORD p0addr_s; /* Address of Player0 - Single Line Resolution */
static UWORD p1addr_s; /* Address of Player1 - Single Line Resolution */
static UWORD p2addr_s; /* Address of Player2 - Single Line Resolution */
static UWORD p3addr_s; /* Address of Player3 - Single Line Resolution */
static UWORD maddr_d; /* Address of Missiles - Double Line Resolution */
static UWORD p0addr_d; /* Address of Player0 - Double Line Resolution */
static UWORD p1addr_d; /* Address of Player1 - Double Line Resolution */
static UWORD p2addr_d; /* Address of Player2 - Double Line Resolution */
static UWORD p3addr_d; /* Address of Player3 - Double Line Resolution */

int wsync_halt = 0;

/*
   =============================================================
   Define screen as ULONG to ensure that it is Longword aligned.
   This allows special optimisations under certain conditions.
   -------------------------------------------------------------
   The extra 16 scanlines is used as an overflow buffer and has
   enough room for any extra mode line. It is needed on the
   occasions that a valid JVB instruction is not found in the
   display list - An automatic break out will occur when ypos
   is greater than the ATARI_HEIGHT, if its one less than this
   there must be enough space for another mode line.
   =============================================================
*/

ULONG *atari_screen = NULL;
UBYTE *scrn_ptr;

void ANTIC_Initialise (int *argc, char *argv[])
{
  int i;
  int j;

  for (i=j=1;i<*argc;i++)
    {
      if (strcmp(argv[i],"-xcolpf1") == 0)
        enable_xcolpf1 = TRUE;
      else
        argv[j++] = argv[i];
    }

  *argc = j;
}

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

int xmin;
int xmax;

int dmactl_xmin_noscroll;
int dmactl_xmax_noscroll;
static int dmactl_xmin_scroll;
static int dmactl_xmax_scroll;
static int char_delta;
static int char_offset;
static int invert_mask;
static int blank_mask;

static UWORD	screenaddr;

static UWORD lookup1[256];
static UWORD lookup2[256];

void antic_blank (int nlines)
{
  if (nlines > 0)
    {
      int nbytes;

      nbytes = nlines * ATARI_WIDTH;

#ifdef DIRECT_VIDEO
      memset (scrn_ptr, colour_lookup[8], nbytes);
#else
      memset (scrn_ptr, PF_COLBK, nbytes);
#endif
    }
}

static int vskipbefore = 0;
static int vskipafter = 0;

void antic_2 ()
{
  UWORD t_screenaddr = screenaddr;
  char *t_scrn_ptr = &scrn_ptr[xmin+scroll_offset];
  int nchars = (xmax - xmin) >> 3;
  int i;

#ifdef DIRECT_VIDEO
  lookup1[0x00] = colour_lookup[6];
  lookup1[0x80] = lookup1[0x40] = lookup1[0x20] =
    lookup1[0x10] = lookup1[0x08] = lookup1[0x04] =
      lookup1[0x02] = lookup1[0x01] = colour_lookup[5];
#else
  lookup1[0x00] = PF_COLPF2;
  lookup1[0x80] = lookup1[0x40] = lookup1[0x20] =
    lookup1[0x10] = lookup1[0x08] = lookup1[0x04] =
      lookup1[0x02] = lookup1[0x01] = PF_COLPF1;
#endif

  for (i=0;i<nchars;i++)
    {
      UBYTE screendata = memory[t_screenaddr++];
      UWORD chaddr;
      UBYTE invert;
      UBYTE blank;
      char *ptr = t_scrn_ptr;
      int j;

      chaddr = chbase_40 + char_offset + ((UWORD)(screendata & 0x7f) << 3);

      if (screendata & invert_mask)
	invert = 0xff;
      else
	invert = 0x00;

      if (screendata & blank_mask)
	blank = 0x00;
      else
	blank = 0xff;

      for (j=0;j<8;j++)
	{
	  UBYTE	chdata;

	  chdata = (memory[chaddr] ^ invert) & blank;
	  chaddr += char_delta;

	  if ((j >= vskipbefore) && (j <= vskipafter))
	    {
	      if (chdata)
		{
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
#ifdef DIRECT_VIDEO
		  *ptr++ = lookup1[0];
		  *ptr++ = lookup1[0];
		  *ptr++ = lookup1[0];
		  *ptr++ = lookup1[0];
		  *ptr++ = lookup1[0];
		  *ptr++ = lookup1[0];
		  *ptr++ = lookup1[0];
		  *ptr++ = lookup1[0];
#else
#ifdef UNALIGNED_LONG_OK
		  ULONG *l_ptr = (ULONG*)ptr;

		  *l_ptr++ = PF4_COLPF2;
		  *l_ptr++ = PF4_COLPF2;

		  ptr = (UBYTE*)l_ptr;
#else
		  UWORD *w_ptr = (UWORD*)ptr;

		  *w_ptr++ = PF2_COLPF2;
		  *w_ptr++ = PF2_COLPF2;
		  *w_ptr++ = PF2_COLPF2;
		  *w_ptr++ = PF2_COLPF2;

		  ptr = (UBYTE*)w_ptr;
#endif
#endif
		}

	      ptr += (ATARI_WIDTH - 8);
	    }
	}

      t_scrn_ptr += 8;
    }

  screenaddr = t_screenaddr;
}

/*
 * Function to display Antic Mode 3
 */

void antic_3 ()
{
  char *t_scrn_ptr = &scrn_ptr[xmin+scroll_offset];
  int nchars = (xmax - xmin) >> 3;
  int i;

#ifdef DIRECT_VIDEO
  lookup1[0x00] = colour_lookup[6];
  lookup1[0x80] = lookup1[0x40] = lookup1[0x20] =
    lookup1[0x10] = lookup1[0x08] = lookup1[0x04] =
      lookup1[0x02] = lookup1[0x01] = colour_lookup[5];
#else
  lookup1[0x00] = PF_COLPF2;
  lookup1[0x80] = lookup1[0x40] = lookup1[0x20] =
    lookup1[0x10] = lookup1[0x08] = lookup1[0x04] =
      lookup1[0x02] = lookup1[0x01] = PF_COLPF1;
#endif

  for (i=0;i<nchars;i++)
    {
      UBYTE screendata = memory[screenaddr++];
      UWORD chaddr;
      UBYTE invert;
      UBYTE blank;
      UBYTE lowercase;
      UBYTE first;
      UBYTE second;
      char *ptr = t_scrn_ptr;
      int j;

      chaddr = chbase_40 + ((UWORD)(screendata & 0x7f) << 3) + char_offset;

      if (screendata & invert_mask)
	invert = 0xff;
      else
	invert = 0x00;

      if (screendata & blank_mask)
	blank = 0x00;
      else
	blank = 0xff;

      if ((screendata & 0x60) == 0x60)
	lowercase = TRUE;
      else
	lowercase = FALSE;

      for (j=0;j<10;j++)
	{
	  UBYTE chdata;

	  if (lowercase)
	    {
	      switch (j)
		{
		case 0 :
		  first = memory[chaddr];
		  chaddr += char_delta;
		  chdata = 0;
		  break;
		case 1 :
		  second = memory[chaddr];
		  chaddr += char_delta;
		  chdata = 0;
		  break;
		case 8 :
		  chdata = first;
		  break;
		case 9 :
		  chdata = second;
		  break;
		default :
		  chdata = memory[chaddr];
		  chaddr += char_delta;
		  break;
		}
	    }
	  else if (j < 8)
	    {
	      chdata = memory[chaddr];
	      chaddr += char_delta;
	    }
	  else
	    {
	      chdata = 0;
	    }

	  chdata = (chdata ^ invert) & blank;

	  if (chdata)
	    {
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
#ifdef DIRECT_VIDEO
	      *ptr++ = lookup1[0];
	      *ptr++ = lookup1[0];
	      *ptr++ = lookup1[0];
	      *ptr++ = lookup1[0];
	      *ptr++ = lookup1[0];
	      *ptr++ = lookup1[0];
	      *ptr++ = lookup1[0];
	      *ptr++ = lookup1[0];
#else
	      *ptr++ = PF_COLPF2;
	      *ptr++ = PF_COLPF2;
	      *ptr++ = PF_COLPF2;
	      *ptr++ = PF_COLPF2;
	      *ptr++ = PF_COLPF2;
	      *ptr++ = PF_COLPF2;
	      *ptr++ = PF_COLPF2;
	      *ptr++ = PF_COLPF2;
#endif
	    }

	  ptr += (ATARI_WIDTH - 8);
	}

      t_scrn_ptr += 8;
    }
}

/*
 * Funtion to display Antic Mode 4
 */

void antic_4 ()
{
  UWORD *t_scrn_ptr = (UWORD*)&scrn_ptr[xmin+scroll_offset];
  int nchars = (xmax - xmin) >> 3;
  int i;

/*
   =================================
   Pixel values when character < 128
   =================================
*/
#ifdef DIRECT_VIDEO
  lookup1[0x00] = (colour_lookup[8] << 8) | colour_lookup[8];
  lookup1[0x40] = lookup1[0x10] = lookup1[0x04] =
    lookup1[0x01] = (colour_lookup[4] << 8) | colour_lookup[4];
  lookup1[0x80] = lookup1[0x20] = lookup1[0x08] =
    lookup1[0x02] = (colour_lookup[5] << 8) | colour_lookup[5];
  lookup1[0xc0] = lookup1[0x30] = lookup1[0x0c] =
    lookup1[0x03] = (colour_lookup[7] << 8) | colour_lookup[6];
#else
  lookup1[0x00] = PF2_COLBK;
  lookup1[0x40] = lookup1[0x10] = lookup1[0x04] =
    lookup1[0x01] = PF2_COLPF0;
  lookup1[0x80] = lookup1[0x20] = lookup1[0x08] =
    lookup1[0x02] = PF2_COLPF1;
  lookup1[0xc0] = lookup1[0x30] = lookup1[0x0c] =
    lookup1[0x03] = PF2_COLPF2;
#endif
/*
   ==================================
   Pixel values when character >= 128
   ==================================
*/
#ifdef DIRECT_VIDEO
  lookup2[0x00] = (colour_lookup[8] << 8) | colour_lookup[8];
  lookup2[0x40] = lookup2[0x10] = lookup2[0x04] =
    lookup2[0x01] = (colour_lookup[4] << 8) | colour_lookup[4];
  lookup2[0x80] = lookup2[0x20] = lookup2[0x08] =
    lookup2[0x02] = (colour_lookup[6] << 8) | colour_lookup[5];
  lookup2[0xc0] = lookup2[0x30] = lookup2[0x0c] =
    lookup2[0x03] = (colour_lookup[7] << 8) | colour_lookup[7];
#else
  lookup2[0x00] = PF2_COLBK;
  lookup2[0x40] = lookup2[0x10] = lookup2[0x04] =
    lookup2[0x01] = PF2_COLPF0;
  lookup2[0x80] = lookup2[0x20] = lookup2[0x08] =
    lookup2[0x02] = PF2_COLPF1;
  lookup2[0xc0] = lookup2[0x30] = lookup2[0x0c] =
    lookup2[0x03] = PF2_COLPF3;
#endif

  for (i=0;i<nchars;i++)
    {
      UBYTE screendata = memory[screenaddr++];
      UWORD chaddr;
      UWORD *lookup;
      UWORD *ptr = t_scrn_ptr;
      int j;

      chaddr = chbase_40 + ((UWORD)(screendata & 0x7f) << 3) + char_offset;

      if (screendata & 0x80)
	lookup = lookup2;
      else
	lookup = lookup1;

      for (j=0;j<8;j++)
	{
	  UBYTE chdata;

	  chdata = memory[chaddr];
	  chaddr += char_delta;

	  if ((j >= vskipbefore) && (j <= vskipafter))
	    {
	      if (chdata)
		{
		  UWORD colour;

		  colour = lookup[chdata & 0xc0];
		  *ptr++ = colour;

		  colour = lookup[chdata & 0x30];
		  *ptr++ = colour;

		  colour = lookup[chdata & 0x0c];
		  *ptr++ = colour;

		  colour = lookup[chdata & 0x03];
		  *ptr++ = colour;
		}
	      else
		{
#ifdef DIRECT_VIDEO
		  *ptr++ = lookup1[0];
		  *ptr++ = lookup1[0];
		  *ptr++ = lookup1[0];
		  *ptr++ = lookup1[0];
#else
		  *ptr++ = PF2_COLBK;
		  *ptr++ = PF2_COLBK;
		  *ptr++ = PF2_COLBK;
		  *ptr++ = PF2_COLBK;
#endif
		}

	      ptr += ((ATARI_WIDTH - 8) >> 1);
	    }
	}

      t_scrn_ptr += 4;
    }
}

/*
 * Function to Display Antic Mode 5
 */

void antic_5 ()
{
  UWORD *t_scrn_ptr1 = (UWORD*)&scrn_ptr[xmin+scroll_offset];
  UWORD *t_scrn_ptr2 = (UWORD*)&scrn_ptr[xmin+ATARI_WIDTH+scroll_offset];
  int nchars = nchars = (xmax - xmin) >> 3;
  int i;

/*
   =================================
   Pixel values when character < 128
   =================================
*/
#ifdef DIRECT_VIDEO
  lookup1[0x00] = (colour_lookup[8] << 8) | colour_lookup[8];;
  lookup1[0x40] = lookup1[0x10] = lookup1[0x04] =
    lookup1[0x01] = (colour_lookup[4] << 8) | colour_lookup[4];
  lookup1[0x80] = lookup1[0x20] = lookup1[0x08] =
    lookup1[0x02] = (colour_lookup[5] << 8) | colour_lookup[5];
  lookup1[0xc0] = lookup1[0x30] = lookup1[0x0c] =
    lookup1[0x03] = (colour_lookup[7] << 8) | colour_lookup[6];
#else
  lookup1[0x00] = PF2_COLBK;
  lookup1[0x40] = lookup1[0x10] = lookup1[0x04] =
    lookup1[0x01] = PF2_COLPF0;
  lookup1[0x80] = lookup1[0x20] = lookup1[0x08] =
    lookup1[0x02] = PF2_COLPF1;
  lookup1[0xc0] = lookup1[0x30] = lookup1[0x0c] =
    lookup1[0x03] = PF2_COLPF2;
#endif
/*
   ==================================
   Pixel values when character >= 128
   ==================================
*/
#ifdef DIRECT_VIDEO
  lookup2[0x00] = (colour_lookup[8] << 8) | colour_lookup[8];
  lookup2[0x40] = lookup2[0x10] = lookup2[0x04] =
    lookup2[0x01] = (colour_lookup[4] << 8) | colour_lookup[4];
  lookup2[0x80] = lookup2[0x20] = lookup2[0x08] =
    lookup2[0x02] = (colour_lookup[6] << 8) | colour_lookup[5];
  lookup2[0xc0] = lookup2[0x30] = lookup2[0x0c] =
    lookup2[0x03] = (colour_lookup[7] << 8) | colour_lookup[7];
#else
  lookup2[0x00] = PF2_COLBK;
  lookup2[0x40] = lookup2[0x10] = lookup2[0x04] =
    lookup2[0x01] = PF2_COLPF0;
  lookup2[0x80] = lookup2[0x20] = lookup2[0x08] =
    lookup2[0x02] = PF2_COLPF1;
  lookup2[0xc0] = lookup2[0x30] = lookup2[0x0c] =
    lookup2[0x03] = PF2_COLPF3;
#endif

  for (i=0;i<nchars;i++)
    {
      UBYTE screendata = memory[screenaddr++];
      UWORD chaddr;
      UWORD *lookup;
      UWORD *ptr1 = t_scrn_ptr1;
      UWORD *ptr2 = t_scrn_ptr2;
      int j;

      chaddr = chbase_40 + ((UWORD)(screendata & 0x7f) << 3) + char_offset;

      if (screendata & 0x80)
	lookup = lookup2;
      else
	lookup = lookup1;

      for (j=0;j<16;j+=2)
	{
	  UBYTE chdata;

	  chdata = memory[chaddr];
	  chaddr += char_delta;

	  if ((j >= vskipbefore) && (j <= vskipafter))
	    {
	      if (chdata)
		{
		  UWORD colour;

		  colour = lookup[chdata & 0xc0];
		  *ptr1++ = colour;
		  *ptr2++ = colour;

		  colour = lookup[chdata & 0x30];
		  *ptr1++ = colour;
		  *ptr2++ = colour;

		  colour = lookup[chdata & 0x0c];
		  *ptr1++ = colour;
		  *ptr2++ = colour;

		  colour = lookup[chdata & 0x03];
		  *ptr1++ = colour;
		  *ptr2++ = colour;
		}
	      else
		{
#ifdef DIRECT_VIDEO
		  *ptr1++ = lookup1[0];
		  *ptr1++ = lookup1[0];
		  *ptr1++ = lookup1[0];
		  *ptr1++ = lookup1[0];

		  *ptr2++ = lookup1[0];
		  *ptr2++ = lookup1[0];
		  *ptr2++ = lookup1[0];
		  *ptr2++ = lookup1[0];
#else
		  *ptr1++ = PF2_COLBK;
		  *ptr1++ = PF2_COLBK;
		  *ptr1++ = PF2_COLBK;
		  *ptr1++ = PF2_COLBK;

		  *ptr2++ = PF2_COLBK;
		  *ptr2++ = PF2_COLBK;
		  *ptr2++ = PF2_COLBK;
		  *ptr2++ = PF2_COLBK;
#endif
		}

	      ptr1 += ((ATARI_WIDTH + ATARI_WIDTH - 8) >> 1);
	      ptr2 += ((ATARI_WIDTH + ATARI_WIDTH - 8) >> 1);
	    }
	}

      t_scrn_ptr1 += (8 >> 1);
      t_scrn_ptr2 += (8 >> 1);
    }
}

/*
 * Function to Display Antic Mode 6
 */

void antic_6 ()
{
  UWORD *t_scrn_ptr = (UWORD*)&scrn_ptr[xmin+scroll_offset];
  int nchars = (xmax - xmin) >> 4; /* Divide by 16 */
  int i;

  for (i=0;i<nchars;i++)
    {
      UBYTE screendata = memory[screenaddr++];
      UWORD chaddr;
      UWORD *ptr = t_scrn_ptr;
      UWORD colour;
      int j;

      chaddr = chbase_20 + ((UWORD)(screendata & 0x3f) << 3) + char_offset;

      switch (screendata & 0xc0)
	{
	case 0x00 :
#ifdef DIRECT_VIDEO
	  colour = (colour_lookup[4] << 8) | colour_lookup[4];
#else
	  colour = PF2_COLPF0;
#endif
	  break;
	case 0x40 :
#ifdef DIRECT_VIDEO
	  colour = (colour_lookup[5] << 8) | colour_lookup[5];
#else
	  colour = PF2_COLPF1;
#endif
	  break;
	case 0x80 :
#ifdef DIRECT_VIDEO
	  colour = (colour_lookup[6] << 8) | colour_lookup[6];
#else
	  colour = PF2_COLPF2;
#endif
	  break;
	case 0xc0 :
#ifdef DIRECT_VIDEO
	  colour = (colour_lookup[7] << 8) | colour_lookup[7];
#else
	  colour = PF2_COLPF3;
#endif
	  break;
	}

      for (j=0;j<8;j++)
	{
	  UBYTE chdata;
	  int k;

	  chdata = memory[chaddr];
	  chaddr += char_delta;

	  if ((j >= vskipbefore) && (j <= vskipafter))
	    {
	      for (k=0;k<8;k++)
		{
		  if (chdata & 0x80)
		    {
		      *ptr++ = colour;
		    }
		  else
		    {
#ifdef DIRECT_VIDEO
		      *ptr++ = (colour_lookup[8] << 8) | colour_lookup[8];
#else
		      *ptr++ = PF2_COLBK;
#endif
		    }
	      
		  chdata = chdata << 1;
		}

	      ptr += ((ATARI_WIDTH - 16) >> 1);
	    }
	}

      t_scrn_ptr += (16 >> 1);
    }
}

/*
 * Function to Display Antic Mode 7
 */

void antic_7 ()
{
  UWORD *t_scrn_ptr1 = (UWORD*)&scrn_ptr[xmin+scroll_offset];
  UWORD *t_scrn_ptr2 = (UWORD*)&scrn_ptr[xmin+ATARI_WIDTH+scroll_offset];
  int nchars = (xmax - xmin) >> 4; /* Divide by 16 */
  int i;

  for (i=0;i<nchars;i++)
    {
      UBYTE screendata = memory[screenaddr++];
      UWORD chaddr;
      UWORD *ptr1 = t_scrn_ptr1;
      UWORD *ptr2 = t_scrn_ptr2;
      UWORD colour;
      int j;

      chaddr = chbase_20 + ((UWORD)(screendata & 0x3f) << 3) + char_offset;

      switch (screendata & 0xc0)
	{
	case 0x00 :
#ifdef DIRECT_VIDEO
	  colour = (colour_lookup[4] << 8) | colour_lookup[4];
#else
	  colour = PF2_COLPF0;
#endif
	  break;
	case 0x40 :
#ifdef DIRECT_VIDEO
	  colour = (colour_lookup[5] << 8) | colour_lookup[5];
#else
	  colour = PF2_COLPF1;
#endif
	  break;
	case 0x80 :
#ifdef DIRECT_VIDEO
	  colour = (colour_lookup[6] << 8) | colour_lookup[6];
#else
	  colour = PF2_COLPF2;
#endif
	  break;
	case 0xc0 :
#ifdef DIRECT_VIDEO
	  colour = (colour_lookup[7] << 8) | colour_lookup[7];
#else
	  colour = PF2_COLPF3;
#endif
	  break;
	}

      for (j=0;j<8;j++)
	{
	  UBYTE chdata;
	  int k;

	  chdata = memory[chaddr];
	  chaddr += char_delta;

	  if ((j >= vskipbefore) && (j <= vskipafter))
	    {
	      for (k=0;k<8;k++)
		{
		  if (chdata & 0x80)
		    {
		      *ptr1++ = colour;
		      *ptr2++ = colour;
		    }
		  else
		    {
#ifdef DIRECT_VIDEO
		      *ptr1++ = (colour_lookup[8] << 8) | colour_lookup[8];
		      *ptr2++ = (colour_lookup[8] << 8) | colour_lookup[8];
#else
		      *ptr1++ = PF2_COLBK;
		      *ptr2++ = PF2_COLBK;
#endif
		    }

		  chdata = chdata << 1;
		}

	      ptr1 += ((ATARI_WIDTH + ATARI_WIDTH - 16) >> 1);
	      ptr2 += ((ATARI_WIDTH + ATARI_WIDTH - 16) >> 1);
	    }
	}

      t_scrn_ptr1 += (16 >> 1);
      t_scrn_ptr2 += (16 >> 1);
    }
}

/*
 * Function to Display Antic Mode 8
 */

void antic_8 ()
{
  char *t_scrn_ptr = &scrn_ptr[xmin+scroll_offset];
  int nbytes = (xmax - xmin) >> 5; /* Divide by 32 */
  int	i;

#ifdef DIRECT_VIDEO
  lookup1[0x00] = colour_lookup[8];
  lookup1[0x40] = colour_lookup[4];
  lookup1[0x80] = colour_lookup[5];
  lookup1[0xc0] = colour_lookup[6];
#else
  lookup1[0x00] = PF_COLBK;
  lookup1[0x40] = PF_COLPF0;
  lookup1[0x80] = PF_COLPF1;
  lookup1[0xc0] = PF_COLPF2;
#endif

  for (i=0;i<nbytes;i++)
    {
      UBYTE screendata = memory[screenaddr++];
      int j;

      for (j=0;j<4;j++)
	{
	  UBYTE colour;
	  char *ptr = t_scrn_ptr;
	  int k;

	  colour = lookup1[screendata & 0xc0];

	  for (k=0;k<8;k++)
	    {
	      memset (ptr, colour, 8);
	      ptr += ATARI_WIDTH;
	    }

	  screendata = screendata << 2;
	  t_scrn_ptr += 8;
	}
    }
}

/*
 * Function to Display Antic Mode 9
 */

void antic_9 ()
{
  char *t_scrn_ptr = &scrn_ptr[xmin+scroll_offset];
  int nbytes = (xmax - xmin) >> 5; /* Divide by 32 */
  int i;

#ifdef DIRECT_VIDEO
  lookup1[0x00] = colour_lookup[8];
  lookup1[0x80] = colour_lookup[4];
#else
  lookup1[0x00] = PF_COLBK;
  lookup1[0x80] = PF_COLPF0;
#endif

  for (i=0;i<nbytes;i++)
    {
      UBYTE screendata = memory[screenaddr++];
      int j;

      for (j=0;j<8;j++)
	{
	  UBYTE colour;
	  char *ptr = t_scrn_ptr;
	  int k;

	  colour = lookup1[screendata & 0x80];

	  for (k=0;k<4;k++)
	    {
	      memset (ptr, colour, 4);
	      ptr += ATARI_WIDTH;
	    }

	  screendata = screendata << 1;
	  t_scrn_ptr += 4;
	}
    }
}

/*
 * Function to Display Antic Mode a
 */

void antic_a ()
{
  char *t_scrn_ptr = &scrn_ptr[xmin+scroll_offset];
  int nbytes = (xmax - xmin) >> 4; /* Divide by 16 */
  int i;

#ifdef DIRECT_VIDEO
  lookup1[0x00] = colour_lookup[8];
  lookup1[0x40] = colour_lookup[4];
  lookup1[0x80] = colour_lookup[5];
  lookup1[0xc0] = colour_lookup[6];
#else
  lookup1[0x00] = PF_COLBK;
  lookup1[0x40] = PF_COLPF0;
  lookup1[0x80] = PF_COLPF1;
  lookup1[0xc0] = PF_COLPF2;
#endif

  for (i=0;i<nbytes;i++)
    {
      UBYTE screendata = memory[screenaddr++];
      int j;

      for (j=0;j<4;j++)
	{
	  UBYTE colour;
	  char *ptr = t_scrn_ptr;
	  int k;

	  colour = lookup1[screendata & 0xc0];

	  for (k=0;k<4;k++)
	    {
	      memset (ptr, colour, 4);
	      ptr += ATARI_WIDTH;
	    }

	  screendata = screendata << 2;
	  t_scrn_ptr += 4;
	}
    }
}

/*
 * Function to Display Antic Mode b
 */

void antic_b ()
{
  char *t_scrn_ptr1 = &scrn_ptr[xmin+scroll_offset];
  char *t_scrn_ptr2 = &scrn_ptr[xmin+ATARI_WIDTH+scroll_offset];
  int nbytes = (xmax - xmin) >> 4; /* Divide by 16 */
  int i;

#ifdef DIRECT_VIDEO
  lookup1[0x00] = colour_lookup[8];
  lookup1[0x80] = colour_lookup[4];
#else
  lookup1[0x00] = PF_COLBK;
  lookup1[0x80] = PF_COLPF0;
#endif

  for (i=0;i<nbytes;i++)
    {
      UBYTE screendata = memory[screenaddr++];
      UWORD colour;
      int j;

      for (j=0;j<8;j++)
	{
	  colour = lookup1[screendata & 0x80];
	  *t_scrn_ptr1++ = colour;
	  *t_scrn_ptr1++ = colour;
	  *t_scrn_ptr2++ = colour;
	  *t_scrn_ptr2++ = colour;
	  screendata = screendata << 1;
	}
    }
}

/*
 * Function to Display Antic Mode c
 */

void antic_c ()
{
  char *t_scrn_ptr = &scrn_ptr[xmin+scroll_offset];
  int nbytes = (xmax - xmin) >> 4; /* Divide by 16 */
  int i;

#ifdef DIRECT_VIDEO
  lookup1[0x00] = colour_lookup[8];
  lookup1[0x80] = lookup1[0x40] = lookup1[0x20] = lookup1[0x10] =
    colour_lookup[4];
#else
  lookup1[0x00] = PF_COLBK;
  lookup1[0x80] = lookup1[0x40] = lookup1[0x20] = lookup1[0x10] = PF_COLPF0;
#endif

  for (i=0;i<nbytes;i++)
    {
      UBYTE screendata = memory[screenaddr++];
      int j;

      for (j=0;j<2;j++)
	{
	  UBYTE colour;

	  colour = lookup1[screendata & 0x80];
	  *t_scrn_ptr++ = colour;
	  *t_scrn_ptr++ = colour;

	  colour = lookup1[screendata & 0x40];
	  *t_scrn_ptr++ = colour;
	  *t_scrn_ptr++ = colour;

	  colour = lookup1[screendata & 0x20];
	  *t_scrn_ptr++ = colour;
	  *t_scrn_ptr++ = colour;

	  colour = lookup1[screendata & 0x10];
	  *t_scrn_ptr++ = colour;
	  *t_scrn_ptr++ = colour;

	  screendata <<= 4;
	}
    }
}

/*
 * Function to Display Antic Mode d
 */

void antic_d ()
{
  UWORD *t_scrn_ptr1 = (UWORD*)&scrn_ptr[xmin+scroll_offset];
  UWORD *t_scrn_ptr2 = (UWORD*)&scrn_ptr[ATARI_WIDTH+xmin+scroll_offset];
  int nbytes = (xmax - xmin) >> 3;
  int i;

#ifdef DIRECT_VIDEO
  lookup1[0x00] = (colour_lookup[8] << 8) | colour_lookup[8];
  lookup1[0x40] = (colour_lookup[4] << 8) | colour_lookup[4];
  lookup1[0x80] = (colour_lookup[5] << 8) | colour_lookup[5];
  lookup1[0xc0] = (colour_lookup[6] << 8) | colour_lookup[6];
#else
  lookup1[0x00] = PF2_COLBK;
  lookup1[0x40] = PF2_COLPF0;
  lookup1[0x80] = PF2_COLPF1;
  lookup1[0xc0] = PF2_COLPF2;
#endif

  for (i=0;i<nbytes;i++)
    {
      UBYTE	screendata;
      
      screendata = memory[screenaddr++];

      if (screendata)
	{
	  UWORD colour;

	  colour = lookup1[screendata & 0xc0];
	  *t_scrn_ptr1++ = colour;
	  *t_scrn_ptr2++ = colour;
	  screendata = screendata << 2;

	  colour = lookup1[screendata & 0xc0];
	  *t_scrn_ptr1++ = colour;
	  *t_scrn_ptr2++ = colour;
	  screendata = screendata << 2;

	  colour = lookup1[screendata & 0xc0];
	  *t_scrn_ptr1++ = colour;
	  *t_scrn_ptr2++ = colour;
	  screendata = screendata << 2;

	  colour = lookup1[screendata & 0xc0];
	  *t_scrn_ptr1++ = colour;
	  *t_scrn_ptr2++ = colour;
	  screendata = screendata << 2;
	}
      else
	{
#ifdef DIRECT_VIDEO
	  *t_scrn_ptr1++ = lookup1[0x00];
	  *t_scrn_ptr1++ = lookup1[0x00];
	  *t_scrn_ptr1++ = lookup1[0x00];
	  *t_scrn_ptr1++ = lookup1[0x00];

	  *t_scrn_ptr2++ = lookup1[0x00];
	  *t_scrn_ptr2++ = lookup1[0x00];
	  *t_scrn_ptr2++ = lookup1[0x00];
	  *t_scrn_ptr2++ = lookup1[0x00];
#else
	  *t_scrn_ptr1++ = PF2_COLBK;
	  *t_scrn_ptr1++ = PF2_COLBK;
	  *t_scrn_ptr1++ = PF2_COLBK;
	  *t_scrn_ptr1++ = PF2_COLBK;

	  *t_scrn_ptr2++ = PF2_COLBK;
	  *t_scrn_ptr2++ = PF2_COLBK;
	  *t_scrn_ptr2++ = PF2_COLBK;
	  *t_scrn_ptr2++ = PF2_COLBK;
#endif
	}
    }
}

/*
 * Function to display Antic Mode e
 */

void antic_e ()
{
  UWORD *t_scrn_ptr = (UWORD*)&scrn_ptr[xmin+scroll_offset];
  int nbytes = (xmax - xmin) >> 3;
  int i;
#ifdef UNALIGNED_LONG_OK
  int background;
#endif
  UBYTE *ptr;

#ifdef AMIGA_TEST
  antic_e_test (nbytes-1, &memory[screenaddr], t_scrn_ptr);
  screenaddr += nbytes;
#else

#ifdef DIRECT_VIDEO
  lookup1[0x00] = (colour_lookup[8] << 8) | colour_lookup[8];
  lookup1[0x40] = lookup1[0x10] = lookup1[0x04] = lookup1[0x01] =
    (colour_lookup[4] << 8) | colour_lookup[4];
  lookup1[0x80] = lookup1[0x20] = lookup1[0x08] = lookup1[0x02] =
    (colour_lookup[5] << 8) | colour_lookup[5];
  lookup1[0xc0] = lookup1[0x30] = lookup1[0x0c] = lookup1[0x03] =
    (colour_lookup[6] << 8) | colour_lookup[6];
#else
  lookup1[0x00] = PF2_COLBK;
  lookup1[0x40] = lookup1[0x10] = lookup1[0x04] = lookup1[0x01] =
    PF2_COLPF0;
  lookup1[0x80] = lookup1[0x20] = lookup1[0x08] = lookup1[0x02] =
    PF2_COLPF1;
  lookup1[0xc0] = lookup1[0x30] = lookup1[0x0c] = lookup1[0x03] =
    PF2_COLPF2;
#endif

#ifdef UNALIGNED_LONG_OK
  background = (lookup1[0x00] << 16) | lookup1[0x00];
#endif

  ptr = &memory[screenaddr];

  for (i=0;i<nbytes;i++)
    {
      UBYTE screendata = ptr[i];

      if (screendata)
	{
	  *t_scrn_ptr++ = lookup1[screendata & 0xc0];
	  *t_scrn_ptr++ = lookup1[screendata & 0x30];
	  *t_scrn_ptr++ = lookup1[screendata & 0x0c];
	  *t_scrn_ptr++ = lookup1[screendata & 0x03];
	}
      else
	{
#ifdef UNALIGNED_LONG_OK
	  ULONG *l_ptr  = (ULONG*)t_scrn_ptr;

	  *l_ptr++ = background;
	  *l_ptr++ = background;

	  t_scrn_ptr = (UWORD*)l_ptr;
#else
	  *t_scrn_ptr++ = lookup1[0x00];
	  *t_scrn_ptr++ = lookup1[0x00];
	  *t_scrn_ptr++ = lookup1[0x00];
	  *t_scrn_ptr++ = lookup1[0x00];
#endif
	}
    }

  screenaddr += nbytes;
#endif
}

/*
 * Function to display Antic Mode f
 */

void antic_f ()
{
  char *t_scrn_ptr = &scrn_ptr[xmin+scroll_offset];
  int nbytes = (xmax - xmin) >> 3;
  int i;

#ifdef DIRECT_VIDEO
  lookup1[0x00] = colour_lookup[6];
  lookup1[0x80] = lookup1[0x40] = lookup1[0x20] =
    lookup1[0x10] = lookup1[0x08] = lookup1[0x04] =
      lookup1[0x02] = lookup1[0x01] = colour_lookup[5];
#else
  lookup1[0x00] = PF_COLPF2;
  lookup1[0x80] = lookup1[0x40] = lookup1[0x20] =
    lookup1[0x10] = lookup1[0x08] = lookup1[0x04] =
      lookup1[0x02] = lookup1[0x01] = PF_COLPF1;
#endif

  for (i=0;i<nbytes;i++)
    {
      UBYTE screendata = memory[screenaddr++];

      if (screendata)
	{
	  *t_scrn_ptr++ = lookup1[screendata & 0x80];
	  *t_scrn_ptr++ = lookup1[screendata & 0x40];
	  *t_scrn_ptr++ = lookup1[screendata & 0x20];
	  *t_scrn_ptr++ = lookup1[screendata & 0x10];
	  *t_scrn_ptr++ = lookup1[screendata & 0x08];
	  *t_scrn_ptr++ = lookup1[screendata & 0x04];
	  *t_scrn_ptr++ = lookup1[screendata & 0x02];
	  *t_scrn_ptr++ = lookup1[screendata & 0x01];
	}
      else
	{
#ifdef DIRECT_VIDEO
	  *t_scrn_ptr++ = colour_lookup[6];
	  *t_scrn_ptr++ = colour_lookup[6];
	  *t_scrn_ptr++ = colour_lookup[6];
	  *t_scrn_ptr++ = colour_lookup[6];
	  *t_scrn_ptr++ = colour_lookup[6];
	  *t_scrn_ptr++ = colour_lookup[6];
	  *t_scrn_ptr++ = colour_lookup[6];
	  *t_scrn_ptr++ = colour_lookup[6];
#else
	  *t_scrn_ptr++ = PF_COLPF2;
	  *t_scrn_ptr++ = PF_COLPF2;
	  *t_scrn_ptr++ = PF_COLPF2;
	  *t_scrn_ptr++ = PF_COLPF2;
	  *t_scrn_ptr++ = PF_COLPF2;
	  *t_scrn_ptr++ = PF_COLPF2;
	  *t_scrn_ptr++ = PF_COLPF2;
	  *t_scrn_ptr++ = PF_COLPF2;
#endif
	}
    }
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
	*****************************************************************
*/

void pmg_dma (void)
{
  if (player_dma_enabled)
    {
      if (singleline)
	{
	  GRAFP0 = memory[p0addr_s + ypos];
	  GRAFP1 = memory[p1addr_s + ypos];
	  GRAFP2 = memory[p2addr_s + ypos];
	  GRAFP3 = memory[p3addr_s + ypos];
	}
      else
	{
	  GRAFP0 = memory[p0addr_d + (ypos >> 1)];
	  GRAFP1 = memory[p1addr_d + (ypos >> 1)];
	  GRAFP2 = memory[p2addr_d + (ypos >> 1)];
	  GRAFP3 = memory[p3addr_d + (ypos >> 1)];
	}
    }

  if (missile_dma_enabled)
    {
      if (singleline)
	GRAFM = memory[maddr_s + ypos];
      else
	GRAFM = memory[maddr_d + (ypos >> 1)];
    }
}

void ANTIC_RunDisplayList (void)
{
  UWORD	dlist;
  int JVB;
  int vscrol_flag;
  int nlines;
  int i;

  wsync_halt = 0;

  /*
   * VCOUNT must equal zero for some games but the first line starts
   * when VCOUNT=4. This portion processes when VCOUNT=0, 1, 2 and 3
   */

  for (ypos = 0;ypos < 8; ypos++)
    GO (114);

  NMIST = 0x00; /* Reset VBLANK */

  scrn_ptr = (UBYTE*)atari_screen;

  ypos = 8;
  vscrol_flag = FALSE;

  dlist = (DLISTH << 8) | DLISTL;
  JVB = FALSE;

  while ((DMACTL & 0x20) && !JVB && (ypos < (ATARI_HEIGHT+8)))
    {
      UBYTE IR;
      UBYTE colpf1;
      int colpf1_fiddled = FALSE;

      IR = memory[dlist++];
      colpf1 = COLPF1;

      switch (IR & 0x0f)
	{
	case 0x00 :
	  {
	    nlines = ((IR >> 4)  & 0x07) + 1;
	    antic_blank (nlines);
	  }
	  break;
	case 0x01 :
	  if (IR & 0x40)
	    {
	      nlines = 0;
	      JVB = TRUE;
	    }
	  else
	    {
	      dlist = (memory[dlist+1] << 8) | memory[dlist];
	      nlines = 1;
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
	      if (!vscrol_flag)
		{
		  vskipbefore = VSCROL;
		  vscrol_flag = TRUE;
		}
	    }
	  else if (vscrol_flag)
	    {
	      vskipafter = VSCROL - 1;
	      vscrol_flag = FALSE;
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
	      nlines = 8;
              if (!enable_xcolpf1)
                {
                  GTIA_PutByte (_COLPF1, (COLPF2 & 0xf0) | (COLPF1 & 0x0f));
                  colpf1_fiddled = TRUE;
                }
	      antic_2 ();
	      break;
	    case 0x03 :
	      nlines = 10;
              if (!enable_xcolpf1)
                {
                  GTIA_PutByte (_COLPF1, (COLPF2 & 0xf0) | (COLPF1 & 0x0f));
                  colpf1_fiddled = TRUE;
                }
	      antic_3 ();
	      break;
	    case 0x04 :
	      nlines = 8;
	      antic_4 ();
	      break;
	    case 0x05 :
	      nlines = 16;
	      antic_5 ();
	      break;
	    case 0x06 :
	      nlines = 8;
	      antic_6 ();
	      break;
	    case 0x07 :
	      nlines = 16;
	      antic_7 ();
	      break;
	    case 0x08 :
	      nlines = 8;
	      antic_8 ();
	      break;
	    case 0x09 :
	      nlines = 4;
	      antic_9 ();
	      break;
	    case 0x0a :
	      nlines = 4;
	      antic_a ();
	      break;
	    case 0x0b :
	      nlines = 2;
	      antic_b ();
	      break;
	    case 0x0c :
	      nlines = 1;
	      antic_c ();
	      break;
	    case 0x0d :
	      nlines = 2;
	      antic_d ();
	      break;
	    case 0x0e :
	      nlines = 1;
	      antic_e ();
	      break;
	    case 0x0f :
	      nlines = 1;
              if (!enable_xcolpf1)
                {
                  GTIA_PutByte (_COLPF1, (COLPF2 & 0xf0) | (COLPF1 & 0x0f));
                  colpf1_fiddled = TRUE;
                }
	      antic_f ();
	      break;
	    default :
	      nlines = 0;
	      JVB = TRUE;
	      break;
	    }
	  break;
	}

      if (nlines > 0)
	{
	  nlines--;

/*
 * Should be able to optimise the (i >= vskipbefore) ...
 * into just the number of lines to display :-)
 */

	  for (i=0;i<nlines;i++)
	    {
	      if ((i >= vskipbefore) && (i <= vskipafter))
		{
		  pmg_dma ();
		  Atari_ScanLine ();
		}
	    }

	  if (IR & 0x80)
	    {
	      if (NMIEN & 0x80)
		{
		  NMIST |= 0x80;
		  NMI ();
		}
	    }

	  pmg_dma ();
	  Atari_ScanLine ();
	}

      vskipbefore = 0;
      vskipafter = 99;

      if (colpf1_fiddled)
        GTIA_PutByte (_COLPF1, colpf1);
    }

  nlines = (ATARI_HEIGHT+8) - ypos;
  antic_blank (nlines);
  for (i=0;i<nlines;i++)
    {
      pmg_dma ();
      Atari_ScanLine ();
    }

  NMIST = 0x40; /* Set VBLANK */
  if (NMIEN & 0x40)
    {
      GO (1); /* Needed for programs that monitor NMIST (Spy's Demise) */
      NMI ();
    }

  if (tv_mode == PAL)
    for (ypos=248;ypos<312;ypos++)
      GO (114);
  else
    for (ypos=248;ypos<262;ypos++)
      GO (114);
}

UBYTE ANTIC_GetByte (UWORD addr)
{
  UBYTE byte;

  addr &= 0xff0f;
  switch (addr)
    {
    case _CHBASE :
      byte = CHBASE;
      break;
    case _CHACTL :
      byte = CHACTL;
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
    case _PENH :
    case _PENV :
      byte = 0x00;
      break;
    case _VCOUNT :
      byte = ypos >> 1;
      break;
    case _NMIEN :
      byte = NMIEN;
      break;
    case _NMIST :
      byte = NMIST;
      break;
    case _WSYNC :
      wsync_halt++;
      byte = 0;
      break;
    }

  return byte;
}

int ANTIC_PutByte (UWORD addr, UBYTE byte)
{
  int abort = FALSE;

  addr &= 0xff0f;
  switch (addr)
    {
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

      if (DMACTL & 0x04)
	missile_dma_enabled = TRUE;
      else
	missile_dma_enabled = FALSE;

      if (DMACTL & 0x08)
	player_dma_enabled = TRUE;
      else
	player_dma_enabled = FALSE;

      if (DMACTL & 0x10)
	singleline = TRUE;
      else
	singleline = FALSE;
      break;
    case _HSCROL :
      HSCROL = byte & 0x0f;
      break;
    case _NMIEN :
      NMIEN = byte;
      break;
    case _NMIRES :
      NMIST = 0x00;
      break;
    case _PMBASE :
	{
	  UWORD pmbase_s;
	  UWORD pmbase_d;

	  PMBASE = byte;

	  pmbase_s = (PMBASE & 0xf8) << 8;
	  pmbase_d = (PMBASE & 0xfc) << 8;

	  maddr_s = pmbase_s + 768;
	  p0addr_s = pmbase_s + 1024;
	  p1addr_s = pmbase_s + 1280;
	  p2addr_s = pmbase_s + 1536;
	  p3addr_s = pmbase_s + 1792;

	  maddr_d = pmbase_d + 384;
	  p0addr_d = pmbase_d + 512;
	  p1addr_d = pmbase_d + 640;
	  p2addr_d = pmbase_d + 768;
	  p3addr_d = pmbase_d + 896;
	}
      break;
    case _VSCROL :
      VSCROL = byte & 0x0f;
      break;
    case _WSYNC :
      wsync_halt++;
      abort = TRUE;
      break;
    }

  return abort;
}
