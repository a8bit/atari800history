#ifndef __SYSTEM_INCLUDED__
#define	__SYSTEM_INCLUDED__

/*
	=================================
	Define Data Types on Local System
	=================================
*/

#define	SBYTE	signed char
#define	SWORD	signed short int
#define	SLONG	signed long int
#define	UBYTE	unsigned char
#define	UWORD	unsigned short int
#define	ULONG	unsigned long int

typedef enum
{
	Atari,
	AtariXL,
	AtariXE
} Machine;

extern Machine	machine;

#endif
