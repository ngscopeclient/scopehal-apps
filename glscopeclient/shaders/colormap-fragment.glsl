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

	//Logarithmic shading?
	float liny = texcolor.a * 2;
	//float y = pow(liny, 0.75);
	//y = min(y, 1);
	//y = max(y, 0);

	//convert to rgba
	finalColor.r = r * liny;
	finalColor.g = g * liny;
	finalColor.b = b * liny;
	/*if(liny > 1)
		finalColor.a = 1;
	else*/ if(liny > 0)
		finalColor.a = 1.0;
	else
		finalColor.a = 0;
}
