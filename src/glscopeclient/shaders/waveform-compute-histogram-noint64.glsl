/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2020 Andrew D. Zonenberg                                                                          *
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
	@brief Waveform rendering shader for histogram waveforms without GL_ARB_gpu_shader_int64 support
 */

#version 430

//The output texture (for now, only alpha channel is used)
layout(binding=0, rgba32f) uniform image2D outputTex;

layout(std430, binding=1) buffer waveform_x
{
	uint xpos[];		//x position, in time ticks
						//actually 64-bit little endian signed ints
};

layout(std430, binding=4) buffer waveform_y
{
	float voltage[];	//y value of the sample, in volts
};

//Global configuration for the run
layout(std430, binding=2) buffer config
{
	uint innerXoff_lo;	//actually a 64-bit little endian signed int
	uint innerXoff_hi;

	uint windowHeight;
	uint windowWidth;
	uint memDepth;
	float alpha;
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

//All this just because most Intel integrated GPUs lack GL_ARB_gpu_shader_int64...
float FetchX(uint i)
{
	//Fetch the input
	uint xpos_lo = xpos[i*2];
	uint xpos_hi = xpos[i*2 + 1];
	uint offset_lo = innerXoff_lo;

	//Chop off the MSB from the low halves and sum them separately
	//(these might turn into carries)
	uint msb_pos = (xpos_lo >> 31);
	uint msb_off = (offset_lo >> 31);
	xpos_lo &= 0x7fffffff;
	offset_lo &= 0x7fffffff;
	uint msb_sum = msb_pos + msb_off;

	//Sum the low halves without MSBs (cannot generate a carry)
	uint sum_lo = xpos_lo + offset_lo;

	//Sum the high halves (no need to worry about carries)
	uint sum_hi = xpos_hi + innerXoff_hi;

	//Patch up the carries between halves
	msb_sum += (sum_lo >> 31);
	sum_lo &= 0x7fffffff;
	sum_lo |= (msb_sum & 1) << 31;
	sum_hi += (msb_sum >> 1);

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
}

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
	vec2 left = vec2(FetchX(istart)*xscale + xoff, (voltage[istart] + yoff)*yscale + ybase);
	vec2 right;
	for(uint i=istart; i<(memDepth-2); i++)
	{
		//Fetch coordinates of the current and upcoming sample
		right = vec2(FetchX(i+1)*xscale + xoff, (voltage[i+1] + yoff)*yscale + ybase);

		//If the current point is right of us, stop
		if(left.x > gl_GlobalInvocationID.x + 1)
			break;

		//If the upcoming point is still left of us, we're not there yet
		if(right.x < gl_GlobalInvocationID.x)
		{
			left = right;
			continue;
		}

		//Clip to window size
		float yval = min(left.y, MAX_HEIGHT);

		//Push current point down the pipeline
		left = right;

		//Fill in the space between min and max for this segment
		for(int y=0; y <= yval; y++)
			g_workingBuffer[gl_LocalInvocationID.x][y] = alpha;

		//TODO: decimation at very wide zooms?
	}

	//Copy working buffer to RGB output
	for(uint y=0; y<windowHeight; y++)
		imageStore(outputTex, ivec2(gl_GlobalInvocationID.x, y), vec4(0, 0, 0, g_workingBuffer[gl_LocalInvocationID.x][y]));
}
