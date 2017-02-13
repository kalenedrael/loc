#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "wav.h"

static wav_t wav_header_mono_16 = {
	.ck_riff = {'R', 'I', 'F', 'F'},
	.size    = 36,
	.waveid  = {'W', 'A', 'V', 'E'},
	.ck_fmt  = {'f', 'm', 't', ' '},
	.fmt_sz  = 16,
	.fmt_tag = 0x0001,
	.chans   = 1,
	.rate    = 0,
	.byps    = 0,
	.align   = 2,
	.bps     = 16,
	.ck_data = {'d', 'a', 't', 'a'},
	.d_size  = 0
};

real_t *wav_read_mono_16(const char *filename, wav_t *wav_header, size_t *len_out)
{
	real_t *samples = NULL;

	int fd = open(filename, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "%s: error opening file\n", filename);
		return NULL;
	}

	struct stat stats;
	if (fstat(fd, &stats) == -1) {
		fprintf(stderr, "%s: cannot stat file\n", filename);
		return NULL;
	}

	char *file = (char*)malloc(stats.st_size);
	if (file == NULL) {
		fprintf(stderr, "%s: cannot allocate enough space to read\n", filename);
		return NULL;
	}

	ssize_t ret = read(fd, file, stats.st_size);
	if (ret != stats.st_size) {
		fprintf(stderr, "%s: cannot read entire file\n", filename);
		goto out;
	}
	close(fd);

	memcpy(wav_header, file, sizeof(*wav_header));
	if (wav_header->chans != 1 || wav_header->bps != 16) {
		fprintf(stderr, "%s: unsupported file type - %d channels, %d bit\n", filename, wav_header->chans, wav_header->bps);
		goto out;
	}

	int16_t *data = (int16_t*)(file + sizeof(wav_t));
	ssize_t len = (stats.st_size - sizeof(wav_t))/2;

	samples = (real_t*)malloc(len * sizeof(real_t));
	if (samples == NULL) {
		fprintf(stderr, "%s: cannot allocate memory for %lu samples\n", filename, len);
		goto out;
	}

	real_t scale = 1.0 / (real_t)((size_t)INT16_MAX + 1);
	for (ssize_t i = 0; i < len; i++) {
		samples[i] = (real_t)data[i] * scale;
	}
	*len_out = len;

out:
	free(file);
	return samples;
}

int wav_write_mono_16(const char *filename, int32_t sample_rate, int16_t *data, size_t len)
{
	wav_t wav_header;
	size_t bytes, data_len = len * 2;

	if (len > (size_t)INT_MAX || data_len < len) {
		return -1;
	}

	int fd = open(filename, O_WRONLY | O_CREAT, 0666);
	if (fd == -1) {
		fprintf(stderr, "%s: could not open output file for writing\n", filename);
		return -1;
	}

	memcpy(&wav_header, &wav_header_mono_16, sizeof(wav_header));
	wav_header.size   += sample_rate * len * 2;
	wav_header.d_size += sample_rate * len * 2;
	wav_header.rate    = sample_rate;
	wav_header.byps    = sample_rate * 2;

	if ((bytes = write(fd, &wav_header, sizeof(wav_header))) < sizeof(wav_header)) {
		fprintf(stderr, "warning: %s: wrote %lu bytes of wav header - file corrupt\n", filename, bytes);
	}
	if ((bytes = write(fd, data, data_len)) < data_len) {
		fprintf(stderr, "warning: %s: wrote %lu/%lu bytes of data - file incomplete\n", filename, bytes, data_len);
	}

	if (close(fd) < 0) {
		fprintf(stderr, "warning: %s: failed to close\n", filename);
	}

	return 0;
}
