/***********************************************************************************************************************
*                                                                                                                      *
* libscopehal v0.1                                                                                                     *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg and contributors                                                         *
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
	@author Andrew D. Zonenberg
	@brief Unit test for Blackman-Harris window
 */
#include <catch2/catch.hpp>

#include "../../lib/scopehal/scopehal.h"
#include "../../lib/scopehal/TestWaveformSource.h"
#include "../../lib/scopeprotocols/scopeprotocols.h"
#include "Primitives.h"

using namespace std;

#ifdef __x86_64__

TEST_CASE("Primitive_BlackmanHarrisWindow")
{
	//const size_t wavelen = 1000000;
	const size_t wavelen = 64;

	const size_t niter = 8;
	for(size_t i=0; i<niter; i++)
	{
		SECTION(string("Iteration ") + to_string(i))
		{
			LogVerbose("Iteration %zu\n", i);
			LogIndenter li;

			//Generate random input waveform
			auto rdist = uniform_real_distribution<float>(-1, 1);
			vector<float> din;
			din.resize(wavelen);
			for(size_t j=0; j<wavelen; j++)
				din[j] = 1;//rdist(g_rng);

			//Run the normal version
			vector<float> dout_normal;
			dout_normal.resize(wavelen);
			double start = GetTime();
			FFTFilter::BlackmanHarrisWindow(&din[0], wavelen, &dout_normal[0]);
			double tbase = GetTime() - start;
			LogVerbose("CPU (no AVX): %.2f ms\n", tbase * 1000);

			//Run the AVX version and compare results
			if(g_hasAvx2)
			{
				vector<float> dout_avx2;
				dout_avx2.resize(wavelen);

				start = GetTime();
				FFTFilter::BlackmanHarrisWindowAVX2(&din[0], wavelen, &dout_avx2[0]);
				double dt = GetTime() - start;
				LogVerbose("CPU (AVX2)  : %.2f ms, %.2fx speedup\n", dt * 1000, tbase / dt);

				for(size_t j=0; j<wavelen; j++)
					REQUIRE(fabs(dout_normal[j] - dout_avx2[j]) < 1e-5f);
			}

		}
	}
}

#endif
