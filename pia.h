#ifndef __PIA__
#define __PIA__

#include "atari.h"

#define _PORTA 0x00
#define _PORTB 0x01
#define _PACTL 0x02
#define _PBCTL 0x03

extern UBYTE PACTL;
extern UBYTE PBCTL;
extern UBYTE PORTA;
extern UBYTE PORTB;

extern int xe_bank;

#endif
