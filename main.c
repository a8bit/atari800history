#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<fcntl.h>
#include	<ctype.h>

#ifdef VMS
#include	<file.h>
#endif

#include	"system.h"

#ifdef CURSES
int	curses_mode;
#endif

main (int argc, char **argv)
{
	enum
	{
		Atari
	} machine = Atari;

	int     i;
	int	j;
/*
	==============================================
	Parse command line to determine emulation mode
	Used arguments are removed prior to calling
	the required 
	==============================================
*/
	for (i=j=1;i<argc;i++)
	{
		if (strcmp(argv[i], "-atari") == 0)
		{
			machine = Atari;
		}
#ifdef CURSES
		else if (strcmp(argv[i], "-left") == 0)
			curses_mode = CURSES_LEFT;
		else if (strcmp(argv[i], "-central") == 0)
			curses_mode = CURSES_CENTRAL;
		else if (strcmp(argv[i], "-right") == 0)
			curses_mode = CURSES_RIGHT;
		else if (strcmp(argv[i], "-wide1") == 0)
			curses_mode = CURSES_WIDE_1;
		else if (strcmp(argv[i], "-wide2") == 0)
			curses_mode = CURSES_WIDE_2;
#endif
		else
		{
			argv[j++] = argv[i];
		}
	}
/*
	===============================
	Attach stderr to error.log file
	===============================
*/
#ifndef SVGALIB
	freopen ("error.log", "w", stderr);
#endif
/*
	=========================
	Invoke requested emulator
	=========================
*/
	switch (machine)
	{
		case Atari :
			atari_main (j, argv);
			break;
		default :
			printf ("Usage: %s [-atari] [-help]\n");
			break;
	}
}
