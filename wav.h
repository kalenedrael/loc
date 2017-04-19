#ifndef _WAV_H_
#define _WAV_H_

#include <stdint.h>

#include "globals.h"

real_t *wav_read_mono_16(const char *filename, int32_t *sample_rate_out, size_t *len_out);
int wav_write_mono_16(const char *filename, int32_t sample_rate, int16_t *data, size_t len);

#endif /* _WAV_H_ */
