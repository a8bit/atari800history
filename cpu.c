/* CPU.C
 *    Original Author     :   David Firth          *
 *      Trim by             :   Radek Sterba, RASTER *  
 *    Date of changes     :   28th Feb 1998        */

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

static char *rcsid = "$Id: cpu.c,v 1.22 1998/02/21 15:02:12 david Exp $";

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
#endif

static UBYTE attrib[65536];
#define	GetByte(addr)		((attrib[addr] == HARDWARE) ? Atari800_GetByte(addr) : memory[addr])
#define	PutByte(addr,byte)	if (attrib[addr] == RAM) memory[addr] = byte; else if (attrib[addr] == HARDWARE) if (Atari800_PutByte(addr,byte)) break;

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
	memory[0x0100 + S--] = regPC & 0xff;
	PHP;
	SetI;
	regPC = (memory[0xfffb] << 8) | memory[0xfffa];
	regS = S;
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
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7	/* Fx */
};

/* decrement 1 or 2 cycles for conditional jumps */
#define NCYCLES_JMP	if ( ((SWORD) sdata + (UBYTE) PC) & 0xff00) ncycles-=2; else ncycles--;
/* decrement 1 cycle for X (or Y) index overflow */
#define NCYCLES_Y		if ( (SWORD) ((UBYTE) addr - (UBYTE) Y) < 0 ) ncycles--;
#define NCYCLES_X		if ( (SWORD) ((UBYTE) addr - (UBYTE) X) < 0 ) ncycles--;


int GO(int ncycles)
{

#ifdef GNU_C
	static void *opcode[256] =
	{
		&&opcode_00, &&opcode_01, &&opcode_02, &&opcode_03,
		&&opcode_04, &&opcode_05, &&opcode_06, &&opcode_07,
		&&opcode_08, &&opcode_09, &&opcode_0a, &&opcode_0b,
		&&opcode_0c, &&opcode_0d, &&opcode_0e, &&opcode_0f,

		&&opcode_10, &&opcode_11, &&opcode_12, &&opcode_13,
		&&opcode_14, &&opcode_15, &&opcode_16, &&opcode_17,
		&&opcode_18, &&opcode_19, &&opcode_1a, &&opcode_1b,
		&&opcode_1c, &&opcode_1d, &&opcode_1e, &&opcode_1f,

		&&opcode_20, &&opcode_21, &&opcode_22, &&opcode_23,
		&&opcode_24, &&opcode_25, &&opcode_26, &&opcode_27,
		&&opcode_28, &&opcode_29, &&opcode_2a, &&opcode_2b,
		&&opcode_2c, &&opcode_2d, &&opcode_2e, &&opcode_2f,

		&&opcode_30, &&opcode_31, &&opcode_32, &&opcode_33,
		&&opcode_34, &&opcode_35, &&opcode_36, &&opcode_37,
		&&opcode_38, &&opcode_39, &&opcode_3a, &&opcode_3b,
		&&opcode_3c, &&opcode_3d, &&opcode_3e, &&opcode_3f,

		&&opcode_40, &&opcode_41, &&opcode_42, &&opcode_43,
		&&opcode_44, &&opcode_45, &&opcode_46, &&opcode_47,
		&&opcode_48, &&opcode_49, &&opcode_4a, &&opcode_4b,
		&&opcode_4c, &&opcode_4d, &&opcode_4e, &&opcode_4f,

		&&opcode_50, &&opcode_51, &&opcode_52, &&opcode_53,
		&&opcode_54, &&opcode_55, &&opcode_56, &&opcode_57,
		&&opcode_58, &&opcode_59, &&opcode_5a, &&opcode_5b,
		&&opcode_5c, &&opcode_5d, &&opcode_5e, &&opcode_5f,

		&&opcode_60, &&opcode_61, &&opcode_62, &&opcode_63,
		&&opcode_64, &&opcode_65, &&opcode_66, &&opcode_67,
		&&opcode_68, &&opcode_69, &&opcode_6a, &&opcode_6b,
		&&opcode_6c, &&opcode_6d, &&opcode_6e, &&opcode_6f,

		&&opcode_70, &&opcode_71, &&opcode_72, &&opcode_73,
		&&opcode_74, &&opcode_75, &&opcode_76, &&opcode_77,
		&&opcode_78, &&opcode_79, &&opcode_7a, &&opcode_7b,
		&&opcode_7c, &&opcode_7d, &&opcode_7e, &&opcode_7f,

		&&opcode_80, &&opcode_81, &&opcode_82, &&opcode_83,
		&&opcode_84, &&opcode_85, &&opcode_86, &&opcode_87,
		&&opcode_88, &&opcode_89, &&opcode_8a, &&opcode_8b,
		&&opcode_8c, &&opcode_8d, &&opcode_8e, &&opcode_8f,

		&&opcode_90, &&opcode_91, &&opcode_92, &&opcode_93,
		&&opcode_94, &&opcode_95, &&opcode_96, &&opcode_97,
		&&opcode_98, &&opcode_99, &&opcode_9a, &&opcode_9b,
		&&opcode_9c, &&opcode_9d, &&opcode_9e, &&opcode_9f,

		&&opcode_a0, &&opcode_a1, &&opcode_a2, &&opcode_a3,
		&&opcode_a4, &&opcode_a5, &&opcode_a6, &&opcode_a7,
		&&opcode_a8, &&opcode_a9, &&opcode_aa, &&opcode_ab,
		&&opcode_ac, &&opcode_ad, &&opcode_ae, &&opcode_af,

		&&opcode_b0, &&opcode_b1, &&opcode_b2, &&opcode_b3,
		&&opcode_b4, &&opcode_b5, &&opcode_b6, &&opcode_b7,
		&&opcode_b8, &&opcode_b9, &&opcode_ba, &&opcode_bb,
		&&opcode_bc, &&opcode_bd, &&opcode_be, &&opcode_bf,

		&&opcode_c0, &&opcode_c1, &&opcode_c2, &&opcode_c3,
		&&opcode_c4, &&opcode_c5, &&opcode_c6, &&opcode_c7,
		&&opcode_c8, &&opcode_c9, &&opcode_ca, &&opcode_cb,
		&&opcode_cc, &&opcode_cd, &&opcode_ce, &&opcode_cf,

		&&opcode_d0, &&opcode_d1, &&opcode_d2, &&opcode_d3,
		&&opcode_d4, &&opcode_d5, &&opcode_d6, &&opcode_d7,
		&&opcode_d8, &&opcode_d9, &&opcode_da, &&opcode_db,
		&&opcode_dc, &&opcode_dd, &&opcode_de, &&opcode_df,

		&&opcode_e0, &&opcode_e1, &&opcode_e2, &&opcode_e3,
		&&opcode_e4, &&opcode_e5, &&opcode_e6, &&opcode_e7,
		&&opcode_e8, &&opcode_e9, &&opcode_ea, &&opcode_eb,
		&&opcode_ec, &&opcode_ed, &&opcode_ee, &&opcode_ef,

		&&opcode_f0, &&opcode_f1, &&opcode_f2, &&opcode_f3,
		&&opcode_f4, &&opcode_f5, &&opcode_f6, &&opcode_f7,
		&&opcode_f8, &&opcode_f9, &&opcode_fa, &&opcode_fb,
		&&opcode_fc, &&opcode_fd, &&opcode_fe, &&opcode_ff,
	};
#endif

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

	if (IRQ) {
		if (!(regP & I_FLAG)) {
			UWORD retadr = PC;

			memory[0x0100 + S--] = retadr >> 8;
			memory[0x0100 + S--] = retadr & 0xff;
			PHP;
			SetI;
			PC = (memory[0xffff] << 8) | memory[0xfffe];
			IRQ = 0;
		}
	}

/*
   =====================================
   Extract Address if Required by Opcode
   =====================================
 */

#define	ABSOLUTE	addr=(memory[PC+1]<<8)+memory[PC];PC+=2;
#define	ZPAGE		addr=memory[PC++];
#define	ABSOLUTE_X	addr=((memory[PC+1]<<8)+memory[PC])+(UWORD)X;PC+=2;
#define 	ABSOLUTE_Y	addr=((memory[PC+1]<<8)+memory[PC])+(UWORD)Y;PC+=2;
#define	INDIRECT_X	addr=(UWORD)memory[PC++]+(UWORD)X;addr=(memory[addr+1]<<8)+memory[addr];
#define	INDIRECT_Y	addr=memory[PC++];addr=(memory[addr+1]<<8)+memory[addr]+(UWORD)Y;
#define	ZPAGE_X		addr=(memory[PC++]+X)&0xff;
#define	ZPAGE_Y		addr=(memory[PC++]+Y)&0xff;

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
			Escape(data);
			CPU_PutStatus();
			UPDATE_LOCAL_REGS;
		}
#endif
		ncycles -= cycles[memory[PC]];

#ifdef GNU_C
		goto *opcode[memory[PC++]];
#else
		switch (memory[PC++]) {
		case 0x00:
			goto opcode_00;
		case 0x01:
			goto opcode_01;
		case 0x02:
			goto opcode_02;
		case 0x03:
			goto opcode_03;
		case 0x04:
			goto opcode_04;
		case 0x05:
			goto opcode_05;
		case 0x06:
			goto opcode_06;
		case 0x07:
			goto opcode_07;
		case 0x08:
			goto opcode_08;
		case 0x09:
			goto opcode_09;
		case 0x0a:
			goto opcode_0a;
		case 0x0b:
			goto opcode_0b;
		case 0x0c:
			goto opcode_0c;
		case 0x0d:
			goto opcode_0d;
		case 0x0e:
			goto opcode_0e;
		case 0x0f:
			goto opcode_0f;

		case 0x10:
			goto opcode_10;
		case 0x11:
			goto opcode_11;
		case 0x12:
			goto opcode_12;
		case 0x13:
			goto opcode_13;
		case 0x14:
			goto opcode_14;
		case 0x15:
			goto opcode_15;
		case 0x16:
			goto opcode_16;
		case 0x17:
			goto opcode_17;
		case 0x18:
			goto opcode_18;
		case 0x19:
			goto opcode_19;
		case 0x1a:
			goto opcode_1a;
		case 0x1b:
			goto opcode_1b;
		case 0x1c:
			goto opcode_1c;
		case 0x1d:
			goto opcode_1d;
		case 0x1e:
			goto opcode_1e;
		case 0x1f:
			goto opcode_1f;

		case 0x20:
			goto opcode_20;
		case 0x21:
			goto opcode_21;
		case 0x22:
			goto opcode_22;
		case 0x23:
			goto opcode_23;
		case 0x24:
			goto opcode_24;
		case 0x25:
			goto opcode_25;
		case 0x26:
			goto opcode_26;
		case 0x27:
			goto opcode_27;
		case 0x28:
			goto opcode_28;
		case 0x29:
			goto opcode_29;
		case 0x2a:
			goto opcode_2a;
		case 0x2b:
			goto opcode_2b;
		case 0x2c:
			goto opcode_2c;
		case 0x2d:
			goto opcode_2d;
		case 0x2e:
			goto opcode_2e;
		case 0x2f:
			goto opcode_2f;

		case 0x30:
			goto opcode_30;
		case 0x31:
			goto opcode_31;
		case 0x32:
			goto opcode_32;
		case 0x33:
			goto opcode_33;
		case 0x34:
			goto opcode_34;
		case 0x35:
			goto opcode_35;
		case 0x36:
			goto opcode_36;
		case 0x37:
			goto opcode_37;
		case 0x38:
			goto opcode_38;
		case 0x39:
			goto opcode_39;
		case 0x3a:
			goto opcode_3a;
		case 0x3b:
			goto opcode_3b;
		case 0x3c:
			goto opcode_3c;
		case 0x3d:
			goto opcode_3d;
		case 0x3e:
			goto opcode_3e;
		case 0x3f:
			goto opcode_3f;

		case 0x40:
			goto opcode_40;
		case 0x41:
			goto opcode_41;
		case 0x42:
			goto opcode_42;
		case 0x43:
			goto opcode_43;
		case 0x44:
			goto opcode_44;
		case 0x45:
			goto opcode_45;
		case 0x46:
			goto opcode_46;
		case 0x47:
			goto opcode_47;
		case 0x48:
			goto opcode_48;
		case 0x49:
			goto opcode_49;
		case 0x4a:
			goto opcode_4a;
		case 0x4b:
			goto opcode_4b;
		case 0x4c:
			goto opcode_4c;
		case 0x4d:
			goto opcode_4d;
		case 0x4e:
			goto opcode_4e;
		case 0x4f:
			goto opcode_4f;

		case 0x50:
			goto opcode_50;
		case 0x51:
			goto opcode_51;
		case 0x52:
			goto opcode_52;
		case 0x53:
			goto opcode_53;
		case 0x54:
			goto opcode_54;
		case 0x55:
			goto opcode_55;
		case 0x56:
			goto opcode_56;
		case 0x57:
			goto opcode_57;
		case 0x58:
			goto opcode_58;
		case 0x59:
			goto opcode_59;
		case 0x5a:
			goto opcode_5a;
		case 0x5b:
			goto opcode_5b;
		case 0x5c:
			goto opcode_5c;
		case 0x5d:
			goto opcode_5d;
		case 0x5e:
			goto opcode_5e;
		case 0x5f:
			goto opcode_5f;

		case 0x60:
			goto opcode_60;
		case 0x61:
			goto opcode_61;
		case 0x62:
			goto opcode_62;
		case 0x63:
			goto opcode_63;
		case 0x64:
			goto opcode_64;
		case 0x65:
			goto opcode_65;
		case 0x66:
			goto opcode_66;
		case 0x67:
			goto opcode_67;
		case 0x68:
			goto opcode_68;
		case 0x69:
			goto opcode_69;
		case 0x6a:
			goto opcode_6a;
		case 0x6b:
			goto opcode_6b;
		case 0x6c:
			goto opcode_6c;
		case 0x6d:
			goto opcode_6d;
		case 0x6e:
			goto opcode_6e;
		case 0x6f:
			goto opcode_6f;

		case 0x70:
			goto opcode_70;
		case 0x71:
			goto opcode_71;
		case 0x72:
			goto opcode_72;
		case 0x73:
			goto opcode_73;
		case 0x74:
			goto opcode_74;
		case 0x75:
			goto opcode_75;
		case 0x76:
			goto opcode_76;
		case 0x77:
			goto opcode_77;
		case 0x78:
			goto opcode_78;
		case 0x79:
			goto opcode_79;
		case 0x7a:
			goto opcode_7a;
		case 0x7b:
			goto opcode_7b;
		case 0x7c:
			goto opcode_7c;
		case 0x7d:
			goto opcode_7d;
		case 0x7e:
			goto opcode_7e;
		case 0x7f:
			goto opcode_7f;

		case 0x80:
			goto opcode_80;
		case 0x81:
			goto opcode_81;
		case 0x82:
			goto opcode_82;
		case 0x83:
			goto opcode_83;
		case 0x84:
			goto opcode_84;
		case 0x85:
			goto opcode_85;
		case 0x86:
			goto opcode_86;
		case 0x87:
			goto opcode_87;
		case 0x88:
			goto opcode_88;
		case 0x89:
			goto opcode_89;
		case 0x8a:
			goto opcode_8a;
		case 0x8b:
			goto opcode_8b;
		case 0x8c:
			goto opcode_8c;
		case 0x8d:
			goto opcode_8d;
		case 0x8e:
			goto opcode_8e;
		case 0x8f:
			goto opcode_8f;

		case 0x90:
			goto opcode_90;
		case 0x91:
			goto opcode_91;
		case 0x92:
			goto opcode_92;
		case 0x93:
			goto opcode_93;
		case 0x94:
			goto opcode_94;
		case 0x95:
			goto opcode_95;
		case 0x96:
			goto opcode_96;
		case 0x97:
			goto opcode_97;
		case 0x98:
			goto opcode_98;
		case 0x99:
			goto opcode_99;
		case 0x9a:
			goto opcode_9a;
		case 0x9b:
			goto opcode_9b;
		case 0x9c:
			goto opcode_9c;
		case 0x9d:
			goto opcode_9d;
		case 0x9e:
			goto opcode_9e;
		case 0x9f:
			goto opcode_9f;

		case 0xa0:
			goto opcode_a0;
		case 0xa1:
			goto opcode_a1;
		case 0xa2:
			goto opcode_a2;
		case 0xa3:
			goto opcode_a3;
		case 0xa4:
			goto opcode_a4;
		case 0xa5:
			goto opcode_a5;
		case 0xa6:
			goto opcode_a6;
		case 0xa7:
			goto opcode_a7;
		case 0xa8:
			goto opcode_a8;
		case 0xa9:
			goto opcode_a9;
		case 0xaa:
			goto opcode_aa;
		case 0xab:
			goto opcode_ab;
		case 0xac:
			goto opcode_ac;
		case 0xad:
			goto opcode_ad;
		case 0xae:
			goto opcode_ae;
		case 0xaf:
			goto opcode_af;

		case 0xb0:
			goto opcode_b0;
		case 0xb1:
			goto opcode_b1;
		case 0xb2:
			goto opcode_b2;
		case 0xb3:
			goto opcode_b3;
		case 0xb4:
			goto opcode_b4;
		case 0xb5:
			goto opcode_b5;
		case 0xb6:
			goto opcode_b6;
		case 0xb7:
			goto opcode_b7;
		case 0xb8:
			goto opcode_b8;
		case 0xb9:
			goto opcode_b9;
		case 0xba:
			goto opcode_ba;
		case 0xbb:
			goto opcode_bb;
		case 0xbc:
			goto opcode_bc;
		case 0xbd:
			goto opcode_bd;
		case 0xbe:
			goto opcode_be;
		case 0xbf:
			goto opcode_bf;

		case 0xc0:
			goto opcode_c0;
		case 0xc1:
			goto opcode_c1;
		case 0xc2:
			goto opcode_c2;
		case 0xc3:
			goto opcode_c3;
		case 0xc4:
			goto opcode_c4;
		case 0xc5:
			goto opcode_c5;
		case 0xc6:
			goto opcode_c6;
		case 0xc7:
			goto opcode_c7;
		case 0xc8:
			goto opcode_c8;
		case 0xc9:
			goto opcode_c9;
		case 0xca:
			goto opcode_ca;
		case 0xcb:
			goto opcode_cb;
		case 0xcc:
			goto opcode_cc;
		case 0xcd:
			goto opcode_cd;
		case 0xce:
			goto opcode_ce;
		case 0xcf:
			goto opcode_cf;

		case 0xd0:
			goto opcode_d0;
		case 0xd1:
			goto opcode_d1;
		case 0xd2:
			goto opcode_d2;
		case 0xd3:
			goto opcode_d3;
		case 0xd4:
			goto opcode_d4;
		case 0xd5:
			goto opcode_d5;
		case 0xd6:
			goto opcode_d6;
		case 0xd7:
			goto opcode_d7;
		case 0xd8:
			goto opcode_d8;
		case 0xd9:
			goto opcode_d9;
		case 0xda:
			goto opcode_da;
		case 0xdb:
			goto opcode_db;
		case 0xdc:
			goto opcode_dc;
		case 0xdd:
			goto opcode_dd;
		case 0xde:
			goto opcode_de;
		case 0xdf:
			goto opcode_df;

		case 0xe0:
			goto opcode_e0;
		case 0xe1:
			goto opcode_e1;
		case 0xe2:
			goto opcode_e2;
		case 0xe3:
			goto opcode_e3;
		case 0xe4:
			goto opcode_e4;
		case 0xe5:
			goto opcode_e5;
		case 0xe6:
			goto opcode_e6;
		case 0xe7:
			goto opcode_e7;
		case 0xe8:
			goto opcode_e8;
		case 0xe9:
			goto opcode_e9;
		case 0xea:
			goto opcode_ea;
		case 0xeb:
			goto opcode_eb;
		case 0xec:
			goto opcode_ec;
		case 0xed:
			goto opcode_ed;
		case 0xee:
			goto opcode_ee;
		case 0xef:
			goto opcode_ef;

		case 0xf0:
			goto opcode_f0;
		case 0xf1:
			goto opcode_f1;
		case 0xf2:
			goto opcode_f2;
		case 0xf3:
			goto opcode_f3;
		case 0xf4:
			goto opcode_f4;
		case 0xf5:
			goto opcode_f5;
		case 0xf6:
			goto opcode_f6;
		case 0xf7:
			goto opcode_f7;
		case 0xf8:
			goto opcode_f8;
		case 0xf9:
			goto opcode_f9;
		case 0xfa:
			goto opcode_fa;
		case 0xfb:
			goto opcode_fb;
		case 0xfc:
			goto opcode_fc;
		case 0xfd:
			goto opcode_fd;
		case 0xfe:
			goto opcode_fe;
		case 0xff:
			goto opcode_ff;
		}
#endif

	  opcode_00:				/* BRK */
		if (!(regP & I_FLAG)) {
			UWORD retadr = PC + 1;
			memory[0x0100 + S--] = retadr >> 8;
			memory[0x0100 + S--] = retadr & 0xff;
			SetB;
			PHP;
			SetI;
			PC = (memory[0xffff] << 8) | memory[0xfffe];
		}
		goto next;

	  opcode_01:				/* ORA (ab,x) */
		INDIRECT_X;
		ORA(GetByte(addr));
		goto next;

	  opcode_04:				/* SKB [unofficial - skip byte] */
		PC++;
		goto next;

	  opcode_05:				/* ORA ab */
		ZPAGE;
		ORA(memory[addr]);
		goto next;

	  opcode_06:				/* ASL ab */
		ZPAGE;
		data = memory[addr];
		C = (data & 0x80) ? 1 : 0;
		Z = N = data << 1;
		memory[addr] = Z;
		goto next;

	  opcode_08:				/* PHP */
		PHP;
		goto next;

	  opcode_09:				/* ORA #ab */
		ORA(memory[PC++]);
		goto next;

	  opcode_0a:				/* ASL */
		C = (A & 0x80) ? 1 : 0;
		Z = N = A = A << 1;
		goto next;

	  opcode_0c:				/* SKW [unofficial - skip word] */
		PC += 2;
		goto next;

	  opcode_0d:				/* ORA abcd */
		ABSOLUTE;
		ORA(GetByte(addr));
		goto next;

	  opcode_0e:				/* ASL abcd */
		ABSOLUTE;
		data = GetByte(addr);
		C = (data & 0x80) ? 1 : 0;
		Z = N = data << 1;
		PutByte(addr, Z);
		goto next;

	  opcode_10:				/* BPL */
		if (!(N & 0x80)) {
			SBYTE sdata = memory[PC];
			NCYCLES_JMP;
			PC += (SWORD) sdata;
		}
		PC++;
		goto next;

	  opcode_11:				/* ORA (ab),y */
		INDIRECT_Y;
		NCYCLES_Y;
		ORA(GetByte(addr));
		goto next;

	  opcode_14:				/* SKB [unofficial - skip byte] */
		PC++;

		goto next;


	  opcode_15:				/* ORA ab,x */
		ZPAGE_X;
		ORA(memory[addr]);
		goto next;

	  opcode_16:				/* ASL ab,x */
		ZPAGE_X;
		data = memory[addr];
		C = (data & 0x80) ? 1 : 0;
		Z = N = data << 1;
		memory[addr] = Z;
		goto next;

	  opcode_18:				/* CLC */
		C = 0;
		goto next;

	  opcode_19:				/* ORA abcd,y */
		ABSOLUTE_Y;
		NCYCLES_Y;
		ORA(GetByte(addr));
		goto next;

	  opcode_1a:				/* NOP (1 byte) [unofficial] */
		goto next;

	  opcode_1c:				/* SKW [unofficial - skip word] !RS! */
		PC += 2;

		goto next;


	  opcode_1d:				/* ORA abcd,x */
		ABSOLUTE_X;
		NCYCLES_X;
		ORA(GetByte(addr));
		goto next;

	  opcode_1e:				/* ASL abcd,x */
		ABSOLUTE_X;
		data = GetByte(addr);
		C = (data & 0x80) ? 1 : 0;
		Z = N = data << 1;
		PutByte(addr, Z);
		goto next;

	  opcode_20:				/* JSR abcd */
		{
			UWORD retadr = PC + 1;
			memory[0x0100 + S--] = retadr >> 8;
			memory[0x0100 + S--] = retadr & 0xff;
			PC = (memory[PC + 1] << 8) | memory[PC];
		}
		goto next;

	  opcode_21:				/* AND (ab,x) */
		INDIRECT_X;
		AND(GetByte(addr));
		goto next;

	  opcode_24:				/* BIT ab */
		ZPAGE;
		N = memory[addr];
		V = N & 0x40;
		Z = (A & N);
		goto next;

	  opcode_25:				/* AND ab */
		ZPAGE;
		AND(memory[addr]);
		goto next;

	  opcode_26:				/* ROL ab */
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
		goto next;

	  opcode_28:				/* PLP */
		PLP;
		goto next;

	  opcode_29:				/* AND #ab */
		AND(memory[PC++]);
		goto next;

	  opcode_2a:				/* ROL */
		if (C) {
			C = (A & 0x80) ? 1 : 0;
			Z = N = A = (A << 1) | 1;
		}
		else {
			C = (A & 0x80) ? 1 : 0;
			Z = N = A = (A << 1);
		}
		goto next;

	  opcode_2c:				/* BIT abcd */
		ABSOLUTE;
		N = GetByte(addr);
		V = N & 0x40;
		Z = (A & N);
		goto next;

	  opcode_2d:				/* AND abcd */
		ABSOLUTE;
		AND(GetByte(addr));
		goto next;

	  opcode_2e:				/* ROL abcd */
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
		goto next;

	  opcode_30:				/* BMI */
		if (N & 0x80) {
			SBYTE sdata = memory[PC];
			NCYCLES_JMP;
			PC += (SWORD) sdata;
		}
		PC++;
		goto next;

	  opcode_31:				/* AND (ab),y */
		INDIRECT_Y;
		NCYCLES_Y;
		AND(GetByte(addr));
		goto next;

	  opcode_34:				/* SKB [unofficial - skip byte] */
		PC++;

		goto next;


	  opcode_35:				/* AND ab,x */
		ZPAGE_X;
		AND(memory[addr]);
		goto next;

	  opcode_36:				/* ROL ab,x */
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
		goto next;

	  opcode_38:				/* SEC */
		C = 1;
		goto next;

	  opcode_39:				/* AND abcd,y */
		ABSOLUTE_Y;
		NCYCLES_Y;
		AND(GetByte(addr));
		goto next;

	  opcode_3a:				/* NOP (1 byte) [unofficial] */
		goto next;

	  opcode_3c:				/* SKW [unofficial - skip word] */
		PC += 2;

		goto next;


	  opcode_3d:				/* AND abcd,x */
		ABSOLUTE_X;
		NCYCLES_X;
		AND(GetByte(addr));
		goto next;

	  opcode_3e:				/* ROL abcd,x */
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
		goto next;

	  opcode_40:				/* RTI */
		PLP;
		data = memory[0x0100 + ++S];
		PC = (memory[0x0100 + ++S] << 8) | data;
		goto next;

	  opcode_41:				/* EOR (ab,x) */
		INDIRECT_X;
		EOR(GetByte(addr));
		goto next;

	  opcode_44:				/* SKB [unofficial - skip byte] */
		PC++;

		goto next;


	  opcode_45:				/* EOR ab */
		ZPAGE;
		EOR(memory[addr]);
		goto next;

	  opcode_46:				/* LSR ab */
		ZPAGE;
		data = memory[addr];
		C = data & 1;
		Z = data >> 1;
		N = 0;
		memory[addr] = Z;
		goto next;

	  opcode_48:				/* PHA */
		memory[0x0100 + S--] = A;
		goto next;

	  opcode_49:				/* EOR #ab */
		EOR(memory[PC++]);
		goto next;

	  opcode_4a:				/* LSR */
		C = A & 1;
		A = A >> 1;
		N = 0;
		Z = A;
		goto next;

	  opcode_4c:				/* JMP abcd */
		PC = (memory[PC + 1] << 8) | memory[PC];
		goto next;

	  opcode_4d:				/* EOR abcd */
		ABSOLUTE;
		EOR(GetByte(addr));
		goto next;

	  opcode_4e:				/* LSR abcd */
		ABSOLUTE;
		data = GetByte(addr);
		C = data & 1;
		Z = data >> 1;
		N = 0;
		PutByte(addr, Z);
		goto next;

	  opcode_50:				/* BVC */
		if (!V) {
			SBYTE sdata = memory[PC];
			NCYCLES_JMP;
			PC += (SWORD) sdata;
		}
		PC++;
		goto next;

	  opcode_51:				/* EOR (ab),y */
		INDIRECT_Y;
		NCYCLES_Y;
		EOR(GetByte(addr));
		goto next;

	  opcode_54:				/* SKB [unofficial - skip byte] */
		PC++;

		goto next;


	  opcode_55:				/* EOR ab,x */
		ZPAGE_X;
		EOR(memory[addr]);
		goto next;

	  opcode_56:				/* LSR ab,x */
		ZPAGE_X;
		data = memory[addr];
		C = data & 1;
		Z = data >> 1;
		N = 0;
		memory[addr] = Z;
		goto next;

	  opcode_58:				/* CLI */
		ClrI;
		if (IRQ) {				/* check pending IRQ, helps in Lucasfilm games */
			UWORD retadr = PC;

			memory[0x0100 + S--] = retadr >> 8;
			memory[0x0100 + S--] = retadr & 0xff;
			PHP;
			SetI;
			PC = (memory[0xffff] << 8) | memory[0xfffe];
			IRQ = 0;
		}
		goto next;

	  opcode_59:				/* EOR abcd,y */
		ABSOLUTE_Y;
		NCYCLES_Y;
		EOR(GetByte(addr));
		goto next;

	  opcode_5a:				/* NOP (1 byte) [unofficial] */
		goto next;

	  opcode_5c:				/* SKW [unofficial - skip word] */
		PC += 2;

		goto next;


	  opcode_5d:				/* EOR abcd,x */
		ABSOLUTE_X;
		NCYCLES_X;
		EOR(GetByte(addr));
		goto next;

	  opcode_5e:				/* LSR abcd,x */
		ABSOLUTE_X;
		data = GetByte(addr);
		C = data & 1;
		Z = data >> 1;
		N = 0;
		PutByte(addr, Z);
		goto next;

	  opcode_60:				/* RTS */
		data = memory[0x0100 + ++S];
		PC = ((memory[0x0100 + ++S] << 8) | data) + 1;
		goto next;

	  opcode_61:				/* ADC (ab,x) */
		INDIRECT_X;
		data = GetByte(addr);
		goto adc;

	  opcode_64:				/* SKB [unofficial - skip byte] */
		PC++;

		goto next;


	  opcode_65:				/* ADC ab */
		ZPAGE;
		data = memory[addr];
		goto adc;

	  opcode_66:				/* ROR ab */
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
		goto next;

	  opcode_68:				/* PLA */
		Z = N = A = memory[0x0100 + ++S];
		goto next;

	  opcode_69:				/* ADC #ab */
		data = memory[PC++];
		goto adc;

	  opcode_6a:				/* ROR */
		if (C) {
			C = A & 1;
			Z = N = A = (A >> 1) | 0x80;
		}
		else {
			C = A & 1;
			Z = N = A = (A >> 1);
		}
		goto next;

	  opcode_6c:				/* JMP (abcd) */
		addr = (memory[PC + 1] << 8) | memory[PC];
#ifdef 65C02
		PC = (memory[addr + 1] << 8) | memory[addr];
#else							/* original 6502 had a bug in jmp (addr) when addr crossed page boundary */
		if ((addr & 0xff) == 0xff)
			PC = (memory[addr & ~0xff] << 8) | memory[addr];
		else
			PC = (memory[addr + 1] << 8) | memory[addr];
#endif
		goto next;

	  opcode_6d:				/* ADC abcd */
		ABSOLUTE;
		data = GetByte(addr);
		goto adc;

	  opcode_6e:				/* ROR abcd */
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
		goto next;

	  opcode_70:				/* BVS */
		if (V) {
			SBYTE sdata = memory[PC];
			NCYCLES_JMP;
			PC += (SWORD) sdata;
		}
		PC++;
		goto next;

	  opcode_71:				/* ADC (ab),y */
		INDIRECT_Y;
		NCYCLES_Y;
		data = GetByte(addr);
		goto adc;

	  opcode_74:				/* SKB [unofficial - skip byte] */
		PC++;

		goto next;


	  opcode_75:				/* ADC ab,x */
		ZPAGE_X;
		data = memory[addr];
		goto adc;

	  opcode_76:				/* ROR ab,x */
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
		goto next;

	  opcode_78:				/* SEI */
		SetI;
		goto next;

	  opcode_79:				/* ADC abcd,y */
		ABSOLUTE_Y;
		NCYCLES_Y;
		data = GetByte(addr);
		goto adc;

	  opcode_7a:				/* NOP (1 byte) [unofficial] */
		goto next;

	  opcode_7c:				/* SKW [unofficial - skip word] */
		PC += 2;

		goto next;


	  opcode_7d:				/* ADC abcd,x */
		ABSOLUTE_X;
		NCYCLES_X;
		data = GetByte(addr);
		goto adc;

	  opcode_7e:				/* ROR abcd,x */
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
		goto next;

	  opcode_80:				/* SKB [unofficial - skip byte] */
		PC++;

		goto next;


	  opcode_81:				/* STA (ab,x) */
		INDIRECT_X;
		PutByte(addr, A);
		goto next;

	  opcode_82:				/* SKB [unofficial - skip byte] */
		PC++;

		goto next;


	  opcode_84:				/* STY ab */
		ZPAGE;
		memory[addr] = Y;
		goto next;

	  opcode_85:				/* STA ab */
		ZPAGE;
		memory[addr] = A;
		goto next;

	  opcode_86:				/* STX ab */
		ZPAGE;
		memory[addr] = X;
		goto next;

	  opcode_88:				/* DEY */
		Z = N = --Y;
		goto next;

	  opcode_8a:				/* TXA */
		Z = N = A = X;
		goto next;

	  opcode_8c:				/* STY abcd */
		ABSOLUTE;
		PutByte(addr, Y);
		goto next;

	  opcode_8d:				/* STA abcd */
		ABSOLUTE;
		PutByte(addr, A);
		goto next;

	  opcode_8e:				/* STX abcd */
		ABSOLUTE;
		PutByte(addr, X);
		goto next;

	  opcode_90:				/* BCC */
		if (!C) {
			SBYTE sdata = memory[PC];
			NCYCLES_JMP;
			PC += (SWORD) sdata;
		}
		PC++;
		goto next;

	  opcode_91:				/* STA (ab),y */
		INDIRECT_Y;
		PutByte(addr, A);
		goto next;

	  opcode_94:				/* STY ab,x */
		ZPAGE_X;
		memory[addr] = Y;
		goto next;

	  opcode_95:				/* STA ab,x */
		ZPAGE_X;
		memory[addr] = A;
		goto next;

	  opcode_96:				/* STX ab,y */
		ZPAGE_Y;
		PutByte(addr, X);
		goto next;

	  opcode_98:				/* TYA */
		Z = N = A = Y;
		goto next;

	  opcode_99:				/* STA abcd,y */
		ABSOLUTE_Y;
		PutByte(addr, A);
		goto next;

	  opcode_9a:				/* TXS */
		S = X;
		goto next;

	  opcode_9d:				/* STA abcd,x */
		ABSOLUTE_X;
		PutByte(addr, A);
		goto next;

	  opcode_a0:				/* LDY #ab */
		LDY(memory[PC++]);
		goto next;

	  opcode_a1:				/* LDA (ab,x) */
		INDIRECT_X;
		LDA(GetByte(addr));
		goto next;

	  opcode_a2:				/* LDX #ab */
		LDX(memory[PC++]);
		goto next;

	  opcode_a3:				/* LAX (ind,x) [unofficial] */
		INDIRECT_X;
		Z = N = X = A = GetByte(addr);
		goto next;

	  opcode_a4:				/* LDY ab */
		ZPAGE;
		LDY(memory[addr]);
		goto next;

	  opcode_a5:				/* LDA ab */
		ZPAGE;
		LDA(memory[addr]);
		goto next;

	  opcode_a6:				/* LDX ab */
		ZPAGE;
		LDX(memory[addr]);
		goto next;

	  opcode_a7:				/* LAX zpage [unofficial] */
		ZPAGE;
		Z = N = X = A = GetByte(addr);
		goto next;

	  opcode_a8:				/* TAY */
		Z = N = Y = A;
		goto next;

	  opcode_a9:				/* LDA #ab */
		LDA(memory[PC++]);
		goto next;

	  opcode_aa:				/* TAX */
		Z = N = X = A;
		goto next;

	  opcode_ac:				/* LDY abcd */
		ABSOLUTE;
		LDY(GetByte(addr));
		goto next;

	  opcode_ad:				/* LDA abcd */
		ABSOLUTE;
		LDA(GetByte(addr));
		goto next;

	  opcode_ae:				/* LDX abcd */
		ABSOLUTE;
		LDX(GetByte(addr));
		goto next;

	  opcode_af:				/* LAX absolute [unofficial] */
		ABSOLUTE;
		Z = N = X = A = GetByte(addr);
		goto next;

	  opcode_b0:				/* BCS */
		if (C) {
			SBYTE sdata = memory[PC];
			NCYCLES_JMP;
			PC += (SWORD) sdata;
		}
		PC++;
		goto next;

	  opcode_b1:				/* LDA (ab),y */
		INDIRECT_Y;
		NCYCLES_Y;
		LDA(GetByte(addr));
		goto next;

	  opcode_b3:				/* LAX (ind),y [unofficial] */
		INDIRECT_Y;
		Z = N = X = A = GetByte(addr);
		goto next;

	  opcode_b4:				/* LDY ab,x */
		ZPAGE_X;
		LDY(memory[addr]);
		goto next;

	  opcode_b5:				/* LDA ab,x */
		ZPAGE_X;
		LDA(memory[addr]);
		goto next;

	  opcode_b6:				/* LDX ab,y */
		ZPAGE_Y;
		LDX(GetByte(addr));
		goto next;

	  opcode_b7:				/* LAX zpage,y [unofficial] */
		ZPAGE_Y;
		Z = N = X = A = GetByte(addr);
		goto next;

	  opcode_b8:				/* CLV */
		V = 0;
		goto next;

	  opcode_b9:				/* LDA abcd,y */
		ABSOLUTE_Y;
		NCYCLES_Y;
		LDA(GetByte(addr));
		goto next;

	  opcode_ba:				/* TSX */
		Z = N = X = S;
		goto next;

	  opcode_bc:				/* LDY abcd,x */
		ABSOLUTE_X;
		NCYCLES_X;
		LDY(GetByte(addr));
		goto next;

	  opcode_bd:				/* LDA abcd,x */
		ABSOLUTE_X;
		NCYCLES_X;
		LDA(GetByte(addr));
		goto next;

	  opcode_be:				/* LDX abcd,y */
		ABSOLUTE_Y;
		NCYCLES_Y;
		LDX(GetByte(addr));
		goto next;

	  opcode_bf:				/* LAX absolute,y [unofficial] */
		ABSOLUTE_Y;
		Z = N = X = A = GetByte(addr);
		goto next;

	  opcode_c0:				/* CPY #ab */
		CPY(memory[PC++]);
		goto next;

	  opcode_c1:				/* CMP (ab,x) */
		INDIRECT_X;
		CMP(GetByte(addr));
		goto next;

	  opcode_c2:				/* SKB [unofficial - skip byte] */
		PC++;

		goto next;


	  opcode_c4:				/* CPY ab */
		ZPAGE;
		CPY(memory[addr]);
		goto next;

	  opcode_c5:				/* CMP ab */
		ZPAGE;
		CMP(memory[addr]);
		goto next;

	  opcode_c6:				/* DEC ab */
		ZPAGE;
		Z = N = --memory[addr];
		goto next;

	  opcode_c8:				/* INY */
		Z = N = ++Y;
		goto next;

	  opcode_c9:				/* CMP #ab */
		CMP(memory[PC++]);
		goto next;

	  opcode_ca:				/* DEX */
		Z = N = --X;
		goto next;

	  opcode_cc:				/* CPY abcd */
		ABSOLUTE;
		CPY(GetByte(addr));
		goto next;

	  opcode_cd:				/* CMP abcd */
		ABSOLUTE;
		CMP(GetByte(addr));
		goto next;

	  opcode_ce:				/* DEC abcd */
		ABSOLUTE;
		Z = N = GetByte(addr) - 1;
		PutByte(addr, Z);
		goto next;

	  opcode_d0:				/* BNE */
		if (Z) {
			SBYTE sdata = memory[PC];
			NCYCLES_JMP;
			PC += (SWORD) sdata;
		}
		PC++;
		goto next;

	  opcode_d1:				/* CMP (ab),y */
		INDIRECT_Y;
		NCYCLES_Y;
		CMP(GetByte(addr));
		goto next;

	  opcode_d4:				/* SKB [unofficial - skip byte] */
		PC++;

		goto next;


	  opcode_d5:				/* CMP ab,x */
		ZPAGE_X;
		CMP(memory[addr]);
		Z = N = A - data;
		C = (A >= data);
		goto next;

	  opcode_d6:				/* DEC ab,x */
		ZPAGE_X;
		Z = N = --memory[addr];
		goto next;

	  opcode_d8:				/* CLD */
		ClrD;
		goto next;

	  opcode_d9:				/* CMP abcd,y */
		ABSOLUTE_Y;
		NCYCLES_Y;
		CMP(GetByte(addr));
		goto next;

	  opcode_da:				/* NOP (1 byte) [unofficial] */
		goto next;

	  opcode_dc:				/* SKW [unofficial - skip word] */
		PC += 2;
		goto next;

	  opcode_dd:				/* CMP abcd,x */
		ABSOLUTE_X;
		NCYCLES_X;
		CMP(GetByte(addr));
		goto next;

	  opcode_de:				/* DEC abcd,x */
		ABSOLUTE_X;
		Z = N = GetByte(addr) - 1;
		PutByte(addr, Z);
		goto next;

	  opcode_e0:				/* CPX #ab */
		CPX(memory[PC++]);
		goto next;

	  opcode_e1:				/* SBC (ab,x) */
		INDIRECT_X;
		data = GetByte(addr);
		goto sbc;

	  opcode_e2:				/* SKB [unofficial - skip byte] */
		PC++;

		goto next;


	  opcode_e4:				/* CPX ab */
		ZPAGE;
		CPX(memory[addr]);
		goto next;

	  opcode_e5:				/* SBC ab */
		ZPAGE;
		data = memory[addr];
		goto sbc;

	  opcode_e6:				/* INC ab */
		ZPAGE;
		Z = N = ++memory[addr];
		goto next;

	  opcode_e8:				/* INX */
		Z = N = ++X;
		goto next;

	  opcode_e9:				/* SBC #ab */
		data = memory[PC++];
		goto sbc;

	  opcode_ea:				/* NOP */
		goto next;

	  opcode_ec:				/* CPX abcd */
		ABSOLUTE;
		CPX(GetByte(addr));
		goto next;

	  opcode_ed:				/* SBC abcd */
		ABSOLUTE;
		data = GetByte(addr);
		goto sbc;

	  opcode_ee:				/* INC abcd */
		ABSOLUTE;
		Z = N = GetByte(addr) + 1;
		PutByte(addr, Z);
		goto next;

	  opcode_f0:				/* BEQ */
		if (!Z) {
			SBYTE sdata = memory[PC];
			NCYCLES_JMP;
			PC += (SWORD) sdata;
		}
		PC++;
		goto next;

	  opcode_f1:				/* SBC (ab),y */
		INDIRECT_Y;
		NCYCLES_Y;
		data = GetByte(addr);
		goto sbc;

	  opcode_f4:				/* SKB [unofficial - skip byte] */
		PC++;

		goto next;


	  opcode_f5:				/* SBC ab,x */
		ZPAGE_X;
		data = memory[addr];
		goto sbc;

	  opcode_f6:				/* INC ab,x */
		ZPAGE_X;
		Z = N = ++memory[addr];
		goto next;

	  opcode_f8:				/* SED */
		SetD;
		goto next;

	  opcode_f9:				/* SBC abcd,y */
		ABSOLUTE_Y;
		NCYCLES_Y;
		data = GetByte(addr);
		goto sbc;

	  opcode_fa:				/* NOP (1 byte) [unofficial] */
		goto next;

	  opcode_fc:				/* SKW [unofficial - skip word] */
		PC += 2;

		goto next;


	  opcode_fd:				/* SBC abcd,x */
		ABSOLUTE_X;
		NCYCLES_X;
		data = GetByte(addr);
		goto sbc;

	  opcode_fe:				/* INC abcd,x */
		ABSOLUTE_X;
		Z = N = GetByte(addr) + 1;
		PutByte(addr, Z);
		goto next;

	  opcode_d2:				/* ESCRTS #ab (JAM) - on Atari is here instruction CIM [unofficial] !RS! */
		data = memory[PC++];
		UPDATE_GLOBAL_REGS;
		CPU_GetStatus();
		Escape(data);
		CPU_PutStatus();
		UPDATE_LOCAL_REGS;
		data = memory[0x0100 + ++S];
		PC = ((memory[0x0100 + ++S] << 8) | data) + 1;
		goto next;

	  opcode_f2:				/* ESC #ab (JAM) - on Atari is here instruction CIM [unofficial] !RS! */
		/* opcode_ff: ESC #ab - opcode FF is now used for INS [unofficial] instruction !RS! */
		data = memory[PC++];
		UPDATE_GLOBAL_REGS;
		CPU_GetStatus();
		Escape(data);
		CPU_PutStatus();
		UPDATE_LOCAL_REGS;
		goto next;

/* R.S.980113
   My changes of original code is marked !RS!
   UNDOCUMENTED INSTRUCTIONS
   NOP (2 bytes) => SKB [unofficial - skip byte]
   NOP (3 bytes) => SKW [unofficial - skip word]
   CIM [unoficial - crash intermediate] => implemented as "PC--;"
   AXS [unofficial - Store result A AND X]
   ASO [unofficial - ASL then ORA with Acc]
   XAA [unofficial - X AND Mem to Acc]
   MKA [unofficial - Acc AND #$40 to Acc]
   MKX [unofficial - X AND #$40 to X]
 */

	  opcode_02:				/* CIM [unofficial - crash intermediate] */
	  opcode_12:
	  opcode_22:
	  opcode_32:
	  opcode_42:
	  opcode_52:
	  opcode_62:
	  opcode_72:
	  opcode_92:
	  opcode_b2:
		/* opcode_d2: Used for ESCRTS #ab (JAM) */
		/* opcode_f2: Used for ESC #ab (JAM) */
		PC--;

		goto next;


	  opcode_83:				/* AXS (ab,x) [unofficial - Store result A AND X] */
		INDIRECT_X;

		Z = N = A & X;

		PutByte(addr, Z);

		goto next;


	  opcode_87:				/* AXS zpage [unofficial - Store result A AND X] */
		ZPAGE;

		Z = N = memory[addr] = A & X;

		goto next;


	  opcode_8f:				/* AXS abcd [unofficial - Store result A AND X] */
		ABSOLUTE;

		Z = N = A & X;

		PutByte(addr, Z);

		goto next;


	  opcode_93:				/* AXS (ab),y [unofficial - Store result A AND X] */
		INDIRECT_Y;

		Z = N = A & X;

		PutByte(addr, Z);

		goto next;


	  opcode_97:				/* AXS zpage,y [unofficial - Store result A AND X] */
		ZPAGE_Y;

		Z = N = memory[addr] = A & X;

		goto next;


	  opcode_03:				/* ASO (ab,x) [unofficial - ASL then ORA with Acc] */
		INDIRECT_X;

		data = GetByte(addr);

		C = (data & 0x80) ? 1 : 0;

		Z = N = A | (data << 1);

		goto next;


	  opcode_07:				/* ASO zpage [unofficial - ASL then ORA with Acc] */
		ZPAGE;

		data = memory[addr];

		C = (data & 0x80) ? 1 : 0;

		Z = N = A | (data << 1);

		goto next;


	  opcode_0b:				/* ASO #ab [unofficial - ASL then ORA with Acc] */
		data = memory[PC++];

		C = (data & 0x80) ? 1 : 0;

		Z = N = A | (data << 1);

		goto next;


	  opcode_0f:				/* ASO abcd [unofficial - ASL then ORA with Acc] */
		ABSOLUTE;

		data = GetByte(addr);

		C = (data & 0x80) ? 1 : 0;

		Z = N = A | (data << 1);

		goto next;


	  opcode_13:				/* ASO (ab),y [unofficial - ASL then ORA with Acc] */
		INDIRECT_Y;

		data = GetByte(addr);

		C = (data & 0x80) ? 1 : 0;

		Z = N = A | (data << 1);

		goto next;


	  opcode_17:				/* ASO zpage,x [unofficial - ASL then ORA with Acc] */
		ZPAGE_X;

		data = memory[addr];

		C = (data & 0x80) ? 1 : 0;

		Z = N = A | (data << 1);

		goto next;


	  opcode_1b:				/* ASO abcd,y [unofficial - ASL then ORA with Acc] */
		ABSOLUTE_Y;

		data = GetByte(addr);

		C = (data & 0x80) ? 1 : 0;

		Z = N = A | (data << 1);

		goto next;


	  opcode_1f:				/* ASO abcd,x [unofficial - ASL then ORA with Acc] */
		ABSOLUTE_X;

		data = GetByte(addr);

		C = (data & 0x80) ? 1 : 0;

		Z = N = A | (data << 1);

		goto next;


	  opcode_8b:				/* XAA #ab [unofficial - X AND Mem to Acc] */
		Z = N = A = X & memory[PC++];

		goto next;


	  opcode_9b:				/* XAA abcd,y [unofficial - X AND Mem to Acc] */
		ABSOLUTE_Y;

		Z = N = A = X & GetByte(addr);

		goto next;


	  opcode_9f:				/* MKA abcd [unofficial - Acc AND #$40 to Acc] */
		ABSOLUTE;

		Z = N = A &= 0x04;

		goto next;


	  opcode_9e:				/* MKX abcd [unofficial - X AND #$40 to X] */
		ABSOLUTE;

		Z = N = X &= 0x04;

		goto next;


/* UNDOCUMENTED INSTRUCTIONS (Part II)
   LSE [unofficial - LSR then EOR result with A]
   ALR [unofficial - Acc AND Data, LSR result]
   ARR [unofficial - Acc AND Data, ROR result]
 */
	  opcode_43:				/* LSE (ab,x) [unofficial - LSR then EOR result with A] */
		INDIRECT_X;

		data = GetByte(addr);

		C = data & 1;

		Z = N = A ^ (data >> 1);

		goto next;


	  opcode_47:				/* LSE zpage [unofficial - LSR then EOR result with A] */
		ZPAGE;

		data = memory[addr];

		C = data & 1;

		Z = N = A ^ (data >> 1);

		goto next;


	  opcode_4f:				/* LSE abcd [unofficial - LSR then EOR result with A] */
		ABSOLUTE;

		data = GetByte(addr);

		C = data & 1;

		Z = N = A ^ (data >> 1);

		goto next;


	  opcode_53:				/* LSE (ab),y [unofficial - LSR then EOR result with A] */
		INDIRECT_Y;

		data = GetByte(addr);

		C = data & 1;

		Z = N = A ^ (data >> 1);

		goto next;


	  opcode_57:				/* LSE zpage,x [unofficial - LSR then EOR result with A] */
		ZPAGE_X;

		data = memory[addr];

		C = data & 1;

		Z = N = A ^ (data >> 1);

		goto next;


	  opcode_5b:				/* LSE abcd,y [unofficial - LSR then EOR result with A] */
		ABSOLUTE_Y;

		data = GetByte(addr);

		C = data & 1;

		Z = N = A ^ (data >> 1);

		goto next;


	  opcode_5f:				/* LSE abcd,x [unofficial - LSR then EOR result with A] */
		ABSOLUTE_X;

		data = GetByte(addr);

		C = data & 1;

		Z = N = A ^ (data >> 1);

		goto next;


	  opcode_4b:				/* ALR #ab [unofficial - Acc AND Data, LSR result] */
		data = A & memory[PC++];

		C = data & 1;

		Z = N = (data >> 1);

		goto next;


	  opcode_6b:				/* ARR #ab [unofficial - Acc AND Data, ROR result] */
		data = A & memory[PC++];

		if (C) {

			C = data & 1;

			Z = N = (data >> 1) | 0x80;

		}

		else {

			C = data & 1;

			Z = N = (data >> 1);

		}

		goto next;


/* UNDOCUMENTED INSTRUCTIONS (Part III)
   RLA [unofficial - ROL Mem, then AND with A]
 */
	  opcode_23:				/* RLA (ab,x) [unofficial - ROL Mem, then AND with A] */
		INDIRECT_X;

	  rla:
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

		Z = N = A & Z;

		goto next;


	  opcode_27:				/* RLA zpage [unofficial - ROL Mem, then AND with A] */
		ZPAGE;

	  rla_zpage:
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

		Z = N = A & Z;

		goto next;


	  opcode_2b:				/* RLA #ab [unofficial - ROL Mem, then AND with A] */
		data = memory[PC++];

		if (C) {

			C = (data & 0x80) ? 1 : 0;
			Z = N = (data << 1) | 1;

		}

		else {

			C = (data & 0x80) ? 1 : 0;
			Z = N = (data << 1);

		}

		Z = N = A & Z;

		goto next;


	  opcode_2f:				/* RLA abcd [unofficial - ROL Mem, then AND with A] */
		ABSOLUTE;

		goto rla;


	  opcode_33:				/* RLA (ab),y [unofficial - ROL Mem, then AND with A] */
		INDIRECT_Y;

		goto rla;


	  opcode_37:				/* RLA zpage,x [unofficial - ROL Mem, then AND with A] */
		ZPAGE_X;

		goto rla_zpage;


	  opcode_3b:				/* RLA abcd,y [unofficial - ROL Mem, then AND with A] */
		ABSOLUTE_Y;

		goto rla;


	  opcode_3f:				/* RLA abcd,x [unofficial - ROL Mem, then AND with A] */
		ABSOLUTE_X;

		goto rla;


/* R.S.980113<<<
   R.S.980114>>>
   RRA [unofficial - ROR Mem, then ADC to Acc]
 */

	  opcode_63:				/* RRA (ab,x) [unofficial - ROR Mem, then ADC to Acc] */
		INDIRECT_X;
	  rra:
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

		goto adc;


	  opcode_67:				/* RRA zpage [unofficial - ROR Mem, then ADC to Acc] */
		ZPAGE;

	  rra_zpage:
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

		goto adc;


	  opcode_6f:				/* RRA abcd [unofficial - ROR Mem, then ADC to Acc] */
		ABSOLUTE;

		goto rra;


	  opcode_73:				/* RRA (ab),y [unofficial - ROR Mem, then ADC to Acc] */
		INDIRECT_Y;

		goto rra;


	  opcode_77:				/* RRA zpage,x [unofficial - ROR Mem, then ADC to Acc] */
		ZPAGE_X;

		goto rra_zpage;


	  opcode_7b:				/* RRA abcd,y [unofficial - ROR Mem, then ADC to Acc] */
		ABSOLUTE_Y;

		goto rra;


	  opcode_7f:				/* RRA abcd,x [unofficial - ROR Mem, then ADC to Acc] */
		ABSOLUTE_X;

		goto rra;


/* OAL [unofficial - ORA Acc with #$EE, then AND with data, then TAX] */

	  opcode_ab:				/* OAL #ab [unofficial - ORA Acc with #$EE, then AND with data, then TAX] */
		A |= 0xEE;

		A &= memory[PC++];

		Z = N = X = A;

		goto next;


/* DCM [unofficial - DEC Mem then CMP with Acc] */

	  opcode_c3:				/* DCM (ab,x) [unofficial - DEC Mem then CMP with Acc] */
		INDIRECT_X;

	  dcm:
		data = GetByte(addr) - 1;

		PutByte(addr, data);

		CMP(data);

		goto next;


	  opcode_c7:				/* DCM zpage [unofficial - DEC Mem then CMP with Acc] */
		ZPAGE;

	  dcm_zpage:

		data = memory[addr] = memory[addr] - 1;

		CMP(data);

		goto next;


	  opcode_cf:				/* DCM abcd [unofficial - DEC Mem then CMP with Acc] */
		ABSOLUTE;

		goto dcm;


	  opcode_d3:				/* DCM (ab),y [unofficial - DEC Mem then CMP with Acc] */
		INDIRECT_Y;

		goto dcm;


	  opcode_d7:				/* DCM zpage,x [unofficial - DEC Mem then CMP with Acc] */
		ZPAGE_X;

		goto dcm_zpage;


	  opcode_db:				/* DCM abcd,y [unofficial - DEC Mem then CMP with Acc] */
		ABSOLUTE_Y;

		goto dcm;


	  opcode_df:				/* DCM abcd,x [unofficial - DEC Mem then CMP with Acc] */
		ABSOLUTE_X;

		goto dcm;


/* SAX   [unofficial - A AND X, then SBC Mem, store to X] */

	  opcode_cb:				/* SAX #ab [unofficial - A AND X, then SBC Mem, store to X] */
		X = A & X;

		data = memory[PC++];

		if (!(regP & D_FLAG)) {

			UWORD temp;

			UWORD t_data;


			t_data = (UWORD) data + (UWORD) ! C;

			temp = (UWORD) X - t_data;


			Z = N = temp & 0xff;


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

		goto next;


/* INS [unofficial - INC Mem then SBC with Acc] */

	  opcode_e3:				/* INS (ab,x) [unofficial - INC Mem then SBC with Acc] */
		INDIRECT_X;

	  ins:
		data = Z = N = GetByte(addr) + 1;

		PutByte(addr, data);

		goto sbc;


	  opcode_e7:				/* INS zpage [unofficial - INC Mem then SBC with Acc] */
		ZPAGE;

	  ins_zpage:
		data = Z = N = memory[addr] + 1;

		memory[addr] = data;

		goto sbc;


	  opcode_ef:				/* INS abcd [unofficial - INC Mem then SBC with Acc] */
		ABSOLUTE;

		goto ins;

	  opcode_f3:				/* INS (ab),y [unofficial - INC Mem then SBC with Acc] */
		INDIRECT_Y;

		goto ins;


	  opcode_f7:				/* INS zpage,x [unofficial - INC Mem then SBC with Acc] */
		ZPAGE_X;

		goto ins_zpage;


	  opcode_fb:				/* INS abcd,y [unofficial - INC Mem then SBC with Acc] */
		ABSOLUTE_Y;

		goto ins;


	  opcode_ff:				/* INS abcd,x [unofficial - INC Mem then SBC with Acc] */
		ABSOLUTE_X;

		goto ins;


/* R.S.980114<<< */

	  opcode_89:				/* SKB [unofficial - skip byte] */
		PC++;

		goto next;


	  opcode_9c:				/* SKW [unofficial - skip word] */
		PC += 2;

		goto next;


/* AXA [unofficial - original decode by R.Sterba and R.Petruzela 15.1.1998 :-)]
   AXA - this is our new imaginative name for instruction with opcode hex BB.
   AXA - Store Mem AND #$FD to Acc and X, then set stackpoint to value (Acc - 4)
   It's cool! :-)
 */

	  opcode_bb:				/* AXA abcd,y [unofficial - Store Mem AND #$FD to Acc and X, then set stackpoint to value (Acc - 4) */
		ABSOLUTE_Y;

		Z = N = A = X = GetByte(addr) & 0xfd;
		S = ((UWORD) A - 4) & 0xff;

		goto next;


	  opcode_eb:				/* SBC #ab [unofficial] */
		data = memory[PC++];

		goto sbc;


	  adc:
		if (!(regP & D_FLAG)) {
			UWORD temp;
			UWORD t_data;

			t_data = (UWORD) data + (UWORD) C;
			Z = N = temp = (UWORD) A + t_data;

			V = (~(A ^ t_data)) & (Z ^ A) & 0x80;
			C = temp >> 8;
			A = Z;
		}
		else {
			int bcd1, bcd2;

			bcd1 = BCDtoDEC[A];
			bcd2 = BCDtoDEC[data];

			bcd1 += bcd2 + C;

			Z = N = DECtoBCD[bcd1];

			V = (Z ^ A) & 0x80;
			C = (bcd1 > 99);
			A = Z;
		}
		goto next;

	  sbc:
		if (!(regP & D_FLAG)) {
			UWORD temp;
			UWORD t_data;

			t_data = (UWORD) data + (UWORD) ! C;
			temp = (UWORD) A - t_data;

			Z = N = temp & 0xff;

/*
 * This was the old code that I have been using upto version 0.5.2
 *  C = (A < ((UWORD)data + (UWORD)!C)) ? 0 : 1;
 */
			V = (~(A ^ t_data)) & (Z ^ A) & 0x80;
			C = (temp > 255) ? 0 : 1;
			A = Z;
		}
		else {
			int bcd1, bcd2;

			bcd1 = BCDtoDEC[A];
			bcd2 = BCDtoDEC[data];

			bcd1 = bcd1 - bcd2 - !C;

			if (bcd1 < 0)
				bcd1 = 100 - (-bcd1);
			Z = N = DECtoBCD[bcd1];

			C = (A < (data + !C)) ? 0 : 1;
			V = (Z ^ A) & 0x80;
			A = Z;
		}
		goto next;

	  next:
		continue;
	}

	UPDATE_GLOBAL_REGS;
	if (wsync_halt && ncycles >= 0)
		return 0;				/* WSYNC stopped CPU */
	return ncycles;
}
