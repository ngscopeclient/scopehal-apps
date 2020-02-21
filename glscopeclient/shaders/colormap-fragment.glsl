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
	float y = pow(texcolor.a, 1.0 / 4);
	y = min(y, 2);
	y = max(y, 0);

	//convert to rgba
	finalColor.r = r * y;
	finalColor.g = g * y;
	finalColor.b = b * y;
	if(y > 0)
		finalColor.a = 1;
	else
		finalColor.a = 0;
}
