#version 130

varying vec2 coord;

void main(void)
{
	coord = gl_Vertex.xy;
	gl_Position = ftransform();
}
