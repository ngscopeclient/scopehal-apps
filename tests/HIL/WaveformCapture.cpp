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
#include "HIL.h"

using namespace std;

class WaveformCaptureTest : public HILTest
{
public:
	WaveformCaptureTest()
	{}

	virtual ~WaveformCaptureTest()
	{}

	virtual void Run()
	{
		HardwareSetup();

		//Disable the output
		//then wait to make sure the output of the signal generator has turned off
		{
			LogNotice("Disable output\n");
			m_siggen->SendCommandQueued("SMA1:MUX OFF");
			SignalGeneratorSetupDone();
		}

		{
			LogNotice("Check trigger with no signal applied\n");
			LogIndenter li;

			//Arm the trigger.
			//It should NOT trigger as we have no signal coming in, make sure of this
			LogNotice("Arm trigger\n");
			m_scope->StartSingleTrigger();
			m_scope->GetTransport()->FlushCommandQueue();
			VerifyNoTrigger();
		}

		{
			LogNotice("Check trigger with signal applied\n");
			LogIndenter li;

			//Enable the output as a clock
			LogNotice("Enable clock output\n");
			m_siggen->SendCommandQueued("SMA1:MUX CLOCK");
			SignalGeneratorSetupDone();

			//Grab the waveform
			AcquireWaveformWithTimeout(0.25);
		}

		//Make sure it's plausible
		SanityCheckWaveform(0, 1000 * 1000);
	}
};

TEST_CASE("HIL_WaveformCapture")
{
	WaveformCaptureTest test;
	test.Run();
}
