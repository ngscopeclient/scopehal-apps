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

/**
	@file
	@author Andrew D. Zonenberg
	@brief Unit test for basic waveform acquisition
 */
#ifdef _CATCH2_V3
#include <catch2/catch_all.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "HIL.h"

using namespace std;

TEST_CASE("HIL_WaveformCapture")
{
	//TODO: actually have scopehal driver here instead of doing it raw
	LogNotice("Connecting to signal generator board\n");
	SCPISocketTransport siggen(g_siggenPath);
	auto siginfo = siggen.SendCommandQueuedWithReply("*IDN?");
	LogNotice("siginfo = %s\n", siginfo.c_str());

	//TODO: Configure mux board we dont yet have

	//Try to connect to the scope
	ThunderScopeOscilloscope scope(new SCPITwinLanTransport(g_scopePath));
	LogNotice("Connected to scope make=%s model=%s serial=%s\n",
		scope.GetVendor().c_str(), scope.GetName().c_str(), scope.GetSerial().c_str());

	{
		LogNotice("Hardware setup\n");
		LogIndenter li;

		//Default setup for one digital input
		SetupScopeForSingleChannelDigitalInput(scope);

		//Disable the output
		//then wait to make sure the output of the signal generator has turned off
		LogNotice("Disable output\n");
		siggen.SendCommandQueued("SMA1:MUX OFF");		//for some reason we have to cycle from off to prbs and back to actually be off??
		siggen.SendCommandQueued("SMA1:MUX PRBS7");
		siggen.SendCommandQueued("SMA1:MUX OFF");
		siggen.FlushCommandQueue();
		this_thread::sleep_for(chrono::milliseconds(100));
	}

	{
		LogNotice("Check trigger with no signal applied\n");
		LogIndenter li;

		//Arm the trigger.
		//It should NOT trigger as we have no signal coming in, make sure of this
		LogNotice("Arm trigger\n");
		scope.StartSingleTrigger();
		scope.GetTransport()->FlushCommandQueue();
		VerifyNoTrigger(scope);
	}

	{
		LogNotice("Check trigger with signal applied\n");
		LogIndenter li;

		//Enable the output as a clock
		LogNotice("Enable clock output\n");
		siggen.SendCommandQueued("SMA1:MUX CLOCK");
		siggen.FlushCommandQueue();

		//Grab the waveform
		AcquireWaveformWithTimeout(scope, 0.25);
	}

	{
		LogNotice("Validate acquired waveform\n");
		LogIndenter li;

		//Verify it's plausible (non-null, right number of samples)
		auto data = scope.GetChannel(0)->GetData(0);
		REQUIRE(data != nullptr);
		auto dwfm = dynamic_cast<UniformAnalogWaveform*>(data);
		REQUIRE(dwfm != nullptr);
		LogNotice("Data is a non-null uniform analog waveform, %zu samples\n", dwfm->size());
		REQUIRE(dwfm->size() == (1000 * 1000));
	}
}
