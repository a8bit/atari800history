#ifndef __SIO__
#define __SIO__

#include "atari.h"

int SIO_Mount (int diskno, char *filename);
int SIO_Dismount (int diskno);
void SIO_DisableDrive (int diskno);
void SIO (void);

void SIO_SEROUT (unsigned char byte, int cmd);
int SIO_SERIN (void);

#endif
