#ifndef __POKEY__
#define __POKEY__

#include "atari.h"

#define _AUDF1 0x00
#define _AUDC1 0x01
#define _AUDF2 0x02
#define _AUDC2 0x03
#define _AUDF3 0x04
#define _AUDC3 0x05
#define _AUDF4 0x06
#define _AUDC4 0x07
#define _AUDCTL 0x08
#define _STIMER 0x09
#define _SKRES 0x0a
#define _POTGO 0x0b
#define _SEROUT 0x0d
#define _IRQEN 0x0e
#define _SKCTLS 0x0f

#define _POT0 0x00
#define _POT1 0x01
#define _POT2 0x02
#define _POT3 0x03
#define _POT4 0x04
#define _POT5 0x05
#define _POT6 0x06
#define _POT7 0x07
#define _ALLPOT 0x08
#define _KBCODE 0x09
#define _RANDOM 0x0a
#define _SERIN 0x0d
#define _IRQST 0x0e
#define _SKSTAT 0x0f

extern UBYTE KBCODE;
extern UBYTE IRQST;
extern UBYTE IRQEN;

UBYTE POKEY_GetByte(UWORD addr);
int POKEY_PutByte(UWORD addr, UBYTE byte);

#endif
