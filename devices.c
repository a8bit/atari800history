
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

static char *rcsid = "$Id: devices.c,v 1.21 1998/02/21 15:17:44 david Exp $";

#define FALSE   0
#define TRUE    1

#include "atari.h"
#include "cpu.h"
#include "devices.h"
#include "rt-config.h"

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
	atari_h1_dir,
	atari_h2_dir,
	atari_h3_dir,
	atari_h4_dir
};

static int devbug = FALSE;

static FILE *fp[8] =
{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static int flag[8];

static int fid;
static char filename[64];

#ifdef DO_DIR
static DIR *dp = NULL;
#endif

char *strtoupper(char *str)
{
	char *ptr;
	for (ptr = str; *ptr; ptr++)
		*ptr = toupper(*ptr);

	return str;
}

void Device_Initialise(int *argc, char *argv[])
{
	int i;
	int j;

	for (i = j = 1; i < *argc; i++) {
		if (strncmp(argv[i], "-H1", 2) == 0)
			H[1] = argv[i] + 2;
		else if (strncmp(argv[i], "-H2", 2) == 0)
			H[2] = argv[i] + 2;
		else if (strncmp(argv[i], "-H3", 2) == 0)
			H[3] = argv[i] + 2;
		else if (strncmp(argv[i], "-H4", 2) == 0)
			H[4] = argv[i] + 2;
		else if (strcmp(argv[i], "-devbug") == 0)
			devbug = TRUE;
		else
			argv[j++] = argv[i];
	}

	if (devbug)
		for (i = 0; i < 5; i++)
			printf("H%d: %s\n", i, H[i]);

	*argc = j;
}

int Device_isvalid(char ch)
{
	int valid;

	if (isalnum(ch))
		valid = TRUE;
	else
		switch (ch) {
		case ':':
		case '.':
		case '_':
		case '*':
		case '?':
			valid = TRUE;
			break;
		default:
			valid = FALSE;
			break;
		}

	return valid;
}

void Device_GetFilename()
{
	int bufadr;
	int offset = 0;
	int devnam = TRUE;

	bufadr = (memory[ICBAHZ] << 8) | memory[ICBALZ];

	while (Device_isvalid(memory[bufadr])) {
		int byte = memory[bufadr];

		if (!devnam) {
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

int match(char *pattern, char *filename)
{
	int status = TRUE;

	while (status && *filename && *pattern) {
		switch (*pattern) {
		case '?':
			pattern++;
			filename++;
			break;
		case '*':
			if (*filename == pattern[1]) {
				pattern++;
			}
			else {
				filename++;
			}
			break;
		default:
			status = (*pattern++ == *filename++);
			break;
		}
	}
	if ((*filename)
		|| ((*pattern)
			&& (((*pattern != '*') && (*pattern != '?'))
				|| pattern[1]))) {
		status = 0;
	}
	return status;
}

void Device_HHOPEN(void)
{
	char fname[128];
	int devnum;
	int temp;

	if (devbug)
		printf("HHOPEN\n");

	fid = memory[0x2e] >> 4;

	if (fp[fid]) {
		fclose(fp[fid]);
		fp[fid] = NULL;
	}
/*
   flag[fid] = (memory[ICDNOZ] == 2) ? TRUE : FALSE;
 */
	Device_GetFilename();
/*
   if (memory[ICDNOZ] == 0)
   sprintf (fname, "./%s", filename);
   else
   sprintf (fname, "%s/%s", h_prefix, filename);
 */
	devnum = memory[ICDNOZ];
	if (devnum > 9) {
		printf("Attempt to access H%d: device\n", devnum);
		exit(1);
	}
	if (devnum >= 5) {
		flag[fid] = TRUE;
		devnum -= 5;
	}
	else {
		flag[fid] = FALSE;
	}

#ifdef VMS
/* Assumes H[devnum] is a directory _logical_, not an explicit directory
   specification! */
	sprintf(fname, "%s:%s", H[devnum], filename);
#else
	sprintf(fname, "%s/%s", H[devnum], filename);
#endif

	temp = memory[ICAX1Z];

	switch (temp) {
	case 4:
		fp[fid] = fopen(fname, "rb");
		if (fp[fid]) {
			regY = 1;
			ClrN;
		}
		else {
			regY = 170;
			SetN;
		}
		break;
	case 6:
	case 7:
#ifdef DO_DIR
		fp[fid] = tmpfile();
		if (fp[fid]) {
			dp = opendir(H[devnum]);
			if (dp) {
				struct dirent *entry;

				while ((entry = readdir(dp))) {
					if (match(filename, entry->d_name))
						fprintf(fp[fid], "%s\n", strtoupper(entry->d_name));
				}

				closedir(dp);

				regY = 1;
				ClrN;

				rewind(fp[fid]);

				flag[fid] = TRUE;
			}
			else {
				regY = 163;
				SetN;
				fclose(fp[fid]);
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
	case 8:
		fp[fid] = fopen(fname, "wb");
		if (fp[fid]) {
			regY = 1;
			ClrN;
		}
		else {
			regY = 170;
			SetN;
		}
		break;
	default:
		regY = 163;
		SetN;
		break;
	}
}

void Device_HHCLOS(void)
{
	if (devbug)
		printf("HHCLOS\n");

	fid = memory[0x2e] >> 4;

	if (fp[fid]) {
		fclose(fp[fid]);
		fp[fid] = NULL;
	}
	regY = 1;
	ClrN;
}

void Device_HHREAD(void)
{
	if (devbug)
		printf("HHREAD\n");

	fid = memory[0x2e] >> 4;

	if (fp[fid]) {
		int ch;

		ch = fgetc(fp[fid]);
		if (ch != EOF) {
			if (flag[fid]) {
				switch (ch) {
				case '\n':
					ch = 0x9b;
					break;
				default:
					break;
				}
			}
			regA = ch;
			regY = 1;
			ClrN;
		}
		else {
			regY = 136;
			SetN;
		}
	}
	else {
		regY = 163;
		SetN;
	}
}

void Device_HHWRIT(void)
{
	if (devbug)
		printf("HHWRIT\n");

	fid = memory[0x2e] >> 4;

	if (fp[fid]) {
		int ch;

		ch = regA;
		if (flag[fid]) {
			switch (ch) {
			case 0x9b:
				ch = 0x0a;
				break;
			default:
				break;
			}
		}
		fputc(ch, fp[fid]);
		regY = 1;
		ClrN;
	}
	else {
		regY = 163;
		SetN;
	}
}

void Device_HHSTAT(void)
{
	if (devbug)
		printf("HHSTAT\n");

	fid = memory[0x2e] >> 4;

	regY = 146;
	SetN;
}

void Device_HHSPEC(void)
{
	if (devbug)
		printf("HHSPEC\n");

	fid = memory[0x2e] >> 4;

	switch (memory[ICCOMZ]) {
	case 0x20:
		printf("RENAME Command\n");
		break;
	case 0x21:
		printf("DELETE Command\n");
		break;
	case 0x23:
		printf("LOCK Command\n");
		break;
	case 0x24:
		printf("UNLOCK Command\n");
		break;
	case 0x25:
		printf("NOTE Command\n");
		break;
	case 0x26:
		printf("POINT Command\n");
		break;
	case 0xFE:
		printf("FORMAT Command\n");
		break;
	default:
		printf("UNKNOWN Command\n");
		break;
	}

	regY = 146;
	SetN;
}

void Device_HHINIT(void)
{
	if (devbug)
		printf("HHINIT\n");
}

static int phfd = -1;
void Device_PHCLOS(void);
static char *spool_file = NULL;

void Device_PHOPEN(void)
{
	if (devbug)
		printf("PHOPEN\n");

	if (phfd != -1)
		Device_PHCLOS();

	spool_file = tmpnam(NULL);
	phfd = open(spool_file, O_CREAT | O_TRUNC | O_WRONLY, 0777);
	if (phfd != -1) {
		regY = 1;
		ClrN;
	}
	else {
		regY = 130;
		SetN;
	}
}

void Device_PHCLOS(void)
{
	if (devbug)
		printf("PHCLOS\n");

	if (phfd != -1) {
		char command[256];
		int status;

		close(phfd);

		sprintf(command, print_command, spool_file);
		system(command);

#ifndef VMS
		status = unlink(spool_file);
		if (status == -1) {
			perror(spool_file);
			exit(1);
		}
#endif

		phfd = -1;
	}
	regY = 1;
	ClrN;
}

void Device_PHREAD(void)
{
	if (devbug)
		printf("PHREAD\n");

	regY = 146;
	SetN;
}

void Device_PHWRIT(void)
{
	unsigned char byte;
	int status;

	if (devbug)
		printf("PHWRIT\n");

	byte = regA;
	if (byte == 0x9b)
		byte = '\n';

	status = write(phfd, &byte, 1);
	if (status == 1) {
		regY = 1;
		ClrN;
	}
	else {
		regY = 144;
		SetN;
	}
}

void Device_PHSTAT(void)
{
	if (devbug)
		printf("PHSTAT\n");
}

void Device_PHSPEC(void)
{
	if (devbug)
		printf("PHSPEC\n");

	regY = 1;
	ClrN;
}

void Device_PHINIT(void)
{
	if (devbug)
		printf("PHINIT\n");

	phfd = -1;
	regY = 1;
	ClrN;
}
