#version 130

in vec2 			texcoord;
uniform sampler2D	fbtex;

out vec4 			finalColor;

void main()
{
	//Look up the original color
	vec4 texcolor = texture(fbtex, vec2(texcoord));

	finalColor.r = texcolor.r;
	finalColor.g = texcolor.r;
	finalColor.b = texcolor.r;
	finalColor.a = 1;
}
