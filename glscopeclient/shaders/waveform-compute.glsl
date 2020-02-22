#version 430

//The output texture (for now, only alpha channel is used)
layout(binding=0, rgba32f) uniform image2D outputTex;

//Voltage data
layout(std430, binding=1) buffer waveform
{
	struct
	{
		float x;		//x pixel position (fractional)
		float voltage;	//y value of this sample, in pixels
	} data[];
};

//Global configuration for the run
layout(std430, binding=2) buffer config
{
	uint windowHeight;
	uint windowWidth;
	uint memDepth;
	uint alpha_scaled;
};

//Indexes so we know which samples go to which X pixel range
layout(std430, binding=3) buffer index
{
	uint xind[];
};

layout(local_size_x=128, local_size_y=1, local_size_z=1) in;

//Interpolate a Y coordinate
float InterpolateY(vec2 left, vec2 right, float slope, float x)
{
	return left.y + ( (x - left.x) * slope );
}

//Maximum height of a single waveform, in pixels.
//This is enough for a nearly fullscreen 4K window so should be plenty.
#define MAX_HEIGHT 2048

void main()
{
	//Make sure image isn't too big for our hard coded max
	//TODO: truncate in this case??
	float g_workingBuffer[MAX_HEIGHT];
	if(windowHeight > MAX_HEIGHT)
		return;

	//Save some constants
	float x = gl_GlobalInvocationID.x;
	if(x > windowWidth)
		return;
	float alpha = float(alpha_scaled) / 256;

	//Clear column to blank
	for(uint y=0; y<windowHeight; y++)
		g_workingBuffer[y] = 0;

	//Loop over the waveform, starting at the leftmost point that overlaps this column
	uint istart = xind[gl_GlobalInvocationID.x];
	vec2 left = vec2(data[istart].x, data[istart].voltage);
	vec2 right;
	for(uint i=istart; i<(memDepth-1); i++)
	{
		//Fetch coordinates of the current and upcoming sample
		right = vec2(data[i+1].x, data[i+1].voltage);

		//If the current point is right of us, stop
		if(left.x > x+1)
			break;

		//If the upcoming point is still left of us, we're not there yet
		if(right.x < x)
		{
			left = right;
			continue;
		}

		//To start, assume we're drawing the entire segment
		float starty = left.y;
		float endy = right.y;

		//Interpolate if either end is outside our column
		float slope = (right.y - left.y) / (right.x - left.x);
		if(left.x < x)
			starty = InterpolateY(left, right, slope, x);
		if(right.x > x+1)
			endy = InterpolateY(left, right, slope, x+1);

		//Sort Y coordinates from min to max
		int ymin = int(min(starty, endy));
		int ymax = int(max(starty, endy));

		//Push current point down the pipeline
		left = right;

		//Fill in the space between min and max for this segment
		for(int y=ymin; y <= ymax; y++)
		{
			g_workingBuffer[y] += alpha;
		}

		//TODO: antialiasing
		//TODO: decimation at very wide zooms
	}

	//Copy working buffer to RGB output
	for(uint y=0; y<windowHeight; y++)
		imageStore(outputTex, ivec2(gl_GlobalInvocationID.x, y), vec4(0, 0, 0, g_workingBuffer[y]));
}
