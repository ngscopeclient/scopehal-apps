#version 150

in vec2 			texcoord;
uniform sampler2D	fbtex;

out vec4 			finalColor;

void main()
{
	//Look up the original color
	vec4 texcolor = texture(fbtex, vec2(texcoord));

	//Logarithmic shading
	float liny = texcolor.r;
	float y = pow(liny, 0.75);

	//clip
	y = min(y, 1);
	y = max(y, 0);

	//convert to rgb
	finalColor.r = y;
	finalColor.g = y;
	finalColor.b = 0;
	finalColor.a = 1;
}
