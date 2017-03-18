#version 130

const int N_MICS = 12;

uniform sampler1D samples;
uniform vec3 mic_pos[N_MICS];

varying vec2 coord;

void main(void)
{
/*
	float acc = 1.0;
	for (int i = 0; i < N_MICS; i++) {
		vec3 p0 = mic_pos[i];
		vec3 p1 = mic_pos[(i + 1) % N_MICS];
		vec3 pos = vec3(coord, 0.0);
		float dt = distance(p0, pos) - distance(p1, pos);

		sample = 
	}
*/
	gl_FragColor = vec4(0.0, 0.0, 0.2, 1.0);
}
