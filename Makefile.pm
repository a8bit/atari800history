        CC		= gcc
CPPFLAGS	= $(OTHER)
#CFLAGS		= -c -g -mpentium -fomit-frame-pointer -ffast-math -O3 -DGNU_C
#CFLAGS2         = -c -g -mpentium -fomit-frame-pointer -ffast-math -O3 -DGNU_C
CFLAGS		= -c -g -mpentium -fomit-frame-pointer -ffast-math -O3 -DGNU_C -DAT_USE_ALLEGRO -DAT_USE_ALLEGRO_JOY -DAT_USE_ALLEGRO_COUNTER -DUSE_DOSSOUND -DDELAYED_VGAINIT -DPOKEY_UPDATE -DSIGNED_SAMPLES -DCLIP -DPERRY_MODIFIED_POLY
#CFLAGS		= -c -g -mpentium -fomit-frame-pointer -ffast-math -O3 -DGNU_C -DAT_USE_ALLEGRO -DAT_USE_ALLEGRO_JOY -DAT_USE_ALLEGRO_COUNTER -DDELAYED_VGAINIT
LD		= gcc
LDFLAGS		=
LDLIBS		= -lm

default :
	@echo To build the Atari 800 Emulator, type:
	@echo make version
	@echo .
	@echo where version is one of
	@echo   basic
	@echo   pdcurses
	@echo   vga
	@echo . 
	@echo To reconfigure options, type: make config
	@echo To clean directory, type: make clean
	@echo To install the Emulator, type:

basic :
	@make atari800.exe CPPFLAGS="-DBASIC" LDLIBS="-lm" OBJ="atari_basic.o"
	@echo Finished.

pdcurses :
	@make atari800.exe CPPFLAGS="-DCURSES" LDLIBS="-lcurso -lm" OBJ="atari_curses.o"
	@echo Finished.

vga :
	@make atari800.exe CPPFLAGS="-DVGA" LDLIBS="-lm -lalleg" OBJ="atari_vga.o"
	@echo Finished.

#
# ======================================================
# You should not need to modify anything below this here
# ======================================================
#

INCLUDES        =       Makefile \
			config.h \
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

config config.h	:	configure.exe
	configure.exe

configure.exe	:	configure.o prompts.o
	$(LD) $(LDFLAGS) configure.o prompts.o $(LDLIBS) -o configure.exe

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
                supercart.o \
                prompts.o \
                rt-config.o \
                ui.o \
                list.o \
		dossound.o \
		pokeysnd.o

atari800.exe        :       $(OBJECTS) $(OBJ)
	$(LD) $(LDFLAGS) $(OBJECTS) $(OBJ) $(DJDIR)/lib/audiodjf.a $(LDLIBS) -o atari800.exe

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

supercart.o     :       supercart.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) supercart.c

ui.o            :       ui.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) ui.c

list.o          :       list.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) list.c

rt-config.o     :       rt-config.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) rt-config.c

prompts.o       :       prompts.c prompts.h
	$(CC) $(CPPFLAGS) $(CFLAGS) prompts.c

dossound.o	:       dossound.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) dossound.c

pokeysnd.o	:       pokeysnd.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) pokeysnd.c

atari_x11.o     :       atari_x11.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) atari_x11.c

atari_svgalib.o :       atari_svgalib.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) atari_svgalib.c

atari_curses.o  :       atari_curses.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) atari_curses.c

atari_amiga.o   :       atari_amiga.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) atari_amiga.c

nas.o           :       nas.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) nas.c

clean   :
	del configure.exe
	del configure
	del config.h
	del core
	del atari800
	del *.o
