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

#define XCOR_LEN 512 /* samples */
#define XCOR_TEX_LEN 512
#define XCOR_MUL 4 /* super-resolution factor */

/* function prototypes */
static void update();

/* SDL stuff */
enum {
	MODE_FIELD = 0,
	MODE_PLOT = 1,
};

static SDL_Surface *screen;
static int need_draw, need_update = 1, view_mode = MODE_FIELD;

/* data */
#include "mic.c"

static size_t n_samples, n_sources;
static real_t *mic_data[N_MICS];
static real_t sample_rate;
static real_t xcor_res[N_MICS * XCOR_LEN * XCOR_MUL];

static int cur_time, old_time, paused;

/* gl stuff */
GLuint shd_field, shd_points, shd_plot;
GLuint tex_correlation;
GLint u_correlation, u_mic_pos, u_samples_per_m, u_intensity;

static double intensity = 0.0001;
static float xcor_tex_data[N_MICS * XCOR_TEX_LEN * XCOR_MUL];
static float mic_pos_data[N_MICS * 3];

static void handle_event(SDL_Event *ev)
{
	switch (ev->type) {
	case SDL_QUIT:
		exit(0);
	case SDL_KEYDOWN:
		switch (ev->key.keysym.sym) {
		case SDLK_q: exit(0);
		case SDLK_SPACE: paused = !paused; break;
		case SDLK_v:
			switch (view_mode) {
			case MODE_FIELD: view_mode = MODE_PLOT; break;
			case MODE_PLOT:  view_mode = MODE_FIELD; break;
			}
			break;
		case SDLK_LEFTBRACKET: intensity *= 0.5; break;
		case SDLK_RIGHTBRACKET: intensity *= 2.0; break;
		default: break;
		}
		break;
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

static void draw_field(void)
{
	/* render field */
	glUseProgram(shd_field);
	glUniform1i(u_correlation, 0);
	glUniform1f(u_samples_per_m, (sample_rate * XCOR_MUL) / SND_SPEED);
	glUniform1f(u_intensity, intensity);
	glUniform3fv(u_mic_pos, N_MICS, mic_pos_data);

	glBegin(GL_TRIANGLE_STRIP);
	glVertex2f( WIDTH * 0.5,  HEIGHT * 0.5);
	glVertex2f( WIDTH * 0.5, -HEIGHT * 0.5);
	glVertex2f(-WIDTH * 0.5,  HEIGHT * 0.5);
	glVertex2f(-WIDTH * 0.5, -HEIGHT * 0.5);
	glEnd();

	/* render points */
	glUseProgram(shd_points);
	glColor4f(1.0, 0.0, 0.0, 1.0);
	glBegin(GL_POINTS);
	for (int i = 0; i < N_MICS; i++) {
		glVertex2f(mic_pos[i].x, mic_pos[i].y);
	}
	glColor3f(1.0, 1.0, 0.0);
	for (int i = 0; i < n_sources; i++) {
		vec3_t pos = liss_pos(cur_time * 0.001 + (real_t)(XCOR_LEN / 2) / sample_rate, i);
		glVertex2f(pos.x, pos.y);
	}
	glEnd();
}

static void draw_plot(void)
{
	glUseProgram(shd_plot);
	glUniform1i(u_correlation, 0);

	glBegin(GL_TRIANGLE_STRIP);
	glVertex2f( WIDTH * 0.5,  HEIGHT * 0.5);
	glVertex2f( WIDTH * 0.5, -HEIGHT * 0.5);
	glVertex2f(-WIDTH * 0.5,  HEIGHT * 0.5);
	glVertex2f(-WIDTH * 0.5, -HEIGHT * 0.5);
	glEnd();
}

static void draw(void)
{
	/* copy cross-correlation data into texture buffer */
	for (int i = 0; i < N_MICS; i++) {
		int offset_i = (i * XCOR_LEN + (XCOR_LEN - XCOR_TEX_LEN) / 2) * XCOR_MUL;
		int offset_o = i * XCOR_TEX_LEN * XCOR_MUL;
		for (int j = 0; j < XCOR_TEX_LEN * XCOR_MUL; j++) {
			xcor_tex_data[j + offset_o] = xcor_res[j + offset_i];
		}
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, XCOR_TEX_LEN * XCOR_MUL, N_MICS,
	             0, GL_RED, GL_FLOAT, xcor_tex_data);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (view_mode == MODE_FIELD) {
		draw_field();
	} else if (view_mode == MODE_PLOT) {
		draw_plot();
	}

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
	file_free((void*)source);

	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (compiled == GL_FALSE) {
		GLint len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

		GLchar *err = (GLchar *)malloc(len);
		glGetShaderInfoLog(shader, len, &len, err);
		fprintf(stderr, "%s: error compiling shader:\n%s\n", file, (char*)err);
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

	/* set up coordinates - (0,0) is middle */
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glOrtho(-WIDTH*0.5, WIDTH*0.5, -HEIGHT*0.5, HEIGHT*0.5, -1.0, 1.0);
	glClearColor(0.0, 0.0, 0.0, 0.0);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_PROGRAM_POINT_SIZE);
	glEnable(GL_POINT_SPRITE);
	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_COLOR_MATERIAL);

	/* set up shaders and uniforms */
	shd_field = create_shader("shaders/field.vert", "shaders/field.frag");
	shd_points = create_shader("shaders/points.vert", "shaders/points.frag");
	shd_plot = create_shader("shaders/plot.vert", "shaders/plot.frag");

	u_correlation = glGetUniformLocation(shd_field, "u_correlation");
	u_mic_pos = glGetUniformLocation(shd_field, "u_mic_pos");
	u_samples_per_m = glGetUniformLocation(shd_field, "u_samples_per_m");
	u_intensity = glGetUniformLocation(shd_field, "u_intensity");

	/* set up cross-correlation texture */
	glGenTextures(1, &tex_correlation);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex_correlation);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	/* get mic positions compatible with GL uniform */
	for (int i = 0, j = 0; i < N_MICS; i++) {
		mic_pos_data[j++] = mic_pos[i].x;
		mic_pos_data[j++] = mic_pos[i].y;
		mic_pos_data[j++] = mic_pos[i].z;
	}

	SDL_WM_SetCaption("you can run, but you can't hide", "unless you're quiet");
	atexit(SDL_Quit);

	/* initialize data structures */
	if (locate_init(XCOR_LEN, N_MICS, XCOR_MUL) < 0) {
		fprintf(stderr, "locate init failed");
		exit(1);
	}

	/* initialize update timer */
	if (SDL_AddTimer(25, timer_cb, NULL) == NULL) {
		fprintf(stderr, "error setting update timer...\n");
		exit(1);
	}
}

int main(int argc, char **argv)
{
	SDL_Event ev;
	char buf[256];
	int32_t wav_rate;
	size_t len = 0;

	if (argc < 3) {
		fprintf(stderr, "usage: %s <file_prefix> <n_sources>\n", argv[0]);
		return 1;
	}

	init();

	for (int i = 0; i < N_MICS; i++) {
		size_t prev_len = len;
		snprintf(buf, 256, "%s.%d.wav", argv[1], i);
		fprintf(stderr, "input %2d: %s\n", i, buf);
		mic_data[i] = wav_read_mono_16(buf, &wav_rate, &len);
		if (mic_data[i] == NULL || (prev_len > 0 && len != prev_len)) {
			fprintf(stderr, "dfuq?\n");
			return 1;
		}
	}

	n_samples = len;
	n_sources = atoi(argv[2]);
	sample_rate = (real_t)wav_rate;

	printf(
		"space: pause\n"
		"v: change view mode\n"
		"[]: decrease/increase intensity\n"
		"q: quit\n"
	);

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
