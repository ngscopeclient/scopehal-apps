/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2023 Andrew D. Zonenberg and contributors                                                         *
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

#version 430
#pragma shader_stage(compute)

layout(std430, binding=0) restrict readonly buffer buf_pixels
{
	float pixels[];
};

layout(binding=1, rgba32f) uniform image2D outputTex;

layout(binding=2) uniform sampler2D colorRamp;

layout(std430, push_constant) uniform constants
{
	uint width;
	uint height;
	uint outwidth;
	uint outheight;
	uint offset_samples;
	float xscale;
};

layout(local_size_x=64, local_size_y=1, local_size_z=1) in;

void main()
{
	if(gl_GlobalInvocationID.x >= outwidth)
		return;
	if(gl_GlobalInvocationID.y >= outheight)
		return;

	//Move the entire output display down if needed, so topmost (newest) row is always visible
	uint yreal = gl_GlobalInvocationID.y + (height - outheight);

	//Figure out which input pixel(s) contribute to this output pixel
	uint istart = uint(floor(gl_GlobalInvocationID.x / xscale)) + offset_samples;
	uint iend = uint(floor((gl_GlobalInvocationID.x + 1) / xscale)) + offset_samples;

	//Cap number of FFT bins per pixel if really zoomed out
	uint maxbins = 256;
	if( (iend - istart) > maxbins)
		iend = istart + maxbins;

	float clampedValue = 0;

	//If out of bounds, nothing to do
	if( (iend < 0) || (istart >= width) )
	{
	}

	else
	{
		istart = max(0, istart);
		iend = max(0, iend);

		istart = min(width-1, istart);
		iend = min(width-1, iend);

		//Intensity graded grayscale input
		//Highest value of the input is our output (this keeps peaks from fading away as we zoom out)
		float pixval = 0;
		for(uint i=istart; i <= iend; i++)
			pixval = max(pixval, pixels[yreal*width + i]);

		//Clamp to texture bounds
		clampedValue = min(pixval, 0.99);
	}

	//Write final output
	vec4 colorOut = texture(colorRamp, vec2(clampedValue + (0.5 / 255.0), 0.5));
	imageStore(
		outputTex,
		ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y),
		colorOut);
}
