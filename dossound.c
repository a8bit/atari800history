#include "config.h"
#include "audio.h"				/* AAA for audio */
#include <fcntl.h>
#include "atari.h"
/*
   #include <sys/ioctl.h>
   #include <sys/soundcard.h>
 */

#include "pokeysnd.h"
extern int timesync;
static char *rcsid = "$Id: sound.c,v 1.2 1997/04/18 22:23:38 david Exp $";
int speed_count = 0;
/* 0002 = 2 Fragments */
/* 0007 = means each fragment is 2^2 or 128 bytes */
/* See voxware docs in /usr/src/linux/drivers/sound for more info */

#define FRAG_SPEC 0x0002000a

#define FALSE 0
#define TRUE 1

/*
   static char dsp_buffer[44100];
   static int sndbufsize;
 */
static int throttle = TRUE;
static sound_enabled = TRUE;
static soundcard = -1;
static int gain = 16;
/*
   static int dsp_fd;

   static int dsp_sample_rate;
   static int dsp_sample_rate_divisor = 35;
 */
static int AUDCTL = 0x00;
static int AUDF[4] =
{0, 0, 0, 0};
static int AUDC[4] =
{0, 0, 0, 0};
/* audio related stuff AAA from mame */
#define NUMVOICES 16
#define SAMPLE_RATE 44100
HAC hVoice[NUMVOICES];
LPAUDIOWAVE lpWave[NUMVOICES];
int Volumi[NUMVOICES];
int MasterVolume = 256;
/*  Fm stuff dont need
   unsigned char No_FM = 0;
   unsigned char RegistersYM[264*5];  /* MAX 5 YM-2203 */
/*stuff from pokyintf.c in mame AAA */
//#define TARGET_EMULATION_RATE 44100   /* will be adapted to be a multiple of buffer_len */
static int buffer_len;
static int emulation_rate;
static int sample_pos;
/* we dont need this
   extern uint8 rng[MAXPOKEYS];
   static uint8 pokey_random[MAXPOKEYS];     /* The random number for each pokey */
/*
   static struct POKEYinterface *intf;
 */
static unsigned char *buffer;
/*added to support pokyintf.c */
static int frames_per_second = 60;
static int updates_per_frame = 1;
static int real_updates_per_frame = 262;	//240;

/* osd play streamed sample from mame AAA */

void osd_play_streamed_sample(int channel, unsigned char *data, int len, int freq, int volume)
{
	static int playing[NUMVOICES];
	static int c[NUMVOICES];


	if (sound_enabled == 0 || channel >= NUMVOICES)
		return;

	if (!playing[channel]) {
		if (lpWave[channel]) {
			AStopVoice(hVoice[channel]);
			ADestroyAudioData(lpWave[channel]);
			free(lpWave[channel]);
			lpWave[channel] = 0;
		}

		if ((lpWave[channel] = (LPAUDIOWAVE) malloc(sizeof(AUDIOWAVE))) == 0)
			return;

		lpWave[channel]->wFormat = AUDIO_FORMAT_8BITS | AUDIO_FORMAT_MONO | AUDIO_FORMAT_LOOP;
		lpWave[channel]->nSampleRate = SAMPLE_RATE - 1;
		lpWave[channel]->dwLength = 3 * len;
		lpWave[channel]->dwLoopStart = 0;
		lpWave[channel]->dwLoopEnd = 3 * len;
		if (ACreateAudioData(lpWave[channel]) != AUDIO_ERROR_NONE) {
			free(lpWave[channel]);
			lpWave[channel] = 0;

			return;
		}

		memset(lpWave[channel]->lpData, 0, 3 * len);
		memcpy(lpWave[channel]->lpData, data, len);
		/* upload the data to the audio DRAM local memory */
		AWriteAudioData(lpWave[channel], 0, 3 * len);
		APlayVoice(hVoice[channel], lpWave[channel]);
		ASetVoiceFrequency(hVoice[channel], freq);
		ASetVoiceVolume(hVoice[channel], MasterVolume * volume / 400);
		playing[channel] = 1;
		c[channel] = 1;
	}
	else {
		DWORD pos;


		if (throttle) {			/* sync with audio only when speed throttling is not turned off */
			for (;;) {
				AGetVoicePosition(hVoice[channel], &pos);
				if (c[channel] == 0 && pos >= len)
					break;
				if (c[channel] == 1 && (pos < len || pos >= 2 * len))
					break;
				if (c[channel] == 2 && pos < 2 * len)
					break;
				AUpdateAudio();
				/*was osd_update_audio(); */
				speed_count++;
			}
		}

		memcpy(&lpWave[channel]->lpData[len * c[channel]], data, len);
		AWriteAudioData(lpWave[channel], len * c[channel], len);
		c[channel]++;
		if (c[channel] == 3)
			c[channel] = 0;
	}

	Volumi[channel] = volume / 4;
}



/* more stuff from pokyintf.c from mame AAA */
static int updatecount;
static float buf_fraction;
void pokey_update(void)
{
	//int buf_start;
	//static float buf_end;
	//int ibuf_end;
	int newpos;
	if (sound_enabled == 0)
		return;
	if (updatecount >= real_updates_per_frame)
		return;
	/*if(updatecount==0){
	   Pokey_process (&buffer[0],buffer_len);
	   updatecount++;
	   }
	   return;
	 */
	/*if(updatecount==0){
	   buf_start=0;
	   buf_end=0;
	   }else{
	   buf_start=(int)buf_end+1;
	   }

	   buf_end=buf_end+buf_fraction;
	   ibuf_end=buf_end;
	   //if(ibuf_end>=buffer_len) ibuf_end=buffer_len-1;
	   if(updatecount==real_updates_per_frame-1){
	   ibuf_end=buffer_len-1;
	   }
	 */
	newpos = buffer_len * (updatecount) / real_updates_per_frame;
	if (newpos > buffer_len)
		newpos = buffer_len;

	Pokey_process(buffer + sample_pos, newpos - sample_pos);
	sample_pos = newpos;
	updatecount++;
}


void pokey_sh_update(void)
{

	if (sound_enabled == 0)
		return;

	//if (updates_per_frame == 1) pokey_update();

/*if (errorlog && updatecount != intf->updates_per_frame)
   fprintf(errorlog,"Error: pokey_update() has not been called %d times in a frame\n",intf->updates_per_frame);
 */
	updatecount = 0;			/* must be zeroed here to keep in sync in case of a reset */
	if (sample_pos < buffer_len)
		Pokey_process(buffer + sample_pos, buffer_len - sample_pos);
	sample_pos = 0;

	osd_play_streamed_sample(0, buffer, buffer_len * updates_per_frame, emulation_rate, 100);

}

int dossound_Initialise(int *argc, char *argv[])
{
	int i, j;
	if (tv_mode == PAL) {
		frames_per_second = 50;
		real_updates_per_frame = 312;
	}
	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-sound") == 0)
			sound_enabled = TRUE;
		else if (strcmp(argv[i], "-nosound") == 0)
			sound_enabled = FALSE;
		/*else if (strcmp(argv[i],"-dsp_divisor") == 0)
		   sscanf (argv[++i],"%d",&dsp_sample_rate_divisor);
		 */
		else if (stricmp(argv[i], "-soundcard") == 0) {
			i++;
			if (i < *argc)
				soundcard = atoi(argv[i]);
		}
		else
			argv[j++] = argv[i];
	}
	*argc = j;


	if (sound_enabled) {
		AUDIOINFO info;
		AUDIOCAPS caps;
		/*char *blaster_env; AAA only need for fm */


		/* initialize SEAL audio library */
		if (AInitialize() == AUDIO_ERROR_NONE) {
			if (soundcard == -1) {
				unsigned int k;


				printf("\nSelect the audio device:\n(if you have an AWE 32, choose Sound Blaster for a more faithful emulation)\n");
				for (k = 0; k < AGetAudioNumDevs(); k++) {
					if (AGetAudioDevCaps(k, &caps) == AUDIO_ERROR_NONE)
						printf("  %2d. %s\n", k, caps.szProductName);
				}
				printf("\n");

				if (k < 10) {
					i = getch();
					soundcard = i - '0';
				}
				else
					scanf("%d", &soundcard);
			}

			if (soundcard == 0)	/* silence */
				sound_enabled = 0;	/*AAA changed to sound_enabled */
			else {
				/* open audio device */
/*                              info.nDeviceId = AUDIO_DEVICE_MAPPER; */
				info.nDeviceId = soundcard;
				info.wFormat = AUDIO_FORMAT_16BITS | AUDIO_FORMAT_MONO;
				info.nSampleRate = SAMPLE_RATE - 1;
				if (AOpenAudio(&info) != AUDIO_ERROR_NONE) {
					printf("audio initialization failed\n");
					return 1;
				}

				/* open and allocate voices, allocate waveforms */
				if (AOpenVoices(NUMVOICES) != AUDIO_ERROR_NONE) {
					printf("voices initialization failed\n");
					return 1;
				}

				for (i = 0; i < NUMVOICES; i++) {
					if (ACreateAudioVoice(&hVoice[i]) != AUDIO_ERROR_NONE) {
						printf("voice #%d creation failed\n", i);
						return 1;
					}

					ASetVoicePanning(hVoice[i], 128);

					lpWave[i] = 0;
				}
			}
		}
	}
	/*if (sound_enabled)
	   {
	 */
	/*int channel;
	   int dspbits;
	   unsigned int formats;
	   int tmp;

	   dsp_fd = open ("/dev/dsp", O_WRONLY, 0777);
	   if (dsp_fd == -1)
	   {
	   perror ("/dev/dsp");
	   exit (1);
	   }
	 */

	/*
	 * Get sound formats
	 */
	/*
	   ioctl (dsp_fd, SNDCTL_DSP_GETFMTS, &formats);
	 */
	/*
	 * Set sound of sound fragment to special?
	 */
	/*
	   tmp = FRAG_SPEC;
	   ioctl (dsp_fd, SNDCTL_DSP_SETFRAGMENT, &tmp);
	 */
	/*
	 * Get preferred buffer size
	 */
	/*
	   ioctl (dsp_fd, SNDCTL_DSP_GETBLKSIZE, &sndbufsize);
	 */
	/*
	 * Set to 8bit sound
	 */
	/*
	   dspbits = 8;
	   ioctl (dsp_fd, SNDCTL_DSP_SAMPLESIZE, &dspbits);
	   ioctl (dsp_fd, SOUND_PCM_READ_BITS, &dspbits);
	 */
	/*
	 * Set sample rate
	 */
	/*
	   ioctl (dsp_fd, SNDCTL_DSP_SPEED, &dsp_sample_rate);
	   ioctl (dsp_fd, SOUND_PCM_READ_RATE, &dsp_sample_rate);
	 */
	/*Pokey_sound_init (FREQ_17_EXACT, dsp_sample_rate);
	 */
	/* } */
	/*Pokey_sound_init (FREQ_17_EXACT,SAMPLE_RATE); */
	/* stuff from pokyintf.c AAA */
	/*intf = interface; */


	/*buffer_len = TARGET_EMULATION_RATE / Machine->drv->frames_per_second / intf->updates_per_frame;
	   emulation_rate = buffer_len * Machine->drv->frames_per_second * intf->updates_per_frame;
	 */
	if (sound_enabled) {
		timesync = FALSE;
		buffer_len = (SAMPLE_RATE - 1) / frames_per_second / updates_per_frame;
		emulation_rate = buffer_len * frames_per_second * updates_per_frame;
		sample_pos = 0;
		//buf_fraction=((float)buffer_len)/(float)real_updates_per_frame;

		if ((buffer = (char *) malloc(buffer_len * updates_per_frame)) == 0) {
			free(buffer);
			return 1;
		}
		memset(buffer, 0x00 /*,0x80 */ , buffer_len * updates_per_frame);

		Pokey_sound_init(FREQ_17_EXACT, emulation_rate, 1);
	}
	else {
		timesync = TRUE;
	}
	return 0;
}

void dossound_Exit(void)
{
	/*if (sound_enabled)
	   close (dsp_fd);
	 */
}

void dossound_UpdateSound(void)
{
	if (sound_enabled) {
		pokey_sh_update();
		/*sndbufsize = dsp_sample_rate / dsp_sample_rate_divisor;
		 */
		/*Pokey_process (dsp_buffer, sndbufsize);
		 */
		/*
		 * Send sound buffer to DSP device
		 */
		/*
		   write (dsp_fd, dsp_buffer, sndbufsize);
		 */

	}
}

void Atari_AUDC(int channel, int byte)
{
	channel--;
	Update_pokey_sound( /*0xd201 */ 1 + channel + channel, byte, 0, gain);
}

void Atari_AUDF(int channel, int byte)
{
	channel--;
	Update_pokey_sound( /*0xd200 */ channel + channel, byte, 0, gain);
}

void Atari_AUDCTL(int byte)
{
	Update_pokey_sound( /*0xd208 */ 8, byte, 0, gain);
}

#if 0
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
