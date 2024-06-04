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
#include "Session.h"
#include "LoadChannel.h"

using namespace std;

void InstrumentThread(InstrumentThreadArgs args)
{
	pthread_setname_np_compat("InstrumentThread");

	auto inst = args.inst;
	auto session = args.session;

	//Extract type-specified fields
	auto load = dynamic_pointer_cast<Load>(inst);
	auto meter = dynamic_pointer_cast<SCPIMultimeter>(inst);
	auto loadstate = args.loadstate;
	auto meterstate = args.meterstate;

	while(!*args.shuttingDown)
	{
		//Flush any pending commands
		inst->GetTransport()->FlushCommandQueue();

		if(load)
		{
			//Read stuff
			load->AcquireData();

			//Poll status
			for(size_t i=0; i<load->GetChannelCount(); i++)
			{
				auto lchan = dynamic_cast<LoadChannel*>(load->GetChannel(i));

				loadstate->m_channelVoltage[i] = lchan->GetScalarValue(LoadChannel::STREAM_VOLTAGE_MEASURED);
				loadstate->m_channelCurrent[i] = lchan->GetScalarValue(LoadChannel::STREAM_CURRENT_MEASURED);

				session->MarkChannelDirty(lchan);
			}
			loadstate->m_firstUpdateDone = true;
		}

		if(meter)
		{
			//Acquire scalar values from hardware
			meter->AcquireData();

			//Poll status
			auto chan = dynamic_cast<MultimeterChannel*>(meter->GetChannel(meter->GetCurrentMeterChannel()));
			if(chan)
			{
				meterstate->m_primaryMeasurement = chan->GetPrimaryValue();
				meterstate->m_secondaryMeasurement = chan->GetSecondaryValue();
				meterstate->m_firstUpdateDone = true;

				session->MarkChannelDirty(chan);
			}
		}
	}
}
