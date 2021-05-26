/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2021 Andrew D. Zonenberg                                                                          *
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
	@brief Waveform processing logic
 */
#include "glscopeclient.h"

using namespace std;

mutex g_waveformReadyMutex;
condition_variable g_waveformReadyCondition;

bool g_waveformReady = false;

void WaveformProcessingThread(OscilloscopeWindow* window)
{
	#ifndef _WIN32
	pthread_setname_np(pthread_self(), "WaveformProcessingThread");
	#endif

	while(!window->m_shuttingDown)
	{
		//Wait for data to be available from all scopes
		if(!window->CheckForPendingWaveforms())
		{
			this_thread::sleep_for(chrono::milliseconds(50));
			continue;
		}

		//We've got data. Download it.
		window->DownloadWaveforms();

		//Unblock the UI threads
		{
			lock_guard<mutex> lock(g_waveformReadyMutex);
			g_waveformReady = true;
		}
		g_waveformReadyCondition.notify_one();

		//Wait for the UI to say that it's processed the data and we can resume polling
		unique_lock<mutex> lock(g_waveformReadyMutex);
		g_waveformReadyCondition.wait(lock);
	}
}
