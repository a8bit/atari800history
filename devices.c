#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>

#ifdef VMS
#include	<unixio.h>
#include	<file.h>
#else
#include	<fcntl.h>
#ifndef	AMIGA
#include	<unistd.h>
#endif
#endif

#define	DO_DIR

#ifdef AMIGA
#undef	DO_DIR
#endif

#ifdef VMS
#undef	DO_DIR
#endif

#ifdef DO_DIR
#include	<dirent.h>
#endif

static char *rcsid = "$Id: devices.c,v 1.13 1996/07/18 00:33:05 david Exp $";

#define FALSE   0
#define TRUE    1

#ifdef AMIGA
#define PRINT_COMMAND "COPY %s PAR:"
#else
#ifdef VMS
#define PRINT_COMMAND "$ PRINT/DELETE %s"
#else
#include "config.h"
#endif
#endif
#include "atari.h"
#include "cpu.h"

#define	ICHIDZ	0x0020
#define	ICDNOZ	0x0021
#define	ICCOMZ	0x0022
#define	ICSTAZ	0x0023
#define	ICBALZ	0x0024
#define	ICBAHZ	0x0025
#define	ICPTLZ	0x0026
#define	ICPTHZ	0x0027
#define	ICBLLZ	0x0028
#define	ICBLHZ	0x0029
#define	ICAX1Z	0x002a
#define	ICAX2Z	0x002b

static char *H[5] =
{
  ".",
  ATARI_H1_DIR,
  ATARI_H2_DIR,
  ATARI_H3_DIR,
  ATARI_H4_DIR
};

static int devbug = FALSE;

static FILE	*fp[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static int	flag[8];

static int	fid;
static char	filename[64];

#ifdef DO_DIR
static DIR	*dp = NULL;
#endif

void Device_Initialise (int *argc, char *argv[])
{
  int i;
  int j;

  for (i=j=1;i<*argc;i++)
    {
      if (strncmp(argv[i],"-H1",2) == 0)
	H[1] = argv[i]+2;
      else if (strncmp(argv[i],"-H2",2) == 0)
	H[2] = argv[i]+2;
      else if (strncmp(argv[i],"-H3",2) == 0)
	H[3] = argv[i]+2;
      else if (strncmp(argv[i],"-H4",2) == 0)
	H[4] = argv[i]+2;
      else if (strcmp(argv[i],"-devbug") == 0)
	devbug = TRUE;
      else
	argv[j++] = argv[i];
    }

  if (devbug)
    for (i=0;i<5;i++)
      printf ("H%d: %s\n", i, H[i]);

  *argc = j;
}

int Device_isvalid (char ch)
{
  int valid;

  if (isalnum(ch))
    valid = TRUE;
  else
    switch (ch)
      {
      case ':' :
      case '.' :
      case '_' :
	valid = TRUE;
	break;
      default :
	valid = FALSE;
	break;
      }

  return valid;
}

Device_GetFilename ()
{
  int bufadr;
  int offset = 0;
  int devnam = TRUE;

  bufadr = (memory[ICBAHZ] << 8) | memory[ICBALZ];

  while (Device_isvalid(memory[bufadr]))
    {
      int byte = memory[bufadr];

      if (!devnam)
	{
	  if (isupper(byte))
	    byte = tolower(byte);

	  filename[offset++] = byte;
	}
      else if (byte == ':')
	devnam = FALSE;

      bufadr++;
    }

  filename[offset++] = '\0';
}

match (char *pattern, char *filename)
{
  int status = TRUE;

  while (status && *filename && *pattern)
    {
      switch (*pattern)
	{
	case '?' :
	  pattern++;
	  filename++;
	  break;
	case '*' :
	  if (*filename == pattern[1])
	    {
	      pattern++;
	    }
	  else
	    {
	      filename++;
	    }
	  break;
	default :
	  status = (*pattern++ == *filename++);
	  break;
	}
    }

  return status;
}

void Device_HHOPEN (void)
{
  char fname[128];
  int devnum;
  int temp;

  if (devbug)
    printf ("HHOPEN\n");

  fid = memory[0x2e] >> 4;

  if (fp[fid])
    {
      fclose (fp[fid]);
      fp[fid] = NULL;
    }
/*
  flag[fid] = (memory[ICDNOZ] == 2) ? TRUE : FALSE;
*/
  Device_GetFilename ();
/*
  if (memory[ICDNOZ] == 0)
    sprintf (fname, "./%s", filename);
  else
    sprintf (fname, "%s/%s", h_prefix, filename);
*/
  devnum = memory[ICDNOZ];
  if (devnum > 9)
    {
      printf ("Attempt to access H%d: device\n", devnum);
      exit (1);
    }

  if (devnum >= 5)
    {
      flag[fid] = TRUE;
      devnum -= 5;
    }
  else
    {
      flag[fid] = FALSE;
    }

#ifdef VMS
/* Assumes H[devnum] is a directory _logical_, not an explicit directory
   specification! */
  sprintf (fname, "%s:%s", H[devnum], filename);
#else
  sprintf (fname, "%s/%s", H[devnum], filename);
#endif

  temp = memory[ICAX1Z];

  switch (temp)
    {
    case 4 :
      fp[fid] = fopen (fname, "r");
      if (fp[fid])
	{
	  regY = 1;
	  ClrN;
	}
      else
	{
	  regY = 170;
	  SetN;
	}
      break;
    case 6 :
#ifdef DO_DIR
      fp[fid] = tmpfile ();
      if (fp[fid])
	{
	  dp = opendir (H[devnum]);
	  if (dp)
	    {
	      struct dirent	*entry;
	      
	      while (entry = readdir (dp))
		{
		  if (match(filename, entry->d_name))
		    fprintf (fp[fid],"%s\n", entry->d_name);
		}
	      
	      closedir (dp);

	      regY = 1;
	      ClrN;

	      rewind (fp[fid]);

	      flag[fid] = TRUE;
	    }
	  else
	    {
	      regY = 163;
	      SetN;
	      fclose (fp[fid]);
	      fp[fid] = NULL;
	    }
	}
      else
#endif
	{
	  regY = 163;
	  SetN;
	}
      break;
    case 8 :
      fp[fid] = fopen (fname, "w");
      if (fp[fid])
	{
	  regY = 1;
	  ClrN;
	}
      else
	{
	  regY = 170;
	  SetN;
	}
      break;
    default :
      regY = 163;
      SetN;
      break;
    }
}

void Device_HHCLOS (void)
{
  if (devbug)
    printf ("HHCLOS\n");

  fid = memory[0x2e] >> 4;

  if (fp[fid])
    {
      fclose (fp[fid]);
      fp[fid] = NULL;
    }

  regY = 1;
  ClrN;
}

void Device_HHREAD (void)
{
  if (devbug)
    printf ("HHREAD\n");

  fid = memory[0x2e] >> 4;

  if (fp[fid])
    {
      int ch;

      ch = fgetc (fp[fid]);
      if (ch != EOF)
	{
	  if (flag[fid])
	    {
	      switch (ch)
		{
		case '\n' :
		  ch = 0x9b;
		  break;
		default :
		  break;
		}
	    }

	  regA = ch;
	  regY = 1;
	  ClrN;
	}
      else
	{
	  regY = 136;
	  SetN;
	}
    }
  else
    {
      regY = 163;
      SetN;
    }
}

void Device_HHWRIT (void)
{
  if (devbug)
    printf ("HHWRIT\n");

  fid = memory[0x2e] >> 4;

  if (fp[fid])
    {
      int ch;

      ch = regA;
      if (flag[fid])
	{
	  switch (ch)
	    {
	    case 0x9b :
	      ch = 0x0a;
	      break;
	    default :
	      break;
	    }
	}
      fputc (ch, fp[fid]);
      regY = 1;
      ClrN;
    }
  else
    {
      regY = 163;
      SetN;
    }
}

void Device_HHSTAT (void)
{
  if (devbug)
    printf ("HHSTAT\n");

  fid = memory[0x2e] >> 4;

  regY = 146;
  SetN;
}

void Device_HHSPEC (void)
{
  if (devbug)
    printf ("HHSPEC\n");

  fid = memory[0x2e] >> 4;

  switch (memory[ICCOMZ])
    {
    case 0x20 :
      printf ("RENAME Command\n");
      break;
    case 0x21 :
      printf ("DELETE Command\n");
      break;
    case 0x23 :
      printf ("LOCK Command\n");
      break;
    case 0x24 :
      printf ("UNLOCK Command\n");
      break;
    case 0x25 :
      printf ("NOTE Command\n");
      break;
    case 0x26 :
      printf ("POINT Command\n");
      break;
    case 0xFE :
      printf ("FORMAT Command\n");
      break;
    default :
      printf ("UNKNOWN Command\n");
      break;
    }

  regY = 146;
  SetN;
}

void Device_HHINIT (void)
{
  if (devbug)
    printf ("HHINIT\n");
}

static int phfd = -1;
void Device_PHCLOS (void);
static char *spool_file = NULL;

void Device_PHOPEN (void)
{
  if (devbug)
    printf ("PHOPEN\n");

  if (phfd != -1)
    Device_PHCLOS ();

  spool_file = tmpnam (NULL);
  phfd = open (spool_file, O_CREAT | O_TRUNC | O_WRONLY, 0777);
  if (phfd != -1)
    {
	regY = 1;
	ClrN;
    }
  else
    {
	regY = 130;
	SetN;
    }
}

void Device_PHCLOS (void)
{
  if (devbug)
    printf ("PHCLOS\n");

  if (phfd != -1)
    {
      char command[256];
      int status;

      close (phfd);

      sprintf (command, PRINT_COMMAND, spool_file);
      system (command);

#ifndef VMS
      status = unlink (spool_file);
      if (status == -1)
	{
	  perror (spool_file);
	  exit (1);
	}
#endif

      phfd = -1;
    }

  regY = 1;
  ClrN;
}

void Device_PHREAD (void)
{
  if (devbug)
    printf ("PHREAD\n");

  regY = 146;
  SetN;
}

void Device_PHWRIT (void)
{
  unsigned char byte;
  int status;

  if (devbug)
    printf ("PHWRIT\n");

  byte = regA;
  if (byte == 0x9b)
    byte = '\n';

  status = write (phfd, &byte, 1);
  if (status == 1)
    {
      regY = 1;
      ClrN;
    }
  else
    {
      regY = 144;
      SetN;
    }
}

void Device_PHSTAT (void)
{
  if (devbug)
    printf ("PHSTAT\n");
}

void Device_PHSPEC (void)
{
  if (devbug)
    printf ("PHSPEC\n");

  regY = 1;
  ClrN;
}

void Device_PHINIT (void)
{
  if (devbug)
    printf ("PHINIT\n");

  phfd = -1;
  regY = 1;
  ClrN;
}

void Device_KHOPEN (void)
{
  if (devbug)
    printf ("KHOPEN\n");
}

void Device_KHCLOS (void)
{
  if (devbug)
    printf ("KHCLOS\n");
}

void Device_KHREAD (void)
{
  if (devbug)
    printf ("KHREAD\n");
}

void Device_KHWRIT (void)
{
  if (devbug)
    printf ("KHWRIT\n");
}

void Device_KHSTAT (void)
{
  if (devbug)
    printf ("KHSTAT\n");
}

void Device_KHSPEC (void)
{
  if (devbug)
    printf ("KHSPEC\n");
}

void Device_KHINIT (void)
{
  if (devbug)
    printf ("KHINIT\n");
}

unsigned char ascii_to_screen[128] =
{
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f
};

#define LMARGN memory[0x52]
#define RMARGN memory[0x53]
#define ROWCRS memory[0x54]
#define COLCRSL memory[0x55]
#define COLCRSH memory[0x56]

static int OLDADR = 0;

void Device_PutChar (UBYTE ch)
{
  int address = 0xb000+ROWCRS*40+COLCRSL;
  int column;

  memory[OLDADR] ^= 0x80;

  switch (ch)
    {
    case 0x1c :
      ROWCRS--;
      break;
    case 0x1d :
      ROWCRS++;
      break;
    case 0x1e :
      column = (COLCRSH << 8) + COLCRSL;
      column--;
      COLCRSH = (COLCRSH >> 8);
      COLCRSL = column & 0xff;
      break;
    case 0x1f :
      column = (COLCRSH << 8) + COLCRSL;
      column++;
      COLCRSH = (COLCRSH >> 8);
      COLCRSL = column & 0xff;
      break;
    case 0x7d :
      memset (&memory[0xb000], 0, 960);
      ROWCRS = 0;
      COLCRSL = LMARGN;
      COLCRSH = 0;
      break;
    case 0x7f :
      column = (COLCRSH << 8) + COLCRSL;
      column = (column & 0xf8) + 8;
      COLCRSH = (COLCRSH >> 8);
      COLCRSL = column & 0xff;
      break;
    case 0x9b :
      COLCRSL = LMARGN;
      COLCRSH = 0;
      if (ROWCRS == 23)
	{
	  UBYTE *lno1 = &memory[0xb000];
	  UBYTE *lno2 = lno1 + 40;
	  int i;

	  for (i=0;i<23;i++)
	    {
	      memcpy (lno1, lno2, 40);
	      lno1 += 40;
	      lno2 += 40;
	    }

	  memset (lno1, 0, 40);
	}
      else
	{
	  ROWCRS++;
	}
      break;
    default :
      {
	UBYTE bit7 = ch & 0x80;

	address = 0xb000 + ROWCRS*40 + COLCRSL;
	ch &= 0x7f;
	ch = ascii_to_screen[ch] | bit7;
	memory[address++] = ch;
	COLCRSL++;
	if (COLCRSL > RMARGN)
	  {
	    COLCRSL = LMARGN;
	    ROWCRS++;
	  }
      }
    }

  address = 0xb000 + ROWCRS*40 + COLCRSL;
  memory[address] ^= 0x80;
  OLDADR = address;
}

void Device_SHOPEN (void)
{
  if (devbug)
    printf ("SHOPEN\n");

  regY = 1;
  ClrN;
}

void Device_SHCLOS (void)
{
  if (devbug)
    printf ("SHCLOS\n");

  regY = 1;
  ClrN;
}

void Device_SHREAD (void)
{
  if (devbug)
    printf ("SHREAD\n");

  regY = 1;
  ClrN;
}

void Device_SHWRIT (void)
{
  if (devbug)
    printf ("SHWRIT\n");

  Device_PutChar (regA);
  regY = 1;
  ClrN;
}

void Device_SHSTAT (void)
{
  if (devbug)
    printf ("SHSTAT\n");

  regY = 1;
  ClrN;
}

void Device_SHSPEC (void)
{
  if (devbug)
    printf ("SHSPEC\n");

  regY = 1;
  ClrN;
}

void Device_SHINIT (void)
{
  if (devbug)
    printf ("SHINIT\n");

  regY = 1;
  ClrN;
}

void Device_EHOPEN (void)
{
  if (devbug)
    printf ("EHOPEN\n");

  LMARGN = 2;
  RMARGN = 39;
  ROWCRS = 0;
  COLCRSL = LMARGN;
  COLCRSH = 0;
}

void Device_EHCLOS (void)
{
  if (devbug)
    printf ("EHCLOS\n");
}

void Device_EHREAD (void)
{
  if (devbug)
    printf ("EHREAD\n");
}

void Device_EHWRIT (void)
{
  if (devbug)
    printf ("EHWRIT\n");

  Device_SHWRIT ();
}

void Device_EHSTAT (void)
{
  if (devbug)
    printf ("EHSTAT\n");
}

void Device_EHSPEC (void)
{
  if (devbug)
    printf ("EHSPEC\n");
}

void Device_EHINIT (void)
{
  if (devbug)
    printf ("EHINIT\n");

}

