#version 130

varying vec4 color;

void main(void)
{
	gl_Position = ftransform();
	gl_PointSize = 10.0;
	color = gl_Color;
}
