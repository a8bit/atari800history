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

	Possible Problems
	-----------------

	Call to Get Word when accessing stack - it doesn't
	wrap around at 0x1ff correctly.
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

static UBYTE	N;
static UBYTE	V;
static UBYTE	B;
static UBYTE	D;
static UBYTE	I;
static UBYTE	Z;
static UBYTE	C;

UBYTE	memory[65536];
UBYTE	attrib[65536];

int	NMI;
int	IRQ;

int	countdown = 15000;
int	count = 0;

/*
	========================================================
	Memory is accessed by calling routines from the hardware
	emulation layer. These are dynamically selected at
	runtime depending on the machine being emulated.
	========================================================
*/

UBYTE (*XGetByte)  (UWORD addr);
void  (*XPutByte)  (UWORD addr, UBYTE byte);
void  (*Hardware) (void);
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

	IRQ = FALSE;
	NMI = FALSE;

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
	PutByte(0x0100 + S, data);
	S--;
}

void PLP (void)
{
	UBYTE	data;

	S++;
	data = GetByte(0x0100 + S);
	N = (data & 0x80) ? 0x80 : 0;
	V = (data & 0x40) ? 1 : 0;
	B = (data & 0x10) ? 1 : 0;
	D = (data & 0x08) ? 1 : 0;
	I = (data & 0x04) ? 1 : 0;
	Z = (data & 0x02) ? 0 : 1;
	C = (data & 0x01) ? 1 : 0;
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

int GO (int CONTINUE)
{
	int	cpu_status = CPU_STATUS_OK;

	UWORD	addr;
	UBYTE	data;
	UBYTE	temp;
	SBYTE	sdata;

	UBYTE	instr;
	UWORD	instr_addr;

	do
	{
		UWORD	retadr;

		if (!count--)
		{
			count = countdown;
			Hardware ();
		}

		if (NMI)
		{
			retadr = PC;
			PutByte (0x0100+S, retadr >> 8);
			S--;
			PutByte (0x0100+S, retadr & 0xff);
			S--;

			PHP ();
			I = 1;
			PC = (GetByte(0xfffb) << 8) | GetByte(0xfffa);
			NMI = FALSE;
		}

		if (IRQ )
		{
			if (!I)
			{
				retadr = PC;
				PutByte (0x0100+S, retadr >> 8);
				S--;
				PutByte (0x0100+S, retadr & 0xff);
				S--;

				PHP ();
				I = 1;
				PC = (GetByte(0xffff) << 8) | GetByte(0xfffe);
				IRQ = FALSE;
			}
		}
/*
		if (CONTINUE != -1)
			disassemble (PC, PC+1);
*/

		instr = GetByte(PC);
		instr_addr = PC++;

/*
	=====================================
	Extract Address if Required by Opcode
	=====================================
*/
		switch (instr)
		{
/*
	=========================
	Absolute Addressing Modes
	=========================
*/
			case 0x6d :	/* ADC */
			case 0x2d :	/* AND */
			case 0x0e :	/* ASL */
			case 0x2c :	/* BIT */
			case 0xcd :	/* CMP */
			case 0xec :	/* CPX */
			case 0xcc :	/* CPY */
			case 0xce :	/* DEC */
			case 0x4d :	/* EOR */
			case 0xee :	/* INC */
			case 0x4c :	/* JMP */
			case 0x20 :	/* JSR */
			case 0xad :	/* LDA */
			case 0xae :	/* LDX */
			case 0xac :	/* LDY */
			case 0x4e :	/* LSR */
			case 0x0d :	/* ORA */
			case 0x2e :	/* ROL */
			case 0x6e :	/* ROR */
			case 0xed :	/* SBC */
			case 0x8d :	/* STA */
			case 0x8e :	/* STX */
			case 0x8c :	/* STY */
				addr = GetWord(PC);
				PC+=2;
				break;
/*
	======================
	0-Page Addressing Mode
	======================
*/
			case 0x65 :	/* ADC */
			case 0x25 :	/* AND */
			case 0x06 :	/* ASL */
			case 0x24 :	/* BIT */
			case 0xc5 :	/* CMP */
			case 0xe4 :	/* CPX */
			case 0xc4 :	/* CPY */
			case 0xc6 :	/* DEC */
			case 0x45 :	/* EOR */
			case 0xe6 :	/* INC */
			case 0xa5 :	/* LDA */
			case 0xa6 :	/* LDX */
			case 0xa4 :	/* LDY */
			case 0x46 :	/* LSR */
			case 0x05 :	/* ORA */
			case 0x26 :	/* ROL */
			case 0x66 :	/* ROR */
			case 0xe5 :	/* SBC */
			case 0x85 :	/* STA */
			case 0x86 :	/* STX */
			case 0x84 :	/* STY */
				addr = GetByte(PC);
				PC++;
				break;
/*
	========================
	Relative Addressing Mode
	========================
*/
			case 0x90 :	/* BCC */
			case 0xb0 :	/* BCS */
			case 0xf0 :	/* BEQ */
			case 0x30 :	/* BMI */
			case 0xd0 :	/* BNE */
			case 0x10 :	/* BPL */
			case 0x50 :	/* BVC */
			case 0x70 :	/* BVS */
				sdata = GetByte(PC);
				PC++;
				break;
/*
	=========================
	Immediate Addressing Mode
	=========================
*/
			case 0x69 :	/* ADC */
			case 0x29 :	/* AND */
			case 0xc9 :	/* CMP */
			case 0xe0 :	/* CPX */
			case 0xc0 :	/* CPY */
			case 0x49 :	/* EOR */
			case 0xa9 :	/* LDA */
			case 0xa2 :	/* LDX */
			case 0xa0 :	/* LDY */
			case 0x09 :	/* ORA */
			case 0xe9 :	/* SBC */
			case 0xff :	/* ESC */
				data = GetByte (PC);
				PC++;
				break;
/*
	=====================
	ABS,X Addressing Mode
	=====================
*/
			case 0x7d :	/* ADC */
			case 0x3d :	/* AND */
			case 0x1e :	/* ASL */
			case 0xdd :	/* CMP */
			case 0xde :	/* DEC */
			case 0x5d :	/* EOR */
			case 0xfe :	/* INC */
			case 0xbd :	/* LDA */
			case 0xbc :	/* LDY */
			case 0x5e :	/* LSR */
			case 0x1d :	/* ORA */
			case 0x3e :	/* ROL */
			case 0x7e :	/* ROR */
			case 0xfd :	/* SBC */
			case 0x9d :	/* STA */
				addr = GetWord(PC) + (UWORD)X;
				PC+=2;
				break;
/*
	=====================
	ABS,Y Addressing Mode
	=====================
*/
			case 0x79 :	/* ADC */
			case 0x39 :	/* AND */
			case 0xd9 :	/* CMP */
			case 0x59 :	/* EOR */
			case 0xb9 :	/* LDA */
			case 0xbe :	/* LDX */
			case 0x19 :	/* ORA */
			case 0xf9 :	/* SBC */
			case 0x99 :	/* STA */
				addr = GetWord(PC) + (UWORD)Y;
				PC+=2;
				break;
/*
	=======================
	(IND,X) Addressing Mode
	=======================
*/
			case 0x61 :	/* ADC */
			case 0x21 :	/* AND */
			case 0xc1 :	/* CMP */
			case 0x41 :	/* EOR */
			case 0xa1 :	/* LDA */
			case 0x01 :	/* ORA */
			case 0xe1 :	/* SBC */
			case 0x81 :	/* STA */
				addr = (UWORD)GetByte(PC) + (UWORD)X;
				addr = GetWord(addr);
				PC++;
				break;
/*
	=======================
	(IND),Y Addressing Mode
	=======================
*/
			case 0x71 :	/* ADC */
			case 0x31 :	/* AND */
			case 0xd1 :	/* CMP */
			case 0x51 :	/* EOR */
			case 0xb1 :	/* LDA */
			case 0x11 :	/* ORA */
			case 0xf1 :	/* SBC */
			case 0x91 :	/* STA */
				addr = GetByte(PC);
				addr = GetWord(addr) + (UWORD)Y;
				PC++;
				break;
/*
	========================
	0-Page,X Addressing Mode
	========================
*/
			case 0x75 :	/* ADC */
			case 0x35 :	/* AND */
			case 0x16 :	/* ASL */
			case 0xd5 :	/* CMP */
			case 0xd6 :	/* DEC */
			case 0x55 :	/* EOR */
			case 0xf6 :	/* INC */
			case 0xb5 :	/* LDA */
			case 0xb4 :	/* LDY */
			case 0x56 :	/* LSR */
			case 0x15 :	/* ORA */
			case 0x36 :	/* ROL */
			case 0x76 :	/* ROR */
			case 0xf5 :	/* SBC */
			case 0x95 :	/* STA */
			case 0x94 :	/* STY */
				addr = (UWORD)GetByte(PC) + (UWORD)X;
				addr = addr & 0xff;
				PC++;
				break;
/*
	========================
	0-Page,Y Addressing Mode
	========================
*/
			case 0xb6 :	/* LDX */
			case 0x96 :	/* STX */
				addr = (UWORD)GetByte(PC) + (UWORD)Y;
				addr = addr & 0xff;
				PC++;
				break;
/*
	========================
	Indirect Addressing Mode
	========================
*/
			case 0x6c :	/* JMP ($xxxx) */
				addr = GetWord(PC);
				addr = GetWord(addr);
				PC+=2;
				break;
			default :
				break;
		}
/*
	==============
	Execute Opcode
	==============
*/
		switch (instr)
		{
/* ADC */		case 0x6d :
			case 0x65 :
			case 0x7d :
			case 0x79 :
			case 0x61 :
			case 0x71 :
			case 0x75 :
				data = GetByte(addr);
			case 0x69 :
				if (!D)
				{
					UBYTE	old_A;
					UWORD	temp;

					old_A = A;

					temp = (UWORD)A + (UWORD)data + (UWORD)C;

					A = temp & 0xff;

					N = A;
					Z = A;
					C = (temp > 255);
					V = ((old_A ^ A) & 0x80) >> 7;
				}
				else
				{
					UBYTE	old_A;
					int	bcd1, bcd2;

					old_A = A;
/*
					bcd1 = ((A >> 4) & 0xf) * 10 + (A & 0xf);
					bcd2 = ((data >> 4) & 0xf) * 10 + (data & 0xf);
*/
					bcd1 = BCD_Lookup1[A];
					bcd2 = BCD_Lookup1[data];

					bcd1 += bcd2 + C;
/*
					A = (((bcd1 % 100) / 10) << 4) | (bcd1 % 10);
*/
					A = BCD_Lookup2[bcd1];

					N = A;
					Z = A;
					C = (bcd1 > 99);
					V = ((old_A ^ A) & 0x80) >> 7;
				}
				break;
/* AND */		case 0x2d :
			case 0x25 :
			case 0x3d :
			case 0x39 :
			case 0x21 :
			case 0x31 :
			case 0x35 :
				data = GetByte(addr);
			case 0x29 :
				Z = N = A = A & data;
				break;
/* ASL */		case 0x0a :
				C = (A >= 128);
				Z = N = A = A << 1;
				break;
			case 0x0e :
			case 0x06 :
			case 0x1e :
			case 0x16 :
				data = GetByte(addr);
				C = (data >= 128);
				Z = N = data = data << 1;
				PutByte(addr, data);
				break;
/* BCC */		case 0x90 :
				if (!C) PC += (SWORD)sdata;
				break;
/* BCS */		case 0xb0 :
				if (C) PC += (SWORD)sdata;
				break;
/* BEQ */		case 0xf0 :
				if (Z == 0) PC += (SWORD)sdata;
				break;
/* BIT */		case 0x2c :
			case 0x24 :
				data = GetByte(addr);
				N = data;
				V = (data & 0x40) >> 6;
				Z = (A & data);
				break;
/* BMI */		case 0x30 :
				if (N & 0x80) PC += (SWORD)sdata;
				break;
/* BNE */		case 0xd0 :
				if (Z != 0) PC += (SWORD)sdata;
				break;
/* BPL */		case 0x10 :
				if (!(N & 0x80)) PC += (SWORD)sdata;
				break;
/* BRK */		case 0x00 :
				printf ("BRK: %x\n", PC-1);
				retadr = PC + 1;
				PutByte (0x0100+S, retadr >> 8);
				S--;
				PutByte (0x0100+S, retadr & 0xff);
				S--;
				B = 1;
				PHP ();
				I = 1;
				PC = GetWord (0xfffe);
				break;
/* BVC */		case 0x50 :
				if (!V) PC += (SWORD)sdata;
				break;
/* BVS */		case 0x70 :
				if (V) PC += (SWORD)sdata;
				break;
/* CLC */		case 0x18 :
				C = 0;
				break;
/* CLD */		case 0xd8 :
				D = 0;
				break;
/* CLI */		case 0x58 :
				I = 0;
				break;
/* CLV */		case 0xb8 :
				V = 0;
				break;
/* CMP */		case 0xcd :
			case 0xc5 :
			case 0xdd :
			case 0xd9 :
			case 0xc1 :
			case 0xd1 :
			case 0xd5 :
				data = GetByte (addr);
			case 0xc9 :
				temp = A - data;
				Z = temp;
				N = temp;
				C = (A >= data);
				break;
/* CPX */		case 0xec :
			case 0xe4 :
				data = GetByte (addr);
			case 0xe0 :
				temp = X - data;
				Z = temp;
				N = temp;
				C = (X >= data);
				break;
/* CPY */		case 0xcc :
			case 0xc4 :
				data = GetByte(addr);
			case 0xc0 :
				temp = Y - data;
				Z = temp;
				N = temp;
				C = (Y >= data);
				break;
/* DEC */		case 0xce :
			case 0xc6 :
			case 0xde :
			case 0xd6 :
				data = GetByte(addr);
				data--;
				PutByte (addr, data);
				N = data;
				Z = data;
				break;
/* DEX */		case 0xca :
				X--;
				Z = N = X;
				break;
/* DEY */		case 0x88 :
				Y--;
				Z = N = Y;
				break;
/* EOR */		case 0x4d :
			case 0x45 :
			case 0x5d :
			case 0x59 :
			case 0x41 :
			case 0x51 :
			case 0x55 :
				data = GetByte (addr);
			case 0x49 :
				Z = N = A = A ^ data;
				break;
/* INC */		case 0xee :
			case 0xe6 :
			case 0xfe :
			case 0xf6 :
				data = GetByte(addr);
				data++;
				PutByte(addr, data);
				N = data;
				Z = data;
				break;
/* INX */		case 0xe8 :
				X++;
				Z = N = X;
				break;
/* INY */		case 0xc8 :
				Y++;
				Z = N = Y;
				break;
/* JMP */		case 0x4c :
			case 0x6c :
				PC = addr;
				break;
/* JSR */		case 0x20 :
				retadr = PC - 1;
				PutByte (0x0100+S, retadr >> 8);
				S--;
				PutByte (0x0100+S, retadr & 0xff);
				S--;

				PC = addr;
				break;
/* LDA */		case 0xad :
			case 0xa5 :
			case 0xbd :
			case 0xb9 :
			case 0xa1 :
			case 0xb1 :
			case 0xb5 :
				data = GetByte(addr);
			case 0xa9 :
				Z = N = A = data;
				break;
/* LDX */		case 0xae :
			case 0xa6 :
			case 0xbe :
			case 0xb6 :
				data = GetByte (addr);
			case 0xa2 :
				Z = N = X = data;
				break;
/* LDY */		case 0xac :
			case 0xa4 :
			case 0xbc :
			case 0xb4 :
				data = GetByte (addr);
			case 0xa0 :
				Z = N = Y = data;
				break;
/* LSR */		case 0x4a :
				C = (A & 1);
				A = A >> 1;
				N = 0;
				Z = A;
				break;
			case 0x4e :
			case 0x46 :
			case 0x5e :
			case 0x56 :
				data = GetByte(addr);
				C = (data & 1);
				data = data >> 1;
				N = 0;
				Z = data;
				PutByte(addr, data);
				break;
/* NOP */		case 0xea :
				break;
/* ORA */		case 0x0d :
			case 0x05 :
			case 0x1d :
			case 0x19 :
			case 0x01 :
			case 0x11 :
			case 0x15 :
				data = GetByte(addr);
			case 0x09 :
				Z = N = A = A | data;
				break;
/* PHA */		case 0x48 :
				PutByte(0x0100 + S, A);
				S--;
				break;
/* PHP */		case 0x08 :
				PHP ();
				break;
/* PLA */		case 0x68 :
				S++;
				A = GetByte(0x0100 + S);
				N = A;
				Z = A;
				break;
/* PLP */		case 0x28 :
				PLP ();
				break;
/* ROL */		case 0x2a :
				temp = (A >= 128);
				A = (A << 1) | C;
				C = temp;
				N = A;
				Z = A;
				break;
			case 0x2e :
			case 0x26 :
			case 0x3e :
			case 0x36 :
				data = GetByte(addr);
				temp = (data >= 128);
				data = (data << 1) | C;
				C = temp;
				N = data;
				Z = data;
				PutByte(addr, data);
				break;
/* ROR */		case 0x6a :
				temp = (A & 1);
				A = (A >> 1) | (C << 7);
				C = temp;
				N = A;
				Z = A;
				break;
			case 0x6e :
			case 0x66 :
			case 0x7e :
			case 0x76 :
				data = GetByte(addr);
				temp = (data & 1);
				data = (data >> 1) | (C << 7);
				C = temp;
				N = data;
				Z = data;
				PutByte(addr, data);
				break;
/* RTI */		case 0x40 :
				PLP ();
				S++;
				retadr = GetByte (0x0100 + S);
				S++;
				retadr |= (GetByte (0x0100 + S) << 8);
				PC = retadr;
				if (CONTINUE == -1)
					CONTINUE = FALSE;
				break;
/* RTS */		case 0x60 :
				S++;
				retadr = GetByte (0x0100 + S);
				S++;
				retadr |= (GetByte (0x0100 + S) << 8);
				PC = retadr + 1;
				break;
/* SBC */		case 0xed :
			case 0xe5 :
			case 0xfd :
			case 0xf9 :
			case 0xe1 :
			case 0xf1 :
			case 0xf5 :
				data = GetByte(addr);
			case 0xe9 :
				if (!D)
				{
					UWORD	temp;
					UBYTE	old_A;

					old_A = A;

					temp = (UWORD)A - (UWORD)data - (UWORD)!C;

					A = temp & 0xff;

					N = A;
					Z = A;
					C = (old_A < ((UWORD)data + (UWORD)!C)) ? 0 : 1;
					V = (((old_A ^ A) & 0x80) != 0);
				}
				else
				{
					int	bcd1, bcd2;
					UBYTE	old_A;

					old_A = A;
/*
					bcd1 = ((A >> 4) & 0xf) * 10 + (A & 0xf);
					bcd2 = ((data >> 4) & 0xf) * 10 + (data & 0xf);
*/
					bcd1 = BCD_Lookup1[A];
					bcd2 = BCD_Lookup1[data];

					bcd1 = bcd1 - bcd2 - !C;
/*
					A = (((bcd1 % 100) / 10) << 4) | (bcd1 % 10);
*/
					A = BCD_Lookup2[bcd1];

					N = A;
					Z = A;
					C = (old_A < (data + !C)) ? 0 : 1;
					V = (((old_A ^ A) & 0x80) != 0);
				}
				break;
/* SEC */		case 0x38 :
				C = 1;
				break;
/* SED */		case 0xf8 :
				D = 1;
				break;
/* SEI */		case 0x78 :
				I = 1;
				break;
/* STA */		case 0x8d :
			case 0x85 :
			case 0x9d :
			case 0x99 :
			case 0x81 :
			case 0x91 :
			case 0x95 :
				PutByte(addr, A);
				break;
/* STX */		case 0x8e :
			case 0x86 :
			case 0x96 :
				PutByte(addr, X);
				break;
/* STY */		case 0x8c :
			case 0x84 :
			case 0x94 :
				PutByte(addr, Y);
				break;
/* TAX */		case 0xaa :
				Z = N = X = A;
				break;
/* TAY */		case 0xa8 :
				Z = N = Y = A;
				break;
/* TSX */		case 0xba :
				Z = N = X = S;
				break;
/* TXA */		case 0x8a :
				Z = N = A = X;
				break;
/* TXS */		case 0x9a :
				S = X;
				break;
/* TYA */		case 0x98 :
				Z = N = A = Y;
				break;
/* ESC */		case 0xff :
				Escape (data);
				break;
			default :
				fprintf (stderr,"*** Invalid Opcode %02x at address %04x\n",instr,instr_addr);
				cpu_status = CPU_STATUS_ERR;
				CONTINUE = FALSE;
				break;
		}
	} while (CONTINUE);

	return cpu_status;
}
