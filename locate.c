#include <complex.h>
#include <fftw3.h>
#include <stdlib.h>

#include "globals.h"
#include "locate.h"

#ifndef USE_DOUBLE
#define fftw_plan fftwf_plan
#define fftw_plan_dft_1d fftwf_plan_dft_1d
#define fftw_alloc_complex fftwf_alloc_complex
#define fftw_execute fftwf_execute
#define fftw_complex fftwf_complex
#endif

static size_t fft_len, fft_len_actual;
static fftw_plan p0, p1, px;
static fftw_complex *in0, *in1, *out0, *out1, *inx, *outx;

void locate_init(size_t n_samples)
{
	size_t i;

	fft_len = n_samples;
	fft_len_actual = fft_len * 2 - 1;
	in0 = fftw_alloc_complex(fft_len_actual);
	in1 = fftw_alloc_complex(fft_len_actual);
	inx = fftw_alloc_complex(fft_len_actual);

	out0 = fftw_alloc_complex(fft_len_actual);
	out1 = fftw_alloc_complex(fft_len_actual);
	outx = fftw_alloc_complex(fft_len_actual);

	if (in0 == NULL || in1 == NULL || inx == NULL ||
	    out0 == NULL || out1 == NULL || outx == NULL) {
		fprintf(stderr, "failed to initialize locate");
		exit(0);
	}

	p0 = fftw_plan_dft_1d(fft_len_actual, in0, out0, FFTW_FORWARD, FFTW_ESTIMATE);
	p1 = fftw_plan_dft_1d(fft_len_actual, in1, out1, FFTW_FORWARD, FFTW_ESTIMATE);
	px = fftw_plan_dft_1d(fft_len_actual, inx, outx, FFTW_BACKWARD, FFTW_ESTIMATE);

	for (i = 0; i < fft_len_actual; i++) {
		in0[i] = 0.0;
	}
	for (i = 0; i < fft_len_actual; i++) {
		in1[i] = 0.0;
	}
}

void locate_xcor(real_t *d0, real_t *d1, real_t *res)
{
	size_t i;

	/* pad input signals to appropriate length */
	for (i = 0; i < fft_len; i++) {
		in0[i] = d0[i];
	}
	for (i = fft_len - 1; i < fft_len_actual; i++) {
		in1[i] = d1[i - (fft_len - 1)];
	}

	fftw_execute(p0);
	fftw_execute(p1);

	/* multiply DFTs */
	for (i = 0; i < fft_len_actual; i++) {
		inx[i] = out0[i] * conj(out1[i]);
	}

	fftw_execute(px);

	/* scale to remove cross-correlation bias */
	for (i = 0; i < fft_len_actual; i++) {
		int d = abs((int)fft_len - (int)i - 1);
		res[i] = outx[i] / (fft_len - d);
	}

	/* find max autocorrelation */
	real_t cor0 = 0.0, cor1 = 0.0;
	for (i = 0; i < fft_len; i++) {
		cor0 += d0[i] * d0[i];
	}
	for (i = 0; i < fft_len; i++) {
		cor1 += d1[i] * d1[i];
	}
	real_t max_cor = cor0 > cor1 ? cor0 : cor1;

	/* scale down by max autocorrelation */
	real_t scale = 1.0 / max_cor;
	for (i = 0; i < fft_len_actual; i++) {
		res[i] = res[i] * scale;
	}
}
