/* CPU.C
 *    Original Author     :   David Firth          *
 *    Last changes        :   26th April 1998, RASTER */
/*
   Ideas for Speed Improvements
   ============================

   N = (result >= 128) could become N = NTAB[result];

   This saves the branch which breaks the
   pipeline.

   Flags could be 0x00 or 0xff instead of 0x00 or 0x01

   This allows branches to be implemented as
   follows :-

   BEQ  PC += (offset & Z)
   BNE  PC += (offset & (~Z))

   again, this prevents the pipeline from
   being broken.

   The 6502 emulation ignore memory attributes for
   instruction fetch. This is because the instruction
   must come from either RAM or ROM. A program that
   executes instructions from within hardware
   addresses will fail since there is never any
   usable code there.

   The 6502 emulation also ignores memory attributes
   for accesses to page 0 and page 1.
 */

#include	<stdio.h>
#include	<stdlib.h>

#define	FALSE	0
#define	TRUE	1

#include	"atari.h"
#include	"cpu.h"

/*
==========================================================
Emulated Registers and Flags are kept local to this module
==========================================================
*/

#define UPDATE_GLOBAL_REGS regPC=PC;regS=S;regA=A;regX=X;regY=Y
#define UPDATE_LOCAL_REGS PC=regPC;S=regS;A=regA;X=regX;Y=regY

UWORD regPC;
UBYTE regA;
UBYTE regP;						/* Processor Status Byte (Partial) */
UBYTE regS;
UBYTE regX;
UBYTE regY;

static UBYTE N;					/* bit7 zero (0) or bit 7 non-zero (1) */
static UBYTE Z;					/* zero (0) or non-zero (1) */
static UBYTE V;
static UBYTE C;					/* zero (0) or one(1) */

#define RAM 0
#define ROM 1
#define HARDWARE 2

								/*
								#define PROFILE
*/

#ifdef TRACE
extern int tron;
#endif

/*
* The following array is used for 6502 instruction profiling
*/

int count[256];

UBYTE memory[65536];

UBYTE IRQ;

extern int wsync_halt;			/* WSYNC_HALT */

#ifdef MONITOR_BREAK
UWORD remember_PC[REMEMBER_PC_STEPS];
extern UWORD break_addr;
UWORD remember_JMP[REMEMBER_JMP_STEPS];
extern UBYTE break_step;
extern UBYTE break_ret;
extern UBYTE break_cim;
extern int ret_nesting;
#endif

UBYTE attrib[65536];
#define	GetByte(addr)		((attrib[addr] == HARDWARE) ? Atari800_GetByte(addr) : memory[addr])
#define	PutByte(addr,byte)	if (attrib[addr] == RAM) memory[addr] = byte; else if (attrib[addr] == HARDWARE) if (Atari800_PutByte(addr,byte)) 

/*
===============================================================
Z flag: This actually contains the result of an operation which
would modify the Z flag. The value is tested for
equality by the BEQ and BNE instruction.
===============================================================
*/

void CPU_GetStatus(void)
{
	if (N)
		SetN;
	else
		ClrN;
	
	if (Z)
		ClrZ;
	else
		SetZ;
	
	if (V)
		SetV;
	else
		ClrV;
	
	if (C)
		SetC;
	else
		ClrC;
}

void CPU_PutStatus(void)
{
	if (regP & N_FLAG)
		N = 0x80;
	else
		N = 0x00;
	
	if (regP & Z_FLAG)
		Z = 0;
	else
		Z = 1;
	
	if (regP & V_FLAG)
		V = 1;
	else
		V = 0;
	
	if (regP & C_FLAG)
		C = 1;
	else
		C = 0;
}

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
	
	IRQ = 0;
	
	regP = 0x20;				/* The unused bit is always 1 */
	regS = 0xff;
	regPC = (GetByte(0xfffd) << 8) | GetByte(0xfffc);
}

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

#define AND(t_data) data = t_data; Z = N = A &= data
#define CMP(t_data) data = t_data; Z = N = A - data; C = (A >= data)
#define CPX(t_data) data = t_data; Z = N = X - data; C = (X >= data);
#define CPY(t_data) data = t_data; Z = N = Y - data; C = (Y >= data);
#define EOR(t_data) data = t_data; Z = N = A ^= data;
#define LDA(data) Z = N = A = data;
#define LDX(data) Z = N = X = data;
#define LDY(data) Z = N = Y = data;
#define ORA(t_data) data = t_data; Z = N = A |= data

#define PHP data =  (N & 0x80); \
	data |= V ? 0x40 : 0; \
	data |= (regP & 0x3c); \
	data |= (Z == 0) ? 0x02 : 0; \
	data |= C; \
memory[0x0100 + S--] = data;

#define PLP data = memory[0x0100 + ++S]; \
	N = (data & 0x80); \
	V = (data & 0x40) ? 1 : 0; \
	Z = (data & 0x02) ? 0 : 1; \
	C = (data & 0x01); \
regP = (data & 0x3c) | 0x20;

void NMI(void)
{
	UBYTE S = regS;
	UBYTE data;
	
	memory[0x0100 + S--] = regPC >> 8;
	memory[0x0100 + S--] = (UBYTE) regPC;
	PHP;
	SetI;
	regPC = (memory[0xfffb] << 8) | memory[0xfffa];
	regS = S;
#ifdef MONITOR_BREAK
	ret_nesting++;
#endif
}

/* check pending IRQ, helps in (not only) Lucasfilm games */
#ifdef MONITOR_BREAK
#define CPUCHECKIRQ							\
{									\
	if (IRQ) {							\
	if (!(regP & I_FLAG)) {					\
	memory[0x0100 + S--] = PC >> 8;			\
	memory[0x0100 + S--] = (UBYTE)PC;		\
	PHP;						\
	SetI;						\
	PC = (memory[0xffff] << 8) | memory[0xfffe];	\
	IRQ = 0;					\
	ret_nesting+=1;					\
	}							\
	}								\
}
#else
#define CPUCHECKIRQ											\
{															\
	if (IRQ) {												\
	if (!(regP & I_FLAG)) {								\
	memory[0x0100 + S--] = PC >> 8;					\
	memory[0x0100 + S--] = (UBYTE)PC;				\
	PHP;											\
	SetI;											\
	PC = (memory[0xffff] << 8) | memory[0xfffe];	\
	IRQ = 0;										\
	}													\
	}														\
}
#endif
/* sets the IRQ flag and checks it */
void GenerateIRQ(void)
{
	IRQ = 1;
	GO(0);						/* does not execute any instruction */
}

/*
==============================================================
The first switch statement is used to determine the addressing
mode, while the second switch implements the opcode. When I
have more confidence that these are working correctly they
will be combined into a single switch statement. At the
moment changes can be made very easily.
==============================================================
*/

/*    0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
int cycles[256] =
{
	7, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6,		/* 0x */
		2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,		/* 1x */
		6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6,		/* 2x */
		2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,		/* 3x */
		
		6, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6,		/* 4x */
		2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,		/* 5x */
		6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6,		/* 6x */
		2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,		/* 7x */
		
		2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,		/* 8x */
		2, 6, 2, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5,		/* 9x */
		2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,		/* Ax */
		2, 5, 2, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4,		/* Bx */
		
		2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,		/* Cx */
		2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,		/* Dx */
		2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,		/* Ex */
		2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7  	/* Fx */
};

/* decrement 1 or 2 cycles for conditional jumps */
#define NCYCLES_JMP	if ( ((SWORD) sdata + (UBYTE) PC) & 0xff00) ncycles-=2; else ncycles--;
/* decrement 1 cycle for X (or Y) index overflow */
#define NCYCLES_Y		if ( (UBYTE) addr < Y ) ncycles--;
#define NCYCLES_X		if ( (UBYTE) addr < X ) ncycles--;


int GO(int ncycles)
{
	UWORD PC;
	UBYTE S;
	UBYTE A;
	UBYTE X;
	UBYTE Y;
	
	UWORD addr;
	UBYTE data;
	
	/*
	This used to be in the main loop but has been removed to improve
	execution speed. It does not seem to have any adverse effect on
	the emulation for two reasons:-
	
	  1. NMI's will can only be raised in atari_custom.c - there is
	  no way an NMI can be generated whilst in this routine.
	  
		2. The timing of the IRQs are not that critical.
	*/
	
	UPDATE_LOCAL_REGS;
	
	CPUCHECKIRQ;
	
	/*
	=====================================
	Extract Address if Required by Opcode
	=====================================
	*/
	
#define	ABSOLUTE	addr=(memory[PC+1]<<8)+memory[PC];PC+=2;
#define	ZPAGE		addr=memory[PC++];
#define	ABSOLUTE_X	addr=((memory[PC+1]<<8)+memory[PC])+X;PC+=2;
#define	ABSOLUTE_Y	addr=((memory[PC+1]<<8)+memory[PC])+Y;PC+=2;
#define	INDIRECT_X	addr=(UBYTE)(memory[PC++]+X);addr=(memory[(UBYTE)(addr+1)]<<8)+memory[addr];
#define	INDIRECT_Y	addr=memory[PC++];addr=(memory[(UBYTE)(addr+1)]<<8)+memory[addr]+Y;
#define	ZPAGE_X		addr=(UBYTE)(memory[PC++]+X);
#define	ZPAGE_Y		addr=(UBYTE)(memory[PC++]+Y);
	
#ifdef __i386__
#undef ABSOLUTE
#undef ABSOLUTE_X
#undef ABSOLUTE_Y
#ifdef __ELF__
#define ABSOLUTE asm("movw memory(%1),%0" \
	: "=r" (addr) \
	: "r" ((ULONG)PC)); PC+=2;
#define ABSOLUTE_X asm("movw memory(%1),%0; addw %2,%0" \
	: "=r" (addr) \
	: "r" ((ULONG)PC), "r" ((UWORD)X));PC+=2;
#define ABSOLUTE_Y asm("movw memory(%1),%0; addw %2,%0" \
	: "=r" (addr) \
	: "r" ((ULONG)PC), "r" ((UWORD)Y));PC+=2;
#else
#define ABSOLUTE asm("movw _memory(%1),%0" \
	: "=r" (addr) \
	: "r" ((ULONG)PC)); PC+=2;
#define ABSOLUTE_X asm("movw _memory(%1),%0; addw %2,%0" \
	: "=r" (addr) \
	: "r" ((ULONG)PC), "r" ((UWORD)X));PC+=2;
#define ABSOLUTE_Y asm("movw _memory(%1),%0; addw %2,%0" \
	: "=r" (addr) \
	: "r" ((ULONG)PC), "r" ((UWORD)Y));PC+=2;
#endif
#endif
	
	while (ncycles > 0 && !wsync_halt) {
		
#ifdef TRACE
		if (tron) {
			disassemble(PC, PC + 1);
			printf("\tA=%2x, X=%2x, Y=%2x, S=%2x\n",
				A, X, Y, S);
		}
#endif
		
#ifdef PROFILE
		count[memory[PC]]++;
#endif
		
#ifdef MONITOR_BREAK
		memmove(&remember_PC[0], &remember_PC[1], (REMEMBER_PC_STEPS - 1) * 2);
		remember_PC[REMEMBER_PC_STEPS - 1] = PC;
		
		if (break_addr == PC) {
			data = ESC_BREAK;
			UPDATE_GLOBAL_REGS;
			CPU_GetStatus();
			AtariEscape(data);
			CPU_PutStatus();
			UPDATE_LOCAL_REGS;
		}
#endif
		ncycles -= cycles[memory[PC]];
		
		switch (memory[PC++]) {
		case 0x00:	/* BRK */
			{
				if (!(regP & I_FLAG)) 
				{
					UWORD retadr = PC + 1;
					memory[0x0100 + S--] = retadr >> 8;
					memory[0x0100 + S--] = (UBYTE) retadr;
					SetB;
					PHP;
					SetI;
					PC = (memory[0xffff] << 8) | memory[0xfffe];
#ifdef MONITOR_BREAK
					ret_nesting++;
#endif
				}
			}
            continue;
            break;
			
		case 0x01:	/* ORA (ab,x) */
			{
				INDIRECT_X;
				ORA(GetByte(addr));
			}
            continue;
			break;
			
		case 0x02:	/* CIM [unofficial - crash intermediate] */
			{
cpucrash:
			PC--;
			data = 0;
#ifdef MONITOR_BREAK
			break_cim = 1;
			data = ESC_BREAK;
#endif
			UPDATE_GLOBAL_REGS;
			CPU_GetStatus();
			AtariEscape(data);
			CPU_PutStatus();
			UPDATE_LOCAL_REGS;
			}
            continue;
            break;
			
		case 0x03:	/* ASO (ab,x) [unofficial - ASL then ORA with Acc] */
			{
				INDIRECT_X;
				data = GetByte(addr);
				C = (data & 0x80) ? 1 : 0;
				data = (data << 1);
				PutByte(addr, data); 
				Z = N = A |= data;
			}
			continue;
			break;
			
		case 0x04:	/* SKB [unofficial - skip byte] */
			PC++;
			continue;
			break;
			
		case 0x05:	/* ORA ab */
			{
				ZPAGE;
				ORA(memory[addr]);
			}
			continue;
			break;
			
		case 0x06:	/* ASL ab */
			{
				ZPAGE;
				data = memory[addr];
				C = (data & 0x80) ? 1 : 0;
				Z = N = data << 1;
				memory[addr] = Z;
			}
			continue;
			break;
			
		case 0x07:	/* ASO zpage [unofficial - ASL then ORA with Acc] */
			{
				ZPAGE;
				data = memory[addr];
				C = (data & 0x80) ? 1 : 0;
				data = (data << 1);
				memory[addr] = data; 
				Z = N = A |= data;
            }
            continue;
            break;
			
		case 0x08:	/* PHP */
			{
				PHP;
            }
            continue;
            break;
			
		case 0x09:	/* ORA #ab */
			{
				ORA(memory[PC++]);
            }
            continue;
            break;
			
		case 0x0a:	/* ASL */
			{
				C = (A & 0x80) ? 1 : 0;
				Z = N = A = A << 1;
            }
            continue;
            break;
			
		case 0x0b:	/* ASO #ab [unofficial - ASL then ORA with Acc] */
			{
				/* !!! Tested on real Atari => AND #ab */
				AND(memory[PC++]);
            }
            continue;
            break;
			
		case 0x0c:	/* SKW [unofficial - skip word] */
			{
				PC += 2;
            }
            continue;
            break;
			
		case 0x0d:	/* ORA abcd */
			{
				ABSOLUTE;
				ORA(GetByte(addr));
            }
            continue;
            break;
			
		case 0x0e:	/* ASL abcd */
			{
				ABSOLUTE;
				data = GetByte(addr);
				C = (data & 0x80) ? 1 : 0;
				Z = N = data << 1;
				PutByte(addr, Z);
            }
            continue;
            break;
			
		case 0x0f:	/* ASO abcd [unofficial - ASL then ORA with Acc] */
			{
				ABSOLUTE;
				data = GetByte(addr);
				C = (data & 0x80) ? 1 : 0;
				data = (data << 1);
				PutByte(addr, data); 
				Z = N = A |= data;
            }
            continue;
            break;
			
		case 0x10:	/* BPL */
			{
				if (!(N & 0x80)) {
					SBYTE sdata = memory[PC];
					NCYCLES_JMP;
					PC += (SWORD) sdata;
				}
				PC++;
            }
            continue;
            break;
			
		case 0x11:	/* ORA (ab),y */
			{
				INDIRECT_Y;
				NCYCLES_Y;
				ORA(GetByte(addr));
            }
            continue;
            break;
			
		case 0x12:	/* CIM [unofficial - crash intermediate] */
			goto cpucrash;
			break;
			
			
		case 0x13:	/* ASO (ab),y [unofficial - ASL then ORA with Acc] */
			{
				INDIRECT_Y;
				data = GetByte(addr);
				C = (data & 0x80) ? 1 : 0;
				data = (data << 1);
				PutByte(addr, data); 
				Z = N = A |= data;
            }
            continue;
            break;
			
		case 0x14:	/* SKB [unofficial - skip byte] */
			{
				PC++;
            }
            continue;
            break;
			
		case 0x15:	/* ORA ab,x */
			{
				ZPAGE_X;
				ORA(memory[addr]);
            }
            continue;
            break;
			
		case 0x16:	/* ASL ab,x */
			{
				ZPAGE_X;
				data = memory[addr];
				C = (data & 0x80) ? 1 : 0;
				Z = N = data << 1;
				memory[addr] = Z;
            }
            continue;
            break;
			
		case 0x17:	/* ASO zpage,x [unofficial - ASL then ORA with Acc] */
			{
				ZPAGE_X;
				data = memory[addr];
				C = (data & 0x80) ? 1 : 0;
				data = (data << 1);
				memory[addr] = data; 
				Z = N = A |= data;
            }
            continue;
            break;
			
		case 0x18:	/* CLC */
			{
				C = 0;
            }
            continue;
            break;
			
		case 0x19:	/* ORA abcd,y */
			{
				ABSOLUTE_Y;
				NCYCLES_Y;
				ORA(GetByte(addr));
            }
            continue;
            break;
			
		case 0x1a:	/* NOP (1 byte) [unofficial] */
			continue;
			break;
			
		case 0x1b:	/* ASO abcd,y [unofficial - ASL then ORA with Acc] */
			{
				ABSOLUTE_Y;
				data = GetByte(addr);
				C = (data & 0x80) ? 1 : 0;
				data = (data << 1);
				PutByte(addr, data); 
				Z = N = A |= data;
            }
            continue;
            break;
			
		case 0x1c:	/* SKW [unofficial - skip word] !RS! */
			{
				PC += 2;
            }
            continue;
            break;
			
		case 0x1d:	/* ORA abcd,x */
			{
				ABSOLUTE_X;
				NCYCLES_X;
				ORA(GetByte(addr));
            }
            continue;
            break;
			
		case 0x1e:	/* ASL abcd,x */
			{
				ABSOLUTE_X;
				data = GetByte(addr);
				C = (data & 0x80) ? 1 : 0;
				Z = N = data << 1;
				PutByte(addr, Z);
            }
            continue;
            break;
			
		case 0x1f:	/* ASO abcd,x [unofficial - ASL then ORA with Acc] */
			{
				ABSOLUTE_X;
				data = GetByte(addr);
				C = (data & 0x80) ? 1 : 0;
				data = (data << 1);
				PutByte(addr, data); 
				Z = N = A |= data;
            }
            continue;
            break;
			
		case 0x20:	/* JSR abcd */
			{
				UWORD retadr = PC + 1;
#ifdef MONITOR_BREAK
				memmove(&remember_JMP[0], &remember_JMP[1], 2 * (REMEMBER_JMP_STEPS - 1));
				remember_JMP[REMEMBER_JMP_STEPS - 1] = PC - 1;
				ret_nesting++;
#endif
				memory[0x0100 + S--] = retadr >> 8;
				memory[0x0100 + S--] = (UBYTE) retadr;
				PC = (memory[PC + 1] << 8) | memory[PC];
			}
			continue;
			break;
			
		case 0x21:	/* AND (ab,x) */
			{
				INDIRECT_X;
				AND(GetByte(addr));
            }
            continue;
            break;
			
		case 0x22:	/* CIM [unofficial - crash intermediate] */
			goto	cpucrash;
			break;
			
			
			/* UNDOCUMENTED INSTRUCTIONS (Part III)
			RLA [unofficial - ROL Mem, then AND with A]
			Changes 26th April 1998 - tested on real Atari:
			*/
		case 0x23:	/* RLA (ab,x) [unofficial - ROL Mem, then AND with A] */
			{
				INDIRECT_X;
				/* RLA */
				data = GetByte(addr);
				if (C) {
					C = (data & 0x80) ? 1 : 0;
					data = (data << 1) | 1;
				}
				else {
					C = (data & 0x80) ? 1 : 0;
					data = (data << 1);
				}
				PutByte(addr, data);
				Z = N = A &= data;
            }
            continue;
            break;
			
		case 0x24:	/* BIT ab */
			{
				ZPAGE;
				N = memory[addr];
				V = N & 0x40;
				Z = (A & N);
            }
            continue;
            break;
			
		case 0x25:	/* AND ab */
			{
				ZPAGE;
				AND(memory[addr]);
            }
            continue;
            break;
			
		case 0x26:	/* ROL ab */
			{
				ZPAGE;
				data = memory[addr];
				if (C) {
					C = (data & 0x80) ? 1 : 0;
					Z = N = (data << 1) | 1;
				}
				else {
					C = (data & 0x80) ? 1 : 0;
					Z = N = (data << 1);
				}
				memory[addr] = Z;
            }
            continue;
            break;
			
		case 0x27:	/* RLA zpage [unofficial - ROL Mem, then AND with A] */
			{
				ZPAGE;
				/* RLA_ZPAGE */
				data = memory[addr];
				if (C) {
					C = (data & 0x80) ? 1 : 0;
					data = (data << 1) | 1;
				}
				else {
					C = (data & 0x80) ? 1 : 0;
					data = (data << 1);
				}
				memory[addr] = data;
				Z = N = A &= data;
            }
            continue;
            break;
			
		case 0x28:	/* PLP */
			{
				PLP;
				CPUCHECKIRQ;
            }
            continue;
            break;
			
		case 0x29:	/* AND #ab */
			{
				AND(memory[PC++]);
            }
            continue;
            break;
			
		case 0x2a:	/* ROL */
			{
				if (C) {
					C = (A & 0x80) ? 1 : 0;
					Z = N = A = (A << 1) | 1;
				}
				else {
					C = (A & 0x80) ? 1 : 0;
					Z = N = A = (A << 1);
				}
            }
            continue;
            break;
			
		case 0x2b:	/* RLA #ab [unofficial - ROL Mem, then AND with A] */
			{
				/* !!! Tested on real Atari => AND #ab */
				AND(memory[PC++]);
            }
            continue;
            break;
			
		case 0x2c:	/* BIT abcd */
			{
				ABSOLUTE;
				N = GetByte(addr);
				V = N & 0x40;
				Z = (A & N);
            }
            continue;
            break;
			
		case 0x2d:	/* AND abcd */
			{
				ABSOLUTE;
				AND(GetByte(addr));
            }
            continue;
            break;
			
		case 0x2e:	/* ROL abcd */
			{
				ABSOLUTE;
				data = GetByte(addr);
				if (C) {
					C = (data & 0x80) ? 1 : 0;
					Z = N = (data << 1) | 1;
				}
				else {
					C = (data & 0x80) ? 1 : 0;
					Z = N = (data << 1);
				}
				PutByte(addr, Z);
            }
            continue;
            break;
			
		case 0x2f:	/* RLA abcd [unofficial - ROL Mem, then AND with A] */
			{
				ABSOLUTE;
				/* RLA */
				data = GetByte(addr);
				if (C) {
					C = (data & 0x80) ? 1 : 0;
					data = (data << 1) | 1;
				}
				else {
					C = (data & 0x80) ? 1 : 0;
					data = (data << 1);
				}
				PutByte(addr, data);
				Z = N = A &= data;
            }
            continue;
            break;
			
		case 0x30:	/* BMI */
			{
				if (N & 0x80) {
					SBYTE sdata = memory[PC];
					NCYCLES_JMP;
					PC += (SWORD) sdata;
				}
				PC++;
            }
            continue;
            break;
			
		case 0x31:	/* AND (ab),y */
			{
				INDIRECT_Y;
				NCYCLES_Y;
				AND(GetByte(addr));
            }
            continue;
            break;
			
		case 0x32:	/* CIM [unofficial - crash intermediate] */
			goto cpucrash;
			break;
			
			
		case 0x33:	/* RLA (ab),y [unofficial - ROL Mem, then AND with A] */
			{
				INDIRECT_Y;
				/* RLA */
				data = GetByte(addr);
				if (C) {
					C = (data & 0x80) ? 1 : 0;
					data = (data << 1) | 1;
				}
				else {
					C = (data & 0x80) ? 1 : 0;
					data = (data << 1);
				}
				PutByte(addr, data);
				Z = N = A &= data;
            }
            continue;
            break;
			
		case 0x34:	/* SKB [unofficial - skip byte] */
			{
				PC++;
            }
            continue;
            break;
			
		case 0x35:	/* AND ab,x */
			{
				ZPAGE_X;
				AND(memory[addr]);
            }
            continue;
            break;
			
		case 0x36:	/* ROL ab,x */
			{
				ZPAGE_X;
				data = memory[addr];
				if (C) {
					C = (data & 0x80) ? 1 : 0;
					Z = N = (data << 1) | 1;
				}
				else {
					C = (data & 0x80) ? 1 : 0;
					Z = N = (data << 1);
				}
				memory[addr] = Z;
            }
            continue;
            break;
			
		case 0x37:	/* RLA zpage,x [unofficial - ROL Mem, then AND with A] */
			{
				ZPAGE_X;
				/* RLA_ZPAGE */
				data = memory[addr];
				if (C) {
					C = (data & 0x80) ? 1 : 0;
					data = (data << 1) | 1;
				}
				else {
					C = (data & 0x80) ? 1 : 0;
					data = (data << 1);
				}
				memory[addr] = data;
				Z = N = A &= data;
            }
            continue;
            break;
			
		case 0x38:	/* SEC */
			{
				C = 1;
            }
            continue;
            break;
			
		case 0x39:	/* AND abcd,y */
			{
				ABSOLUTE_Y;
				NCYCLES_Y;
				AND(GetByte(addr));
            }
            continue;
            break;
			
		case 0x3a:	/* NOP (1 byte) [unofficial] */
            continue;
            break;
			
		case 0x3b:	/* RLA abcd,y [unofficial - ROL Mem, then AND with A] */
			{
				ABSOLUTE_Y;
				/* RLA */
				data = GetByte(addr);
				if (C) {
					C = (data & 0x80) ? 1 : 0;
					data = (data << 1) | 1;
				}
				else {
					C = (data & 0x80) ? 1 : 0;
					data = (data << 1);
				}
				PutByte(addr, data);
				Z = N = A &= data;
            }
            continue;
            break;
			
		case 0x3c:	/* SKW [unofficial - skip word] */
			{
				PC += 2;
            }
            continue;
            break;
			
		case 0x3d:	/* AND abcd,x */
			{
				ABSOLUTE_X;
				NCYCLES_X;
				AND(GetByte(addr));
            }
            continue;
            break;
			
		case 0x3e:	/* ROL abcd,x */
			{
				ABSOLUTE_X;
				data = GetByte(addr);
				if (C) {
					C = (data & 0x80) ? 1 : 0;
					Z = N = (data << 1) | 1;
				}
				else {
					C = (data & 0x80) ? 1 : 0;
					Z = N = (data << 1);
				}
				PutByte(addr, Z);
            }
            continue;
            break;
			
		case 0x3f:	/* RLA abcd,x [unofficial - ROL Mem, then AND with A] */
			{
				ABSOLUTE_X;
				/* RLA */
				data = GetByte(addr);
				if (C) {
					C = (data & 0x80) ? 1 : 0;
					data = (data << 1) | 1;
				}
				else {
					C = (data & 0x80) ? 1 : 0;
					data = (data << 1);
				}
				PutByte(addr, data);
				Z = N = A &= data;
            }
            continue;
            break;
			
		case 0x40:	/* RTI */
			{
				PLP;
				data = memory[0x0100 + ++S];
				PC = (memory[0x0100 + ++S] << 8) | data;
				CPUCHECKIRQ;
#ifdef MONITOR_BREAK
				if (break_ret && ret_nesting <= 0)
					break_step = 1;
				ret_nesting--;
#endif
			}
			continue;
			break;
			
		case 0x41:	/* EOR (ab,x) */
			{
				INDIRECT_X;
				EOR(GetByte(addr));
            }
            continue;
            break;
			
		case 0x42:
			goto cpucrash;
			break;
			
			
		case 0x43:	/* LSE (ab,x) [unofficial - LSR then EOR result with A] */
			{
				INDIRECT_X;
				data = GetByte(addr);
				C = data & 1;
				data = data >> 1;
				PutByte(addr, data);
				Z = N = A ^= data;
            }
            continue;
            break;
			
		case 0x44:	/* SKB [unofficial - skip byte] */
			{
				PC++;
            }
            continue;
            break;
			
		case 0x45:	/* EOR ab */
			{
				ZPAGE;
				EOR(memory[addr]);
            }
            continue;
            break;
			
		case 0x46:	/* LSR ab */
			{
				ZPAGE;
				data = memory[addr];
				C = data & 1;
				Z = data >> 1;
				N = 0;
				memory[addr] = Z;
            }
            continue;
            break;
			
		case 0x47:	/* LSE zpage [unofficial - LSR then EOR result with A] */
			{
				ZPAGE;
				data = memory[addr];
				C = data & 1;
				data = data >> 1;
				memory[addr] = data;
				Z = N = A ^= data;
            }
            continue;
            break;
			
		case 0x48:	/* PHA */
			{
				memory[0x0100 + S--] = A;
            }
            continue;
            break;
			
		case 0x49:	/* EOR #ab */
			{
				EOR(memory[PC++]);
            }
            continue;
            break;
			
		case 0x4a:	/* LSR */
			{
				C = A & 1;
				A = A >> 1;
				N = 0;
				Z = A;
            }
            continue;
            break;
			
		case 0x4b:	/* ALR #ab [unofficial - Acc AND Data, LSR result] */
			{
				data = A & memory[PC++];
				C = data & 1;
				Z = N = A = (data >> 1);
            }
            continue;
            break;
			
		case 0x4c:	/* JMP abcd */
			{
#ifdef MONITOR_BREAK
				memmove(&remember_JMP[0], &remember_JMP[1], 2 * (REMEMBER_JMP_STEPS - 1));
				remember_JMP[REMEMBER_JMP_STEPS - 1] = PC - 1;
#endif
				PC = (memory[PC + 1] << 8) | memory[PC];
            }
            continue;
            break;
			
		case 0x4d:	/* EOR abcd */
			{
				ABSOLUTE;
				EOR(GetByte(addr));
            }
            continue;
            break;
			
		case 0x4e:	/* LSR abcd */
			{
				ABSOLUTE;
				data = GetByte(addr);
				C = data & 1;
				Z = data >> 1;
				N = 0;
				PutByte(addr, Z);
            }
            continue;
            break;
			
		case 0x4f:	/* LSE abcd [unofficial - LSR then EOR result with A] */
			{
				ABSOLUTE;
				data = GetByte(addr);
				C = data & 1;
				data = data >> 1;
				PutByte(addr, data);
				Z = N = A ^= data;
            }
            continue;
            break;
			
		case 0x50:	/* BVC */
			{
				if (!V) {
					SBYTE sdata = memory[PC];
					NCYCLES_JMP;
					PC += (SWORD) sdata;
				}
				PC++;
            }
            continue;
            break;
			
		case 0x51:	/* EOR (ab),y */
			{
				INDIRECT_Y;
				NCYCLES_Y;
				EOR(GetByte(addr));
            }
            continue;
            break;
			
		case 0x52:
			goto cpucrash;
			break;
			
			
		case 0x53:	/* LSE (ab),y [unofficial - LSR then EOR result with A] */
			{
				INDIRECT_Y;
				data = GetByte(addr);
				C = data & 1;
				data = data >> 1;
				PutByte(addr, data);
				Z = N = A ^= data;
            }
            continue;
            break;
			
		case 0x54:	/* SKB [unofficial - skip byte] */
			{
				PC++;
            }
            continue;
            break;
			
		case 0x55:	/* EOR ab,x */
			{
				ZPAGE_X;
				EOR(memory[addr]);
            }
            continue;
            break;
			
		case 0x56:	/* LSR ab,x */
			{
				ZPAGE_X;
				data = memory[addr];
				C = data & 1;
				Z = data >> 1;
				N = 0;
				memory[addr] = Z;
            }
            continue;
            break;
			
		case 0x57:	/* LSE zpage,x [unofficial - LSR then EOR result with A] */
			{
				ZPAGE_X;
				data = memory[addr];
				C = data & 1;
				data = data >> 1;
				memory[addr] = data;
				Z = N = A ^= data;
            }
            continue;
            break;
			
		case 0x58:	/* CLI */
			{
				ClrI;
				CPUCHECKIRQ;
            }
            continue;
            break;
			
		case 0x59:	/* EOR abcd,y */
			{
				ABSOLUTE_Y;
				NCYCLES_Y;
				EOR(GetByte(addr));
            }
            continue;
            break;
			
			
		case 0x5a:	/* NOP (1 byte) [unofficial] */
            continue;
            break;
			
		case 0x5b:	/* LSE abcd,y [unofficial - LSR then EOR result with A] */
			{
				ABSOLUTE_Y;
				data = GetByte(addr);
				C = data & 1;
				data = data >> 1;
				PutByte(addr, data);
				Z = N = A ^= data;
            }
            continue;
            break;
			
		case 0x5c:	/* SKW [unofficial - skip word] */
			{
				PC += 2;
            }
            continue;
            break;
			
			
		case 0x5d:	/* EOR abcd,x */
			{
				ABSOLUTE_X;
				NCYCLES_X;
				EOR(GetByte(addr));
            }
            continue;
            break;
			
		case 0x5e:	/* LSR abcd,x */
			{
				ABSOLUTE_X;
				data = GetByte(addr);
				C = data & 1;
				Z = data >> 1;
				N = 0;
				PutByte(addr, Z);
            }
            continue;
            break;
			
		case 0x5f:	/* LSE abcd,x [unofficial - LSR then EOR result with A] */
			{
				ABSOLUTE_X;
				data = GetByte(addr);
				C = data & 1;
				data = data >> 1;
				PutByte(addr, data);
				Z = N = A ^= data;
            }
            continue;
            break;
			
		case 0x60:	/* RTS */
			{
				data = memory[0x0100 + ++S];
				PC = ((memory[0x0100 + ++S] << 8) | data) + 1;
#ifdef MONITOR_BREAK
				if (break_ret && ret_nesting <= 0)
					break_step = 1;
				ret_nesting--;
#endif
			}
			continue;
			break;
			
		case 0x61:	/* ADC (ab,x) */
			{
				INDIRECT_X;
				data = GetByte(addr);
				goto adc;
			}
			break;
			
			
		case 0x62:
			goto cpucrash;
			break;
			
			
		case 0x63:	/* RRA (ab,x) [unofficial - ROR Mem, then ADC to Acc] */
			{
				INDIRECT_X;
				/* RRA */
				data = GetByte(addr);
				if (C) {
					C = data & 1;
					data = (data >> 1) | 0x80;
				}
				else {
					C = data & 1;
					data = (data >> 1);
				}
				PutByte(addr, data);
				goto adc;
			}
			break;
			
			
		case 0x64:	/* SKB [unofficial - skip byte] */
			{
				PC++;
            }
            continue;
            break;
			
		case 0x65:	/* ADC ab */
			{
				ZPAGE;
				data = memory[addr];
				goto adc;
			}
			break;
			
			
		case 0x66:	/* ROR ab */
			{
				ZPAGE;
				data = memory[addr];
				if (C) {
					C = data & 1;
					Z = N = (data >> 1) | 0x80;
				}
				else {
					C = data & 1;
					Z = N = (data >> 1);
				}
				memory[addr] = Z;
            }
            continue;
            break;
			
		case 0x67:/* RRA zpage [unofficial - ROR Mem, then ADC to Acc] */
			{
				ZPAGE;
				/* RRA_ZPAGE */
				data = memory[addr];
				if (C) {
					C = data & 1;
					data = (data >> 1) | 0x80;
				}
				else {
					C = data & 1;
					data = (data >> 1);
				}
				memory[addr] = data;
				goto adc;
			}
			break;
			
			
		case 0x68:	/* PLA */
			{
				Z = N = A = memory[0x0100 + ++S];
            }
            continue;
            break;
			
		case 0x69:	/* ADC #ab */
			{
				data = memory[PC++];
				goto adc;
			}
			break;
			
			
		case 0x6a:	/* ROR */
			{
				if (C) {
					C = A & 1;
					Z = N = A = (A >> 1) | 0x80;
				}
				else {
					C = A & 1;
					Z = N = A = (A >> 1);
				}
            }
            continue;
            break;
			
		case 0x6b:	/* ARR #ab [unofficial - Acc AND Data, ROR result] */
			{
				data = A & memory[PC++];
				if (C) {
					C = data & 1;
					Z = N = A = (data >> 1) | 0x80;
				}
				else {
					C = data & 1;
					Z = N = A = (data >> 1);
				}
            }
            continue;
            break;
			
		case 0x6c:	/* JMP (abcd) */
			{
#ifdef MONITOR_BREAK
				memmove(&remember_JMP[0], &remember_JMP[1], 2 * (REMEMBER_JMP_STEPS - 1));
				remember_JMP[REMEMBER_JMP_STEPS - 1] = PC - 1;
#endif
				addr = (memory[PC + 1] << 8) | memory[PC];
#ifdef CPU65C02
				PC = (memory[addr + 1] << 8) | memory[addr];
#else							/* original 6502 had a bug in jmp (addr) when addr crossed page boundary */
				if ((UBYTE) addr == 0xff)
					PC = (memory[addr & ~0xff] << 8) | memory[addr];
				else
					PC = (memory[addr + 1] << 8) | memory[addr];
#endif
			}
			continue;
			break;
			
		case 0x6d:	/* ADC abcd */
			{
				ABSOLUTE;
				data = GetByte(addr);
				goto adc;
			}
			break;
			
			
			
		case 0x6e:	/* ROR abcd */
			{
				ABSOLUTE;
				data = GetByte(addr);
				if (C) {
					C = data & 1;
					Z = N = (data >> 1) | 0x80;
				}
				else {
					C = data & 1;
					Z = N = (data >> 1);
				}
				PutByte(addr, Z);
            }
            continue;
            break;
			
		case 0x6f:	/* RRA abcd [unofficial - ROR Mem, then ADC to Acc] */
			{
				ABSOLUTE;
				/* RRA */
				data = GetByte(addr);
				if (C) {
					C = data & 1;
					data = (data >> 1) | 0x80;
				}
				else {
					C = data & 1;
					data = (data >> 1);
				}
				PutByte(addr, data);
				goto adc;
			}
			break;
			
			
		case 0x70:	/* BVS */
			{
				if (V) {
					SBYTE sdata = memory[PC];
					NCYCLES_JMP;
					PC += (SWORD) sdata;
				}
				PC++;
            }
            continue;
            break;
			
		case 0x71:	/* ADC (ab),y */
			{
				INDIRECT_Y;
				NCYCLES_Y;
				data = GetByte(addr);
				goto adc;
			}
			break;
			
			
		case 0x72:
			goto cpucrash;
			break;
			
		case 0x73:	/* RRA (ab),y [unofficial - ROR Mem, then ADC to Acc] */
			{
				INDIRECT_Y;
				/* RRA */
				data = GetByte(addr);
				if (C) {
					C = data & 1;
					data = (data >> 1) | 0x80;
				}
				else {
					C = data & 1;
					data = (data >> 1);
				}
				PutByte(addr, data);
				goto adc;
			}
			break;
			
		case 0x74:	/* SKB [unofficial - skip byte] */
			{
				PC++;
            }
            continue;
            break;
			
		case 0x75:	/* ADC ab,x */
			{
				ZPAGE_X;
				data = memory[addr];
				goto adc;
			}
			break;
			
			
		case 0x76:	/* ROR ab,x */
			{
				ZPAGE_X;
				data = memory[addr];
				if (C) {
					C = data & 1;
					Z = N = (data >> 1) | 0x80;
				}
				else {
					C = data & 1;
					Z = N = (data >> 1);
				}
				memory[addr] = Z;
            }
            continue;
            break;
			
		case 0x77:	/* RRA zpage,x [unofficial - ROR Mem, then ADC to Acc] */
			{
				ZPAGE_X;
				/* RRA_ZPAGE */
				data = memory[addr];
				if (C) {
					C = data & 1;
					data = (data >> 1) | 0x80;
				}
				else {
					C = data & 1;
					data = (data >> 1);
				}
				memory[addr] = data;
				goto adc;
			}
			break;
			
			
		case 0x78:	/* SEI */
			{
				SetI;
            }
            continue;
            break;
			
		case 0x79:
			{
				/* ADC abcd,y */
				ABSOLUTE_Y;
				NCYCLES_Y;
				data = GetByte(addr);
				goto adc;
			}
			break;
			
			
		case 0x7a:	/* NOP (1 byte) [unofficial] */
            continue;
            break;
			
		case 0x7b:	/* RRA abcd,y [unofficial - ROR Mem, then ADC to Acc] */
			{
				ABSOLUTE_Y;
				/* RRA */
				data = GetByte(addr);
				if (C) {
					C = data & 1;
					data = (data >> 1) | 0x80;
				}
				else {
					C = data & 1;
					data = (data >> 1);
				}
				PutByte(addr, data);
				goto adc;
			}
			break;
			
			
		case 0x7c:	/* SKW [unofficial - skip word] */
			{
				PC += 2;
            }
            continue;
            break;
			
		case 0x7d:	/* ADC abcd,x */
			{
				ABSOLUTE_X;
				NCYCLES_X;
				data = GetByte(addr);
				goto adc;
			}
			break;
			
			
		case 0x7e:	/* ROR abcd,x */
			{
				ABSOLUTE_X;
				data = GetByte(addr);
				if (C) {
					C = data & 1;
					Z = N = (data >> 1) | 0x80;
				}
				else {
					C = data & 1;
					Z = N = (data >> 1);
				}
				PutByte(addr, Z);
            }
            continue;
            break;
			
		case 0x7f:	/* RRA abcd,x [unofficial - ROR Mem, then ADC to Acc] */
			{
				ABSOLUTE_X;
				/* RRA */
				data = GetByte(addr);
				if (C) {
					C = data & 1;
					data = (data >> 1) | 0x80;
				}
				else {
					C = data & 1;
					data = (data >> 1);
				}
				PutByte(addr, data);
				goto adc;
			}
			break;
			
			
		case 0x80:	/* SKB [unofficial - skip byte] */
			{
				PC++;
            }
            continue;
            break;
			
		case 0x81:	/* STA (ab,x) */
			{
				INDIRECT_X;
				PutByte(addr, A);
            }
            continue;
            break;
			
		case 0x82:	/* SKB [unofficial - skip byte] */
			{
				PC++;
            }
            continue;
            break;
			
		case 0x83:	/* AXS (ab,x) [unofficial - Store result A AND X] */
			{
				INDIRECT_X;
				Z = N = A & X;
				PutByte(addr, Z);
            }
            continue;
            break;
			
		case 0x84:	/* STY ab */
			{
				ZPAGE;
				memory[addr] = Y;
            }
            continue;
            break;
			
		case 0x85:	/* STA ab */
			{
				ZPAGE;
				memory[addr] = A;
            }
            continue;
            break;
			
		case 0x86:	/* STX ab */
			{
				ZPAGE;
				memory[addr] = X;
            }
            continue;
            break;
			
		case 0x87:	/* AXS zpage [unofficial - Store result A AND X] */
			{
				ZPAGE;
				Z = N = memory[addr] = A & X;
            }
            continue;
            break;
			
		case 0x88:	/* DEY */
			{
				Z = N = --Y;
            }
            continue;
            break;
			
		case 0x89:	/* SKB [unofficial - skip byte] */
			{
				PC++;
            }
            continue;
            break;
			
		case 0x8a:	/* TXA */
			{
				Z = N = A = X;
            }
            continue;
            break;
			
		case 0x8b:	/* XAA #ab [unofficial - X AND Mem to Acc] */
			{
				/* !!! Tested on real Atari => AND #ab */
				AND(memory[PC++]);
            }
            continue;
            break;
			
		case 0x8c:	/* STY abcd */
			{
				ABSOLUTE;
				PutByte(addr, Y);
            }
            continue;
            break;
			
		case 0x8d:	/* STA abcd */
			{
				ABSOLUTE;
				PutByte(addr, A);
            }
            continue;
            break;
			
		case 0x8e:	/* STX abcd */
			{
				ABSOLUTE;
				PutByte(addr, X);
            }
            continue;
            break;
			
		case 0x8f:	/* AXS abcd [unofficial - Store result A AND X] */
			{
				ABSOLUTE;
				Z = N = A & X;
				PutByte(addr, Z);
            }
            continue;
            break;
			
		case 0x90:	/* BCC */
			{
				if (!C) {
					SBYTE sdata = memory[PC];
					NCYCLES_JMP;
					PC += (SWORD) sdata;
				}
				PC++;
            }
            continue;
            break;
			
		case 0x91:	/* STA (ab),y */
			{
				INDIRECT_Y;
				PutByte(addr, A);
            }
            continue;
            break;
			
		case 0x92:
			goto cpucrash;
			break;
			
			
		case 0x93:	/* AXS (ab),y [unofficial - Store result A AND X] */
			{
				INDIRECT_Y;
				Z = N = A & X;
				PutByte(addr, Z);
            }
            continue;
            break;
			
		case 0x94:	/* STY ab,x */
			{
				ZPAGE_X;
				memory[addr] = Y;
            }
            continue;
            break;
			
		case 0x95:	/* STA ab,x */
			{
				ZPAGE_X;
				memory[addr] = A;
            }
            continue;
            break;
			
		case 0x96:	/* STX ab,y */
			{
				ZPAGE_Y;
				PutByte(addr, X);
            }
            continue;
            break;
			
		case 0x97:	/* AXS zpage,y [unofficial - Store result A AND X] */
			{
				ZPAGE_Y;
				Z = N = memory[addr] = A & X;
            }
            continue;
            break;
			
		case 0x98:	/* TYA */
			{
				Z = N = A = Y;
            }
            continue;
            break;
			
		case 0x99:	/* STA abcd,y */
			{
				ABSOLUTE_Y;
				PutByte(addr, A);
            }
            continue;
            break;
			
		case 0x9a:	/* TXS */
			{
				S = X;
            }
            continue;
            break;
			
		case 0x9b:	/* XAA abcd,y [unofficial - X AND Mem to Acc] */
			{
				/* !!! Tested on real Atari => Store result A AND X AND 0x01 to mem (no modify FLAG) !!!!! */
				/* address mode ABSOLUTE Y */
				ABSOLUTE_Y;
				PutByte( addr, A & X & 0x01 );
            }
            continue;
            break;
			
		case 0x9c:	/* SKW [unofficial - skip word] */
			{
				PC += 2;
            }
            continue;
            break;
			
		case 0x9d:	/* STA abcd,x */
			{
				ABSOLUTE_X;
				PutByte(addr, A);
            }
            continue;
            break;
			
		case 0x9e:	/* MKX abcd [unofficial - X AND #$04 to X] */
			{
				/* !!! Tested on real Atari => Store result A AND X AND 0x01 to mem (no modify FLAG) !!!!! */
				/* address mode ABSOLUTE Y */
				ABSOLUTE_Y;
				PutByte( addr, A & X & 0x01 );
            }
            continue;
            break;
			
		case 0x9f:	/* MKA abcd [unofficial - Acc AND #$04 to Acc] */
			{
				/* !!! Tested on real Atari => Store result A AND X AND 0x01 to mem (no modify FLAG) !!!!! */
				/* address mode ABSOLUTE Y */
				ABSOLUTE_Y;
				PutByte( addr, A & X & 0x01 );
            }
            continue;
            break;
			
		case 0xa0:	/* LDY #ab */
			{
				LDY(memory[PC++]);
            }
            continue;
            break;
			
		case 0xa1:	/* LDA (ab,x) */
			{
				INDIRECT_X;
				LDA(GetByte(addr));
            }
            continue;
            break;
			
		case 0xa2:	/* LDX #ab */
			{
				LDX(memory[PC++]);
            }
            continue;
            break;
			
		case 0xa3:	/* LAX (ind,x) [unofficial] */
			{
				INDIRECT_X;
				Z = N = X = A = GetByte(addr);
            }
            continue;
            break;
			
		case 0xa4:	/* LDY ab */
			{
				ZPAGE;
				LDY(memory[addr]);
            }
            continue;
            break;
			
		case 0xa5:	/* LDA ab */
			{
				ZPAGE;
				LDA(memory[addr]);
            }
            continue;
            break;
			
		case 0xa6:	/* LDX ab */
			{
				ZPAGE;
				LDX(memory[addr]);
            }
            continue;
            break;
			
		case 0xa7:	/* LAX zpage [unofficial] */
			{
				ZPAGE;
				Z = N = X = A = GetByte(addr);
            }
            continue;
            break;
			
		case 0xa8:	/* TAY */
			{
				Z = N = Y = A;
            }
            continue;
            break;
			
		case 0xa9:	/* LDA #ab */
			{
				LDA(memory[PC++]);
            }
            continue;
            break;
			
		case 0xaa:	/* TAX */
			{
				Z = N = X = A;
            }
            continue;
            break;
			
		case 0xab:	/* OAL #ab [unofficial - ORA Acc with #$EE, then AND with data, then TAX] */
			{
				/* !!! Tested on real Atari => AND #ab, TAX */
				A &= memory[PC++];
				Z = N = X = A;
            }
            continue;
            break;
			
		case 0xac:	/* LDY abcd */
			{
				ABSOLUTE;
				LDY(GetByte(addr));
            }
            continue;
            break;
			
		case 0xad:	/* LDA abcd */
			{
				ABSOLUTE;
				LDA(GetByte(addr));
            }
            continue;
            break;
			
		case 0xae:	/* LDX abcd */
			{
				ABSOLUTE;
				LDX(GetByte(addr));
            }
            continue;
            break;
			
		case 0xaf:	/* LAX absolute [unofficial] */
			{
				ABSOLUTE;
				Z = N = X = A = GetByte(addr);
            }
            continue;
            break;
			
		case 0xb0:	/* BCS */
			{
				if (C) {
					SBYTE sdata = memory[PC];
					NCYCLES_JMP;
					PC += (SWORD) sdata;
				}
				PC++;
            }
            continue;
            break;
			
		case 0xb1:	/* LDA (ab),y */
			{
				INDIRECT_Y;
				NCYCLES_Y;
				LDA(GetByte(addr));
            }
            continue;
            break;
			
		case 0xb2:
			goto cpucrash;
			break;
			
			
		case 0xb3:	/* LAX (ind),y [unofficial] */
			{
				INDIRECT_Y;
				Z = N = X = A = GetByte(addr);
            }
            continue;
            break;
			
		case 0xb4:	/* LDY ab,x */
			{
				ZPAGE_X;
				LDY(memory[addr]);
            }
            continue;
            break;
			
		case 0xb5:	/* LDA ab,x */
			{
				ZPAGE_X;
				LDA(memory[addr]);
            }
            continue;
            break;
			
		case 0xb6:	/* LDX ab,y */
			{
				ZPAGE_Y;
				LDX(GetByte(addr));
            }
            continue;
            break;
			
		case 0xb7:	/* LAX zpage,y [unofficial] */
			{
				ZPAGE_Y;
				Z = N = X = A = GetByte(addr);
            }
            continue;
            break;
			
		case 0xb8:	/* CLV */
			{
				V = 0;
            }
            continue;
            break;
			
		case 0xb9:	/* LDA abcd,y */
			{
				ABSOLUTE_Y;
				NCYCLES_Y;
				LDA(GetByte(addr));
            }
            continue;
            break;
			
		case 0xba:	/* TSX */
			{
				Z = N = X = S;
            }
            continue;
            break;
			
		case 0xbb:	/* AXA abcd,y [unofficial - Store Mem AND #$FD to Acc and X, then set stackpoint to value (Acc - 4) */
			{
				ABSOLUTE_Y;
				Z = N = A = X = GetByte(addr) & 0xfd;
				S = (UBYTE) (A - 4);
            }
            continue;
            break;
			
		case 0xbc:	/* LDY abcd,x */
			{
				ABSOLUTE_X;
				NCYCLES_X;
				LDY(GetByte(addr));
            }
            continue;
            break;
			
		case 0xbd:	/* LDA abcd,x */
			{
				ABSOLUTE_X;
				NCYCLES_X;
				LDA(GetByte(addr));
            }
            continue;
            break;
			
		case 0xbe:	/* LDX abcd,y */
			{
				ABSOLUTE_Y;
				NCYCLES_Y;
				LDX(GetByte(addr));
            }
            continue;
            break;
			
		case 0xbf:	/* LAX absolute,y [unofficial] */
			{
				ABSOLUTE_Y;
				Z = N = X = A = GetByte(addr);
            }
            continue;
            break;
			
		case 0xc0:	/* CPY #ab */
			{
				CPY(memory[PC++]);
            }
            continue;
            break;
			
		case 0xc1:	/* CMP (ab,x) */
			{
				INDIRECT_X;
				CMP(GetByte(addr));
            }
            continue;
            break;
			
		case 0xc2:	/* SKB [unofficial - skip byte] */
			{
				PC++;
            }
            continue;
            break;
			
		case 0xc3:	/* DCM (ab,x) [unofficial - DEC Mem then CMP with Acc] */
			{
				INDIRECT_X;
				/* DCM */
				data = GetByte(addr) - 1;
				PutByte(addr, data);
				CMP(data);
            }
            continue;
            break;
			
		case 0xc4:	/* CPY ab */
			{
				ZPAGE;
				CPY(memory[addr]);
            }
            continue;
            break;
			
			
		case 0xc5:	/* CMP ab */
			{
				ZPAGE;
				CMP(memory[addr]);
            }
            continue;
            break;
			
		case 0xc6:	/* DEC ab */
			{
				ZPAGE;
				Z = N = --memory[addr];
            }
            continue;
            break;
			
		case 0xc7:	/* DCM zpage [unofficial - DEC Mem then CMP with Acc] */
			{
				ZPAGE;
				/* DCM_ZPAGE */
				data = memory[addr] = memory[addr] - 1;
				CMP(data);
            }
            continue;
            break;
			
		case 0xc8:	/* INY */
			{
				Z = N = ++Y;
            }
            continue;
            break;
			
		case 0xc9:	/* CMP #ab */
			{
				CMP(memory[PC++]);
            }
            continue;
            break;
			
		case 0xca:	/* DEX */
			{
				Z = N = --X;
            }
            continue;
            break;
			
		case 0xcb:	/* SAX #ab [unofficial - A AND X, then SBC Mem, store to X] */
			{
				X = A & X;
				data = memory[PC++];
				if (!(regP & D_FLAG)) {
					UWORD temp;
					UWORD t_data;
					t_data = data + !C;
					temp = X - t_data;
					Z = N = (UBYTE) temp;
					V = (~(X ^ t_data)) & (Z ^ X) & 0x80;
					C = (temp > 255) ? 0 : 1;
					X = Z;
				}
				else {
					int bcd1, bcd2;
					bcd1 = BCDtoDEC[X];
					bcd2 = BCDtoDEC[data];
					bcd1 = bcd1 - bcd2 - !C;
					if (bcd1 < 0)
						bcd1 = 100 - (-bcd1);
					Z = N = DECtoBCD[bcd1];
					C = (X < (data + !C)) ? 0 : 1;
					V = (Z ^ X) & 0x80;
					X = Z;
				}
            }
            continue;
            break;
			
		case 0xcc:	/* CPY abcd */
			{
				ABSOLUTE;
				CPY(GetByte(addr));
            }
            continue;
            break;
			
		case 0xcd:	/* CMP abcd */
			{
				ABSOLUTE;
				CMP(GetByte(addr));
            }
            continue;
            break;
			
		case 0xce:	/* DEC abcd */
			{
				ABSOLUTE;
				Z = N = GetByte(addr) - 1;
				PutByte(addr, Z);
            }
            continue;
            break;
			
		case 0xcf:	/* DCM abcd [unofficial - DEC Mem then CMP with Acc] */
			{
				ABSOLUTE;
				/* DCM */
				data = GetByte(addr) - 1;
				PutByte(addr, data);
				CMP(data);
            }
            continue;
            break;
			
		case 0xd0:	/* BNE */
			{
				if (Z) {
					SBYTE sdata = memory[PC];
					NCYCLES_JMP;
					PC += (SWORD) sdata;
				}
				PC++;
            }
            continue;
            break;
			
		case 0xd1:	/* CMP (ab),y */
			{
				INDIRECT_Y;
				NCYCLES_Y;
				CMP(GetByte(addr));
            }
            continue;
            break;
			
		case 0xd2:	/* ESCRTS #ab (JAM) - on Atari is here instruction CIM [unofficial] !RS! */
			{
				data = memory[PC++];
				UPDATE_GLOBAL_REGS;
				CPU_GetStatus();
				AtariEscape(data);
				CPU_PutStatus();
				UPDATE_LOCAL_REGS;
				data = memory[0x0100 + ++S];
				PC = ((memory[0x0100 + ++S] << 8) | data) + 1;
#ifdef MONITOR_BREAK
				if (break_ret && ret_nesting <= 0)
					break_step = 1;
				ret_nesting--;
#endif
			}
			continue;
			break;
			
		case 0xd3:	/* DCM (ab),y [unofficial - DEC Mem then CMP with Acc] */
			{
				INDIRECT_Y;
				/* DCM */
				data = GetByte(addr) - 1;
				PutByte(addr, data);
				CMP(data);
            }
            continue;
            break;
			
		case 0xd4:	/* SKB [unofficial - skip byte] */
			{
				PC++;
            }
            continue;
            break;
			
		case 0xd5:	/* CMP ab,x */
			{
				ZPAGE_X;
				CMP(memory[addr]);
				Z = N = A - data;
				C = (A >= data);
            }
            continue;
            break;
			
		case 0xd6:	/* DEC ab,x */
			{
				ZPAGE_X;
				Z = N = --memory[addr];
            }
            continue;
            break;
			
		case 0xd7:	/* DCM zpage,x [unofficial - DEC Mem then CMP with Acc] */
			{
				ZPAGE_X;
				/* DCM_ZPAGE */
				data = memory[addr] = memory[addr] - 1;
				CMP(data);
            }
            continue;
            break;
			
		case 0xd8:	/* CLD */
			{
				ClrD;
            }
            continue;
            break;
			
		case 0xd9:	/* CMP abcd,y */
			{
				ABSOLUTE_Y;
				NCYCLES_Y;
				CMP(GetByte(addr));
            }
            continue;
            break;
			
		case 0xda:	/* NOP (1 byte) [unofficial] */
            continue;
            break;
			
		case 0xdb:	/* DCM abcd,y [unofficial - DEC Mem then CMP with Acc] */
			{
				ABSOLUTE_Y;
				/* DCM */
				data = GetByte(addr) - 1;
				PutByte(addr, data);
				CMP(data);
            }
            continue;
            break;
			
			
		case 0xdc:	/* SKW [unofficial - skip word] */
			{
				PC += 2;
            }
            continue;
            break;
			
		case 0xdd:	/* CMP abcd,x */
			{
				ABSOLUTE_X;
				NCYCLES_X;
				CMP(GetByte(addr));
            }
            continue;
            break;
			
		case 0xde:	/* DEC abcd,x */
			{
				ABSOLUTE_X;
				Z = N = GetByte(addr) - 1;
				PutByte(addr, Z);
            }
            continue;
            break;
			
		case 0xdf:	/* DCM abcd,x [unofficial - DEC Mem then CMP with Acc] */
			{
				ABSOLUTE_X;
				/* DCM */
				data = GetByte(addr) - 1;
				PutByte(addr, data);
				CMP(data);
            }
            continue;
            break;
			
		case 0xe0:	/* CPX #ab */
			{
				CPX(memory[PC++]);
            }
            continue;
            break;
			
		case 0xe1:	/* SBC (ab,x) */
			{
				INDIRECT_X;
				data = GetByte(addr);
				goto sbc;
			}
			break;
			
			
		case 0xe2:	/* SKB [unofficial - skip byte] */
			{
				PC++;
            }
            continue;
            break;
			
		case 0xe3:	/* INS (ab,x) [unofficial - INC Mem then SBC with Acc] */
			{
				INDIRECT_X;
				/* INS */
				data = Z = N = GetByte(addr) + 1;
				PutByte(addr, data);
				goto sbc;
			}
			break;
			
			
		case 0xe4:	/* CPX ab */
			{
				ZPAGE;
				CPX(memory[addr]);
            }
            continue;
            break;
			
		case 0xe5:	/* SBC ab */
			{
				ZPAGE;
				data = memory[addr];
				goto sbc;
			}
			break;
			
			
		case 0xe6:	/* INC ab */
			{
				ZPAGE;
				Z = N = ++memory[addr];
            }
            continue;
            break;
			
		case 0xe7:	/* INS zpage [unofficial - INC Mem then SBC with Acc] */
			{
				ZPAGE;
				/* INS_ZPAGE */
				data = Z = N = memory[addr] + 1;
				memory[addr] = data;
				goto sbc;
			}
			break;
			
			
		case 0xe8:	/* INX */
			{
				Z = N = ++X;
            }
            continue;
            break;
			
		case 0xe9:	/* SBC #ab */
			{
				data = memory[PC++];
				goto sbc;
			}
			break;
			
			
		case 0xea:	/* NOP */
            continue;
            break;
			
		case 0xeb:	/* SBC #ab [unofficial] */
			{
				data = memory[PC++];
				goto sbc;
			}
			break;
			
			
		case 0xec:	/* CPX abcd */
			{
				ABSOLUTE;
				CPX(GetByte(addr));
            }
            continue;
            break;
			
		case 0xed:	/* SBC abcd */
			{
				ABSOLUTE;
				data = GetByte(addr);
				goto sbc;
			}
			break;
			
			
		case 0xee:	/* INC abcd */
			{
				ABSOLUTE;
				Z = N = GetByte(addr) + 1;
				PutByte(addr, Z);
            }
            continue;
            break;
			
		case 0xef:	/* INS abcd [unofficial - INC Mem then SBC with Acc] */
			{
				ABSOLUTE;
				/* INS */
				data = Z = N = GetByte(addr) + 1;
				PutByte(addr, data);
				goto sbc;
			}
			break;
			
			
		case 0xf0:	/* BEQ */
			{
				if (!Z) {
					SBYTE sdata = memory[PC];
					NCYCLES_JMP;
					PC += (SWORD) sdata;
				}
				PC++;
            }
            continue;
            break;
			
		case 0xf1:	/* SBC (ab),y */
			{
				INDIRECT_Y;
				NCYCLES_Y;
				data = GetByte(addr);
				goto sbc;
			}
			break;
			
			
		case 0xf2:	/* ESC #ab (JAM) - on Atari is here instruction CIM [unofficial] !RS! */
			{
				/* opcode_ff: ESC #ab - opcode FF is now used for INS [unofficial] instruction !RS! */
				data = memory[PC++];
				UPDATE_GLOBAL_REGS;
				CPU_GetStatus();
				AtariEscape(data);
				CPU_PutStatus();
				UPDATE_LOCAL_REGS;
            }
            continue;
            break;
			
		case 0xf3:	/* INS (ab),y [unofficial - INC Mem then SBC with Acc] */
			{
				INDIRECT_Y;
				/* INS */
				data = Z = N = GetByte(addr) + 1;
				PutByte(addr, data);
				goto sbc;
			}
			break;
			
			
		case 0xf4:	/* SKB [unofficial - skip byte] */
			{
				PC++;
            }
            continue;
            break;
			
		case 0xf5:	/* SBC ab,x */
			{
				ZPAGE_X;
				data = memory[addr];
				goto sbc;
			}
			break;
			
			
		case 0xf6:	/* INC ab,x */
			{
				ZPAGE_X;
				Z = N = ++memory[addr];
            }
            continue;
            break;
			
		case 0xf7:	/* INS zpage,x [unofficial - INC Mem then SBC with Acc] */
			{
				ZPAGE_X;
				/* INS_ZPAGE */
				data = Z = N = memory[addr] + 1;
				memory[addr] = data;
				goto sbc;
			}
			break;
			
			
		case 0xf8:	/* SED */
			SetD;
			continue;
			break;
		case 0xf9:	/* SBC abcd,y */
			{
				ABSOLUTE_Y;
				NCYCLES_Y;
				data = GetByte(addr);
				goto sbc;
			}
			break;
			
			
		case 0xfa:	/* NOP (1 byte) [unofficial] */
			continue;
			break;
		case 0xfb:	/* INS abcd,y [unofficial - INC Mem then SBC with Acc] */
			{
				ABSOLUTE_Y;
				/* INS */
				data = Z = N = GetByte(addr) + 1;
				PutByte(addr, data);
				goto sbc;
			}
			break;
			
			
		case 0xfc:	/* SKW [unofficial - skip word] */
			{
				PC += 2;
            }
            continue;
            break;
			
		case 0xfd:	/* SBC abcd,x */
			{
				ABSOLUTE_X;
				NCYCLES_X;
				data = GetByte(addr);
				goto sbc;
			}
			break;
			
			
		case 0xfe:	/* INC abcd,x */
			{
				ABSOLUTE_X;
				Z = N = GetByte(addr) + 1;
				PutByte(addr, Z);
            }
            continue;
            break;
			
		case 0xff:	/* INS abcd,x [unofficial - INC Mem then SBC with Acc] */
			{
				ABSOLUTE_X;
				/* INS */
				data = Z = N = GetByte(addr) + 1;
				PutByte(addr, data);
				goto sbc;
			}
			break;
	}		
	/* ---------------------------------------------- */
	/* ADC and SBC routines */
	
adc:
	if (!(regP & D_FLAG)) {
		
		/* THOR: */
		UWORD tmp;
		/* Binary mode */
		tmp = A + data + (UWORD)C;
		C = tmp > 0xff;
		V = !((A ^ data) & 0x80) && ((A ^ tmp) & 0x80);
		Z = N = A = tmp;
	} else {
		UWORD al,ah;      
		/* Decimal mode */
		al = (A & 0x0f) + (data & 0x0f) + (UWORD)C; /* lower nybble */
		if (al > 9) al += 6;          /* BCD fixup for lower nybble */
		ah = (A >> 4) + (data >> 4);  /* Calculate upper nybble */
		if (al > 0x0f) ah++;
		
		Z = A + data + (UWORD)C;	/* Set flags */
		N = ah << 4;	        /* Only highest bit used */
		V = (((ah << 4) ^ A) & 0x80) && !((A ^ data) & 0x80);
		
		if (ah > 9) ah += 6;	/* BCD fixup for upper nybble */
		C = ah > 0x0f;		/* Set carry flag */
		A = (ah << 4) | (al & 0x0f);	/* Compose result */
	}
	goto next;
	
	
sbc:
	
	if (!(regP & D_FLAG)) {
		
		/* THOR */
		UWORD tmp;
		
		tmp = A - data - !C;
		
		C = tmp < 0x100;
		V = ((A ^ tmp) & 0x80) && ((A ^ data) & 0x80);
		Z = N = A = tmp;
	} else {
		UWORD al, ah, tmp;
		
		tmp = A - data - !C;
		
		al = (A & 0x0f) - (data & 0x0f) - !C;	/* Calculate lower nybble */
		ah = (A >> 4) - (data >> 4);		/* Calculate upper nybble */
		if (al & 0x10) {
			al -= 6;	/* BCD fixup for lower nybble */
			ah--;
		}
		if (ah & 0x10) ah -= 6;	 /* BCD fixup for upper nybble */
		
		C = tmp < 0x100;		 /* Set flags */
		V = ((A ^ tmp) & 0x80) && ((A ^ data) & 0x80);
		Z = N = tmp;
		
		A = (ah << 4) | (al & 0x0f);	/* Compose result */
	}
	goto next;
	
	
next:
#ifdef MONITOR_BREAK
	if (break_step) {
		data = ESC_BREAK;
		UPDATE_GLOBAL_REGS;
		CPU_GetStatus();
		AtariEscape(data);
		CPU_PutStatus();
		UPDATE_LOCAL_REGS;
	}
#endif
	continue;
	}
	
	UPDATE_GLOBAL_REGS;
	if (wsync_halt && ncycles >= 0)
		return 0;				/* WSYNC stopped CPU */
	return ncycles;
}
