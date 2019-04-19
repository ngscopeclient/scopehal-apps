#version 130

out vec4 finalColor;
uniform float alpha;

void main()
{
	finalColor = vec4(1, 1, 1, alpha);
}
