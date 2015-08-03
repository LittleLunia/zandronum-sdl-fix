/*  _______         ____    __         ___    ___
 * \    _  \       \    /  \  /       \   \  /   /       '   '  '
 *  |  | \  \       |  |    ||         |   \/   |         .      .
 *  |  |  |  |      |  |    ||         ||\  /|  |
 *  |  |  |  |      |  |    ||         || \/ |  |         '  '  '
 *  |  |  |  |      |  |    ||         ||    |  |         .      .
 *  |  |_/  /        \  \__//          ||    |  |
 * /_______/ynamic    \____/niversal  /__\  /____\usic   /|  .  . ibliotheque
 *                                                      /  \
 *                                                     / .  \
 * sterpan.c - The stereo pan (SPAN) signal type.     / / \  \
 *                                                   | <  /   \_
 * By entheh.                                        |  \/ /\   /
 *                                                    \_  /  > /
 * This takes a single monaural signal and              | \ / /
 * expands it to two channels, applying a               |  ' /
 * stereo pan in the process. The stereo pan             \__/
 * is generated by delaying and damping the
 * channel opposite the sound source. If only
 * one channel is requested of this signal, it will simply chain to the other
 * signal.
 *
 * In order for the delay to work properly, this must be played at 65536 Hz.
 * The pitch at which you want the sample to play can be passed in parameter
 * #1. Parameter #0 specifies the panning position, -256 to 256.
 *
 * NOTE: THIS IS NOT HOW IT WORKS AT THE MOMENT. AT THE MOMENT, THIS ROUTINE
 * SIMPLY VARIES THE VOLUMES.
 */

#include <stdlib.h>

#include "dumb.h"



#define SIGTYPE_STEREOPAN DUMB_ID('S','P','A','N')



#define SPANPARAM_PAN 0



typedef struct STEREOPAN_SIGNAL
{
	int sig;
}
STEREOPAN_SIGNAL;



typedef struct STEREOPAN_SAMPINFO
{
	float pan;
	int stereo;
	DUH_SIGNAL_SAMPINFO *csampinfo;
}
STEREOPAN_SAMPINFO;



static void *stereopan_load_signal(DUH *duh, DUMBFILE *file)
{
	STEREOPAN_SIGNAL *signal;

	(void)duh;

	signal = malloc(sizeof(*signal));

	if (!signal)
		return NULL;

	signal->sig = dumbfile_igetl(file);

	if (dumbfile_error(file)) {
		free(signal);
		return NULL;
	}

	return signal;
}



static void *stereopan_start_samples(DUH *duh, void *signal, int n_channels, long pos)
{
	STEREOPAN_SAMPINFO *sampinfo;

#define signal ((STEREOPAN_SIGNAL *)signal)

	if ((unsigned int)(n_channels - 1) >= 2) {
		TRACE("Stereo pan signal requiring 1 or 2 channels called with %d channels.\n", n_channels);
		return NULL;
	}

	sampinfo = malloc(sizeof(*sampinfo));
	if (!sampinfo)
		return NULL;

	sampinfo->pan = 0;

	sampinfo->stereo = n_channels - 1;

	sampinfo->csampinfo = duh_signal_start_samples(duh, signal->sig, 1, pos);
	if (!sampinfo->csampinfo) {
		free(sampinfo);
		return NULL;
	}

#undef signal

	return sampinfo;
}



static void stereopan_set_parameter(void *sampinfo, unsigned char id, long value)
{
#define sampinfo ((STEREOPAN_SAMPINFO *)sampinfo)

	if (id == SPANPARAM_PAN && value >= -256 && value <= 256)
		sampinfo->pan = value * (1.0f / 256.0f);

#undef sampinfo
}



static long stereopan_render_samples(
	void *sampinfo,
	float volume, float delta,
	long size, sample_t **samples
)
{
#define sampinfo ((STEREOPAN_SAMPINFO *)sampinfo)

	if (!sampinfo->stereo)
		return duh_signal_render_samples(sampinfo->csampinfo, volume, delta, size, samples);

	if (sampinfo->pan >= 0) {
		long sz = duh_signal_render_samples(sampinfo->csampinfo, volume * (1.0f + sampinfo->pan), delta, size, samples + 1);
		long s;
		int vol;

		volume = (1.0f - sampinfo->pan) / (1.0f + sampinfo->pan);
		vol = (int)(volume * 65536 + 0.5);

		for (s = 0; s < sz; s++)
			samples[0][s] = (samples[1][s] * vol) >> 16;

		return sz;
	} else {
		long sz = duh_signal_render_samples(sampinfo->csampinfo, volume * (1.0f - sampinfo->pan), delta, size, samples);
		long s;
		int vol;

		volume = (1.0f + sampinfo->pan) / (1.0f - sampinfo->pan);
		vol = (int)(volume * 65536 + 0.5);

		for (s = 0; s < sz; s++)
			samples[1][s] = (samples[0][s] * vol) >> 16;

		return sz;
	}

#undef sampinfo
}



static void stereopan_end_samples(void *sampinfo)
{
#define sampinfo ((STEREOPAN_SAMPINFO *)sampinfo)

	duh_signal_end_samples(sampinfo->csampinfo);
	free(sampinfo);

#undef sampinfo
}



static void stereopan_unload_signal(void *signal)
{
	free(signal);
}



static DUH_SIGTYPE_DESC sigtype_stereopan = {
	SIGTYPE_STEREOPAN,
	&stereopan_load_signal,
	&stereopan_start_samples,
	&stereopan_set_parameter,
	&stereopan_render_samples,
	&stereopan_end_samples,
	&stereopan_unload_signal
};



void dumb_register_sigtype_stereopan(void)
{
	dumb_register_sigtype(&sigtype_stereopan);
}
