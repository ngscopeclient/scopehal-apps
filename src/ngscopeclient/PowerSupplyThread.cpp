/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg                                                                          *
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
	@brief Implementation of PowerSupplyThread
 */
#include "ngscopeclient.h"
#include "pthread_compat.h"

using namespace std;

void PowerSupplyThread(PowerSupplyThreadArgs args)
{
	pthread_setname_np_compat("PSUThread");

	auto psu = args.psu;
	auto state = args.state;
	auto nchans = psu->GetPowerChannelCount();
	while(!*args.shuttingDown)
	{
		//Flush any pending commands
		psu->GetTransport()->FlushCommandQueue();

		//TODO: skip polling if the channel in question is off

		//Poll status
		for(int i=0; i<nchans; i++)
		{
			state->m_channelVoltage[i] = psu->GetPowerVoltageActual(i);
			state->m_channelCurrent[i] = psu->GetPowerCurrentActual(i);
			state->m_channelConstantCurrent[i] = psu->IsPowerConstantCurrent(i);
			state->m_channelFuseTripped[i] = psu->GetPowerOvercurrentShutdownTripped(i);
		}
		state->m_firstUpdateDone = true;

		//Cap update rate to 20 Hz
		this_thread::sleep_for(chrono::milliseconds(50));
	}
}
