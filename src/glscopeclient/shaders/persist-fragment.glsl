#version 130

out vec4 			finalColor;

void main()
{
	//Output a black overlay with no color
	finalColor.r = 0;
	finalColor.g = 0;
	finalColor.b = 0;
	finalColor.a = 0;
}
