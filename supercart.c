
#include "atari.h"
#include "cpu.h"

#define FALSE 0
#define TRUE 1

static char *rcsid = "$Id: supercart.c,v 1.5 1998/02/21 14:51:54 david Exp $";

extern UBYTE *cart_image;
extern int cart_type;

int SuperCart_PutByte(UWORD addr, UBYTE byte)
{
	switch (cart_type) {
	case OSS_SUPERCART:
		switch (addr & 0xff0f) {
		case 0xd500:
			if (cart_image)
				memcpy(memory + 0xa000, cart_image, 0x1000);
			break;
		case 0xd504:
			if (cart_image)
				memcpy(memory + 0xa000, cart_image + 0x1000, 0x1000);
			break;
		case 0xd503:
		case 0xd507:
			if (cart_image)
				memcpy(memory + 0xa000, cart_image + 0x2000, 0x1000);
			break;
		}
		break;
	case DB_SUPERCART:
		switch (addr & 0xff07) {
		case 0xd500:
			if (cart_image)
				memcpy(memory + 0x8000, cart_image, 0x2000);
			break;
		case 0xd501:
			if (cart_image)
				memcpy(memory + 0x8000, cart_image + 0x2000, 0x2000);
			break;
		case 0xd506:
			if (cart_image)
				memcpy(memory + 0x8000, cart_image + 0x4000, 0x2000);
			break;
		}
		break;
	default:
		break;
	}

	return FALSE;
}
