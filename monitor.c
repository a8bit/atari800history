#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#ifdef VMS
#include <unixio.h>
#include <file.h>
#else
#include <fcntl.h>
#endif

static char *rcsid = "$Id: monitor.c,v 1.15 1998/02/21 15:19:59 david Exp $";

#include "atari.h"
#include "cpu.h"
#include "antic.h"
#include "pia.h"
#include "gtia.h"

#define FALSE   0
#define TRUE    1

extern int count[256];
extern int cycles[256];

extern UBYTE memory[65536];

extern int rom_inserted;

#ifdef TRACE
int tron = FALSE;
#endif

unsigned int disassemble(UWORD addr1);
UWORD show_instruction(UWORD inad, int wid);

static UWORD addr = 0;

char *get_token(char *string)
{
	static char *s;
	char *t;

	if (string)
		s = string;				/* New String */

	while (*s == ' ')
		s++;					/* Skip Leading Spaces */

	if (*s) {
		t = s;					/* Start of String */
		while (*s != ' ' && *s) {	/* Locate End of String */
			s++;
		}

		if (*s == ' ') {		/* Space Terminated ? */
			*s = '\0';			/* C String Terminator */
			s++;				/* Point to Next Char */
		}
	}
	else {
		t = NULL;
	}

	return t;					/* Pointer to String */
}

int get_hex(char *string, UWORD * hexval)
{
	int ihexval;
	char *t;

	t = get_token(string);
	if (t) {
		sscanf(t, "%X", &ihexval);
		*hexval = ihexval;
		return 1;
	}
	return 0;
}

UWORD break_addr;

int monitor(void)
{
	UWORD addr;
	char s[128];
	int p;

	addr = 0;

	CPU_GetStatus();

	while (TRUE) {
		char *t;

		printf("> ");
		fflush(stdout);
		if (gets(s) == NULL) {
			printf("\n> CONT\n");
			strcpy(s, "CONT");
		}
		t = get_token(s);
		if (t == NULL) {
			continue;
		}
		for (p = 0; t[p] != 0; p++)
			if (islower(t[p]))
				s[p] = toupper(t[p]);

		if (strcmp(t, "CONT") == 0) {
			int i;

			for (i = 0; i < 256; i++)
				count[i] = 0;

			return 1;
		}
#ifdef MONITOR_BREAK
		else if (strcmp(t, "BREAK") == 0) {
			get_hex(NULL, &break_addr);
		}
		else if (strcmp(t, "HISTORY") == 0) {
			int i;
			for (i = 0; i < REMEMBER_PC_STEPS; i++)
				printf("%04x  ", remember_PC[i]);
			printf("\n");
		}
#endif
		else if (strcmp(t, "DLIST") == 0) {
			UWORD dlist = (DLISTH << 8) + DLISTL;
			UWORD addr;
			int done = FALSE;
			int nlines = 0;

			while (!done) {
				UBYTE IR;

				printf("%04x: ", dlist);

				IR = memory[dlist++];

				if (IR & 0x80)
					printf("DLI ");

				switch (IR & 0x0f) {
				case 0x00:
					printf("%d BLANK", ((IR >> 4) & 0x07) + 1);
					break;
				case 0x01:
					addr = memory[dlist] | (memory[dlist + 1] << 8);
					if (IR & 0x40) {
						printf("JVB %04x ", addr);
						dlist += 2;
						done = TRUE;
					}
					else {
						printf("JMP %04x ", addr);
						dlist = addr;
					}
					break;
				default:
					if (IR & 0x40) {
						addr = memory[dlist] | (memory[dlist + 1] << 8);
						dlist += 2;
						printf("LMS %04x ", addr);
					}
					if (IR & 0x20)
						printf("VSCROL ");

					if (IR & 0x10)
						printf("HSCROL ");

					printf("MODE %X ", IR & 0x0f);
				}

				printf("\n");
				nlines++;

				if (nlines == 15) {
					char gash[4];

					printf("Press return to continue: ");
					gets(gash);
					nlines = 0;
				}
			}
		}
		else if (strcmp(t, "SETPC") == 0) {
			get_hex(NULL, &addr);

			regPC = addr;
		}
		else if (strcmp(t, "SETS") == 0) {
			get_hex(NULL, &addr);
			regS = addr & 0xff;
		}
		else if (strcmp(t, "SETA") == 0) {
			get_hex(NULL, &addr);
			regA = addr & 0xff;
		}
		else if (strcmp(t, "SETX") == 0) {
			get_hex(NULL, &addr);
			regX = addr & 0xff;
		}
		else if (strcmp(t, "SETY") == 0) {
			get_hex(NULL, &addr);
			regY = addr & 0xff;
		}
		else if (strcmp(t, "SETN") == 0) {
			get_hex(NULL, &addr);
			if (addr)
				SetN;
			else
				ClrN;
		}
		else if (strcmp(t, "SETV") == 0) {
			get_hex(NULL, &addr);
			if (addr)
				SetV;
			else
				ClrV;
		}
		else if (strcmp(t, "SETB") == 0) {
			get_hex(NULL, &addr);
			if (addr)
				SetB;
			else
				ClrB;
		}
		else if (strcmp(t, "SETD") == 0) {
			get_hex(NULL, &addr);
			if (addr)
				SetD;
			else
				ClrD;
		}
		else if (strcmp(t, "SETI") == 0) {
			get_hex(NULL, &addr);
			if (addr)
				SetI;
			else
				ClrI;
		}
		else if (strcmp(t, "SETZ") == 0) {
			get_hex(NULL, &addr);
			if (addr)
				SetZ;
			else
				ClrZ;
		}
		else if (strcmp(t, "SETC") == 0) {
			get_hex(NULL, &addr);
			if (addr)
				SetC;
			else
				ClrC;
		}
#ifdef TRACE
		else if (strcmp(t, "TRON") == 0)
			tron = TRUE;
		else if (strcmp(t, "TROFF") == 0)
			tron = FALSE;
#endif
		else if (strcmp(t, "PROFILE") == 0) {
			int i;

			for (i = 0; i < 10; i++) {
				int max, instr;
				int j;

				max = count[0];
				instr = 0;

				for (j = 1; j < 256; j++) {
					if (count[j] > max) {
						max = count[j];
						instr = j;
					}
				}

				if (max > 0) {
					count[instr] = 0;
					printf("%02x has been executed %d times\n",
						   instr, max);
				}
				else {
					printf("Instruction Profile data not available\n");
					break;
				}
			}
		}
		else if (strcmp(t, "SHOW") == 0) {
			printf("PC=%04x, A=%02x, S=%02x, X=%02x, Y=%02x, P=%02x\n",
				   regPC,
				   regA,
				   regS,
				   regX,
				   regY,
				   regP);
		}
		else if (strcmp(t, "ROM") == 0) {
			UWORD addr1;
			UWORD addr2;

			int status;

			status = get_hex(NULL, &addr1);
			status |= get_hex(NULL, &addr2);

			if (status) {
				SetROM(addr1, addr2);
				printf("Changed Memory from %4x to %4x into ROM\n",
					   addr1, addr2);
			}
			else {
				printf("*** Memory Unchanged (Missing Parameter) ***\n");
			}
		}
		else if (strcmp(t, "RAM") == 0) {
			UWORD addr1;
			UWORD addr2;

			int status;

			status = get_hex(NULL, &addr1);
			status |= get_hex(NULL, &addr2);

			if (status) {
				SetRAM(addr1, addr2);
				printf("Changed Memory from %4x to %4x into RAM\n",
					   addr1, addr2);
			}
			else {
				printf("*** Memory Unchanged (Missing Parameter) ***\n");
			}
		}
		else if (strcmp(t, "HARDWARE") == 0) {
			UWORD addr1;
			UWORD addr2;

			int status;

			status = get_hex(NULL, &addr1);
			status |= get_hex(NULL, &addr2);

			if (status) {
				SetHARDWARE(addr1, addr2);
				printf("Changed Memory from %4x to %4x into HARDWARE\n",
					   addr1, addr2);
			}
			else {
				printf("*** Memory Unchanged (Missing Parameter) ***\n");
			}
		}
		else if (strcmp(t, "COLDSTART") == 0) {
			Coldstart();
		}
		else if (strcmp(t, "WARMSTART") == 0) {
			Warmstart();
		}
		else if (strcmp(t, "READ") == 0) {
			char *filename;
			UWORD addr;
			UWORD nbytes;
			int status;

			filename = get_token(NULL);
			if (filename) {
				status = get_hex(NULL, &addr);
				if (status) {
					status = get_hex(NULL, &nbytes);
					if (status) {
						int fd;

						fd = open(filename, O_RDONLY | O_BINARY);
						if (fd == -1) {
							perror(filename);
							Atari800_Exit(FALSE);
							exit(1);
						}
						if (read(fd, &memory[addr], nbytes) == -1) {
							perror("read");
							Atari800_Exit(FALSE);
							exit(1);
						}
						close(fd);
					}
				}
			}
		}
		else if (strcmp(t, "WRITE") == 0) {
			UWORD addr1;
			UWORD addr2;

			int status;

			status = get_hex(NULL, &addr1);
			status |= get_hex(NULL, &addr2);

			if (status) {
				int fd;

				fd = open("memdump.dat", O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0777);
				if (fd == -1) {
					perror("open");
					Atari800_Exit(FALSE);
					exit(1);
				}
				write(fd, &memory[addr1], addr2 - addr1 + 1);

				close(fd);
			}
		}
		else if (strcmp(t, "SUM") == 0) {
			UWORD addr1;
			UWORD addr2;
			int status;

			status = get_hex(NULL, &addr1);
			status |= get_hex(NULL, &addr2);

			if (status) {
				int sum = 0;
				int i;

				for (i = addr1; i <= addr2; i++)
					sum += (UWORD) memory[i];
				printf("SUM: %X\n", sum);
			}
		}
		else if (strcmp(t, "M") == 0) {
			UWORD xaddr1;
			int i;
			int count;
			if (get_hex(NULL, &xaddr1))
				addr = xaddr1;
			count = 16;
			while (count) {
				printf("%04X : ", addr);
				for (i = 0; i < 16; i++)
					printf("%02X ", memory[(UWORD) (addr + i)]);
				printf("\t");
				for (i = 0; i < 16; i++) {
					if (isalnum(memory[(UWORD) (addr + i)])) {
						printf("%c", memory[(UWORD) (addr + i)]);
					}
					else {
						printf(".");
					}
				}
				printf("\n");
				addr += 16;
				count--;
			}
		}
		else if (strcmp(t, "F") == 0) {
			int addr;
			int addr1;
			int addr2;
			UWORD xaddr1;
			UWORD xaddr2;
			UWORD hexval;

			get_hex(NULL, &xaddr1);
			get_hex(NULL, &xaddr2);
			get_hex(NULL, &hexval);

			addr1 = xaddr1;
			addr2 = xaddr2;

			for (addr = addr1; addr <= addr2; addr++)
				memory[addr] = (UBYTE) (hexval & 0x00ff);
		}
		else if (strcmp(t, "S") == 0) {
			int addr;
			int addr1;
			int addr2;
			UWORD xaddr1;
			UWORD xaddr2;
			UWORD hexval;

			get_hex(NULL, &xaddr1);
			get_hex(NULL, &xaddr2);
			get_hex(NULL, &hexval);

			addr1 = xaddr1;
			addr2 = xaddr2;

			for (addr = addr1; addr <= addr2; addr++)
				if (memory[addr] == (UBYTE) (hexval & 0x00ff)) {
					if (hexval & 0xff00)
						if (memory[addr + 1] != (UBYTE) (hexval >> 8))
							continue;
					printf("Found at %04x\n", addr);
				}
		}
		else if (strcmp(t, "C") == 0) {
			UWORD addr;
			UWORD temp;
			addr = 0;
			temp = 0;

			get_hex(NULL, &addr);

			while (get_hex(NULL, &temp)) {
				memory[addr] = (UBYTE) temp;
				addr++;
			}
		}
		else if (strcmp(t, "D") == 0) {
			UWORD addr1;
			addr1 = addr;
			get_hex(NULL, &addr1);
			addr = disassemble(addr1);
		}
		else if (strcmp(t, "ANTIC") == 0) {
			printf("DMACTL=%02x    CHACTL=%02x    DLISTL=%02x    "
				   "DLISTH=%02x    HSCROL=%02x    VSCROL=%02x\n",
				   DMACTL, CHACTL, DLISTL, DLISTH, HSCROL, VSCROL);
			printf("PMBASE=%02x    CHBASE=%02x    WSYNC= xx    "
				   "VCOUNT=%02x    ypos=%3d\n",
				   PMBASE, CHBASE, (ypos + 8) >> 1, ypos);
			printf("NMIEN= %02x    NMIRES=xx\n", NMIEN);
		}
		else if (strcmp(t, "PIA") == 0) {
			printf("PACTL=%02x      PBCTL=%02x     PORTA=%02x     "
				   "PORTB=%02x   ROM inserted: %s\n", PACTL, PBCTL, PORTA, PORTB, rom_inserted ? "Yes" : "No");
		}
		else if (strcmp(t, "GTIA") == 0) {
			printf("HPOSP0=%02x    HPOSP1=%02x    HPOSP2=%02x    "
				   "HPOSP3=%02x    HPOSM0=%02x    HPOSM1=%02x\n",
				   HPOSP0, HPOSP1, HPOSP2, HPOSP3, HPOSM0, HPOSM1);
			printf("HPOSM2=%02x    HPOSM3=%02x    SIZEP0=%02x    "
				   "SIZEP1=%02x    SIZEP2=%02x    SIZEP3=%02x\n",
				   HPOSM2, HPOSM3, SIZEP0, SIZEP1, SIZEP2, SIZEP3);
			printf("SIZEM= %02x    GRAFP0=%02x    GRAFP1=%02x    "
				   "GRAFP2=%02x    GRAFP3=%02x    GRAFM =%02x\n",
				   SIZEM, GRAFP0, GRAFP1, GRAFP2, GRAFP3, GRAFM);
			printf("COLPM0=%02x    COLPM1=%02x    COLPM2=%02x    "
				   "COLPM3=%02x    COLPF0=%02x    COLPF1=%02x\n",
				   COLPM0, COLPM1, COLPM2, COLPM3, COLPF0, COLPF1);
			printf("COLPF2=%02x    COLPF3=%02x    COLBK= %02x    "
				   "PRIOR= %02x    GRACTL=%02x\n",
				   COLPF2, COLPF3, COLBK, PRIOR, GRACTL);
		}
		else if (strcmp(t, "HELP") == 0 || strcmp(t, "?") == 0) {
			printf("SET{PC,A,S,X,Y} hexval    - Set Register Value\n");
			printf("SET{N,V,B,D,I,Z,C} hexval - Set Flag Value\n");
			printf("C [startaddr] [hexval...] - Change memory\n");
			printf("D [startaddr]             - Disassemble memory\n");
			printf("F [startaddr] [endaddr] hexval - Fill memory\n");
			printf("M [startaddr]             - Memory list\n");
			printf("S [startaddr] [endaddr] hexval - Search memory\n");
			printf("ROM addr1 addr2           - Convert Memory Block into ROM\n");
			printf("RAM addr1 addr2           - Convert Memory Block into RAM\n");
			printf("HARDWARE addr1 addr2      - Convert Memory Block into HARDWARE\n");
			printf("CONT                      - Continue\n");
			printf("SHOW                      - Show Registers\n");
			printf("READ filename addr nbytes - Read file into memory\n");
			printf("WRITE startaddr endaddr   - Write specified area of memory to memdump.dat\n");
			printf("SUM [startaddr] [endaddr] - SUM of specified memory range\n");
#ifdef TRACE
			printf("TRON                      - Trace On\n");
			printf("TROFF                     - Trace Off\n");
#endif
#ifdef MONITOR_BREAK
			printf("BREAK [addr]              - Set breakpoint at address\n");
			printf("HISTORY                   - List last 16 PC addresses\n");
#endif
			printf("ANTIC                     - Display ANTIC registers\n");
			printf("GTIA                      - Display GTIA registers\n");
			printf("DLIST                     - Display current display list\n");
			printf("PROFILE                   - Display profiling statistics\n");
			printf("COLDSTART                 - Perform system coldstart\n");
			printf("WARMSTART                 - Perform system warmstart\n");
			printf("QUIT                      - Quit Emulation\n");
			printf("HELP or ?                 - This Text\n");
		}
		else if (strcmp(t, "QUIT") == 0) {
			return 0;
		}
		else {
			printf("Invalid command.\n");
		}
	}
}

unsigned int disassemble(UWORD addr1)
{
	UWORD i;
	int count;

	addr = addr1;
	count = 24;

	while (count) {
		printf("%04X\t", addr);
		addr1 = show_instruction(addr, 20);
		printf("; %Xcyc ; ", cycles[memory[addr]]);
		for (i = 0; i < addr1; i++)
			printf("%02X ", memory[(UWORD) (addr + i)]);
		printf("\n");
		addr += addr1;
		count--;
	}
	return addr;
}

static char *instr6502[256] =
{
	"BRK", "ORA ($1,X)", "CIM", "ASO ($1,X)", "SKB", "ORA $1", "ASL $1", "ASO $1",
 "PHP", "ORA #$1", "ASL", "ASO #$1", "SKW", "ORA $2", "ASL $2", "ASO $2",

	"BPL $0", "ORA ($1),Y", "CIM", "ASO ($1),Y", "SKB", "ORA $1,X", "ASL $1,X", "ASO $1,X",
	"CLC", "ORA $2,Y", "NOP", "ASO $2,Y", "SKW", "ORA $2,X", "ASL $2,X", "ASO $2,X",

	"JSR $2", "AND ($1,X)", "CIM", "RLA ($1,X)", "BIT $1", "AND $1", "ROL $1", "RLA $1",
	"PLP", "AND #$1", "ROL", "RLA #$1", "BIT $2", "AND $2", "ROL $2", "RLA $2",

	"BMI $0", "AND ($1),Y", "CIM", "RLA ($1),Y", "SKB", "AND $1,X", "ROL $1,X", "RLA $1,X",
	"SEC", "AND $2,Y", "NOP", "RLA $2,Y", "SKW", "AND $2,X", "ROL $2,X", "RLA $2,X",


	"RTI", "EOR ($1,X)", "CIM", "LSE ($1,X)", "SKB", "EOR $1", "LSR $1", "LSE $1",
	"PHA", "EOR #$1", "LSR", "ALR #$1", "JMP $2", "EOR $2", "LSR $2", "LSE $2",

	"BVC $0", "EOR ($1),Y", "CIM", "LSE ($1),Y", "SKB", "EOR $1,X", "LSR $1,X", "LSE $1,X",
	"CLI", "EOR $2,Y", "NOP", "LSE $2,Y", "SKW", "EOR $2,X", "LSR $2,X", "LSE $2,X",

	"RTS", "ADC ($1,X)", "CIM", "RRA ($1,X)", "SKB", "ADC $1", "ROR $1", "RRA $1",
	"PLA", "ADC #$1", "ROR", "ARR #$1", "JMP ($2)", "ADC $2", "ROR $2", "RRA $2",

	"BVS $0", "ADC ($1),Y", "CIM", "RRA ($1),Y", "SKB", "ADC $1,X", "ROR $1,X", "RRA $1,X",
	"SEI", "ADC $2,Y", "NOP", "RRA $2,Y", "SKW", "ADC $2,X", "ROR $2,X", "RRA $2,X",


	"SKB", "STA ($1,X)", "SKB", "AXS ($1,X)", "STY $1", "STA $1", "STX $1", "AXS $1",
  "DEY", "SKB", "TXA", "XAA #$1", "STY $2", "STA $2", "STX $2", "AXS $2",

	"BCC $0", "STA ($1),Y", "CIM", "AXS ($1),Y", "STY $1,X", "STA $1,X", "STX $1,Y", "AXS $1,Y",
	"TYA", "STA $2,Y", "TXS", "XAA $2,Y", "SKW", "STA $2,X", "MKX $2", "MKA $2",

	"LDY #$1", "LDA ($1,X)", "LDX #$1", "LAX ($1,X)", "LDY $1", "LDA $1", "LDX $1", "LAX $1",
	"TAY", "LDA #$1", "TAX", "OAL #$1", "LDY $2", "LDA $2", "LDX $2", "LAX $2",

	"BCS $0", "LDA ($1),Y", "CIM", "LAX ($1),Y", "LDY $1,X", "LDA $1,X", "LDX $1,Y", "LAX $1,X",
	"CLV", "LDA $2,Y", "TSX", "AXA $2,Y", "LDY $2,X", "LDA $2,X", "LDX $2,Y", "LAX $2,Y",


	"CPY #$1", "CMP ($1,X)", "SKB", "DCM ($1,X)", "CPY $1", "CMP $1", "DEC $1", "DCM $1",
	"INY", "CMP #$1", "DEX", "SAX #$1", "CPY $2", "CMP $2", "DEC $2", "DCM $2",

	"BNE $0", "CMP ($1),Y", "CIM      [ESCRTS]", "DCM ($1),Y", "SKB", "CMP $1,X", "DEC $1,X", "DCM $1,X",
	"CLD", "CMP $2,Y", "NOP", "DCM $2,Y", "SKW", "CMP $2,X", "DEC $2,X", "DCM $2,X",


	"CPX #$1", "SBC ($1,X)", "SKB", "INS ($1,X)", "CPX $1", "SBC $1", "INC $1", "INS $1",
	"INX", "SBC #$1", "NOP", "SBC #$1", "CPX $2", "SBC $2", "INC $2", "INS $2",

	"BEQ $0", "SBC ($1),Y", "CIM      [ESC]", "INS ($1),Y", "SKB", "SBC $1,X", "INC $1,X", "INS $1,X",
	"SED", "SBC $2,Y", "NOP", "INS $2,Y", "SKW", "SBC $2,X", "INC $2,X", "INS $2,X"
};

UWORD show_instruction(UWORD inad, int wid)
{
	UBYTE instr;
	UWORD value;
	char dissbf[32];
	int i;

	instr = memory[inad];
	strcpy(dissbf, instr6502[instr]);

	for (i = 0; dissbf[i] != 0; i++) {
		if (dissbf[i] == '$') {
			wid -= i;
			dissbf[i] = 0;
			printf(dissbf);
			switch (dissbf[i + 1]) {
			case '0':
				value = (UWORD) (inad + (char) memory[(UWORD) (inad + 1)] + 2);
				inad = 2;
				wid -= 5;
				printf("$%04X", value);
				break;
			case '1':
				value = (UBYTE) memory[(UWORD) (inad + 1)];
				inad = 2;
				wid -= 3;
				printf("$%02X", value);
				break;
			case '2':
				value = (UWORD) memory[(UWORD) (inad + 1)] | (memory[(UWORD) (inad + 2)] << 8);
				inad = 3;
				wid -= 5;
				printf("$%04X", value);
				break;
			}
			printf(dissbf + i + 2);
			for (; dissbf[i + 2] != 0; i++)
				wid--;
			i = 0;
			break;
		}
	}
	if (dissbf[i] == 0) {
		printf(dissbf);
		wid -= i;
		inad = 1;
	}
	for (i = wid; i > 0; i--)
		printf(" ");
	return inad;
}
