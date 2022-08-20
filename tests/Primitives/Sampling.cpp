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
	@brief Unit test for SampleOn* primitives
 */
#include <catch2/catch.hpp>

#include "../../lib/scopehal/scopehal.h"
#include "../../lib/scopehal/TestWaveformSource.h"
#include "../../lib/scopeprotocols/scopeprotocols.h"
#include "Primitives.h"

using namespace std;

TEST_CASE("Primitive_SampleOnRisingEdges")
{
	const size_t wavelen = 1000000;

	SECTION("DigitalWaveform")
	{
		//Generate a random data/clock waveform
		DigitalWaveform data;
		DigitalWaveform clock;
		data.m_timescale = 5;
		clock.m_timescale = 5;
		DigitalWaveform samples_expected;
		uniform_int_distribution<int> edgeprob(0, 3);
		uniform_int_distribution<int> dataprob(0, 1);
		size_t nsamples = 0;
		bool last_was_high = false;
		for(size_t i=0; i<wavelen; i++)
		{
			LogIndenter li;

			//75% chance of emitting a random data bit with clock low.
			//Always emit a 0 bit for the first clock sample, since rising edges at time zero
			//are indistinguishable from a constant-high clock.
			//Also, always emit a clock-low sample if the clock was high, since we need a low period before
			//the next rising edge.
			if( (edgeprob(g_rng) == 0) || (i == 0) || last_was_high)
			{
				//Create the data bit
				data.m_offsets.push_back(i);
				data.m_durations.push_back(1);
				data.m_samples.push_back(dataprob(g_rng));

				//Create the clock bit
				//TODO: generate test waveforms with multi-cycle clock samples
				clock.m_offsets.push_back(i);
				clock.m_durations.push_back(1);
				clock.m_samples.push_back(0);

				last_was_high = false;
			}

			//25% chance of emitting a rising clock edge with the same data value as last clock
			else
			{
				bool value = data.m_samples[data.m_samples.size()-1];

				//Create the data bit
				data.m_offsets.push_back(i);
				data.m_durations.push_back(1);
				data.m_samples.push_back(value);

				//Create the clock bit
				//TODO: generate test waveforms with multi-cycle clock samples
				clock.m_offsets.push_back(i);
				clock.m_durations.push_back(1);
				clock.m_samples.push_back(1);

				//Extend the last data bit, if present
				if(nsamples)
				{
					samples_expected.m_durations[nsamples-1] =
						(i * data.m_timescale) - samples_expected.m_offsets[nsamples-1];
				}

				//Save this as an expected data bit
				//Duration is 1 until we get another clock edge.
				//The last sample in the waveform always has duration 1, because we don't have an endpoint.
				//TODO: should we extend to end of the waveform instead?
				samples_expected.m_samples.push_back(value);
				samples_expected.m_offsets.push_back(i * data.m_timescale);
				samples_expected.m_durations.push_back(1);
				nsamples ++;

				last_was_high = true;
			}
		}

		//Sample it
		DigitalWaveform samples;
		Filter::SampleOnRisingEdges(&data, &clock, samples);

		//Initial sanity check: we should have the same number of data bits as we generated,
		//and all sizes should be consistent
		REQUIRE(nsamples == samples.m_offsets.size());
		REQUIRE(nsamples == samples.m_durations.size());
		REQUIRE(nsamples == samples.m_samples.size());

		//Check each of the bits
		for(size_t i=0; i<nsamples; i++)
		{
			REQUIRE(samples.m_offsets[i] == samples_expected.m_offsets[i]);
			REQUIRE(samples.m_durations[i] == samples_expected.m_durations[i]);
			REQUIRE(samples.m_samples[i] == samples_expected.m_samples[i]);
		}
	}

	//TODO: add test for DigitalBusWaveform version

	//TODO: Add test for AnalogWaveform version
}
