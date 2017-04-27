#version 130

#define PLOT_HEIGHT 128

uniform sampler2D u_correlation;

void main(void)
{
	int fft_len = textureSize(u_correlation, 0).x;
	ivec2 coord = ivec2(gl_FragCoord.xy);

	if (coord.x >= fft_len) discard;

	int y_off = coord.y % PLOT_HEIGHT;
	float v = texelFetch(u_correlation, ivec2(int(gl_FragCoord.x), coord.y / PLOT_HEIGHT), 0).r * 90.0;
	float pix = clamp(1.5 - abs(y_off - 30.0 - v), 0.0, 1.0);

	vec3 axis_mix = vec3(0.0);
	axis_mix = (coord.x == fft_len / 2) ? vec3(1.0, 0.0, 0.0) : axis_mix;
	axis_mix = (y_off == 30) ? vec3(0.0, 0.0, 1.0) : axis_mix;
	gl_FragColor = vec4(mix(axis_mix, vec3(1.0), pix), 1.0);
}
