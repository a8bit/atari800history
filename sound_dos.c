#include "config.h"

#include "pokeysnd.h"
#include "sbdrv.h"

static char *rcsid = "$Id: sound_dos.c,v 1.2 1998/02/21 15:00:23 david Exp $";

#define FALSE 0
#define TRUE 1

static sound_enabled = TRUE;

int playback_freq = FREQ_17_APPROX / 28 / 3;
int buffersize = 400;
int DMAmode = AUTO_DMA;

static int AUDCTL = 0x00;
static int AUDF[4] =
{0, 0, 0, 0};
static int AUDC[4] =
{0, 0, 0, 0};

void Sound_Initialise(int *argc, char *argv[])
{
	int i, j;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-sound") == 0)
			sound_enabled = TRUE;
		else if (strcmp(argv[i], "-nosound") == 0)
			sound_enabled = FALSE;
		else
			argv[j++] = argv[i];
	}

	*argc = j;

	if (sound_enabled) {
		int channel;
		int dspbits;
		unsigned int formats;
		int tmp;

		if (!OpenSB(playback_freq, buffersize)) {
			printf("Cannot init sound card\n");
			sound_enabled = FALSE;
		}
		else {
/*
   Set_line_volume(15,15);
   Set_master_volume(15,15);
 */
			Pokey_sound_init(FREQ_17_APPROX, playback_freq, 1);
			Start_audio_output(DMAmode, Pokey_process);
		}
	}
}

void Sound_Exit(void)
{
	if (sound_enabled)
		CloseSB();
}

void Atari_AUDC(int channel, int byte)
{
	channel--;
	Update_pokey_sound(0xd201 + channel + channel, byte, 0, 4);
}

void Atari_AUDF(int channel, int byte)
{
	channel--;
	Update_pokey_sound(0xd200 + channel + channel, byte, 0, 4);
}

void Atari_AUDCTL(int byte)
{
	Update_pokey_sound(0xd208, byte, 0, 4);
}
