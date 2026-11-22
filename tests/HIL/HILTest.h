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

#ifndef HILTest_h
#define HILTest_h

/**
	@brief Base class for hardware-in-loop tests
 */
class HILTest
{
public:
	HILTest();
	virtual ~HILTest();

	virtual void Run() =0;
	virtual void HardwareSetup(
		size_t memoryDepth = 1000 * 1000,
		bool sel0 = false,
		bool sel1 = false,
		bool sel2 = false,
		bool sel3 = false);

protected:
	void SetupRelayMuxes(bool sel0, bool sel1, bool sel2, bool sel3);
	void SetupScopeForSingleChannelDigitalInput(size_t memoryDepth = 1000 * 1000);
	bool AcquireWaveformWithTimeout(float timeoutSec);
	void VerifyNoTrigger();
	void RunFilterGraph();

	void SignalGeneratorSetupDone()
	{
		m_siggen->FlushCommandQueue();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	UniformAnalogWaveform* SanityCheckWaveform(size_t channel, size_t expectedDepth);

	std::unique_ptr<SCPISocketTransport> m_siggen;
	std::unique_ptr<SCPISocketTransport> m_mux;
	std::unique_ptr<ThunderScopeOscilloscope> m_scope;

	FilterGraphExecutor m_executor;
	std::set<FlowGraphNode*> m_nodes;
};

#endif
