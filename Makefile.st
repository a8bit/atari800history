CC		= gcc
CPPFLAGS	= $(OTHER)
CFLAGS		= -c -O2 -fno-strength-reduce -DGNU_C
LD		= gcc
LDFLAGS		=
LDLIBS		=

default :
	@echo To build the Atari 800 Emulator, type:
	@echo make version
	@echo .
	@echo where version is one of
	@echo   basic
	@echo   curses
	@echo   st
	@echo . 
	@echo To reconfigure options, type: make config
	@echo To clean directory, type: make clean
	@echo To install the Emulator, type:

basic :
	@make atari800 CPPFLAGS="-DBASIC" LDLIBS="-lm" OBJ="atari_basic.o"
	@echo Finished.

curses :
	@make atari800 CPPFLAGS="-DCURSES" LDLIBS="-lcurses -lm" OBJ="a_curses.o"
	@echo Finished.

st :
	@make atari800 OBJ="atari_st.o c2p.o videl.o"
	@echo Finished.

#
# ======================================================
# You should not need to modify anything below this here
# ======================================================
#

INCLUDES        =       config.h \
			rt-config.h \
			cpu.h \
			atari.h \
			colours.h \
			antic.h \
			gtia.h \
			pokey.h \
			pia.h \
			devices.h \
			monitor.h \
			sio.h \
			supercart.h \
			platform.h

config config.h	:	configure
	./configure

configure	:	configure.o prompts.o
	$(LD) $(LDFLAGS) configure.o prompts.o $(LDLIBS) -o configure

configure.o	:	configure.c
	$(CC) $(CPPFLAGS) $(CFLAGS) configure.c

OBJECTS =       atari.o \
		cpu.o \
		monitor.o \
		sio.o \
		devices.o \
		antic.o \
		gtia.o \
		pokey.o \
		pia.o \
                supercar.o \
                prompts.o \
                rtconfig.o \
                ui.o \
                list.o \
		sound.o \
		pokey11.o

atari800        :       $(OBJECTS) $(OBJ)
	$(LD) $(LDFLAGS) $(OBJECTS) $(OBJ) $(LDLIBS) -o atari800

atari.o         :       atari.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) atari.c

cpu.o           :       cpu.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) cpu.c

monitor.o       :       monitor.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) monitor.c

sio.o           :       sio.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) sio.c

devices.o       :       devices.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) devices.c

antic.o         :       antic.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) antic.c

gtia.o          :       gtia.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) gtia.c

pokey.o         :       pokey.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) pokey.c

pia.o           :       pia.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) pia.c

supercar.o     :       supercart.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) supercart.c -o supercar.o

ui.o            :       ui.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) ui.c

list.o          :       list.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) list.c

rtconfig.o     :       rt-config.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) rt-config.c -o rtconfig.o

prompts.o       :       prompts.c prompts.h
	$(CC) $(CPPFLAGS) $(CFLAGS) prompts.c

sound.o		:       sound.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) sound.c

c2p.o	:	c2p.s c2pmac.s
	xgen -L1 c2p.o c2p.s
	gst2gcc gcc c2p.o

videl.o	:	videl.s
	xgen -L1 videl.o videl.s
	gst2gcc gcc videl.o

pokey11.o	:       pokey11.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) pokey11.c

atari_x11.o     :       atari_x11.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) atari_x11.c

atari_st.o :       atari_st.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) atari_st.c

a_curses.o  :       atari_curses.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) atari_curses.c

atari_amiga.o   :       atari_amiga.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) atari_amiga.c

nas.o           :       nas.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) nas.c

clean   :
	rm -f configure
	rm -f config.h
	rm -f core
	rm -f *.o
