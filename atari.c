#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef VMS
#include <unixio.h>
#include <file.h>
#else
#include <fcntl.h>
#endif

#ifdef DJGPP
#include "djgpp.h"
#endif

static char *rcsid = "$Id: atari.c,v 1.30 1996/09/29 22:01:05 david Exp $";

#define FALSE   0
#define TRUE    1

#ifdef VMS
#define DEFAULT_REFRESH_RATE 4
#else
#ifdef AMIGA
#define DEFAULT_REFRESH_RATE 4
#else
#include "config.h"
#endif
#endif

#include "atari.h"
#include "cpu.h"
#include "antic.h"
#include "gtia.h"
#include "pia.h"
#include "pokey.h"
#include "supercart.h"
#include "ffp.h"
#include "devices.h"
#include "sio.h"
#include "monitor.h"
#include "platform.h"

Machine machine = Atari;

static int os = 2;
static int enable_c000 = FALSE;
static int ffp_patch = FALSE;
static int sio_patch = TRUE;

extern UBYTE NMIEN;
extern UBYTE NMIST;
extern UBYTE PORTA;
extern UBYTE PORTB;

extern int xe_bank;
extern int rom_inserted;
extern UBYTE atari_basic[8192];
extern UBYTE atarixl_os[16384];

UBYTE *cart_image = NULL;	/* For cartridge memory */
int cart_type = NO_CART;
int DELAYED_SERIN_IRQ;
int DELAYED_SEROUT_IRQ;
int DELAYED_XMTDONE_IRQ;
int countdown_rate = 4000;
int refresh_rate = DEFAULT_REFRESH_RATE;

#ifdef PAL
double deltatime = (1.0 / 50.0);
#else
double deltatime = (1.0 / 60.0);
#endif

static char *atari_library = ATARI_LIBRARY;

void add_esc (UWORD address, UBYTE esc_code);
int Atari800_Exit (int run_monitor);
void Atari800_Hardware (void);

int load_cart (char *filename, int type);

static char *rom_filename = NULL;

void sigint_handler (int num)
{
  int restart;
  int diskno;

  restart = Atari800_Exit (TRUE);
  if (restart)
    {
      signal (SIGINT, sigint_handler);
      return;
    }

  for (diskno=1;diskno<8;diskno++)
    SIO_Dismount (diskno);

  exit (0);
}


int load_image (char *filename, int addr, int nbytes)
{
  int	status = FALSE;
  int	fd;

  fd = open (filename, O_RDONLY, 0777);
  if (fd != -1)
    {
      status = read (fd, &memory[addr], nbytes);
      if (status != nbytes)
	{
	  printf ("Error reading %s\n", filename);
	  Atari800_Exit (FALSE);
	  exit (1);
	}

      close (fd);

      status = TRUE;
    }

  return status;
}

void EnablePILL (void)
{
  SetROM (0x8000, 0xbfff);
}

/*
 * Load a standard 8K ROM from the specified file
 */

int Insert_8K_ROM (char *filename)
{
  int status = FALSE;
  int fd;

  fd = open (filename, O_RDONLY, 0777);
  if (fd != -1)
    {
      read (fd, &memory[0xa000], 0x2000);
      close (fd);
      SetRAM (0x8000, 0x9fff);
      SetROM (0xa000, 0xbfff);
      cart_type = NORMAL8_CART;
      rom_inserted = TRUE;
      status = TRUE;
    }

  return status;
}

/*
 * Load a standard 16K ROM from the specified file
 */

int Insert_16K_ROM (char *filename)
{
  int status = FALSE;
  int fd;

  fd = open (filename, O_RDONLY, 0777);
  if (fd != -1)
    {
      read (fd, &memory[0x8000], 0x4000);
      close (fd);
      SetROM (0x8000, 0xbfff);
      cart_type = NORMAL16_CART;
      rom_inserted = TRUE;
      status = TRUE;
    }

  return status;
}

/*
 * Load an OSS Supercartridge from the specified file
 * The OSS cartridge is a 16K bank switched cartridge
 * that occupies 8K of address space between $a000
 * and $bfff
 */

int Insert_OSS_ROM (char *filename)
{
  int status = FALSE;
  int fd;

  fd = open (filename, O_RDONLY, 0777);
  if (fd != -1)
    {
	cart_image = (UBYTE*)malloc(0x4000);
	if (cart_image)
	  {
	    read (fd, cart_image, 0x4000);
	    memcpy (&memory[0xa000], cart_image, 0x1000);
	    memcpy (&memory[0xb000], cart_image+0x3000, 0x1000);
	    SetRAM (0x8000, 0x9fff);
	    SetROM (0xa000, 0xbfff);
	    cart_type = OSS_SUPERCART;
	    rom_inserted = TRUE;
	    status = TRUE;
	  }

	close (fd);
    }

  return status;
}

/*
 * Load a DB Supercartridge from the specified file
 * The DB cartridge is a 32K bank switched cartridge
 * that occupies 16K of address space between $8000
 * and $bfff
 */

int Insert_DB_ROM (char *filename)
{
  int status = FALSE;
  int fd;

  fd = open (filename, O_RDONLY, 0777);
  if (fd != -1)
    {
      cart_image = (UBYTE*)malloc(0x8000);
      if (cart_image)
	{
	  read (fd, cart_image, 0x8000);
	  memcpy (&memory[0x8000], cart_image, 0x2000);
	  memcpy (&memory[0xa000], cart_image+0x6000, 0x2000);
	  SetROM (0x8000, 0xbfff);
	  cart_type = DB_SUPERCART;
	  rom_inserted = TRUE;
	  status = TRUE;
	}
      close (fd);
    }

  return status;
}

/*
 * Load a 32K 5200 ROM from the specified file
 */

int Insert_32K_5200ROM (char *filename)
{
  int status = FALSE;
  int fd;

  fd = open (filename, O_RDONLY, 0777);
  if (fd != -1)
    {
      read (fd, &memory[0x4000], 0x8000);
      close (fd);
      SetROM (0x4000, 0xbfff);
      cart_type = AGS32_CART;
      rom_inserted = TRUE;
      status = TRUE;
    }

  return status;
}

/*
 * This removes any loaded cartridge ROM files from the emulator
 * It doesn't remove either the OS, FFP or character set ROMS.
 */

int Remove_ROM (void)
{
  if (cart_image) /* Release memory allocated for Super Cartridges */
    {
      free (cart_image);
      cart_image = NULL;
    }

  SetRAM (0x8000, 0xbfff); /* Ensure cartridge area is RAM */
  cart_type = NO_CART;
  rom_inserted = FALSE;

  return TRUE;
}

void PatchOS (void)
{
  const unsigned short	o_open = 0;
  const unsigned short	o_close = 2;
  const unsigned short	o_read = 4;
  const unsigned short	o_write = 6;
  const unsigned short	o_status = 8;
  const unsigned short	o_special = 10;
  const unsigned short	o_init = 12;

  unsigned short addr;
  unsigned short entry;
  unsigned short devtab;
  int i;
/*
   =====================
   Disable Checksum Test
   =====================
*/
  if (sio_patch)
    add_esc (0xe459, ESC_SIOV);

  switch (machine)
    {
    case Atari :
      break;
    case AtariXL :
    case AtariXE :
      memory[0xc314] = 0x8e;
      memory[0xc315] = 0xff;
      memory[0xc319] = 0x8e;
      memory[0xc31a] = 0xff;
      break;
    }
/*
   ==========================================
   Patch O.S. - Modify Handler Table (HATABS)
   ==========================================
*/
  switch (machine)
    {
    case Atari :
      addr = 0xf0e3;
      break;
    case AtariXL :
    case AtariXE :
      addr = 0xc42e;
      break;
    }

  for (i=0;i<5;i++)
    {
      devtab = (memory[addr+2] << 8) | memory[addr+1];

      switch (memory[addr])
	{
	case 'P' :
	  entry = (memory[devtab+o_open+1] << 8) | memory[devtab+o_open];
	  add_esc (entry+1, ESC_PHOPEN);
	  entry = (memory[devtab+o_close+1] << 8) | memory[devtab+o_close];
	  add_esc (entry+1, ESC_PHCLOS);
/*
	  entry = (memory[devtab+o_read+1] << 8) | memory[devtab+o_read];
	  add_esc (entry+1, ESC_PHREAD);
*/
	  entry = (memory[devtab+o_write+1] << 8) | memory[devtab+o_write];
	  add_esc (entry+1, ESC_PHWRIT);
	  entry = (memory[devtab+o_status+1] << 8) | memory[devtab+o_status];
	  add_esc (entry+1, ESC_PHSTAT);
/*
	  entry = (memory[devtab+o_special+1] << 8) | memory[devtab+o_special];
	  add_esc (entry+1, ESC_PHSPEC);
*/
	  memory[devtab+o_init] = 0xd2;
	  memory[devtab+o_init+1] = ESC_PHINIT;
	  break;
	case 'C' :
	  memory[addr] = 'H';
	  entry = (memory[devtab+o_open+1] << 8) | memory[devtab+o_open];
	  add_esc (entry+1, ESC_HHOPEN);
	  entry = (memory[devtab+o_close+1] << 8) | memory[devtab+o_close];
	  add_esc (entry+1, ESC_HHCLOS);
	  entry = (memory[devtab+o_read+1] << 8) | memory[devtab+o_read];
	  add_esc (entry+1, ESC_HHREAD);
	  entry = (memory[devtab+o_write+1] << 8) | memory[devtab+o_write];
	  add_esc (entry+1, ESC_HHWRIT);
	  entry = (memory[devtab+o_status+1] << 8) | memory[devtab+o_status];
	  add_esc (entry+1, ESC_HHSTAT);
	  break;
	case 'E' :
#ifdef BASIC
	  printf ("Editor Device\n");
	  entry = (memory[devtab+o_open+1] << 8) | memory[devtab+o_open];
	  add_esc (entry+1, ESC_E_OPEN);
	  entry = (memory[devtab+o_read+1] << 8) | memory[devtab+o_read];
	  add_esc (entry+1, ESC_E_READ);
	  entry = (memory[devtab+o_write+1] << 8) | memory[devtab+o_write];
	  add_esc (entry+1, ESC_E_WRITE);
#endif
	  break;
	case 'S' :
	  break;
	case 'K' :
#ifdef BASIC
	  printf ("Keyboard Device\n");
	  entry = (memory[devtab+o_read+1] << 8) | memory[devtab+o_read];
	  add_esc (entry+1, ESC_K_READ);
#endif
	  break;
	default :
	  break;
	}

      addr += 3;	/* Next Device in HATABS */
    }

  if (ffp_patch)
    {
      add_esc (0xd800, ESC_AFP);
      add_esc (0xd8e6, ESC_FASC);
      add_esc (0xd9aa, ESC_IFP);
      add_esc (0xd9d2, ESC_FPI);
      add_esc (0xda66, ESC_FADD);
      add_esc (0xda60, ESC_FSUB);
      add_esc (0xdadb, ESC_FMUL);
      add_esc (0xdb28, ESC_FDIV);
      add_esc (0xdecd, ESC_LOG);
      add_esc (0xded1, ESC_LOG10);
      add_esc (0xddc0, ESC_EXP);
      add_esc (0xddcc, ESC_EXP10);
      add_esc (0xdd40, ESC_PLYEVL);
      add_esc (0xda44, ESC_ZFR0);
      add_esc (0xda46, ESC_ZF1);
      add_esc (0xdd89, ESC_FLD0R);
      add_esc (0xdd8d, ESC_FLD0P);
      add_esc (0xdd98, ESC_FLD1R);
      add_esc (0xdd9c, ESC_FLD1P);
      add_esc (0xdda7, ESC_FST0R);
      add_esc (0xddab, ESC_FST0P);
      add_esc (0xddb6, ESC_FMOVE);
    }
}

void Coldstart (void)
{
  NMIEN = 0x00;
  NMIST = 0x00;
  PORTA = 0xff;
  PORTB = 0xff;
  memory[0x244] = 1;
  CPU_Reset ();
}

void Warmstart (void)
{
  NMIEN = 0x00;
  NMIST = 0x00;
  PORTA = 0xff;
  PORTB = 0xff;
  CPU_Reset ();
}

int Initialise_AtariOSA (void)
{
  char filename[128];
  int status;

#ifdef VMS
  sprintf (filename, "%s:atariosa.rom", atari_library);
#else
  sprintf (filename, "%s/atariosa.rom", atari_library);
#endif

  status = load_image (filename, 0xd800, 0x2800);
  if (status)
    {
      machine = Atari;
      PatchOS ();
      SetRAM (0x0000, 0xbfff);
      if (enable_c000)
	SetRAM (0xc000, 0xcfff);
      else
	SetROM (0xc000, 0xcfff);
      SetROM (0xd800, 0xffff);
      SetHARDWARE (0xd000, 0xd7ff);
      Coldstart ();
    }

  return status;
}

int Initialise_AtariOSB (void)
{
  char filename[128];
  int status;

#ifdef VMS
  sprintf (filename, "%s:atariosb.rom", atari_library);
#else
  sprintf (filename, "%s/atariosb.rom", atari_library);
#endif

  status = load_image (filename, 0xd800, 0x2800);
  if (status)
    {
      machine = Atari;
      PatchOS ();
      SetRAM (0x0000, 0xbfff);
      if (enable_c000)
	SetRAM (0xc000, 0xcfff);
      else
	SetROM (0xc000, 0xcfff);
      SetROM (0xd800, 0xffff);
      SetHARDWARE (0xd000, 0xd7ff);
      Coldstart ();
    }

  return status;
}

int Initialise_AtariXL (void)
{
  char filename[128];
  int status;

#ifdef VMS
  sprintf (filename, "%s:atarixl.rom", atari_library);
#else
  sprintf (filename, "%s/atarixl.rom", atari_library);
#endif

  status = load_image (filename, 0xc000, 0x4000);
  if (status)
    {
      machine = AtariXL;
      PatchOS ();
      memcpy (atarixl_os, memory+0xc000, 0x4000);
      
#ifdef VMS
      sprintf (filename, "%s:ataribas.rom", atari_library);
#else
      sprintf (filename, "%s/ataribas.rom", atari_library);
#endif

      if (cart_type == NO_CART)
	{
	  status = Insert_8K_ROM (filename);
	  if (status)
	    {
	      memcpy (atari_basic, memory+0xa000, 0x2000);
	      SetRAM (0x0000, 0x9fff);
	      SetROM (0xc000, 0xffff);
	      SetHARDWARE (0xd000, 0xd7ff);
	      rom_inserted = FALSE;
	      Coldstart ();
	    }
	  else
	    {
	      printf ("Unable to load %s\n", filename);
	      Atari800_Exit (FALSE);
	      exit (1);
	    }
	}
      else
	{
	  SetRAM (0x0000, 0xbfff);
	  SetROM (0xc000, 0xffff);
	  SetHARDWARE (0xd000, 0xd7ff);
	  rom_inserted = FALSE;
	  Coldstart ();
	}
    }

  return status;
}

int Initialise_AtariXE (void)
{
  int status;

  status = Initialise_AtariXL ();
  machine = AtariXE;
  xe_bank = -1;

  return status;
}

int Initialise_Atari5200 (void)
{
  char filename[128];
  int status;

#ifdef VMS
  sprintf (filename, "%s:atari5200.rom", atari_library);
#else
  sprintf (filename, "%s/atari5200.rom", atari_library);
#endif

  status = load_image (filename, 0xf800, 0x800);
  if (status)
    {
      machine = Atari5200;
      SetRAM (0x0000, 0x3fff);
      SetROM (0xf800, 0xffff);
      SetROM (0x4000, 0xffff);
      SetHARDWARE (0xc000, 0xc0ff); /* 5200 GTIA Chip */
      SetHARDWARE (0xd400, 0xd4ff); /* 5200 ANTIC Chip */
      SetHARDWARE (0xe800, 0xe8ff); /* 5200 POKEY Chip */
      SetHARDWARE (0xeb00, 0xebff); /* 5200 POKEY Chip */
      Coldstart ();
    }

  return status;
}

main (int argc, char **argv)
{
  char filename[128];
  int status;
  int error;
  int diskno = 1;
  int i;
  int j;
  char *ptr;

  ptr = getenv ("ATARI_LIBRARY");
  if (ptr) atari_library = ptr;

  error = FALSE;

  for (i=j=1;i<argc;i++)
    {
      if (strcmp(argv[i],"-atari") == 0)
	machine = Atari;
      else if (strcmp(argv[i],"-xl") == 0)
	machine = AtariXL;
      else if (strcmp(argv[i],"-xe") == 0)
	machine = AtariXE;
      else if (strcmp(argv[i],"-5200") == 0)
	machine = Atari5200;
      else if (strcmp(argv[i],"-ffp") == 0)
	ffp_patch = TRUE;
      else if (strcmp(argv[i],"-nopatch") == 0)
	sio_patch = FALSE;
      else if (strcmp(argv[i],"-rom") == 0)
	{
	  rom_filename = argv[++i];
	  cart_type = NORMAL8_CART;
	}
      else if (strcmp(argv[i],"-rom16") == 0)
	{
	  rom_filename = argv[++i];
	  cart_type = NORMAL16_CART;
	}
      else if (strcmp(argv[i],"-ags32") == 0)
	{
	  rom_filename = argv[++i];
	  cart_type = AGS32_CART;
	}
      else if (strcmp(argv[i],"-oss") == 0)
	{
	  rom_filename = argv[++i];
	  cart_type = OSS_SUPERCART;
	}
      else if (strcmp(argv[i],"-db") == 0)	/* db 16/32 superduper cart */
	{
	  rom_filename = argv[++i];
	  cart_type = DB_SUPERCART;
	}
      else if (strcmp(argv[i],"-refresh") == 0)
	{
	  sscanf (argv[++i],"%d", &refresh_rate);
	  if (refresh_rate < 1)
	    refresh_rate = 1;
	}
      else if (strcmp(argv[i],"-help") == 0)
	{
	  printf ("\t-atari        Standard Atari 800 mode\n");
	  printf ("\t-xl           Atari XL mode\n");
	  printf ("\t-xe           Atari XE mode (Currently same as -xl)\n");
	  printf ("\t-5200         Atari 5200 Games System\n");
	  printf ("\t-ffp          Use Fast Floating Point routines\n");
	  printf ("\t-rom          Install standard 8K Cartridge\n");
	  printf ("\t-rom16        Install standard 16K Cartridge\n");
	  printf ("\t-oss %%s       Install OSS Super Cartridge\n");
	  printf ("\t-db %%s        Install DB's 16/32K Cartridge (not for normal use)\n");
	  printf ("\t-refresh %%d   Specify screen refresh rate\n");
	  printf ("\t-nopatch      Don't patch SIO routine in OS (Ongoing Development)\n");
	  printf ("\t-a            Use A OS\n");
	  printf ("\t-b            Use B OS\n");
	  printf ("\t-c            Enable RAM between 0xc000 and 0xd000\n");
	  printf ("\t-v            Show version/release number\n");
	  argv[j++] = argv[i];
	}
      else if (strcmp(argv[i],"-a") == 0)
	os = 1;
      else if (strcmp(argv[i],"-b") == 0)
	os = 2;
      else if (strcmp(argv[i],"-c") == 0)
	enable_c000 = TRUE;
      else if (strcmp(argv[i],"-v") == 0)
	{
	  printf ("%s\n", ATARI_TITLE);
	  exit (1);
	}
      else
	argv[j++] = argv[i];
    }

  argc = j;

  Device_Initialise (&argc, argv);

  Atari_Initialise (&argc, argv); /* Platform Specific Initialisation */

  if (!atari_screen)
    {
      atari_screen = (ULONG*)malloc((ATARI_HEIGHT+16) * ATARI_WIDTH);
      for (i=0;i<256;i++)
	colour_translation_table[i] = i;
    }

  /*
   * Initialise basic 64K memory to zero.
   */

  for (i=0;i<65536;i++)
    memory[i] = 0;

  /*
   * Initialise Custom Chips
   */

  GTIA_Initialise (&argc, argv);
  PIA_Initialise (&argc, argv);

  /*
   * Initialise Serial Port Interrupts
   */

  DELAYED_SERIN_IRQ = 0;
  DELAYED_SEROUT_IRQ = 0;
  DELAYED_XMTDONE_IRQ = 0;

  /*
   * Any parameters left on the command line must be disk images.
   */

  for (i=1;i<argc;i++)
    {
      if (!SIO_Mount (diskno++, argv[i]))
	{
	  printf ("Disk File %s not found\n", argv[i]);
	  error = TRUE;
	}
    }

  if (error)
    {
      printf ("Usage: %s [-rom filename] [-oss filename] [diskfile1...diskfile8]\n", argv[0]);
      printf ("\t-help         Extended Help\n");
      Atari800_Exit (FALSE);
      exit (1);
    }

  /*
   * Install CTRL-C Handler
   */

  signal (SIGINT, sigint_handler);

  /*
   * Configure Atari System
   */

  switch (machine)
    {
    case Atari :
      if (os == 1)
	status = Initialise_AtariOSA ();
      else
	status = Initialise_AtariOSB ();
      break;
    case AtariXL :
      status = Initialise_AtariXL ();
      break;
    case AtariXE :
      status = Initialise_AtariXE ();
      break;
    case Atari5200 :
      status = Initialise_Atari5200 ();
      break;
    default :
      printf ("Fatal Error in atari.c\n");
      Atari800_Exit (FALSE);
      exit (1);
    }

  if (!status)
    {
      printf ("Operating System not available\n");
      Atari800_Exit (FALSE);
      exit (1);
    }
/*
 * ================================
 * Install requested ROM cartridges
 * ================================
 */
  if (rom_filename)
    {
      switch (cart_type)
	{
	case OSS_SUPERCART :
	  status = Insert_OSS_ROM (rom_filename);
	  break;
	case DB_SUPERCART :
	  status = Insert_DB_ROM (rom_filename);
	  break;
	case NORMAL8_CART :
	  status = Insert_8K_ROM (rom_filename);
	  break;
	case NORMAL16_CART :
	  status = Insert_16K_ROM (rom_filename);
	  break;
	case AGS32_CART :
	  status = Insert_32K_5200ROM (rom_filename);
	  break;
	}

      if (status)
	{
	  rom_inserted = TRUE;
	}
      else
	{
	  rom_inserted = FALSE;
	}
    }
  else
    {
      rom_inserted = FALSE;
    }
/*
 * ======================================
 * Reset CPU and start hardware emulation
 * ======================================
 */
  Atari800_Hardware ();
}

void add_esc (UWORD address, UBYTE esc_code)
{
  memory[address++] = 0xff;	/* ESC */
  memory[address++] = esc_code;	/* ESC CODE */
  memory[address] = 0x60;	/* RTS */
}

/*
   ================================
   N = 0 : I/O Successful and Y = 1
   N = 1 : I/O Error and Y = error#
   ================================
*/

K_Device (UBYTE esc_code)
{
  char	ch;

  switch (esc_code)
    {
    case ESC_K_OPEN :
    case ESC_K_CLOSE :
      regY = 1;
      ClrN;
      break;
    case ESC_K_WRITE :
    case ESC_K_STATUS :
    case ESC_K_SPECIAL :
      regY = 146;
      SetN;
      break;
    case ESC_K_READ :
      ch = getchar();
      switch (ch)
	{
	case '\n' :
	  ch = 0x9b;
	  break;
	default :
	  break;
	}
      regA = ch;
      regY = 1;
      ClrN;
      break;
    }
}

E_Device (UBYTE esc_code)
{
  UBYTE	ch;

  switch (esc_code)
    {
    case ESC_E_OPEN :
      printf ("Editor Open\n");
      regY = 1;
      ClrN;
      break;
    case ESC_E_READ :
      ch = getchar();
      switch (ch)
	{
	case '\n' :
	  ch = 0x9b;
	  break;
	default :
	  break;
	}
      regA = ch;
      regY = 1;
      ClrN;
      break;
    case ESC_E_WRITE :
      ch = regA;
      switch (ch)
	{
	case 0x7d :
	  putchar ('*');
	  break;
	case 0x9b :
	  putchar ('\n');
	  break;
	default :
	  if ((ch >= 0x20) && (ch <= 0x7e)) /* for DJGPP */
	    putchar (ch & 0x7f);
	  break;
	}
      regY = 1;
      ClrN;
      break;
    }
}

DSKIN ()
{
  SIO ();
}

#define CDTMV1 0x0218

void Escape (UBYTE esc_code)
{
  int addr;

  switch (esc_code)
    {
    case ESC_SIOV :
      SIO ();
      break;
    case ESC_DSKINV :
      DSKIN ();
      break;
    case ESC_SETVBV :
      printf ("SETVBV %d\n", regA);
      addr = (CDTMV1 - 2) + (regA * 2);
      memory[addr] = regY;
      memory[addr+1] = regX;
      break;
    case ESC_AFP :
      ffp_afp() ;
      break;
    case ESC_FASC :
      ffp_fasc();
      break;
    case ESC_IFP :
      ffp_ifp();
      break;
    case ESC_FPI :
      ffp_fpi();
      break;
    case ESC_FADD :
      ffp_fadd();
      break;
    case ESC_FSUB :
      ffp_fsub();
      break;
    case ESC_FMUL :
      ffp_fmul();
      break;
    case ESC_FDIV :
      ffp_fdiv();
      break;
    case ESC_LOG :
      ffp_log();
      break;
    case ESC_LOG10 :
      ffp_log10();
      break;
    case ESC_EXP :
      ffp_exp();
      break;
    case ESC_EXP10 :
      ffp_exp10();
      break;
    case ESC_PLYEVL :
      ffp_plyevl();
      break;
    case ESC_ZFR0 :
      ffp_zfr0();
      break;
    case ESC_ZF1 :
      ffp_zf1();
      break;
    case ESC_FLD0R :
      ffp_fld0r();
      break;
    case ESC_FLD0P :
      ffp_fld0p();
      break;
    case ESC_FLD1R :
      ffp_fld1r();
      break;
    case ESC_FLD1P :
      ffp_fld1p();
      break;
    case ESC_FST0R :
      ffp_fst0r();
      break;
    case ESC_FST0P :
      ffp_fst0p();
      break;
    case ESC_FMOVE :
      ffp_fmove();
      break;
    case ESC_K_OPEN :
    case ESC_K_CLOSE :
    case ESC_K_READ :
    case ESC_K_WRITE :
    case ESC_K_STATUS :
    case ESC_K_SPECIAL :
      K_Device (esc_code);
      break;
    case ESC_E_OPEN :
    case ESC_E_READ :
    case ESC_E_WRITE :
      E_Device (esc_code);
      break;
    case ESC_KHOPEN :
      Device_KHOPEN ();
      break;
    case ESC_KHCLOS :
      Device_KHCLOS ();
      break;
    case ESC_KHREAD :
      Device_KHREAD ();
      break;
    case ESC_KHWRIT :
      Device_KHWRIT ();
      break;
    case ESC_KHSTAT :
      Device_KHSTAT ();
      break;
    case ESC_KHSPEC :
      Device_KHSPEC ();
      break;
    case ESC_KHINIT :
      Device_KHINIT ();
      break;
    case ESC_SHOPEN :
      Device_SHOPEN ();
      break;
    case ESC_SHCLOS :
      Device_SHCLOS ();
      break;
    case ESC_SHREAD :
      Device_SHREAD ();
      break;
    case ESC_SHWRIT :
      Device_SHWRIT ();
      break;
    case ESC_SHSTAT :
      Device_SHSTAT ();
      break;
    case ESC_SHSPEC :
      Device_SHSPEC ();
      break;
    case ESC_SHINIT :
      Device_SHINIT ();
      break;
    case ESC_EHOPEN :
      Device_EHOPEN ();
      break;
    case ESC_EHCLOS :
      Device_EHCLOS ();
      break;
    case ESC_EHREAD :
      Device_EHREAD ();
      break;
    case ESC_EHWRIT :
      Device_EHWRIT ();
      break;
    case ESC_EHSTAT :
      Device_EHSTAT ();
      break;
    case ESC_EHSPEC :
      Device_EHSPEC ();
      break;
    case ESC_EHINIT :
      Device_EHINIT ();
      break;
    case ESC_PHOPEN :
      Device_PHOPEN ();
      break;
    case ESC_PHCLOS :
      Device_PHCLOS ();
      break;
    case ESC_PHREAD :
      Device_PHREAD ();
      break;
    case ESC_PHWRIT :
      Device_PHWRIT ();
      break;
    case ESC_PHSTAT :
      Device_PHSTAT ();
      break;
    case ESC_PHSPEC :
      Device_PHSPEC ();
      break;
    case ESC_PHINIT :
      Device_PHINIT ();
      break;
    case ESC_HHOPEN :
      Device_HHOPEN ();
      break;
    case ESC_HHCLOS :
      Device_HHCLOS ();
      break;
    case ESC_HHREAD :
      Device_HHREAD ();
      break;
    case ESC_HHWRIT :
      Device_HHWRIT ();
      break;
    case ESC_HHSTAT :
      Device_HHSTAT ();
      break;
    case ESC_HHSPEC :
      Device_HHSPEC ();
      break;
    case ESC_HHINIT :
      Device_HHINIT ();
      break;
    default         :
      Atari800_Exit (FALSE);
      printf ("Invalid ESC Code %x at Address %x\n",
	      esc_code, regPC - 2);
      monitor();
      exit (1);
    }
}

int Atari800_Exit (int run_monitor)
{
  return Atari_Exit (run_monitor);
}

#ifdef DEBUG
UBYTE Atari800_GetByte (UWORD addr, UWORD PC)
#else
UBYTE Atari800_GetByte (UWORD addr)
#endif
{
  UBYTE	byte;
/*
	============================================================
	GTIA, POKEY, PIA and ANTIC do not fully decode their address
	------------------------------------------------------------
	PIA (At least) is fully decoded when emulating the XL/XE
	============================================================
*/
  switch (addr & 0xff00)
    {
    case 0xd000: /* GTIA */
      byte = GTIA_GetByte (addr - 0xd000);
      break;
    case 0xd200: /* POKEY */
      byte = POKEY_GetByte (addr - 0xd200);
      break;
    case 0xd300: /* PIA */
      byte = PIA_GetByte (addr - 0xd300);
      break;
    case 0xd400: /* ANTIC */
      byte = ANTIC_GetByte (addr - 0xd400);
      break;
    case 0xc000: /* GTIA - 5200 */
      byte = GTIA_GetByte (addr - 0xc000);
      break;
    case 0xe800 : /* POKEY - 5200 */
      byte = POKEY_GetByte (addr - 0xe800);
      break;
    case 0xeb00 : /* POKEY - 5200 */
      byte = POKEY_GetByte (addr - 0xeb00);
      break;
    default:
      break;
    }

  return byte;
}

#ifdef DEBUG
int Atari800_PutByte (UWORD addr, UBYTE byte, UWORD PC)
#else
int Atari800_PutByte (UWORD addr, UBYTE byte)
#endif
{
  int abort = FALSE;
/*
	============================================================
	GTIA, POKEY, PIA and ANTIC do not fully decode their address
	------------------------------------------------------------
	PIA (At least) is fully decoded when emulating the XL/XE
	============================================================
*/
  switch (addr & 0xff00)
    {
    case 0xd000: /* GTIA */
      abort = GTIA_PutByte (addr - 0xd000, byte);
      break;
    case 0xd200: /* POKEY */
      abort = POKEY_PutByte (addr - 0xd200, byte);
      break;
    case 0xd300: /* PIA */
      abort = PIA_PutByte (addr - 0xd300, byte);
      break;
    case 0xd400: /* ANTIC */
      abort = ANTIC_PutByte (addr - 0xd400, byte);
      break;
    case 0xd500: /* Super Cartridges */
      abort = SuperCart_PutByte (addr, byte);
      break;
    case 0xc000: /* GTIA - 5200 */
      abort = GTIA_PutByte (addr - 0xc000, byte);
      break;
    case 0xeb00: /* POKEY - 5200 */
      abort = POKEY_PutByte (addr - 0xeb00, byte);
      break;
    default:
      break;
    }

  return abort;
}

void Atari800_Hardware (void)
{
  static int	pil_on = FALSE;

  while (TRUE)
    {
      static struct timeval tp;
      static struct timezone tzp;
      static double lasttime = -1.0;
      static int test_val = 0;

      int keycode;

#ifndef BASIC
/*
      colour_lookup[8] = colour_translation_table[COLBK];
*/

      keycode = Atari_Keyboard ();

      switch (keycode)
	{
	case AKEY_COLDSTART :
	  Coldstart ();
	  break;
	case AKEY_WARMSTART :
	  Warmstart ();
	  break;
	case AKEY_EXIT :
	  Atari800_Exit (FALSE);
	  exit (1);
	case AKEY_BREAK :
	  IRQST &= 0x7f;
	  IRQ = 1;
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
	  break;
	case AKEY_DISKCHANGE :
	  {
	    char filename[128];
	    int driveno;

	    printf ("Next Disk: ");
	    scanf ("\n%s", filename);
	    printf ("Drive Number: ");
	    scanf ("\n%d", &driveno);

	    SIO_Dismount (driveno);
	    if (!SIO_Mount (driveno,filename))
	      {
		printf ("Failed to mount %s on D%d\n",
			filename, driveno);
	      }
	  }
	case AKEY_NONE :
	  break;
	default :
	  KBCODE = keycode;
	  IRQST &= 0xbf;
	  IRQ = 1;
	  break;
	}
#endif

      /*
       * Generate Screen
       */

#ifndef BASIC
      if (++test_val == refresh_rate)
	{
	  ANTIC_RunDisplayList ();
	  Atari_DisplayScreen ((UBYTE*)atari_screen);
	  test_val = 0;
	}
      else
	{
	  for (ypos=0;ypos<ATARI_HEIGHT;ypos++)
	    {
	      GO (114);
	    }
	}
#else
      for (ypos=0;ypos<ATARI_HEIGHT;ypos++)
	{
	  GO (114);
	}
#endif

      if (deltatime > 0.0)
	{
	  if (lasttime >= 0.0)
	    {
	      double curtime;

	      do
		{
		  gettimeofday (&tp, &tzp);
		  curtime = tp.tv_sec + (tp.tv_usec / 1000000.0);
		} while (curtime < (lasttime + deltatime));
	  
	      lasttime = curtime;
	    }
	  else
	    {
	      gettimeofday (&tp, &tzp);
	      lasttime = tp.tv_sec + (tp.tv_usec / 1000000.0);
	    }
	}
    }
}

