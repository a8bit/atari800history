/*
	Ideas for Speed Improvements
	============================

	N = (result >= 128) could become N = NTAB[result];

		This saves the branch which breaks the
		pipeline.

	Flags could be 0x00 or 0xff instead of 0x00 or 0x01

		This allows branches to be implemented as
		follows :-

		BEQ	PC += (offset & Z)
		BNE	PC += (offset & (~Z))

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

	Possible Problems
	-----------------

	Call to Get Word when accessing stack - it doesn't
	wrap around at 0x1ff correctly.

	Some instruction that read and modify memory (DEC abcd etc.)
	may not work if they operate on ROM or HARDWARE addresses.
*/

#include	<stdio.h>
#include	<stdlib.h>

#define	FALSE	0
#define	TRUE	1

#include	"system.h"
#include	"cpu.h"

/*
	==========================================================
	Emulated Registers and Flags are kept local to this module
	==========================================================
*/

static UWORD	PC;
static UBYTE	A;
static UBYTE	S;
static UBYTE	X;
static UBYTE	Y;

static UBYTE	N;	/* bit7 zero (0) or bit 7 non-zero (1) */
static UBYTE	V;
static UBYTE	B;
static UBYTE	D;
static UBYTE	I;
static UBYTE	Z;	/* zero (0) or non-zero (1) */
static UBYTE	C;	/* zero (0) or non-zero (1) */

UBYTE	memory[65536];
UBYTE	attrib[65536];

int	INTERRUPT;
int	ncycles;

void  (*Escape)   (UBYTE byte);

/*
	===============================================================
	Z flag: This actually contains the result of an operation which
		would modify the Z flag. The value is tested for
		equality by the BEQ and BNE instruction.
	===============================================================
*/

void CPU_GetStatus (CPU_Status *cpu_status)
{
	cpu_status->PC = PC;
	cpu_status->A = A;
	cpu_status->S = S;
	cpu_status->X = X;
	cpu_status->Y = Y;

	if (N)
		cpu_status->flag.N = 1;
	else
		cpu_status->flag.N = 0;

	cpu_status->flag.V = V;
	cpu_status->flag.B = B;
	cpu_status->flag.D = D;
	cpu_status->flag.I = I;

	if (Z)
		cpu_status->flag.Z = 0;
	else
		cpu_status->flag.Z = 1;

	cpu_status->flag.C = C;
}

void CPU_PutStatus (CPU_Status *cpu_status)
{
	PC = cpu_status->PC;
	A = cpu_status->A;
	S = cpu_status->S;
	X = cpu_status->X;
	Y = cpu_status->Y;

	if (cpu_status->flag.N)
		N = 0x80;
	else
		N = 0x00;

	V = cpu_status->flag.V;
	B = cpu_status->flag.B;
	D = cpu_status->flag.D;
	I = cpu_status->flag.I;

	if (cpu_status->flag.Z)
		Z = 0;
	else
		Z = 1;

	C = cpu_status->flag.C;
}

static UBYTE	BCD_Lookup1[256];
static UBYTE	BCD_Lookup2[256];

void CPU_Reset (void)
{
	int	i;

	for (i=0;i<256;i++)
	{
		BCD_Lookup1[i] = ((i >> 4) & 0xf) * 10 + (i & 0xf);
		BCD_Lookup2[i] = (((i % 100) / 10) << 4) | (i % 10);
	}

	INTERRUPT = 0x00;

	D = 0;
	S = 0xff;
	PC = (GetByte(0xfffd) << 8) | GetByte(0xfffc);
}

void PHP (void)
{
	UBYTE	data;

	data =  (N & 0x80) ? 0x80 : 0;
	data |= V ? 0x40 : 0;
	data |= 0x20;
	data |= B ? 0x10 : 0;
	data |= D ? 0x08 : 0;
	data |= I ? 0x04 : 0;
	data |= (Z == 0) ? 0x02 : 0;
	data |= C ? 0x01 : 0;

	memory[0x0100 + S--] = data;
}

void PLP (void)
{
	UBYTE	data;

	data = memory[0x0100 + ++S];

	N = (data & 0x80) ? 0x80 : 0;
	V = (data & 0x40) ? 1 : 0;
	B = (data & 0x10) ? 1 : 0;
	D = (data & 0x08) ? 1 : 0;
	I = (data & 0x04) ? 1 : 0;
	Z = (data & 0x02) ? 0 : 1;
	C = (data & 0x01) ? 1 : 0;
}

void ADC (UBYTE data)
{
	if (C) C = 1;
	if (!D)
	{
		UBYTE	old_A;
		UWORD	temp;

		old_A = A;

		temp = (UWORD)A + (UWORD)data + (UWORD)C;

		Z = N = A = temp & 0xff;
/*
		C = (temp > 255);
*/
		C = temp >> 8;
		V = (old_A ^ A) & 0x80;
	}
	else
	{
		UBYTE	old_A;
		int	bcd1, bcd2;

		old_A = A;

		bcd1 = BCD_Lookup1[A];
		bcd2 = BCD_Lookup1[data];

		bcd1 += bcd2 + C;

		Z = N = A = BCD_Lookup2[bcd1];

		C = (bcd1 > 99);
		V = (old_A ^ A) & 0x80;
	}
}

void SBC (UBYTE data)
{
	if (C) C = 1;
	if (!D)
	{
		UWORD	temp;
		UBYTE	old_A;

		old_A = A;

		temp = (UWORD)A - (UWORD)data - (UWORD)!C;

		Z = N = A = temp & 0xff;

		C = (old_A < ((UWORD)data + (UWORD)!C)) ? 0 : 1;
		V = (old_A ^ A) & 0x80;
	}
	else
	{
		int	bcd1, bcd2;
		UBYTE	old_A;

		old_A = A;

		bcd1 = BCD_Lookup1[A];
		bcd2 = BCD_Lookup1[data];

		bcd1 = bcd1 - bcd2 - !C;

		if (bcd1 < 0) bcd1 = 100 - (-bcd1);
		Z = N = A = BCD_Lookup2[bcd1];

		C = (old_A < (data + !C)) ? 0 : 1;
		V = (old_A ^ A) & 0x80;
	}
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

int GO (void)
{
  int	cpu_status = CPU_STATUS_OK;

  UWORD	retadr;
  UWORD	addr;
  UBYTE	data;
  SBYTE	sdata;

  UBYTE	opcode;

/*
   This used to be in the main loop but has been removed to improve
   execution speed. It does not seem to have any adverse effect on
   the emulation for two reasons:-

   1. NMI's will can only be raised in atari_custom.c - there is
      no way an NMI can be generated whilst in this routine.

   2. The timing of the IRQs are not that critical.
*/

  if (INTERRUPT)
    {
      if (INTERRUPT & NMI_MASK)
	{
	  retadr = PC;
	  memory[0x0100 + S--] = retadr >> 8;
	  memory[0x0100 + S--] = retadr & 0xff;
	  PHP ();
	  I = 1;
	  PC = (memory[0xfffb] << 8) | memory[0xfffa];
	  INTERRUPT &= ~NMI_MASK;
	}

      if (INTERRUPT & IRQ_MASK)
	{
	  if (!I)
	    {
	      retadr = PC;
	      memory[0x0100 + S--] = retadr >> 8;
	      memory[0x0100 + S--] = retadr & 0xff;
	      PHP ();
	      I = 1;
	      PC = (memory[0xffff] << 8) | memory[0xfffe];
	      INTERRUPT &= ~IRQ_MASK;
	    }
	}
    }

  while (ncycles--)
    {
#ifdef TRACE
      disassemble (PC, PC+1);
#endif

      opcode = memory[PC++];
/*
   =====================================
   Extract Address if Required by Opcode
   =====================================
*/

#define	ABSOLUTE	addr=(memory[PC+1]<<8)|memory[PC];PC+=2;
#define	ZPAGE		addr=memory[PC++];
#define	ABSOLUTE_X	addr=((memory[PC+1]<<8)|memory[PC])+(UWORD)X;PC+=2;
#define ABSOLUTE_Y	addr=((memory[PC+1]<<8)|memory[PC])+(UWORD)Y;PC+=2;
#define	INDIRECT_X	addr=(UWORD)memory[PC++]+(UWORD)X;addr=GetWord(addr);
#define	INDIRECT_Y	addr=memory[PC++];addr=GetWord(addr)+(UWORD)Y;
#define	ZPAGE_X		addr=(memory[PC++]+X)&0xff;

/*
   ==============
   Execute Opcode
   ==============
*/
      switch (opcode)
	{
/* BRK */
	case 0x00 :
	  retadr = PC + 1;
	  memory[0x0100 + S--] = retadr >> 8;
	  memory[0x0100 + S--] = retadr & 0xff;
	  B = 1;
	  PHP ();
	  I = 1;
	  PC = (memory[0xffff] << 8) | memory[0xfffe];
	  break;
/* ORA (ab,x) */
	case 0x01 :
	  INDIRECT_X;
	  Z = N = A = A | GetByte(addr);
	  break;
/* ORA ab */
	case 0x05 :
	  ZPAGE;
	  Z = N = A = A | memory[addr];
	  break;
/* ASL ab */
	case 0x06 :
	  ZPAGE;
	  data = memory[addr];
	  C = data & 0x80;
	  Z = N = data = data << 1;
	  memory[addr] = data;
	  break;
/* PHP */
	case 0x08 :
	  PHP ();
	  break;
/* ORA #ab */
	case 0x09 :
	  Z = N = A = A | memory[PC++];
	  break;
/* ASL */
	case 0x0a :
	  C = A & 0x80;
	  Z = N = A = A << 1;
	  break;
/* ORA abcd */
	case 0x0d :
	  ABSOLUTE;
	  Z = N = A = A | GetByte(addr);
	  break;
/* ASL abcd */
	case 0x0e :
	  ABSOLUTE;
	  data = GetByte(addr);
	  C = data & 0x80;
	  Z = N = data = data << 1;
	  PutByte(addr, data);
	  break;
/* BPL */
	case 0x10 :
	  if (!(N & 0x80))
	    {
	      sdata = memory[PC++];
	      PC += (SWORD)sdata;
	    }
	  else
	    {
	      PC++;
	    }
	  break;
/* ORA (ab),y */
	case 0x11 :
	  INDIRECT_Y;
	  Z = N = A = A | GetByte(addr);
	  break;
/* ORA ab,x */
	case 0x15 :
	  ZPAGE_X;
	  Z = N = A = A | memory[addr];
	  break;
/* ASL ab,x */
	case 0x16 :
	  ZPAGE_X;
	  data = memory[addr];
	  C = data & 0x80;
	  Z = N = data = data << 1;
	  memory[addr] = data;
	  break;
/* CLC */
	case 0x18 :
	  C = 0;
	  break;
/* ORA abcd,y */
	case 0x19 :
	  ABSOLUTE_Y;
	  Z = N = A = A | GetByte(addr);
	  break;
/* ORA abcd,x */
	case 0x1d :
	  ABSOLUTE_X;
	  Z = N = A = A | GetByte(addr);
	  break;
/* ASL abcd,x */
	case 0x1e :
	  ABSOLUTE_X;
	  data = GetByte(addr);
	  C = data & 0x80;
	  Z = N = data = data << 1;
	  PutByte(addr, data);
	  break;
/* JSR abcd */
	case 0x20 :
	  retadr = PC + 1;
	  memory[0x0100 + S--] = retadr >> 8;
	  memory[0x0100 + S--] = retadr & 0xff;
	  PC = (memory[PC+1] << 8) | memory[PC];
	  break;
/* AND (ab,x) */
	case 0x21 :
	  INDIRECT_X;
	  Z = N = A = A & GetByte(addr);
	  break;
/* BIT ab */
	case 0x24 :
	  ZPAGE;
	  data = memory[addr];
	  N = data;
	  V = data & 0x40;
	  Z = (A & data);
	  break;
/* AND ab */
	case 0x25 :
	  ZPAGE;
	  Z = N = A = A & memory[addr];
	  break;
/* ROL ab */
	case 0x26 :
	  ZPAGE;
	  data = memory[addr];
	  if (C)
	    {
	      C = data & 0x80;
	      Z = N = data = (data << 1) | 1;
	    }
	  else
	    {
	      C = data & 0x80;
	      Z = N = data = (data << 1);
	    }
	  memory[addr] = data;
	  break;
/* PLP */
	case 0x28 :
	  PLP ();
	  break;
/* AND #ab */
	case 0x29 :
	  Z = N = A = A & memory[PC++];
	  break;
/* ROL */
	case 0x2a :
	  if (C)
	    {
	      C = A & 0x80;
	      Z = N = A = (A << 1) | 1;
	    }
	  else
	    {
	      C = A & 0x80;
	      Z = N = A = (A << 1);
	    }
	  break;
/* BIT abcd */
	case 0x2c :
	  ABSOLUTE;
	  data = GetByte(addr);
	  N = data;
	  V = data & 0x40;
	  Z = (A & data);
	  break;
/* AND abcd */
	case 0x2d :
	  ABSOLUTE;
	  Z = N = A = A & GetByte(addr);
	  break;
/* ROL abcd */
	case 0x2e :
	  ABSOLUTE;
	  data = GetByte(addr);
	  if (C)
	    {
	      C = data & 0x80;
	      Z = N = data = (data << 1) | 1;
	    }
	  else
	    {
	      C = data & 0x80;
	      Z = N = data = (data << 1);
	    }
	  PutByte(addr, data);
	  break;
/* BMI */
	case 0x30 :
	  if (N & 0x80)
	    {
	      sdata = memory[PC++];
	      PC += (SWORD)sdata;
	    }
	  else
	    {
	      PC++;
	    }
	  break;
/* AND (ab),y */
	case 0x31 :
	  INDIRECT_Y;
	  Z = N = A = A & GetByte(addr);
	  break;
/* AND ab,x */
	case 0x35 :
	  ZPAGE_X;
	  Z = N = A = A & memory[addr];
	  break;
/* ROL ab,x */
	case 0x36 :
	  ZPAGE_X;
	  data = memory[addr];
	  if (C)
	    {
	      C = data & 0x80;
	      Z = N = data = (data << 1) | 1;
	    }
	  else
	    {
	      C = data & 0x80;
	      Z = N = data = (data << 1);
	    }
	  memory[addr] = data;
	  break;
/* SEC */
	case 0x38 :
	  C = 1;
	  break;
/* AND abcd,y */
	case 0x39 :
	  ABSOLUTE_Y;
	  Z = N = A = A & GetByte(addr);
	  break;
/* AND abcd,x */
	case 0x3d :
	  ABSOLUTE_X;
	  Z = N = A = A & GetByte(addr);
	  break;
/* ROL abcd,x */
	case 0x3e :
	  ABSOLUTE_X;
	  data = GetByte(addr);
	  if (C)
	    {
	      C = data & 0x80;
	      Z = N = data = (data << 1) | 1;
	    }
	  else
	    {
	      C = data & 0x80;
	      Z = N = data = (data << 1);
	    }
	  PutByte(addr, data);
	  break;
/* RTI */
	case 0x40 :
	  PLP ();
	  retadr = memory[0x0100 + ++S];
	  PC = (memory[0x0100 + ++S] << 8) | retadr;
	  break;
/* EOR (ab,x) */
	case 0x41 :
	  INDIRECT_X;
	  Z = N = A = A ^ GetByte(addr);
	  break;
/* EOR ab */
	case 0x45 :
	  ZPAGE;
	  Z = N = A = A ^ memory[addr];
	  break;
/* LSR ab */
	case 0x46 :
	  ZPAGE;
	  data = memory[addr];
	  C = data & 1;
	  Z = data = data >> 1;
	  N = 0;
	  memory[addr] = data;
	  break;
/* PHA */
	case 0x48 :
	  memory[0x0100 + S--] = A;
	  break;
/* EOR #ab */
	case 0x49 :
	  Z = N = A = A ^ memory[PC++];
	  break;
/* LSR */
	case 0x4a :
	  C = A & 1;
	  A = A >> 1;
	  N = 0;
	  Z = A;
	  break;
/* JMP abcd */
	case 0x4c :
	  PC = (memory[PC+1] << 8) | memory[PC];
	  break;
/* EOR abcd */
	case 0x4d :
	  ABSOLUTE;
	  Z = N = A = A ^ GetByte(addr);
	  break;
/* LSR abcd */
	case 0x4e :
	  ABSOLUTE;
	  data = GetByte(addr);
	  C = data & 1;
	  Z = data = data >> 1;
	  N = 0;
	  PutByte(addr, data);
	  break;
/* BVC */
	case 0x50 :
	  if (!V)
	    {
	      sdata = memory[PC++];
	      PC += (SWORD)sdata;
	    }
	  else
	    {
	      PC++;
	    }
	  break;
/* EOR (ab),y */
	case 0x51 :
	  INDIRECT_Y;
	  Z = N = A = A ^ GetByte(addr);
	  break;
/* EOR ab,x */
	case 0x55 :
	  ZPAGE_X;
	  Z = N = A = A ^ memory[addr];
	  break;
/* LSR ab,x */
	case 0x56 :
	  ZPAGE_X;
	  data = memory[addr];
	  C = data & 1;
	  Z = data = data >> 1;
	  N = 0;
	  memory[addr] = data;
	  break;
/* CLI */
	case 0x58 :
	  I = 0;
	  break;
/* EOR abcd,y */
	case 0x59 :
	  ABSOLUTE_Y;
	  Z = N = A = A ^ GetByte(addr);
	  break;
/* EOR abcd,x */
	case 0x5d :
	  ABSOLUTE_X;
	  Z = N = A = A ^ GetByte(addr);
	  break;
/* LSR abcd,x */
	case 0x5e :
	  ABSOLUTE_X;
	  data = GetByte(addr);
	  C = data & 1;
	  Z = data = data >> 1;
	  N = 0;
	  PutByte(addr, data);
	  break;
/* RTS */
	case 0x60 :
	  retadr = memory[0x0100 + ++S];
	  retadr |= (memory[0x0100 + ++S] << 8);
	  PC = retadr + 1;
	  break;
/* ADC (ab,x) */
	case 0x61 :
	  INDIRECT_X;
	  data = GetByte(addr);
	  ADC (data);
	  break;
/* ADC ab */
	case 0x65 :
	  ZPAGE;
	  data = memory[addr];
	  ADC (data);
	  break;
/* ROR ab */
	case 0x66 :
	  ZPAGE;
	  data = memory[addr];
	  if (C)
	    {
	      C = data & 1;
	      Z = N = data = (data >> 1) | 0x80;
	    }
	  else
	    {
	      C = data & 1;
	      Z = N = data = (data >> 1);
	    }
	  memory[addr] = data;
	  break;
/* PLA */
	case 0x68 :
	  Z = N = A = memory[0x0100 + ++S];
	  break;
/* ADC #ab */
	case 0x69 :
	  ADC (memory[PC++]);
	  break;
/* ROR */
	case 0x6a :
	  if (C)
	    {
	      C = A & 1;
	      Z = N = A = (A >> 1) | 0x80;
	    }
	  else
	    {
	      C = A & 1;
	      Z = N = A = (A >> 1);
	    }
	  break;
/* JMP (abcd) */
	case 0x6c :
	  addr = (memory[PC+1] << 8) | memory[PC];
	  PC = (memory[addr+1] << 8) | memory[addr];
	  break;
/* ADC abcd */
	case 0x6d :
	  ABSOLUTE;
	  data = GetByte(addr);
	  ADC (data);
	  break;
/* ROR abcd */
	case 0x6e :
	  ABSOLUTE;
	  data = GetByte(addr);
	  if (C)
	    {
	      C = data & 1;
	      Z = N = data = (data >> 1) | 0x80;
	    }
	  else
	    {
	      C = data & 1;
	      Z = N = data = (data >> 1);
	    }
	  PutByte(addr, data);
	  break;
/* BVS */
	case 0x70 :
	  if (V)
	    {
	      sdata = memory[PC++];
	      PC += (SWORD)sdata;
	    }
	  else
	    {
	      PC++;
	    }
	  break;
/* ADC (ab),y */
	case 0x71 :
	  INDIRECT_Y;
	  data = GetByte(addr);
	  ADC (data);
	  break;
/* ADC ab,x */
	case 0x75 :
	  ZPAGE_X;
	  data = memory[addr];
	  ADC (data);
	  break;
/* ROR ab,x */
	case 0x76 :
	  ZPAGE_X;
	  data = memory[addr];
	  if (C)
	    {
	      C = data & 1;
	      Z = N = data = (data >> 1) | 0x80;
	    }
	  else
	    {
	      C = data & 1;
	      Z = N = data = (data >> 1);
	    }
	  memory[addr] = data;
	  break;
/* SEI */
	case 0x78 :
	  I = 1;
	  break;
/* ADC abcd,y */
	case 0x79 :
	  ABSOLUTE_Y;
	  data = GetByte(addr);
	  ADC (data);
	  break;
/* ADC abcd,x */
	case 0x7d :
	  ABSOLUTE_X;
	  data = GetByte(addr);
	  ADC (data);
	  break;
/* ROR abcd,x */
	case 0x7e :
	  ABSOLUTE_X;
	  data = GetByte(addr);
	  if (C)
	    {
	      C = data & 1;
	      Z = N = data = (data >> 1) | 0x80;
	    }
	  else
	    {
	      C = data & 1;
	      Z = N = data = (data >> 1);
	    }
	  PutByte(addr, data);
	  break;
/* STA (ab,x) */
	case 0x81 :
	  INDIRECT_X;
	  PutByte(addr, A);
	  break;
/* STY ab */
	case 0x84 :
	  ZPAGE;
	  memory[addr] = Y;
	  break;
/* STA ab */
	case 0x85 :
	  ZPAGE;
	  memory[addr] = A;
	  break;
/* STX ab */
	case 0x86 :
	  ZPAGE;
	  memory[addr] = X;
	  break;
/* DEY */
	case 0x88 :
	  Z = N = --Y;
	  break;
/* TXA */
	case 0x8a :
	  Z = N = A = X;
	  break;
/* STY abcd */
	case 0x8c :
	  ABSOLUTE;
	  PutByte(addr, Y);
	  break;
/* STA abcd */
	case 0x8d :
	  ABSOLUTE;
	  PutByte(addr, A);
	  break;
/* STX abcd */
	case 0x8e :
	  ABSOLUTE;
	  PutByte(addr, X);
	  break;
/* BCC */
	case 0x90 :
	  if (!C)
	    {
	      sdata = memory[PC++];
	      PC += (SWORD)sdata;
	    }
	  else
	    {
	      PC++;
	    }
	  break;
/* STA (ab),y */
	case 0x91 :
	  INDIRECT_Y;
	  PutByte(addr, A);
	  break;
/* STY ab,x */
	case 0x94 :
	  ZPAGE_X;
	  memory[addr] = Y;
	  break;
/* STA ab,x */
	case 0x95 :
	  ZPAGE_X;
	  memory[addr] = A;
	  break;
/* STX ab,y */
	case 0x96 :
	  addr = (UWORD)memory[PC++] + (UWORD)Y;
	  addr = addr & 0xff;
	  PutByte(addr, X);
	  break;
/* TYA */
	case 0x98 :
	  Z = N = A = Y;
	  break;
/* STA abcd,y */
	case 0x99 :
	  ABSOLUTE_Y;
	  PutByte(addr, A);
	  break;
/* TXS */
	case 0x9a :
	  S = X;
	  break;
/* STA abcd,x */
	case 0x9d :
	  ABSOLUTE_X;
	  PutByte(addr, A);
	  break;
/* LDY #ab */
	case 0xa0 :
	  Z = N = Y = memory[PC++];
	  break;
/* LDA (ab,x) */
	case 0xa1 :
	  INDIRECT_X;
	  Z = N = A = GetByte(addr);
	  break;
/* LDX #ab */
	case 0xa2 :
	  Z = N = X = memory[PC++];
	  break;
/* LDY ab */
	case 0xa4 :
	  ZPAGE;
	  Z = N = Y = memory[addr];
	  break;
/* LDA ab */
	case 0xa5 :
	  ZPAGE;
	  Z = N = A = memory[addr];
	  break;
/* LDX ab */
	case 0xa6 :
	  ZPAGE;
	  Z = N = X = memory[addr];
	  break;
/* TAY */
	case 0xa8 :
	  Z = N = Y = A;
	  break;
/* LDA #ab */
	case 0xa9 :
	  Z = N = A = memory[PC++];
	  break;
/* TAX */
	case 0xaa :
	  Z = N = X = A;
	  break;
/* LDY abcd */
	case 0xac :
	  ABSOLUTE;
	  Z = N = Y = GetByte (addr);
	  break;
/* LDA abcd */
	case 0xad :
	  ABSOLUTE;
	  Z = N = A = GetByte(addr);
	  break;
/* LDX abcd */
	case 0xae :
	  ABSOLUTE;
	  Z = N = X = GetByte (addr);
	  break;
/* BCS */
	case 0xb0 :
	  if (C)
	    {
	      sdata = memory[PC++];
	      PC += (SWORD)sdata;
	    }
	  else
	    {
	      PC++;
	    }
	  break;
/* LDA (ab),y */
	case 0xb1 :
	  INDIRECT_Y;
	  Z = N = A = GetByte(addr);
	  break;
/* LDY ab,x */
	case 0xb4 :
	  ZPAGE_X;
	  Z = N = Y = memory[addr];
	  break;
/* LDA ab,x */
	case 0xb5 :
	  ZPAGE_X;
	  Z = N = A = memory[addr];
	  break;
/* LDX ab,y */
	case 0xb6 :
	  addr = (UWORD)memory[PC++] + (UWORD)Y;
	  addr = addr & 0xff;
	  Z = N = X = GetByte (addr);
	  break;
/* CLV */
	case 0xb8 :
	  V = 0;
	  break;
/* LDA abcd,y */
	case 0xb9 :
	  ABSOLUTE_Y;
	  Z = N = A = GetByte(addr);
	  break;
/* TSX */
	case 0xba :
	  Z = N = X = S;
	  break;
/* LDY abcd,x */
	case 0xbc :
	  ABSOLUTE_X;
	  Z = N = Y = GetByte (addr);
	  break;
/* LDA abcd,x */
	case 0xbd :
	  ABSOLUTE_X;
	  Z = N = A = GetByte(addr);
	  break;
/* LDX abcd,y */
	case 0xbe :
	  ABSOLUTE_Y;
	  Z = N = X = GetByte (addr);
	  break;
/* CPY #ab */
	case 0xc0 :
	  data = memory[PC++];
	  Z = N = Y - data;
	  C = (Y >= data);
	  break;
/* CMP (ab,x) */
	case 0xc1 :
	  INDIRECT_X;
	  data = GetByte(addr);
	  Z = N = A - data;
	  C = (A >= data);
	  break;
/* CPY ab */
	case 0xc4 :
	  ZPAGE;
	  data = memory[addr];
	  Z = N = Y - data;
	  C = (Y >= data);
	  break;
/* CMP ab */
	case 0xc5 :
	  ZPAGE;
	  data = memory[addr];
	  Z = N = A - data;
	  C = (A >= data);
	  break;
/* DEC ab */
	case 0xc6 :
	  ZPAGE;
	  Z = N = --memory[addr];
	  break;
/* INY */
	case 0xc8 :
	  Z = N = ++Y;
	  break;
/* CMP #ab */
	case 0xc9 :
	  data = memory[PC++];
	  Z = N = A - data;
	  C = (A >= data);
	  break;
/* DEX */
	case 0xca :
	  Z = N = --X;
	  break;
/* CPY abcd */
	case 0xcc :
	  ABSOLUTE;
	  data = GetByte(addr);
	  Z = N = Y - data;
	  C = (Y >= data);
	  break;
/* CMP abcd */
	case 0xcd :
	  ABSOLUTE;
	  data = GetByte(addr);
	  Z = N = A - data;
	  C = (A >= data);
	  break;
/* DEC abcd */
	case 0xce :
	  ABSOLUTE;
	  if (attrib[addr] == RAM)
	    Z = N = --memory[addr];
	  break;
/* BNE */
	case 0xd0 :
	  if (Z != 0)
	    {
	      sdata = memory[PC++];
	      PC += (SWORD)sdata;
	    }
	  else
	    {
	      PC++;
	    }
	  break;
/* CMP (ab),y */
	case 0xd1 :
	  INDIRECT_Y;
	  data = GetByte(addr);
	  Z = N = A - data;
	  C = (A >= data);
	  break;
/* CMP ab,x */
	case 0xd5 :
	  ZPAGE_X;
	  data = memory[addr];
	  Z = N = A - data;
	  C = (A >= data);
	  break;
/* DEC ab,x */
	case 0xd6 :
	  ZPAGE_X;
	  Z = N = --memory[addr];
	  break;
/* CLD */
	case 0xd8 :
	  D = 0;
	  break;
/* CMP abcd,y */
	case 0xd9 :
	  ABSOLUTE_Y;
	  data = GetByte(addr);
	  Z = N = A - data;
	  C = (A >= data);
	  break;
/* CMP abcd,x */
	case 0xdd :
	  ABSOLUTE_X;
	  data = GetByte(addr);
	  Z = N = A - data;
	  C = (A >= data);
	  break;
/* DEC abcd,x */
	case 0xde :
	  ABSOLUTE_X;
	  if (attrib[addr] == RAM)
	    Z = N = --memory[addr];
	  break;
/* CPX #ab */
	case 0xe0 :
	  data = memory[PC++];
	  Z = N = X - data;
	  C = (X >= data);
	  break;
/* SBC (ab,x) */
	case 0xe1 :
	  INDIRECT_X;
	  data = GetByte(addr);
	  SBC (data);
	  break;
/* CPX ab */
	case 0xe4 :
	  ZPAGE;
	  data = memory[addr];
	  Z = N = X - data;
	  C = (X >= data);
	  break;
/* SBC ab */
	case 0xe5 :
	  ZPAGE;
	  data = memory[addr];
	  SBC (data);
	  break;
/* INC ab */
	case 0xe6 :
	  ZPAGE;
	  Z = N = ++memory[addr];
	  break;
/* INX */
	case 0xe8 :
	  Z = N = ++X;
	  break;
/* SBC #ab */
	case 0xe9 :
	  SBC (memory[PC++]);
	  break;
/* NOP */
	case 0xea :
	  break;
/* CPX abcd */
	case 0xec :
	  ABSOLUTE;
	  data = GetByte (addr);
	  Z = N = X - data;
	  C = (X >= data);
	  break;
/* SBC abcd */
	case 0xed :
	  ABSOLUTE;
	  data = GetByte(addr);
	  SBC (data);
	  break;
/* INC abcd */
	case 0xee :
	  ABSOLUTE;
	  if (attrib[addr] == RAM)
	    Z = N = ++memory[addr];
	  break;
/* BEQ */
	case 0xf0 :
	  if (Z == 0)
	    {
	      sdata = memory[PC++];
	      PC += (SWORD)sdata;
	    }
	  else
	    {
	      PC++;
	    }
	  break;
/* SBC (ab),y */
	case 0xf1 :
	  INDIRECT_Y;
	  data = GetByte(addr);
	  SBC (data);
	  break;
/* SBC ab,x */
	case 0xf5 :
	  ZPAGE_X;
	  data = memory[addr];
	  SBC (data);
	  break;
/* INC ab,x */
	case 0xf6 :
	  ZPAGE_X;
	  Z = N = ++memory[addr];
	  break;
/* SED */
	case 0xf8 :
	  D = 1;
	  break;
/* SBC abcd,y */
	case 0xf9 :
	  ABSOLUTE_Y;
	  data = GetByte(addr);
	  SBC (data);
	  break;
/* SBC abcd,x */
	case 0xfd :
	  ABSOLUTE_X;
	  data = GetByte(addr);
	  SBC (data);
	  break;
/* INC abcd,x */
	case 0xfe :
	  ABSOLUTE_X;
	  if (attrib[addr] == RAM)
	    Z = N = ++memory[addr];
	  break;
/* ESC #ab */
	case 0xff :
	  Escape (memory[PC++]);
	  break;
	default :
	  fprintf (stderr,"*** Invalid Opcode %02x at address %04x\n", opcode, PC-1);
	  cpu_status = CPU_STATUS_ERR;
	  ncycles = 1;
	  break;
	}
    }
	
  return cpu_status;
}
