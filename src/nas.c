#include <audio/audiolib.h>
#include "config.h"

#define FALSE 0
#define TRUE 1

#define SAMPLE_RATE 8000

static nasbug = FALSE;			/* Debug Flag */
static sound_enabled = FALSE;

static AuServer *au_server;
static AuDeviceID *au_device;
static AuFlowID au_channel[4];

static int waveform = AuWaveFormSquare;
static int au_channel_clock[4] =
{64000, 64000, 64000, 64000};
static int M[4] =
{1, 1, 1, 1};

static int AUDCTL = 0x00;
static int AUDF[4] =
{0, 0, 0, 0};
static int AUDC[4] =
{0, 0, 0, 0};

void NAS_Initialise(int *argc, char *argv[])
{
	int i, j;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-sound") == 0)
			sound_enabled = TRUE;
		else if (strcmp(argv[i], "-squarewave") == 0)
			waveform = AuWaveFormSquare;
		else if (strcmp(argv[i], "-sinewave") == 0)
			waveform = AuWaveFormSine;
		else if (strcmp(argv[i], "-nasbug") == 0)
			nasbug = TRUE;
		else {
			if (strcmp(argv[i], "-help") == 0) {
				printf("\t-sound        Enable NAS sound support\n");
				printf("\t-squarewave   Generate sound with a square wave\n");
				printf("\t-sinewave     Generate sound with a sine wave\n");
				printf("\t-nasbug       Enable debug code in nas.c\n");
			}
			argv[j++] = argv[i];
		}
	}

	*argc = j;

	if (sound_enabled) {
		au_server = AuOpenServer(NULL, 0, NULL, 0, NULL, NULL);
		if (!au_server) {
			printf("Unable to open audio server\n");
			exit(1);
		}
		for (au_device = NULL, i = 0; i < AuServerNumDevices(au_server); i++) {
			if ((AuDeviceKind(AuServerDevice(au_server, i)) ==
				 AuComponentKindPhysicalOutput) &&
				AuDeviceNumTracks(AuServerDevice(au_server, i)) == 1) {
				au_device = (AuDeviceID *) AuDeviceIdentifier(AuServerDevice(au_server, i));
				break;
			}
		}

		au_channel[0] = AuCreateFlow(au_server, NULL);
		au_channel[1] = AuCreateFlow(au_server, NULL);
		au_channel[2] = AuCreateFlow(au_server, NULL);
		au_channel[3] = AuCreateFlow(au_server, NULL);

		for (i = 0; i < 4; i++) {
			AuElement elements[3];
			int freq = 0;
			int volume = AuFixedPointFromFraction(0, 100);

			AuMakeElementImportWaveForm(&elements[0], SAMPLE_RATE,
										waveform,
										AuUnlimitedSamples,
										freq,
										0, NULL);
			AuMakeElementMultiplyConstant(&elements[1], 0, volume);
			AuMakeElementExportDevice(&elements[2], 1, (int) au_device, SAMPLE_RATE,
									  AuUnlimitedSamples, 0, NULL);

			AuSetElements(au_server, au_channel[i], AuTrue, 3, elements, NULL);
			AuStartFlow(au_server, au_channel[i], NULL);
		}
	}
}

void NAS_Exit(void)
{
	if (sound_enabled) {
		int i;

		for (i = 0; i < 4; i++) {
			AuStopFlow(au_server, au_channel[i], NULL);
		}

		AuCloseServer(au_server);
	}
}

NAS_SetFrequency(int channel, int frequency)
{
	if (frequency < 8000) {
		static AuElementParameters parms;

		parms.flow = channel;
		parms.element_num = 0;
		parms.num_parameters = AuParmsImportWaveForm;
		parms.parameters[AuParmsImportWaveFormFrequency] = frequency;
		parms.parameters[AuParmsImportWaveFormNumSamples] = AuUnlimitedSamples;
		AuSetElementParameters(au_server, 1, &parms, NULL);
	}
}

NAS_SetVolume(int channel, int volume)
{
	static AuElementParameters parms;

	parms.flow = channel;
	parms.element_num = 1;
	parms.num_parameters = AuParmsMultiplyConstant;
	parms.parameters[AuParmsMultiplyConstantConstant] = volume;
	AuSetElementParameters(au_server, 1, &parms, NULL);
}

static int join12 = FALSE;
static int join34 = FALSE;

void NAS_UpdateSound(void)
{
	if (sound_enabled) {
		int i;

		for (i = 0; i < 4; i++) {
			int freqtmp;
			int flag = FALSE;

			if ((i < 2) && join12) {
				if (i == 1) {
					int N = ((AUDF[1] << 8) | AUDF[0]) + M[0];

					if (nasbug)
						printf("join12: N=%d\n", N);

					freqtmp = au_channel_clock[0] / (N + N);

					flag = TRUE;
				}
			}
			else if ((i >= 2) && join34) {
				if (i == 3) {
					int N = ((AUDF[3] << 8) | AUDF[2]) + M[2];

					if (nasbug)
						printf("join34: N=%d\n", N);

					freqtmp = au_channel_clock[2] / (N + N);

					flag = TRUE;
				}
			}
			else {
				int N = AUDF[i] + M[i];

				freqtmp = au_channel_clock[i] / (N + N);

				flag = TRUE;
			}

			if (flag) {
				int distortion = AUDC[i] & 0xf0;
				int voltmp;

				if ((distortion == 0xa0) || (distortion == 0xd0))
					voltmp = AuFixedPointFromFraction((AUDC[i] & 0x0f), 30);
				else
					voltmp = AuFixedPointFromFraction(0, 30);

				NAS_SetFrequency(au_channel[i], freqtmp);
				NAS_SetVolume(au_channel[i], voltmp);
			}
		}

		AuFlush(au_server);
	}
}

void Atari_AUDC(int channel, int byte)
{
	AUDC[channel - 1] = byte;
}

void Atari_AUDF(int channel, int byte)
{
	AUDF[channel - 1] = byte;
}

void Atari_AUDCTL(int byte)
{
	AUDCTL = byte;

	if (byte & 0x01) {
		au_channel_clock[0] = 15000;
		au_channel_clock[1] = 15000;
		au_channel_clock[2] = 15000;
		au_channel_clock[3] = 15000;
	}
	else {
		au_channel_clock[0] = 64000;
		au_channel_clock[1] = 64000;
		au_channel_clock[2] = 64000;
		au_channel_clock[3] = 64000;
	}

	M[0] = M[1] = M[2] = M[3] = 1;

	join34 = (byte & 0x08) ? TRUE : FALSE;	/* Clock channel 4 with channel 3 */
	join12 = (byte & 0x10) ? TRUE : FALSE;	/* Clock channel 2 with channel 1 */

	if (byte & 0x20) {
		au_channel_clock[2] = 1790000;	/* Clock channel 3 with 1.79 MHZ */
		M[2] = (join34) ? 7 : 4;
	}
	if (byte & 0x40) {
		au_channel_clock[0] = 1790000;	/* Clock channel 1 with 1.79 MHZ */
		M[0] = (join12) ? 7 : 4;
	}
	if (nasbug && (byte & 0xfe))
		printf("AUDCTL: %02x\n", byte);
}
