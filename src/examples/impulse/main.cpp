/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
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
	@file	main.cpp
	@brief	Impulse response calculator for S-parameters

	Run "impulse file.s2p"

	Output is a CSV with time-domain transform of the S-parameters at 1ps resolution.
 */

#include "../../../lib/scopehal/scopehal.h"
#include <ffts.h>

using namespace std;

int64_t GetGroupDelay(SParameterVector& vec);

int main(int argc, char* argv[])
{
	Severity console_verbosity = Severity::NOTICE;

	//Parse command-line arguments
	string fname;
	for(int i=1; i<argc; i++)
	{
		string s(argv[i]);

		//Let the logger eat its args first
		if(ParseLoggerArguments(i, argc, argv, console_verbosity))
			continue;

		if(s == "--help")
		{
			//not implemented
			return 0;
		}
		else if(s[0] == '-')
		{
			fprintf(stderr, "Unrecognized command-line argument \"%s\", use --help\n", s.c_str());
			return 1;
		}
		else
			fname = argv[i];
	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(console_verbosity));

	//Initialize
	AlignedAllocator< float, 64 > allocator;
	size_t npoints = 131072;
	size_t ps_per_sample = 1;
	double sample_ghz = 1000;
	double bin_hz = round((0.5f * sample_ghz * 1e9f) / npoints);
	ffts_plan_t* forwardPlan = ffts_init_1d_real(npoints, FFTS_FORWARD);
	ffts_plan_t* reversePlan = ffts_init_1d_real(npoints, FFTS_BACKWARD);

	//Load the S-parameters
	SParameters params;
	TouchstoneParser parser;
	if(!parser.Load(fname, params))
	{
		LogError("Couldn't open file\n");
		return 1;
	}

	//Generate the input waveform (TODO: impulse or step)
	float* din = allocator.allocate(npoints);
	size_t nmid = npoints/2;
	for(size_t i=0; i<npoints; i++)
	{
		if(i < nmid)
			din[i] = 0;
		else
			din[i] = 1;
	}

	//Do the forward FFT
	float* dfreq = allocator.allocate(npoints*2);
	ffts_execute(forwardPlan, din, dfreq);

	//Apply the S-parameter transformation to each channel
	float* dtfreq[2][2];
	for(int to=0; to<2; to++)
		for(int from=0; from<2; from++)
			dtfreq[to][from] = allocator.allocate(npoints*2);
	for(size_t i=0; i<npoints; i++)
	{
		for(int to=0; to<2; to++)
		{
			for(int from=0; from<2; from++)
			{
				auto point = params.SamplePoint(to+1, from+1, bin_hz * i);
				float sinval = sin(point.m_phase);
				float cosval = cos(point.m_phase);

				float real = dfreq[i*2 + 0];
				float imag = dfreq[i*2 + 1];

				dtfreq[to][from][i*2 + 0] = (real*cosval - imag*sinval) * point.m_amplitude;
				dtfreq[to][from][i*2 + 1] = (real*sinval + imag*cosval) * point.m_amplitude;
			}
		}
	}

	//Do the reverse FFTs
	float* dttime[2][2];
	for(int to=0; to<2; to++)
	{
		for(int from=0; from<2; from++)
		{
			dttime[to][from] = allocator.allocate(npoints);
			ffts_execute(reversePlan, dtfreq[to][from], dttime[to][from]);

			//Rescale
			for(size_t i=0; i<npoints; i++)
				dttime[to][from][i] /= npoints;
		}
	}

	//Calculate maximum group delay for the first few S21 bins (approx propagation delay of the channel)
	int64_t groupdelay_samples =ceil( GetGroupDelay(params[SPair(2,1)]) / ps_per_sample );

	//Write the output
	LogNotice("ps, s11, s21, s12, s22\n");
	float tstart = nmid;
	for(size_t i=groupdelay_samples; i<npoints; i++)
	{
		LogNotice("%.0f, %f, %f, %f, %f\n",
			(i*ps_per_sample) - tstart,
			dttime[0][0][i],
			dttime[1][0][i],
			dttime[0][1][i],
			dttime[1][1][i]
			);
	}

	//Clean up
	ffts_free(forwardPlan);
	ffts_free(reversePlan);
	allocator.deallocate(din);
	allocator.deallocate(dfreq);
	for(int to=0; to<2; to++)
	{
		for(int from=0; from<2; from++)
		{
			allocator.deallocate(dtfreq[to][from]);
			allocator.deallocate(dttime[to][from]);
		}
	}
}

int64_t GetGroupDelay(SParameterVector& vec)
{
	float max_delay = 0;
	for(size_t i=0; i<vec.size()-1 && i<50; i++)
		max_delay = max(max_delay, vec.GetGroupDelay(i));
	return max_delay * 1e12;
}
