#ifndef __ANTIC__
#define __ANTIC__

#include "atari.h"

/*
 * Offset to registers in custom relative to start of antic memory addresses.
 */

#define _DMACTL 0x00
#define _CHACTL 0x01
#define _DLISTL 0x02
#define _DLISTH 0x03
#define _HSCROL 0x04
#define _VSCROL 0x05
#define _PMBASE 0x07
#define _CHBASE 0x09
#define _WSYNC 0x0a
#define _VCOUNT 0x0b
#define _PENH 0x0c
#define _PENV 0x0d
#define _NMIEN 0x0e
#define _NMIRES 0x0f
#define _NMIST 0x0f

extern UBYTE CHACTL;
extern UBYTE CHBASE;
extern UBYTE DLISTH;
extern UBYTE DLISTL;
extern UBYTE DMACTL;
extern UBYTE HSCROL;
extern UBYTE NMIEN;
extern UBYTE NMIST;
extern UBYTE PMBASE;
extern UBYTE VSCROL;

extern int ypos;
extern int wsync_halt;
extern int antic23f;			/* true if in antic modes 2, 3 or f */

extern UBYTE *scrn_ptr;
extern int xmin;
extern int xmax;
extern int dmactl_xmin_noscroll;
extern int dmactl_xmax_noscroll;
extern ULONG *atari_screen;

void ANTIC_Initialise(int *argc, char *argv[]);
void ANTIC_RunDisplayList(void);
UBYTE ANTIC_GetByte(UWORD addr);
int ANTIC_PutByte(UWORD addr, UBYTE byte);

#endif
