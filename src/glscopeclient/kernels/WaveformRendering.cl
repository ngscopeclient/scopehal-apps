/***********************************************************************************************************************
*                                                                                                                      *
* GLSCOPECLIENT v0.1                                                                                                   *
*                                                                                                                      *
* Copyright (c) 2012-2021 Andrew D. Zonenberg and contributors                                                         *
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

//Maximum height of a single waveform, in pixels.
//This is enough for a nearly fullscreen 4K window so should be plenty.
#define MAX_HEIGHT		2048

//Number of threads per column of pixels
#define ROWS_PER_BLOCK	64

/**
	@brief Render a dense-packed analog waveform

	@param plotRight		Right edge of displayed waveform
	@param width			Width of the image
	@param height			Height of the image
	@param firstcol			Index of the column corresponding to global ID of 0
							(may be nonzero if the image is more than CL_DEVICE_MAX_WORK_GROUP_SIZE pixels wide)
	@param depth			Number of samples in the waveform
	@param innerXoff		X offset (in samples)
	@param offsetSamples	X offset (in samples)
	@param alpha			Alpha value for intensity grading
	@param xoff				X offset (in X units)
	@param xscale			X scale (pixels/X unit)
	@param ybase			Y position of zero (for overlays etc)
	@param yscale			Y scale
	@param yoff				Y offset
	@param persistScale		Decay factor for persistence
	@param ypos				Y coordinates of output waveform
	@param outbuf			Output waveform buffer
 */
__kernel void RenderAnalogWaveform(
	unsigned int plotRight,
	unsigned int width,
	unsigned int height,
	unsigned long firstcol,
	unsigned long depth,
	unsigned long innerXoff,
	unsigned long offsetSamples,
	float alpha,
	unsigned long xoff,
	float xscale,
	float ybase,
	float yscale,
	float yoff,
	float persistScale,	//unimplemented for now
	__global float* ypos,
	__global float* outbuf
	)
{
	//Shared buffer for the current column of pixels (8 kB)
	__local float workingBuffer[MAX_HEIGHT];

	//Min/max for the current sample
	__local int blockmin[ROWS_PER_BLOCK];
	__local int blockmax[ROWS_PER_BLOCK];
	__local bool done;
	__local bool updating[ROWS_PER_BLOCK];

	//Don't do anything if we're off the right end of the buffer
	unsigned long x = get_global_id(0) + firstcol;
	if(x >= width)
		return;

	//TODO: more than one thread at a time
	unsigned long tid = get_local_id(1);
	if(tid > 0)
		return;

	//In buffer, but outside plot? Zero fill
	if(x >= plotRight)
	{
		for(unsigned long y=0; y<height; y++)
			outbuf[y*width + x] = 0;
		return;
	}

	//Demo fill
	for(unsigned long y=0; y<height; y++)
		outbuf[y*width + x] = (float)x / width;
}
