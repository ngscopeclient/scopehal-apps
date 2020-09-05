#version 450
#extension GL_ARB_gpu_shader_int64 : require

//The output texture (for now, only alpha channel is used)
layout(binding=0, rgba32f) uniform image2D outputTex;

layout(std430, binding=1) buffer waveform_x
{
	int64_t xpos[];		//x position, in time ticks
};

layout(std430, binding=4) buffer waveform_y
{
	float voltage[];	//y value of this sample, in pixels
};

//Global configuration for the run
layout(std430, binding=2) buffer config
{
	int64_t innerXoff;
	uint windowHeight;
	uint windowWidth;
	uint memDepth;
	float alpha;
	uint digital;
	float xoff;
	float xscale;
	float ybase;
	float yscale;
	float yoff;
};

//Indexes so we know which samples go to which X pixel range
layout(std430, binding=3) buffer index
{
	uint xind[];
};

//Maximum height of a single waveform, in pixels.
//This is enough for a nearly fullscreen 4K window so should be plenty.
#define MAX_HEIGHT		2048

//Number of columns of pixels per thread block
#define COLS_PER_BLOCK	2

layout(local_size_x=COLS_PER_BLOCK, local_size_y=1, local_size_z=1) in;

//Interpolate a Y coordinate
float InterpolateY(vec2 left, vec2 right, float slope, float x)
{
	return left.y + ( (x - left.x) * slope );
}

/*
NEW IDEA
Multiple threads per X coordinate (say, 32 - 1 warp)
Parallel fetch base[i+z] and atomically increment local memory

Each local has a 2D shared array
Assuming 96 KB shared memory, we can fit a total of 24K float32 temp pixels
Assuming 2K max line height, that's up to 12 pixels of width per local
*/

//Shared buffer for the local working buffer
shared float g_workingBuffer[COLS_PER_BLOCK][MAX_HEIGHT];

void main()
{
	//Abort if window height is too big, or if we're off the end of the window
	if(windowHeight > MAX_HEIGHT)
		return;
	if(gl_GlobalInvocationID.x > windowWidth)
		return;

	//Clear column to blank in the first thread of the block
	if(gl_LocalInvocationID.y == 0)
	{
		for(uint y=0; y<windowHeight; y++)
			g_workingBuffer[gl_LocalInvocationID.x][y] = 0;
	}
	barrier();
	memoryBarrierShared();

	//Loop over the waveform, starting at the leftmost point that overlaps this column
	uint istart = xind[gl_GlobalInvocationID.x];
	vec2 left = vec2(float(xpos[istart] + innerXoff) * xscale + xoff, (voltage[istart] + yoff)*yscale + ybase);
	vec2 right;
	for(uint i=istart; i<(memDepth-1); i++)
	{
		//Fetch coordinates of the current and upcoming sample
		right = vec2(float(xpos[i+1] + innerXoff)*xscale + xoff, (voltage[i+1] + yoff)*yscale + ybase);

		//If the current point is right of us, stop
		if(left.x > gl_GlobalInvocationID.x + 1)
			break;

		//If the upcoming point is still left of us, we're not there yet
		if(right.x < gl_GlobalInvocationID.x)
		{
			left = right;
			continue;
		}

		//To start, assume we're drawing the entire segment
		float starty = left.y;
		float endy = right.y;

		//Interpolate analog signals if either end is outside our column
		if(digital == 0)
		{
			float slope = (right.y - left.y) / (right.x - left.x);
			if(left.x < gl_GlobalInvocationID.x)
				starty = InterpolateY(left, right, slope, gl_GlobalInvocationID.x);
			if(right.x > gl_GlobalInvocationID.x + 1)
				endy = InterpolateY(left, right, slope, gl_GlobalInvocationID.x + 1);
		}

		//Digital signal - do not interpolate
		else
		{
			//Draw the vertical line exactly one sample wide at edges
		}

		//Clip to window size
		starty = min(starty, MAX_HEIGHT);
		endy = min(endy, MAX_HEIGHT);

		//Sort Y coordinates from min to max
		int ymin = int(min(starty, endy));
		int ymax = int(max(starty, endy));

		//Push current point down the pipeline
		left = right;

		//Fill in the space between min and max for this segment
		for(int y=ymin; y <= ymax; y++)
			g_workingBuffer[gl_LocalInvocationID.x][y] += alpha;

		//TODO: antialiasing
		//TODO: decimation at very wide zooms
	}

	//Copy working buffer to RGB output
	for(uint y=0; y<windowHeight; y++)
		imageStore(outputTex, ivec2(gl_GlobalInvocationID.x, y), vec4(0, 0, 0, g_workingBuffer[gl_LocalInvocationID.x][y]));
}
