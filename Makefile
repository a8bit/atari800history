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
# =============================
# Uncomment for SVGALIB Version
# =============================
#

#CC		=	gcc
#CPPFLAGS	=	-DSVGALIB
#CFLAGS		=	-c -O6
#LD		=	gcc
#LDFLAGS		=
#LDLIBS		=	-lvgagl -lvga

#
# =========================
# Uncomment for X11 Version
# Remove -DDOUBLE_SIZE for
# the smaller window.
# =========================
#

CC		=	gcc
CPPFLAGS	=	-DX11 -DDOUBLE_SIZE
CFLAGS		=	-c -O6
LD		=	gcc
LDFLAGS		=
LDLIBS		=	-lX11

#
# ================================================
# Uncomment for CURSES version (Solaris + others?)
# ================================================
#

#CC		=	gcc
#CPPFLAGS	=	-DCURSES
#CFLAGS		=	-c -O6
#LD		=	gcc
#LDFLAGS		=
#LDLIBS		=	-lcurses

#
# ===============================================
# Uncomment for NCURSES version (Linux + others?)
# ===============================================
#

#CC		=	gcc
#CPPFLAGS	=	-I/usr/include/ncurses -DCURSES -DNCURSES
#CFLAGS		=	-c -O6
#LD		=	gcc
#LDFLAGS		=
#LDLIBS		=	-lncurses

#
# ======================================================
# Uncomment for Commodore Amiga Version. Add -DINTUITION
# after -DAMIGA to enable test Graphic support - the
# keyboard is unusable! I don't think that the use of
# INTUITION is going to provide a very good emulator.
# Is anyone going to volunteer to Hit the Hardware!
# ======================================================
#

#CC		=	dcc
#CPPFLAGS	=	-DAMIGA
#CFLAGS		=	-c -mD
#LD		=	dcc
#LDFLAGS		=
#LDLIBS		=

#
# ======================================================
# You should not need to modify anything below this here
# ======================================================
#

DOCS		=	CHANGES COPYING CREDITS INSTALL OVERVIEW README USAGE vmsbuild.com
INCLUDES	=	Makefile system.h cpu.h atari.h atari_custom.h atari_h_device.h

6502		:	main.o atari.o cpu.o monitor.o atari_sio.o atari_h_device.o atari_custom.o $(DOCS)
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

clean	:
	rm *.o
