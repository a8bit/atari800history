#include <stdio.h>

#include "atari.h"
#include "cpu.h"
#include "pia.h"
#include "platform.h"

#define FALSE 0
#define TRUE 1

static char *rcsid = "$Id: pia.c,v 1.10 1997/03/31 09:34:33 david Exp $";

UBYTE PACTL;
UBYTE PBCTL;
UBYTE PORTA;
UBYTE PORTB;

int xe_bank = -1;

int rom_inserted;
UBYTE atari_basic[8192];
UBYTE atarixl_os[16384];
static UBYTE under_atari_basic[8192];
static UBYTE under_atarixl_os[16384];
static UBYTE atarixe_memory[65536];
static UBYTE atarixe_16kbuffer[16384];

static UBYTE PORTA_mask = 0xff;
static UBYTE PORTB_mask = 0xff;

void PIA_Initialise (int *argc, char *argv[])
{
  PORTA = 0xff;
  PORTB = 0xff;
}

UBYTE PIA_GetByte (UWORD addr)
{
  UBYTE byte;

  if (machine == Atari)
    addr &= 0xff03;
  switch (addr)
    {
    case _PACTL :
      byte = PACTL;
      break;
    case _PBCTL :
      byte = PBCTL;
#ifdef DEBUG1
      printf ("RD: PBCTL = %x, PC = %x\n", PBCTL, PC);
#endif
      break;
    case _PORTA :
      byte = Atari_PORT (0);
      byte &= PORTA_mask;
      break;
    case _PORTB :
      switch (machine)
	{
	case Atari :
	  byte = Atari_PORT (1);
          byte &= PORTB_mask;
	  break;
	case AtariXL :
	case AtariXE :
	  byte = PORTB;
	  break;
	default :
	  printf ("Fatal Error in pia.c: PIA_GetByte(): Unknown machine\n");
	  Atari800_Exit (FALSE);
	  exit (1);
	  break;
	}
      break;
    }

  return byte;
}

int PIA_PutByte (UWORD addr, UBYTE byte)
{
  if (machine == Atari)
    addr &= 0xff03;

  switch (addr)
    {
    case _PACTL :
      PACTL = byte;
      break;
    case _PBCTL :
      PBCTL = byte;
#ifdef DEBUG1
      printf ("WR: PBCTL = %x, PC = %x\n", PBCTL, PC);
#endif
      break;
    case _PORTA :
      if (!(PACTL & 0x04))
        PORTA_mask = ~byte;
      break;
    case _PORTB :
      switch (machine)
	{
	case Atari :
          if (!(PBCTL & 0x04))
            PORTB_mask = ~byte;
	  break;
	case AtariXE :
	  {
	    int cpu_flag = (byte & 0x10);
#ifdef DEBUG
	    int antic_flag = (byte & 0x20);
#endif
	    int bank = (byte & 0x0c) >> 2;

#ifdef DEBUG
	    printf ("CPU = %d, ANTIC = %d, XE BANK = %d\n",
		    cpu_flag, antic_flag, bank);
#endif

/*
 * Possible Bank Transitions
 *
 * Main        -> Main
 * Main        -> Bank1,2,3,4
 * Bank1,2,3,4 -> Main
 * Bank1,2,3,4 -> Bank1,2,3,4
 */
	    if (cpu_flag)
	      {
		if (xe_bank != -1)
		  {
		    memcpy (&atarixe_memory[xe_bank*16384],
			    &memory[0x4000],
			    16384);
		    memcpy (&memory[0x4000], atarixe_16kbuffer, 16384);
		    xe_bank = -1;
		  }
	      }
	    else if (bank != xe_bank)
	      {
		if (xe_bank == -1)
		  {
		    memcpy (atarixe_16kbuffer,
			    &memory[0x4000],
			    16384);
		  }
		else
		  {
		    memcpy (&atarixe_memory[xe_bank*16384],
			    &memory[0x4000],
			    16384);
		  }

		memcpy (&memory[0x4000],
			&atarixe_memory[bank*16384],
			16384);
		xe_bank = bank;
	      }
	  }
	case AtariXL :
#ifdef DEBUG
	  printf ("Storing %x to PORTB, PC = %x\n", byte, regPC);
#endif
/*
 * Enable/Disable OS ROM 0xc000-0xcfff and 0xd800-0xffff
 */
	  if (!(PORTB & 0x01))
	    {
	      memcpy (under_atarixl_os, memory+0xc000, 0x1000);
	      memcpy (under_atarixl_os+0x1800, memory+0xd800, 0x2800);
	    }

	  if (byte & 0x01)
	    {
#ifdef DEBUG
	      printf ("OS ROM Enabled\n");
#endif
	      memcpy (memory+0xc000, atarixl_os, 0x1000);
	      memcpy (memory+0xd800, atarixl_os+0x1800, 0x2800);
	      SetROM (0xc000, 0xcfff);
	      SetROM (0xd800, 0xffff);
	    }
	  else
	    {
#ifdef DEBUG
	      printf ("OS ROM Disabled\n");
#endif
	      memcpy (memory+0xc000, under_atarixl_os, 0x1000);
	      memcpy (memory+0xd800, under_atarixl_os+0x1800, 0x2800);
	      SetRAM (0xc000, 0xcfff);
	      SetRAM (0xd800, 0xffff);
	    }
/*
	=====================================
	An Atari XL/XE can only disable Basic
	Other cartridge cannot be disable
	=====================================
*/
	  if (!rom_inserted)
	    {
/*
 * If RAM is currently enabled between 0xa000 and
 * 0xbfff then under_atari_basic is updated
 */
	      if (PORTB & 0x02)
		{
		  memcpy (under_atari_basic, memory+0xa000, 0x2000);
		}
/*
 * Enable/Disable BASIC ROM
 */
	      if (byte & 0x02)
		{
#ifdef DEBUG
		  printf ("BASIC disabled\n");
#endif
		  memcpy (memory+0xa000, under_atari_basic, 0x2000);
		  SetRAM (0xa000, 0xbfff);
		}
	      else
		{
#ifdef DEBUG
		  printf ("BASIC enabled\n");
#endif
		  memcpy (memory+0xa000, atari_basic, 0x2000);
		  SetROM (0xa000, 0xbfff);
		}
	    }
/*
 * Enable/Disable Self Test ROM
 */
	  if (PORTB & 0x80)
	    {
	      memcpy (under_atarixl_os+0x1000, memory+0x5000, 0x800);
	    }

	  if (byte & 0x80)
	    {
#ifdef DEBUG
	      printf ("Self Test ROM Disabled\n");
#endif
	      memcpy (memory+0x5000, under_atarixl_os+0x1000, 0x800);
	      SetRAM (0x5000, 0x57ff);
	    }
	  else
	    {
#ifdef DEBUG
	      printf ("Self Test ROM Enabled\n");
#endif
	      memcpy (memory+0x5000, atarixl_os+0x1000, 0x800);
	      SetROM (0x5000, 0x57ff);
	    }

	  PORTB = byte;
	  break;
	default :
	  printf ("Fatal Error in pia.c: PIA_PutByte(): Unknown machine\n");
	  Atari800_Exit (FALSE);
	  exit (1);
	  break;
	}
      break;
    }

  return FALSE;
}
