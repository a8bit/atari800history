#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<fcntl.h>
#include	<ctype.h>

#ifdef VMS
#include	<file.h>
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
	freopen ("error.log", "w", stderr);
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
