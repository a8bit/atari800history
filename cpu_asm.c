#include	<stdio.h>
#include	<stdlib.h>
#ifdef ATARIST
#include	<osbind.h>
#endif

static char *rcsid = "$Id: cpu_asm.c,v 1.2 1998/02/21 15:02:46 david Exp $";

#define	FALSE	0
#define	TRUE	1

#include	"atari.h"
#include	"cpu_asm.h"
#include	"pia.h"
#include	"gtia.h"
#include	"sio.h"
#include	"pokey.h"
#include	"antic.h"


long CEKEJ;
extern UBYTE IRQ;
extern int wsync_halt;

extern UWORD break_addr;

/*
   ==========================================================
   Emulated Registers and Flags are kept local to this module
   ==========================================================
 */

#define	GetByte(addr)		((attrib[addr] == HARDWARE) ? Atari800_GetByte(addr) : memory[addr])
#define	PutByte(addr,byte)	if (attrib[addr] == RAM) memory[addr] = byte; else if (attrib[addr] == HARDWARE) if (Atari800_PutByte(addr,byte)) break;


#define RAM 0
#define ROM 1
#define HARDWARE 2
int count[256];

extern void tisk(void);
UBYTE memory[65536];

UBYTE attrib[65536];


/*
   ===============================================================
   Z flag: This actually contains the result of an operation which
   would modify the Z flag. The value is tested for
   equality by the BEQ and BNE instruction.
   ===============================================================
 */



UBYTE BCDtoDEC[256];
UBYTE DECtoBCD[256];

void CPU_Reset(void)
{
	int i;

	for (i = 0; i < 256; i++) {
		BCDtoDEC[i] = ((i >> 4) & 0xf) * 10 + (i & 0xf);
		DECtoBCD[i] = (((i % 100) / 10) << 4) | (i % 10);
#ifdef PROFILE
		count[i] = 0;
#endif
	}

	IRQ = 0x00;

	regP = 0x20;				/* The unused bit is always 1 */
	regS = 0xff;
	regPC = (GetByte(0xfffd) << 8) | GetByte(0xfffc);
}

 /*  it was there only for debug purposes.....
    #define PHP data =  (N & 0x80); \
    data |= V ? 0x40 : 0; \
    data |= (regP & 0x3c); \
    data |= (Z == 0) ? 0x02 : 0; \
    data |= C; \
    memory[0x0100 + S--] = data;

    void NMI (void)
    {
    UBYTE S = regS;
    UBYTE data;

    memory[0x0100 + S--] = regPC >> 8;
    memory[0x0100 + S--] = regPC & 0xff;
    memory[0x100 + S--] = regP;
    SetI;
    regPC = (memory[0xfffb] << 8) | memory[0xfffa];
    regS = S;
    }
  */

void SetRAM(int addr1, int addr2)
{
	int i;

	for (i = addr1; i <= addr2; i++) {
		attrib[i] = RAM;
	}
}

void SetROM(int addr1, int addr2)
{
	int i;

	for (i = addr1; i <= addr2; i++) {
		attrib[i] = ROM;
	}
}

void SetHARDWARE(int addr1, int addr2)
{
	int i;

	for (i = addr1; i <= addr2; i++) {
		attrib[i] = HARDWARE;
	}
}

UBYTE GETBYTE(UWORD addr)
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

int PUTBYTE(UWORD addr, UBYTE byte)
{
	int abort = TRUE;			/*FALSE; */
/*
   ============================================================
   GTIA, POKEY, PIA and ANTIC do not fully decode their address
   ------------------------------------------------------------
   PIA (At least) is fully decoded when emulating the XL/XE
   ============================================================
 */
	switch (addr & 0xff00) {
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
		if (wsync_halt)
			abort = TRUE;
		break;
	case 0xd500:				/* Super Cartridges */
		abort = SuperCart_PutByte(addr, byte);
		break;
	case 0xc000:				/* GTIA - 5200 */
		abort = GTIA_PutByte(addr - 0xc000, byte);
		break;
	case 0xeb00:				/* POKEY - 5200 */
		abort = POKEY_PutByte(addr - 0xeb00, byte);
		break;
	default:
		break;
	}

	return abort;
}

void CPU_GetStatus(void)
{
	CPUGET();
}

void GenerateIRQ(void)
{
	IRQ = 1;
	GO(0);						/* does not execute any instruction */
}

void tisk(void)
{
	void *adresa = (long *) 0x400000;
	printf("OLD %d %d %d %d %d %d\n", *(UBYTE *) (adresa - 1), *(UBYTE *) (adresa - 2),
		   *(UBYTE *) (adresa - 3), *(UBYTE *) (adresa - 4), *(UBYTE *) (adresa - 5), *(UWORD *) (adresa - 8));
	printf("NEW %d %d %d %d %d %d\n", regP, regS, regA, regX, regY, regPC);
}
