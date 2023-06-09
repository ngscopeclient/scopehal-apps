/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2023 Andrew D. Zonenberg                                                                          *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

/**
	@file
	@brief Waveform rendering shader
 */

#version 430
#pragma shader_stage(compute)

#extension GL_ARB_compute_shader : require
#extension GL_ARB_shader_storage_buffer_object : require
#ifdef HAS_INT64
#extension GL_ARB_gpu_shader_int64 : require
#endif

//Maximum height of a single waveform, in pixels.
//This is enough for a nearly fullscreen 4K window so should be plenty.
#define MAX_HEIGHT		2048

//Number of threads per column of pixels
#define ROWS_PER_BLOCK	64

//Shared buffer for the local working buffer (8 kB)
shared float g_workingBuffer[MAX_HEIGHT];

//Min/max for the current sample
shared int g_blockmin[ROWS_PER_BLOCK];
shared int g_blockmax[ROWS_PER_BLOCK];
shared bool g_done;
shared bool g_updating[ROWS_PER_BLOCK];

layout(local_size_x=1, local_size_y=ROWS_PER_BLOCK, local_size_z=1) in;

//Global configuration for the run
layout(std430, push_constant) uniform constants
{
#ifdef HAS_INT64
	int64_t innerXoff;
#else
	uint innerXoff_lo;	//actually a 64-bit little endian signed int
	uint innerXoff_hi;
#endif
	uint windowHeight;
	uint windowWidth;
	uint memDepth;
	uint offset_samples;
	float alpha;
	float xoff;
	float xscale;
	float ybase;
	float yscale;
	float yoff;
	float persistScale;
};

//The output texture data
layout(std430, binding=0) buffer outputTex
{
	float outval[];
};

#ifdef ANALOG_PATH
	layout(std430, binding=1) buffer waveform_y
	{
		float voltage[];  //y value of the sample, in volts
	};
#endif /* ANALOG_PATH */

#ifdef DIGITAL_PATH
	layout(std430, binding=1) buffer waveform_y
	{
		int voltage[]; //y value of the sample, boolean 0/1 for 4 samples per int
	};

	int GetBoolean(uint i)
	{
		int block = voltage[i/4];
		uint nbyte = (i & 3);
		return (block >> (8*nbyte) ) & 0xff;
	}
#endif /* DIGITAL_PATH */

#ifndef DENSE_PACK
layout(std430, binding=2) buffer waveform_x
{
#ifdef HAS_INT64
	int64_t xpos[];  //x position, in time ticks
#else
	uint xpos[];		//x position, in time ticks
						//actually 64-bit little endian signed ints
#endif
};

//Indexes so we know which samples go to which X pixel range
layout(std430, binding=3) buffer index
{
	uint xind[];
};
#endif

#ifndef DENSE_PACK
layout(std430, binding=4) buffer durs
{
#ifdef HAS_INT64
	int64_t durations[];
	#define FETCH_DURATION(i) float(durations[i])
#else
	uint durations[];
	#define FETCH_DURATION(i) ((float(durations[i*2 + 1]) * 4294967296.0) + float(durations[i]))
#endif
};
#else
#define FETCH_DURATION(i) float(1)
#endif

#ifdef NO_INTERPOLATION
	#ifndef HISTOGRAM_PATH
		#undef USE_NEXT_COORDS
		// No interpolation requested, use exact bounds
	#else
		#define USE_NEXT_COORDS
		// Histogram needs next coords to draw bar
	#endif
#else
	#define USE_NEXT_COORDS
	// Use coordinates of next point to allow interpolation
	// TODO: Somehow avoid doing this if the waveform is not continous
#endif

#ifdef USE_NEXT_COORDS
	#define ADDTL_NEEDED_SAMPLES 1
#else
	#define ADDTL_NEEDED_SAMPLES 0
#endif

float FetchX(uint i)
{
#ifdef HAS_INT64
	#ifdef DENSE_PACK
		return float(int64_t(i) + innerXoff);
	#else
		return float(xpos[i] + innerXoff);
	#endif
#else
	//All this just because most Intel integrated GPUs lack GL_ARB_gpu_shader_int64...
	#ifdef DENSE_PACK
		uint xpos_lo = i;
		uint xpos_hi = 0;
	#else
		//Fetch the input
		uint xpos_lo = xpos[i*2];
		uint xpos_hi = xpos[i*2 + 1];
	#endif
	uint offset_lo = innerXoff_lo;

	//Sum the low halves
	uint carry;
	uint sum_lo = uaddCarry(xpos_lo, offset_lo, carry);

	//Sum the high halves with carry in
	uint sum_hi = xpos_hi + innerXoff_hi + carry;

	//If MSB is 1, we're negative.
	//Calculate the twos complement by flipping all the bits.
	//To complete the complement we need to add 1, but that comes later.
	bool negative = ( (sum_hi & 0x80000000) == 0x80000000 );
	if(negative)
	{
		sum_lo = ~sum_lo;
		sum_hi = ~sum_hi;
	}

	//Convert back to floating point
	float f = (float(sum_hi) * 4294967296.0) + float(sum_lo);
	if(negative)
		f = -f + 1;
	return f;
#endif
}

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
	if(gl_GlobalInvocationID.x >= windowWidth)
		return;
	if(memDepth < (1 + ADDTL_NEEDED_SAMPLES))
		return;

	//Clear (or persistence load) working buffer
	for(uint y=gl_LocalInvocationID.y; y < windowHeight; y += ROWS_PER_BLOCK)
	{
		if(persistScale == 0)
			g_workingBuffer[y] = 0;
		else
		{
			g_workingBuffer[y] =
				outval[(windowWidth * y) + gl_GlobalInvocationID.x] * persistScale;
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
		if(i < (memDepth - ADDTL_NEEDED_SAMPLES) )
		{
			//Fetch coordinates
			#ifdef ANALOG_PATH
				vec2 left = vec2(FetchX(i) * xscale + xoff, (voltage[i] + yoff)*yscale + ybase);

				#ifdef USE_NEXT_COORDS
					vec2 right = vec2(FetchX(i+1) * xscale + xoff, (voltage[i+1] + yoff)*yscale + ybase);
				#else
					vec2 right = left;
					right.x += FETCH_DURATION(i) * xscale;
				#endif
			#endif

			#ifdef DIGITAL_PATH
				vec2 left = vec2(FetchX(i) * xscale + xoff, GetBoolean(i)*yscale + ybase);

				#ifdef USE_NEXT_COORDS
					vec2 right = vec2(FetchX(i+1)*xscale + xoff, GetBoolean(i+1)*yscale + ybase);
				#else
					vec2 right = left;
					right.x += FETCH_DURATION(i) * xscale;
				#endif
			#endif

			//Skip offscreen samples
			if( (right.x >= gl_GlobalInvocationID.x) && (left.x <= gl_GlobalInvocationID.x + 1) )
			{
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

				//If start and end are both off screen, nothing to draw
				if( ( (starty < 0) && (endy < 0) ) ||
					( (starty >= windowHeight) && (endy >= windowHeight) ) )
				{
					g_updating[gl_LocalInvocationID.y] = false;
				}

				//Something is visible. Clip to window size in case anything is partially offscreen
				else
				{
					g_updating[gl_LocalInvocationID.y] = true;

					starty = min(starty, windowHeight - 1);
					endy = min(endy, windowHeight - 1);
					starty = max(starty, 0);
					endy = max(endy, 0);

					//Sort Y coordinates from min to max
					g_blockmin[gl_LocalInvocationID.y] = int(min(starty, endy));
					g_blockmax[gl_LocalInvocationID.y] = int(max(starty, endy));
				}
			}
			else
				g_updating[gl_LocalInvocationID.y] = false;

			//Check if we're at the end of the pixel
			if(right.x > gl_GlobalInvocationID.x + 1)
				g_done = true;
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

	//Copy working buffer to float[] output
	for(uint y=gl_LocalInvocationID.y; y<windowHeight; y+= ROWS_PER_BLOCK)
	{
		outval[(windowWidth * y) + gl_GlobalInvocationID.x] = g_workingBuffer[y];
	}
}
