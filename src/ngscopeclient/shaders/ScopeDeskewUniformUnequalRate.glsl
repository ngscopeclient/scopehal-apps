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

//Each block handles one correlation
#define ROWS_PER_BLOCK 1

//Min/max for the current sample
shared double g_partialSum[ROWS_PER_BLOCK];
shared int64_t g_partialSamples[ROWS_PER_BLOCK];

layout(local_size_x=32, local_size_y=ROWS_PER_BLOCK, local_size_z=1) in;

//Global configuration for the run
layout(std430, push_constant) uniform constants
{
	int64_t priTimescale;
	int64_t secTimescale;

	int64_t priTrigPhase;
	int64_t secTrigPhase;

	int64_t priLen;
	int64_t secLen;

	int64_t startingDelta;
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
	int64_t trigPhaseDelta = priTrigPhase - secTrigPhase;
	int64_t d = int64_t(gl_GlobalInvocationID.x) + startingDelta;
	int64_t deltaFs = (priTimescale * d) + trigPhaseDelta;

	//Loop over samples in the primary waveform, then correlate to secondary samples
	int64_t samplesProcessed = 0;
	int isecondary = 0;
	double partialSum = 0;
	for(int64_t i=int64_t(gl_LocalInvocationID.y); i<priLen; i += ROWS_PER_BLOCK)
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
		partialSum += priSamples[int(i)] * secSamples[isecondary];
		samplesProcessed ++;
	}

	//Output results from this thread
	g_partialSum[gl_LocalInvocationID.y] = partialSum;
	g_partialSamples[gl_LocalInvocationID.y] = samplesProcessed;

	//Block until all threads for this correlation have finished
	barrier();
	memoryBarrierShared();

	//Sum the results from all threads in the block
	double finalSum = 0;
	int64_t finalSamples = 0;
	for(int i=0; i<ROWS_PER_BLOCK; i++)
	{
		finalSum += g_partialSum[i];
		finalSamples += g_partialSamples[i];
	}

	//Output the final correlation
	corrOut[gl_GlobalInvocationID.x] = finalSum / finalSamples;
}
