#define RSQRT_3 0.57735026918962576451
static vec3_t mic_pos[] = {
#if 1
	{ .x = -0.5, .y = -RSQRT_3/2.0, .z = 0.0 },
	{ .x =  0.5, .y = -RSQRT_3/2.0, .z = 0.0 },
	{ .x =  0.0, .y =  RSQRT_3/1.0, .z = 0.0 },
#else
	{ .x = -0.5, .y = -0.5, .z = 0.0 },
	{ .x =  0.5, .y = -0.5, .z = 0.0 },
	{ .x =  0.5, .y =  0.5, .z = 0.0 },
	{ .x = -0.5, .y =  0.5, .z = 0.0 },
#endif
};
#define N_MICS (sizeof(mic_pos)/sizeof(mic_pos[0]))
