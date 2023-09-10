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

//for now, no fallback for no-int64
#extension GL_ARB_gpu_shader_int64 : require

layout(local_size_x=32, local_size_y=1, local_size_z=1) in;

//Global configuration for the run
layout(std430, push_constant) uniform constants
{
	int64_t priTimescale;
	int64_t secTimescale;

	int64_t trigPhaseDelta;

	int		startingDelta;

	int		priLen;
	int		secLen;
};

//The output data
layout(std430, binding=0) buffer corr
{
	double[] corrOut;
};

//Input sample data
layout(std430, binding=1) buffer primary
{
	float priSamples[];
};

layout(std430, binding=2) buffer secondary
{
	float secSamples[];
};

void main()
{
	//Convert delta from samples of the primary waveform to femtoseconds
	int d = int(gl_GlobalInvocationID.x) + startingDelta;
	int64_t deltaFs = (priTimescale * int64_t(d)) + trigPhaseDelta;

	//Loop over samples in the primary waveform, then correlate to secondary samples
	int samplesProcessed = 0;
	int isecondary = 0;
	double sum = 0;
	for(int i=0; i<priLen; i++)
	{
		//Target timestamp in the secondary waveform
		int64_t target = i * priTimescale + deltaFs;

		//If off the start of the waveform, skip it
		if(target < 0)
			continue;

		//Skip secondary samples if the current secondary sample ends before the primary sample starts
		//TODO: optimize this
		bool done = false;
		while( ((isecondary + 1) *	secTimescale) < target)
		{
			isecondary ++;

			//If off the end of the waveform, stop
			if(isecondary >= secLen)
			{
				done = true;
				break;
			}
		}
		if(done)
			break;

		//Do the actual cross-correlation
		sum += priSamples[i] * secSamples[isecondary];
		samplesProcessed ++;
	}

	//Output the final correlation
	corrOut[gl_GlobalInvocationID.x] = sum / samplesProcessed;
}
