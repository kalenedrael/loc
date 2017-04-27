/** @file locate.c
 *  @brief Signal processing functions for audio source localization
 */

#include <complex.h>
#include <fftw3.h>
#include <stdlib.h>
#include <string.h>

#include "globals.h"
#include "locate.h"

#ifndef USE_DOUBLE
#define fftw_plan fftwf_plan
#define fftw_plan_many_dft fftwf_plan_many_dft
#define fftw_alloc_complex fftwf_alloc_complex
#define fftw_execute fftwf_execute
#define fftw_complex fftwf_complex
#endif

static struct fft {
	fftw_plan plan;
	fftw_complex *in, *out;
	int len;
} fft_f, fft_r;

static int fft_count, fft_data_len, fft_out_len, fft_upres;

/** @brief Initializes a single FFT
 *  @param fft FFT to initialize
 *  @param len Length of FFT to initialize
 *  @param direction Direction of FFT (FFTW_FORWARD or FFTW_BACKWARD)
 *  @return 0 on success, negative on failure
 */
static int init_fft(struct fft *fft, int len, int direction)
{
	fft->len = len;
	fft->in = fftw_alloc_complex(len * fft_count);
	fft->out = fftw_alloc_complex(len * fft_count);

	if (fft->in == NULL || fft->in == NULL) {
		return -1;
	}

	fft->plan = fftw_plan_many_dft(1,              /* rank */
	                               &fft->len,      /* dimensions */
	                               fft_count,      /* number of FFTs */

	                               /* buffer, embed, stride, distance */
	                               fft->in , NULL, 1, len, /* input */
	                               fft->out, NULL, 1, len, /* output */

	                               direction,      /* direction */
	                               FFTW_ESTIMATE); /* flags */

	memset(fft->in, 0, len * fft_count * sizeof(*(fft->in)));
	return 0;
}

/** @brief Initializes locate
 *  @param n_samples Number of samples to take from input data
 *  @param n_mics Number of signals
 *  @param upres_factor Super-resolution factor
 *  @return 0 on success, negative on failure
 *
 *  Initializes `locate_xcor` to compute the cross-correlation of `n_mics`
 *  input signals using `n_samples` samples from each one.
 */
int locate_init(int n_samples, int n_mics, int upres_factor)
{
	fft_count = n_mics;
	fft_upres = upres_factor;
	fft_data_len = n_samples;
	fft_out_len = n_samples * upres_factor;

	if (init_fft(&fft_f, n_samples * 2, FFTW_FORWARD) < 0 ||
	    init_fft(&fft_r, n_samples * upres_factor * 2, FFTW_BACKWARD) < 0) {
		return -1;
	}

	return 0;
}

/** @brief Computes phase cross-correlation of multiple arrays
 *  @param data Array of arrays of input data
 *  @param data_offset Offset in each data array to start reading data
 *  @param res Result array - two-dimensional array of outputs
 *
 *  Computes the phase cross-correlation between successive arrays of input
 *  data, e.g. for 3 input arrays:
 *
 *  res[0] = xcor(data[0], data[1])
 *  res[1] = xcor(data[1], data[2])
 *  res[2] = xcor(data[2], data[0])
 *
 *  Each row of the result contains the normalized cross-correlation from
 *  offset `-n_samples/2` to offset `n_samples/2`, with index `n_samples/2`
 *  containing the 0-offset cross-correlation. Resolution is increased by
 *  a factor of `upres_factor`, thus `res` is expected to be a
 *  `n_mics * n_samples * upres_factor` array.
 */
void locate_xcor(real_t **data, size_t data_offset, real_t *res)
{
	int i, j;

	/* gather input data */
	for (i = 0; i < fft_count; i++) {
		real_t *src = data[i] + data_offset;
		fftw_complex *dst = fft_f.in + fft_f.len * i;

		for (j = 0; j < fft_data_len; j++) {
			dst[j] = src[j];
		}
	}

	fftw_execute(fft_f.plan);

	/* multiply each DFT by the conjugate of the next DFT */
	for (i = 0; i < fft_count; i++) {
		fftw_complex *src      = fft_f.out + fft_f.len * i;
		fftw_complex *src_next = fft_f.out + fft_f.len * ((i + 1) % fft_count);
		fftw_complex *dst      = fft_r.in  + fft_r.len * i;

		/* to achieve super-resolution, expand FFT as band-limited FFT
		 * before reversing - first half goes at the beginning
		 */
		for (j = 0; j < fft_f.len / 2; j++) {
			dst[j] = src[j] * conj(src_next[j]);
			dst[j] /= cabs(dst[j]);
		}

		/* second half goes at the end */
		dst += fft_r.len - fft_f.len;
		for (; j < fft_f.len; j++) {
			dst[j] = src[j] * conj(src_next[j]);
			dst[j] /= cabs(dst[j]);
		}
	}

	fftw_execute(fft_r.plan);

	/* copy (shifted) to result, scaling to remove partial overlap bias */
	real_t scale = fft_upres * 0.5;
	for (i = 0; i < fft_count; i++) {
		real_t *dst = res + fft_out_len * i;
		fftw_complex *src = fft_r.out + fft_r.len * i;

		for (j = 0; j < fft_out_len; j++) {
			int d = abs((int)j - fft_out_len / 2);
			int j_off = (j + fft_r.len - fft_out_len / 2) % fft_r.len;
			dst[j] = src[j_off] * scale / (fft_out_len - d);
		}
	}
}
