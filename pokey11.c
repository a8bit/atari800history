/*****************************************************************************/
/*                                                                           */
/* Module:  POKEY Chip Simulator, V1.1                                       */
/* Purpose: To emulate the sound generation hardware of the Atari POKEY chip. */
/* Author:  Ron Fries                                                        */
/* Date:    September 22, 1996                                               */
/*                                                                           */
/*****************************************************************************/
/*                                                                           */
/*                 License Information and Copyright Notice                  */
/*                 ========================================                  */
/*                                                                           */
/* PokeySound is Copyright(c) 1996 by Ron Fries                              */
/*                                                                           */
/* This library is free software; you can redistribute it and/or modify it   */
/* under the terms of version 2 of the GNU Library General Public License    */
/* as published by the Free Software Foundation.                             */
/*                                                                           */
/* This library is distributed in the hope that it will be useful, but       */
/* WITHOUT ANY WARRANTY; without even the implied warranty of                */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library */
/* General Public License for more details.                                  */
/* To obtain a copy of the GNU Library General Public License, write to the  */
/* Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.   */
/*                                                                           */
/* Any permitted reproduction of these routines, in whole or in part, must   */
/* bear this legend.                                                         */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "pokey11.h"

/* CONSTANT DEFINITIONS */

/* definitions for AUDCx (D201, D203, D205, D207) */
#define NOTPOLY5    0x80		/* selects POLY5 or direct CLOCK */
#define POLY4       0x40		/* selects POLY4 or POLY17 */
#define PURE        0x20		/* selects POLY4/17 or PURE tone */
#define VOL_ONLY    0x10		/* selects VOLUME OUTPUT ONLY */
#define VOLUME_MASK 0x0f		/* volume mask */

/* definitions for AUDCTL (D208) */
#define POLY9       0x80		/* selects POLY9 or POLY17 */
#define CH1_179     0x40		/* selects 1.78979 MHz for Ch 1 */
#define CH3_179     0x20		/* selects 1.78979 MHz for Ch 3 */
#define CH1_CH2     0x10		/* clocks channel 1 w/channel 2 */
#define CH3_CH4     0x08		/* clocks channel 3 w/channel 4 */
#define CH1_FILTER  0x04		/* selects channel 1 high pass filter */
#define CH2_FILTER  0x02		/* selects channel 2 high pass filter */
#define CLOCK_15    0x01		/* selects 15.6999kHz or 63.9210kHz */

#define AUDF1_C     0xd200
#define AUDC1_C     0xd201
#define AUDF2_C     0xd202
#define AUDC2_C     0xd203
#define AUDF3_C     0xd204
#define AUDC3_C     0xd205
#define AUDF4_C     0xd206
#define AUDC4_C     0xd207
#define AUDCTL_C    0xd208

/* for accuracy, the 64kHz and 15kHz clocks are exact divisions of
   the 1.79MHz clock */
#define DIV_64      28			/* divisor for 1.79MHz clock to 64 kHz */
#define DIV_15      114			/* divisor for 1.79MHz clock to 15 kHz */

/* the size (in entries) of the 4 polynomial tables */
#define POLY4_SIZE  0x000f
#define POLY5_SIZE  0x001f
#define POLY9_SIZE  0x01ff

#ifdef COMP16					/* if 16-bit compiler */
#define POLY17_SIZE 0x00007fffL	/* reduced to 15 bits for simplicity */
#else							/*  */
#define POLY17_SIZE 0x0001ffffL	/* else use the full 17 bits */
#endif							/*  */

/* channel definitions */
#define CHAN1       0
#define CHAN2       1
#define CHAN3       2
#define CHAN4       3
#define SAMPLE      4


#define FALSE       0
#define TRUE        1


/* GLOBAL VARIABLE DEFINITIONS */

/* structures to hold the 9 pokey control bytes */
static uint8 AUDF[4];			/* AUDFx (D200, D202, D204, D206) */

static uint8 AUDC[4];			/* AUDCx (D201, D203, D205, D207) */

static uint8 AUDCTL;			/* AUDCTL (D208) */


static uint8 Outbit[4];			/* current state of the output (high or low) */


static uint8 Outvol[4];			/* last output volume for each channel */



/* Initialze the bit patterns for the polynomials. */

/* The 4bit and 5bit patterns are the identical ones used in the pokey chip. */
/* Though the patterns could be packed with 8 bits per byte, using only a */
/* single bit per byte keeps the math simple, which is important for */
/* efficient processing. */

static uint8 bit4[POLY4_SIZE] =
{1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0};


static uint8 bit5[POLY5_SIZE] =
{0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1};


static uint8 bit17[POLY17_SIZE];	/* Rather than have a table with 131071 */

							/* entries, I use a random number generator. */
							/* It shouldn't make much difference since */
							/* the pattern rarely repeats anyway. */

static uint32 Poly17_size;		/* global for the poly 17 size, since it can */

						   /* be changed from 17 bit to 9 bit */

static uint32 Poly_adjust;		/* the amount that the polynomial will need */

						   /* to be adjusted to process the next bit */

static uint32 P4 = 0,			/* Global position pointer for the 4-bit  POLY array */
 P5 = 0,						/* Global position pointer for the 5-bit  POLY array */
 P17 = 0;						/* Global position pointer for the 17-bit POLY array */


static uint32 Div_n_cnt[4],		/* Divide by n counter. one for each channel */
 Div_n_max[4];					/* Divide by n maximum, one for each channel */


static uint32 Samp_n_max,		/* Sample max.  For accuracy, it is *256 */
 Samp_n_cnt[2];					/* Sample cnt. */


static uint32 Base_mult;		/* selects either 64Khz or 15Khz clock mult */


/*****************************************************************************/
/* In my routines, I treat the sample output as another divide by N counter  */
/* For better accuracy, the Samp_n_cnt has a fixed binary decimal point      */
/* which has 8 binary digits to the right of the decimal point.  I use a two */
/* byte array to give me a minimum of 40 bits, and then use pointer math to  */
/* reference either the 24.8 whole/fraction combination or the 32-bit whole  */
/* only number.  This is mainly used to keep the math simple for             */
/* optimization. See below:                                                  */
/*                                                                           */
/* xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx | xxxxxxxx xxxxxxxx xxxxxxxx.xxxxxxxx */
/*  unused   unused   unused    whole      whole    whole    whole  fraction */
/*                                                                           */
/* Samp_n_cnt[0] gives me a 32-bit int 24 whole bits with 8 fractional bits, */
/* while (uint32 *)((uint8 *)(&Samp_n_cnt[0])+1) gives me the 32-bit whole   */
/* number only.                                                              */
/*****************************************************************************/


/*****************************************************************************/
/* Module:  Pokey_sound_init()                                               */
/* Purpose: to handle the power-up initialization functions                  */
/*          these functions should only be executed on a cold-restart        */
/*                                                                           */
/* Author:  Ron Fries                                                        */
/* Date:    September 22, 1996                                               */
/*                                                                           */
/* Inputs:  freq17 - the value for the '1.79MHz' Pokey audio clock           */
/*          playback_freq - the playback frequency in samples per second     */
/*                                                                           */
/* Outputs: Adjusts local globals - no return value                          */
/*                                                                           */
/*****************************************************************************/

void Pokey_sound_init(uint32 freq17, uint16 playback_freq)
{

	uint8 chan;

	int32 n;


	/* fill the 17bit polynomial with random bits */
	for (n = 0; n < POLY17_SIZE; n++) {

		bit17[n] = rand() & 0x01;	/* fill poly 17 with random bits */

	}


	/* start all of the polynomial counters at zero */
	Poly_adjust = 0;

	P4 = 0;

	P5 = 0;

	P17 = 0;


	/* calculate the sample 'divide by N' value based on the playback freq. */
	Samp_n_max = ((uint32) freq17 << 8) / playback_freq;


	Samp_n_cnt[0] = 0;			/* initialize all bits of the sample */

	Samp_n_cnt[1] = 0;			/* 'divide by N' counter */


	Poly17_size = POLY17_SIZE;


	for (chan = CHAN1; chan <= CHAN4; chan++) {

		Outvol[chan] = 0;

		Outbit[chan] = 0;

		Div_n_cnt[chan] = 0;

		Div_n_max[chan] = 0x7fffffffL;

		AUDC[chan] = 0;

		AUDF[chan] = 0;

	}


	AUDCTL = 0;


	Base_mult = DIV_64;

}



/*****************************************************************************/
/* Module:  Update_pokey_sound()                                             */
/* Purpose: To process the latest control values stored in the AUDF, AUDC,   */
/*          and AUDCTL registers.  It pre-calculates as much information as  */
/*          possible for better performance.  This routine has not been      */
/*          optimized.                                                       */
/*                                                                           */
/* Author:  Ron Fries                                                        */
/* Date:    September 22, 1996                                               */
/*                                                                           */
/* Inputs:  addr - the address of the parameter to be changed                */
/*          val - the new value to be placed in the specified address        */
/*                                                                           */
/* Outputs: Adjusts local globals - no return value                          */
/*                                                                           */
/*****************************************************************************/

void Update_pokey_sound(uint16 addr, uint8 val)
{

	uint32 new_val = 0;

	uint8 chan;

	uint8 chan_mask;


	/* determine which address was changed */
	switch (addr) {

	case AUDF1_C:

		AUDF[CHAN1] = val;

		chan_mask = 1 << CHAN1;


		if (AUDCTL & CH1_CH2)	/* if ch 1&2 tied together */
			chan_mask |= 1 << CHAN2;	/* then also change on ch2 */

		break;


	case AUDC1_C:

		AUDC[CHAN1] = val;

		chan_mask = 1 << CHAN1;

		break;


	case AUDF2_C:

		AUDF[CHAN2] = val;

		chan_mask = 1 << CHAN2;

		break;


	case AUDC2_C:

		AUDC[CHAN2] = val;

		chan_mask = 1 << CHAN2;

		break;


	case AUDF3_C:

		AUDF[CHAN3] = val;

		chan_mask = 1 << CHAN3;


		if (AUDCTL & CH3_CH4)	/* if ch 3&4 tied together */
			chan_mask |= 1 << CHAN4;	/* then also change on ch4 */

		break;


	case AUDC3_C:

		AUDC[CHAN3] = val;

		chan_mask = 1 << CHAN3;

		break;


	case AUDF4_C:

		AUDF[CHAN4] = val;

		chan_mask = 1 << CHAN4;

		break;


	case AUDC4_C:

		AUDC[CHAN4] = val;

		chan_mask = 1 << CHAN4;

		break;


	case AUDCTL_C:

		AUDCTL = val;

		chan_mask = 15;			/* all channels */


		/* set poly17 counter to 9- or 17-bit */
		if (AUDCTL & POLY9)
			Poly17_size = POLY9_SIZE;

		else
			Poly17_size = POLY17_SIZE;


		/* determine the base multiplier for the 'div by n' calculations */
		if (AUDCTL & CLOCK_15)
			Base_mult = DIV_15;

		else
			Base_mult = DIV_64;


		break;


	default:

		chan_mask = 0;

		break;

	}


/************************************************************/
	/* As defined in the manual, the exact Div_n_cnt values are */
	/* different depending on the frequency and resolution:     */
	/*    64 kHz or 15 kHz - AUDF + 1                           */
	/*    1 MHz, 8-bit -     AUDF + 4                           */
	/*    1 MHz, 16-bit -    AUDF[CHAN1]+256*AUDF[CHAN2] + 7    */
/************************************************************/

	/* only reset the channels that have changed */

	if (chan_mask & (1 << CHAN1)) {

		/* process channel 1 frequency */
		if (AUDCTL & CH1_179)
			new_val = AUDF[CHAN1] + 4;

		else
			new_val = (AUDF[CHAN1] + 1) * Base_mult;


		if (new_val != Div_n_max[CHAN1]) {

			Div_n_max[CHAN1] = new_val;

			Div_n_cnt[CHAN1] = 0;

		}

	}


	if (chan_mask & (1 << CHAN2)) {

		/* process channel 2 frequency */
		if (AUDCTL & CH1_CH2)
			if (AUDCTL & CH1_179)
				new_val = AUDF[CHAN2] * 256 + AUDF[CHAN1] + 7;

			else
				new_val = (AUDF[CHAN2] * 256 + AUDF[CHAN1] + 1) * Base_mult;

		else
			new_val = (AUDF[CHAN2] + 1) * Base_mult;


		if (new_val != Div_n_max[CHAN2]) {

			Div_n_max[CHAN2] = new_val;

			Div_n_cnt[CHAN2] = 0;

		}

	}


	if (chan_mask & (1 << CHAN3)) {

		/* process channel 3 frequency */
		if (AUDCTL & CH3_179)
			new_val = AUDF[CHAN3] + 4;

		else
			new_val = (AUDF[CHAN3] + 1) * Base_mult;


		if (new_val != Div_n_max[CHAN3]) {

			Div_n_max[CHAN3] = new_val;

			Div_n_cnt[CHAN3] = 0;

		}

	}


	if (chan_mask & (1 << CHAN4)) {

		/* process channel 4 frequency */
		if (AUDCTL & CH3_CH4)
			if (AUDCTL & CH3_179)
				new_val = AUDF[CHAN4] * 256 + AUDF[CHAN3] + 7;

			else
				new_val = (AUDF[CHAN4] * 256 + AUDF[CHAN3] + 1) * Base_mult;

		else
			new_val = (AUDF[CHAN4] + 1) * Base_mult;


		if (new_val != Div_n_max[CHAN4]) {

			Div_n_max[CHAN4] = new_val;

			Div_n_cnt[CHAN4] = 0;

		}

	}


	/* if channel is volume only, set current output */
	for (chan = CHAN1; chan <= CHAN4; chan++) {

		if (chan_mask & (1 << chan)) {

			/* I've disabled any frequencies that exceed the sampling
			   frequency.  There isn't much point in processing frequencies
			   that the hardware can't reproduce.  I've also disabled 
			   processing if the volume is zero. */

			/* if the channel is volume only */
			/* or the channel is off (volume == 0) */
			/* or the channel freq is greater than the playback freq */
			if ((AUDC[chan] & VOL_ONLY) ||
				((AUDC[chan] & VOLUME_MASK) == 0) ||
				(Div_n_max[chan] < (Samp_n_max >> 8))) {

				/* then set the channel to the selected volume */
				Outvol[chan] = AUDC[chan] & VOLUME_MASK;

				/* and set channel freq to max to reduce processing */
				Div_n_max[chan] = 0x7fffffffL;

			}

		}

	}

}



/*****************************************************************************/
/* Module:  Pokey_process_2()                                                */
/* Purpose: To fill the output buffer with the sound output based on the     */
/*          pokey chip parameters.  This routine has not been optimized.     */
/*          Though it is not used by the program, I've left it for reference. */
/*                                                                           */
/* Author:  Ron Fries                                                        */
/* Date:    September 22, 1996                                               */
/*                                                                           */
/* Inputs:  *buffer - pointer to the buffer where the audio output will      */
/*                    be placed                                              */
/*          n - size of the playback buffer                                  */
/*                                                                           */
/* Outputs: the buffer will be filled with n bytes of audio - no return val  */
/*                                                                           */
/*****************************************************************************/

void Pokey_process_2(register unsigned char *buffer, register uint16 n)
{

	register uint32 *samp_cnt_w_ptr;

	register uint32 event_min;

	register uint8 next_event;

	register uint8 cur_val;

	register uint8 chan;


	/* set a pointer to the whole portion of the samp_n_cnt */
	samp_cnt_w_ptr = (uint32 *) ((uint8 *) (&Samp_n_cnt[0]) + 1);


	/* loop until the buffer is filled */
	while (n) {

		/* Normally the routine would simply decrement the 'div by N' */
		/* counters and react when they reach zero.  Since we normally */
		/* won't be processing except once every 80 or so counts, */
		/* I've optimized by finding the smallest count and then */
		/* 'accelerated' time by adjusting all pointers by that amount. */

		/* find next smallest event (either sample or chan 1-4) */
		next_event = SAMPLE;

		event_min = *samp_cnt_w_ptr;


		for (chan = CHAN1; chan <= CHAN4; chan++) {

			if (Div_n_cnt[chan] <= event_min) {

				event_min = Div_n_cnt[chan];

				next_event = chan;

			}

		}



		/* decrement all counters by the smallest count found */
		for (chan = CHAN1; chan <= CHAN4; chan++) {

			Div_n_cnt[chan] -= event_min;

		}


		*samp_cnt_w_ptr -= event_min;


		/* since the polynomials require a mod (%) function which is 
		   division, I don't adjust the polynomials on the SAMPLE events,
		   only the CHAN events.  I have to keep track of the change,
		   though. */
		Poly_adjust += event_min;


		/* if the next event is a channel change */
		if (next_event != SAMPLE) {

			/* shift the polynomial counters */
			P4 = (P4 + Poly_adjust) % POLY4_SIZE;

			P5 = (P5 + Poly_adjust) % POLY5_SIZE;

			P17 = (P17 + Poly_adjust) % Poly17_size;


			/* reset the polynomial adjust counter to zero */
			Poly_adjust = 0;


			/* adjust channel counter */
			Div_n_cnt[next_event] += Div_n_max[next_event];


			/* From here, a good understanding of the hardware is required */
			/* to understand what is happening.  I won't be able to provide */
			/* much description to explain it here. */

			/* if the output is pure or the output is poly5 and the poly5 bit */
			/* is set */
			if ((AUDC[next_event] & NOTPOLY5) || bit5[P5]) {

				/* if the PURE bit is set */
				if (AUDC[next_event] & PURE) {

					/* then simply toggle the output */
					Outbit[next_event] = !Outbit[next_event];

				}

				/* otherwise if POLY4 is selected */
				else if (AUDC[next_event] & POLY4) {

					/* then use the poly4 bit */
					Outbit[next_event] = bit4[P4];

				}

				else {

					/* otherwise use the poly17 bit */
					Outbit[next_event] = bit17[P17];

				}

			}


			/* At this point I haven't emulated the filters.  Though I don't
			   expect it to be complicated, I don't believe this feature is
			   used much anyway.  I'll work on it later. */
			if ((next_event == CHAN1) || (next_event == CHAN3)) {

				/* INSERT FILTER HERE */
			}


			/* if the current output bit is set */
			if (Outbit[next_event]) {

				/* then set to the current volume */
				Outvol[next_event] = AUDC[next_event] & VOLUME_MASK;

			}

			else {

				/* set the volume to zero */
				Outvol[next_event] = 0;

			}

		}

		else {					/* otherwise we're processing a sample */

			/* adjust the sample counter - note we're using the 24.8 integer
			   which includes an 8 bit fraction for accuracy */
			*Samp_n_cnt += Samp_n_max;


			cur_val = 0;


			/* add the output values of all 4 channels */
			for (chan = CHAN1; chan <= CHAN4; chan++) {

				cur_val += Outvol[chan];

			}


			/* multiply the volume by 4 and add 8 to center around 128 */
			/* NOTE: this statement could be eliminated for efficiency, */
			/* though the volume would be lower. */
			cur_val = (cur_val << 2) + 8;


			/* add the current value to the output buffer */
			*buffer++ = cur_val;


			/* and indicate one less byte in the buffer */
			n--;

		}

	}

}



/*****************************************************************************/
/* Module:  Pokey_process()                                                  */
/* Purpose: To fill the output buffer with the sound output based on the     */
/*          pokey chip parameters.  This routine has not been optimized.     */
/*          Though it is not used by the program, I've left it for reference. */
/*                                                                           */
/* Author:  Ron Fries                                                        */
/* Date:    September 22, 1996                                               */
/*                                                                           */
/* Inputs:  *buffer - pointer to the buffer where the audio output will      */
/*                    be placed                                              */
/*          n - size of the playback buffer                                  */
/*                                                                           */
/* Outputs: the buffer will be filled with n bytes of audio - no return val  */
/*                                                                           */
/*****************************************************************************/

void Pokey_process(register unsigned char *buffer, register uint16 n)
{

	register uint32 *div_n_ptr;

	register uint32 *samp_cnt_w_ptr;

	register uint32 event_min;

	register uint8 next_event;

	register uint8 cur_val;

	register uint8 *out_ptr;

	register uint8 audc;

	register uint8 toggle;



	/* set a pointer to the whole portion of the samp_n_cnt */
	samp_cnt_w_ptr = (uint32 *) ((uint8 *) (&Samp_n_cnt[0]) + 1);


	/* set a pointer for optimization */
	out_ptr = Outvol;


	/* The current output is pre-determined and then adjusted based on each */
	/* output change for increased performance (less over-all math). */
	/* add the output values of all 4 channels */
	cur_val = 2;				/* start with a small offset */

	cur_val += *out_ptr++;

	cur_val += *out_ptr++;

	cur_val += *out_ptr++;

	cur_val += *out_ptr++;


	/* loop until the buffer is filled */
	while (n) {

		/* Normally the routine would simply decrement the 'div by N' */
		/* counters and react when they reach zero.  Since we normally */
		/* won't be processing except once every 80 or so counts, */
		/* I've optimized by finding the smallest count and then */
		/* 'accelerated' time by adjusting all pointers by that amount. */

		/* find next smallest event (either sample or chan 1-4) */
		next_event = SAMPLE;

		event_min = *samp_cnt_w_ptr;


		/* Though I could have used a loop here, this is faster */
		div_n_ptr = Div_n_cnt;

		if (*div_n_ptr <= event_min) {

			event_min = *div_n_ptr;

			next_event = CHAN1;

		}

		div_n_ptr++;

		if (*div_n_ptr <= event_min) {

			event_min = *div_n_ptr;

			next_event = CHAN2;

		}

		div_n_ptr++;

		if (*div_n_ptr <= event_min) {

			event_min = *div_n_ptr;

			next_event = CHAN3;

		}

		div_n_ptr++;

		if (*div_n_ptr <= event_min) {

			event_min = *div_n_ptr;

			next_event = CHAN4;

		}


		/* decrement all counters by the smallest count found */
		/* again, no loop for efficiency */
		*div_n_ptr -= event_min;

		div_n_ptr--;

		*div_n_ptr -= event_min;

		div_n_ptr--;

		*div_n_ptr -= event_min;

		div_n_ptr--;

		*div_n_ptr -= event_min;


		*samp_cnt_w_ptr -= event_min;


		/* since the polynomials require a mod (%) function which is 
		   division, I don't adjust the polynomials on the SAMPLE events,
		   only the CHAN events.  I have to keep track of the change,
		   though. */
		Poly_adjust += event_min;


		/* if the next event is a channel change */
		if (next_event != SAMPLE) {

			/* shift the polynomial counters */
			P4 = (P4 + Poly_adjust) % POLY4_SIZE;

			P5 = (P5 + Poly_adjust) % POLY5_SIZE;

			P17 = (P17 + Poly_adjust) % Poly17_size;


			/* reset the polynomial adjust counter to zero */
			Poly_adjust = 0;


			/* adjust channel counter */
			Div_n_cnt[next_event] += Div_n_max[next_event];


			/* get the current AUDC into a register (for optimization) */
			audc = AUDC[next_event];


			/* set a pointer to the current output (for opt...) */
			out_ptr = &Outvol[next_event];


			/* assume no changes to the output */
			toggle = FALSE;


			/* From here, a good understanding of the hardware is required */
			/* to understand what is happening.  I won't be able to provide */
			/* much description to explain it here. */

			/* if the output is pure or the output is poly5 and the poly5 bit */
			/* is set */
			if ((audc & NOTPOLY5) || bit5[P5]) {

				/* if the PURE bit is set */
				if (audc & PURE) {

					/* then simply toggle the output */
					toggle = TRUE;

				}

				/* otherwise if POLY4 is selected */
				else if (audc & POLY4) {

					/* then compare to the poly4 bit */
					toggle = (bit4[P4] == !(*out_ptr));

				}

				else {

					/* otherwise compare to the poly17 bit */
					toggle = (bit17[P17] == !(*out_ptr));

				}

			}


			/* At this point I haven't emulated the filters.  Though I don't
			   expect it to be complicated, I don't believe this feature is
			   used much anyway.  I'll work on it later. */
			if ((next_event == CHAN1) || (next_event == CHAN3)) {

				/* INSERT FILTER HERE */
			}


			/* if the current output bit has changed */
			if (toggle) {

				if (*out_ptr) {

					/* remove this channel from the signal */
					cur_val -= *out_ptr;


					/* and turn the output off */
					*out_ptr = 0;

				}

				else {

					/* turn the output on */
					*out_ptr = audc & VOLUME_MASK;


					/* and add it to the output signal */
					cur_val += *out_ptr;

				}

			}

		}

		else {					/* otherwise we're processing a sample */

			/* adjust the sample counter - note we're using the 24.8 integer
			   which includes an 8 bit fraction for accuracy */
			*Samp_n_cnt += Samp_n_max;


			/* add the current value to the output buffer */
			*buffer++ = cur_val << 2;


			/* and indicate one less byte in the buffer */
			n--;

		}

	}

}
