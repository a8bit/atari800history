#ifndef A800_INCLUDED
#define	A800_INCLUDED

enum ESCAPE
{
	ESC_SIO,
	ESC_E_OPEN,
	ESC_E_CLOSE,
	ESC_E_READ,
	ESC_E_WRITE,
	ESC_E_STATUS,
	ESC_E_SPECIAL,
	ESC_K_OPEN,
	ESC_K_CLOSE,
	ESC_K_READ,
	ESC_K_WRITE,
	ESC_K_STATUS,
	ESC_K_SPECIAL,
	ESC_H_OPEN,
	ESC_H_CLOSE,
	ESC_H_READ,
	ESC_H_WRITE,
	ESC_H_STATUS,
	ESC_H_SPECIAL
};

/*
	=======================================
	Define CIOV Input Output Control Blocks
	=======================================
*/

#define	OPEN		0x03
#define	GETREC		0x05
#define	GETCHR		0x07
#define	PUTREC		0x09
#define	PUTCHR		0x0b
#define	CLOSE		0x0c
#define	STATUS		0x0d

/*
	==========================
	Define Function Prototypes
	==========================
*/

void	initialise_os ();
void	add_esc (UWORD address, UBYTE esc_code);
void	A800_RUN (void);
void	ESC (UBYTE esc_code);

#endif
