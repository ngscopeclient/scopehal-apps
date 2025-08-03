/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2025 Andrew D. Zonenberg and contributors                                                         *
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

#define X_BLOCK_SIZE 64

layout(local_size_x=X_BLOCK_SIZE, local_size_y=1, local_size_z=1) in;

//Global configuration for the run
layout(std430, push_constant) uniform constants
{
	int64_t priTimescale;
	int64_t secTimescale;

	int64_t trigPhaseDelta;

	int		startingDelta;
	int		numDeltas;

	int		priLen;
	int		secLen;
};

//The output data
layout(std430, binding=0) restrict writeonly buffer corr
{
	float[] corrOut;
};

//Input sample data
layout(std430, binding=1) restrict readonly buffer primary
{
	float priSamples[];
};

layout(std430, binding=2) restrict readonly buffer secondary
{
	float secSamples[];
};

shared float priSampleCache[X_BLOCK_SIZE];

void main()
{
	uint nthread = (gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x) + gl_GlobalInvocationID.x;
	if(nthread >= numDeltas)
		return;

	//Convert delta from samples of the primary waveform to femtoseconds
	int d = int(nthread) + startingDelta;
	int64_t deltaFs = (priTimescale * int64_t(d)) + trigPhaseDelta;
	int phaseshift = int(deltaFs / priTimescale);

	//Loop over samples in the primary waveform, then correlate to secondary samples
	int samplesProcessed = 0;
	float sum = 0;
	for(int i=0; i<priLen; i += X_BLOCK_SIZE)
	{
		//Prefetch primary samples in parallel
		if( (i+gl_LocalInvocationID.x) < priLen)
			priSampleCache[gl_LocalInvocationID.x] = priSamples[i+gl_LocalInvocationID.x];
		barrier();
		memoryBarrierShared();

		//TODO: prefetch secondary too?

		for(int j=0; j<X_BLOCK_SIZE; j++)
		{
			//Make sure we're not going off the end of the primary
			int index = i+j;
			if(index >= priLen)
				break;

			//Target timestamp in the secondary waveform
			int target = index + phaseshift;

			//If off the start of the waveform, skip it
			if(target < 0)
				continue;
			if(target >= secLen)
				break;

			//Do the actual cross-correlation
			sum += priSampleCache[j] * secSamples[target];
			samplesProcessed ++;
		}
	}

	//Output the final correlation
	corrOut[nthread] = sum / samplesProcessed;
}
