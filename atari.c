#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef VMS
#include <unixio.h>
#include <file.h>
#else
#include <fcntl.h>
#endif

#ifdef DJGPP
#include "djgpp.h"
int timesync = 1;
#endif

#ifdef USE_DOSSOUND
#include <audio.h>
#endif

#ifdef AT_USE_ALLEGRO
#include <allegro.h>
static int i_love_bill = TRUE;	/* Perry, why this? */
#endif

static char *rcsid = "$Id: atari.c,v 1.48 1998/02/21 15:22:04 david Exp $";

#define FALSE   0
#define TRUE    1

#include "atari.h"
#include "cpu.h"
#include "antic.h"
#include "gtia.h"
#include "pia.h"
#include "pokey.h"
#include "supercart.h"
#include "devices.h"
#include "sio.h"
#include "monitor.h"
#include "platform.h"
#include "prompts.h"
#include "rt-config.h"

TVmode tv_mode = PAL;
Machine machine = Atari;
int mach_xlxe = FALSE;
int verbose = FALSE;
double fps;
int nframes;
ULONG lastspeed;				/* measure time between two Antic runs */

static int os = 2;
static int pil_on = FALSE;

extern UBYTE NMIEN;
extern UBYTE NMIST;
extern UBYTE PORTA;
extern UBYTE PORTB;

extern int xe_bank;
extern int selftest_enabled;
extern int Ram256;
extern int rom_inserted;
extern UBYTE atari_basic[8192];
extern UBYTE atarixl_os[16384];

UBYTE *cart_image = NULL;		/* For cartridge memory */
int cart_type = NO_CART;
int DELAYED_SERIN_IRQ;
int DELAYED_SEROUT_IRQ;
int DELAYED_XMTDONE_IRQ;
int countdown_rate = 4000;
double deltatime;

#ifdef BACKUP_MSG
char backup_msg_buf[512];
#endif

void add_esc(UWORD address, UBYTE esc_code);
int Atari800_Exit(int run_monitor);
void Atari800_Hardware(void);

int load_cart(char *filename, int type);

static char *rom_filename = NULL;

void sigint_handler(int num)
{
	int restart;
	int diskno;

	restart = Atari800_Exit(TRUE);
	if (restart) {
		signal(SIGINT, sigint_handler);
		return;
	}
	for (diskno = 1; diskno < 8; diskno++)
		SIO_Dismount(diskno);

	exit(0);
}


int load_image(char *filename, int addr, int nbytes)
{
	int status = FALSE;
	int fd;

	fd = open(filename, O_RDONLY, 0777);
	if (fd != -1) {
		status = read(fd, &memory[addr], nbytes);
		if (status != nbytes) {
#ifdef BACKUP_MSG
			sprintf(backup_msg_buf, "Error reading %s\n", filename);
#else
			printf("Error reading %s\n", filename);
#endif
			Atari800_Exit(FALSE);
			exit(1);
		}
		close(fd);

		status = TRUE;
	}
	else if (verbose) {
		perror(filename);
	}
	return status;
}

void EnablePILL(void)
{
	if (os != 3) {				/* Disable PIL when running Montezumma's Revenge */
		SetROM(0x8000, 0xbfff);
		pil_on = TRUE;
	}
}

void DisablePILL(void)
{
	if (os != 3) {				/* Disable PIL when running Montezumma's Revenge */
		SetRAM(0x8000, 0xbfff);
		pil_on = FALSE;
	}
}

/*
 * Load a standard 8K ROM from the specified file
 */

int Insert_8K_ROM(char *filename)
{
	int status = FALSE;
	int fd;

	fd = open(filename, O_RDONLY, 0777);
	if (fd != -1) {
		read(fd, &memory[0xa000], 0x2000);
		close(fd);
		SetRAM(0x8000, 0x9fff);
		SetROM(0xa000, 0xbfff);
		cart_type = NORMAL8_CART;
		rom_inserted = TRUE;
		status = TRUE;
	}
	return status;
}

/*
 * Load a standard 16K ROM from the specified file
 */

int Insert_16K_ROM(char *filename)
{
	int status = FALSE;
	int fd;

	fd = open(filename, O_RDONLY, 0777);
	if (fd != -1) {
		read(fd, &memory[0x8000], 0x4000);
		close(fd);
		SetROM(0x8000, 0xbfff);
		cart_type = NORMAL16_CART;
		rom_inserted = TRUE;
		status = TRUE;
	}
	return status;
}

/*
 * Load an OSS Supercartridge from the specified file
 * The OSS cartridge is a 16K bank switched cartridge
 * that occupies 8K of address space between $a000
 * and $bfff
 */

int Insert_OSS_ROM(char *filename)
{
	int status = FALSE;
	int fd;

	fd = open(filename, O_RDONLY, 0777);
	if (fd != -1) {
		cart_image = (UBYTE *) malloc(0x4000);
		if (cart_image) {
			read(fd, cart_image, 0x4000);
			memcpy(&memory[0xa000], cart_image, 0x1000);
			memcpy(&memory[0xb000], cart_image + 0x3000, 0x1000);
			SetRAM(0x8000, 0x9fff);
			SetROM(0xa000, 0xbfff);
			cart_type = OSS_SUPERCART;
			rom_inserted = TRUE;
			status = TRUE;
		}
		close(fd);
	}
	return status;
}

/*
 * Load a DB Supercartridge from the specified file
 * The DB cartridge is a 32K bank switched cartridge
 * that occupies 16K of address space between $8000
 * and $bfff
 */

int Insert_DB_ROM(char *filename)
{
	int status = FALSE;
	int fd;

	fd = open(filename, O_RDONLY, 0777);
	if (fd != -1) {
		cart_image = (UBYTE *) malloc(0x8000);
		if (cart_image) {
			read(fd, cart_image, 0x8000);
			memcpy(&memory[0x8000], cart_image, 0x2000);
			memcpy(&memory[0xa000], cart_image + 0x6000, 0x2000);
			SetROM(0x8000, 0xbfff);
			cart_type = DB_SUPERCART;
			rom_inserted = TRUE;
			status = TRUE;
		}
		close(fd);
	}
	return status;
}

/*
 * Load a 32K 5200 ROM from the specified file
 */

int Insert_32K_5200ROM(char *filename)
{
	int status = FALSE;
	int fd;

	fd = open(filename, O_RDONLY, 0777);
	if (fd != -1) {
		/* read the first 16k */
		if (read(fd, &memory[0x4000], 0x4000) != 0x4000) {
			close(fd);
			return FALSE;
		}
		/* try and read next 16k */
		cart_type = AGS32_CART;
		if ((status = read(fd, &memory[0x8000], 0x4000)) == 0) {
			/* note: AB__ ABB_ ABBB AABB */
			memcpy(&memory[0x8000], &memory[0x6000], 0x2000);
			memcpy(&memory[0xA000], &memory[0x6000], 0x2000);
			memcpy(&memory[0x6000], &memory[0x4000], 0x2000);
		}
		else if (status != 0x4000) {
			close(fd);
			printf("%X", status);
			exit(0);
			return FALSE;
		}
		else {
			UBYTE temp_byte;
			if (read(fd, &temp_byte, 1) == 1) {
				/* ABCD EFGH IJ */
				if (!(cart_image = (UBYTE *) malloc(0x8000)))
					return FALSE;
				*cart_image = temp_byte;
				if (read(fd, &cart_image[1], 0x1fff) != 0x1fff)
					return FALSE;
				memcpy(&cart_image[0x2000], &memory[0x6000], 0x6000);	/* IJ CD EF GH :CI */

				memcpy(&memory[0xa000], &cart_image[0x0000], 0x2000);	/* AB CD EF IJ :MEM */

				memcpy(&memory[0x8000], &cart_image[0x0000], 0x2000);	/* AB CD IJ IJ :MEM?ij copy */

				memcpy(&cart_image[0x0000], &memory[0x4000], 0x2000);	/* AB CD EF GH :CI */

				memcpy(&memory[0x5000], &cart_image[0x4000], 0x1000);	/* AE CD IJ IJ :MEM CD dont care? */

				SetHARDWARE(0x4ff6, 0x4ff9);
				SetHARDWARE(0x5ff6, 0x5ff9);
			}
		}
		close(fd);
		/* SetROM (0x4000, 0xbfff); */
		/* cart_type = AGS32_CART; */
		rom_inserted = TRUE;
		status = TRUE;
	}
	return status;
}

/*
 * This removes any loaded cartridge ROM files from the emulator
 * It doesn't remove either the OS, FFP or character set ROMS.
 */

int Remove_ROM(void)
{
	if (cart_image) {			/* Release memory allocated for Super Cartridges */
		free(cart_image);
		cart_image = NULL;
	}
	SetRAM(0x8000, 0xbfff);		/* Ensure cartridge area is RAM */
	cart_type = NO_CART;
	rom_inserted = FALSE;

	return TRUE;
}

int Insert_Cartridge(char *filename)
{
	int status = FALSE;
	int fd;

	fd = open(filename, O_RDONLY, 0777);
	if (fd != -1) {
		UBYTE header[16];

		read(fd, header, sizeof(header));
		if ((header[0] == 'C') &&
			(header[1] == 'A') &&
			(header[2] == 'R') &&
			(header[3] == 'T')) {
			int type;
			int checksum;

			type = (header[4] << 24) |
				(header[5] << 16) |
				(header[6] << 8) |
				header[7];

			checksum = (header[4] << 24) |
				(header[5] << 16) |
				(header[6] << 8) |
				header[7];

			switch (type) {
#define STD_8K 1
#define STD_16K 2
#define OSS 3
#define AGS 4
			case STD_8K:
				read(fd, &memory[0xa000], 0x2000);
				SetRAM(0x8000, 0x9fff);
				SetROM(0xa000, 0xbfff);
				cart_type = NORMAL8_CART;
				rom_inserted = TRUE;
				status = TRUE;
				break;
			case STD_16K:
				read(fd, &memory[0x8000], 0x4000);
				SetROM(0x8000, 0xbfff);
				cart_type = NORMAL16_CART;
				rom_inserted = TRUE;
				status = TRUE;
				break;
			case OSS:
				cart_image = (UBYTE *) malloc(0x4000);
				if (cart_image) {
					read(fd, cart_image, 0x4000);
					memcpy(&memory[0xa000], cart_image, 0x1000);
					memcpy(&memory[0xb000], cart_image + 0x3000, 0x1000);
					SetRAM(0x8000, 0x9fff);
					SetROM(0xa000, 0xbfff);
					cart_type = OSS_SUPERCART;
					rom_inserted = TRUE;
					status = TRUE;
				}
				break;
			case AGS:
				read(fd, &memory[0x4000], 0x8000);
				close(fd);
				SetROM(0x4000, 0xbfff);
				cart_type = AGS32_CART;
				rom_inserted = TRUE;
				status = TRUE;
				break;
			default:
				printf("%s is in unsupported cartridge format %d\n",
					   filename, type);
				break;
			}
		}
		else {
			printf("%s is not a cartridge\n", filename);
		}
		close(fd);
	}
	return status;
}

void PatchOS(void)
{
	const unsigned short o_open = 0;
	const unsigned short o_close = 2;
	const unsigned short o_read = 4;
	const unsigned short o_write = 6;
	const unsigned short o_status = 8;
	/* const unsigned short   o_special = 10; */
	const unsigned short o_init = 12;

	unsigned short addr;
	unsigned short entry;
	unsigned short devtab;
	int i;
/*
   =====================
   Disable Checksum Test
   =====================
 */
	if (enable_sio_patch)
		add_esc(0xe459, ESC_SIOV);

	switch (machine) {
	case Atari:
		break;
	case AtariXL:
	case AtariXE:
	case Atari320XE:
		memory[0xc314] = 0x8e;
		memory[0xc315] = 0xff;
		memory[0xc319] = 0x8e;
		memory[0xc31a] = 0xff;
		break;
	default:
		printf("Fatal Error in atari.c: PatchOS(): Unknown machine\n");
		Atari800_Exit(FALSE);
		exit(1);
		break;
	}
/*
   ==========================================
   Patch O.S. - Modify Handler Table (HATABS)
   ==========================================
 */
	switch (machine) {
	case Atari:
		addr = 0xf0e3;
		break;
	case AtariXL:
	case AtariXE:
	case Atari320XE:
		addr = 0xc42e;
		break;
	default:
		printf("Fatal Error in atari.c: PatchOS(): Unknown machine\n");
		Atari800_Exit(FALSE);
		exit(1);
		break;
	}

	for (i = 0; i < 5; i++) {
		devtab = (memory[addr + 2] << 8) | memory[addr + 1];

		switch (memory[addr]) {
		case 'P':
			entry = (memory[devtab + o_open + 1] << 8) | memory[devtab + o_open];
			add_esc(entry + 1, ESC_PHOPEN);
			entry = (memory[devtab + o_close + 1] << 8) | memory[devtab + o_close];
			add_esc(entry + 1, ESC_PHCLOS);
/*
   entry = (memory[devtab+o_read+1] << 8) | memory[devtab+o_read];
   add_esc (entry+1, ESC_PHREAD);
 */
			entry = (memory[devtab + o_write + 1] << 8) | memory[devtab + o_write];
			add_esc(entry + 1, ESC_PHWRIT);
			entry = (memory[devtab + o_status + 1] << 8) | memory[devtab + o_status];
			add_esc(entry + 1, ESC_PHSTAT);
/*
   entry = (memory[devtab+o_special+1] << 8) | memory[devtab+o_special];
   add_esc (entry+1, ESC_PHSPEC);
 */
			memory[devtab + o_init] = 0xd2;
			memory[devtab + o_init + 1] = ESC_PHINIT;
			break;
		case 'C':
			memory[addr] = 'H';
			entry = (memory[devtab + o_open + 1] << 8) | memory[devtab + o_open];
			add_esc(entry + 1, ESC_HHOPEN);
			entry = (memory[devtab + o_close + 1] << 8) | memory[devtab + o_close];
			add_esc(entry + 1, ESC_HHCLOS);
			entry = (memory[devtab + o_read + 1] << 8) | memory[devtab + o_read];
			add_esc(entry + 1, ESC_HHREAD);
			entry = (memory[devtab + o_write + 1] << 8) | memory[devtab + o_write];
			add_esc(entry + 1, ESC_HHWRIT);
			entry = (memory[devtab + o_status + 1] << 8) | memory[devtab + o_status];
			add_esc(entry + 1, ESC_HHSTAT);
			break;
		case 'E':
#ifdef BASIC
			printf("Editor Device\n");
			entry = (memory[devtab + o_open + 1] << 8) | memory[devtab + o_open];
			add_esc(entry + 1, ESC_E_OPEN);
			entry = (memory[devtab + o_read + 1] << 8) | memory[devtab + o_read];
			add_esc(entry + 1, ESC_E_READ);
			entry = (memory[devtab + o_write + 1] << 8) | memory[devtab + o_write];
			add_esc(entry + 1, ESC_E_WRITE);
#endif
			break;
		case 'S':
			break;
		case 'K':
#ifdef BASIC
			printf("Keyboard Device\n");
			entry = (memory[devtab + o_read + 1] << 8) | memory[devtab + o_read];
			add_esc(entry + 1, ESC_K_READ);
#endif
			break;
		default:
			break;
		}

		addr += 3;				/* Next Device in HATABS */
	}
}

static int Reset_Disabled = FALSE;

void Coldstart(void)
{
	if (os == 3) {
		os = 2;					/* Fiddle OS to prevent infinite recursion */
		Initialise_Monty();
		os = 3;
	}
	NMIEN = 0x00;
	NMIST = 0x00;
	PORTA = 0xff;
	if (mach_xlxe) {
		selftest_enabled = TRUE;	/* necessary for init RAM */
		PORTB = 0x00;			/* necessary for init RAM */
		PIA_PutByte(_PORTB, 0xff);	/* turn on operating system */
	}
	else
		PORTB = 0xff;
	memory[0x244] = 1;
	CPU_Reset();

	if (hold_option)
		next_console_value = 0x03;	/* Hold Option During Reboot */
}

void Warmstart(void)
{
	if (os == 3)
		Initialise_Monty();

	NMIEN = 0x00;
	NMIST = 0x00;
	PORTA = 0xff;
	PORTB = 0xff;
	CPU_Reset();
}

int Initialise_AtariOSA(void)
{
	int status;
	mach_xlxe = FALSE;
	status = load_image(atari_osa_filename, 0xd800, 0x2800);
	if (status) {
		machine = Atari;
		PatchOS();
		SetRAM(0x0000, 0xbfff);
		if (enable_c000_ram)
			SetRAM(0xc000, 0xcfff);
		else
			SetROM(0xc000, 0xcfff);
		SetROM(0xd800, 0xffff);
		SetHARDWARE(0xd000, 0xd7ff);
		Coldstart();
	}
	return status;
}

int Initialise_AtariOSB(void)
{
	int status;
	mach_xlxe = FALSE;
	status = load_image(atari_osb_filename, 0xd800, 0x2800);
	if (status) {
		machine = Atari;
		PatchOS();
		SetRAM(0x0000, 0xbfff);
		if (enable_c000_ram)
			SetRAM(0xc000, 0xcfff);
		else
			SetROM(0xc000, 0xcfff);
		SetROM(0xd800, 0xffff);
		SetHARDWARE(0xd000, 0xd7ff);
		Coldstart();
	}
	return status;
}

int Initialise_AtariXL(void)
{
	int status;
	mach_xlxe = TRUE;
	status = load_image(atari_xlxe_filename, 0xc000, 0x4000);
	if (status) {
		machine = AtariXL;
		PatchOS();
		memcpy(atarixl_os, memory + 0xc000, 0x4000);

		if (cart_type == NO_CART) {
			status = Insert_8K_ROM(atari_basic_filename);
			if (status) {
				memcpy(atari_basic, memory + 0xa000, 0x2000);
				SetRAM(0x0000, 0x9fff);
				SetROM(0xc000, 0xffff);
				SetHARDWARE(0xd000, 0xd7ff);
				rom_inserted = FALSE;
				Coldstart();
			}
			else {
#ifdef BACKUP_MSG
				sprintf(backup_msg_buf, "Unable to load %s\n", atari_basic_filename);
#else
				printf("Unable to load %s\n", atari_basic_filename);
#endif
				Atari800_Exit(FALSE);
				exit(1);
			}
		}
		else {
			SetRAM(0x0000, 0xbfff);
			SetROM(0xc000, 0xffff);
			SetHARDWARE(0xd000, 0xd7ff);
			rom_inserted = FALSE;
			Coldstart();
		}
	}
	return status;
}

int Initialise_AtariXE(void)
{
	int status;
	mach_xlxe = TRUE;
	status = Initialise_AtariXL();
	machine = AtariXE;
	xe_bank = 0;
	return status;
}

int Initialise_Atari320XE(void)
{
	int status;
	mach_xlxe = TRUE;
	status = Initialise_AtariXE();
	machine = Atari320XE;
	return status;
}

int Initialise_Atari5200(void)
{
	int status;
	mach_xlxe = FALSE;
	memset(memory, 0, 0xf800);
	status = load_image(atari_5200_filename, 0xf800, 0x800);
	if (status) {
		machine = Atari5200;
		SetRAM(0x0000, 0x3fff);
		SetROM(0xf800, 0xffff);
		SetROM(0x4000, 0xffff);
		SetHARDWARE(0xc000, 0xc0ff);	/* 5200 GTIA Chip */
		SetHARDWARE(0xd400, 0xd4ff);	/* 5200 ANTIC Chip */
		SetHARDWARE(0xe800, 0xe8ff);	/* 5200 POKEY Chip */
		SetHARDWARE(0xeb00, 0xebff);	/* 5200 POKEY Chip */
		Coldstart();
	}
	return status;
}

/*
 * Initialise System with Montezuma's Revenge
 * image. Standard Atari OS is not required.
 */

#include "emuos.h"

#ifdef __BUILT_IN_MONTY__
#include "monty.h"
#endif

int Initialise_Monty(void)
{
	int status;

	status = load_image("monty-emuos.img", 0x0000, 0x10000);
#ifdef __BUILT_IN_MONTY__
	if (!status) {
		memcpy(&memory[0x0000], monty_h, 0xc000);
		memcpy(&memory[0xc000], emuos_h, 0x4000);
		status = TRUE;
	}
#endif

	if (status) {
		machine = Atari;
		/* PatchOS (); */
		SetRAM(0x0000, 0xbfff);
		if (enable_c000_ram)
			SetRAM(0xc000, 0xcfff);
		else
			SetROM(0xc000, 0xcfff);
		SetROM(0xd800, 0xffff);
		SetHARDWARE(0xd000, 0xd7ff);
		Coldstart();
	}
	return status;
}

/*
 * Initialise System with an replacement OS. It has just
 * enough functionality to run Defender and Star Raider.
 */

int Initialise_EmuOS(void)
{
	int status;

	status = load_image("emuos.img", 0xc000, 0x4000);
	if (!status)
		memcpy(&memory[0xc000], emuos_h, 0x4000);
	else
		printf("EmuOS: Using external emulated OS\n");

	machine = Atari;
	PatchOS();
	SetRAM(0x0000, 0xbfff);
	if (enable_c000_ram)
		SetRAM(0xc000, 0xcfff);
	else
		SetROM(0xc000, 0xcfff);
	SetROM(0xd800, 0xffff);
	SetHARDWARE(0xd000, 0xd7ff);
	Coldstart();

	status = TRUE;

	return status;
}

int main(int argc, char **argv)
{
	int status;
	int error;
	int diskno = 1;
	int i;
	int j;

	char *rtconfig_filename = NULL;
	int config = FALSE;

	error = FALSE;

	for (i = j = 1; i < argc; i++) {
		if (strcmp(argv[i], "-configure") == 0)
			config = TRUE;
		else if (strcmp(argv[i], "-config") == 0)
			rtconfig_filename = argv[++i];
		else if (strcmp(argv[i], "-v") == 0) {
			printf("%s\n", ATARI_TITLE);
			exit(1);
		}
		else if (strcmp(argv[i], "-verbose") == 0)
			verbose = TRUE;
		else
			argv[j++] = argv[i];
	}

	argc = j;

	if (!RtConfigLoad(rtconfig_filename))
		config = TRUE;

	if (config) {
		RtConfigUpdate();
		RtConfigSave();
	}
	switch (default_system) {
	case 1:
		machine = Atari;
		os = 1;
		break;
	case 2:
		machine = Atari;
		os = 2;
		break;
	case 3:
		machine = AtariXL;
		break;
	case 4:
		machine = AtariXE;
		break;
	case 5:
		machine = Atari320XE;
		break;
	case 6:
		machine = Atari5200;
		break;
	default:
		machine = AtariXL;
		break;
	}

	switch (default_tv_mode) {
	case 1:
		tv_mode = PAL;
		break;
	case 2:
		tv_mode = NTSC;
		break;
	default:
		tv_mode = PAL;
		break;
	}

	for (i = j = 1; i < argc; i++) {
		if (strcmp(argv[i], "-atari") == 0)
			machine = Atari;
		else if (strcmp(argv[i], "-xl") == 0)
			machine = AtariXL;
		else if (strcmp(argv[i], "-xe") == 0)
			machine = AtariXE;
		else if (strcmp(argv[i], "-320xe") == 0) {
			machine = Atari320XE;
			if (!Ram256)
				Ram256 = 2;	/* default COMPY SHOP */
		}
		else if (strcmp(argv[i], "-rambo") == 0) {
			machine = Atari320XE;
			Ram256 = 1;
		}
		else if (strcmp(argv[i], "-5200") == 0)
			machine = Atari5200;
		else if (strcmp(argv[i], "-nobasic") == 0)
			hold_option = TRUE;
		else if (strcmp(argv[i], "-nopatch") == 0)
			enable_sio_patch = FALSE;
		else if (strcmp(argv[i], "-pal") == 0)
			tv_mode = PAL;
		else if (strcmp(argv[i], "-ntsc") == 0)
			tv_mode = NTSC;
		else if (strcmp(argv[i], "-osa_rom") == 0)
			strcpy(atari_osa_filename, argv[++i]);
		else if (strcmp(argv[i], "-osb_rom") == 0)
			strcpy(atari_osb_filename, argv[++i]);
		else if (strcmp(argv[i], "-xlxe_rom") == 0)
			strcpy(atari_xlxe_filename, argv[++i]);
		else if (strcmp(argv[i], "-5200_rom") == 0)
			strcpy(atari_5200_filename, argv[++i]);
		else if (strcmp(argv[i], "-basic_rom") == 0)
			strcpy(atari_basic_filename, argv[++i]);
		else if (strcmp(argv[i], "-cart") == 0) {
			rom_filename = argv[++i];
			cart_type = CARTRIDGE;
		}
		else if (strcmp(argv[i], "-rom") == 0) {
			rom_filename = argv[++i];
			cart_type = NORMAL8_CART;
		}
		else if (strcmp(argv[i], "-rom16") == 0) {
			rom_filename = argv[++i];
			cart_type = NORMAL16_CART;
		}
		else if (strcmp(argv[i], "-ags32") == 0) {
			rom_filename = argv[++i];
			cart_type = AGS32_CART;
		}
		else if (strcmp(argv[i], "-oss") == 0) {
			rom_filename = argv[++i];
			cart_type = OSS_SUPERCART;
		}
		else if (strcmp(argv[i], "-db") == 0) {		/* db 16/32 superduper cart */
			rom_filename = argv[++i];
			cart_type = DB_SUPERCART;
		}
		else if (strcmp(argv[i], "-refresh") == 0) {
			sscanf(argv[++i], "%d", &refresh_rate);
			if (refresh_rate < 1)
				refresh_rate = 1;
		}
		else if (strcmp(argv[i], "-help") == 0) {
			printf("\t-configure    Update Configuration File\n");
			printf("\t-config fnm   Specify Alternate Configuration File\n");
			printf("\t-atari        Standard Atari 800 mode\n");
			printf("\t-xl           Atari 800XL mode\n");
			printf("\t-xe           Atari 130XE mode\n");
			printf("\t-320xe        Atari 320XE mode (COMPY SHOP)\n");
			printf("\t-rambo        Atari 320XE mode (RAMBO)\n");
			printf("\t-5200         Atari 5200 Games System\n");
			printf("\t-pal          Enable PAL TV mode\n");
			printf("\t-ntsc         Enable NTSC TV mode\n");
			printf("\t-rom fnm      Install standard 8K Cartridge\n");
			printf("\t-rom16 fnm    Install standard 16K Cartridge\n");
			printf("\t-oss fnm      Install OSS Super Cartridge\n");
			printf("\t-db fnm       Install DB's 16/32K Cartridge (not for normal use)\n");
			printf("\t-refresh num  Specify screen refresh rate\n");
			printf("\t-nopatch      Don't patch SIO routine in OS\n");
			printf("\t-a            Use A OS\n");
			printf("\t-b            Use B OS\n");
			printf("\t-c            Enable RAM between 0xc000 and 0xd000\n");
			printf("\t-v            Show version/release number\n");
			argv[j++] = argv[i];
			printf("\nPress Return/Enter to continue...");
			getchar();
			printf("\r                                 \n");
		}
		else if (strcmp(argv[i], "-a") == 0)
			os = 1;
		else if (strcmp(argv[i], "-b") == 0)
			os = 2;
		else if (strcmp(argv[i], "-monty") == 0) {
			machine = Atari;
			os = 3;
		}
		else if (strcmp(argv[i], "-emuos") == 0) {
			machine = Atari;
			os = 4;
		}
		else if (strcmp(argv[i], "-c") == 0)
			enable_c000_ram = TRUE;
		else
			argv[j++] = argv[i];
	}

	argc = j;

	if (tv_mode == PAL)
		deltatime = (1.0 / 50.0);
	else
		deltatime = (1.0 / 60.0);

	Device_Initialise(&argc, argv);
	SIO_Initialise(&argc, argv);

	Atari_Initialise(&argc, argv);	/* Platform Specific Initialisation */

	if (!atari_screen) {
		atari_screen = (ULONG *) malloc((ATARI_HEIGHT + 16) * ATARI_WIDTH);
		for (i = 0; i < 256; i++)
			colour_translation_table[i] = i;
	}
	/*
	 * Initialise basic 64K memory to zero.
	 */

	memset(memory, 0, 65536);	/* Optimalize by Raster */

	/*
	 * Initialise Custom Chips
	 */

	ANTIC_Initialise(&argc, argv);
	GTIA_Initialise(&argc, argv);
	PIA_Initialise(&argc, argv);

	/*
	 * Initialise Serial Port Interrupts
	 */

	DELAYED_SERIN_IRQ = 0;
	DELAYED_SEROUT_IRQ = 0;
	DELAYED_XMTDONE_IRQ = 0;

	/*
	 * Any parameters left on the command line must be disk images.
	 */

	for (i = 1; i < argc; i++) {
		if (!SIO_Mount(diskno++, argv[i])) {
			printf("Disk File %s not found\n", argv[i]);
			error = TRUE;
		}
	}

	if (error) {
#ifdef BACKUP_MSG
		sprintf(backup_msg_buf, "Usage: %s [-rom filename] [-oss filename] [diskfile1...diskfile8]\n", argv[0]);
		sprintf(backup_msg_buf + strlen(backup_msg_buf), "\t-help         Extended Help\n");
#else
		printf("Usage: %s [-rom filename] [-oss filename] [diskfile1...diskfile8]\n", argv[0]);
		printf("\t-help         Extended Help\n");
#endif
		Atari800_Exit(FALSE);
		exit(1);
	}
	/*
	 * Install CTRL-C Handler
	 */

	signal(SIGINT, sigint_handler);

	/*
	 * Configure Atari System
	 */

	switch (machine) {
	case Atari:
		Ram256 = 0;
		if (os == 1)
			status = Initialise_AtariOSA();
		else if (os == 2)
			status = Initialise_AtariOSB();
		else if (os == 3)
			status = Initialise_Monty();
		else
			status = Initialise_EmuOS();
		break;
	case AtariXL:
		Ram256 = 0;
		status = Initialise_AtariXL();
		break;
	case AtariXE:
		Ram256 = 0;
		status = Initialise_AtariXE();
		break;
	case Atari320XE:
		/* Ram256 is even now set */
		status = Initialise_Atari320XE();
		break;
	case Atari5200:
		Ram256 = 0;
		status = Initialise_Atari5200();
		break;
	default:
		printf("Fatal Error in atari.c\n");
		Atari800_Exit(FALSE);
		exit(1);
	}

	if (!status) {
		printf("Operating System not available - using Emulated OS\n");
		status = Initialise_EmuOS();
	}
/*
 * ================================
 * Install requested ROM cartridges
 * ================================
 */
	if (rom_filename) {
		switch (cart_type) {
		case CARTRIDGE:
			status = Insert_Cartridge(rom_filename);
			break;
		case OSS_SUPERCART:
			status = Insert_OSS_ROM(rom_filename);
			break;
		case DB_SUPERCART:
			status = Insert_DB_ROM(rom_filename);
			break;
		case NORMAL8_CART:
			status = Insert_8K_ROM(rom_filename);
			break;
		case NORMAL16_CART:
			status = Insert_16K_ROM(rom_filename);
			break;
		case AGS32_CART:
			status = Insert_32K_5200ROM(rom_filename);
			break;
		}

		if (status) {
			rom_inserted = TRUE;
		}
		else {
			rom_inserted = FALSE;
		}
	}
	else {
		rom_inserted = FALSE;
	}
/*
 * ======================================
 * Reset CPU and start hardware emulation
 * ======================================
 */

#ifdef DELAYED_VGAINIT
	SetupVgaEnvironment();
#endif

	Atari800_Hardware();
	printf("Fatal error: Atari800_Hardware() returned\n");
	Atari800_Exit(FALSE);
	exit(1);
	return 1;
}

void add_esc(UWORD address, UBYTE esc_code)
{
	memory[address++] = 0xf2;	/* ESC */
	memory[address++] = esc_code;	/* ESC CODE */
	memory[address] = 0x60;		/* RTS */
}

/*
   ================================
   N = 0 : I/O Successful and Y = 1
   N = 1 : I/O Error and Y = error#
   ================================
 */

void K_Device(UBYTE esc_code)
{
	char ch;

	switch (esc_code) {
	case ESC_K_OPEN:
	case ESC_K_CLOSE:
		regY = 1;
		ClrN;
		break;
	case ESC_K_WRITE:
	case ESC_K_STATUS:
	case ESC_K_SPECIAL:
		regY = 146;
		SetN;
		break;
	case ESC_K_READ:
		ch = getchar();
		switch (ch) {
		case '\n':
			ch = 0x9b;
			break;
		default:
			break;
		}
		regA = ch;
		regY = 1;
		ClrN;
		break;
	}
}

void E_Device(UBYTE esc_code)
{
	UBYTE ch;

	switch (esc_code) {
	case ESC_E_OPEN:
		printf("Editor Open\n");
		regY = 1;
		ClrN;
		break;
	case ESC_E_READ:
		ch = getchar();
		switch (ch) {
		case '\n':
			ch = 0x9b;
			break;
		default:
			break;
		}
		regA = ch;
		regY = 1;
		ClrN;
		break;
	case ESC_E_WRITE:
		ch = regA;
		switch (ch) {
		case 0x7d:
			putchar('*');
			break;
		case 0x9b:
			putchar('\n');
			break;
		default:
			if ((ch >= 0x20) && (ch <= 0x7e))	/* for DJGPP */
				putchar(ch & 0x7f);
			break;
		}
		regY = 1;
		ClrN;
		break;
	}
}

#define CDTMV1 0x0218

void Escape(UBYTE esc_code)
{
	switch (esc_code) {
#ifdef MONITOR_BREAK
	case ESC_BREAK:
		Atari800_Exit(TRUE);
		break;
#endif
	case ESC_SIOV:
		SIO();
		break;
	case ESC_K_OPEN:
	case ESC_K_CLOSE:
	case ESC_K_READ:
	case ESC_K_WRITE:
	case ESC_K_STATUS:
	case ESC_K_SPECIAL:
		K_Device(esc_code);
		break;
	case ESC_E_OPEN:
	case ESC_E_READ:
	case ESC_E_WRITE:
		E_Device(esc_code);
		break;
	case ESC_PHOPEN:
		Device_PHOPEN();
		break;
	case ESC_PHCLOS:
		Device_PHCLOS();
		break;
	case ESC_PHREAD:
		Device_PHREAD();
		break;
	case ESC_PHWRIT:
		Device_PHWRIT();
		break;
	case ESC_PHSTAT:
		Device_PHSTAT();
		break;
	case ESC_PHSPEC:
		Device_PHSPEC();
		break;
	case ESC_PHINIT:
		Device_PHINIT();
		break;
	case ESC_HHOPEN:
		Device_HHOPEN();
		break;
	case ESC_HHCLOS:
		Device_HHCLOS();
		break;
	case ESC_HHREAD:
		Device_HHREAD();
		break;
	case ESC_HHWRIT:
		Device_HHWRIT();
		break;
	case ESC_HHSTAT:
		Device_HHSTAT();
		break;
	case ESC_HHSPEC:
		Device_HHSPEC();
		break;
	case ESC_HHINIT:
		Device_HHINIT();
		break;
	default:
		Atari800_Exit(FALSE);
		printf("Invalid ESC Code %x at Address %x\n",
			   esc_code, regPC - 2);
		monitor();
		exit(1);
	}
}

int Atari800_Exit(int run_monitor)
{
	if (verbose) {
		printf("Current Frames per Secound = %f\n", fps);
	}
	return Atari_Exit(run_monitor);
}

/* special support of Bounty Bob on Atari5200 */
int bounty_bob1(UWORD addr)
{
	if (addr >= 0x4ff6 && addr <= 0x4ff9) {
		addr -= 0x4ff6;
		memcpy(&memory[0x4000], &cart_image[addr << 12], 0x1000);
		return 0;
	}
}

int bounty_bob2(UWORD addr)
{
	if (addr >= 0x5ff6 && addr <= 0x5ff9) {
		addr -= 0x5ff6;
		memcpy(&memory[0x5000], &cart_image[(addr << 12) + 0x4000], 0x1000);
		return 0;
	}
}

UBYTE Atari800_GetByte(UWORD addr)
{
	UBYTE byte;
/*
   ============================================================
   GTIA, POKEY, PIA and ANTIC do not fully decode their address
   ------------------------------------------------------------
   PIA (At least) is fully decoded when emulating the XL/XE
   ============================================================
 */
	switch (addr & 0xff00) {
	case 0x4f00:
		bounty_bob1(addr);
		byte = 0;
		break;
	case 0x5f00:
		bounty_bob2(addr);
		byte = 0;
		break;
	case 0xd000:				/* GTIA */
		byte = GTIA_GetByte(addr - 0xd000);
		break;
	case 0xd200:				/* POKEY */
		byte = POKEY_GetByte(addr - 0xd200);
		break;
	case 0xd300:				/* PIA */
		byte = PIA_GetByte(addr - 0xd300);
		break;
	case 0xd400:				/* ANTIC */
		byte = ANTIC_GetByte(addr - 0xd400);
		break;
	case 0xc000:				/* GTIA - 5200 */
		byte = GTIA_GetByte(addr - 0xc000);
		break;
	case 0xe800:				/* POKEY - 5200 */
		byte = POKEY_GetByte(addr - 0xe800);
		break;
	case 0xeb00:				/* POKEY - 5200 */
		byte = POKEY_GetByte(addr - 0xeb00);
		break;
	default:
		break;
	}

	return byte;
}

int Atari800_PutByte(UWORD addr, UBYTE byte)
{
	int abort = FALSE;
/*
   ============================================================
   GTIA, POKEY, PIA and ANTIC do not fully decode their address
   ------------------------------------------------------------
   PIA (At least) is fully decoded when emulating the XL/XE
   ============================================================
 */
	switch (addr & 0xff00) {
	case 0x4f00:
		abort = bounty_bob1(addr);
		break;
	case 0x5f00:
		abort = bounty_bob2(addr);
		break;
	case 0xd000:				/* GTIA */
		abort = GTIA_PutByte(addr - 0xd000, byte);
		break;
	case 0xd200:				/* POKEY */
		abort = POKEY_PutByte(addr - 0xd200, byte);
		break;
	case 0xd300:				/* PIA */
		abort = PIA_PutByte(addr - 0xd300, byte);
		break;
	case 0xd400:				/* ANTIC */
		abort = ANTIC_PutByte(addr - 0xd400, byte);
		break;
	case 0xd500:				/* Super Cartridges */
		abort = SuperCart_PutByte(addr, byte);
		break;
	case 0xc000:				/* GTIA - 5200 */
		abort = GTIA_PutByte(addr - 0xc000, byte);
		break;
	case 0xe800:				/* POKEY - 5200 AAA added other pokey space */
		abort = POKEY_PutByte(addr - 0xe800, byte);
		break;
	case 0xeb00:				/* POKEY - 5200 */
		abort = POKEY_PutByte(addr - 0xeb00, byte);
		break;
	default:
		break;
	}

	return abort;
}

void ShowRealSpeed(ULONG * atari_screen, int refresh_rate)
{
	UWORD *ptr, *ptr2;
	int i = (clock() - lastspeed) * (tv_mode == PAL ? 50 : 60) / CLK_TCK / refresh_rate;
	lastspeed = clock();

	if (i > ATARI_WIDTH / 4)
		return;

	ptr = (UWORD *) atari_screen;
	ptr += (ATARI_WIDTH / 2) * (ATARI_HEIGHT - 2) + ATARI_WIDTH / 4;	/* begin in middle of line */
	ptr2 = ptr + ATARI_WIDTH / 2;

	while (--i > 0) {			/* number of blocks = times slower */
		int j = i << 1;
		ptr[j] = ptr2[j] = 0x0707;
		j++;
		ptr[j] = ptr2[j] = 0;
	}
}

void Atari800_Hardware(void)
{
	static struct timeval tp;
	static struct timezone tzp;
	static double lasttime;
#ifdef USE_CLOCK
	static ULONG nextclock = 0;	/* put here a non-zero value to enable speed regulator */
#endif
	static long emu_too_fast = 0;	/* automatically enable/disable snailmeter */
	static Key_Held = FALSE;

	nframes = 0;
	gettimeofday(&tp, &tzp);
	lasttime = tp.tv_sec + (tp.tv_usec / 1000000.0);
	fps = 0.0;

	while (TRUE) {
		static int test_val = 0;
		int keycode;

#ifndef BASIC
/*
   colour_lookup[8] = colour_translation_table[COLBK];
 */

		keycode = Atari_Keyboard();

		switch (keycode) {
		case AKEY_COLDSTART:
			Coldstart();
			break;
		case AKEY_WARMSTART:
			Warmstart();
			break;
		case AKEY_EXIT:
			Atari800_Exit(FALSE);
			exit(1);
		case AKEY_BREAK:
			if (IRQEN & 0x80) {
				IRQST &= ~0x80;
				IRQ = 1;
			}
			break;
		case AKEY_UI:
			ui(atari_screen);
			break;
		case AKEY_PIL:
			if (pil_on)
				DisablePILL();
			else
				EnablePILL();
			break;
		case AKEY_NONE:
			Key_Held = FALSE;
			break;
		default:
			if (Key_Held)
				break;
			KBCODE = keycode;
			Key_Held = TRUE;
			if (IRQEN & 0x40) {
				IRQST &= ~0x40;
				IRQ = 1;
			}
			break;
		}
#endif

		/*
		 * Generate Screen
		 */

#ifndef BASIC
		if (++test_val == refresh_rate) {
			ANTIC_RunDisplayList();
			if (emu_too_fast == 0)
				ShowRealSpeed(atari_screen, refresh_rate);
			Atari_DisplayScreen((UBYTE *) atari_screen);
			test_val = 0;
		}
		else {
			for (ypos = 0; ypos < ATARI_HEIGHT; ypos++) {
				GO(114);
			}
		}
#else
		for (ypos = 0; ypos < ATARI_HEIGHT; ypos++) {
			GO(114);
		}
#endif

		nframes++;

/* if too fast host computer, slow the emulation down to 100% of original speed */

#ifdef USE_CLOCK
		/* on Atari Falcon CLK_TCK = 200 (i.e. 5 ms granularity) */
		/* on DOS (DJGPP) CLK_TCK = 91 */
		if (nextclock) {
			ULONG curclock;

			emu_too_fast = -1;
			do {
				curclock = clock();
				emu_too_fast++;
			} while (curclock < nextclock);

			nextclock = curclock + (CLK_TCK / (tv_mode == PAL ? 50 : 60));
		}
#else
#ifndef DJGPP
		if (deltatime > 0.0) {
			double curtime;

			emu_too_fast = -1;
			do {
				gettimeofday(&tp, &tzp);
				curtime = tp.tv_sec + (tp.tv_usec / 1000000.0);
				emu_too_fast++;
			} while (curtime < (lasttime + deltatime));

			fps = 1.0 / (curtime - lasttime);
			lasttime = curtime;
		}
#else							/* for dos, count ticks and use the ticks_per_second global variable */
		/* Use the high speed clock tick function uclock() */
		if (timesync) {
			if (deltatime > 0.0) {
				static uclock_t lasttime = 0;
				uclock_t curtime, uclockd;
				unsigned long uclockd_hi, uclockd_lo;
				unsigned long uclocks_per_int;

				uclocks_per_int = (deltatime * (double) UCLOCKS_PER_SEC) + 0.5;
				/* printf( "ticks_per_int %d, %.8X\n", uclocks_per_int, (unsigned long) lasttime ); */

				emu_too_fast = -1;
				do {
					curtime = uclock();
					if (curtime > lasttime) {
						uclockd = curtime - lasttime;
					}
					else {
						uclockd = ((uclock_t) ~ 0 - lasttime) + curtime + (uclock_t) 1;
					}
					uclockd_lo = (unsigned long) uclockd;
					uclockd_hi = (unsigned long) (uclockd >> 32);
					emu_too_fast++;
				} while ((uclockd_hi == 0) && (uclocks_per_int > uclockd_lo));

				lasttime = curtime;
			}
		}
#endif							/* DJGPP */
#endif							/* FALCON */
	}
}
