#version 130

#define FFT_LEN 512
#define FFT_MID 256
#define PLOT_HEIGHT 128

uniform sampler2D u_correlation;

void main(void)
{
	ivec2 coord = ivec2(gl_FragCoord.xy);

	if (coord.x >= FFT_LEN) discard;

	int y_off = coord.y % PLOT_HEIGHT;
	float v = texelFetch(u_correlation, ivec2(int(gl_FragCoord.x), coord.y / PLOT_HEIGHT), 0).r * 90.0;
	float pix = clamp(1.5 - abs(y_off - 30.0 - v), 0.0, 1.0);

	vec3 axis = (y_off == 30) ? vec3(0.0, 0.0, 1.0) : (coord.x == FFT_MID) ? vec3(1.0, 0.0, 0.0) : vec3(0.0);
	gl_FragColor = vec4(mix(axis, vec3(1.0), pix), 1.0);
}
