
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
static UBYTE my2,my3,my9,my10,my11,my12,my13,orig_dos_l,orig_dos_h;

extern int tron;

void set_vect( UBYTE dos_l, UBYTE dos_h )
{
	if( my2==dGetByte(2) && my3==dGetByte(3) )
	{	my2=dGetByte(736);
		my3=dGetByte(737);
		dPutByte(2,my2);
		dPutByte(3,my3);
	}
	if( my12==dGetByte(12) && my13==dGetByte(13) )
	{	my12=dGetByte(736);
		my13=dGetByte(737);
		dPutByte(12,my12);
		dPutByte(13,my13);
	}
	if( my10==dGetByte(10) && my11==dGetByte(11) )
	{	my10=dos_l;
		my11=dos_h;
		dPutByte(10,my10);
		dPutByte(11,my11);
	}
	if( my9==dGetByte(9) )
	{	my9=1;
		dPutByte(9,my9);
	}
}

static void BIN_load( void )
{ UBYTE buf[65536];
  int from,to,len,i;
  UBYTE low;

	if( (i=read(bin_fd,buf,4))!=4 )
	{	close(bin_fd); bin_fd=-1;
		regS=orig_regS;
		if( i!=0 )
			Aprint("Boot Error: Garbage %d byte(s) at end of file\n"
					,i);
		/* Ok all loaded */
		set_vect( orig_dos_l, orig_dos_h );
		regPC=(dGetByte(736)|dGetByte(737)<<8);
		return;
	}
	if( buf[0]==0xff && buf[1]==0xff )
	{	buf[0]=buf[2];
		buf[1]=buf[3];
		if( (i=read(bin_fd,buf+2,2))!=2 )
		{	close(bin_fd); bin_fd=-1;
			regS=orig_regS;
			Aprint("Boot Error 2\n");
			return;
		}
	}

	dPutByte( 738 , addrL_init );	/* init low */
	dPutByte( 739 , 0x01 );		/* init high */

	from=buf[0]|buf[1]<<8;
	to=buf[2]|buf[3]<<8;
	len= (0x10000+to-from+1) & 0x0000ffff;
	if( dGetByte(736)==addrL_init && dGetByte(737)==0x01 )
	{	dPutByte( 736, buf[0] );
		dPutByte( 737, buf[1] );
	}
		
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
	set_vect( low+1, 0x01 );
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
		dPutByte( 736 , addrL_init );	/* only for default start */
		dPutByte( 737 , 0x01 );	

		my2= dGetByte( 2 );
		my3= dGetByte( 3 );
		my9= dGetByte( 9 );
		orig_dos_l=my10= dGetByte( 10 );
		orig_dos_h=my11= dGetByte( 11 );
		my12= dGetByte( 12 );
		my13= dGetByte( 13 );
		dPutByte(580,0);

		BIN_load();
		start_binloading=0;
	}
}

