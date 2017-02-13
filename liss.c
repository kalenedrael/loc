#include <math.h>

#include "liss.h"

/* lissajous trajectory paremeters */
struct {
	real_t duration;
	vec3_t period;
	vec3_t phase;
	vec3_t scale;
	vec3_t trans;
	real_t rt, rp;
} liss_param = {
	.duration = 8.0,
	.period = { 1.0,    1.0, 0.0 },
	.phase  = { 0.0, M_PI/2, 0.0 },
	.scale  = { 5.0,    5.0, 0.0 },
	.trans  = { 0.0,    0.0, 0.0 },
	.rt = 0.0, .rp = 0.0,
};

vec3_t liss_pos(real_t t)
{
	real_t nt = t * M_PI * 2.0 / liss_param.duration;
	/* no rotation yet */
	vec3_t bv = vec3_sin(vec3_add(vec3_scale(liss_param.period, nt), liss_param.phase));
	return vec3_add(vec3_mul(bv, liss_param.scale), liss_param.trans);
}
