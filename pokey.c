#include <stdlib.h>
#ifdef VMS
#include <types.h>
#else
#include <sys/types.h>
#endif
#include <time.h>

#include "atari.h"
#include "pia.h"
#include "pokey.h"
#include "gtia.h"
#include "sio.h"
#include "platform.h"

#ifdef USE_DOSSOUND
#include "pokeysnd.h"
#endif

#define FALSE 0
#define TRUE 1

static char *rcsid = "$Id: pokey.c,v 1.11 1998/02/21 15:20:28 david Exp $";

UBYTE KBCODE;
UBYTE IRQST;
UBYTE IRQEN;
UBYTE SKSTAT;
int SHIFT_KEY = FALSE, KEYPRESSED = FALSE;

UBYTE POKEY_GetByte(UWORD addr)
{
	UBYTE byte;
	static int rand_init = 0;

	addr &= 0xff0f;
	switch (addr) {
	case _KBCODE:
		byte = KBCODE;
		break;
	case _IRQST:
		byte = IRQST;
#ifdef DEBUG1
		printf("RD: IRQST = %x, PC = %x\n", byte, PC);
#endif
		break;
	case _POT0:
		byte = Atari_POT(0);
		break;
	case _POT1:
		byte = Atari_POT(1);
		break;
	case _POT2:
		byte = Atari_POT(2);
		break;
	case _POT3:
		byte = Atari_POT(3);
		break;
	case _POT4:
		byte = Atari_POT(4);
		break;
	case _POT5:
		byte = Atari_POT(5);
		break;
	case _POT6:
		byte = Atari_POT(6);
		break;
	case _POT7:
		byte = Atari_POT(7);
		break;
	case _RANDOM:
		if (!rand_init) {
			srand((int) time((time_t *) NULL));
			rand_init = 1;
		}
		byte = rand();
		break;
	case _SERIN:
		/* byte = SIO_SERIN(); */
		byte = SIO_GetByte();
/*
   colour_lookup[8] = colour_translation_table[byte];
 */
#ifdef DEBUG1
		printf("RD: SERIN = %x, BUFRFL = %x, CHKSUM = %x, BUFR = %02x%02x, BFEN=%02x%02x, PC = %x\n",
			byte, memory[0x38], memory[0x31], memory[0x33], memory[0x32],
			   memory[0x35], memory[0x34], PC);
		printf("AR RD: SERIN = %x, BUFRFL = %x, CHKSUM = %x, BUFR = %02x%02x, BFEN=%02x%02x, PC = %x\n",
			 byte, memory[0x38], memory[0x23c], memory[0x1], memory[0x0],
			   memory[0x23b], memory[0x23a], PC);
#endif
		break;
	case _SKSTAT:
		byte = 0xff;
		if (SHIFT_KEY)
			byte &= ~8;
		if (KEYPRESSED)
			byte &= ~4;
		break;
	}

	return byte;
}

int POKEY_PutByte(UWORD addr, UBYTE byte)
{
	addr &= 0xff0f;
	switch (addr) {
	case _AUDC1:
		Atari_AUDC(1, byte);
		break;
	case _AUDC2:
		Atari_AUDC(2, byte);
		break;
	case _AUDC3:
		Atari_AUDC(3, byte);
		break;
	case _AUDC4:
		Atari_AUDC(4, byte);
		break;
	case _AUDCTL:
		Atari_AUDCTL(byte);
		break;
	case _AUDF1:
		Atari_AUDF(1, byte);
		break;
	case _AUDF2:
		Atari_AUDF(2, byte);
		break;
	case _AUDF3:
		Atari_AUDF(3, byte);
		break;
	case _AUDF4:
		Atari_AUDF(4, byte);
		break;
	case _IRQEN:
		IRQEN = byte;
#ifdef DEBUG1
		printf("WR: IRQEN = %x, PC = %x\n", IRQEN, PC);
#endif
		IRQST |= (~byte);
		break;
	case _POTGO:
		break;
	case _SEROUT:
	/*
		{
			int cmd_flag = (PBCTL & 0x08) ? 0 : 1;

#ifdef DEBUG1
			printf("WR: SEROUT = %x, BUFRFL = %x, CHKSUM = %x, BUFL = %02x%02x, BFEN = %02x%02x, PC = %x\n",
				   byte, memory[0x38], memory[0x31],
				   memory[0x33], memory[0x32],
				   memory[0x35], memory[0x34],
				   PC);
#endif
			colour_lookup[8] = colour_translation_table[byte];
			SIO_SEROUT(byte, cmd_flag);
		}
	*/
		if ((SKSTAT & 0x70) == 0x20) {
			if (pokeysnd_siocheck()) {
				/* if ((AUDF[CHAN3] == 0x28) && (AUDF[CHAN4] == 0x00) && (AUDCTL & 0x28)==0x28) */
				SIO_PutByte(byte);
			}
		}
		break;
	case _STIMER:
#ifdef DEBUG1
		printf("WR: STIMER = %x\n", byte);
#endif
		break;
	case _SKSTAT:
		SKSTAT = byte;
		break;
	}

	return FALSE;
}
