#ifndef AMIGA
#include "config.h"
#endif

#ifdef VOXWARE
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include "pokey11.h"

static char *rcsid = "$Id: sound.c,v 1.5 1998/02/21 15:20:42 david Exp $";

/* 0002 = 2 Fragments */
/* 0007 = means each fragment is 2^2 or 128 bytes */
/* See voxware docs in /usr/src/linux/drivers/sound for more info */

#define FRAG_SPEC 0x0002000a

#define FALSE 0
#define TRUE 1

static char dsp_buffer[44100];
static int sndbufsize;

static sound_enabled = TRUE;
static int dsp_fd;

static int dsp_sample_rate;
static int dsp_sample_rate_divisor = 35;
static int AUDCTL = 0x00;
static int AUDF[4] =
{0, 0, 0, 0};
static int AUDC[4] =
{0, 0, 0, 0};

void Voxware_Initialise(int *argc, char *argv[])
{
	int i, j;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-sound") == 0)
			sound_enabled = TRUE;
		else if (strcmp(argv[i], "-nosound") == 0)
			sound_enabled = FALSE;
		else if (strcmp(argv[i], "-dsp_divisor") == 0)
			sscanf(argv[++i], "%d", &dsp_sample_rate_divisor);
		else
			argv[j++] = argv[i];
	}

	*argc = j;

	if (sound_enabled) {
		int channel;
		int dspbits;
		unsigned int formats;
		int tmp;

		dsp_fd = open("/dev/dsp", O_WRONLY, 0777);
		if (dsp_fd == -1) {
			perror("/dev/dsp");
			exit(1);
		}
		/*
		 * Get sound formats
		 */

		ioctl(dsp_fd, SNDCTL_DSP_GETFMTS, &formats);

		/*
		 * Set sound of sound fragment to special?
		 */

		tmp = FRAG_SPEC;
		ioctl(dsp_fd, SNDCTL_DSP_SETFRAGMENT, &tmp);

		/*
		 * Get preferred buffer size
		 */

		ioctl(dsp_fd, SNDCTL_DSP_GETBLKSIZE, &sndbufsize);

		/*
		 * Set to 8bit sound
		 */

		dspbits = 8;
		ioctl(dsp_fd, SNDCTL_DSP_SAMPLESIZE, &dspbits);
		ioctl(dsp_fd, SOUND_PCM_READ_BITS, &dspbits);

		/*
		 * Set sample rate
		 */

		ioctl(dsp_fd, SNDCTL_DSP_SPEED, &dsp_sample_rate);
		ioctl(dsp_fd, SOUND_PCM_READ_RATE, &dsp_sample_rate);

		Pokey_sound_init(FREQ_17_EXACT, dsp_sample_rate);
	}
}

void Voxware_Exit(void)
{
	if (sound_enabled)
		close(dsp_fd);
}

void Voxware_UpdateSound(void)
{
	if (sound_enabled) {
		sndbufsize = dsp_sample_rate / dsp_sample_rate_divisor;

		Pokey_process(dsp_buffer, sndbufsize);
		/*
		 * Send sound buffer to DSP device
		 */

		write(dsp_fd, dsp_buffer, sndbufsize);
	}
}

void Atari_AUDC(int channel, int byte)
{
	channel--;
	Update_pokey_sound(0xd201 + channel + channel, byte);
}

void Atari_AUDF(int channel, int byte)
{
	channel--;
	Update_pokey_sound(0xd200 + channel + channel, byte);
}

void Atari_AUDCTL(int byte)
{
	Update_pokey_sound(0xd208, byte);
}

#else
void Atari_AUDC(int channel, int byte)
{
}

void Atari_AUDF(int channel, int byte)
{
}

void Atari_AUDCTL(int byte)
{
}

#endif
