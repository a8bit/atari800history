#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <setjmp.h>

#include "atari.h"
#include "prompts.h"

static char *rcsid = "$Id: configure.c,v 1.20 1998/02/21 14:56:45 david Exp $";

jmp_buf jmpbuf;

void bus_err()
{
	longjmp(jmpbuf, 1);
}

int unaligned_long_ok()
{
#ifdef DJGPP
	return 1;
#else
	long l[2];

	if (setjmp(jmpbuf) == 0) {
		signal(SIGBUS, bus_err);
		*((long *) ((char *) l + 1)) = 1;
		signal(SIGBUS, SIG_DFL);
		return 1;
	}
	else {
		signal(SIGBUS, SIG_DFL);
		return 0;
	}
#endif
}

int main(void)
{
	FILE *fp;
	char config_filename[256];
	char *home;

	char config_version[256];
	char linux_joystick = 'N';
	char joymouse = 'N';
	char voxware = 'N';
	int allow_unaligned_long = 0;

	home = getenv("~");
	if (!home)
		home = getenv("HOME");
	if (!home)
		home = ".";

#ifndef DJGPP
	sprintf(config_filename, "%s/.atari800", home);
#else
	sprintf(config_filename, "%s/atari800.djgpp", home);
#endif

	fp = fopen(config_filename, "r");
	if (fp) {
		printf("\nReading: %s\n\n", config_filename);

		fgets(config_version, 256, fp);
		RemoveLF(config_version);

		if (strcmp(ATARI_TITLE, config_version) == 0) {
			if (fscanf(fp, "\n%c", &linux_joystick) == 0)
				linux_joystick = 'N';

			if (fscanf(fp, "\n%c", &joymouse) == 0)
				joymouse = 'N';

			if (fscanf(fp, "\n%c\n", &voxware) == 0)
				voxware = 'N';
		}
		else {
			printf("Cannot use this configuration file\n");
		}

		fclose(fp);
	}
	YesNo("Enable LINUX Joystick [%c] ", &linux_joystick);
	YesNo("Support for Toshiba Joystick Mouse (Linux SVGALIB Only) [%c] ", &joymouse);
	YesNo("Enable Voxware Sound Support (Linux) [%c] ", &voxware);

	printf("Testing unaligned long accesses...");
	if ((allow_unaligned_long = unaligned_long_ok())) {
		printf("OK\n");
	}
	else {
		printf("not OK\n");
	}

	fp = fopen("config.h", "w");
	if (fp) {
		fprintf(fp, "#ifndef __CONFIG__\n");
		fprintf(fp, "#define __CONFIG__\n");

		if (linux_joystick == 'Y')
			fprintf(fp, "#define LINUX_JOYSTICK\n");

		if (joymouse == 'Y')
			fprintf(fp, "#define JOYMOUSE\n");

		if (voxware == 'Y')
			fprintf(fp, "#define VOXWARE\n");

		if (allow_unaligned_long == 1)
			fprintf(fp, "#define UNALIGNED_LONG_OK\n");

		fprintf(fp, "#endif\n");

		fclose(fp);
	}
	fp = fopen(config_filename, "w");
	if (fp) {
		printf("\nWriting: %s\n\n", config_filename);

		fprintf(fp, "%s\n", ATARI_TITLE);
		fprintf(fp, "%c\n", linux_joystick);
		fprintf(fp, "%c\n", joymouse);
		fprintf(fp, "%c\n", voxware);

		fclose(fp);
	}
	else {
		perror(config_filename);
		exit(1);
	}

	return 0;
}
