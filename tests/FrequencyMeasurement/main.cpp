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
	@file
	@author Andrew D. Zonenberg
	@brief Unit test for FrequencyMeasurement filter
 */
#include "../../lib/scopehal/scopehal.h"
#include "../../lib/scopehal/TestWaveformSource.h"
#include "../../lib/scopehal/TestCase.h"
#include "../../lib/scopeprotocols/scopeprotocols.h"

using namespace std;

class FrequencyMeasurementTest : public TestCase
{
public:
	FrequencyMeasurementTest(int argc, char* argv[])
		: TestCase(argc, argv, "Frequency")
		, m_source(m_rng)
	{
		//Create the channels for our test
		m_scope.AddChannel(new OscilloscopeChannel(
			&m_scope, "CH1", OscilloscopeChannel::CHANNEL_TYPE_ANALOG, "#ffffff", 1, 0, true));
	}

	virtual ~FrequencyMeasurementTest()
	{
	}

	virtual bool Iteration(size_t i)
	{
		LogNotice("Iteration %zu\n", i);
		LogIndenter li;

		//Select random frequency, amplitude, and phase
		float gen_freq = uniform_real_distribution<float>(0.5e9, 5e9)(m_rng);
		float gen_period = 1e12 / gen_freq;
		float gen_amp = uniform_real_distribution<float>(0.01, 1)(m_rng);

		//Starting phase
		float start_phase = uniform_real_distribution<float>(-M_PI, M_PI)(m_rng);

		//Generate the input signal.
		//50 Gsps, 1M points, no added noise
		m_scope.GetChannel(0)->SetData(
			m_source.GenerateNoisySinewave(gen_amp, start_phase, gen_period, 20, 1000000, 0),
			0);

		Unit hz(Unit::UNIT_HZ);
		LogVerbose("Frequency: %s\n", hz.PrettyPrint(gen_freq).c_str());
		LogVerbose("Period:    %s\n", Unit(Unit::UNIT_PS).PrettyPrint(gen_period).c_str());
		LogVerbose("Amplitude: %s\n", Unit(Unit::UNIT_VOLTS).PrettyPrint(gen_amp).c_str());

		//Run the filter
		m_filter->SetInput("din", StreamDescriptor(m_scope.GetChannel(0), 0));
		m_filter->Refresh();

		//Get the output data
		auto data = dynamic_cast<AnalogWaveform*>(m_filter->GetData(0));
		if(!data)
		{
			LogError("Filter generated an invalid or null waveform\n");
			return false;
		}

		//Process the individual frequency measurements and sanity check them
		//TODO: check timestamps and durations of samples too
		float fmin = FLT_MAX;
		float fmax = 0;
		float avg = 0;
		for(auto f : data->m_samples)
		{
			fmin = min(fmin, (float)f);
			fmax = max(fmax, (float)f);
			avg += f;
		}
		avg /= data->m_samples.size();
		LogDebug("Results:\n");
		LogIndenter li2;
		float davg = gen_freq - avg;
		float dmin = gen_freq - fmin;
		float dmax = fmax - gen_freq;
		LogDebug("Min: %s (err = %s)\n", hz.PrettyPrint(fmin).c_str(), hz.PrettyPrint(dmin).c_str());
		LogDebug("Avg: %s (err = %s)\n", hz.PrettyPrint(avg).c_str(), hz.PrettyPrint(davg).c_str());
		LogDebug("Max: %s (err = %s)\n", hz.PrettyPrint(fmax).c_str(), hz.PrettyPrint(dmax).c_str());

		//Fail if the average frequency is more than 0.1% off the nominal
		if(fabs(davg) > 0.001 * gen_freq)
			return false;

		//Min and max should be within 5% of nominal
		if(fabs(dmin) > 0.05 * gen_freq)
			return false;
		if(fabs(dmax) > 0.05 * gen_freq)
			return false;

		//TODO: verify stdev?

		return true;
	}

	TestWaveformSource m_source;
};

int main(int argc, char* argv[])
{
	ScopeProtocolStaticInit();
	FrequencyMeasurementTest test(argc, argv);
	if(!test.Run())
		return -1;
	return 0;
}
