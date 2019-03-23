#version 150

in vec2 			texcoord;
uniform sampler2D	fbtex;

out vec4 			finalColor;

void main()
{
	//Look up the original color
	vec4 texcolor = texture(fbtex, vec2(texcoord));

	//alpha blend
	finalColor.r = texcolor.r;
	finalColor.g = texcolor.g;
	finalColor.b = texcolor.b;
	finalColor.a = 0.95;
}
