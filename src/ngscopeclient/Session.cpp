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
	@brief Implementation of Session
 */
#include "ngscopeclient.h"
#include "Session.h"
#include "MainWindow.h"
#include "FunctionGeneratorDialog.h"
#include "MultimeterDialog.h"
#include "PowerSupplyDialog.h"
#include "RFGeneratorDialog.h"

extern Event g_waveformReadyEvent;
extern Event g_waveformProcessedEvent;
extern Event g_rerenderDoneEvent;

extern std::shared_mutex g_vulkanActivityMutex;

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

Session::Session(MainWindow* wnd)
	: m_mainWindow(wnd)
	, m_shuttingDown(false)
	, m_modifiedSinceLastSave(false)
	, m_tArm(0)
	, m_tPrimaryTrigger(0)
	, m_triggerArmed(false)
	, m_triggerOneShot(false)
	, m_multiScopeFreeRun(false)
	, m_lastFilterGraphExecTime(0)
{
}

Session::~Session()
{
	Clear();
}

/**
	@brief Terminate all background threads for instruments

	You must call Clear() after calling this function, however it's OK to do other cleanup in between.

	The reason for the split is that canceling the background threads is needed to prevent rendering or waveform
	processing from happening while we're in the middle of destroying stuff. But we can't clear the scopes etc until
	we've deleted all of the views and waveform groups as they hold onto references to them.
 */
void Session::ClearBackgroundThreads()
{
	LogTrace("Clearing background threads\n");

	//Signal our threads to exit
	//The sooner we do this, the faster they'll exit.
	m_shuttingDown = true;

	//Stop the trigger so there's no pending waveforms
	StopTrigger();

	//Clear our trigger state
	//Important to signal the WaveformProcessingThread so it doesn't block waiting on response that's not going to come
	m_triggerArmed = false;
	g_waveformReadyEvent.Clear();
	g_rerenderDoneEvent.Clear();
	g_waveformProcessedEvent.Signal();

	//Block until our processing threads exit
	for(auto& t : m_threads)
		t->join();
	if(m_waveformThread)
		m_waveformThread->join();
	m_waveformThread = nullptr;
	m_threads.clear();

	//Clear shutdown flag in case we're reusing the session object
	m_shuttingDown = false;
}

/**
	@brief Clears all session state and returns the object to an empty state
 */
void Session::Clear()
{
	LogTrace("Clearing session\n");
	LogIndenter li;

	lock_guard<recursive_mutex> lock(m_waveformDataMutex);

	ClearBackgroundThreads();

	//TODO: do we need to lock the mutex now that all of the background threads should have terminated?
	//Might be redundant.
	lock_guard<mutex> lock2(m_scopeMutex);

	//Delete scopes once we've terminated the threads
	for(auto scope : m_oscilloscopes)
		delete scope;
	m_oscilloscopes.clear();
	m_psus.clear();
	m_rfgenerators.clear();
	m_meters.clear();
	m_scopeDeskewCal.clear();

	//Reset state
	m_triggerOneShot = false;
	m_multiScopeFreeRun = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Instrument management

void Session::AddOscilloscope(Oscilloscope* scope)
{
	lock_guard<mutex> lock(m_scopeMutex);

	m_modifiedSinceLastSave = true;
	m_oscilloscopes.push_back(scope);

	m_threads.push_back(make_unique<thread>(ScopeThread, scope, &m_shuttingDown));

	m_mainWindow->AddToRecentInstrumentList(dynamic_cast<SCPIOscilloscope*>(scope));
	m_mainWindow->OnScopeAdded(scope);

	if(m_waveformThread == nullptr)
		m_waveformThread = make_unique<thread>(WaveformThread, this, &m_shuttingDown);
}

/**
	@brief Adds a power supply to the session
 */
void Session::AddPowerSupply(SCPIPowerSupply* psu)
{
	m_modifiedSinceLastSave = true;

	//Create shared PSU state
	auto state = make_shared<PowerSupplyState>(psu->GetPowerChannelCount());
	m_psus[psu] = make_unique<PowerSupplyConnectionState>(psu, state);

	//Add the dialog to view/control it
	m_mainWindow->AddDialog(make_shared<PowerSupplyDialog>(psu, state, this));

	m_mainWindow->AddToRecentInstrumentList(psu);
}

/**
	@brief Removes a power supply from the session
 */
void Session::RemovePowerSupply(SCPIPowerSupply* psu)
{
	m_modifiedSinceLastSave = true;
	m_psus.erase(psu);
}

/**
	@brief Adds a multimeter to the session
 */
void Session::AddMultimeter(SCPIMultimeter* meter)
{
	m_modifiedSinceLastSave = true;

	//Create shared meter state
	auto state = make_shared<MultimeterState>();
	m_meters[meter] = make_unique<MultimeterConnectionState>(meter, state);

	//Add the dialog to view/control it
	m_mainWindow->AddDialog(make_shared<MultimeterDialog>(meter, state, this));

	m_mainWindow->AddToRecentInstrumentList(meter);
}

/**
	@brief Removes a multimeter from the session
 */
void Session::RemoveMultimeter(SCPIMultimeter* meter)
{
	m_modifiedSinceLastSave = true;
	m_meters.erase(meter);
}

/**
	@brief Adds a function generator to the session
 */
void Session::AddFunctionGenerator(SCPIFunctionGenerator* generator)
{
	m_modifiedSinceLastSave = true;

	m_generators.push_back(generator);
	m_mainWindow->AddDialog(make_shared<FunctionGeneratorDialog>(generator, this));

	m_mainWindow->AddToRecentInstrumentList(generator);
}

/**
	@brief Removes a function generator from the session
 */
void Session::RemoveFunctionGenerator(SCPIFunctionGenerator* generator)
{
	m_modifiedSinceLastSave = true;

	for(size_t i=0; i<m_generators.size(); i++)
	{
		if(m_generators[i] == generator)
		{
			m_generators.erase(m_generators.begin() + i);
			break;
		}
	}

	//Free it iff it's not part of an oscilloscope or RF signal generator
	if( (dynamic_cast<Oscilloscope*>(generator) == nullptr) && (dynamic_cast<RFSignalGenerator*>(generator) == nullptr) )
		delete generator;
}

/**
	@brief Adds an RF signal generator to the session
 */
void Session::AddRFGenerator(SCPIRFSignalGenerator* generator)
{
	m_modifiedSinceLastSave = true;

	//Create shared meter state
	auto state = make_shared<RFSignalGeneratorState>(generator->GetChannelCount());
	m_rfgenerators[generator] = make_unique<RFSignalGeneratorConnectionState>(generator, state);

	m_mainWindow->AddDialog(make_shared<RFGeneratorDialog>(generator, state, this));

	m_mainWindow->AddToRecentInstrumentList(generator);
}

/**
	@brief Removes an RF signal from the session
 */
void Session::RemoveRFGenerator(SCPIRFSignalGenerator* generator)
{
	m_modifiedSinceLastSave = true;

	//If the generator is also a function generator, delete that too
	//FIXME: This is not the best UX. Would be best to ref count and delete when both are closed
	auto func = dynamic_cast<SCPIFunctionGenerator*>(generator);
	if(func != nullptr)
	{
		RemoveFunctionGenerator(func);
		m_mainWindow->RemoveFunctionGenerator(func);
	}

	m_rfgenerators.erase(generator);
}

/**
	@brief Returns a list of all connected instruments, of any type

	Multi-type instruments are only counted once.
 */
set<SCPIInstrument*> Session::GetSCPIInstruments()
{
	lock_guard<mutex> lock(m_scopeMutex);

	set<SCPIInstrument*> insts;
	for(auto& scope : m_oscilloscopes)
	{
		auto s = dynamic_cast<SCPIInstrument*>(scope);
		if(s != nullptr)
			insts.emplace(s);
	}
	for(auto& it : m_psus)
	{
		auto s = dynamic_cast<SCPIInstrument*>(it.first);
		if(s != nullptr)
			insts.emplace(s);
	}
	for(auto& it : m_meters)
	{
		auto s = dynamic_cast<SCPIInstrument*>(it.first);
		if(s != nullptr)
			insts.emplace(s);
	}
	for(auto& it : m_rfgenerators)
		insts.emplace(it.first);
	for(auto gen : m_generators)
		insts.emplace(gen);

	return insts;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Trigger control

/**
	@brief Arms the trigger on all scopes
 */
void Session::ArmTrigger(TriggerType type)
{
	lock_guard<mutex> lock(m_scopeMutex);

	bool oneshot = (type == TRIGGER_TYPE_FORCED) || (type == TRIGGER_TYPE_SINGLE);
	m_triggerOneShot = oneshot;

	if(!HasOnlineScopes())
	{
		m_tArm = GetTime();
		m_triggerArmed = true;
		return;
	}

	/*
		If we have multiple scopes, always use single trigger to keep them synced.
		Multi-trigger can lead to race conditions and dropped triggers if we're still downloading a secondary
		instrument's waveform and the primary re-arms.

		Also, order of arming is critical. Secondaries must be completely armed before the primary (instrument 0) to
		ensure that the primary doesn't trigger until the secondaries are ready for the event.
	*/
	m_tPrimaryTrigger = -1;
	if(!oneshot && (m_oscilloscopes.size() > 1) )
		m_multiScopeFreeRun = true;
	else
		m_multiScopeFreeRun = false;

	//In multi-scope mode, make sure all scopes are stopped with no pending waveforms
	if(m_oscilloscopes.size() > 1)
	{
		for(ssize_t i=m_oscilloscopes.size()-1; i >= 0; i--)
		{
			if(m_oscilloscopes[i]->PeekTriggerArmed())
				m_oscilloscopes[i]->Stop();

			if(m_oscilloscopes[i]->HasPendingWaveforms())
			{
				LogWarning("Scope %s had pending waveforms before arming\n", m_oscilloscopes[i]->m_nickname.c_str());
				m_oscilloscopes[i]->ClearPendingWaveforms();
			}
		}
	}

	for(ssize_t i=m_oscilloscopes.size()-1; i >= 0; i--)
	{
		//If we have >1 scope, all secondaries always use single trigger synced to the primary's trigger output
		if(i > 0)
			m_oscilloscopes[i]->StartSingleTrigger();

		else
		{
			switch(type)
			{
				//Normal trigger: all scopes lock-step for multi scope
				//for single scope, use normal trigger
				case TRIGGER_TYPE_NORMAL:
					if(m_oscilloscopes.size() > 1)
						m_oscilloscopes[i]->StartSingleTrigger();
					else
						m_oscilloscopes[i]->Start();
					break;

				case TRIGGER_TYPE_AUTO:
					LogError("ArmTrigger(TRIGGER_TYPE_AUTO) not implemented\n");
					break;

				case TRIGGER_TYPE_SINGLE:
					m_oscilloscopes[i]->StartSingleTrigger();
					break;

				case TRIGGER_TYPE_FORCED:
					m_oscilloscopes[i]->ForceTrigger();
					break;

				default:
					break;
			}
		}

		//If we have multiple scopes, ping the secondaries to make sure the arm command went through
		if(i != 0)
		{
			double start = GetTime();

			while(!m_oscilloscopes[i]->PeekTriggerArmed())
			{
				//After 3 sec of no activity, time out
				//(must be longer than the default 2 sec socket timeout)
				double now = GetTime();
				if( (now - start) > 3)
				{
					LogWarning("Timeout waiting for scope %s to arm\n",  m_oscilloscopes[i]->m_nickname.c_str());
					m_oscilloscopes[i]->Stop();
					m_oscilloscopes[i]->StartSingleTrigger();
					start = now;
				}
			}

			//Scope is armed. Clear any garbage in the pending queue
			m_oscilloscopes[i]->ClearPendingWaveforms();
		}
	}
	m_tArm = GetTime();
	m_triggerArmed = true;
}

/**
	@brief Stop the trigger on all scopes
 */
void Session::StopTrigger()
{
	m_multiScopeFreeRun = false;
	m_triggerArmed = false;

	for(auto scope : m_oscilloscopes)
	{
		scope->Stop();

		//Clear out any pending data (the user doesn't want it, and we don't want stale stuff hanging around)
		scope->ClearPendingWaveforms();
	}
}

/**
	@brief Returns true if we have at least one scope that isn't offline
 */
bool Session::HasOnlineScopes()
{
	for(auto scope : m_oscilloscopes)
	{
		if(!scope->IsOffline())
			return true;
	}
	return false;
}

bool Session::CheckForPendingWaveforms()
{
	lock_guard<mutex> lock(m_scopeMutex);

	//No online scopes to poll? Re-run the filter graph
	if(!HasOnlineScopes())
		return m_triggerArmed;

	//Wait for every online scope to have triggered
	for(auto scope : m_oscilloscopes)
	{
		if(scope->IsOffline())
			continue;
		if(!scope->HasPendingWaveforms())
			return false;
	}

	//Keep track of when the primary instrument triggers.
	if(m_multiScopeFreeRun)
	{
		//See when the primary triggered
		if( (m_tPrimaryTrigger < 0) && m_oscilloscopes[0]->HasPendingWaveforms() )
			m_tPrimaryTrigger = GetTime();

		//All instruments should trigger within 1 sec (arbitrary threshold) of the primary.
		//If it's been longer than that, something went wrong. Discard all pending data and re-arm the trigger.
		double twait = GetTime() - m_tPrimaryTrigger;
		if( (m_tPrimaryTrigger > 0) && ( twait > 1 ) )
		{
			LogWarning("Timed out waiting for one or more secondary instruments to trigger (%.2f ms). Resetting...\n",
				twait*1000);

			//Cancel any pending triggers
			StopTrigger();

			//Discard all pending waveform data
			for(auto scope : m_oscilloscopes)
			{
				//Don't touch anything offline
				if(scope->IsOffline())
					continue;

				scope->IDPing();
				scope->ClearPendingWaveforms();
			}

			//Re-arm the trigger and get back to polling
			ArmTrigger(TRIGGER_TYPE_NORMAL);
			return false;
		}
	}

	//If we get here, we had waveforms on all instruments
	return true;
}

/**
	@brief Pull the waveform data out of the queue and make it current
 */
void Session::DownloadWaveforms()
{
	{
		lock_guard<mutex> lock(m_perfClockMutex);
		m_waveformDownloadRate.Tick();
	}

	lock_guard<recursive_mutex> lock(m_waveformDataMutex);
	lock_guard<mutex> lock2(m_scopeMutex);

	//Process the waveform data from each instrument
	for(auto scope : m_oscilloscopes)
	{
		//Don't touch anything offline
		if(scope->IsOffline())
			continue;

		//Make sure we don't free the old waveform data
		//TODO: only do this once we have history
		LogTrace("TODO: release waveform once we have history\n");
		/*
		for(size_t i=0; i<scope->GetChannelCount(); i++)
		{
			auto chan = scope->GetChannel(i);
			for(size_t j=0; j<chan->GetStreamCount(); j++)
				chan->Detach(j);
		}*/

		//Download the data
		scope->PopPendingWaveform();
	}

	//If we're in offline one-shot mode, disarm the trigger
	if( (m_oscilloscopes.empty()) && m_triggerOneShot)
		m_triggerArmed = false;

	//In multi-scope mode, retcon the timestamps of secondary scopes' waveforms so they line up with the primary.
	if(m_oscilloscopes.size() > 1)
	{
		LogTrace("Multi scope: patching timestamps\n");
		LogIndenter li;

		//Get the timestamp of the primary scope's first waveform
		bool hit = false;
		time_t timeSec = 0;
		int64_t timeFs  = 0;
		auto prim = m_oscilloscopes[0];
		for(size_t i=0; i<prim->GetChannelCount(); i++)
		{
			auto chan = prim->GetChannel(i);
			for(size_t j=0; j<chan->GetStreamCount(); j++)
			{
				auto data = chan->GetData(j);
				if(data != nullptr)
				{
					timeSec = data->m_startTimestamp;
					timeFs = data->m_startFemtoseconds;
					hit = true;
					break;
				}
			}
			if(hit)
				break;
		}

		//Patch all secondary scopes
		for(size_t i=1; i<m_oscilloscopes.size(); i++)
		{
			auto sec = m_oscilloscopes[i];

			for(size_t j=0; j<sec->GetChannelCount(); j++)
			{
				auto chan = sec->GetChannel(j);
				for(size_t k=0; k<chan->GetStreamCount(); k++)
				{
					auto data = chan->GetData(k);
					if(data == nullptr)
						continue;

					auto skew = m_scopeDeskewCal[sec];

					data->m_startTimestamp = timeSec;
					data->m_startFemtoseconds = timeFs;
					data->m_triggerPhase -= skew;
				}
			}
		}
	}
}

/**
	@brief Check if new waveform data has arrived

	This runs in the main GUI thread.

	TODO: this might be best to move to MainWindow?
 */
void Session::CheckForWaveforms(vk::raii::CommandBuffer& cmdbuf)
{
	bool hadNewWaveforms = false;
	if(m_triggerArmed)
	{
		if(g_waveformReadyEvent.Peek())
		{
			LogTrace("Waveform is ready\n");

			//Crunch the new waveform
			{
				lock_guard<recursive_mutex> lock2(m_waveformDataMutex);

				//Update the history windows
				/*
				for(auto scope : m_oscilloscopes)
				{
					if(!scope->IsOffline())
						m_historyWindows[scope]->OnWaveformDataReady();
				}
				*/
			}

			//Tone-map all of our waveforms
			hadNewWaveforms = true;
			m_mainWindow->ToneMapAllWaveforms(cmdbuf);

			//Release the waveform processing thread
			g_waveformProcessedEvent.Signal();

			//In multi-scope free-run mode, re-arm every instrument's trigger after we've processed all data
			if(m_multiScopeFreeRun)
				ArmTrigger(TRIGGER_TYPE_NORMAL);
		}
	}

	//Discard all pending waveform data if the trigger isn't armed.
	//Failure to do this can lead to a spurious trigger after we wanted to stop.
	else
	{
		lock_guard<mutex> lock(m_scopeMutex);
		for(auto scope : m_oscilloscopes)
			scope->ClearPendingWaveforms();

		//If waveform thread is blocking for us to process its last waveform, release it
		if(g_waveformReadyEvent.Peek())
			g_waveformProcessedEvent.Signal();
	}

	//If a re-render operation completed, tone map everything again
	if(g_rerenderDoneEvent.Peek() && !hadNewWaveforms)
		m_mainWindow->ToneMapAllWaveforms(cmdbuf);

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Filter processing

size_t Session::GetFilterCount()
{
	set<Filter*> filters;
	{
		lock_guard<mutex> lock2(m_filterUpdatingMutex);
		filters = Filter::GetAllInstances();
	}
	return filters.size();
}

void Session::RefreshAllFilters()
{
	double tstart = GetTime();

	lock_guard<recursive_mutex> lock(m_waveformDataMutex);

	//SyncFilterColors();

	set<Filter*> filters;
	{
		lock_guard<mutex> lock3(m_filterUpdatingMutex);
		filters = Filter::GetAllInstances();
	}

	shared_lock<shared_mutex> lock2(g_vulkanActivityMutex);
	m_graphExecutor.RunBlocking(filters);

	//Update statistic displays after the filter graph update is complete
	//for(auto g : m_waveformGroups)
	//	g->RefreshMeasurements();
	LogTrace("TODO: refresh statistics\n");

	m_lastFilterGraphExecTime = (GetTime() - tstart) * FS_PER_SECOND;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Gets the last execution time of the tone mapping shaders
 */
int64_t Session::GetToneMapTime()
{
	return m_mainWindow->GetToneMapTime();
}

void Session::RenderWaveformTextures(vk::raii::CommandBuffer& cmdbuf, vector<shared_ptr<DisplayedChannel> >& channels)
{
	lock_guard<recursive_mutex> lock(m_waveformDataMutex);
	m_mainWindow->RenderWaveformTextures(cmdbuf, channels);
}
