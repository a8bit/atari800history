#include <stdio.h>
#include <unistd.h>

#ifndef AMIGA
#include "config.h"
#endif

#ifdef VOXWARE
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include "pokeysnd.h"

#define FRAGSIZE	7

#define FALSE 0
#define TRUE 1

#define DEFDSPRATE 22050

static char *dspname = "/dev/dsp";
static int dsprate = DEFDSPRATE;
static int fragstofill = 0;
static int snddelay = 40;		/* delay in milliseconds */

static int gain = 4;

static int sound_enabled = TRUE;
static int dsp_fd;
/*
static int dsp_sample_rate_divisor = 35;
static int AUDCTL = 0x00;
static int AUDF[4] = {0, 0, 0, 0};
static int AUDC[4] = {0, 0, 0, 0};
*/
void Voxware_Initialise(int *argc, char *argv[])
{
	int i, j;
	struct audio_buf_info abi;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-sound") == 0)
			sound_enabled = TRUE;
		else if (strcmp(argv[i], "-nosound") == 0)
			sound_enabled = FALSE;
		else if (strcmp(argv[i], "-dsprate") == 0)
			sscanf(argv[++i], "%d", &dsprate);
		else if (strcmp(argv[i], "-snddelay") == 0)
			sscanf(argv[++i], "%d", &snddelay);
		else
			argv[j++] = argv[i];
	}

	*argc = j;

	if (sound_enabled) {
		if ((dsp_fd = open(dspname, O_WRONLY)) == -1) {
			perror(dspname);
			sound_enabled = 0;
			return;
		}

		if (ioctl(dsp_fd, SNDCTL_DSP_SPEED, &dsprate)) {
			fprintf(stderr, "%s: cannot set %d speed\n", dspname, dsprate);
			close(dsp_fd);
			sound_enabled = 0;
			return;
		}

		i = AFMT_U8;
		if (ioctl(dsp_fd, SNDCTL_DSP_SETFMT, &i)) {
			fprintf(stderr, "%s: cannot set 8-bit sample\n", dspname);
			close(dsp_fd);
			sound_enabled = 0;
			return;
		}

		fragstofill = ((dsprate * snddelay / 1000) >> FRAGSIZE) + 1;
		if (fragstofill > 100)
			fragstofill = 100;

		/* fragments of size 2^FRAGSIZE bytes */
		i = ((fragstofill + 1) << 16) | FRAGSIZE;
		if (ioctl(dsp_fd, SNDCTL_DSP_SETFRAGMENT, &i)) {
			fprintf(stderr, "%s: cannot set fragments\n", dspname);
			close(dsp_fd);
			sound_enabled = 0;
			return;
		}

		if (ioctl(dsp_fd, SNDCTL_DSP_GETOSPACE, &abi)) {
			fprintf(stderr, "%s: unable to get output space\n", dspname);
			close(dsp_fd);
			sound_enabled = 0;
			return;
		}

		printf("%s: %d(%d) fragments(free) of %d bytes, %d bytes free\n",
			   dspname,
			   abi.fragstotal,
			   abi.fragments,
			   abi.fragsize,
			   abi.bytes);

		Pokey_sound_init(FREQ_17_EXACT, dsprate, 1);
	}
}

void Voxware_Exit(void)
{
	if (sound_enabled)
		close(dsp_fd);
}

void Voxware_UpdateSound(void)
{
	int i;
	struct audio_buf_info abi;
	char dsp_buffer[1 << FRAGSIZE];

	if (!sound_enabled)
		return;

	if (ioctl(dsp_fd, SNDCTL_DSP_GETOSPACE, &abi))
		return;
		
	i = abi.fragstotal - abi.fragments;

	/* we need fragstofill fragments to be filled */
	for (; i < fragstofill; i++) {
		Pokey_process(dsp_buffer, sizeof(dsp_buffer));
		write(dsp_fd, dsp_buffer, sizeof(dsp_buffer));
	}
}

void Atari_AUDC(int channel, int byte)
{
	channel--;
	Update_pokey_sound( /* 0xd201 */ 1 + channel + channel, byte, 0, gain);
}

void Atari_AUDF(int channel, int byte)
{
	channel--;
	Update_pokey_sound( /* 0xd200 */ 0 + channel + channel, byte, 0, gain);
}

void Atari_AUDCTL(int byte)
{
	Update_pokey_sound( /* 0xd208 */ 8, byte, 0, gain);
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
