#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<fcntl.h>
#include	<ctype.h>

#ifdef VMS
#include	<file.h>
#endif

#include	"system.h"

Machine	machine = Atari;

main (int argc, char **argv)
{
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
		else if (strcmp(argv[i], "-xl") == 0)
		{
			machine = AtariXL;
		}
		else if (strcmp(argv[i], "-xe") == 0)
		{
			machine = AtariXE;
		}
		else
		{
			argv[j++] = argv[i];
		}
	}
/*
	=========================
	Invoke requested emulator
	=========================
*/
	switch (machine)
	{
		case Atari :
		case AtariXL :
		case AtariXE :
			atari_main (j, argv);
			break;
		default :
			printf ("Usage: %s [-atari] [-xl] [-xe] [-help]\n", argv[0]);
			break;
	}
}
