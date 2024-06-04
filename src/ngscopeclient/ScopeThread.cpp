/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg and contributors                                                         *
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
	@brief Implementation of ScopeThread
 */
#include "ngscopeclient.h"
#include "pthread_compat.h"

using namespace std;

void ScopeThread(ScopeThreadArgs args)
{
	pthread_setname_np_compat("ScopeThread");
	auto sscope = dynamic_pointer_cast<SCPIOscilloscope>(args.scope);

	LogTrace("Initializing %s\n", args.scope->m_nickname.c_str());

	while(!*args.shuttingDown)
	{
		//Push any pending queued commands
		if(sscope)
			sscope->GetTransport()->FlushCommandQueue();

		//If the queue is too big, stop grabbing data
		size_t npending = args.scope->GetPendingWaveformCount();
		if(npending > 5)
		{
			LogTrace("Queue is too big, sleeping\n");
			this_thread::sleep_for(chrono::milliseconds(5));
			continue;
		}

		//If trigger isn't armed, don't even bother polling for a while.
		if(!args.scope->IsTriggerArmed())
		{
			//LogTrace("Scope isn't armed, sleeping\n");
			this_thread::sleep_for(chrono::milliseconds(5));
			continue;
		}

		//Grab data if it's ready
		auto stat = args.scope->PollTrigger();
		if(stat == Oscilloscope::TRIGGER_MODE_TRIGGERED)
			args.scope->AcquireData();
	}
}
