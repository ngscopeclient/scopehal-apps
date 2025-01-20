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
	if(!inst)
	{
		LogError("InstrumentThread called with null instrument (bug)\n");
		return;
	}

	auto session = args.session;

	//Extract type-specified fields
	auto load = dynamic_pointer_cast<Load>(inst);
	auto scope = dynamic_pointer_cast<Oscilloscope>(inst);
	auto bert = dynamic_pointer_cast<SCPIBERT>(inst);
	auto meter = dynamic_pointer_cast<SCPIMultimeter>(inst);
	auto rfgen = dynamic_pointer_cast<SCPIRFSignalGenerator>(inst);
	auto misc = dynamic_pointer_cast<SCPIMiscInstrument>(inst);
	auto psu = dynamic_pointer_cast<SCPIPowerSupply>(inst);
	auto awg = dynamic_pointer_cast<FunctionGenerator>(inst);
	auto loadstate = args.loadstate;
	auto meterstate = args.meterstate;
	auto bertstate = args.bertstate;
	auto psustate = args.psustate;
	auto awgstate = args.awgstate;

	bool triggerUpToDate = false;

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
				if(!triggerUpToDate)
				{	// Check for trigger state change
					auto stat = scope->PollTrigger();
					session->GetInstrumentConnectionState(inst)->m_lastTriggerState = stat;
					if(stat == Oscilloscope::TRIGGER_MODE_STOP || stat == Oscilloscope::TRIGGER_MODE_RUN || stat == Oscilloscope::TRIGGER_MODE_TRIGGERED)
					{	// Final state
						triggerUpToDate = true;
					}
				}
			}

			//Grab data if it's ready
			//TODO: how is this going to play with reading realtime BER from BERT+scope deviecs?
			else
			{
				auto stat = scope->PollTrigger();
				session->GetInstrumentConnectionState(inst)->m_lastTriggerState = stat;
				if(stat == Oscilloscope::TRIGGER_MODE_TRIGGERED)
					scope->AcquireData();
				triggerUpToDate = false;
			}
		}

		//Always acquire data from non-scope instruments
		else
			inst->AcquireData();

		//Populate scalar channel and do other instrument-specific processing
		if(psu && psustate)
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
				psustate->m_channelOn[i] = psu->GetPowerChannelActive(i);

				session->MarkChannelDirty(pchan);
			}

			if(psu->SupportsMasterOutputSwitching())
				psustate->m_masterEnable = psu->GetMasterPowerEnable();

			psustate->m_firstUpdateDone = true;
		}
		if(load && loadstate)
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
		if(meter && meterstate)
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
		if(bert && bertstate)
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
		// Read and cache FunctionGenerator settings
		if(awg && awgstate)
		{
			//Read status for channels that need it
			for(size_t i=0; i<awg->GetChannelCount(); i++)
			{
				if(awgstate->m_needsUpdate[i])
				{
					Unit volts(Unit::UNIT_VOLTS);

					//Skip non-awg channels
					auto awgchan = dynamic_cast<FunctionGeneratorChannel*>(awg->GetChannel(i));
					if(!awgchan)
						continue;
					awgstate->m_channelActive[i] = awg->GetFunctionChannelActive(i);
					awgstate->m_channelAmplitude[i] = awg->GetFunctionChannelAmplitude(i);
					awgstate->m_channelOffset[i] = awg->GetFunctionChannelOffset(i);
					awgstate->m_channelFrequency[i] = awg->GetFunctionChannelFrequency(i);
					awgstate->m_channelShape[i] = awg->GetFunctionChannelShape(i);
					awgstate->m_channelOutputImpedance[i] = awg->GetFunctionChannelOutputImpedance(i);
					session->MarkChannelDirty(awgchan);

					awgstate->m_needsUpdate[i] = false;
				}

			}
		}

		//TODO: does this make sense to do in the instrument thread?
		session->RefreshDirtyFiltersNonblocking();

		//Rate limit to 100 Hz to avoid saturating CPU with polls
		//(this also provides a yield point for the gui thread to get mutex ownership etc)
		this_thread::sleep_for(chrono::milliseconds(10));
	}

	LogTrace("Shutting down instrument thread\n");
}
