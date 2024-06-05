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
	auto scope = dynamic_pointer_cast<Oscilloscope>(inst);
	auto bert = dynamic_pointer_cast<SCPIBERT>(inst);
	auto meter = dynamic_pointer_cast<SCPIMultimeter>(inst);
	auto rfgen = dynamic_pointer_cast<SCPIRFSignalGenerator>(inst);
	auto misc = dynamic_pointer_cast<SCPIMiscInstrument>(inst);
	auto psu = dynamic_pointer_cast<SCPIPowerSupply>(inst);
	auto loadstate = args.loadstate;
	auto meterstate = args.meterstate;
	auto bertstate = args.bertstate;
	auto psustate = args.psustate;

	while(!*args.shuttingDown)
	{
		//Flush any pending commands
		inst->GetTransport()->FlushCommandQueue();

		//Scope processing
		if(scope)
		{
			//If the queue is too big, stop grabbing data
			size_t npending = scope->GetPendingWaveformCount();
			if(npending > 5)
			{
				LogTrace("Queue is too big, sleeping\n");
				this_thread::sleep_for(chrono::milliseconds(5));
			}

			//If trigger isn't armed, don't even bother polling for a while.
			else if(!scope->IsTriggerArmed())
			{
				//LogTrace("Scope isn't armed, sleeping\n");
				this_thread::sleep_for(chrono::milliseconds(5));
			}

			//Grab data if it's ready
			else
			{
				auto stat = scope->PollTrigger();
				if(stat == Oscilloscope::TRIGGER_MODE_TRIGGERED)
					scope->AcquireData();
			}
		}

		//Always acquire data from non-scope instruments
		else
			inst->AcquireData();

		//Populate scalar channel and do other instrument-specific processing
		if(psu)
		{
			//Poll status
			for(size_t i=0; i<psu->GetChannelCount(); i++)
			{
				//Skip non-power channels
				auto pchan = dynamic_cast<PowerSupplyChannel*>(psu->GetChannel(i));
				if(!pchan)
					continue;

				psustate->m_channelVoltage[i] = pchan->GetVoltageMeasured();
				psustate->m_channelCurrent[i] = pchan->GetCurrentMeasured();
				psustate->m_channelConstantCurrent[i] = psu->IsPowerConstantCurrent(i);
				psustate->m_channelFuseTripped[i] = psu->GetPowerOvercurrentShutdownTripped(i);

				session->MarkChannelDirty(pchan);
			}
			psustate->m_firstUpdateDone = true;
		}
		if(load)
		{
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
			auto chan = dynamic_cast<MultimeterChannel*>(meter->GetChannel(meter->GetCurrentMeterChannel()));
			if(chan)
			{
				meterstate->m_primaryMeasurement = chan->GetPrimaryValue();
				meterstate->m_secondaryMeasurement = chan->GetSecondaryValue();
				meterstate->m_firstUpdateDone = true;

				session->MarkChannelDirty(chan);
			}
		}
		if(misc || rfgen || bert)
		{
			for(size_t i=0; i<inst->GetChannelCount(); i++)
			{
				auto chan = inst->GetChannel(i);
				if(chan)
					session->MarkChannelDirty(chan);
			}
		}
		if(bert)
		{
			//Check if we have any pending acquisition requests
			for(size_t i=0; i<bert->GetChannelCount(); i++)
			{
				if(bertstate->m_horzBathtubScanPending[i].exchange(false))
				{
					Unit fs(Unit::UNIT_FS);
					auto expected = bert->GetExpectedBathtubCaptureTime(i);
					LogTrace("Starting bathtub scan, expecting to take %s\n", fs.PrettyPrint(expected).c_str());

					double start = GetTime();
					bert->MeasureHBathtub(i);
					double dt = (GetTime() - start) * FS_PER_SECOND;

					LogTrace("Scan actually took %s\n", fs.PrettyPrint(dt).c_str());
				}

				if(bertstate->m_eyeScanPending[i].exchange(false))
				{
					Unit fs(Unit::UNIT_FS);
					auto expected = bert->GetExpectedEyeCaptureTime(i);
					LogTrace("Starting eye scan, expecting to take %s\n", fs.PrettyPrint(expected).c_str());

					double start = GetTime();
					bert->MeasureEye(i);
					double dt = (GetTime() - start) * FS_PER_SECOND;

					LogTrace("Scan actually took %s\n", fs.PrettyPrint(dt).c_str());
				}
			}

			bertstate->m_firstUpdateDone = true;
		}

		//TODO: does this make sense to do in the instrument thread?
		session->RefreshDirtyFiltersNonblocking();
	}
}
