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

static int	disk[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

int SIO_Mount (int diskno, char *filename)
{
	disk[diskno-1] = open (filename, O_RDWR, 0777);

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
		sector = DAUX1 + DAUX2 * 256;
		buffer = DBUFLO + DBUFHI * 256;
		count = DBYTLO + DBYTHI * 256;

		lseek (disk[DUNIT-1], (sector-1)*128+0, SEEK_SET);

#ifdef DEBUG
		printf ("SIO: DCOMND = %x, SECTOR = %d, BUFADR = %x, BUFLEN = %d\n",
			DCOMND, sector, buffer, count);
#endif

		switch (DCOMND)
		{
			case 0x50 :
			case 0x57 :
				for (i=0;i<count;i++)
				{
					char	ch;

					ch = GetByte (buffer+i);
					write (disk[DUNIT-1], &ch, 1);
				}

				cpu_status.Y = 1;
				cpu_status.flag.N = 0;
				break;
			case 0x52 :
				for (i=0;i<count;i++)
				{
					char	ch;

					read (disk[DUNIT-1], &ch, 1);
					PutByte (buffer+i, ch);
				}

				cpu_status.Y = 1;
				cpu_status.flag.N = 0;
				break;
			case 0x53 :
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
}
