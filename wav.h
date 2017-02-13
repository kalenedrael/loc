#ifndef _WAV_H_
#define _WAV_H_

#include <stdint.h>

#include "globals.h"

typedef struct {
	char ck_riff[4];
	int32_t size;
	char waveid[4];
	char ck_fmt[4];
	int32_t fmt_sz;
	int16_t fmt_tag;
	int16_t chans;
	int32_t rate;
	int32_t byps;
	int16_t align;
	int16_t bps;
	char ck_data[4];
	int32_t d_size;
} wav_t;

real_t *wav_read_mono_16(const char *filename, wav_t *wav_header, size_t *len_out);
int wav_write_mono_16(const char *filename, int32_t sample_rate, int16_t *data, size_t len);

#endif /* _WAV_H_ */
