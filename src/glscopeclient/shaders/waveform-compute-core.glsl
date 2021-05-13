//Maximum height of a single waveform, in pixels.
//This is enough for a nearly fullscreen 4K window so should be plenty.
#define MAX_HEIGHT		2048

//Number of columns of pixels per thread block
#define COLS_PER_BLOCK	1
#define ROWS_PER_BLOCK	32

//The output texture (for now, only alpha channel is used)
layout(binding=0, rgba32f) uniform image2D outputTex;

//Indexes so we know which samples go to which X pixel range
layout(std430, binding=3) buffer index
{
	uint xind[];
};

//Shared buffer for the local working buffer (8 kB)
shared float g_workingBuffer[COLS_PER_BLOCK][MAX_HEIGHT];

//Min/max for the current sample
shared int g_blockmin[COLS_PER_BLOCK];
shared int g_blockmax[COLS_PER_BLOCK];
shared bool g_done[COLS_PER_BLOCK];
shared bool g_updating[COLS_PER_BLOCK];

layout(local_size_x=COLS_PER_BLOCK, local_size_y=ROWS_PER_BLOCK, local_size_z=1) in;

//Interpolate a Y coordinate
float InterpolateY(vec2 left, vec2 right, float slope, float x)
{
	return left.y + ( (x - left.x) * slope );
}

void main()
{
	uint i;
	uint istart;
	vec2 left;
	vec2 right;

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
			g_workingBuffer[gl_LocalInvocationID.x][y] = 0;
		else
		{
			vec4 rgba = imageLoad(outputTex, ivec2(gl_GlobalInvocationID.x, y));
			g_workingBuffer[gl_LocalInvocationID.x][y] = rgba.a * persistScale;
		}
	}

	//Setup for main loop
	if(gl_LocalInvocationID.y == 0)
	{
		//Clear loop variables
		g_done[gl_LocalInvocationID.x] = false;
		g_updating[gl_LocalInvocationID.x] = false;

		#ifdef DENSE_PACK
			istart = uint(floor(gl_GlobalInvocationID.x / xscale)) + offset_samples;
		#else
			istart = xind[gl_GlobalInvocationID.x];
		#endif


		i = istart;

		#ifdef ANALOG_PATH
			left = vec2(FetchX(istart) * xscale + xoff, (voltage[istart] + yoff)*yscale + ybase);
		#endif

		#ifdef DIGITAL_PATH
			left = vec2(FetchX(istart) * xscale + xoff, GetBoolean(istart)*yscale + ybase);
		#endif
	}

	barrier();
	memoryBarrierShared();

	//Main loop
	while(true)
	{
		//Main thread
		if(gl_LocalInvocationID.y == 0)
		{
			if(i < (memDepth-2) )
			{
				//Fetch coordinates of the current and upcoming sample

				#ifdef ANALOG_PATH
					right = vec2(FetchX(i+1) * xscale + xoff, (voltage[i+1] + yoff)*yscale + ybase);
				#endif

				#ifdef DIGITAL_PATH
					right = vec2(FetchX(i+1)*xscale + xoff, GetBoolean(i+1)*yscale + ybase);
				#endif

				//If the upcoming point is still left of us, we're not there yet
				if(right.x < gl_GlobalInvocationID.x)
					left = right;

				else
				{
					g_updating[gl_LocalInvocationID.x] = true;

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
						if(abs(right.x - gl_GlobalInvocationID.x) <= 1)
						{
							starty = left.y;
							endy = right.y;
						}

						//otherwise draw a single pixel
						else
						{
							starty = left.y;
							endy = left.y;
						}

					#endif

					#ifdef HISTOGRAM_PATH
						starty = 0;
						endy = left.y;
					#endif

					//Clip to window size
					starty = min(starty, MAX_HEIGHT);
					endy = min(endy, MAX_HEIGHT);

					//Sort Y coordinates from min to max
					g_blockmin[gl_LocalInvocationID.x] = int(min(starty, endy));
					g_blockmax[gl_LocalInvocationID.x] = int(max(starty, endy));

					//Push current point down the pipeline
					left = right;

					if(left.x > gl_GlobalInvocationID.x + 1)
						g_done[gl_LocalInvocationID.x] = true;
				}
			}

			else
				g_done[gl_LocalInvocationID.x] = true;

			i++;
		}

		barrier();
		memoryBarrierShared();

		//Only update if we need to
		if(g_updating[gl_LocalInvocationID.x])
		{
			//Parallel fill
			int ymin = g_blockmin[gl_LocalInvocationID.x];
			int ymax = g_blockmax[gl_LocalInvocationID.x];
			int len = ymax - ymin;
			for(uint y=gl_LocalInvocationID.y; y <= len; y += ROWS_PER_BLOCK)
			{
				#ifdef HISTOGRAM_PATH
					g_workingBuffer[gl_LocalInvocationID.x][ymin + y] = alpha;
				#else
					g_workingBuffer[gl_LocalInvocationID.x][ymin + y] += alpha;
				#endif
			}
		}

		if(g_done[gl_LocalInvocationID.x])
			break;
	}

	barrier();
	memoryBarrierShared();

	//Copy working buffer to RGB output
	if(gl_LocalInvocationID.y == 0)
	{
		for(uint y=0; y<windowHeight; y++)
		{
			imageStore(
				outputTex,
				ivec2(gl_GlobalInvocationID.x, y),
				vec4(0, 0, 0, g_workingBuffer[gl_LocalInvocationID.x][y]));
		}
	}
}
