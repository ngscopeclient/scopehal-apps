#version 130

in vec2 		vert;
uniform float	xoff;
uniform float	xscale;
uniform float	yoff;
uniform float	yscale;
uniform mat4	projection;

void main()
{
	vec4 transformed = vec4(
		(vert.x * xscale) + xoff,
		(vert.y * yscale) + yoff,
		0,
		1
		);
	gl_Position = projection * transformed;
}
