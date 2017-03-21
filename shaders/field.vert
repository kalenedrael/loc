#version 130

out vec2 coord;

void main(void)
{
	coord = gl_Vertex.xy;
	gl_Position = ftransform();
}
