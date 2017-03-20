/** @file view.c
 *  @brief Plots likely positions of sound sources from multiple audio streams
 *
 *  Currently only meant to work with the simulated streams from `gen`.
 */

#include <SDL/SDL.h>
#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "file.h"
#include "globals.h"
#include "liss.h"
#include "locate.h"
#include "vector.h"
#include "wav.h"

#define XRES 1200
#define YRES 1200
#define WIDTH 12.0 /* meters */
#define HEIGHT 12.0
#define XCOR_LEN 4096 /* samples */
#define XCOR_PLOT_LEN 1024

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

static int cur_time, old_time, paused;

/* gl stuff */
GLuint shd_field, shd_points;

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
}

static void draw(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(shd_field);
	glBegin(GL_TRIANGLE_STRIP);
	glVertex2f( WIDTH * 0.5,  HEIGHT * 0.5);
	glVertex2f( WIDTH * 0.5, -HEIGHT * 0.5);
	glVertex2f(-WIDTH * 0.5,  HEIGHT * 0.5);
	glVertex2f(-WIDTH * 0.5, -HEIGHT * 0.5);
	glEnd();

	glUseProgram(shd_points);
	glColor4f(1.0, 0.0, 0.0, 1.0);
	glBegin(GL_POINTS);
	for (int i = 0; i < N_MICS; i++) {
		glVertex2f(mic_pos[i].x, mic_pos[i].y);
	}
	glColor3f(1.0, 1.0, 0.0);
	vec3_t pos = liss_pos(cur_time * 0.001);
	glVertex2f(pos.x, pos.y);
	glEnd();

	SDL_GL_SwapBuffers();
}

static GLuint load_shader(const char *file, GLint shader_type)
{
	ssize_t size;
	const GLchar *source = file_read(file, &size);
	if (source == NULL) {
		return 0;
	}

	GLint size_int = (int)size;
	GLuint shader = glCreateShader(shader_type);
	glShaderSource(shader, 1, &source, &size_int);
	glCompileShader(shader);
	file_free((char*)source);

	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (compiled == GL_FALSE) {
		GLint len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

		GLchar *err = (GLchar *)malloc(len);
		glGetShaderInfoLog(shader, len, &len, err);
		fprintf(stderr, "%s: error compiling shader: %s\n", file, (char*)err);
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

static GLuint create_shader(const char *vert_file, const char *frag_file)
{
	GLuint program = glCreateProgram();
	GLuint shader_vert = load_shader(vert_file, GL_VERTEX_SHADER);
	GLuint shader_frag = load_shader(frag_file, GL_FRAGMENT_SHADER);

	if (!shader_vert || !shader_frag) {
		goto fail;
	}

	glAttachShader(program, shader_vert);
	glAttachShader(program, shader_frag);
	glLinkProgram(program);

	GLint linked;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (linked == GL_TRUE) {
		glDetachShader(program, shader_vert);
		glDetachShader(program, shader_frag);
		glDeleteShader(shader_vert);
		glDeleteShader(shader_frag);
		return program;
	}
fail:
	fprintf(stderr, "failed to load shader [%s] [%s]\n", vert_file, frag_file);
	exit(1);
}

static void init(void)
{
	/* set up SDL */
	screen = NULL;
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
		fprintf(stderr, "init failed: %s\n", SDL_GetError());
		exit(1);
	}

	/* set up GL */
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	screen = SDL_SetVideoMode(XRES, YRES, 32, SDL_OPENGL | SDL_HWSURFACE);
	if (!screen) {
		fprintf(stderr, "video mode init failed: %s\n", SDL_GetError());
		exit(1);
	}
	GLenum glew_status = glewInit();
	if (glew_status != GLEW_OK) {
		fprintf(stderr, "glew init failed\n");
		exit(1);
	}

	fprintf(stderr, "GL version: %s\n", glGetString(GL_VERSION));
	fprintf(stderr, "GLSL version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	/* (0,0) is upper left */
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glOrtho(-WIDTH*0.5, WIDTH*0.5, -HEIGHT*0.5, HEIGHT*0.5, -1.0, 1.0);
	glClearColor(0.0, 0.0, 0.0, 0.0);

	glEnable(GL_PROGRAM_POINT_SIZE);
	glEnable(GL_POINT_SPRITE);
	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_COLOR_MATERIAL);

	shd_field = create_shader("shaders/field.vert", "shaders/field.frag");
	shd_points = create_shader("shaders/points.vert", "shaders/points.frag");

	SDL_WM_SetCaption("you can run, but you can't hide", "unless you're quiet");
	atexit(SDL_Quit);

	/* initialize data structures */
	locate_init(XCOR_LEN, N_MICS);

	/* initialize update timer */
	if (SDL_AddTimer(25, timer_cb, NULL) == NULL) {
		fprintf(stderr, "error setting update timer...\n");
		exit(1);
	}
}

int main(int argc, char **argv)
{
	SDL_Event ev;
	wav_t wav;
	char buf[256];

	if (argc < 2) {
		fprintf(stderr, "usage: %s <file prefix>\n", argv[0]);
		return 1;
	}

	init();

	size_t len = 0;
	for (int i = 0; i < N_MICS; i++) {
		size_t prev_len = len;
		snprintf(buf, 256, "%s.%d.wav", argv[1], i);
		fprintf(stderr, "input %2d: %s\n", i, buf);
		mic_data[i] = wav_read_mono_16(buf, &wav, &len);
		if (mic_data[i] == NULL || prev_len > 0 && len != prev_len) {
			fprintf(stderr, "dfuq?\n");
			return 1;
		}
	}

	n_samples = len;
	sample_rate = (real_t)wav.rate;

	while (SDL_WaitEvent(&ev)) {
		do {
			handle_event(&ev);
		} while (SDL_PollEvent(&ev));
		if (need_draw) {
			need_draw = 0;
			draw();
			need_update = 1;
		}
	}

	return 1;
}
