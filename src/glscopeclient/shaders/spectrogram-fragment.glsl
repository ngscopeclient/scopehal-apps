#version 130

in vec2 			texcoord;
uniform sampler2D	fbtex;
uniform sampler2D	ramp;

out vec4 			finalColor;

void main()
{
	if( (texcoord.x < 0) || (texcoord.x > 1) )
	{
		//discard;
		finalColor.r = 1;
		finalColor.g = 0;
		finalColor.b = 0;
		finalColor.a = 1;
		return;
	}

	//Look up the intensity value and clamp it
	vec4 yvec = texture(fbtex, vec2(texcoord));
	float y = yvec.r;
	if( (texcoord.x < 0) || (texcoord.y > 1) )
	if(y >= 0.99)
		y = 0.99;

	//Look up the actual color
	vec2 pos;
	pos.x = y;
	pos.y = 0.5;
	vec4 color = texture(ramp, pos);

	finalColor.r = color.r;
	finalColor.g = color.g;
	finalColor.b = color.b;
	finalColor.a = 1;
}
