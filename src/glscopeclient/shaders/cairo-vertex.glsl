#version 150 //hack: brought up from version 130 for apple compat, make dynamic if this hurts other platforms

in vec2 		vert;
out vec2		texcoord;

void main()
{
	gl_Position = vec4(vert.x, vert.y, 0, 1);
	texcoord = vec2( (vert.x + 1)/2, (vert.y + 1)/2 );
}
