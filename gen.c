#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "globals.h"
#include "liss.h"
#include "vector.h"
#include "wav.h"

#define SND_SPEED 343.0 /* m/s */
#define BASELINE_DIST 5.0 /* distance at which null mic is capturing this sound. used for amplitude adjust */
#define RESAMPLE_SIZE 3

#define SINC_EPSILON 0.0001

static vec3_t mic_pos[] = {
	{ .x = -0.5, .y = -RSQRT_3/2.0, .z = 0.0 },
	{ .x =  0.5, .y = -RSQRT_3/2.0, .z = 0.0 },
	{ .x =  0.0, .y =  RSQRT_3/1.0, .z = 0.0 },
};
static const size_t N_MICS = ARRAY_SIZE(mic_pos);

static inline real_t sinc(real_t x)
{
	if (x > -SINC_EPSILON && x < SINC_EPSILON) {
		return 1.0;
	}

	real_t rx = x * M_PI;
	return sin(rx) / rx;
}

static real_t resample(real_t *data, size_t len, size_t base, real_t ds)
{
	real_t dsi = floor(ds), dsf = ds - dsi;
	ssize_t off = (ssize_t)dsi + base;
	ssize_t i;

	real_t acc = 0.0;
	for (i = -RESAMPLE_SIZE; i < RESAMPLE_SIZE; i++) {
		ssize_t oi = i + off;
		if (oi < 0 || oi >= len) {
			continue;
		}
		acc += data[oi] * sinc((real_t)i + dsf);
	}
	return acc;
}

static void gen_delay(real_t *data, size_t len, real_t rate, vec3_t mic_pos, int16_t *res)
{
	size_t i;
	real_t irate = 1.0 / rate;

	for (i = 0; i < len; i++) {
		vec3_t source_pos = liss_pos((real_t)i * irate);
		real_t d0 = vec3_dist(source_pos, vec3_zero);
		real_t d1 = vec3_dist(source_pos, mic_pos);
		real_t dl = d0 - d1;
		real_t amp = (BASELINE_DIST * BASELINE_DIST) / ((dl + BASELINE_DIST) * (dl + BASELINE_DIST));
		real_t sample = amp * resample(data, len, i, dl / SND_SPEED * rate);
		int32_t isample = (int32_t)(sample * ((int32_t)INT16_MAX + 1));
		res[i] = isample > INT16_MAX ? INT16_MAX : (isample < INT16_MIN ? INT16_MIN : (int16_t)isample);
	}
}

static void write_file(char *orig_fname, size_t num, int32_t rate, int16_t *data, size_t len)
{
	char *fname = strdup(orig_fname);
	char buf[256];

	/* truncate extension. I hope. */
	fname[strlen(fname) - 4] = '\0';
	snprintf(buf, 256, "%s.%lu.wav", fname, num);
	free(fname);

	wav_write_mono_16(buf, rate, data, len);
	printf("%s written\n", buf);
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s <infile>\n", argv[0]);
		return 1;
	}

	wav_t wav;
	size_t n_samples;
	real_t *samples = wav_read_mono_16(argv[1], &wav, &n_samples);
	if (samples == NULL) {
		return 1;
	}

	int16_t *out_samples = (int16_t*)malloc(n_samples * 2);
	if (out_samples == NULL) {
		fprintf(stderr, "can't allocate space for output\n");
		return 1;
	}

	printf("%s: rate %d, %lu samples\n", argv[1], wav.rate, n_samples);
	for (size_t i = 0; i < N_MICS; i++) {
		printf("mic: %lu\n", i);
		gen_delay(samples, n_samples, (real_t)(wav.rate), mic_pos[i], out_samples);
		write_file(argv[1], i, wav.rate, out_samples, n_samples);
	}
}
