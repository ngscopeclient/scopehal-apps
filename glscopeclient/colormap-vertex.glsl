#version 150

in vec2 		vert;
out vec2		texcoord;

void main()
{
	gl_Position = vec4(vert.x, vert.y, 0, 1);
	texcoord = vec2( (vert.x + 1)/2, (vert.y + 1)/2 );
}
