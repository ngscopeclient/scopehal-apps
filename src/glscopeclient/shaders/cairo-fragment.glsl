#version 150 //hack: brought up from version 130 for apple compat, make dynamic if this hurts other platforms

in vec2 			texcoord;
uniform sampler2D	fbtex;
out vec4 			finalColor;

void main()
{
	//Look up the original color
	vec4 texcolor = texture(fbtex, vec2(texcoord));

	//Copy it
	finalColor = vec4(texcolor.b, texcolor.g, texcolor.r, texcolor.a);
}
