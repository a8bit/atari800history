#
# =============================
# Uncomment for Commodore Amiga
# =============================
#

#CC		=	dcc
#CPPFLAGS	=	-DAMIGA
#CFLAGS		=	-c -mD
#LD		=	dcc
#LDFLAGS		=
#LDLIBS		=

#
# ===========================
# Uncomment for BASIC Version
# ===========================
#

#CC		=	gcc
#CPPFLAGS	=	-DBASIC
#CFLAGS		=	-c -O6
#LD		=	gcc
#LDFLAGS		=
#LDLIBS		=

#
# =========================
# Uncomment for X11 Version
# =========================
#

CC		=	gcc
CPPFLAGS	=	-DX11
CFLAGS		=	-c -O6
LD		=	gcc
LDFLAGS		=
LDLIBS		=	-lX11

INCLUDES	=	system.h cpu.h atari.h atari_custom.h atari_h_device.h

6502		:	main.o atari.o cpu.o monitor.o atari_sio.o atari_h_device.o atari_custom.o
	$(LD) $(LDFLAGS) main.o atari.o cpu.o monitor.o atari_sio.o atari_h_device.o atari_custom.o $(LDLIBS) -o 6502

main.o		:	main.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) main.c

atari.o		:	atari.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) atari.c

cpu.o		:	cpu.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) cpu.c

monitor.o	:	monitor.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) monitor.c

atari_sio.o	:	atari_sio.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) atari_sio.c

atari_h_device.o:	atari_h_device.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) atari_h_device.c

atari_custom.o	:	atari_custom.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) atari_custom.c
