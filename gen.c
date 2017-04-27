/** @file gen.c
 *  @brief Program to generate simulated audio streams for `view`.
 */

#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "globals.h"
#include "liss.h"
#include "vector.h"
#include "wav.h"

#define BASELINE_DIST 5.0 /* distance associated with base input stream, used for amplitude adjust */
#define RESAMPLE_SIZE 31 /* width of sinc kernel (number of samples in each direction) */

#include "mic.c"

struct {
	atomic_int index;
	int n_streams;
	real_t **streams;
	real_t *out_acc;
	int16_t *out_samples;
	char *file_prefix;
	size_t n_samples;
	int32_t sample_rate;
} param;

/* input file shit */
int input_n_files;
real_t **input_streams;

/** @brief Generates an interpolated sample at a given base plus delay
 *  @param data Audio input data to sample
 *  @param len Length of input data
 *  @param base Base sample offset
 *  @param ds Delay from base to sample at (in samples)
 *  @return The interpolated sample at data[base + ds]
 *
 *  Uses a rectangular windowed sinc with RESAMPLE_SIZE * 2 samples. Maybe it
 *  should be windowed better but in practice it doesn't matter much when the
 *  kernel is wide enough. It's not for audiophiles anyway.
 */
static real_t resample(real_t *data, size_t len, size_t base, real_t ds)
{
	real_t dsi = floor(ds), dsf = dsi + 1.0 - ds;
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

/** @brief Generates a varying-delay audio stream for a given microphone
 *  @param data Input audio data
 *  @param len Length of input data
 *  @param rate Sample rate, in Hz
 *  @param liss_idx Index of lissajous path parameters to use
 *  @param mic_pos Simulated microphone position
 *  @param res Result; generated samples will be accumulated here
 *
 *  Simulates a sound source moving in a lissajous trajectory (see liss.c) and
 *  emitting the given audio data being recorded by a microphone at the given
 *  position.
 */
static void gen_delay(real_t *data, size_t len, real_t rate, int liss_idx,
                      vec3_t mic_pos, real_t *res)
{
	real_t irate = 1.0 / rate;

	for (size_t i = 0; i < len; i++) {
		vec3_t source_pos = liss_pos((real_t)i * irate, liss_idx);
		real_t d0 = vec3_dist(source_pos, vec3_zero);
		real_t d1 = vec3_dist(source_pos, mic_pos);
		real_t dl = d0 - d1;

		/* sample and adjust amplitude: inverse linear, not inverse square */
		real_t amp = BASELINE_DIST / (dl + BASELINE_DIST);
		res[i] += amp * resample(data, len, i, dl / SND_SPEED * rate);
	}
}

/** @brief Loads files into separate audio streams and extends them to be of equal length
 *  @param n_files Number of files to load
 *  @param fnames Filenames to load
 *  @param n_samples_out Output; number of samples will be stored here
 *  @param sample_rate_out Output; sample rate will be stored here
 */
static real_t **load_files(int n_files, char **fnames, size_t *n_samples_out, int32_t *sample_rate_out)
{
	size_t max_len = 0;
	int32_t wav_rate, sample_rate = 0;

	real_t **streams = calloc(n_files, sizeof(streams[0]));
	size_t *stream_len = calloc(n_files, sizeof(stream_len[0]));
	if (streams == NULL || stream_len == NULL) {
		goto fail_alloc;
	}

	/* load all files and track max stream length */
	for (int i = 0; i < n_files; i++) {
		streams[i] = wav_read_mono_16(fnames[i], &wav_rate, &stream_len[i]);
		if (streams[i] == NULL) {
			goto fail_load;
		}

		/* sample rates must be the same for all input files */
		printf("%s: rate %d, %lu samples\n", fnames[i], wav_rate, stream_len[i]);
		if (sample_rate && sample_rate != wav_rate) {
			fprintf(stderr, "sample rate mismatch: current %d <> previous %d\n", wav_rate, sample_rate);
			goto fail_load;
		}

		sample_rate = wav_rate;
		max_len = stream_len[i] > max_len ? stream_len[i] : max_len;
	}

	/* extend shorter streams to maximum length */
	for (int i = 0; i < n_files; i++) {
		size_t len = stream_len[i];
		if (len < max_len) {
			streams[i] = realloc(streams[i], max_len * sizeof(streams[i][0]));
			if (streams[i] == NULL) {
				fprintf(stderr, "extending stream %d failed\n", i);
				goto fail_load;
			}

			for (size_t pos = len; pos < max_len; pos += len) {
				size_t to_copy = len > (max_len - pos) ? max_len - pos : len;
				memcpy(streams[i] + pos, streams[i], to_copy * sizeof(streams[i][0]));
			}
		}
	}

	*n_samples_out = max_len;
	*sample_rate_out = sample_rate;
	free(stream_len);
	return streams;

fail_load:
	for (int i = 0; i < n_files; i++) {
		free(streams[i]);
	}
fail_alloc:
	free(streams);
	free(stream_len);
	return NULL;
}

/** @brief Writes a sound stream to a file
 *  @param file_prefix Filename prefix; output filenames will be f_prefix.number.wav
 *  @param num Number of file
 *  @param rate Sample rate, in Hz
 *  @param data Audio samples to write
 *  @param len Length of data
 */
static void write_file(char *file_prefix, size_t num, int32_t rate, int16_t *data, size_t len)
{
	char buf[256];
	snprintf(buf, 256, "%s.%lu.wav", file_prefix, num);
	wav_write_mono_16(buf, rate, data, len);
	printf("%s written\n", buf);
}

void *gen_thread(void *id_v)
{
	size_t n_samples = param.n_samples;
	int index, thr_id = (intptr_t)id_v;
	real_t istreams = 1.0 / (real_t)param.n_streams;

	while ((index = atomic_fetch_add(&param.index, 1)) < N_MICS) {
		int16_t *out_samples = param.out_samples + n_samples * thr_id;
		real_t *out_acc = param.out_acc + n_samples * thr_id;

		/* accumulate output streams */
		printf("starting mic: %d\n", index);
		memset(out_acc, 0, n_samples * sizeof(out_acc[0]));
		for (int i = 0; i < param.n_streams; i++) {
			gen_delay(param.streams[i], n_samples, (real_t)(param.sample_rate),
			          i, mic_pos[index], out_acc);
		}

		/* scale to 16-bit int, round, and clamp sample */
		for (size_t i = 0; i < n_samples; i++) {
			int32_t isample = (int32_t)round(out_acc[i] * istreams * ((int32_t)INT16_MAX + 1));
			out_samples[i] = isample > INT16_MAX ? INT16_MAX : 
			                 isample < INT16_MIN ? INT16_MIN :
			                 (int16_t)isample;
		}

		write_file(param.file_prefix, index, param.sample_rate, out_samples, n_samples);
		printf("finished: %d\n", index);
	}

	return NULL;
}

int main(int argc, char **argv)
{
	pthread_t threads[N_MICS];
	size_t n_samples;
	int32_t sample_rate;
	int n_threads, n_streams = argc - 2;

	if (argc < 3) {
		fprintf(stderr, "usage: %s <outfile_prefix> <infile1> ...\n", argv[0]);
		return 1;
	}

#ifdef _SC_NPROCESSORS_ONLN
	n_threads = sysconf(_SC_NPROCESSORS_ONLN);
#else
	n_threads = 2;
#endif
	n_threads = n_threads < 1 ? 1 : n_threads > N_MICS ? N_MICS : n_threads;

	real_t **streams = load_files(n_streams, argv + 2, &n_samples, &sample_rate);
	if (streams == NULL) {
		fprintf(stderr, "failed to load input files\n");
		return 1;
	}

	param.out_samples = malloc(n_samples * n_threads * sizeof(param.out_samples[0]));
	param.out_acc = malloc(n_samples * n_threads * sizeof(param.out_acc[0]));
	if (param.out_samples == NULL || param.out_acc == NULL) {
		fprintf(stderr, "can't allocate space for output\n");
		return 1;
	}

	printf("rate %d, %lu samples\n", sample_rate, n_samples);
	printf("using %d threads\n", n_threads);

	param.index = 0;
	param.n_streams = n_streams;
	param.streams = streams;
	param.file_prefix = argv[1];
	param.n_samples = n_samples;
	param.sample_rate = sample_rate;

	memset(threads, 0, sizeof(threads));
	for (int i = 0; i < n_threads; i++) {
		pthread_create(&threads[i], NULL, gen_thread, (void*)(intptr_t)i);
	}
	for (int i = 0; i < n_threads; i++) {
		pthread_join(threads[i], NULL);
	}

	return 0;
}
