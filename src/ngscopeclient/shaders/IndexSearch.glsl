/***********************************************************************************************************************
*                                                                                                                      *
* libscopehal                                                                                                          *
*                                                                                                                      *
* Copyright (c) 2012-2026 Andrew D. Zonenberg and contributors                                                         *
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
#extension GL_ARB_gpu_shader_int64 : require

layout(std430, binding=0) restrict readonly buffer buf_din
{
	int64_t din[];
};

layout(std430, binding=1) restrict writeonly buffer buf_results
{
	uint results[];
};

layout(std430, push_constant) uniform constants
{
	int64_t offset_samples;
	float xscale;
	uint len;
	uint w;
};

layout(local_size_x=64, local_size_y=1, local_size_z=1) in;

void main()
{
	//Get thread index and bounds check
	if(gl_GlobalInvocationID.x >= w)
		return;

	//Get timestamp of the first sample in this thread's block
	int64_t dx = int64_t(floor(float(gl_GlobalInvocationID.x) / xscale));
	int64_t target = dx + offset_samples;

	//Binary search for the first clock edge after this sample
	uint pos = len/2;
	uint last_lo = 0;
	uint last_hi = len-1;
	uint iclk = 0;
	if(len > 0)
	{
		//Clip if out of range
		if(din[0] >= target)
			iclk = 0;
		else if(din[last_hi] < target)
			iclk = len-1;

		//Main loop
		else
		{
			while(true)
			{
				//Stop if we've bracketed the target
				if( (last_hi - last_lo) <= 1)
				{
					iclk = last_lo;
					break;
				}

				//Move down
				if(din[pos] > target)
				{
					uint delta = pos - last_lo;
					last_hi = pos;
					pos = last_lo + delta/2;
				}

				//Move up
				else
				{
					uint delta = last_hi - pos;
					last_lo = pos;
					pos = last_hi - delta/2;
				}
			}
		}
	}

	//We want one before the target
	iclk --;

	results[gl_GlobalInvocationID.x] = iclk;
}
