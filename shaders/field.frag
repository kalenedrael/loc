#version 130

#define FFT_LEN 512
#define FFT_HALF 256

const int N_MICS = 12;

uniform sampler2D u_correlation;
uniform float u_samples_per_m;
uniform float u_intensity;
uniform vec3 u_mic_pos[N_MICS];

in vec2 coord;

void main(void)
{
	float acc = 0.0;
	for (int i = 0; i < N_MICS; i++) {
		int i_next = (i + 1) % N_MICS;
		vec3 p0 = u_mic_pos[i];
		vec3 p1 = u_mic_pos[i_next];
		vec3 pos = vec3(coord, 0.0);
		float dt = distance(p0, pos) - distance(p1, pos);
		int ds = int(round(dt * u_samples_per_m));

		acc += clamp(texelFetch(u_correlation, ivec2(FFT_HALF + ds, i), 0).r, 0.0, 1.0);
	}

	acc *= acc;
	acc *= acc;
	acc *= u_intensity;
	gl_FragColor = vec4(acc * 0.24, acc * 0.3, acc, 1.0);
}
