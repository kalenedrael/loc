/** @file locate.c
 *  @brief Signal processing functions for audio source localization
 */

#include <complex.h>
#include <fftw3.h>
#include <stdlib.h>

#include "globals.h"
#include "locate.h"

#ifndef USE_DOUBLE
#define fftw_plan fftwf_plan
#define fftw_plan_many_dft fftwf_plan_many_dft
#define fftw_alloc_complex fftwf_alloc_complex
#define fftw_execute fftwf_execute
#define fftw_complex fftwf_complex
#endif

static int fft_data_len, fft_len, fft_count, fft_buf_len;
static fftw_plan plan_f, plan_r;
static fftw_complex *in_f, *in_r, *out_f, *out_r;

/** @brief Initializes locate
 *  @param n_samples Number of samples to take from input data
 *  @param n_mics Number of signals
 *
 *  Initializes `locate_xcor` to compute the cross-correlation of `n_mics`
 *  input signals using `n_samples` samples from each one.
 */
void locate_init(int n_samples, int n_mics)
{
	fft_data_len = n_samples;
	fft_len = fft_data_len * 2;
	fft_buf_len = fft_len * n_mics;
	fft_count = n_mics;

	in_f = fftw_alloc_complex(fft_buf_len);
	in_r = fftw_alloc_complex(fft_buf_len);

	out_f = fftw_alloc_complex(fft_buf_len);
	out_r = fftw_alloc_complex(fft_buf_len);

	if (in_f == NULL || in_r == NULL || out_f == NULL || out_r == NULL) {
		fprintf(stderr, "failed to initialize locate");
		exit(0);
	}

	plan_f = fftw_plan_many_dft(1,              /* rank */
	                            &fft_len,       /* dimensions */
	                            fft_count,      /* number of FFTs */

	                            /* buffer, embed, stride, distance */
	                            in_f , NULL, 1, fft_len, /* input */
	                            out_f, NULL, 1, fft_len, /* output */

	                            FFTW_FORWARD,   /* direction */
	                            FFTW_ESTIMATE); /* flags */

	plan_r = fftw_plan_many_dft(1,              /* rank */
	                            &fft_len,       /* dimensions */
	                            fft_count,      /* number of FFTs */

	                            /* buffer, embed, stride, distance */
	                            in_r , NULL, 1, fft_len, /* input */
	                            out_r, NULL, 1, fft_len, /* output */

	                            FFTW_BACKWARD,  /* direction */
	                            FFTW_ESTIMATE); /* flags */

	for (size_t i = 0; i < fft_buf_len; i++) {
		in_f[i] = 0.0;
	}
}

/** @brief Computes cross-correlation of multiple arrays
 *  @param data Array of arrays of input data
 *  @param data_offset Offset in each data array to start reading data
 *  @param res Result array - two-dimensional array of outputs
 *
 *  Computes the normalized cross-correlation between successive arrays of
 *  input data, e.g. for 3 input arrays:
 *
 *  res[0] = xcor(data[0], data[1])
 *  res[1] = xcor(data[1], data[2])
 *  res[2] = xcor(data[2], data[0])
 *
 *  Each row of the result contains the normalized cross-correlation from
 *  offset `-n_samples/2` to offset `n_samples/2`, with index `n_samples/2`
 *  containing the 0-offset cross-correlation.
 *
 *  `res` is expected to be a `n_mics * n_samples` array.
 */
void locate_xcor(real_t **data, size_t data_offset, real_t *res)
{
	int i, j;

	/* gather input data */
	for (i = 0; i < fft_count; i++) {
		int offset = i * fft_len;
		real_t *src = data[i] + data_offset;
		for (j = 0; j < fft_data_len; j++) {
			in_f[j + offset] = src[j];
		}
	}

	fftw_execute(plan_f);

	/* multiply each DFT by the conjugate of the next DFT */
	for (i = 0; i < fft_buf_len; i++) {
		if ((j = i + fft_len) >= fft_buf_len) {
			j -= fft_buf_len;
		}
		in_r[i] = out_f[i] * conj(out_f[j]);
	}

	fftw_execute(plan_r);

	/* copy (shifted) to result, scaling to remove partial overlap bias */
	for (i = 0; i < fft_count; i++) {
		int offset = i * fft_len;
		int res_offset = i * fft_data_len;

		for (j = 0; j < fft_data_len; j++) {
			int d = abs((int)j - fft_data_len / 2);
			int j_off = (j + fft_len - fft_data_len / 2) % fft_len;
			res[res_offset + j] = out_r[offset + j_off] / (fft_data_len - d);
		}
	}

	/* normalize: maximum possible cross-correlation of two signals is the
	 * maximum of their autocorrelations
	 */
	real_t cor = 0.0, cor_prev = 0.0;

	/* do the first signal twice */
	for (i = 0; i < fft_count + 1; i++) {
		int i_actual = i % fft_count;
		real_t *src = data[i_actual] + data_offset;

		/* find autocorrelation */
		cor = 0.0;
		for (j = 0; j < fft_data_len; j++) {
			cor += src[j] * src[j];
		}

		/* scale down by max of this and previous autocorrelation */
		if (i > 0) {
			real_t scale = 0.5 / (cor > cor_prev ? cor : cor_prev);
			for (j = 0; j < fft_data_len; j++) {
				res[(i - 1) * fft_data_len + j] *= scale;
			}
		}

		cor_prev = cor;
	}
}
