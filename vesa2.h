#include "atari.h"

/* routines with VESA2 support for Atari800 emulator */


extern UBYTE VESA_getmode(int width,int height,UWORD *videomode,ULONG *memaddress,ULONG *line,ULONG *memsize);

extern UBYTE VESA_open(UWORD mode,ULONG memaddress,ULONG memsize,ULONG *linear,int *selector);

extern UBYTE VESA_close(ULONG *linear,int *selector);

extern void VESA_blit(void *mem,ULONG width,ULONG height,ULONG bitmapline,ULONG videoline,UWORD selector);

extern void VESA_i_blit(void *mem,ULONG width,ULONG height,ULONG bitmapline,ULONG videoline,UWORD selector);

