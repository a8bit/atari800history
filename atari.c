#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<fcntl.h>
#include	<ctype.h>
#include	<signal.h>

#ifdef VMS
#include	<file.h>
#endif

#define FALSE   0
#define TRUE    1

#include	"system.h"
#include	"cpu.h"
#include	"atari.h"
#include	"atari_h_device.h"

static int	os = 2;
static int	enable_c000 = FALSE;

extern int	rom_inserted;
extern UBYTE	atari_basic[8192];
extern UBYTE	atarixl_os[16384];

UBYTE	*cart_image;	/* For cartridge memory */
int	cart_type;

static char *atari_library = ATARI_LIBRARY;

void Atari800_OS ();
void Atari800_ESC (UBYTE code);

int load_cart (char *filename, int type);

static char *rom_filename = NULL;
static int sio_patch = TRUE;

void sigint_handler ()
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

atari_main (int argc, char **argv)
{
  int error;
  int diskno = 1;
  int i;
  char *ptr;

  ptr = getenv ("ATARI_LIBRARY");
  if (ptr) atari_library = ptr;

  Atari800_Initialise (&argc, argv);

  signal (SIGINT, sigint_handler);

  error = FALSE;

  for (i=1;i<argc;i++)
    {
      if (strcmp(argv[i],"-rom") == 0)
	{
	  rom_filename = argv[++i];
	  cart_type = NORMAL8_CART;
	}
      else if (strcmp(argv[i],"-rom16") == 0)
	{
	  rom_filename = argv[++i];
	  cart_type = NORMAL16_CART;
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
      else if (strcmp(argv[i],"-nopatch") == 0)
	{
	  sio_patch = FALSE;
	}
      else if (*argv[i] == '-')
	{
	  switch (*(argv[i]+1))
	    {
	    case 'a' :
	      os = 1;
	      break;
	    case 'b' :
	      os = 2;
	      break;
	    case 'c' :
	      enable_c000 = TRUE;
	      break;
	    case 'h' :
	      h_prefix = (char*)(argv[i]+2);
	      break;
	    case 'v' :
	      printf ("%s\n", ATARI_TITLE);
	      Atari800_Exit (FALSE);
	      exit (1);
	    default :
	      error = TRUE;
	      break;
	    }
	}
      else
	{
	  if (!SIO_Mount (diskno++, argv[i]))
	    {
	      printf ("Disk File %s not found\n", argv[i]);
	    }
	}
    }

  if (error)
    {
      printf ("Usage: %s [-rom filename] [-oss filename] [diskfile1...diskfile8]\n", argv[0]);
      printf ("\t-rom filename\tLoad Specified 8K ROM\n");
      printf ("\t-oss filename\tLoad Specified OSS Super Cartridge\n");
      printf ("\t-c\t\tEnable RAM from 0xc000 upto 0xcfff\n");
      printf ("\t-hdirectory/\tSpecifies directory to use for H:\n");
      Atari800_Exit (FALSE);
      exit (1);
    }

  Atari800_OS ();
}

void Atari800_OS ()
{
  const unsigned short	o_open = 0;
  const unsigned short	o_close = 2;
  const unsigned short	o_read = 4;
  const unsigned short	o_write = 6;
  const unsigned short	o_status = 8;
  const unsigned short	o_special = 10;
  const unsigned short	o_init = 12;

  unsigned short	addr;
  unsigned short	entry;
  unsigned short	devtab;

  char filename[128];

  int	status;
  int	i;
/*
   ======================================================
   Load Floating Point Package, Font and Operating System
   ======================================================
*/
  switch (machine)
    {
    case Atari :
      if (os == 1)
	{
	  sprintf (filename, "%s/atariosa.rom", atari_library);
	  status = load_image (filename, 0xd800, 0x2800);
	}
      else
	{
	  sprintf (filename, "%s/atariosb.rom", atari_library);
	  status = load_image (filename, 0xd800, 0x2800);
	}
      if (!status)
	{
	  printf ("Unable to load %s\n", filename);
	  Atari800_Exit (FALSE);
	  exit (1);
	}
      break;
    case AtariXL :
    case AtariXE :
      sprintf (filename, "%s/atarixl.rom", atari_library);
      status = load_image (filename, 0xc000, 0x4000);
      if (!status)
	{
	  printf ("Unable to load %s\n", filename);
	  Atari800_Exit (FALSE);
	  exit (1);
	}
      memcpy (atarixl_os, memory+0xc000, 0x4000);
      
      sprintf (filename, "%s/ataribas.rom", atari_library);
      status = load_image (filename, 0xa000, 0x2000);
      if (!status)
	{
	  printf ("Unable to load %s\n", filename);
	  Atari800_Exit (FALSE);
	  exit (1);
	}
      memcpy (atari_basic, memory+0xa000, 0x2000);
      break;
    default :
      printf ("Fatal Error in atari.c\n");
      Atari800_Exit (FALSE);
      exit (1);
    }

  if (sio_patch)
    add_esc (0xe459, ESC_SIO);
/*
   =====================
   Disable Checksum Test
   =====================
*/
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
      devtab = (GetByte(addr+2) << 8) | GetByte(addr+1);

      switch (GetByte(addr))
	{
	case 'P' :
	  break;
	case 'C' :
	  PutByte (addr, 'H');
	  entry = GetWord (devtab + o_open);
	  add_esc (entry+1, ESC_H_OPEN);
	  entry = GetWord (devtab + o_close);
	  add_esc (entry+1, ESC_H_CLOSE);
	  entry = GetWord (devtab + o_read);
	  add_esc (entry+1, ESC_H_READ);
	  entry = GetWord (devtab + o_write);
	  add_esc (entry+1, ESC_H_WRITE);
	  entry = GetWord (devtab + o_status);
	  add_esc (entry+1, ESC_H_STATUS);
/*
   ======================================
   Cannot change the special offset since
   it only points to an RTS instruction.
   ======================================
*/
/*
   entry = GetWord (devtab + o_special);
   add_esc (entry+1, ESC_H_SPECIAL);
*/
	  break;
	case 'E' :
#ifdef BASIC
	  printf ("Editor Device\n");
	  entry = GetWord (devtab + o_open);	/* Get Address of Editor Open Routine */
	  add_esc (entry+1, ESC_E_OPEN);		/* Replace Editor Open */
	  entry = GetWord (devtab + o_read);	/* Get Address of Editor Read Routine */
	  add_esc (entry+1, ESC_E_READ);	/* Replace Editor Read */
	  entry = GetWord (devtab + o_write);	/* Get Address of Editor Write Routine */
	  add_esc (entry+1, ESC_E_WRITE);	/* Replace Editor Write */
#endif
	  break;
	case 'S' :
	  break;
	case 'K' :
#ifdef BASIC
	  printf ("Keyboard Device\n");
/*
   entry = GetWord (devtab + o_open);
   add_esc (entry+1, ESC_K_OPEN);
   entry = GetWord (devtab + o_close);
   add_esc (entry+1, ESC_K_CLOSE);
*/
	  entry = GetWord (devtab + o_read);
	  add_esc (entry+1, ESC_K_READ);
/*
   entry = GetWord (devtab + o_write);
   add_esc (entry+1, ESC_K_WRITE);
   entry = GetWord (devtab + o_status);
   add_esc (entry+1, ESC_K_STATUS);
*/
/*
   ======================================
   Cannot change the special offset since
   it only points to an RTS instruction.
   ======================================
*/
/*
   entry = GetWord (devtab + o_special);
   add_esc (entry+1, ESC_K_SPECIAL);
*/
#endif
	  break;
	default :
	  break;
	}

      addr += 3;	/* Next Device in HATABS */
    }
/*
   ======================
   Set O.S. Area into ROM
   ======================
*/
  switch (machine)
    {
    case Atari :
      SetROM (0xd800, 0xffff);
      SetHARDWARE (0xd000, 0xd7ff);
      if (enable_c000)
	SetRAM (0xc000, 0xcfff);
      else
	SetROM (0xc000, 0xcfff);
      break;
    case AtariXL :
    case AtariXE :
      SetROM (0xc000, 0xffff);
      SetHARDWARE (0xd000, 0xd7ff);
      break;
    }

  if (rom_filename)
    {
      status = load_cart(rom_filename, cart_type);
      if (!status)
	{
	  printf ("Unable to load %s\n", rom_filename);
	  Atari800_Exit (FALSE);
	  exit (1);
	}

      switch (cart_type)
	{
	case OSS_SUPERCART :
	  memcpy (memory+0xa000,cart_image,0x1000);
	  memcpy (memory+0xb000,cart_image+0x3000,0x1000);
	  SetROM (0xa000, 0xbfff);
	  break;
	case DB_SUPERCART :
	  memcpy (memory+0x8000,cart_image,0x2000);
	  memcpy (memory+0xa000,cart_image+0x6000,0x2000);
	  SetROM (0x8000, 0xbfff);
	  break;
	case NORMAL8_CART :
	  memcpy (memory+0xa000,cart_image,0x2000);
	  SetROM (0xa000, 0xbfff);
	  free (cart_image);
	  cart_image = NULL;
	  break;
	case NORMAL16_CART :
	  memcpy (memory+0x8000,cart_image,0x4000);
	  SetROM (0x8000, 0xbfff);
	  free (cart_image);
	  cart_image = NULL;
	  break;
	}

      rom_inserted = TRUE;
    }
  else
    {
      rom_inserted = FALSE;
    }

  Escape = Atari800_ESC;

  CPU_Reset ();

  Atari800_Hardware ();
}

void add_esc (UWORD address, UBYTE esc_code)
{
  PutByte (address, 0xff);	/* ESC */
  PutByte (address+1, esc_code);	/* ESC CODE */
  PutByte (address+2, 0x60);	/* RTS */
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

int load_cart (char *filename, int type)
{
  int fd;
  int len;
  int status;

  switch (type)
    {
    case OSS_SUPERCART :
      len = 0x4000;
      break;
    case DB_SUPERCART :
      len = 0x8000;
      break;
    case NORMAL8_CART :
      len = 0x4000;
      break;
    case NORMAL16_CART :
      len = 0x8000;
      break;
    }

  cart_image = (UBYTE*)malloc(len);
  if (!cart_image)
    {
      perror ("malloc");
      Atari800_Exit (FALSE);
      exit (1);
    }

  fd = open (filename, O_RDONLY, 0777);
  if (fd == -1)
    {
      perror (filename);
      Atari800_Exit (FALSE);
      exit (1);
    }

  read (fd, cart_image, len);

  close (fd);

  status = TRUE;

  return status;
}

/*
   ================================
   N = 0 : I/O Successful and Y = 1
   N = 1 : I/O Error and Y = error#
   ================================
*/

static CPU_Status	cpu_status;

K_Device (UBYTE esc_code)
{
  char	ch;

  CPU_GetStatus (&cpu_status);

  switch (esc_code)
    {
    case ESC_K_OPEN :
    case ESC_K_CLOSE :
      cpu_status.Y = 1;
      cpu_status.flag.N = 0;
      break;
    case ESC_K_WRITE :
    case ESC_K_STATUS :
    case ESC_K_SPECIAL :
      cpu_status.Y = 146;
      cpu_status.flag.N = 1;
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
      cpu_status.A = ch;
      cpu_status.Y = 1;
      cpu_status.flag.N = 0;
      break;
    }

  CPU_PutStatus (&cpu_status);
}

E_Device (UBYTE esc_code)
{
  UBYTE	ch;

  CPU_GetStatus (&cpu_status);

  switch (esc_code)
    {
    case ESC_E_OPEN :
      printf ("Editor Open\n");
      cpu_status.Y = 1;
      cpu_status.flag.N = 0;
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
      cpu_status.A = ch;
      cpu_status.Y = 1;
      cpu_status.flag.N = 0;
      break;
    case ESC_E_WRITE :
      ch = cpu_status.A;
      switch (ch)
	{
	case 0x7d :
	  putchar ('*');
	  break;
	case 0x9b :
	  putchar ('\n');
	  break;
	default :
	  putchar (ch & 0x7f);
	  break;
	}
      cpu_status.Y = 1;
      cpu_status.flag.N = 0;
      break;
    }

  CPU_PutStatus (&cpu_status);
}

void Atari800_ESC (UBYTE esc_code)
{
  switch (esc_code)
    {
    case ESC_SIO :
      SIO ();
      break;
    case ESC_K_OPEN :
    case ESC_K_CLOSE :
    case ESC_K_READ :
    case ESC_K_WRITE :
    case ESC_K_STATUS :
    case ESC_K_SPECIAL :
      K_Device (esc_code);
      break;
    case ESC_H_OPEN :
    case ESC_H_CLOSE :
    case ESC_H_READ :
    case ESC_H_WRITE :
    case ESC_H_STATUS :
    case ESC_H_SPECIAL :
      H_Device (esc_code);
      break;
    case ESC_E_OPEN :
    case ESC_E_READ :
    case ESC_E_WRITE :
      E_Device (esc_code);
      break;
    default         :
      Atari800_Exit (FALSE);
      printf ("Invalid ESC Code %x at Address %x\n",
	      esc_code, cpu_status.PC);
      monitor();
      exit (1);
    }
}
