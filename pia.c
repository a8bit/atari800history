#include <stdio.h>

#include "atari.h"
#include "cpu.h"
#include "pia.h"
#include "platform.h"
#include "sio.h"

#define FALSE 0
#define TRUE 1

static char *rcsid = "$Id: pia.c,v 1.12 1998/02/21 14:55:43 david Exp $";

UBYTE PACTL;
UBYTE PBCTL;
UBYTE PORTA;
UBYTE PORTB;

int xe_bank = 0;
int selftest_enabled = 0;
int Ram256 = 0;

extern int mach_xlxe;

int rom_inserted;
UBYTE atari_basic[8192];
UBYTE atarixl_os[16384];
static UBYTE under_atari_basic[8192];
static UBYTE under_atarixl_os[16384];
static UBYTE atarixe_memory[278528];	/* 16384 (for RAM under BankRAM buffer) + 65536 (for 130XE) * 4 (for Atari320) */

static UBYTE PORTA_mask = 0xff;
static UBYTE PORTB_mask = 0xff;

void PIA_Initialise(int *argc, char *argv[])
{
	PORTA = 0xff;
	PORTB = 0xff;
}

UBYTE PIA_GetByte(UWORD addr)
{
	UBYTE byte;

	if (machine == Atari)
		addr &= 0xff03;
	switch (addr) {
	case _PACTL:
		byte = PACTL;
		break;
	case _PBCTL:
		byte = PBCTL;
#ifdef DEBUG1
		printf("RD: PBCTL = %x, PC = %x\n", PBCTL, PC);
#endif
		break;
	case _PORTA:
	/*
		byte = Atari_PORT(0);
		byte &= PORTA_mask;
	*/
		byte = (PORTA & (~PORTA_mask)) | (Atari_PORT(0) & PORTA_mask);
		break;
	case _PORTB:
		switch (machine) {
		case Atari:
			byte = Atari_PORT(1);
			byte &= PORTB_mask;
			break;
		case AtariXL:
		case AtariXE:
		case Atari320XE:
			/* byte = PORTB; */
			byte = (PORTB & (~PORTB_mask)) | PORTB_mask;
			break;
		default:
			printf("Fatal Error in pia.c: PIA_GetByte(): Unknown machine\n");
			Atari800_Exit(FALSE);
			exit(1);
			break;
		}
		break;
	}

	return byte;
}

int PIA_PutByte(UWORD addr, UBYTE byte)
{
	if (machine == Atari)
		addr &= 0xff03;

	switch (addr) {
	case _PACTL:
		PACTL = byte;
		break;
	case _PBCTL:
		/* This code is part of the serial I/O emulation */
		if ((PBCTL ^ byte) & 0x08) {	/* The command line status has changed */
			SwitchCommandFrame((byte & 0x08) ? (0) : (1));
		}
		PBCTL = byte;
#ifdef DEBUG1
		printf("WR: PBCTL = %x, PC = %x\n", PBCTL, PC);
#endif
		break;
	case _PORTA:
		if (!(PACTL & 0x04))
 			PORTA_mask = ~byte;
		else
			PORTA = byte;		/* change from thor */

		break;
	case _PORTB:
		if (!(PBCTL & 0x04)) {	/* change from thor */
			PORTB_mask = ~byte;
			byte = PORTB;
			break;
		}

		if (((byte | PORTB_mask) == 0) && mach_xlxe)
			break;				/* special hack for old Atari800 games like is Tapper, for example */

		switch (machine) {
		case Atari:
		/*
			if (!(PBCTL & 0x04))
				PORTB_mask = ~byte;
		*/
			PORTB = byte;
			break;
		case AtariXE:
		case Atari320XE:
			{
				int bank;
				/* bank = 0 ...normal RAM */
				/* bank = 1..4 (..16) ...extended RAM */

				if (byte & 0x10) {
					/* CPU to normal RAM */
					bank = 0;
				}
				else {
					/* CPU to extended RAM */
					if (machine == Atari320XE) {
						if (Ram256 == 1)
							bank = (((byte & 0x0c) | ((byte & 0x60) >> 1)) >> 2) + 1; /* RAMBO */
						else 
							bank = (((byte & 0x0c) | ((byte & 0xc0) >> 2)) >> 2) + 1; /* COMPY SHOP */
					}
					else
						bank = ((byte & 0x0c) >> 2) + 1;
				}

				if (bank != xe_bank) {
					if (selftest_enabled) {
						/* SelfTestROM Disable */
						memcpy(memory + 0x5000, under_atarixl_os + 0x1000, 0x800);
						SetRAM(0x5000, 0x57ff);
						selftest_enabled = FALSE;
					}
					memcpy(atarixe_memory + (((long) xe_bank) << 14), memory + 0x4000, 16384);
					memcpy(memory + 0x4000, atarixe_memory + (((long) bank) << 14), 16384);
					xe_bank = bank;
				}
			}
		case AtariXL:
#ifdef DEBUG
			printf("Storing %x to PORTB, PC = %x\n", byte, regPC);
#endif
/*
 * Enable/Disable OS ROM 0xc000-0xcfff and 0xd800-0xffff
 */
			if ((PORTB ^ byte) & 0x01) {	/* Only when is changed this bit !RS! */
				if (byte & 0x01) {
					/* OS ROM Enable */
					memcpy(under_atarixl_os, memory + 0xc000, 0x1000);
					memcpy(under_atarixl_os + 0x1800, memory + 0xd800, 0x2800);
					memcpy(memory + 0xc000, atarixl_os, 0x1000);
					memcpy(memory + 0xd800, atarixl_os + 0x1800, 0x2800);
					SetROM(0xc000, 0xcfff);
					SetROM(0xd800, 0xffff);
				}
				else {
					/* OS ROM Disable */
					memcpy(memory + 0xc000, under_atarixl_os, 0x1000);
					memcpy(memory + 0xd800, under_atarixl_os + 0x1800, 0x2800);
					SetRAM(0xc000, 0xcfff);
					SetRAM(0xd800, 0xffff);
				}
			}

/*
   =====================================
   Enable/Disable BASIC ROM
   An Atari XL/XE can only disable Basic
   Other cartridge cannot be disable
   =====================================
 */
			if (!rom_inserted) {
				if ((PORTB ^ byte) & 0x02) {	/* Only when change this bit !RS! */
					if (byte & 0x02) {
						/* BASIC Disable */
						memcpy(memory + 0xa000, under_atari_basic, 0x2000);
						SetRAM(0xa000, 0xbfff);
					}
					else {
						/* BASIC Enable */
						memcpy(under_atari_basic, memory + 0xa000, 0x2000);
						memcpy(memory + 0xa000, atari_basic, 0x2000);
						SetROM(0xa000, 0xbfff);
					}
				}
			}
/*
 * Enable/Disable Self Test ROM
 */
			if (byte & 0x80) {
				/* SelfTestROM Disable */
				if (selftest_enabled) {
					memcpy(memory + 0x5000, under_atarixl_os + 0x1000, 0x800);
					SetRAM(0x5000, 0x57ff);
					selftest_enabled = FALSE;
				}
			}
			else {
				/* SELFTEST ROM enable */
				if (!selftest_enabled && ((byte & 0x10) || (Ram256 < 2))) {
					/* Only when CPU access to normal RAM or isn't 256Kb RAM or RAMBO mode is set */
					memcpy(under_atarixl_os + 0x1000, memory + 0x5000, 0x800);
					memcpy(memory + 0x5000, atarixl_os + 0x1000, 0x800);
					SetROM(0x5000, 0x57ff);
					selftest_enabled = TRUE;
				}
			}

			PORTB = byte;
			break;
		default:
			printf("Fatal Error in pia.c: PIA_PutByte(): Unknown machine\n");
			Atari800_Exit(FALSE);
			exit(1);
			break;
		}
		break;
	}

	return FALSE;
}
