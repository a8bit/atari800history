CC		= gcc
CPPFLAGS	= $(OTHER)
CFLAGS		= -c -O6
LD		= gcc
LDFLAGS		=
LDLIBS		=

PREFIX		= /usr/local
BIN_PATH	= ${PREFIX}/bin
LIB_PATH	= ${PREFIX}/lib
MAN_PATH	= ${PREFIX}/man

ATARI_LIBRARY	= ${LIB_PATH}/atari

default :
	@echo "To build the Atari 800 Emulator, type:"
	@echo "make <version>"
	@echo ""
	@echo "where <version> is one of"
	@echo "  basic"
	@echo "  linux-svgalib linux-svgalib-joystick"
	@echo "  linux-x11-joystick linux-xview-joystick"
	@echo "  x11 openwin hp9000-ansic-x11 xview"
	@echo "  curses sunos-curses linux-ncurses"
	@echo ""
	@echo "To install the Emulator, type:"
	@echo ""
	@echo "make install-svgalib"
	@echo "make install"

basic :
	make atari800 \
		CPPFLAGS="-DBASIC" \
		OBJ="atari_basic.o"

linux-svgalib :
	make atari800 \
		LDLIBS="-lvgagl -lvga" \
		OBJ="atari_svgalib.o"

linux-svgalib-joystick :
	make atari800 \
		OTHER="-DLINUX_JOYSTICK" \
		LDLIBS="-lvgagl -lvga" \
		OBJ="atari_svgalib.o"

linux-x11-joystick :
	make atari800 \
		OTHER="-DLINUX_JOYSTICK" \
		LDFLAGS="-L/usr/X11/lib" \
		LDLIBS="-lX11" \
		OBJ="atari_x11.o"

linux-xview-joystick :
	make atari800 \
		OTHER="-I/usr/openwin/include -DXVIEW -DLINUX_JOYSTICK" \
		LDFLAGS="-L/usr/openwin/lib -L/usr/X11/lib" \
		LDLIBS="-lxview -lolgx -lX11" \
		OBJ="atari_x11.o"

x11 :
	make atari800 \
		LDFLAGS="-L/usr/X11/lib" \
		LDLIBS="-lX11" \
		OBJ="atari_x11.o"

openwin :
	make atari800 \
		CPPFLAGS="-I/usr/openwin/include" \
		LDFLAGS="-L/usr/openwin/lib" \
		LDLIBS="-lX11" \
		OBJ="atari_x11.o"

hp9000-ansic-x11 :
	make atari800 \
		CC="cc" \
		CPPFLAGS="-D_POSIX_SOURCE" \
		CFLAGS="-c -O -Aa -I/usr/include/X11R5" \
		LD="cc" \
		LDFLAGS="-L/usr/lib/X11R5" \
		LDLIBS="-lX11"

xview :
	make atari800 \
		OTHER="-I/usr/openwin/include -DXVIEW" \
		LDFLAGS="-L/usr/openwin/lib -L/usr/X11/lib" \
		LDLIBS="-lxview -lolgx -lX11" \
		OBJ="atari_x11.o"

curses :
	make atari800 \
		CPPFLAGS="-DCURSES" \
		LDLIBS="-lcurses" \
		OBJ="atari_curses.o"

sunos-curses :
	make atari800 \
		CPPFLAGS="-I/usr/5include -DCURSES" \
		LDLIBS="-lcurses" \
		LDFLAGS="-L/usr/5lib" \
		OBJ="atari_curses.o"

linux-ncurses :
	make atari800 \
		CPPFLAGS="-I/usr/include/ncurses -DCURSES -DNCURSES" \
		LDLIBS="-lncurses" \
		OBJ="atari_curses.o"
#
# ======================================================
# You should not need to modify anything below this here
# ======================================================
#

DOCS		=	BUGS CHANGES COPYING CREDITS INSTALL OVERVIEW README USAGE vmsbuild.com
INCLUDES	=	Makefile system.h cpu.h atari.h atari_custom.h atari_h_device.h colours.h

atari800	:	main.o atari.o cpu.o monitor.o atari_sio.o atari_h_device.o atari_custom.o $(OBJ) $(DOCS)
	$(LD) $(LDFLAGS) main.o atari.o cpu.o monitor.o atari_sio.o atari_h_device.o atari_custom.o $(OBJ) $(LDLIBS) -o atari800

main.o		:	main.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) main.c

atari.o		:	atari.c $(INCLUDES)
	$(CC) $(CPPFLAGS) -DATARI_LIBRARY="\"${ATARI_LIBRARY}\"" $(CFLAGS) atari.c

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

atari_x11.o	:	atari_x11.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) atari_x11.c

atari_svgalib.o	:	atari_svgalib.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) atari_svgalib.c

atari_curses.o	:	atari_curses.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) atari_curses.c

atari_amiga.o	:	atari_amiga.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) atari_amiga.c

clean	:
	rm *.o

install-svgalib : install
	chown root.root ${BIN_PATH}/atari800
	chmod 4755 ${BIN_PATH}/atari800

install :
	cp atari800 ${BIN_PATH}/atari800
	cp atari800.man ${MAN_PATH}/man1/atari800.1
