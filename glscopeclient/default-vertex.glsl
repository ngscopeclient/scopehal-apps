#version 150

in vec3 		vert;
uniform mat4	projection;

void main()
{
	gl_Position = projection * vec4(vert, 1);
}
