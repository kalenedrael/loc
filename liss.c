#include <math.h>

#include "liss.h"

/* lissajous trajectory paremeters */
typedef struct {
	real_t duration;
	vec3_t period;
	vec3_t phase;
	vec3_t scale;
	vec3_t trans;
	real_t rt, rp;
} liss_t;

#define PI2 (M_PI/2.0)

liss_t liss_param[] = {
	{
		.duration = 30.0,
		.period = { 1.0, 1.0, 0.0 },
		.phase  = { 0.0, PI2, 0.0 },
		.scale  = { 5.0, 3.0, 0.0 },
		.trans  = { 0.0, 0.0, 0.0 },
		.rt = 0.0, .rp = 0.0,
	},
	{
		.duration = 10.0,
		.period = { 1.0, 1.0, 0.0 },
		.phase  = { PI2, 0.0, 0.0 },
		.scale  = { 0.3, 0.3, 0.0 },
		.trans  = { 0.0, 0.0, 0.0 },
		.rt = 0.0, .rp = 0.0,
	},
};

/** @brief Generates a point on a (hard-coded) lissajous path
 *  @param t Time to generate for, in seconds
 *  @param i Index of parameter set to use
 */
vec3_t liss_pos(real_t t, int i)
{
	liss_t *l = &liss_param[i % (sizeof liss_param / sizeof liss_param[0])];
	real_t nt = t * M_PI * 2.0 / l->duration;
	/* no rotation yet */
	vec3_t bv = vec3_sin(vec3_add(vec3_scale(l->period, nt), l->phase));
	return vec3_add(vec3_mul(bv, l->scale), l->trans);
}
