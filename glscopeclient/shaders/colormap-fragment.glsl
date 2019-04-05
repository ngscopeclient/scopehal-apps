#version 130

in vec2 			texcoord;
uniform sampler2D	fbtex;
uniform float		r;
uniform float		g;
uniform float		b;

out vec4 			finalColor;

void main()
{
	//Look up the original color
	vec4 texcolor = texture(fbtex, vec2(texcoord));

	//Logarithmic shading
	float liny = texcolor.a * 2;
	//float y = pow(liny, 0.75);
	//y = min(y, 1);
	//y = max(y, 0);

	//convert to rgba
	finalColor.r = r * liny;
	finalColor.g = g * liny;
	finalColor.b = b * liny;
	finalColor.a = liny;
}
