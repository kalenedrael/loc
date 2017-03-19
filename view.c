#include <SDL/SDL.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "globals.h"
#include "liss.h"
#include "locate.h"
#include "vector.h"
#include "wav.h"

#define SCALE 0.01 /* meters per pixel */

/* function prototypes */
static void update();

/* SDL stuff */
static SDL_Surface *screen;
static int need_draw, need_update = 1;

/* data */
#include "mic.c"

static size_t n_samples;
static real_t *mic_data[N_MICS];
static real_t sample_rate;
static real_t xcor_res[N_MICS * XCOR_LEN];
static int32_t distances[N_MICS][XRES*YRES];

static int cur_time, old_time, paused;

static void handle_event(SDL_Event *ev)
{
	switch(ev->type) {
	case SDL_QUIT:
		exit(0);
	case SDL_KEYDOWN:
		switch(ev->key.keysym.sym) {
		case SDLK_q: exit(0);
		case SDLK_SPACE: paused = !paused;
		default: return;
		}
	case SDL_USEREVENT: /* update event */
		if (need_update) {
			need_update = 0;
			update();
			need_draw = 1;
		}
	default:
		return;
	}
}

static Uint32 timer_cb(Uint32 x, void* p)
{
	SDL_Event tev;
	tev.user.type = SDL_USEREVENT;
	SDL_PushEvent(&tev);
	return x;
}

static int clamp(int v)
{
	if (v < 0 || v >= XRES * YRES) {
		return 0;
	}
	return v;
}

static int transform(vec3_t pos)
{
	vec3_t scaled = vec3_scale(pos, 1.0/SCALE);
	return clamp((-(int)scaled.y + YRES/2) * XRES + (int)scaled.x + XRES/2);
}

static void draw_mark(uint32_t *p, int pos, uint32_t color)
{
	for (int i = 0; i < 11; i++) {
		p[clamp(pos + i - 5)] = color;
	}
	for (int i = 0; i < 11; i++) {
		p[clamp(pos + (i - 5) * XRES)] = color;
	}
}

static void update(void)
{
	int time = SDL_GetTicks(), dt = time - old_time;
	old_time = time;
	if (paused) return;

	cur_time += dt;
	size_t sample = (size_t)(cur_time * sample_rate * 0.001);

	/* check if we're done */
	if (sample >= n_samples - XCOR_LEN) {
		exit(0);
	}

	locate_xcor(mic_data, sample, xcor_res);

	SDL_LockSurface(screen);
	uint32_t *p = screen->pixels;
#if 1
	/* draw the sound field */
	for (int i = 0; i < XRES*YRES; i++) {
		real_t acc = 1.0;
		for (int j = 0; j < N_MICS; j++) {
			size_t offset = distances[j][i];
			acc *= xcor_res[j * XCOR_LEN + offset];
		}
		uint32_t iacc = (uint32_t)(acc * 10.0);
		p[i] = acc > 0.0 ? (iacc > 255 ? 255 : iacc) : 0;
	}

	for (int i = 0; i < N_MICS; i++) {
		draw_mark(p, transform(mic_pos[i]), 0xFF0000);
	}
	int pos = transform(liss_pos(cur_time * 0.001));
	draw_mark(p, pos, 0xFFFF00);
#else
	/* draw the raw cross-correlations (first 3) */
	memset(p, 0, XRES*YRES*4);

	/* draw y axis (delay = 0) */
	for (int i = 0; i < YRES; i++) {
		p[i * XRES + XCOR_LEN / 2] = 0x0000FF;
	}

	for (int i = 0; i < 3; i++) {
		int y_off = (i + 1) * YRES / 4;

		/* draw x axis (y = 0) */
		for (int j = 0; j < XCOR_LEN; j++) {
			p[y_off * XRES + j] = 0xFF0000;
		}

		/* plot cross-correlation */
		for (int j = 0; j < XCOR_LEN; j++) {
			int y = y_off - (int)(xcor_res[i * XCOR_LEN + j] * 30.0);
			y = y < 0 ? 0 : y >= YRES ? YRES - 1 : y;
			p[y * XRES + j] = 0xFFFFFF;
		}
	}
#endif

	SDL_UnlockSurface(screen);
}

static void draw(void)
{
	SDL_Flip(screen);
}

static void init(void)
{
	/* set up SDL */
	screen = NULL;
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
		fprintf(stderr, "init failed: %s\n", SDL_GetError());
		exit(1);
	}

	screen = SDL_SetVideoMode(XRES, YRES, 32, SDL_SWSURFACE);
	if(!screen) {
		fprintf(stderr, "video mode init failed: %s\n", SDL_GetError());
		exit(1);
	}

	SDL_WM_SetCaption("you can run, but you can't hide", "unless you're quiet");
	atexit(SDL_Quit);

	/* initialize data structures */
	locate_init(XCOR_LEN, N_MICS);

	/* initialize update timer */
	if(SDL_AddTimer(25, timer_cb, NULL) == NULL) {
		fprintf(stderr, "error setting update timer...\n");
		exit(1);
	}
}

static void init_pos(vec3_t mic0, vec3_t mic1, int32_t *dist, real_t sample_rate)
{
	vec3_t pos;
	pos.z = 0.0;

	for (int i = 0; i < YRES; i++) {
		pos.y = (real_t)((YRES/2) - i) * SCALE;
		for (int j = 0; j < XRES; j++) {
			pos.x = (real_t)(j - (XRES/2)) * SCALE;
			real_t d0 = vec3_dist(pos, mic0);
			real_t d1 = vec3_dist(pos, mic1);
			dist[i * XRES + j] = (int32_t)round((d0 - d1) / SND_SPEED * sample_rate) + XCOR_LEN/2;
		}
	}
}

int main(int argc, char **argv)
{
	SDL_Event ev;
	wav_t wav;

	if (argc < N_MICS + 1) {
		fprintf(stderr, "usage: %s <file 1> <file 2> ...\n", argv[0]);
		return 1;
	}

	init();

	size_t len = 0;
	for (size_t i = 0; i < N_MICS; i++) {
		size_t prev_len = len;
		mic_data[i] = wav_read_mono_16(argv[i+1], &wav, &len);
		if (prev_len > 0 && len != prev_len) {
			fprintf(stderr, "dfuq?\n");
			return 1;
		}
	}

	n_samples = len;
	sample_rate = (real_t)wav.rate;

	for (int i = 0; i < N_MICS; i++) {
		init_pos(mic_pos[i], mic_pos[(i + 1) % N_MICS], distances[i], sample_rate);
	}

	while (SDL_WaitEvent(&ev)) {
		do {
			handle_event(&ev);
		} while (SDL_PollEvent(&ev));
		if(need_draw) {
			need_draw = 0;
			draw();
			need_update = 1;
		}
	}

	return 1;
}
