#include <stdio.h>
#include <ctype.h>

#ifdef VMS
#include <unixio.h>
#include <file.h>
#else
#include <fcntl.h>
#endif

static char *rcsid = "$Id: monitor.c,v 1.10 1996/09/23 21:26:22 david Exp $";

#include "atari.h"
#include "cpu.h"
#include "antic.h"
#include "gtia.h"

#define FALSE   0
#define TRUE    1

extern int count[256];

extern UBYTE	memory[65536];

#ifdef TRACE
int tron = FALSE;
#endif

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

int monitor (void)
{
  UWORD	addr;
  UWORD	break_addr;
  char	s[128];
  int     p;

  addr = 0;

  CPU_GetStatus ();

  while (TRUE)
    {
      char    *t;

      printf ("> ");
      if (gets (s) == NULL)
        {
	  printf("\n> CONT\n");
	  strcpy(s, "CONT");
	}

      for (p=0;s[p]!=0;p++)
	if (islower(s[p]))
	  s[p] = toupper(s[p]);

      t = get_token(s);
      if (t == NULL)
        {
	  continue;
        }

      if (strcmp(t,"BREAK") == 0)
	{
	  get_hex (NULL, &break_addr);
	}
      else if (strcmp(t,"CONT") == 0)
	{
	  int i;

	  for (i=0;i<256;i++)
	    count[i] = 0;

	  return 1;
	}
      else if (strcmp(t,"DLIST") == 0)
	{
	  UWORD dlist = (DLISTH << 8) + DLISTL;
	  UWORD addr;
	  int done = FALSE;
	  int nlines = 0;

	  while (!done)
	    {
	      UBYTE IR;

	      printf ("%04x: ", dlist);

	      IR = memory[dlist++];

	      if (IR & 0x80)
		printf ("DLI ");

	      switch (IR & 0x0f)
		{
		case 0x00 :
		  printf ("%d BLANK", ((IR >> 4) & 0x07) + 1);
		  break;
		case 0x01 :
		  addr = memory[dlist] | (memory[dlist+1] << 8);
		  if (IR & 0x40)
		    {
		      printf ("JVB %04x ", addr);
		      dlist += 2;
		      done = TRUE;
		    }
		  else
		    {
		      printf ("JMP %04x ", addr);
		      dlist = addr;
		    }
		  break;
		default :
		  if (IR & 0x40)
		    {
		      addr = memory[dlist] | (memory[dlist+1] << 8);
		      dlist += 2;
		      printf ("LMS %04x ", addr);
		    }

		  if (IR & 0x20)
		    printf ("VSCROL ");

		  if (IR & 0x10)
		    printf ("HSCROL ");

		  printf ("MODE %x ", IR & 0x0f);
		}

	      printf ("\n");
	      nlines++;

	      if (nlines == 15)
		{
		  char gash[4];

		  printf ("Press return to continue: ");
		  gets (gash);
		  nlines = 0;
		}
	    }
	}
      else if (strcmp(t,"SETPC") == 0)
	{
	  get_hex (NULL, &addr);
	  
	  regPC = addr;
	}
      else if (strcmp(t,"SETS") == 0)
	{
	  get_hex (NULL, &addr);
	  regS = addr & 0xff;
	}
      else if (strcmp(t,"SETA") == 0)
	{
	  get_hex (NULL, &addr);
	  regA = addr & 0xff;
	}
      else if (strcmp(t,"SETX") == 0)
	{
	  get_hex (NULL, &addr);
	  regX = addr & 0xff;
	}
      else if (strcmp(t,"SETY") == 0)
	{
	  get_hex (NULL, &addr);
	  regY = addr & 0xff;
	}
      else if (strcmp(t,"SETN") == 0)
	{
	  get_hex (NULL, &addr);
	  if (addr)
	    SetN;
	  else
	    ClrN;
	}
      else if (strcmp(t,"SETV") == 0)
	{
	  get_hex (NULL, &addr);
	  if (addr)
	    SetV;
	  else
	    ClrV;
	}
      else if (strcmp(t,"SETB") == 0)
	{
	  get_hex (NULL, &addr);
	  if (addr)
	    SetB;
	  else
	    ClrB;
	}
      else if (strcmp(t,"SETD") == 0)
	{
	  get_hex (NULL, &addr);
	  if (addr)
	    SetD;
	  else
	    ClrD;
	}
      else if (strcmp(t,"SETI") == 0)
	{
	  get_hex (NULL, &addr);
	  if (addr)
	    SetI;
	  else
	    ClrI;
	}
      else if (strcmp(t,"SETZ") == 0)
	{
	  get_hex (NULL, &addr);
	  if (addr)
	    SetZ;
	  else
	    ClrZ;
	}
      else if (strcmp(t,"SETC") == 0)
	{
	  get_hex (NULL, &addr);
	  if (addr)
	    SetC;
	  else
	    ClrC;
	}
#ifdef TRACE
      else if (strcmp(t,"TRON") == 0)
	tron = TRUE;
      else if (strcmp(t,"TROFF") == 0)
	tron = FALSE;
#endif
      else if (strcmp(t,"PROFILE") == 0)
	{
	  int i;

	  for (i=0;i<10;i++)
	    {
	      int max, instr;
	      int j;

	      max = count[0];
	      instr = 0;

	      for (j=1;j<256;j++)
		{
		  if (count[j] > max)
		    {
		      max = count[j];
		      instr = j;
		    }
		}

	      if (max > 0)
		{
		  count[instr] = 0;
		  printf ("%02x has been executed %d times\n",
			  instr, max);
		}
	      else
		{
		  printf ("Instruction Profile data not available\n");
		  break;
		}
	    }
	}
      else if (strcmp(t,"SHOW") == 0)
	{
	  printf ("PC=%04x, A=%02x, S=%02x, X=%02x, Y=%02x, P=%02x\n",
		  regPC,
		  regA,
		  regS,
		  regX,
		  regY,
		  regP);
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
	      printf ("Changed Memory from %4x to %4x into ROM\n",
		      addr1, addr2);
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
	      printf ("Changed Memory from %4x to %4x into RAM\n",
		      addr1, addr2);
	    }
	  else
	    {
	      printf ("*** Memory Unchanged (Missing Parameter) ***\n");
	    }
	}
      else if (strcmp(t,"HARDWARE") == 0)
	{
	  UWORD	addr1;
	  UWORD	addr2;

	  int	status;

	  status = get_hex (NULL, &addr1);
	  status |= get_hex (NULL, &addr2);

	  if (status)
	    {
	      SetHARDWARE (addr1, addr2);
	      printf ("Changed Memory from %4x to %4x into HARDWARE\n",
		      addr1, addr2);
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
		  printf ("%2x ",memory[temp]);
		  temp++;
		  if (temp > addr2) break;
		}

	      temp = addr1;

	      printf ("\t");

	      for (i=0;i<16;i++)
		{
		  if (isalnum(memory[temp]))
		    {
		      printf ("%c",memory[temp]);
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
	      memory[addr] = (UBYTE)temp;
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
      else if (strcmp(t,"ANTIC") == 0)
	{
	  printf ("DMACTL=%02x    CHACTL=%02x    DLISTL=%02x    "
		  "DLISTH=%02x    HSCROL=%02x    VSCROL=%02x\n",
		  DMACTL, CHACTL, DLISTL, DLISTH, HSCROL, VSCROL);
	  printf ("PMBASE=%02x    CHBASE=%02x    WSYNC= xx    "
		  "VCOUNT=%02x    PENH=  xx    PENV=  xx\n",
		  PMBASE, CHBASE, (ypos + 8) >> 1);
	  printf ("NMIEN= %02x    NMIRES=xx\n", NMIEN);
	}
      else if (strcmp(t,"GTIA") == 0)
	{
	  printf ("HPOSP0=%02x    HPOSP1=%02x    HPOSP2=%02x    "
		  "HPOSP3=%02x    HPOSM0=%02x    HPOSM1=%02x\n",
		  HPOSP0, HPOSP1, HPOSP2, HPOSP3, HPOSM0, HPOSM1);
	  printf ("HPOSM2=%02x    HPOSM3=%02x    SIZEP0=%02x    "
		  "SIZEP1=%02x    SIZEP2=%02x    SIZEP3=%02x\n",
		  HPOSM2, HPOSM3, SIZEP0, SIZEP1, SIZEP2, SIZEP3);
	  printf ("SIZEM= %02x    GRAFP0=%02x    GRAFP1=%02x    "
		  "GRAFP2=%02x    GRAFP3=%02x    GRAFM =%02x\n",
		  SIZEM, GRAFP0, GRAFP1, GRAFP2, GRAFP3, GRAFM);
	  printf ("COLPM0=%02x    COLPM1=%02x    COLPM2=%02x    "
		  "COLPM3=%02x    COLPF0=%02x    COLPF1=%02x\n",
		  COLPM0, COLPM1, COLPM2, COLPM3, COLPF0, COLPF1);
	  printf ("COLPF2=%02x    COLPF3=%02x    COLBK= %02x    "
		  "PRIOR= %02x    GRACTL=%02x\n",
		  COLPF2, COLPF3, COLBK, PRIOR, GRACTL);
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
	  printf ("HARDWARE addr1 addr2      - Convert Memory Block into HARDWARE\n");
	  printf ("CONT                      - Continue\n");
	  printf ("SHOW                      - Show Registers\n");
	  printf ("WRITE startaddr endaddr   - Write specified area of memory to memdump.dat\n");
	  printf ("SUM [startaddr] [endaddr] - SUM of specified memory range\n");
#ifdef TRACE
	  printf ("TRON                      - Trace On\n");
	  printf ("TROFF                     - Trace Off\n");
#endif
          printf ("ANTIC                     - Display ANTIC registers\n");
          printf ("GTIA                      - Display GTIA registers\n");
          printf ("DLIST                     - Display current display list\n");
          printf ("PROFILE                   - Display profiling statistics\n");
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
    }
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

		instr = memory[addr];
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
	case 0x6d : case 0x65 : case 0x69:
	case 0x79 : case 0x7d : case 0x61 :
	case 0x71 : case 0x75 :
	  printf ("ADC");
	  break;
	case 0x2d : case 0x25 : case 0x29 :
	case 0x39 : case 0x3d : case 0x21 :
	case 0x31 : case 0x35 :
	  printf ("AND");
	  break;
	case 0x0e : case 0x06 : case 0x1e :
	case 0x16 :
	  printf ("ASL");
	  break;
	case 0x0a :
	  printf ("ASL\tA");
	  break;
	case 0x90 :
	  printf ("BCC");
	  break;
	case 0xb0 :
	  printf ("BCS");
	  break;
	case 0xf0 :
	  printf ("BEQ");
	  break;
	case 0x2c : case 0x24 :
	  printf ("BIT");
	  break;
	case 0x30 :
	  printf ("BMI");
	  break;
	case 0xd0 :
	  printf ("BNE");
	  break;
	case 0x10 :
	  printf ("BPL");
	  break;
	case 0x00 :
	  printf ("BRK");
	  break;
	case 0x50 :
	  printf ("BVC");
	  break;
	case 0x70 :
	  printf ("BVS");
	  break;
	case 0x18 :
	  printf ("CLC");
	  break;
	case 0xd8 :
	  printf ("CLD");
	  break;
	case 0x58 :
	  printf ("CLI");
	  break;
	case 0xb8 :
	  printf ("CLV");
	  break;
	case 0xcd : case 0xc5 : case 0xc9 :
	case 0xdd : case 0xd9 :	case 0xc1 :
	case 0xd1 : case 0xd5 :
	  printf ("CMP");
	  break;
	case 0xec : case 0xe4 : case 0xe0 :
	  printf ("CPX");
	  break;
	case 0xcc : case 0xc4 : case 0xc0 :
	  printf ("CPY");
	  break;
	case 0xce : case 0xc6 : case 0xde :
	case 0xd6 :
	  printf ("DEC");
	  break;
	case 0xca :
	  printf ("DEX");
	  break;
	case 0x88 :
	  printf ("DEY");
	  break;
	case 0x4d : case 0x45 : case 0x49 :
	case 0x5d : case 0x59 : case 0x41 :
	case 0x51 : case 0x55 :
	  printf ("EOR");
	  break;
	case 0xff : /* [unofficial] */
	  printf ("ESC");
	  break;
	case 0xee : case 0xe6 : case 0xfe :
	case 0xf6 :
	  printf ("INC");
	  break;
	case 0xe8 :
	  printf ("INX");
	  break;
	case 0xc8 :
	  printf ("INY");
	  break;
	case 0x4c : case 0x6c :
	  printf ("JMP");
	  break;
	case 0x20 :
	  printf ("JSR");
	  break;
	case 0xa3 : case 0xa7 : case 0xaf : /* [unofficial] */
	case 0xb3 : case 0xb7 : case 0xbf :
	  printf ("LAX");
	  break;
	case 0xad : case 0xa5 : case 0xa9 :
	case 0xbd : case 0xb9 : case 0xa1 :
	case 0xb1 : case 0xb5 :
	  printf ("LDA");
	  break;
	case 0xae : case 0xa6 : case 0xa2 :
	case 0xbe : case 0xb6 :
	  printf ("LDX");
	  break;
	case 0xac : case 0xa4 : case 0xa0 :
	case 0xbc : case 0xb4 :
	  printf ("LDY");
	  break;
	case 0x4e : case 0x46 : case 0x5e :
	case 0x56 :
	  printf ("LSR");
	  break;
	case 0x4a :
	  printf ("LSR\tA");
	  break;
	case 0xea :
	  printf ("NOP");
	  break;
	case 0x0d : case 0x05 :	case 0x09 :
	case 0x1d : case 0x19 :	case 0x01 :
	case 0x11 : case 0x15 :
	  printf ("ORA");
	  break;
	case 0x48 :
	  printf ("PHA");
	  break;
	case 0x08 :
	  printf ("PHP");
	  break;
	case 0x68 :
	  printf ("PLA");
	  break;
	case 0x28 :
	  printf ("PLP");
	  break;
	case 0x2e : case 0x26 : case 0x3e :
	case 0x36 :
	  printf ("ROL");
	  break;
	case 0x2a :
	  printf ("ROL\tA");
	  break;
	case 0x6e : case 0x66 : case 0x7e :
	case 0x76 :
	  printf ("ROR");
	  break;
	case 0x6a :
	  printf ("ROR\tA");
	  break;
	case 0x40 :
	  printf ("RTI");
	  break;
	case 0x60 :
	  printf ("RTS");
	  break;
	case 0xed : case 0xe5 : case 0xe9 :
	case 0xfd : case 0xf9 :	case 0xe1 :
	case 0xf1 : case 0xf5 :
	  printf ("SBC");
	  break;
	case 0x38 :
	  printf ("SEC");
	  break;
	case 0xf8 :
	  printf ("SED");
	  break;
	case 0x78 :
	  printf ("SEI");
	  break;
	case 0x8d : case 0x85 :	case 0x9d :
	case 0x99 : case 0x81 :	case 0x91 :
	case 0x95 :
	  printf ("STA");
	  break;
	case 0x8e : case 0x86 : case 0x96 :
	  printf ("STX");
	  break;
	case 0x8c : case 0x84 : case 0x94 :
	  printf ("STY");
	  break;
	case 0xaa :
	  printf ("TAX");
	  break;
	case 0xa8 :
	  printf ("TAY");
	  break;
	case 0xba :
	  printf ("TSX");
	  break;
	case 0x8a :
	  printf ("TXA");
	  break;
	case 0x9a :
	  printf ("TXS");
	  break;
	case 0x98 :
	  printf ("TYA");
	  break;
	default :
	  printf ("*** ILLEGAL INSTRUCTION (%x) ***",instr);
	  break;
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
		case 0xaf :     /* LAX [unofficial] */
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
			word = (memory[addr+1] << 8) | memory[addr];
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
		case 0xa7 :     /* LAX [unofficial] */
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
			byte = memory[addr];
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
			byte = memory[addr];
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
			byte = memory[addr];
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
			word = (memory[addr+1] << 8) | memory[addr];
			printf ("\t$%x,X",word);
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
		case 0xbf :     /* LAX [unofficial] */
		case 0xb9 :	/* LDA */
		case 0xbe :	/* LDX */
		case 0x19 :	/* ORA */
		case 0xf9 :	/* SBC */
		case 0x99 :	/* STA */
			word = (memory[addr+1] << 8) | memory[addr];
			printf ("\t$%x,Y",word);
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
		case 0xa3 :     /* LAX [unofficial] */
		case 0xa1 :	/* LDA */
		case 0x01 :	/* ORA */
		case 0xe1 :	/* SBC */
		case 0x81 :	/* STA */
			byte = memory[addr];
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
		case 0xb3 :     /* LAX [unofficial] */
		case 0xb1 :	/* LDA */
		case 0x11 :	/* ORA */
		case 0xf1 :	/* SBC */
		case 0x91 :	/* STA */
			byte = memory[addr];
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
			byte = memory[addr];
			addr++;
			printf ("\t$%x,X",byte);
			break;
/*
	========================
	0-Page,Y Addressing Mode
	========================
*/
		case 0xb7 :     /* LAX [unofficial] */
		case 0xb6 :	/* LDX */
		case 0x96 :	/* STX */
			byte = memory[addr];
			addr++;
			printf ("\t$%x,Y",byte);
			break;
/*
	========================
	Indirect Addressing Mode
	========================
*/
		case 0x6c :	/* printf ("JMP INDIRECT at %x\n",instr_addr); */
			word = (memory[addr+1] << 8) | memory[addr];
			printf ("\t($%x)",word);
			addr += 2;
			break;
	}
}
