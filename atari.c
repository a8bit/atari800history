#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<fcntl.h>
#include	<ctype.h>
#include	<signal.h>

#ifdef VMS
#include	<file.h>
#endif

#define FALSE   0
#define TRUE    1

#include	"system.h"
#include	"cpu.h"
#include	"atari.h"
#include	"atari_h_device.h"

static int	enable_c000 = FALSE;
static int	enable_basic = FALSE;

extern int	countdown;

void Atari800_OS ();
void Atari800_ESC (UBYTE code);

void sigint_handler ()
{
	int	diskno;

#ifdef BASIC
	printf ("*** break ***\n");

	if (monitor () == 1)
	{
		signal (SIGINT, sigint_handler);
		return;
	}
#endif

#ifdef X11
	printf ("*** break ***\n");

	if (monitor () == 1)
	{
		signal (SIGINT, sigint_handler);
		return;
	}
#endif

	for (diskno=1;diskno<8;diskno++)
		SIO_Dismount (diskno);

	fclose (stderr);

#ifdef X11
	Atari800_Exit ();
#endif

	exit (0);
}

atari_main (int argc, char **argv)
{
	int     error;
	int	diskno = 1;
	int     i;

	Atari800_Initialise ();

	printf ("Atari 800 Emulator\n");

	signal (SIGINT, sigint_handler);

	error = FALSE;

	for (i=1;i<argc;i++)
	{
		if (*argv[i] == '-')
		{
			switch (*(argv[i]+1))
			{
				case '1' :
					countdown = 10000;
					break;
				case '2' :
					countdown = 20000;
					break;
				case '3' :
					countdown = 30000;
					break;
				case '4' :
					countdown = 40000;
					break;
				case '5' :
					countdown = 50000;
					break;
				case '6' :
					countdown = 60000;
					break;
				case '7' :
					countdown = 70000;
					break;
				case '8' :
					countdown = 80000;
					break;
				case '9' :
					countdown = 90000;
					break;
				case 'b' :
					enable_basic = TRUE;
					break;
				case 'c' :
					enable_c000 = TRUE;
					break;
				case 'h' :
					h_prefix = (char*)(argv[i]+2);
					break;
				default :
					error = TRUE;
					break;
			}
		}
		else
		{
			if (!SIO_Mount (diskno++, argv[i]))
			{
				printf ("Disk File %s not found\n", argv[i]);
			}
		}
	}

	if (error)
	{
		printf ("Usage: %s\n", argv[0]);
		exit (1);
	}

	Atari800_OS ();
}

void Atari800_OS ()
{
	const unsigned short	o_open = 0;
	const unsigned short	o_close = 2;
	const unsigned short	o_read = 4;
	const unsigned short	o_write = 6;
	const unsigned short	o_status = 8;
	const unsigned short	o_special = 10;
	const unsigned short	o_init = 12;

	unsigned short	addr;
	unsigned short	entry;
	unsigned short	devtab;

	int	status;
/*
	======================================================
	Load Floating Point Package, Font and Operating System
	======================================================
*/
	status = load_image ("object/atariosb.rom", 65536-10240);
	if (!status)
	{
		printf ("Unable to load object/atariosb.rom\n");
		exit (1);
	}

	add_esc (0xe459, ESC_SIO);
/*
	==========================================
	Patch O.S. - Modify Handler Table (HATABS)
	==========================================
*/
	for (addr=0xf0e3;addr<0xf0f2;addr+=3)
	{
		devtab = (GetByte(addr+2) << 8) | GetByte(addr+1);

		switch (GetByte(addr))
		{
			case 'P' :
				printf ("Printer Device\n");
				break;
			case 'C' :
				printf ("Cassette Device changed to Host Device (H:)\n");
				PutByte (addr, 'H');
				entry = GetWord (devtab + o_open);
				add_esc (entry+1, ESC_H_OPEN);
				entry = GetWord (devtab + o_close);
				add_esc (entry+1, ESC_H_CLOSE);
				entry = GetWord (devtab + o_read);
				add_esc (entry+1, ESC_H_READ);
				entry = GetWord (devtab + o_write);
				add_esc (entry+1, ESC_H_WRITE);
				entry = GetWord (devtab + o_status);
				add_esc (entry+1, ESC_H_STATUS);
/*
	======================================
	Cannot change the special offset since
	it only points to an RTS instruction.
	======================================
*/
/*
				entry = GetWord (devtab + o_special);
				add_esc (entry+1, ESC_H_SPECIAL);
*/
				break;
			case 'E' :
#ifdef BASIC
				printf ("Editor Device\n");
				entry = GetWord (devtab + o_open);	/* Get Address of Editor Open Routine */
				add_esc (entry+1, ESC_E_OPEN);		/* Replace Editor Open */
				entry = GetWord (devtab + o_read);	/* Get Address of Editor Read Routine */
				add_esc (entry+1, ESC_E_READ);	/* Replace Editor Read */
				entry = GetWord (devtab + o_write);	/* Get Address of Editor Write Routine */
				add_esc (entry+1, ESC_E_WRITE);	/* Replace Editor Write */
#endif
				break;
			case 'S' :
				printf ("Screen Device\n");
				break;
			case 'K' :
#ifdef BASIC
				printf ("Keyboard Device\n");
/*
				entry = GetWord (devtab + o_open);
				add_esc (entry+1, ESC_K_OPEN);
				entry = GetWord (devtab + o_close);
				add_esc (entry+1, ESC_K_CLOSE);
*/
				entry = GetWord (devtab + o_read);
				add_esc (entry+1, ESC_K_READ);
/*
				entry = GetWord (devtab + o_write);
				add_esc (entry+1, ESC_K_WRITE);
				entry = GetWord (devtab + o_status);
				add_esc (entry+1, ESC_K_STATUS);
*/
/*
	======================================
	Cannot change the special offset since
	it only points to an RTS instruction.
	======================================
*/
/*
				entry = GetWord (devtab + o_special);
				add_esc (entry+1, ESC_K_SPECIAL);
*/
#endif
				break;
			default :
				break;
		}
	}
/*
	======================
	Set O.S. Area into ROM
	======================
*/
	SetROM (0xd800, 0xffff);
	SetHARDWARE (0xd000, 0xd7ff);

	if (!enable_c000)
		SetROM (0xc000, 0xcfff);

	if (enable_basic)
	{
		status = load_image ("object/ataribas.rom", 0xa000);
		if (!status)
		{
			printf ("Unable to load object/ataribas.rom\n");
			exit (1);
		}

		SetROM (0xa000, 0xbfff);
	}

	Escape = Atari800_ESC;

	CPU_Reset ();

	if (GO (TRUE) == CPU_STATUS_ERR)
		monitor ();
}

void add_esc (UWORD address, UBYTE esc_code)
{
	PutByte (address, 0xff);	/* ESC */
	PutByte (address+1, esc_code);	/* ESC CODE */
	PutByte (address+2, 0x60);	/* RTS */
}

int load_image (char *filename, int addr)
{
	int	status = FALSE;
	int	fd;

	fd = open (filename, O_RDONLY);
	if (fd != -1)
	{
		char	ch;

		while (read (fd, &ch, 1) == 1)
		{
			PutByte (addr, ch);
			addr++;
		}

		close (fd);

		status = TRUE;
	}

	return status;
}

/*
	================================
	N = 0 : I/O Successful and Y = 1
	N = 1 : I/O Error and Y = error#
	================================
*/

static CPU_Status	cpu_status;

K_Device (UBYTE esc_code)
{
	char	ch;

	CPU_GetStatus (&cpu_status);

	switch (esc_code)
	{
		case ESC_K_OPEN :
		case ESC_K_CLOSE :
			cpu_status.Y = 1;
			cpu_status.flag.N = 0;
			break;
		case ESC_K_WRITE :
		case ESC_K_STATUS :
		case ESC_K_SPECIAL :
			cpu_status.Y = 146;
			cpu_status.flag.N = 1;
			break;
		case ESC_K_READ :
			ch = getchar();
			switch (ch)
			{
				case '\n' :
					ch = 0x9b;
					break;
				default :
					break;
			}
			cpu_status.A = ch;
			cpu_status.Y = 1;
			cpu_status.flag.N = 0;
			break;
	}

	CPU_PutStatus (&cpu_status);
}

E_Device (UBYTE esc_code)
{
	UBYTE	ch;

	CPU_GetStatus (&cpu_status);

	switch (esc_code)
	{
		case ESC_E_OPEN :
			printf ("Editor Open\n");
			cpu_status.Y = 1;
			cpu_status.flag.N = 0;
			break;
		case ESC_E_READ :
			ch = getchar();
			switch (ch)
			{
				case '\n' :
					ch = 0x9b;
					break;
				default :
					break;
			}
			cpu_status.A = ch;
			cpu_status.Y = 1;
			cpu_status.flag.N = 0;
			break;
		case ESC_E_WRITE :
			ch = cpu_status.A;
			switch (ch)
			{
				case 0x7d :
					putchar ('*');
					break;
				case 0x9b :
					putchar ('\n');
					break;
				default :
					putchar (ch & 0x7f);
					break;
			}
			cpu_status.Y = 1;
			cpu_status.flag.N = 0;
			break;
	}

	CPU_PutStatus (&cpu_status);
}

void Atari800_ESC (UBYTE esc_code)
{
	switch (esc_code)
	{
		case ESC_SIO :
			SIO ();
			break;
		case ESC_K_OPEN :
		case ESC_K_CLOSE :
		case ESC_K_READ :
		case ESC_K_WRITE :
		case ESC_K_STATUS :
		case ESC_K_SPECIAL :
			K_Device (esc_code);
			break;
		case ESC_H_OPEN :
		case ESC_H_CLOSE :
		case ESC_H_READ :
		case ESC_H_WRITE :
		case ESC_H_STATUS :
		case ESC_H_SPECIAL :
			H_Device (esc_code);
			break;
		case ESC_E_OPEN :
		case ESC_E_READ :
		case ESC_E_WRITE :
			E_Device (esc_code);
			break;
		default         :
			printf ("Invalid ESC Code %x at Address\n",
				esc_code, cpu_status.PC);
			exit (1);
	}
}
