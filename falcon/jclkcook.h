/* Joy Clocky CookieJar public interface header file */

#define CLOCKY_IDENT		'JCLK'
#define CLOCKY_VERSION		0x220

#ifndef UBYTE
#define UBYTE unsigned char;
#endif

#ifdef CLOCKY_CONFIG
#define KBDLEN				384
#define PRNTBLLEN			128
#define DEADTBLLEN			30
#define HOTKEYS				11
#endif

struct	_jclkstruct {
	long	name;		/* compare with CLOCKY_IDENT, must be equal */
	short	version;	/* compare with CLOCKY_VERSION, it must be equal or higher! */
	struct {
		unsigned ShowTime:1;	/* 31 */
		unsigned ShowKuk:1;		/* 30 */
		unsigned Showdate:1;	/* 29 */
		unsigned Showday:1;		/* 28 */
		unsigned Showyear:1;	/* 27 */
		unsigned ShowCaps:1;	/* 26 */

		unsigned Unused1:3;		/* 23-25 */

		unsigned KbdEHC:1;		/* 22 */
		unsigned Kbddead:1;		/* 21 */
		unsigned Kbdasci:1;		/* 20 */
		unsigned Kbdcink:1;		/* 19 */
		unsigned Kbdbell:1;		/* 18 */
		unsigned Kbdlayo:2;		/* 16-17 */

		unsigned Misc4x:1;		/* 15 */
		unsigned Miscmys:1;		/* 14 */
		unsigned Miscprnt:1;	/* 13 */
		unsigned Miscturb:1;	/* 12 */
		unsigned Miscinv:1;		/* 11 */
		unsigned Misctut:1;		/* 10 */

		unsigned Unused2:2;		/* 8-9 */

		unsigned Saveron:1;		/* 7 */
		unsigned SaveMod1:1;	/* 6 */
		unsigned SaveMod2:1;	/* 5 */
		unsigned Saveact1:1;	/* 4 */
		unsigned Saveact2:1;	/* 3 */
		unsigned Unused3:3;		/* 0-2 */
	}parametry;
	short	saverlen;
	short	savecount;
	char	hotshift;			/* bits 0..3 */
	char	hottime;			/* bits 0..3 */
	char	unused_yet;			/* currently unused */
	char	actual_key;			/* scancode: 1..127 */
	char	*ehc_table;			/* ptr to char ehc_table[128] */
#ifdef CLOCKY_CONFIG
	UBYTE	normal_kbd[KBDLEN];
	UBYTE	ceska_kbd[KBDLEN];
	UBYTE	prntable[PRNTBLLEN];
	UBYTE	deadkeys_defined;	/* max DEADTABLELEN */
	UBYTE	deadtbl1[DEADTBLLEN];
	UBYTE	deadtbl2[DEADTBLLEN];
	UBYTE	deadtbl3[DEADTBLLEN];
	char	hotkeys[HOTKEYS];
#endif
};

typedef struct _jclkstruct JCLKSTRUCT;

/*
 * Pri pouziti struktur je vzdycky dobre zkontrolovat, jestli
 * zvoleny kompiler neco neudelal s rozmistenim polozek v pameti!
*/

#include <assert.h>
#ifdef CLOCKY_CONFIG
#define CHECK_CLOCKY_STRUCT	assert( sizeof(JCLKSTRUCT) == (22 + 2*KBDLEN + PRNTBLLEN + 1 + 3*DEADTBLLEN + HOTKEYS) );
#else
#define CHECK_CLOCKY_STRUCT	assert( sizeof(JCLKSTRUCT) == 22 );
#endif
