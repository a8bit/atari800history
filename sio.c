/*

 * All Input is assumed to be going to RAM (no longer, ROM works, too.)
 * All Output is assumed to be coming from either RAM or ROM
 *
 */
#define Peek(a) (memory[(a)])
#define DPeek(a) ( memory[(a)]+( memory[(a)+1]<<8 ) )
#define Poke(a,b) ( memory[(a)] = (b) )
/* PM Notes:
   note the versions in Thors emu are able to deal with ROM better than
   these ones.  These are only used for the 'fake' fast SIO replacement
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>
#include <fcntl.h>
extern char	memory_log[];
extern char	scratch[];
extern unsigned int	memory_log_index;
#define ADDLOG( x ) { if( memory_log_index + strlen( x ) > 8192 ) memory_log_index = 0; memcpy( &memory_log[ memory_log_index ], x, strlen( x ) ); memory_log_index += strlen( x ); }
#define ADDLOGTEXT( x ) { if( memory_log_index + strlen( x ) > 8192 ) memory_log_index = 0; memcpy( &memory_log[ memory_log_index ], x"\x00d\x00a", strlen( x"\x00d\x00a" ) ); memory_log_index += strlen( x"\x00d\x00a" ); }
#else
#ifdef VMS
#include <unixio.h>
#include <file.h>
#else
#include <fcntl.h>
#ifndef AMIGA
#include <unistd.h>
#endif
#endif
#endif

#ifdef DJGPP
#include "djgpp.h"
#endif
extern int DELAYED_SERIN_IRQ;
extern int DELAYED_SEROUT_IRQ;
extern int DELAYED_XMTDONE_IRQ;
typedef int ATPtr;


static char *rcsid = "$Id: sio.c,v 1.9 1998/02/21 14:55:21 david Exp $";

#define FALSE   0
#define TRUE    1

#include "atari.h"
#include "cpu.h"
#include "sio.h"
#include "pokeysnd.h"

void CopyFromMem(int to, UBYTE *, int size);
void CopyToMem(UBYTE *, ATPtr from, int size);

#define	MAGIC1	0x96
#define	MAGIC2	0x02

struct ATR_Header {
	unsigned char magic1;
	unsigned char magic2;
	unsigned char seccountlo;
	unsigned char seccounthi;
	unsigned char secsizelo;
	unsigned char secsizehi;
	unsigned char hiseccountlo;
	unsigned char hiseccounthi;
	unsigned char gash[8];
};

/*
 * it seems, there are two different ATR formats with different handling for
 * DD sectors
 */
static int atr_format[MAX_DRIVES];

typedef enum Format {
	XFD, ATR
} Format;

static Format format[MAX_DRIVES];
static int disk[MAX_DRIVES] =
{-1, -1, -1, -1, -1, -1, -1, -1};
static int sectorcount[MAX_DRIVES];
static int sectorsize[MAX_DRIVES];

static enum DriveStatus {
	Off,
	NoDisk,
	ReadOnly,
	ReadWrite
} drive_status[MAX_DRIVES];

char sio_filename[MAX_DRIVES][FILENAME_LEN];

/* Serial I/O emulation support */
UBYTE CommandFrame[6];
int CommandIndex = 0;
UBYTE DataBuffer[256 + 3];
char sio_status[256];
int DataIndex = 0;
int TransferStatus = 0;
int ExpectedBytes = 0;


void SIO_Initialise(int *argc, char *argv[])
{
	int i;

	for (i = 0; i < MAX_DRIVES; i++)
		strcpy(sio_filename[i], "Empty");

	TransferStatus = SIO_NoFrame;
}

int SIO_Mount(int diskno, char *filename)
{
	struct ATR_Header header;

	int fd;

	drive_status[diskno - 1] = ReadWrite;
	strcpy(sio_filename[diskno - 1], "Empty");

	fd = open(filename, O_RDWR | O_BINARY, 0777);
	if (fd == -1) {
		fd = open(filename, O_RDONLY | O_BINARY, 0777);
		drive_status[diskno - 1] = ReadOnly;
	}

	if (fd >= 0) {
		int status;
		ULONG file_length = lseek(fd, 0L, SEEK_END);
		lseek(fd, 0L, SEEK_SET);

		status = read(fd, &header, sizeof(struct ATR_Header));
		if (status == -1) {
			close(fd);
			disk[diskno - 1] = -1;
			return FALSE;
		}

		strcpy(sio_filename[diskno - 1], filename);

		if ((header.magic1 == MAGIC1) && (header.magic2 == MAGIC2)) {
			format[diskno - 1] = ATR;

			sectorcount[diskno - 1] = header.hiseccounthi << 24 |
				header.hiseccountlo << 16 |
				header.seccounthi << 8 |
				header.seccountlo;

			sectorsize[diskno - 1] = header.secsizehi << 8 |
				header.secsizelo;

			sectorcount[diskno - 1] /= 8;
			if (sectorsize[diskno - 1] == 256) {
				sectorcount[diskno - 1] += 3;	/* Compensate for first 3 sectors */
				sectorcount[diskno - 1] /= 2;
			}

			/* ok, try to check if it's a SIO2PC ATR */
			{
				ULONG len = (sectorcount[diskno - 1] - 3) * sectorsize[diskno - 1] + 16 + 3*128;
				if (file_length == len)
					atr_format[diskno - 1] = SIO2PC_ATR;
				else
					atr_format[diskno - 1] = OTHER_ATR;
			}

#ifdef DEBUG
			printf("ATR: sectorcount = %d, sectorsize = %d\n",
				   sectorcount[diskno - 1],
				   sectorsize[diskno - 1]);
			printf("%s ATR\n", atr_format[diskno - 1] == SIO2PC_ATR ? "SIO2PC" : "???");
#endif
		}
		else {
			format[diskno - 1] = XFD;
			/* XFD might be of double density ! (PS) */
			sectorsize[diskno - 1] = (file_length > (1040 * 128)) ? 256 : 128;
			sectorcount[diskno - 1] = file_length / sectorsize[diskno - 1];
		}
	}
	else {
		drive_status[diskno - 1] = NoDisk;
	}

	disk[diskno - 1] = fd;

	return (disk[diskno - 1] != -1) ? TRUE : FALSE;
}

void SIO_Dismount(int diskno)
{
	if (disk[diskno - 1] != -1) {
		close(disk[diskno - 1]);
		disk[diskno - 1] = -1;
		drive_status[diskno - 1] = NoDisk;
		strcpy(sio_filename[diskno - 1], "Empty");
	}
}

void SIO_DisableDrive(int diskno)
{
	drive_status[diskno - 1] = Off;
	strcpy(sio_filename[diskno - 1], "Off");
}

void SizeOfSector(UBYTE unit, int sector, int *sz, ULONG * ofs)
{
	int size = sectorsize[unit];
	ULONG offset = 0;

	switch (format[unit]) {
	case XFD:
		offset = (sector - 1) * size;
		break;
	case ATR:
		if (sector < 4)
			/* special case for first three sectors in ATR image */
			size = 128;
		if (atr_format[unit] == SIO2PC_ATR && sector >= 4)
			offset = (sector - 4) * size + 16 + 3*128;
		else
			offset = (sector - 1) * size + 16;
		break;
	default:
#ifdef WIN32
		ADDLOGTEXT( "Fatal error in atari_sio.c" );
		Atari800_Exit(FALSE);
		return;
#else
		printf("Fatal Error in atari_sio.c\n");
		Atari800_Exit(FALSE);
#endif
	}

	if (sz)
		*sz = size;

	if (ofs)
		*ofs = offset;
}

int SeekSector(int unit, int sector)
{
	ULONG offset;
	int size;

	sprintf(sio_status, "%d: %d", unit + 1, sector);
	SizeOfSector((UBYTE)unit, sector, &size, &offset);
	/* printf("Sector %x,Offset: %x\n",sector,offset); */
	if (offset > lseek(disk[unit], 0L, SEEK_END)) {
#ifdef DEBUG
		printf("Seek after end of file\n");
#endif
/*
		Atari800_Exit(FALSE);
		exit(1);
*/
	}
	else
		lseek(disk[unit], offset, SEEK_SET);

	return size;
}


/* Unit counts from zero up */
int ReadSector(int unit, int sector, UBYTE * buffer)
{
	int size;

	if (drive_status[unit] != Off) {
		if (disk[unit] != -1) {
			if (sector <= sectorcount[unit]) {
				size = SeekSector(unit, sector);
				read(disk[unit], buffer, size);
				return 'C';
			}
			else
				return 'E';
		}
		else
			return 'N';
	}
	else
		return 0;
}

int WriteSector(int unit, int sector, UBYTE * buffer)
{
	int size;

	if (drive_status[unit] != Off) {
		if (disk[unit] != -1) {
			if (drive_status[unit] == ReadWrite) {
				if (sector <= sectorcount[unit]) {
					size = SeekSector(unit, sector);
					write(disk[unit], buffer, size);
					return 'C';
				}
				else
					return 'E';
			}
			else
				return 'E';
		}
		else
			return 'N';
	}
	else
		return 0;
}

/*
 * FormatSingle is used on the XF551 for formating SS/DD and DS/DD too
 * however, I have to check if they expect a 256 byte buffer or if 128
 * is ok either
 */
int FormatSingle(int unit, UBYTE * buffer)
{
	int i;

	if (drive_status[unit] != Off) {
		if (disk[unit] != -1) {
			if (drive_status[unit] == ReadWrite) {
				sectorcount[unit] = 720;
				sectorsize[unit] = 128;
				format[unit] = XFD;
				SeekSector(unit, 1);
				memset(buffer, 0, 128);
				for (i = 1; i <= 720; i++)
					write(disk[unit], buffer, 128);
				memset(buffer, 0xff, 128);
				return 'C';
			}
			else
				return 'E';
		}
		else
			return 'N';
	}
	else
		return 0;
}

int FormatEnhanced(int unit, UBYTE * buffer)
{
	int i;

	if (drive_status[unit] != Off) {
		if (disk[unit] != -1) {
			if (drive_status[unit] == ReadWrite) {
				sectorcount[unit] = 1040;
				sectorsize[unit] = 128;
				format[unit] = XFD;
				SeekSector(unit, 1);
				memset(buffer, 0, 128);
				for (i = 1; i <= 1040; i++)
					write(disk[unit], buffer, 128);
				memset(buffer, 0xff, 128);
				return 'C';
			}
			else
				return 'E';
		}
		else
			return 'N';
	}
	else
		return 0;
}

int WriteStatusBlock(int unit, UBYTE * buffer)
{
	int size;

	if (drive_status[unit] != Off) {
		/*
		 * We only care about the density and the sector count
		 * here. Setting everything else right here seems to
		 * be non-sense
		 */
		if (format[unit] == ATR) {
			/* I'm not sure about this density settings, my XF551
			 * honnors only the sector size and ignore the density
			 */
			size = buffer[6] * 256 + buffer[7];
			if (size == 128 || size == 256)
				sectorsize[unit] = size;
			else if (buffer[5] == 8) {
				sectorsize[unit] = 256;
			}
			else {
				sectorsize[unit] = 128;
			}
			/* Note, that the number of heads are minus 1 */
			sectorcount[unit] = buffer[0] * (buffer[2] * 256 + buffer[3]) * (buffer[4] + 1);
			return 'C';
		}
		else
			return 'E';
	}
	else
		return 0;
}

/*
 * My german "Atari Profi Buch" says, buffer[4] holds the number of
 * heads. However, BiboDos and my XF551 think that´s the number minus 1.
 *
 * ???
 */
int ReadStatusBlock(int unit, UBYTE * buffer)
{
	int size, spt, heads;

	if (drive_status[unit] != Off) {
		spt = sectorcount[unit] / 40;
		heads =  (spt > 26) ? 2 : 1;
		SizeOfSector((UBYTE)unit, 0x168, &size, NULL);

		buffer[0] = 40;			/* # of tracks */
		buffer[1] = 1;			/* step rate. No idea what this means */
		buffer[2] = spt >> 8;	/* sectors per track. HI byte */
		buffer[3] = spt & 0xFF;	/* sectors per track. LO byte */
		buffer[4] = heads-1;	/* # of heads */
		if (size == 128) {
			buffer[5] = 4;		/* density */
			buffer[6] = 0;		/* HI bytes per sector */
			buffer[7] = 128;	/* LO bytes per sector */
		}
		else {
			buffer[5] = 8;		/* double density */
			buffer[6] = 1;		/* HI bytes per sector */
			buffer[7] = 0;		/* LO bytes per sector */
		}
		buffer[8] = 1;			/* drive is online */
		buffer[9] = 192;		/* transfer speed. Whatever this means */
		return 'C';
	}
	else
		return 0;
}

/*
   Status Request from Atari 400/800 Technical Reference Notes

   DVSTAT + 0   Command Status
   DVSTAT + 1   Hardware Status
   DVSTAT + 2   Timeout
   DVSTAT + 3   Unused

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
int DriveStatus(int unit, UBYTE * buffer)
{
	if (drive_status[unit] != Off) {
		if (drive_status[unit] == ReadWrite) {
			buffer[0] = (sectorsize[unit] == 256) ? (32 + 16) : (16);
			buffer[1] = (disk[unit] != -1) ? (128) : (0);
		}
		else {
			buffer[0] = (sectorsize[unit] == 256) ? (32) : (0);
			buffer[1] = (disk[unit] != -1) ? (192) : (64);
		}
		if (sectorcount[unit] == 1040)
			buffer[0] |= 128;
		buffer[2] = 1;
		buffer[3] = 0;
		return 'C';
	}
	else
		return 0;
}


void SIO(void)
{
	int sector = DPeek(0x30a);
	UBYTE unit = Peek(0x301) - 1;
	UBYTE result = 0x00;
	ATPtr data = DPeek(0x304);
	int length = DPeek(0x308);
	int realsize = 0;
	int cmd = Peek(0x302);

#ifdef SET_LED
	Atari_Set_LED(1);
#endif

/*
   printf("SIO: Unit %x,Sector %x,Data %x,Length %x,CMD %x\n",unit,sector,data,length,cmd);
 */
	if (Peek(0x300) == 0x31)
		switch (cmd) {
		case 0x4e:				/* Read Status Block */
			if (12 == length) {
				result = ReadStatusBlock(unit, DataBuffer);
				if (result == 'C')
					CopyToMem(DataBuffer, data, 12);
			}
			else
				result = 'E';
			break;
		case 0x4f:				/* Write Status Block */
			if (12 == length) {
				CopyFromMem(data, DataBuffer, 12);
				result = WriteStatusBlock(unit, DataBuffer);
			}
			else
				result = 'E';
			break;
		case 0x50:				/* Write */
		case 0x57:
			SizeOfSector(unit, sector, &realsize, NULL);
			if (realsize == length) {
				CopyFromMem(data, DataBuffer, realsize);
				result = WriteSector(unit, sector, DataBuffer);
			}
			else
				result = 'E';
			break;
		case 0x52:				/* Read */
			SizeOfSector(unit, sector, &realsize, NULL);
			if (realsize == length) {
				result = ReadSector(unit, sector, DataBuffer);
				if (result == 'C')
					CopyToMem(DataBuffer, data, realsize);
			}
			else
				result = 'E';
			break;
		case 0x53:				/* Status */
			if (4 == length) {
				result = DriveStatus(unit, DataBuffer);
				CopyToMem(DataBuffer, data, 4);
			}
			else
				result = 'E';
			break;
		case 0x21:				/* Single Density Format */
			if (length == 128) {
				result = FormatSingle(unit, DataBuffer);
				if (result == 'C')
					CopyToMem(DataBuffer, data, realsize);
			}
			else
				result = 'E';
			break;
		case 0x22:				/* Enhanced Density Format */
			if (length == 128) {
				result = FormatEnhanced(unit, DataBuffer);
				if (result == 'C')
					CopyToMem(DataBuffer, data, realsize);
			}
			else
				result = 'E';
			break;
		case 0x66:				/* US Doubler Format - I think! */
			result = 'A';		/* Not yet supported... to be done later... */
			break;
		default:
			result = 'N';
		}

	switch (result) {
	case 0x00:					/* Device disabled, generate timeout */
		regY = 138;
		SetN;
		break;
	case 'A':					/* Device acknoledge */
	case 'C':					/* Operation complete */
		regY = 1;
		ClrN;
		break;
	case 'N':					/* Device NAK */
		regY = 144;
		SetN;
		break;
	case 'E':					/* Device error */
	default:
		regY = 146;
		SetN;
		break;
	}

	Poke(0x0303, regY);

#ifdef SET_LED
	Atari_Set_LED(0);
#endif

}

void SIO_Initialize(void)
{
	TransferStatus = SIO_NoFrame;
}


UBYTE ChkSum(UBYTE * buffer, UWORD length)
{
	int i;
	int checksum = 0;

	for (i = 0; i < length; i++, buffer++) {
		checksum += *buffer;
		while (checksum > 255)
			checksum -= 255;
	}

	return checksum;
}

void Command_Frame(void)
{
	int unit;
	int result = 'A';
	int sector;
	int realsize;


	sector = CommandFrame[2] | (((UWORD) (CommandFrame[3])) << 8);
	unit = CommandFrame[0] - '1';
	if (unit > 8) {				/* UBYTE - range ! */
#ifdef WIN32
		sprintf( scratch, "Unknown command frame: %02x %02x %02x %02x %02x\n",
			   CommandFrame[0], CommandFrame[1], CommandFrame[2],
			   CommandFrame[3], CommandFrame[4]);
		ADDLOG( scratch );
#else
		printf("Unknown command frame: %02x %02x %02x %02x %02x\n",
			   CommandFrame[0], CommandFrame[1], CommandFrame[2],
			   CommandFrame[3], CommandFrame[4]);
#endif
		result = 0;
	}
	else
		switch (CommandFrame[1]) {
		case 0x4e:				/* Read Status */
			DataBuffer[0] = ReadStatusBlock(unit, DataBuffer + 1);
			DataBuffer[13] = ChkSum(DataBuffer + 1, 12);
			DataIndex = 0;
			ExpectedBytes = 14;
			TransferStatus = SIO_ReadFrame;
			DELAYED_SERIN_IRQ += SERIN_INTERVAL;
			break;
		case 0x4f:
			ExpectedBytes = 13;
			DataIndex = 0;
			TransferStatus = SIO_WriteFrame;
			break;
		case 0x50:				/* Write */
		case 0x57:
			SizeOfSector((UBYTE)unit, sector, &realsize, NULL);
			ExpectedBytes = realsize + 1;
			DataIndex = 0;
			TransferStatus = SIO_WriteFrame;
			break;
		case 0x52:				/* Read */
			SizeOfSector((UBYTE)unit, sector, &realsize, NULL);
			DataBuffer[0] = ReadSector(unit, sector, DataBuffer + 1);
			DataBuffer[1 + realsize] = ChkSum(DataBuffer + 1, realsize);
			DataIndex = 0;
			ExpectedBytes = 2 + realsize;
			TransferStatus = SIO_ReadFrame;
			DELAYED_SERIN_IRQ += SERIN_INTERVAL;
			break;
		case 0x53:				/* Status */
			DataBuffer[0] = DriveStatus(unit, DataBuffer + 1);
			DataBuffer[1 + 4] = ChkSum(DataBuffer + 1, 4);
			DataIndex = 0;
			ExpectedBytes = 6;
			TransferStatus = SIO_ReadFrame;
			DELAYED_SERIN_IRQ += SERIN_INTERVAL;
			break;
		case 0x21:				/* Single Density Format */
			DataBuffer[0] = FormatSingle(unit, DataBuffer + 1);
			DataBuffer[1 + 128] = ChkSum(DataBuffer + 1, 128);
			DataIndex = 0;
			ExpectedBytes = 2 + 128;
			TransferStatus = SIO_FormatFrame;
			DELAYED_SERIN_IRQ += SERIN_INTERVAL;
			break;
		case 0x22:				/* Duel Density Format */
			DataBuffer[0] = FormatEnhanced(unit, DataBuffer + 1);
			DataBuffer[1 + 128] = ChkSum(DataBuffer + 1, 128);
			DataIndex = 0;
			ExpectedBytes = 2 + 128;
			TransferStatus = SIO_FormatFrame;
			DELAYED_SERIN_IRQ += SERIN_INTERVAL;
			break;
		case 0x66:				/* US Doubler Format - I think! */
			result = 'A';		/* Not yet supported... to be done later... */
			break;
		default:
#ifdef WIN32
			sprintf( scratch, "Command frame: %02x %02x %02x %02x %02x\n",
				   CommandFrame[0], CommandFrame[1], CommandFrame[2],
				   CommandFrame[3], CommandFrame[4]);
			ADDLOG( scratch );
#else
			printf("Command frame: %02x %02x %02x %02x %02x\n",
				   CommandFrame[0], CommandFrame[1], CommandFrame[2],
				   CommandFrame[3], CommandFrame[4]);
#endif
			break;
			result = 0;
		}

	if (result == 0)
		TransferStatus = SIO_NoFrame;
}


/* Enable/disable the command frame */
void SwitchCommandFrame(int onoff)
{

	if (onoff) {				/* Enabled */
		if (TransferStatus != SIO_NoFrame)
#ifdef WIN32
		{
			sprintf( scratch, "Unexpected command frame %x.\n", TransferStatus);
			ADDLOG( scratch );
		}
#else
			printf("Unexpected command frame %x.\n", TransferStatus);
#endif
		CommandIndex = 0;
		DataIndex = 0;
		ExpectedBytes = 5;
		TransferStatus = SIO_CommandFrame;
#ifdef SET_LED
		Atari_Set_LED(1);
#endif
		/* printf("Command frame expecting.\n"); */
	}
	else {
		if (TransferStatus != SIO_StatusRead && TransferStatus != SIO_NoFrame &&
			TransferStatus != SIO_ReadFrame) {
			if (!(TransferStatus == SIO_CommandFrame && CommandIndex == 0))
#ifdef WIN32
			{
				sprintf( scratch, "Command frame %02x unfinished.\n", TransferStatus);
				ADDLOG( scratch );
			}
#else
				printf("Command frame %02x unfinished.\n", TransferStatus);
#endif
			TransferStatus = SIO_NoFrame;
		}
		CommandIndex = 0;
	}
}

UBYTE WriteSectorBack(void)
{
	UWORD sector;
	UBYTE unit;
	UBYTE result;

	sector = CommandFrame[2] | (((UWORD) (CommandFrame[3])) << 8);
	unit = CommandFrame[0] - '1';
	if (unit > 8) {				/* UBYTE range ! */
		result = 0;
	}
	else
		switch (CommandFrame[1]) {
		case 0x4f:				/* Write Status Block */
			result = WriteStatusBlock(unit, DataBuffer);
			break;
		case 0x50:				/* Write */
		case 0x57:
			result = WriteSector(unit, sector, DataBuffer);
			break;
		default:
			result = 'E';
		}

	return result;
}

/* Put a byte that comes out of POKEY. So get it here... */
void SIO_PutByte(int byte)
{
	UBYTE sum, result;

	switch (TransferStatus) {
	case SIO_CommandFrame:
		if (CommandIndex < ExpectedBytes) {
			CommandFrame[CommandIndex++] = byte;
			if (CommandIndex >= ExpectedBytes) {
				/* printf("%x\n",CommandFrame[0]); */
				if (((CommandFrame[0] >= 0x31) && (CommandFrame[0] <= 0x38))) {
					TransferStatus = SIO_StatusRead;
					/* printf("Command frame done.\n"); */
					DELAYED_SERIN_IRQ += SERIN_INTERVAL + ACK_INTERVAL;
				}
				else
					TransferStatus = SIO_NoFrame;
			}
		}
		else {
#ifdef WIN32
			ADDLOGTEXT( "Invalid command frame!" );
#else
			printf("Invalid command frame!\n");
#endif
			TransferStatus = SIO_NoFrame;
		}
		break;
	case SIO_WriteFrame:		/* Expect data */
		if (DataIndex < ExpectedBytes) {
			DataBuffer[DataIndex++] = byte;
			if (DataIndex >= ExpectedBytes) {
				sum = ChkSum(DataBuffer, ExpectedBytes - 1);
				if (sum == DataBuffer[ExpectedBytes - 1]) {
					result = WriteSectorBack();
					if (result) {
						DataBuffer[0] = 'A';
						DataBuffer[1] = result;
						DataIndex = 0;
						ExpectedBytes = 2;
						DELAYED_SERIN_IRQ += SERIN_INTERVAL + ACK_INTERVAL;
						TransferStatus = SIO_FinalStatus;
						/* printf("Sector written, result= %x.\n",result); */
					}
					else
						TransferStatus = SIO_NoFrame;
				}
				else {
					DataBuffer[0] = 'E';
					DataIndex = 0;
					ExpectedBytes = 1;
					DELAYED_SERIN_IRQ += SERIN_INTERVAL + ACK_INTERVAL;
					TransferStatus = SIO_FinalStatus;
				}
			}
		}
		else {
#ifdef WIN32
			ADDLOGTEXT( "Invalid data frame!" );
#else
			printf("Invalid data frame!\n");
#endif
		}
		break;
	default:
#ifdef WIN32
		{
		sprintf( scratch, "Unexpected data output :%x\n", byte);
		ADDLOG( scratch );
		}
#else
		printf("Unexpected data output :%x\n", byte);
#endif
	}
	DELAYED_SEROUT_IRQ += SEROUT_INTERVAL;

}

/* Get a byte from the floppy to the pokey. */

int SIO_GetByte(void)
{
	int byte = 0;

	switch (TransferStatus) {
	case SIO_StatusRead:
		byte = 'A';				/* Command acknoledged */
		/* printf("Command status read\n"); */
		Command_Frame();		/* Handle now the command */
		break;
	case SIO_FormatFrame:
		TransferStatus = SIO_ReadFrame;
		DELAYED_SERIN_IRQ += SERIN_INTERVAL << 3;
	case SIO_ReadFrame:
		if (DataIndex < ExpectedBytes) {
			byte = DataBuffer[DataIndex++];
			if (DataIndex >= ExpectedBytes) {
				TransferStatus = SIO_NoFrame;
#ifdef SET_LED
				Atari_Set_LED(0);
#endif
				/* printf("Transfer complete.\n"); */
			}
			else {
				DELAYED_SERIN_IRQ += SERIN_INTERVAL;
			}
		}
		else {
#ifdef WIN32
			ADDLOG("Invalid read frame!");
#else
			printf("Invalid read frame!\n");
#endif
			TransferStatus = SIO_NoFrame;
		}
		break;
	case SIO_FinalStatus:
		if (DataIndex < ExpectedBytes) {
			byte = DataBuffer[DataIndex++];
			if (DataIndex >= ExpectedBytes) {
				TransferStatus = SIO_NoFrame;
#ifdef SET_LED
				Atari_Set_LED(0);
#endif
				/* printf("Write complete.\n"); */
			}
			else {
				if (DataIndex == 0)
					DELAYED_SERIN_IRQ += SERIN_INTERVAL + ACK_INTERVAL;
				else
					DELAYED_SERIN_IRQ += SERIN_INTERVAL;
			}
		}
		else {
#ifdef WIN32
			ADDLOG("Invalid read frame!");
#else
			printf("Invalid read frame!\n");
#endif
#ifdef SET_LED
			Atari_Set_LED(0);
#endif
			TransferStatus = SIO_NoFrame;
		}
		break;
	default:
#ifdef WIN32
		ADDLOG("Unexpected data reading!\n");
#else
		printf("Unexpected data reading!\n");
#endif
		break;
	}

	return byte;
}
void CopyFromMem(ATPtr from, UBYTE * to, int size)
{
	memcpy(to, from + memory, size);
}

extern UBYTE attrib[65536];
void CopyToMem(UBYTE * from, ATPtr to, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (!attrib[to])
			Poke(to, *from);
		from++, to++;
	}
}
