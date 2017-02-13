#ifndef _VECTOR_H_
#define _VECTOR_H_

#include <math.h>
#include "globals.h"

typedef struct {
	real_t x, y, z;
} vec3_t;

static const vec3_t vec3_zero = { 0.0, 0.0, 0.0 };

static inline vec3_t vec3_add(vec3_t a, vec3_t b)
{
	vec3_t ret;
	ret.x = a.x + b.x;
	ret.y = a.y + b.y;
	ret.z = a.z + b.z;
	return ret;
}

static inline vec3_t vec3_sub(vec3_t a, vec3_t b)
{
	vec3_t ret;
	ret.x = a.x - b.x;
	ret.y = a.y - b.y;
	ret.z = a.z - b.z;
	return ret;
}

static inline vec3_t vec3_scale(vec3_t a, real_t scale)
{
	vec3_t ret;
	ret.x = a.x * scale;
	ret.y = a.y * scale;
	ret.z = a.z * scale;
	return ret;
}

static inline vec3_t vec3_mul(vec3_t a, vec3_t b)
{
	vec3_t ret;
	ret.x = a.x * b.x;
	ret.y = a.y * b.y;
	ret.z = a.z * b.z;
	return ret;
}

static inline vec3_t vec3_sin(vec3_t v)
{
	vec3_t ret;
	ret.x = sin(v.x);
	ret.y = sin(v.y);
	ret.z = sin(v.z);
	return ret;
}

static inline real_t vec3_dist(vec3_t a, vec3_t b)
{
	vec3_t d = vec3_sub(a, b);
	return sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

#endif /* _VECTOR_H */
