#ifndef __PLATFORM__
#define __PLATFORM__

/*
 * This include file defines prototypes for platforms specific functions.
 */

void Atari_Initialise (int *argc, char *argv[]);
int Atari_Exit (int run_monitor);
int Atari_Keyboard (void);
void Atari_DisplayScreen (UBYTE *screen);

int Atari_PORT (int num);
int Atari_TRIG (int num);
int Atari_POT (int num);
int Atari_CONSOL (void);
int Atari_AUDC (int channel, int byte);
int Atari_AUDF (int channel, int byte);
int Atari_AUDCTL (int byte);

#endif
