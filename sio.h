#ifndef __SIO__
#define __SIO__

#include "atari.h"

#define MAX_DRIVES 8
#define FILENAME_LEN 256

extern char sio_status[256];
extern char sio_filename[MAX_DRIVES][FILENAME_LEN];

int SIO_Mount (int diskno, char *filename);
void SIO_Dismount (int diskno);
void SIO_DisableDrive (int diskno);
void SIO (void);

void SIO_SEROUT (unsigned char byte, int cmd);
int SIO_SERIN (void);

#endif
