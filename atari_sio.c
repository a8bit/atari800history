/*
   *
   * All Input is assumed to be going to RAM
   * All Output is assumed to be coming from either RAM or ROM
   *
*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<fcntl.h>

#ifndef AMIGA
#include	<unistd.h>
#endif

#ifdef VMS
#include	<file.h>
#endif

#define FALSE   0
#define TRUE    1

#include	"system.h"
#include	"cpu.h"
#include	"atari.h"

#define	MAX_DRIVES	8

#define	MAGIC1	0x96
#define	MAGIC2	0x02

struct ATR_Header
{
  unsigned char	magic1;
  unsigned char	magic2;
  unsigned char	seccountlo;
  unsigned char	seccounthi;
  unsigned char	secsizelo;
  unsigned char	secsizehi;
  unsigned char	hiseccountlo;
  unsigned char	hiseccounthi;
  unsigned char	gash[8];
};

typedef enum Format { XFD, ATR } Format;

static Format	format[MAX_DRIVES];
static int	disk[MAX_DRIVES] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static int	sectorcount[MAX_DRIVES];
static int	sectorsize[MAX_DRIVES];

int SIO_Mount (int diskno, char *filename)
{
  struct ATR_Header	header;

  int	fd;

  fd = open (filename, O_RDWR, 0777);
  if (fd)
    {
      int	status;

      status = read (fd, &header, sizeof(struct ATR_Header));
      if (status == -1)
	{
	  perror ("SIO_Mount");
	  Atari800_Exit (FALSE);
	  exit (1);
	}

      if ((header.magic1 == MAGIC1) && (header.magic2 == MAGIC2))
	{
	  sectorcount[diskno-1] = header.hiseccounthi << 24 |
	    header.hiseccountlo << 16 |
	      header.seccounthi << 8 |
		header.seccountlo;

	  sectorsize[diskno-1] = header.secsizehi << 8 |
	    header.secsizelo;

	  printf ("ATR: sectorcount = %d, sectorsize = %d\n",
		  sectorcount[diskno-1],
		  sectorsize[diskno-1]);

	  format[diskno-1] = ATR;
	}
      else
	{
	  format[diskno-1] = XFD;
	}
    }

  disk[diskno-1] = fd;

  return (disk[diskno-1] != -1) ? TRUE : FALSE;
}

int SIO_Dismount (int diskno)
{
  if (disk[diskno-1] != -1)
    {
      close (disk[diskno-1]);
      disk[diskno-1] = -1;
    }
}

SIO ()
{
  CPU_Status	cpu_status;

  UBYTE DDEVIC = GetByte (0x0300);
  UBYTE DUNIT = GetByte (0x0301);
  UBYTE DCOMND = GetByte (0x0302);
  UBYTE DSTATS = GetByte(0x0303);
  UBYTE DBUFLO = GetByte(0x0304);
  UBYTE DBUFHI = GetByte(0x0305);
  UBYTE DTIMLO = GetByte(0x0306);
  UBYTE DBYTLO = GetByte(0x0308);
  UBYTE DBYTHI = GetByte(0x0309);
  UBYTE DAUX1 = GetByte(0x030a);
  UBYTE DAUX2 = GetByte(0x030b);

  int	sector;
  int	buffer;
  int	count;
  int	i;

  CPU_GetStatus (&cpu_status);

  if (disk[DUNIT-1] != -1)
    {
      int	offset;

      sector = DAUX1 + DAUX2 * 256;
      buffer = DBUFLO + DBUFHI * 256;
      count = DBYTLO + DBYTHI * 256;

      switch (format[DUNIT-1])
	{
	case XFD :
	  offset = (sector-1)*128+0;
	  break;
	case ATR :
	  if (sector < 4)
	    offset = (sector-1) * 128 + 16;
	  else
	    offset = (sector-1) * sectorsize[DUNIT-1] + 16;
	  break;
	default :
	  printf ("Fatal Error in atari_sio.c\n");
	  Atari800_Exit (FALSE);
	  exit (1);
	}

      lseek (disk[DUNIT-1], offset, SEEK_SET);

#ifdef DEBUG
      printf ("SIO: DCOMND = %x, SECTOR = %d, BUFADR = %x, BUFLEN = %d\n",
	      DCOMND, sector, buffer, count);
#endif

      switch (DCOMND)
	{
	case 0x50 :
	case 0x57 :
	  write (disk[DUNIT-1], &memory[buffer], count);
	  cpu_status.Y = 1;
	  cpu_status.flag.N = 0;
	  break;
	case 0x52 :
	  read (disk[DUNIT-1], &memory[buffer], count);
	  cpu_status.Y = 1;
	  cpu_status.flag.N = 0;
	  break;
	case 0x21 :	/* Single Density Format */
	  cpu_status.Y = 1;
	  cpu_status.flag.N = 0;
	  break;
/*
   Status Request from Atari 400/800 Technical Reference Notes

   DVSTAT + 0	Command Status
   DVSTAT + 1	Hardware Status
   DVSTAT + 2	Timeout
   DVSTAT + 3	Unused

   Command Status Bits

   Bit 0 = 1 indicates an invalid command frame was received
   Bit 1 = 1 indicates an invalid data frame was received
   Bit 2 = 1 indicates that a PUT operation was unsuccessful
   Bit 3 = 1 indicates that the diskete is write protected
   Bit 4 = 1 indicates active/standby
   
   plus

   Bit 5 = 1 indicates double density
   Bit 7 = 1 indicates duel density disk (1050 format)
*/
	case 0x53 :	/* Get Status */
	  for (i=0;i<count;i++)
	    {
	      if (sectorsize[DUNIT-1] == 256)
		PutByte (buffer+i, 32 + 16)
	      else
		PutByte (buffer+i, 16)
	    }
	  cpu_status.Y = 1;
	  cpu_status.flag.N = 0;
	  break;
	default :
	  printf ("SIO: DCOMND = %0x\n", DCOMND);
	  cpu_status.Y = 146;
	  cpu_status.flag.N = 1;
	  break;
	}
    }
  else
    {
      cpu_status.Y = 146;
      cpu_status.flag.N = 1;
    }

  CPU_PutStatus (&cpu_status);

  PutByte(0x0303, cpu_status.Y);
}

static unsigned char cmd_frame[5];
static int ncmd = 0;
static int checksum = 0;

static unsigned char data[128];
static int offst;

static int buffer_offset;
static int buffer_size;

void Command_Frame (void)
{
  switch (cmd_frame[1])
    {
    case 'R' : /* Read */
      printf ("Read command\n");
      break;
    case 'W' : /* Write with verify */
      printf ("Write command\n");
      break;
    case 'S' : /* Status */
      printf ("Status command\n");
      data[0] = 0x41; /* ACK */
      data[1] = 0x10;
      data[2] = 0x10;
      data[3] = 0x10;
      data[4] = 0x10;
      data[5] = 0x40; /* Checksum */
      data[6] = 0x43; /* COMPLETE */
      buffer_offset = 0;
      buffer_size = 7;
      break;
    case 'P' : /* Put without verify */
      printf ("Put command\n");
      break;
    case '!' : /* Format */
      printf ("Format command\n");
      break;
    case 'T' : /* Read Address */
      printf ("Read Address command\n");
      break;
    case 'Q' : /* Read Spin */
      printf ("Read Spin command\n");
      break;
    case 'U' : /* Motor On */
      printf ("Motor On command\n");
      break;
    case 'V' : /* Verify Sector */
      printf ("Verify Sector\n");
      break;
    default :
      printf ("Unknown command\n");
      break;
  }
}

void SIO_SEROUT (unsigned char byte, int cmd)
{
  checksum += (unsigned char)byte;
  while (checksum > 255)
    checksum = checksum - 255;

#ifdef DEBUG
  printf ("SIO_SEROUT: byte = %x, checksum = %x, cmd = %d\n",
	  byte, checksum, cmd);
#endif

  if (cmd)
    {
      cmd_frame[ncmd++] = byte;
      if (ncmd == 5)
	{
	  int sector;

	  Command_Frame ();
/*
	  sector = cmd_frame[2] + cmd_frame[3] * 256;
	  printf ("Sector: %d(%x)\n", sector, sector);
	  lseek (disk[0], (sector-1)*128, SEEK_SET);
	  read (disk[0], data, 128);
*/
	  offst = 0;
	  checksum = 0;
/*
	  IRQST &= 0xdf;
*/
/*
	  IRQST &= 0xc7;
*/
	  IRQST &= ~(IRQEN & (0x10 | 0x08));
	  INTERRUPT |= IRQ_MASK;
	  ncmd = 0;
	}
      else
	{
/*
	  IRQST &= 0xe7;
*/
	  IRQST &= ~(IRQEN & (0x10 | 0x08));
	  INTERRUPT |= IRQ_MASK;
	}

      if (cmd_frame[0] == 0)
	ncmd = 0;
    }
  else
    {
      ncmd = 0;
/*
      IRQST &= 0xe7;
*/
      IRQST &= ~(IRQEN & (0x10 | 0x80));
      INTERRUPT |= IRQ_MASK;
    }
}

int SIO_SERIN ()
{
  int byte;

  if (buffer_offset < buffer_size)
    {
      byte = (int)data[buffer_offset++];

#ifdef DEBUG
      printf ("SERIN: byte = %x\n", byte);
#endif

      if (buffer_offset < buffer_size)
	{
/*
	  IRQST &= 0xdf;
*/
	  IRQST &= ~(IRQEN & 0x20);
	  INTERRUPT |= IRQ_MASK;
	  printf ("Setting SERIN Interrupt again\n");
	}
    }

  return byte;
}


