
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "atari.h"
#include "config.h"
#include "log.h"
#include "cpu.h"
#include "memory-d.h"
#include "pia.h"

static int bin_fd=-1;
static UBYTE orig_regS;
int start_binloading=0;

static UBYTE addrL_init;

extern int tron;

static void BIN_load( void )
{ UBYTE buf[65536];
  int from,to,len,i;
  UBYTE low;

	if( (i=read(bin_fd,buf,4))!=4 )
	{	close(bin_fd); bin_fd=-1;
		regS=orig_regS;
		if( i==0 || i==2 )	/* Ok all loaded */
		{	regPC=(dGetByte(736)|dGetByte(737)<<8);
		}
		else
		{	/* error */
printf("Boot Error\n");
		}
		return;
	}

	dPutByte( 738 , addrL_init );	/* init low */
	dPutByte( 739 , 0x01 );		/* init high */

	from=buf[0]|buf[1]<<8;
	to=buf[2]|buf[3]<<8;
	len= (0x10000+to-from+1) & 0x0000ffff;
	if( read(bin_fd,buf,len)!=len )
	{	close(bin_fd); bin_fd=-1;
		regS=orig_regS;
		/* error */
		Aprint("Boot Error\n");
		return;
	}
	PIA_PutByte(_PORTB, 0xfe);	/* turn off operating system */
	for(i=0;i<len;i++)
	{	dPutByte(from,buf[i]);
		from=(from+1)&0x0000ffff;
	}
	PIA_PutByte(_PORTB, 0xff);	/* turn on operating system */
	dPutByte((0x0100 + regS--), ESC_BINLOADER_CONT );
	dPutByte((0x0100 + regS--), 0xf2 );	/* ESC */
	low=regS;
	dPutByte((0x0100 + regS--), 0x01 );	/* high */
	dPutByte((0x0100 + regS--), low );	/* low */
	regPC=(dGetByte(738)|dGetByte(739)<<8);
	return;
}

int BIN_loader( char *filename )
{ UBYTE buf[2];
	if( bin_fd>=0 )
	{	close(bin_fd);
		regS=orig_regS;
	}
	if( (bin_fd=open(filename, O_RDONLY | O_BINARY))<0 )
		return FALSE;
	if( read(bin_fd,buf,2)!=2 || buf[0]!=0xff || buf[1]!=0xff )
	{	close(bin_fd); bin_fd=-1;
		return FALSE;
	}

	Coldstart();
	start_binloading=1;
	
	return TRUE;
}

int BIN_loade_start( UBYTE *buffer )
{
	buffer[0]= 0x00;
	buffer[1]= 0x01;
	buffer[2]= 0x00;
	buffer[3]= 0x07;
	buffer[4]= 0x06;
	buffer[5]= 0x07;
	buffer[6]= 0xf2;	/* ESC */
	buffer[7]= ESC_BINLOADER_CONT;
	return('C');
}

void BIN_loader_cont( void )
{
	if( !start_binloading )
	{	regS+=2;	/* 4 - 2 */
		BIN_load();
	}
	else
  	{ 
		orig_regS=regS;
		addrL_init=regS;
		dPutByte((0x0100 + regS--), 0x60 );	/* RTS */
		dPutByte( 738 , addrL_init );	/* init low */
		dPutByte( 739 , 0x01 );		/* init high */
		BIN_load();
		start_binloading=0;
	}
}

