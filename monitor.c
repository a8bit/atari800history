#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>

#ifdef VMS
#include <file.h>
#endif

#include "system.h"
#include "cpu.h"

#define FALSE   0
#define TRUE    1

extern UBYTE	memory[65536];

unsigned int disassemble (UWORD addr1, UWORD addr2);
void show_opcode (UBYTE instr);
void show_operand (UBYTE instr);

char *get_token (char *string)
{
	static char     *s;
	char            *t;

	if (string) s = string;                 /* New String */

	while (*s == ' ') s++;                  /* Skip Leading Spaces */

	if (*s)
	{
		t = s;                          /* Start of String */
		while (*s != ' ' && *s)         /* Locate End of String */
		{
			s++;
		}

		if (*s == ' ')                  /* Space Terminated ? */
		{
			*s = '\0';              /* C String Terminator */
			s++;                    /* Point to Next Char */
		}
	}
	else
	{
		t = NULL;
	}

	return t;                               /* Pointer to String */
}

int get_hex (char *string, UWORD *hexval)
{
	int	ihexval;
	char    *t;

	t = get_token(string);
	if (t)
	{
		sscanf (t,"%x",&ihexval);
		*hexval = ihexval;
		return 1;
	}

	return 0;
}

int monitor ()
{
	CPU_Status	cpu_status;

	UWORD	addr;
	UWORD	break_addr;
	char	s[128];
	int     p;

	addr = 0;

	CPU_GetStatus (&cpu_status);

	while (TRUE)
	{
		char    *t;

		printf ("> ");
		gets (s);

		for (p=0;s[p]!=0;p++)
		  if (islower(s[p]))
		    s[p] = toupper(s[p]);

		t = get_token(s);

		if (strcmp(t,"BREAK") == 0)
		{
			get_hex (NULL, &break_addr);
		}
		else if (strcmp(t,"CONT") == 0)
		{
			return 1;
		}
		else if (strcmp(t,"SETPC") == 0)
		{
			get_hex (NULL, &addr);

			cpu_status.PC = addr;
		}
		else if (strcmp(t,"SETS") == 0)
		{
			get_hex (NULL, &addr);
			cpu_status.S = addr & 0xff;
		}
		else if (strcmp(t,"SETA") == 0)
		{
			get_hex (NULL, &addr);
			cpu_status.A = addr & 0xff;
		}
		else if (strcmp(t,"SETX") == 0)
		{
			get_hex (NULL, &addr);
			cpu_status.X = addr & 0xff;
		}
		else if (strcmp(t,"SETY") == 0)
		{
			get_hex (NULL, &addr);
			cpu_status.Y = addr & 0xff;
		}
		else if (strcmp(t,"SETN") == 0)
		{
			get_hex (NULL, &addr);
			cpu_status.flag.N = addr;
		}
		else if (strcmp(t,"SETV") == 0)
		{
			get_hex (NULL, &addr);
			cpu_status.flag.V = addr;
		}
		else if (strcmp(t,"SETB") == 0)
		{
			get_hex (NULL, &addr);
			cpu_status.flag.B = addr;
		}
		else if (strcmp(t,"SETD") == 0)
		{
			get_hex (NULL, &addr);
			cpu_status.flag.D = addr;
		}
		else if (strcmp(t,"SETI") == 0)
		{
			get_hex (NULL, &addr);
			cpu_status.flag.I = addr;
		}
		else if (strcmp(t,"SETZ") == 0)
		{
			get_hex (NULL, &addr);
			cpu_status.flag.Z = addr;
		}
		else if (strcmp(t,"SETC") == 0)
		{
			get_hex (NULL, &addr);
			cpu_status.flag.C = addr;
		}
		else if (strcmp(t,"SHOW") == 0)
		{
			printf ("PC=%4x, A=%2x, S=%2x, X=%2x, Y=%2x, C=%1x, Z=%1x, I=%1x, D=%1x, B=%1x, V=%1x, N=%1x\n",
				cpu_status.PC,
				cpu_status.A,
				cpu_status.S,
				cpu_status.X,
				cpu_status.Y,
				cpu_status.flag.C,
				cpu_status.flag.Z,
				cpu_status.flag.I,
				cpu_status.flag.D,
				cpu_status.flag.B,
				cpu_status.flag.V,
				cpu_status.flag.N);
		}
		else if (strcmp(t,"ROM") == 0)
		{
			UWORD	addr1;
			UWORD	addr2;

			int	status;

			status = get_hex (NULL, &addr1);
			status |= get_hex (NULL, &addr2);

			if (status)
			{
				SetROM (addr1, addr2);
				printf ("Changed Memory from %4x to %4x into ROM\n", addr1, addr2);
			}
			else
			{
				printf ("*** Memory Unchanged (Missing Parameter) ***\n");
			}
		}
		else if (strcmp(t,"RAM") == 0)
		{
			UWORD	addr1;
			UWORD	addr2;

			int	status;

			status = get_hex (NULL, &addr1);
			status |= get_hex (NULL, &addr2);

			if (status)
			{
				SetRAM (addr1, addr2);
				printf ("Changed Memory from %4x to %4x into RAM\n", addr1, addr2);
			}
			else
			{
				printf ("*** Memory Unchanged (Missing Parameter) ***\n");
			}
		}
		else if (strcmp(t,"WRITE") == 0)
		{
			UWORD	addr1;
			UWORD	addr2;

			int	status;

			status = get_hex (NULL, &addr1);
			status |= get_hex (NULL, &addr2);

			if (status)
			{
				int	fd;

				fd = open ("memdump.dat", O_CREAT | O_TRUNC | O_WRONLY, 0777);
				if (fd == -1)
				{
					perror ("open");
					Atari800_Exit (FALSE);
					exit (1);
				}

				write (fd, &memory[addr1], addr2 - addr1 + 1);

				close (fd);
			}
		}
		else if (strcmp(t,"SUM") == 0)
		{
			UWORD	addr1;
			UWORD	addr2;
			int status;

			status = get_hex (NULL, &addr1);
			status |= get_hex (NULL, &addr2);

			if (status)
			{
			  int sum = 0;
			  int i;

			  for (i=addr1;i<=addr2;i++)
			    sum += (UWORD)memory[i];
			  printf ("SUM: %x\n", sum);
			}
		}
		else if (strcmp(t,"D") == 0)
		{
			int	addr1;
			int	addr2;
			UWORD	xaddr1;
			UWORD	xaddr2;
			UWORD	temp;
			int	i;

			addr1 = addr;
			addr2 = 0;

			get_hex (NULL, &xaddr1);
			get_hex (NULL, &xaddr2);

			addr1 = xaddr1;
			addr2 = xaddr2;

			if (addr2 == 0) addr2 = addr1 + 255;

			addr = addr2 + 1;

			while (addr1 <= addr2)
			{
				temp = addr1;

				printf ("%4x : ",temp);

				for (i=0;i<16;i++)
				{
					printf ("%2x ",GetByte(temp));
					temp++;
					if (temp > addr2) break;
				}

				temp = addr1;

				printf ("\t");

				for (i=0;i<16;i++)
				{
					if (isalnum(GetByte(temp)))
					{
						printf ("%c",GetByte(temp));
					}
					else
					{
						printf (".");
					}
					temp++;
					if (temp > addr2) break;
				}

				printf ("\n");

				addr1 += 16;
			}
		}
		else if (strcmp(t,"M") == 0)
		{
			UWORD	addr;
			UWORD	temp;

			get_hex (NULL, &addr);

			while (get_hex(NULL, &temp))
			{
				PutByte (addr,(UBYTE)temp);
				addr++;
			}
		}
		else if (strcmp(t,"Y") == 0)
		{
			UWORD	addr1;
			UWORD	addr2;

			addr1 = addr;
			addr2 = 0;

			get_hex (NULL, &addr1);
			get_hex (NULL, &addr2);

			addr = disassemble (addr1,addr2);
		}
		else if (strcmp(t,"HELP") == 0 || strcmp(t,"?") == 0)
		{
			printf ("SET{PC,A,S,X,Y} hexval    - Set Register Value\n");
			printf ("SET{N,V,B,D,I,Z,C} hexval - Set Flag Value\n");
			printf ("D [startaddr] [endaddr]   - Display Memory\n");
			printf ("M [startaddr] [hexval...] - Modify Memory\n");
			printf ("Y [startaddr] [endaddr]   - Disassemble Memory\n");
			printf ("ROM addr1 addr2           - Convert Memory Block into ROM\n");
			printf ("RAM addr1 addr2           - Convert Memory Block into RAM\n");
			printf ("CONT                      - Continue\n");
			printf ("SHOW                      - Show Registers\n");
			printf ("QUIT                      - Quit Emulation\n");
			printf ("HELP or ?                 - This Text\n");
		}
		else if (strcmp(t,"QUIT") == 0)
		{
			return 0;
		}
		else
		{
			printf ("Invalid command.\n");
		}
	};
}

static UWORD	addr;

unsigned int disassemble (UWORD addr1, UWORD addr2)
{
	UBYTE	instr;
	int	count;

	addr = addr1;

	count = (addr2 == 0) ? 20 : 0;

	while (addr < addr2 || count > 0)
	{
		printf ("%x\t",addr);

		instr = GetByte(addr);
		addr++;

		show_opcode (instr);
		show_operand (instr);

		printf ("\n");
		
		if (count > 0) count--;
	}

	return addr;
}

void show_opcode (UBYTE instr)
{
	switch (instr)
	{
		case 0x6d :	printf ("ADC"); break;
		case 0x2d :	printf ("AND"); break;
		case 0x0e :	printf ("ASL"); break;
		case 0x2c :	printf ("BIT"); break;
		case 0xcd :	printf ("CMP"); break;
		case 0xec :	printf ("CPX"); break;
		case 0xcc :	printf ("CPY"); break;
		case 0xce :	printf ("DEC"); break;
		case 0x4d :	printf ("EOR"); break;
		case 0xee :	printf ("INC"); break;
		case 0x4c :	printf ("JMP"); break;
		case 0x20 :	printf ("JSR"); break;
		case 0xad :	printf ("LDA"); break;
		case 0xae :	printf ("LDX"); break;
		case 0xac :	printf ("LDY"); break;
		case 0x4e :	printf ("LSR"); break;
		case 0x0d :	printf ("ORA"); break;
		case 0x2e :	printf ("ROL"); break;
		case 0x6e :	printf ("ROR"); break;
		case 0xed :	printf ("SBC"); break;
		case 0x8d :	printf ("STA"); break;
		case 0x8e :	printf ("STX"); break;
		case 0x8c :	printf ("STY"); break;
		case 0x65 :	printf ("ADC"); break;
		case 0x25 :	printf ("AND"); break;
		case 0x06 :	printf ("ASL"); break;
		case 0x24 :	printf ("BIT"); break;
		case 0xc5 :	printf ("CMP"); break;
		case 0xe4 :	printf ("CPX"); break;
		case 0xc4 :	printf ("CPY"); break;
		case 0xc6 :	printf ("DEC"); break;
		case 0x45 :	printf ("EOR"); break;
		case 0xe6 :	printf ("INC"); break;
		case 0xa5 :	printf ("LDA"); break;
		case 0xa6 :	printf ("LDX"); break;
		case 0xa4 :	printf ("LDY"); break;
		case 0x46 :	printf ("LSR"); break;
		case 0x05 :	printf ("ORA"); break;
		case 0x26 :	printf ("ROL"); break;
		case 0x66 :	printf ("ROR"); break;
		case 0xe5 :	printf ("SBC"); break;
		case 0x85 :	printf ("STA"); break;
		case 0x86 :	printf ("STX"); break;
		case 0x84 :	printf ("STY"); break;
		case 0x90 :	printf ("BCC"); break;
		case 0xb0 :	printf ("BCS"); break;
		case 0xf0 :	printf ("BEQ"); break;
		case 0x30 :	printf ("BMI"); break;
		case 0xd0 :	printf ("BNE"); break;
		case 0x10 :	printf ("BPL"); break;
		case 0x50 :	printf ("BVC"); break;
		case 0x70 :	printf ("BVS"); break;
		case 0x69 :	printf ("ADC"); break;
		case 0x29 :	printf ("AND"); break;
		case 0xc9 :	printf ("CMP"); break;
		case 0xe0 :	printf ("CPX"); break;
		case 0xc0 :	printf ("CPY"); break;
		case 0x49 :	printf ("EOR"); break;
		case 0xa9 :	printf ("LDA"); break;
		case 0xa2 :	printf ("LDX"); break;
		case 0xa0 :	printf ("LDY"); break;
		case 0x09 :	printf ("ORA"); break;
		case 0xe9 :	printf ("SBC"); break;
		case 0xff :	printf ("ESC"); break;
		case 0x7d :	printf ("ADC"); break;
		case 0x3d :	printf ("AND"); break;
		case 0x1e :	printf ("ASL"); break;
		case 0xdd :	printf ("CMP"); break;
		case 0xde :	printf ("DEC"); break;
		case 0x5d :	printf ("EOR"); break;
		case 0xfe :	printf ("INC"); break;
		case 0xbd :	printf ("LDA"); break;
		case 0xbc :	printf ("LDY"); break;
		case 0x5e :	printf ("LSR"); break;
		case 0x1d :	printf ("ORA"); break;
		case 0x3e :	printf ("ROL"); break;
		case 0x7e :	printf ("ROR"); break;
		case 0xfd :	printf ("SBC"); break;
		case 0x9d :	printf ("STA"); break;
		case 0x79 :	printf ("ADC"); break;
		case 0x39 :	printf ("AND"); break;
		case 0xd9 :	printf ("CMP"); break;
		case 0x59 :	printf ("EOR"); break;
		case 0xb9 :	printf ("LDA"); break;
		case 0xbe :	printf ("LDX"); break;
		case 0x19 :	printf ("ORA"); break;
		case 0xf9 :	printf ("SBC"); break;
		case 0x99 :	printf ("STA"); break;
		case 0x61 :	printf ("ADC"); break;
		case 0x21 :	printf ("AND"); break;
		case 0xc1 :	printf ("CMP"); break;
		case 0x41 :	printf ("EOR"); break;
		case 0xa1 :	printf ("LDA"); break;
		case 0x01 :	printf ("ORA"); break;
		case 0xe1 :	printf ("SBC"); break;
		case 0x81 :	printf ("STA"); break;
		case 0x71 :	printf ("ADC"); break;
		case 0x31 :	printf ("AND"); break;
		case 0xd1 :	printf ("CMP"); break;
		case 0x51 :	printf ("EOR"); break;
		case 0xb1 :	printf ("LDA"); break;
		case 0x11 :	printf ("ORA"); break;
		case 0xf1 :	printf ("SBC"); break;
		case 0x91 :	printf ("STA"); break;
		case 0x75 :	printf ("ADC"); break;
		case 0x35 :	printf ("AND"); break;
		case 0x16 :	printf ("ASL"); break;
		case 0xd5 :	printf ("CMP"); break;
		case 0xd6 :	printf ("DEC"); break;
		case 0x55 :	printf ("EOR"); break;
		case 0xf6 :	printf ("INC"); break;
		case 0xb5 :	printf ("LDA"); break;
		case 0xb4 :	printf ("LDY"); break;
		case 0x56 :	printf ("LSR"); break;
		case 0x15 :	printf ("ORA"); break;
		case 0x36 :	printf ("ROL"); break;
		case 0x76 :	printf ("ROR"); break;
		case 0xf5 :	printf ("SBC"); break;
		case 0x95 :	printf ("STA"); break;
		case 0x94 :	printf ("STY"); break;
		case 0xb6 :	printf ("LDX"); break;
		case 0x96 :	printf ("STX"); break;
		case 0x6c :	printf ("JMP"); break;
		
		case 0x0a :	printf ("ASL\tA"); break;
		case 0x00 :	printf ("BRK"); break;
		case 0x18 :	printf ("CLC"); break;
		case 0xd8 :	printf ("CLD"); break;
		case 0x58 :	printf ("CLI"); break;
		case 0xb8 :	printf ("CLV"); break;
		case 0xca :	printf ("DEX"); break;
		case 0x88 :	printf ("DEY"); break;
		case 0xe8 :	printf ("INX"); break;
		case 0xc8 :	printf ("INY"); break;
		case 0x4a :	printf ("LSR\tA"); break;
		case 0xea :	printf ("NOP"); break;
		case 0x48 :	printf ("PHA"); break;
		case 0x08 :	printf ("PHP"); break;
		case 0x68 :	printf ("PLA"); break;
		case 0x28 :	printf ("PLP"); break;
		case 0x2a :	printf ("ROL\tA"); break;
		case 0x6a :	printf ("ROR\tA"); break;
		case 0x40 :	printf ("RTI"); break;
		case 0x60 :	printf ("RTS"); break;
		case 0x38 :	printf ("SEC"); break;
		case 0xf8 :	printf ("SED"); break;
		case 0x78 :	printf ("SEI"); break;
		case 0xaa :	printf ("TAX"); break;
		case 0xa8 :	printf ("TAY"); break;
		case 0xba :	printf ("TSX"); break;
		case 0x8a :	printf ("TXA"); break;
		case 0x9a :	printf ("TXS"); break;
		case 0x98 :	printf ("TYA"); break;
		
		default :	printf ("*** ILLEGAL INSTRUCTION (%x) ***",instr); break;
	}
}

void show_operand (UBYTE instr)
{
	UBYTE	byte;
	UWORD	word;

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
			word = (GetByte(addr+1) << 8) | GetByte(addr);
			printf ("\t$%x",word);
			addr += 2;
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
			byte = GetByte(addr);
			addr++;
			printf ("\t$%x",byte);
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
			byte = GetByte(addr);
			addr++;
			printf ("\t$%x",addr+(SBYTE)byte);
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
			byte = GetByte(addr);
			addr++;
			printf ("\t#$%x",byte);
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
			word = (GetByte(addr+1) << 8) | GetByte(addr);
			printf ("\t%x,X",word);
			addr += 2;
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
			word = (GetByte(addr+1) << 8) | GetByte(addr);
			printf ("\t%x,Y",word);
			addr += 2;
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
			byte = GetByte(addr);
			addr++;
			printf ("\t($%x,X)",byte);
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
			byte = GetByte(addr);
			addr++;
			printf ("\t($%x),Y",byte);
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
			byte = GetByte(addr);
			addr++;
			printf ("\t$%x,X",byte);
			break;
/*
	========================
	0-Page,Y Addressing Mode
	========================
*/
		case 0xb6 :	/* LDX */
		case 0x96 :	/* STX */
			byte = GetByte(addr);
			addr++;
			printf ("\t$%x,Y",byte);
			break;
/*
	========================
	Indirect Addressing Mode
	========================
*/
		case 0x6c :	/* printf ("JMP INDIRECT at %x\n",instr_addr); */
			word = (GetByte(addr+1) << 8) | GetByte(addr);
			printf ("\t($%x)",word);
			addr += 2;
			break;
	}
}
