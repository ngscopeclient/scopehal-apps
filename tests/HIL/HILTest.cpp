/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
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

#include "HIL.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

HILTest::HILTest()
{
	LogNotice("Connecting to signal generator board\n");
	m_siggen = make_unique<SCPISocketTransport>(g_siggenPath);
	auto siginfo = m_siggen->SendCommandQueuedWithReply("*IDN?");
	LogNotice("siginfo = %s\n", siginfo.c_str());

	//TODO: connect to mux board once we have it

	//Try to connect to the scope
	m_scope = make_unique<ThunderScopeOscilloscope>(new SCPITwinLanTransport(g_scopePath));
	LogNotice("Connected to scope make=%s model=%s serial=%s\n",
		m_scope->GetVendor().c_str(), m_scope->GetName().c_str(), m_scope->GetSerial().c_str());
}

HILTest::~HILTest()
{
	//Delete filters
	for(auto n : m_nodes)
		n->DetachInputs();
	for(auto n : m_nodes)
		delete n;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test setup helpers

void HILTest::HardwareSetup(size_t memoryDepth)
{
	LogNotice("Hardware setup\n");
	LogIndenter li;

	//Default setup for one digital input
	SetupScopeForSingleChannelDigitalInput(memoryDepth);
}

UniformAnalogWaveform* HILTest::SanityCheckWaveform(size_t channel, size_t expectedDepth)
{
	LogNotice("Validate acquired waveform\n");
	LogIndenter li;

	//Verify it's plausible (non-null, right number of samples)
	auto data = m_scope->GetChannel(channel)->GetData(0);
	REQUIRE(data != nullptr);
	auto dwfm = dynamic_cast<UniformAnalogWaveform*>(data);
	REQUIRE(dwfm != nullptr);
	LogNotice("Data is a non-null uniform analog waveform, %zu samples\n", dwfm->size());
	REQUIRE(dwfm->size() == expectedDepth);
	return dwfm;
}

/**
	@brief Helper for initializing a scope with a single digital input channel
 */
void HILTest::SetupScopeForSingleChannelDigitalInput(size_t memoryDepth)
{
	//Enable channel 1 and only channel 1
	LogNotice("Channel setup\n");
	m_scope->EnableChannel(0);
	m_scope->DisableChannel(1);
	m_scope->DisableChannel(2);
	m_scope->DisableChannel(3);

	//Set up timebase: requested depth @ 1 Gsps
	LogNotice("Timebase setup\n");
	m_scope->SetSampleDepth(memoryDepth);
	m_scope->SetSampleRate(1000 * 1000 * 1000);

	//Set up trigger: rising edge, 900 mV
	LogNotice("Trigger setup\n");
	auto trig = new EdgeTrigger(m_scope.get());
	trig->SetType(EdgeTrigger::EDGE_RISING);
	trig->SetInput(0, m_scope->GetChannel(0));
	trig->SetLevel(0.9);
	m_scope->SetTrigger(trig);
	m_scope->SetTriggerOffset(0);
	m_scope->PushTrigger();

	//Stop the trigger if it had been previously armed
	m_scope->Stop();

	//Configure the channel
	LogNotice("Frontend setup\n");
	m_scope->SetChannelCoupling(0, OscilloscopeChannel::COUPLE_DC_1M);
	m_scope->SetChannelAttenuation(0, 1.0);
	m_scope->SetChannelBandwidthLimit(0, 0);
	m_scope->SetChannelVoltageRange(0, 0, 2.25);
	m_scope->SetChannelOffset(0, 0, -0.9);

	m_scope->GetTransport()->FlushCommandQueue();

	//Sanity check only CH1 is enabled
	REQUIRE(m_scope->GetEnabledChannelCount() == 1);
}

/**
	@brief Helper to make sure the scope does not trigger
 */
void HILTest::VerifyNoTrigger()
{
	LogNotice("Wait, verify no trigger\n");
	this_thread::sleep_for(chrono::milliseconds(250));
	auto stat = m_scope->PollTrigger();

	if(stat != Oscilloscope::TRIGGER_MODE_RUN)
		LogError("Scope should not have triggered, but did anyway\n");
	REQUIRE(stat == Oscilloscope::TRIGGER_MODE_RUN);
}

/**
	@brief Acquire a waveform and complain if it doesn't trigger within the timeout period
 */
bool HILTest::AcquireWaveformWithTimeout(float timeoutSec)
{
	double now = GetTime();
	double deadline = now + timeoutSec;

	//Make sure we got a waveform
	LogNotice("Wait for waveform\n");
	while(true)
	{
		auto stat = m_scope->PollTrigger();
		if(stat == Oscilloscope::TRIGGER_MODE_TRIGGERED)
			break;

		now = GetTime();
		if(now >= deadline)
		{
			REQUIRE(now < deadline);
			return false;
		}
	}

	//Grab it
	LogNotice("Download waveform\n");
	m_scope->AcquireData();
	while(true)
	{
		m_scope->BackgroundProcessing();
		if(m_scope->GetPendingWaveformCount() >= 1)
			break;

		now = GetTime();
		if(now >= deadline)
		{
			REQUIRE(now < deadline);
			return false;
		}
	}

	m_scope->PopPendingWaveform();
	return true;
}

void HILTest::RunFilterGraph()
{
	LogNotice("Filter graph\n");
	LogIndenter li;

	//Run it
	double start = GetTime();
	m_executor.RunBlocking(m_nodes);
	double dt = GetTime() - start;
	LogNotice("Filter graph took %.3f ms\n", dt * 1000);

	for(auto n : m_nodes)
	{
		if(n->HasMessages())
			LogError("Node %s has errors\n", dynamic_cast<Filter*>(n)->GetDisplayName().c_str());

		REQUIRE(!n->HasMessages());
	}
}
