/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2023 Andrew D. Zonenberg                                                                          *
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
#include "../scopeprotocols/ExportFilter.h"
#include "MainWindow.h"
#include "FunctionGeneratorDialog.h"
#include "LoadDialog.h"
#include "MultimeterDialog.h"
#include "PowerSupplyDialog.h"
#include "RFGeneratorDialog.h"

#include "../scopehal/LeCroyOscilloscope.h"

extern Event g_waveformReadyEvent;
extern Event g_waveformProcessedEvent;
extern Event g_rerenderDoneEvent;
extern Event g_refilterRequestedEvent;
extern Event g_partialRefilterRequestedEvent;
extern Event g_refilterDoneEvent;

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
	, m_history(*this)
	, m_nextMarkerNum(1)
{
	CreateReferenceFilters();
}

Session::~Session()
{
	Clear();
	DestroyReferenceFilters();
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

	//HACK: for now, export filters keep an open reference to themselves to avoid memory leaks
	//Free this refererence now.
	//Long term we can probably do this better https://github.com/glscopeclient/scopehal-apps/issues/573
	auto filters = Filter::GetAllInstances();
	for(auto f : filters)
	{
		auto e = dynamic_cast<ExportFilter*>(f);
		if(e)
			e->Release();
	}

	//TODO: do we need to lock the mutex now that all of the background threads should have terminated?
	//Might be redundant.
	lock_guard<mutex> lock2(m_scopeMutex);

	//Clear history before destroying scopes.
	//This ordering is important since waveforms removed from history get pushed into the WaveformPool of the scopes,
	//so the scopes must not have been destroyed yet.
	m_history.clear();

	//Delete scopes once we've terminated the threads
	//Detach waveforms before we destroy the scope, since history owns them
	for(auto scope : m_oscilloscopes)
	{
		for(size_t i=0; i<scope->GetChannelCount(); i++)
		{
			auto chan = scope->GetOscilloscopeChannel(i);
			if(!chan)
				continue;
			for(size_t j=0; j<chan->GetStreamCount(); j++)
				chan->Detach(j);
		}
		delete scope;
	}
	m_oscilloscopes.clear();
	m_psus.clear();
	m_loads.clear();
	m_rfgenerators.clear();
	m_meters.clear();
	m_scopeDeskewCal.clear();

	//We SHOULD not have any filters at this point.
	//But there have been reports that some stick around. If this happens, print an error message.
	filters = Filter::GetAllInstances();
	for(auto f : filters)
		LogWarning("Leaked filter %s (%zu refs)\n", f->GetHwname().c_str(), f->GetRefCount());

	//Reset state
	m_triggerOneShot = false;
	m_multiScopeFreeRun = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Instrument management

void Session::ApplyPreferences(Oscilloscope* scope)
{
	//Apply driver-specific preference settings
	auto lecroy = dynamic_cast<LeCroyOscilloscope*>(scope);
	if(lecroy)
	{
		if(m_preferences.GetBool("Drivers.Teledyne LeCroy.force_16bit"))
			lecroy->ForceHDMode(true);

		//else auto resolution depending on instrument type
	}
}

/**
	@brief Starts the WaveformThread if we don't already have one
 */
void Session::StartWaveformThreadIfNeeded()
{
	if(m_waveformThread == nullptr)
		m_waveformThread = make_unique<thread>(WaveformThread, this, &m_shuttingDown);
}

void Session::AddOscilloscope(Oscilloscope* scope)
{
	lock_guard<mutex> lock(m_scopeMutex);

	m_modifiedSinceLastSave = true;
	m_oscilloscopes.push_back(scope);

	m_threads.push_back(make_unique<thread>(ScopeThread, scope, &m_shuttingDown));

	m_mainWindow->AddToRecentInstrumentList(dynamic_cast<SCPIOscilloscope*>(scope));
	m_mainWindow->OnScopeAdded(scope);

	StartWaveformThreadIfNeeded();
}

/**
	@brief Adds a power supply to the session
 */
void Session::AddPowerSupply(SCPIPowerSupply* psu)
{
	m_modifiedSinceLastSave = true;

	//Create shared PSU state
	auto state = make_shared<PowerSupplyState>(psu->GetChannelCount());
	m_psus[psu] = make_unique<PowerSupplyConnectionState>(psu, state, this);

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
	m_meters[meter] = make_unique<MultimeterConnectionState>(meter, state, this);

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
	@brief Adds a Load to the session
 */
void Session::AddLoad(SCPILoad* load)
{
	m_modifiedSinceLastSave = true;

	//Create shared load state
	auto state = make_shared<LoadState>(load->GetChannelCount());
	m_loads[load] = make_unique<LoadConnectionState>(load, state, this);

	//Add the dialog to view/control it
	m_mainWindow->AddDialog(make_shared<LoadDialog>(load, state, this));

	m_mainWindow->AddToRecentInstrumentList(load);
}

/**
	@brief Removes a function generator from the session
 */
void Session::RemoveLoad(SCPILoad* load)
{
	m_modifiedSinceLastSave = true;

	m_loads.erase(load);
	delete load;
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
	@brief Returns a list of all connected SCPI instruments, of any type

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
	for(auto& it : m_loads)
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

/**
	@brief Returns a list of all connected instruments, of any type

	Multi-type instruments are only counted once.
 */
set<Instrument*> Session::GetInstruments()
{
	lock_guard<mutex> lock(m_scopeMutex);

	set<Instrument*> insts;
	for(auto& scope : m_oscilloscopes)
		insts.emplace(scope);
	for(auto& it : m_psus)
		insts.emplace(it.first);
	for(auto& it : m_meters)
		insts.emplace(it.first);
	for(auto& it : m_loads)
		insts.emplace(it.first);
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

		//Detach old waveforms since they're now owned by history manager
		for(size_t i=0; i<scope->GetChannelCount(); i++)
		{
			auto chan = scope->GetOscilloscopeChannel(i);
			if(!chan)

				continue;
			for(size_t j=0; j<chan->GetStreamCount(); j++)
				chan->Detach(j);
		}

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
			auto chan = prim->GetOscilloscopeChannel(i);
			if(!chan)
				continue;
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
				auto chan = sec->GetOscilloscopeChannel(j);
				if(!chan)
					continue;
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

	@return True if a new waveform came in, false if not
 */
bool Session::CheckForWaveforms(vk::raii::CommandBuffer& cmdbuf)
{
	bool hadNewWaveforms = false;

	if(g_waveformReadyEvent.Peek())
	{
		LogTrace("Waveform is ready\n");

		//Add to history
		auto scopes = GetScopes();
		{
			lock_guard<recursive_mutex> lock2(m_waveformDataMutex);
			m_history.AddHistory(scopes);
		}

		//Tone-map all of our waveforms
		//(does not need waveform data locked since it only works on *rendered* data)
		hadNewWaveforms = true;
		m_mainWindow->ToneMapAllWaveforms(cmdbuf);

		//Release the waveform processing thread
		g_waveformProcessedEvent.Signal();

		//In multi-scope free-run mode, re-arm every instrument's trigger after we've processed all data
		if(m_multiScopeFreeRun)
			ArmTrigger(TRIGGER_TYPE_NORMAL);
	}

	//If a re-render operation completed, tone map everything again
	if((g_rerenderDoneEvent.Peek() || g_refilterDoneEvent.Peek()) && !hadNewWaveforms)
		m_mainWindow->ToneMapAllWaveforms(cmdbuf);

	return hadNewWaveforms;
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


/**
	@brief Queues a request to refresh all filters the next time we poll stuff
 */
void Session::RefreshAllFiltersNonblocking()
{
	g_refilterRequestedEvent.Signal();
}

/**
	@brief Queues a request to refresh dirty filters the next time we poll stuff
 */
void Session::RefreshDirtyFiltersNonblocking()
{
	g_partialRefilterRequestedEvent.Signal();
}

void Session::RefreshAllFilters()
{
	double tstart = GetTime();

	set<Filter*> filters;
	{
		lock_guard<mutex> lock2(m_filterUpdatingMutex);
		filters = Filter::GetAllInstances();
	}

	{
		shared_lock<shared_mutex> lock3(g_vulkanActivityMutex);
		lock_guard<recursive_mutex> lock(m_waveformDataMutex);
		m_graphExecutor.RunBlocking(filters);
		UpdatePacketManagers(filters);
	}

	//Update statistic displays after the filter graph update is complete
	//for(auto g : m_waveformGroups)
	//	g->RefreshMeasurements();
	LogTrace("TODO: refresh statistics\n");

	m_lastFilterGraphExecTime = (GetTime() - tstart) * FS_PER_SECOND;
}

/**
	@brief Refresh dirty filters (and anything in their downstream influence cone)
 */
void Session::RefreshDirtyFilters()
{
	set<Filter*> filtersToUpdate;

	{
		lock_guard<mutex> lock(m_dirtyChannelsMutex);
		if(m_dirtyChannels.empty())
			return;

		//Start with all filters
		set<Filter*> filters;
		{
			lock_guard<mutex> lock2(m_filterUpdatingMutex);
			filters = Filter::GetAllInstances();
		}

		//Check each one to see if it needs updating
		for(auto f : filters)
		{
			if(f->IsDownstreamOf(m_dirtyChannels))
				filtersToUpdate.emplace(f);
		}

		//Reset list for next round
		m_dirtyChannels.clear();
	}
	if(filtersToUpdate.empty())
		return;

	//Refresh the dirty filters only
	double tstart = GetTime();

	{
		shared_lock<shared_mutex> lock3(g_vulkanActivityMutex);
		lock_guard<recursive_mutex> lock(m_waveformDataMutex);
		m_graphExecutor.RunBlocking(filtersToUpdate);
		UpdatePacketManagers(filtersToUpdate);
	}

	//Update statistic displays after the filter graph update is complete
	//for(auto g : m_waveformGroups)
	//	g->RefreshMeasurements();
	LogTrace("TODO: refresh statistics\n");

	m_lastFilterGraphExecTime = (GetTime() - tstart) * FS_PER_SECOND;
}

/**
	@brief Flags a single channel as dirty (updated outside of a global trigger event)
 */
void Session::MarkChannelDirty(InstrumentChannel* chan)
{
	lock_guard<mutex> lock(m_dirtyChannelsMutex);
	m_dirtyChannels.emplace(chan);
}

/**
	@brief Clear state on all of our filters
 */
void Session::ClearSweeps()
{
	lock_guard<recursive_mutex> lock(m_waveformDataMutex);

	set<Filter*> filters;
	{
		lock_guard<mutex> lock2(m_filterUpdatingMutex);
		filters = Filter::GetAllInstances();
	}

	for(auto f : filters)
		f->ClearSweeps();
}

/**
	@brief Update all of the packet managers when new data arrives
 */
void Session::UpdatePacketManagers(const set<Filter*>& filters)
{
	lock_guard<mutex> lock(m_packetMgrMutex);

	set<PacketDecoder*> deletedFilters;
	for(auto it : m_packetmgrs)
	{
		//Remove filters that no longer exist
		if(filters.find(it.first) == filters.end())
			deletedFilters.emplace(it.first);

		//It exists, update it
		else
			it.second->Update();
	}

	//Delete managers for nonexistent filters
	for(auto f : deletedFilters)
		m_packetmgrs.erase(f);
}

/**
	@brief Called when a new packet filter is created
 */
shared_ptr<PacketManager> Session::AddPacketFilter(PacketDecoder* filter)
{
	LogTrace("Adding packet manager for %s\n", filter->GetDisplayName().c_str());

	lock_guard<mutex> lock(m_packetMgrMutex);
	shared_ptr<PacketManager> ret = make_shared<PacketManager>(filter);
	m_packetmgrs[filter] = ret;
	return ret;
}

/**
	@brief Deletes packets from our packet managers for a waveform timestamp
 */
void Session::RemovePackets(TimePoint t)
{
	for(auto it : m_packetmgrs)
		it.second->RemoveHistoryFrom(t);
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
	m_mainWindow->RenderWaveformTextures(cmdbuf, channels);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Reference filters

/**
	@brief Creates one filter of each known type to use as a reference for what inputs are legal to use to a new filter
 */
void Session::CreateReferenceFilters()
{
	double start = GetTime();

	vector<string> names;
	Filter::EnumProtocols(names);

	for(auto n : names)
	{
		auto f = Filter::CreateFilter(n.c_str(), "");;
		f->HideFromList();
		m_referenceFilters[n] = f;
	}

	LogTrace("Created %zu reference filters in %.2f ms\n", m_referenceFilters.size(), (GetTime() - start) * 1000);
}

/**
	@brief Destroys the reference filters

	This only needs to be done at application shutdown, not in Clear(), because the reference filters have no persistent
	state. The only thing they're ever used for is calling ValidateChannel() on them.
 */
void Session::DestroyReferenceFilters()
{
	for(auto it : m_referenceFilters)
		delete it.second;
	m_referenceFilters.clear();
}
