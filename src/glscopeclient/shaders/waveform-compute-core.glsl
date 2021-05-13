//Maximum height of a single waveform, in pixels.
//This is enough for a nearly fullscreen 4K window so should be plenty.
#define MAX_HEIGHT		2048

//Number of threads per column of pixels
#define ROWS_PER_BLOCK	64

//The output texture (for now, only alpha channel is used)
layout(binding=0, rgba32f) uniform image2D outputTex;

//Indexes so we know which samples go to which X pixel range
layout(std430, binding=3) buffer index
{
	uint xind[];
};

//Shared buffer for the local working buffer (8 kB)
shared float g_workingBuffer[MAX_HEIGHT];

//Min/max for the current sample
shared int g_blockmin[ROWS_PER_BLOCK];
shared int g_blockmax[ROWS_PER_BLOCK];
shared bool g_done;
shared bool g_updating[ROWS_PER_BLOCK];

layout(local_size_x=1, local_size_y=ROWS_PER_BLOCK, local_size_z=1) in;

//Interpolate a Y coordinate
float InterpolateY(vec2 left, vec2 right, float slope, float x)
{
	return left.y + ( (x - left.x) * slope );
}

void main()
{
	//Abort if window height is too big, or if we're off the end of the window
	if(windowHeight > MAX_HEIGHT)
		return;
	if(gl_GlobalInvocationID.x > windowWidth)
		return;
	if(memDepth < 2)
		return;

	//Clear (or persistence load) working buffer
	for(uint y=gl_LocalInvocationID.y; y < windowHeight; y += ROWS_PER_BLOCK)
	{
		if(persistScale == 0)
			g_workingBuffer[y] = 0;
		else
		{
			vec4 rgba = imageLoad(outputTex, ivec2(gl_GlobalInvocationID.x, y));
			g_workingBuffer[y] = rgba.a * persistScale;
		}
	}

	//Setup for main loop
	if(gl_LocalInvocationID.y == 0)
		g_done = false;

	#ifdef DENSE_PACK
		uint istart = uint(floor(gl_GlobalInvocationID.x / xscale)) + offset_samples;
		uint iend = uint(floor((gl_GlobalInvocationID.x + 1) / xscale)) + offset_samples;
		if(iend <= 0)
			g_done = true;
	#else
		uint istart = xind[gl_GlobalInvocationID.x];
		if( (gl_GlobalInvocationID.x + 1) < windowWidth)
		{
			uint iend = xind[gl_GlobalInvocationID.x + 1];
			if(iend <= 0)
				g_done = true;
		}
	#endif
	uint i = istart + gl_GlobalInvocationID.y;

	//Main loop
	while(true)
	{
		//Main thread
		if(i < (memDepth-1) )
		{
			//Fetch coordinates
			#ifdef ANALOG_PATH
				vec2 left = vec2(FetchX(i) * xscale + xoff, (voltage[i] + yoff)*yscale + ybase);
				vec2 right = vec2(FetchX(i+1) * xscale + xoff, (voltage[i+1] + yoff)*yscale + ybase);
			#endif

			#ifdef DIGITAL_PATH
				vec2 left = vec2(FetchX(i) * xscale + xoff, GetBoolean(i)*yscale + ybase);
				vec2 right = vec2(FetchX(i+1)*xscale + xoff, GetBoolean(i+1)*yscale + ybase);
			#endif

			//Skip offscreen samples
			if( (right.x >= gl_GlobalInvocationID.x) && (left.x <= gl_GlobalInvocationID.x + 1) )
			{
				g_updating[gl_LocalInvocationID.y] = true;

				//To start, assume we're drawing the entire segment
				float starty = left.y;
				float endy = right.y;

				#ifdef ANALOG_PATH

					#ifndef NO_INTERPOLATION

						//Interpolate analog signals if either end is outside our column
						float slope = (right.y - left.y) / (right.x - left.x);
						if(left.x < gl_GlobalInvocationID.x)
							starty = InterpolateY(left, right, slope, gl_GlobalInvocationID.x);
						if(right.x > gl_GlobalInvocationID.x + 1)
							endy = InterpolateY(left, right, slope, gl_GlobalInvocationID.x + 1);

					#endif

				#endif

				#ifdef DIGITAL_PATH

					//If we are very near the right edge, draw vertical line
					starty = left.y;
					if(abs(right.x - gl_GlobalInvocationID.x) <= 1)
						endy = right.y;

					//otherwise draw a single pixel
					else
						endy = left.y;

				#endif

				#ifdef HISTOGRAM_PATH
					starty = 0;
					endy = left.y;
				#endif

				//Clip to window size
				starty = min(starty, MAX_HEIGHT);
				endy = min(endy, MAX_HEIGHT);

				//Sort Y coordinates from min to max
				g_blockmin[gl_LocalInvocationID.y] = int(min(starty, endy));
				g_blockmax[gl_LocalInvocationID.y] = int(max(starty, endy));

				//Check if we're at the end of the pixel
				if(right.x > gl_GlobalInvocationID.x + 1)
					g_done = true;
			}
			else
				g_updating[gl_LocalInvocationID.y] = false;
		}

		else
		{
			g_done = true;
			g_updating[gl_LocalInvocationID.y] = false;
		}

		i += ROWS_PER_BLOCK;

		//Only update if we need to
		for(int y = 0; y<ROWS_PER_BLOCK; y++)
		{
			barrier();
			memoryBarrierShared();

			if(g_updating[y])
			{
				//Parallel fill
				int ymin = g_blockmin[y];
				int len = g_blockmax[y] - ymin;
				for(uint y=gl_LocalInvocationID.y; y <= len; y += ROWS_PER_BLOCK)
				{
					#ifdef HISTOGRAM_PATH
						g_workingBuffer[ymin + y] = alpha;
					#else
						g_workingBuffer[ymin + y] += alpha;
					#endif
				}
			}
		}

		if(g_done)
			break;
	}

	barrier();
	memoryBarrierShared();

	//Copy working buffer to RGB output
	for(uint y=gl_LocalInvocationID.y; y<windowHeight; y+= ROWS_PER_BLOCK)
	{
		imageStore(
			outputTex,
			ivec2(gl_GlobalInvocationID.x, y),
			vec4(0, 0, 0, g_workingBuffer[y]));
	}
}
