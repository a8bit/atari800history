#ifndef __PLATFORM__
#define __PLATFORM__

/*
 * This include file defines prototypes for platforms specific functions.
 */

void Atari_Initialise(int *argc, char *argv[]);
#ifdef DELAYED_VGAINIT
void SetupVgaEnvironment();
#endif
int Atari_Exit(int run_monitor);
int Atari_Keyboard(void);
#ifdef WIN32
void (*Atari_DisplayScreen)(UBYTE *screen);
#else
void Atari_DisplayScreen (UBYTE *screen);
#endif

int Atari_PORT(int num);
int Atari_TRIG(int num);
int Atari_POT(int num);
int Atari_CONSOL(void);
void Atari_AUDC(int channel, int byte);
void Atari_AUDF(int channel, int byte);
void Atari_AUDCTL(int byte);

#endif
