#ifndef CPU_INCLUDED

#include	"system.h"

#define	CPU_STATUS_OK	0x0100
#define	CPU_STATUS_ERR	0x0200
#define	CPU_STATUS_ESC	0x0300

#define	CPU_STATUS_MASK	0xff00
#define	CPU_ESC_MASK	0x00ff

#define	N_MASK	0x80
#define	V_MASK	0x40
#define	B_MASK	0x10
#define	D_MASK	0x08
#define	I_MASK	0x04
#define	Z_MASK	0x02
#define	C_MASK	0x01

typedef struct CPU_Status
{
	UWORD	PC;
	UBYTE	A;
	UBYTE	S;
	UBYTE	X;
	UBYTE	Y;
	struct Flags
	{
		unsigned int	N : 1;
		unsigned int	V : 1;
		unsigned int	B : 1;
		unsigned int	D : 1;
		unsigned int	I : 1;
		unsigned int	Z : 1;
		unsigned int	C : 1;
	} flag;
} CPU_Status;

void	CPU_Reset (void);
void	CPU_GetStatus (CPU_Status *cpu_status);
void	CPU_PutStatus (CPU_Status *cpu_status);
int	GO (void);

#define	RAM		0
#define	ROM		1
#define	HARDWARE	2

extern UBYTE	memory[65536];
extern UBYTE	attrib[65536];

extern void  (*Escape)   (UBYTE code);

#define	GetByte(addr)		((attrib[addr] == HARDWARE) ? Atari800_GetByte(addr) : memory[addr])
#define	PutByte(addr,byte)	if (attrib[addr] == RAM) memory[addr] = byte; else if (attrib[addr] == HARDWARE) Atari800_PutByte(addr,byte);
#define	GetWord(addr)		((GetByte(addr+1) << 8) | GetByte(addr))

#define	NMI_MASK	0x01
#define	IRQ_MASK	0x02

extern int	INTERRUPT;
extern int	ncycles;

#endif
