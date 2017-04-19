/** @file wav.c
 *  @brief Functions to handle WAV file reading and writing
 */

#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "wav.h"
#include "file.h"

typedef struct {
	struct {
		char magic[4];
		int32_t size;
		char waveid[4];
	} hdr;
	struct {
		char magic[4];
		int32_t size;
		int16_t tag;
		int16_t chans;
		int32_t rate;
		int32_t byps;
		int16_t align;
		int16_t bps;
	} fmt;
	struct {
		char magic[4];
		int32_t size;
	} data;
} wav_t;

static wav_t wav_header_mono_16 = {
	.hdr = {
		.magic  = {'R', 'I', 'F', 'F'},
		.size   = 36,
		.waveid = {'W', 'A', 'V', 'E'},
	},
	.fmt = {
		.magic  = {'f', 'm', 't', ' '},
		.size   = 16,
		.tag    = 0x0001,
		.chans  = 1,
		.rate   = 0,
		.byps   = 0,
		.align  = 2,
		.bps    = 16,
	},
	.data = {
		.magic  = {'d', 'a', 't', 'a'},
		.size   = 0,
	},
};

/** @brief Reads a data chunk from a WAV file
 *  @param dst Location to copy data to
 *  @param src Beginning of chunk to read from
 *  @param src_end End of source data; will not read past here
 *  @param magic Magic value (4 bytes) to check for
 *  @param to_read Number of bytes to read
 *  @return Size of chunk (number of bytes from src to end of chunk)
 *
 *  Chunk format is as per the RIFF spec:
 *  byte 0 1 2 3 4 5 6 7 8 9 ...
 *       [magic] [size ] [data...]
 *
 *  I really didn't want to do this, but it turns out not all WAV files have
 *  the same size header. One WAV file I encountered had a fmt section with a
 *  length of 0x12 = 18 instead of 0x10 = 16 like it really should be. This
 *  code is more complex than I'd like but at least it's more robust.
 */
static ssize_t read_chunk(char *dst, char *src, char *src_end, char *magic, ssize_t to_read)
{
	int32_t size;

	/* chunk must be long enough */
	if (src + to_read > src_end) {
		return -1;
	}

	/* chunk must match given magic */
	if (memcmp(src, magic, 4)) {
		return -1;
	}

	memcpy(dst, src, to_read);
	size = *(int32_t*)(src + 4);
	return size + 8;
}

/** @brief Reads a 16-bit mono WAV file as array of floating point samples
 *  @param filename Name of file to read
 *  @param sample_rate_out Output; sample rate of WAV file
 *  @param len_out Output; number of samples
 *  @return Array of samples, or NULL on failure
 */
real_t *wav_read_mono_16(const char *filename, int32_t *sample_rate_out, size_t *len_out)
{
	wav_t wav_header = wav_header_mono_16;
	real_t *samples = NULL;
	ssize_t total_size, wav_len, file_len, len, chunk_len;
	ssize_t offset = offsetof(wav_t, fmt);

	char *file_end, *file = file_read(filename, &total_size);
	if (file == NULL) {
		goto out;
	}
	file_end = file + total_size;

	/* read format info chunk */
	chunk_len = read_chunk((char*)&wav_header.fmt, file + offset, file_end,
	                       "fmt ", sizeof(wav_header.fmt));
	if (chunk_len < sizeof(wav_header.fmt)) {
		fprintf(stderr, "%s: bad 'fmt ' chunk: %zd\n", filename, chunk_len);
		goto out;
	}
	offset += chunk_len;

	/* only accept 16-bit mono */
	if (wav_header.fmt.chans != 1 || wav_header.fmt.bps != 16) {
		fprintf(stderr, "%s: unsupported file type - %d channels, %d bit\n",
		        filename, wav_header.fmt.chans, wav_header.fmt.bps);
		goto out;
	}

	/* read data chunk header */
	chunk_len = read_chunk((char*)&wav_header.data, file + offset, file_end,
	                       "data", sizeof(wav_header.data));
	if (chunk_len < sizeof(wav_header.data)) {
		fprintf(stderr, "%s: bad 'data' chunk: %zd\n", filename, chunk_len);
		goto out;
	}
	offset += sizeof(wav_header.data);

	/* use the shorter of the header-specified size or the file size */
	int16_t *data = (int16_t*)(file + offset);
	wav_len = wav_header.data.size / 2;
	file_len = (total_size - offset) / 2;
	len = wav_len > file_len ? file_len : wav_len;

	samples = malloc(len * sizeof(real_t));
	if (samples == NULL) {
		fprintf(stderr, "%s: cannot allocate memory for %lu samples\n", filename, len);
		goto out;
	}

	real_t scale = 1.0 / (real_t)((size_t)INT16_MAX + 1);
	for (ssize_t i = 0; i < len; i++) {
		samples[i] = (real_t)data[i] * scale;
	}

	*len_out = len;
	*sample_rate_out = wav_header.fmt.rate;
out:
	file_free(file);
	return samples;
}

/** @brief Writes an array of 16-bit samples into a 16-bit mono WAV file
 *  @param filename Name of file to write
 *  @param sample_rate Sample rate
 *  @param data Sample data
 *  @param len Number of samples
 *  @return 0 on success, negative on failure
 */
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
	wav_header.hdr.size   += data_len;
	wav_header.data.size  += data_len;
	wav_header.fmt.rate    = sample_rate;
	wav_header.fmt.byps    = sample_rate * 2;

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
