#version 130

in vec2 		vert;
out vec2		texcoord;

uniform float	xscale;
uniform float	xoff;

void main()
{
	gl_Position = vec4((vert.x+1)*xscale - 2*xoff - 1, vert.y, 0, 1);
	texcoord = vec2((vert.x+1)/2, (vert.y + 1)/2 );
}
