#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<fcntl.h>
#include	<ctype.h>

#ifndef	AMIGA
#include	<unistd.h>
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

#define FALSE   0
#define TRUE    1

#include	"system.h"
#include	"atari.h"
#include	"cpu.h"
#include	"atari_h_device.h"

char	*h_prefix = "H/";

static FILE	*fp[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static int	flag[8];

static int	fid;
static char	filename[64];

#ifdef DO_DIR
static DIR	*dp = NULL;
#endif

static CPU_Status	cpu_status;

Device_GetFilename ()
{
	int	bufadr;
	int	buflen;
	int	offset = 0;
	int	devnam = TRUE;
	int	byte;

	bufadr = (GetByte(ICBAHZ) << 8) | GetByte(ICBALZ);
	buflen = (GetByte(ICBLHZ) << 8) | GetByte(ICBLLZ);

	while ((byte = GetByte (bufadr)) != 0x9b)
	{
		if (!devnam)
			filename[offset++] = byte;
		else if (byte == ':')
			devnam = FALSE;

		bufadr++;
	}

	filename[offset++] = '\0';
}

match (char *pattern, char *filename)
{
	int	status = TRUE;

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

void H_Open ()
{
	char	fname[128];
	int	temp;

	if (fp[fid])
	{
		fclose (fp[fid]);
		fp[fid] = NULL;
	}

	flag[fid] = (GetByte(ICDNOZ) == 2) ? TRUE : FALSE;

	Device_GetFilename ();

	strcpy (fname, h_prefix);
	strcat (fname, filename);

	temp = GetByte(ICAX1Z);

	switch (temp)
	{
		case 4 :
			fp[fid] = fopen (fname, "r");
			if (fp[fid])
			{
				cpu_status.Y = 1;
				cpu_status.flag.N = 0;
			}
			else
			{
				cpu_status.Y = 170;
				cpu_status.flag.N = 1;
			}
			break;
		case 6 :
#ifdef DO_DIR
			fp[fid] = tmpfile ();
			if (fp[fid])
			{
				dp = opendir (h_prefix);
				if (dp)
				{
					struct dirent	*entry;

					while (entry = readdir (dp))
					{
						if (match(filename, entry->d_name))
							fprintf (fp[fid],"%s\n", entry->d_name);
					}

					closedir (dp);

					cpu_status.Y = 1;
					cpu_status.flag.N = 0;

					rewind (fp[fid]);

					flag[fid] = TRUE;
				}
				else
				{
					cpu_status.Y = 163;
					cpu_status.flag.N = 1;
					fclose (fp[fid]);
					fp[fid] = NULL;
				}
			}
			else
#endif
			{
				cpu_status.Y = 163;
				cpu_status.flag.N = 1;
			}
			break;
		case 8 :
			fp[fid] = fopen (fname, "w");
			if (fp[fid])
			{
				cpu_status.Y = 1;
				cpu_status.flag.N = 0;
			}
			else
			{
				cpu_status.Y = 170;
				cpu_status.flag.N = 1;
			}
			break;
		default :
			cpu_status.Y = 163;
			cpu_status.flag.N = 1;
			break;
	}
}

void H_Close ()
{
	if (fp[fid])
	{
		fclose (fp[fid]);
		fp[fid] = NULL;
	}

	cpu_status.Y = 1;
	cpu_status.flag.N = 0;
}

void H_Read ()
{
	if (fp[fid])
	{
		int	ch;

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

			cpu_status.A = ch;
			cpu_status.Y = 1;
			cpu_status.flag.N = 0;
		}
		else
		{
			cpu_status.Y = 136;
			cpu_status.flag.N = 1;
		}
	}
	else
	{
		cpu_status.Y = 163;
		cpu_status.flag.N = 1;
	}
}

void H_Write ()
{
	if (fp[fid])
	{
		int	ch;

		ch = cpu_status.A;
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
		cpu_status.Y = 1;
		cpu_status.flag.N = 0;
	}
	else
	{
		cpu_status.Y = 163;
		cpu_status.flag.N = 1;
	}
}

void H_Status ()
{
	cpu_status.Y = 146;
	cpu_status.flag.N = 1;
}

void H_Special ()
{
	switch (GetByte(ICCOMZ))
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

	cpu_status.Y = 146;
	cpu_status.flag.N = 1;
}

void H_Device (UBYTE esc_code)
{
	CPU_GetStatus (&cpu_status);

	fid = cpu_status.X >> 4;

	switch (esc_code)
	{
		case ESC_H_OPEN :
			H_Open ();
			break;
		case ESC_H_CLOSE :
			H_Close ();
			break;
		case ESC_H_READ :
			H_Read ();
			break;
		case ESC_H_WRITE :
			H_Write ();
			break;
		case ESC_H_STATUS :
			H_Status ();
			break;
		case ESC_H_SPECIAL :
			H_Special ();
			break;
	}

	CPU_PutStatus (&cpu_status);
}
