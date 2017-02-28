#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
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
#define RESAMPLE_SIZE 31 /* width of sinc kernel (number of samples in each direction) */

#include "mic.c"

typedef struct {
	atomic_int index;
	real_t *samples;
	int16_t *out_samples;
	char *filename;
	size_t n_samples;
	int32_t sample_rate;
} param_t;

static real_t resample(real_t *data, size_t len, size_t base, real_t ds)
{
	real_t dsi = floor(ds), dsf = ds - dsi;
	real_t sin_dsf = sin(dsf * M_PI) / M_PI, acc = 0.0;
	ssize_t off = (ssize_t)dsi + base;

	if (sin_dsf == 0.0) {
		return off >= 0 && off < len ? data[off] : 0.0;
	}

	for (ssize_t i = -RESAMPLE_SIZE; i < RESAMPLE_SIZE; i++) {
		ssize_t oi = i + off;
		if (oi < 0 || oi >= len) {
			continue;
		}
		real_t sin_x = i % 2 ? -sin_dsf : sin_dsf;
		acc += data[oi] * (sin_x / ((real_t)i + dsf));
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

void *gen_thread(void *param_v)
{
	param_t *param = (param_t*)param_v;
	int val;

	while ((val = atomic_fetch_add(&param->index, 1)) < N_MICS) {
		printf("starting mic: %d\n", val);
		int16_t *out_samples = param->out_samples + param->n_samples * val;
		gen_delay(param->samples, param->n_samples, (real_t)(param->sample_rate), mic_pos[val], out_samples);
		write_file(param->filename, val, param->sample_rate, out_samples, param->n_samples);
		printf("finished: %d\n", val);
	}

	return NULL;
}

int main(int argc, char **argv)
{
	int n_threads = 1;
	pthread_t threads[N_MICS];
	param_t params;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <infile> [number of threads]\n", argv[0]);
		return 1;
	}

	if (argc >= 3) {
		n_threads = atoi(argv[2]);
		n_threads = n_threads < 1 ? 1 : n_threads > N_MICS ? N_MICS : n_threads;
	}

	wav_t wav;
	size_t n_samples;
	real_t *samples = wav_read_mono_16(argv[1], &wav, &n_samples);
	if (samples == NULL) {
		return 1;
	}

	int16_t *out_samples = (int16_t*)malloc(n_samples * sizeof(int16_t) * N_MICS);
	if (out_samples == NULL) {
		fprintf(stderr, "can't allocate space for output\n");
		return 1;
	}

	printf("%s: rate %d, %lu samples\n", argv[1], wav.rate, n_samples);
	printf("using %d threads\n", n_threads);

	params.index = 0;
	params.samples = samples;
	params.out_samples = out_samples;
	params.filename = argv[1];
	params.n_samples = n_samples;
	params.sample_rate = wav.rate;

	memset(threads, 0, sizeof(threads));
	for (int i = 0; i < n_threads; i++) {
		pthread_create(&threads[i], NULL, gen_thread, &params);
	}
	for (int i = 0; i < n_threads; i++) {
		pthread_join(threads[i], NULL);
	}

	return 0;
}
