#version 130

varying vec4 color;

void main(void)
{
	vec2 coord_d = abs(gl_PointCoord - vec2(0.5)) * 10.0;
	float d = 1.0 - min(coord_d.x, coord_d.y);
	gl_FragColor = vec4(color.rgb, mix(0.0, color.a, d * 2.0));
}
