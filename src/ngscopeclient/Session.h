/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2025 Andrew D. Zonenberg and contributors                                                         *
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
	@brief Declaration of Session
 */
#ifndef Session_h
#define Session_h

class MainWindow;
class WaveformArea;
class DisplayedChannel;

#include "../xptools/HzClock.h"
#include "HistoryManager.h"
#include "PacketManager.h"
#include "PreferenceManager.h"
#include "Marker.h"
#include "TriggerGroup.h"

extern std::atomic<int64_t> g_lastWaveformRenderTime;

class Session;

class InstrumentConnectionState
{
public:
	InstrumentConnectionState(InstrumentThreadArgs args)
	{
		m_shuttingDown = false;
		args.shuttingDown = &m_shuttingDown;
		m_thread = std::make_unique<std::thread>(InstrumentThread, args);
		m_lastTriggerState = Oscilloscope::TRIGGER_MODE_WAIT;
	}

	~InstrumentConnectionState()
	{ Close(); }

	void Close()
	{
		if(m_thread)
		{
			//Terminate the thread
			m_shuttingDown = true;
			m_thread->join();
		}
		m_thread = nullptr;
	}

	///@brief Termination flag for shutting down the polling thread
	std::atomic<bool> m_shuttingDown;

	///@brief Thread for polling the instrument
	std::unique_ptr<std::thread> m_thread;

	///@brief Cached trigger state, to reflect in the UI
	Oscilloscope::TriggerMode m_lastTriggerState;
};

/**
	@brief A Session stores all of the instrument configuration and other state the user has open.

	Generally only accessed from the GUI thread.
	TODO: interlocking if needed?
 */
class Session
{
public:
	Session(MainWindow* wnd);
	virtual ~Session();

	bool OnMemoryPressure(MemoryPressureLevel level, MemoryPressureType type, size_t requestedSize);

	void ArmTrigger(TriggerGroup::TriggerType type, bool all=false);
	void StopTrigger(bool all=false);
	bool HasOnlineScopes();
	void DownloadWaveforms();
	bool CheckForWaveforms(vk::raii::CommandBuffer& cmdbuf);
	void RefreshAllFilters();
	void RefreshAllFiltersNonblocking();
	void RefreshDirtyFiltersNonblocking();
	bool RefreshDirtyFilters();
	void FlushConfigCache();

	void MarkChannelDirty(InstrumentChannel* chan);

	void RenderWaveformTextures(
		vk::raii::CommandBuffer& cmdbuf,
		std::vector<std::shared_ptr<DisplayedChannel> >& channels);

	void Clear();
	void ClearBackgroundThreads();

	bool PreLoadFromYaml(const YAML::Node& node, const std::string& dataDir, bool online);
	bool LoadFromYaml(const YAML::Node& node, const std::string& dataDir, bool online);
	YAML::Node SerializeInstrumentConfiguration();
	YAML::Node SerializeMetadata();
	YAML::Node SerializeTriggerGroups();
	bool LoadTriggerGroups(const YAML::Node& node);
	YAML::Node SerializeFilterConfiguration();
	YAML::Node SerializeMarkers();
	bool SerializeWaveforms(const std::string& dataDir);
	bool SerializeSparseWaveform(SparseWaveformBase* wfm, const std::string& path);
	bool SerializeUniformWaveform(UniformWaveformBase* wfm, const std::string& path);

	void AddMultimeterDialog(std::shared_ptr<SCPIMultimeter> meter);
	std::shared_ptr<PacketManager> AddPacketFilter(PacketDecoder* filter);

	void AddInstrument(std::shared_ptr<Instrument> inst, bool createDialogs = true);
	void RemoveInstrument(std::shared_ptr<Instrument> inst);
	std::shared_ptr<InstrumentConnectionState> GetInstrumentConnectionState(std::shared_ptr<Instrument> inst) { return m_instrumentStates[inst]; }

	bool IsMultiScope()
	{ return m_multiScope; }

	MainWindow* GetMainWindow()
	{ return m_mainWindow; }

	/**
		@brief Returns a pointer to the state for a BERT
	 */
	std::shared_ptr<BERTState> GetBERTState(std::shared_ptr<BERT> bert)
	{
		std::lock_guard<std::mutex> lock(m_scopeMutex);
		return m_berts[bert];
	}

	/**
		@brief Returns a pointer to the state for a power supply
	 */
	std::shared_ptr<PowerSupplyState> GetPSUState(std::shared_ptr<SCPIPowerSupply> psu)
	{
		std::lock_guard<std::mutex> lock(m_scopeMutex);
		return m_psus[psu];
	}

	/**
		@brief Returns a pointer to the state for a function generator
	 */
	std::shared_ptr<FunctionGeneratorState> GetFunctionGeneratorState(std::shared_ptr<FunctionGenerator> awg)
	{
		std::lock_guard<std::mutex> lock(m_scopeMutex);
		return m_awgs[awg];
	}

	/**
		@brief Returns a pointer to the existing packet manager for a protocol decode filter
	 */
	std::shared_ptr<PacketManager> GetPacketManager(PacketDecoder* filter)
	{
		std::lock_guard<std::mutex> lock(m_packetMgrMutex);
		return m_packetmgrs[filter];
	}

	void ApplyPreferences(std::shared_ptr<Oscilloscope> scope);

	size_t GetFilterCount();

	bool IsChannelBeingDragged();

	int64_t GetToneMapTime();

	/**
		@brief Gets the last execution time of the filter graph
	 */
	int64_t GetFilterGraphExecTime()
	{ return m_lastFilterGraphExecTime.load(); }

	/**
		@brief Gets the last run time of the waveform rendering shaders
	 */
	int64_t GetLastWaveformRenderTime()
	{ return g_lastWaveformRenderTime.load(); }

	/**
		@brief Gets the average rate at which we are pulling waveforms off the scope, in Hz
	 */
	double GetWaveformDownloadRate()
	{
		std::lock_guard<std::mutex> lock(m_perfClockMutex);
		return m_waveformDownloadRate.GetAverageHz();
	}

	/**
		@brief Get the set of scopes we're currently connected to
	 */
	const std::vector<std::shared_ptr<Oscilloscope>> GetScopes()
	{
		std::lock_guard<std::mutex> lock(m_scopeMutex);
		return m_oscilloscopes;
	}

	/**
		@brief Get the set of BERTs we're currently connected to
	 */
	const std::vector<std::shared_ptr<BERT> > GetBERTs()
	{
		std::lock_guard<std::mutex> lock(m_scopeMutex);
		std::vector<std::shared_ptr<BERT> > berts;
		for(auto& it : m_berts)
			berts.push_back(it.first);
		return berts;
	}

	/**
		@brief Gets the set of all SCPI instruments we're connect to (regardless of type)
	 */
	std::set<std::shared_ptr<SCPIInstrument>> GetSCPIInstruments();

	/**
		@brief Gets the set of all instruments we're connect to (regardless of type)
	 */
	std::set<std::shared_ptr<Instrument>> GetInstruments();

	/**
		@brief Get the number of instruments we're connected to (regardless of type)
	 */
	size_t GetInstrumentCount()
	{ return GetInstruments().size(); }

	/**
		@brief Check if we have data available from all of our scopes
	 */
	bool CheckForPendingWaveforms();

	/**
		@brief Get the mutex controlling access to waveform data
	 */
	std::shared_mutex& GetWaveformDataMutex()
	{ return m_waveformDataMutex; }

	/**
		@brief Get our history manager
	 */
	HistoryManager& GetHistory()
	{ return m_history; }

	/**
		@brief Adds a marker
	 */
	void AddMarker(Marker m);

	void StartWaveformThreadIfNeeded();

	void ClearSweeps();

	/**
		@brief Get the mutex controlling access to rasterized waveforms
	 */
	std::mutex& GetRasterizedWaveformMutex()
	{ return m_rasterizedWaveformMutex; }

	/**
		@brief ID mapping used for serialization
	 */
	IDTable m_idtable;

	/**
		@brief Session notes
	 */
	std::string m_generalNotes;

	/**
		@brief Session notes about the experimental setup
	 */
	std::string m_setupNotes;

	std::vector<std::shared_ptr<TriggerGroup> > GetTriggerGroups()
	{
		std::lock_guard<std::recursive_mutex> lock(m_triggerGroupMutex);
		return m_triggerGroups;
	}

	void GarbageCollectTriggerGroups();

	void MakeNewTriggerGroup(std::shared_ptr<Oscilloscope> scope);
	void MakeNewTriggerGroup(PausableFilter* filter);

	int64_t GetDeskew(std::shared_ptr<Oscilloscope> scope)
	{ return m_scopeDeskewCal[scope]; }

	void SetDeskew(std::shared_ptr<Oscilloscope> scope, int64_t skew)
	{ m_scopeDeskewCal[scope] = skew; }

	bool IsPrimaryOfMultiScopeGroup(std::shared_ptr<Oscilloscope> scope);
	bool IsSecondaryOfMultiScopeGroup(std::shared_ptr<Oscilloscope> scope);

	std::shared_ptr<TriggerGroup> GetTriggerGroupForScope(std::shared_ptr<Oscilloscope> scope);
	std::shared_ptr<TriggerGroup> GetTriggerGroupForFilter(PausableFilter* filter);

	const ConfigWarningList& GetWarnings()
	{ return m_warnings; }

	///@brief Get the state for a load
	std::shared_ptr<LoadState> GetLoadState(std::shared_ptr<Load> load)
	{
		auto it = m_loads.find(load);
		if(it != m_loads.end())
			return it->second;
		else
			return nullptr;
	}

	std::shared_ptr<TriggerGroup> GetTrendFilterGroup();

	void OnMarkerChanged();

	///@brief Return the last filter graph runtime stats
	std::map<FlowGraphNode*, int64_t> GetFilterGraphRuntime()
	{
		std::lock_guard<std::mutex> lock(m_lastFilterGraphRuntimeMutex);
		return m_lastFilterGraphRuntimeStats;
	}

protected:
	void UpdatePacketManagers(const std::set<FlowGraphNode*>& nodes);

	std::string GetRegisteredTypeOfDriver(const std::string& drivername);

	bool LoadInstruments(int version, const YAML::Node& node, bool online);
	bool PreLoadInstruments(int version, const YAML::Node& node, bool online);
	SCPITransport* CreateTransportForNode(const YAML::Node& node);
	bool VerifyInstrument(const YAML::Node& node, std::shared_ptr<Instrument> inst);
	bool PreLoadVNA(int version, const YAML::Node& node, bool online);
	bool PreLoadOscilloscope(int version, const YAML::Node& node, bool online);
	bool PreLoadPowerSupply(int version, const YAML::Node& node, bool online);
	bool PreLoadRFSignalGenerator(int version, const YAML::Node& node, bool online);
	bool PreLoadFunctionGenerator(int version, const YAML::Node& node, bool online);
	bool PreLoadMultimeter(int version, const YAML::Node& node, bool online);
	bool PreLoadSpectrometer(int version, const YAML::Node& node, bool online);
	bool PreLoadSDR(int version, const YAML::Node& node, bool online);
	bool PreLoadBERT(int version, const YAML::Node& node, bool online);
	bool PreLoadLoad(int version, const YAML::Node& node, bool online);
	bool PreLoadMisc(int version, const YAML::Node& node, bool online);
	bool LoadFilters(int version, const YAML::Node& node);
	bool LoadInstrumentInputs(int version, const YAML::Node& node);
	bool LoadWaveformData(int version, const std::string& dataDir);
	bool LoadWaveformDataForScope(
		int version,
		const YAML::Node& node,
		std::shared_ptr<Oscilloscope> scope,
		const std::string& dataDir);
	bool LoadWaveformDataForFilters(
		int version,
		const YAML::Node& node,
		const std::string& dataDir);
	void DoLoadWaveformDataForStream(
		OscilloscopeChannel* chan,
		int stream,
		std::string format,
		std::string fname);

	///@brief Version of the file being loaded
	int m_fileLoadVersion;

	///@brief Warnings generated by loading the current file
	ConfigWarningList m_warnings;

	///@brief Deskew correction coefficients for multi-scope
	std::map<std::shared_ptr<Oscilloscope>, int64_t> m_scopeDeskewCal;

	///@brief Mutex for controlling access to scope vectors
	std::mutex m_scopeMutex;

	///@brief Mutex for controlling access to waveform data
	std::shared_mutex m_waveformDataMutex;

	///@brief Mutex for controlling access to filter graph
	std::mutex m_filterUpdatingMutex;

	///@brief Top level UI window
	MainWindow* m_mainWindow;

	///@brief Flag for shutting down all scope threads when we exit
	std::atomic<bool> m_shuttingDown;

	///@brief True if the session has been modified since last time it was saved
	bool m_modifiedSinceLastSave;

	///@brief Oscilloscopes we are currently connected to
	std::vector<std::shared_ptr<Oscilloscope>> m_oscilloscopes;

	///@brief Power supplies we are currently connected to
	std::map<std::shared_ptr<PowerSupply>, std::shared_ptr<PowerSupplyState> > m_psus;

	///@brief Multimeters we are currently connected to
	std::map<std::shared_ptr<Multimeter>, std::shared_ptr<MultimeterState> > m_meters;

	///@brief Loads we are currently connected to
	std::map<std::shared_ptr<Load>, std::shared_ptr<LoadState> > m_loads;

	///@brief BERTs we are currently connected to
	std::map<std::shared_ptr<BERT>, std::shared_ptr<BERTState> > m_berts;

	///@brief Function generator we are currently connected to
	std::map<std::shared_ptr<FunctionGenerator>, std::shared_ptr<FunctionGeneratorState> > m_awgs;

	///@brief Trigger groups for syncing oscilloscopes
	std::vector<std::shared_ptr<TriggerGroup> > m_triggerGroups;

	///@brief Trigger group dedicated to trend filters
	std::shared_ptr<TriggerGroup> m_trendTriggerGroup;

	///@brief Mutex controlling access to m_triggerGroups
	std::recursive_mutex m_triggerGroupMutex;

	///@brief Worker threads and other bookkeeping metadata for instruments
	std::map<std::shared_ptr<Instrument>, std::shared_ptr<InstrumentConnectionState> > m_instrumentStates;

	///@brief Processing thread for waveform data
	std::unique_ptr<std::thread> m_waveformThread;

	///@brief Scopes whose data is currently being processed for history
	std::set<std::shared_ptr<Oscilloscope> > m_recentlyTriggeredScopes;

	///@brief Groups whose data is currently being processed
	std::set<std::shared_ptr<TriggerGroup>> m_recentlyTriggeredGroups;

	///@brief Mutex to synchronize access to m_recentlyTriggeredScopes
	std::mutex m_recentlyTriggeredScopeMutex;

	///@brief Time we last armed the global trigger
	double m_tArm;

	///@brief Time that the primary scope triggered (in multi-scope setups)
	double m_tPrimaryTrigger;

	///@brief Indicates trigger is armed (incoming waveforms are ignored if not armed)
	bool m_triggerArmed;

	///@brief If true, trigger is currently armed in single-shot mode
	bool m_triggerOneShot;

	///@brief Context for filter graph evaluation
	FilterGraphExecutor m_graphExecutor;

	///@brief Time spent on the last filter graph execution
	std::atomic<int64_t> m_lastFilterGraphExecTime;

	///@brief Mutex for controlling access to m_lastFilterGraphRuntimeStats
	std::mutex m_lastFilterGraphRuntimeMutex;

	///@brief Performance stats from last graph execution
	std::map<FlowGraphNode*, int64_t> m_lastFilterGraphRuntimeStats;

	///@brief Mutex for controlling access to performance counters
	std::mutex m_perfClockMutex;

	///@brief Frequency at which we are pulling waveforms off of scopes
	HzClock m_waveformDownloadRate;

	///@brief Historical waveform data
	HistoryManager m_history;

	///@brief Mutex for controlling access to m_packetmgrs
	std::mutex m_packetMgrMutex;

	///@brief Historical packet data from filters
	std::map<PacketDecoder*, std::shared_ptr<PacketManager> > m_packetmgrs;

	///@brief Mutex for controlling access to rasterized waveforms
	std::mutex m_rasterizedWaveformMutex;

	///@brief True if we have >1 oscilloscope
	bool m_multiScope;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Markers

	///@brief Map of waveform timestamps to markers
	std::map<TimePoint, std::vector<Marker> > m_markers;

	///@brief Number for next autogenerated waveform name
	int m_nextMarkerNum;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Partial graph refreshes

	///@brief Set of dirty channels
	std::set<FlowGraphNode*> m_dirtyChannels;

	///@brief Mutex controlling access to m_dirtyChannels
	std::mutex m_dirtyChannelsMutex;

public:

	/**
		@brief Generate an automatic name for a newly created marker
	 */
	std::string GetNextMarkerName()
	{ return std::string("M") + std::to_string(m_nextMarkerNum ++); }

	/**
		@brief Get the markers for a given waveform timestamp
	 */
	std::vector<Marker>& GetMarkers(TimePoint t)
	{ return m_markers[t]; }

	/**
		@brief Get a list of timestamps for markers
	 */
	std::vector<TimePoint> GetMarkerTimes();

	/**
		@brief Deletes markers for a waveform timestamp
	 */
	void RemoveMarkers(TimePoint t)
	{ m_markers.erase(t); }

	void RemovePackets(TimePoint t, bool immediateRefresh = true);

	std::set<FlowGraphNode*> GetAllGraphNodes();

	///@brief Returns the timestamp of the protocol analyzer event that the mouse is over, if any
	std::optional<TimePoint> GetHoveredPacketTimestamp()
	{ return m_hoverTime; }

	void SetHoveredPacketTimestamp(std::optional<TimePoint> t)
	{ m_hoverTime = t; }

protected:
	std::optional<TimePoint> m_hoverTime;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// End user preferences (persistent across sessions)

	//Preferences state
	PreferenceManager m_preferences;

public:
	PreferenceManager& GetPreferences()
	{ return m_preferences; }

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Reference filters (used to query legal inputs to filters etc)

public:

	/**
		@brief Gets the reference instance of a given filter
	 */
	Filter* GetReferenceFilter(const std::string& name)
	{ return m_referenceFilters[name]; }

	const std::map<std::string, Filter*>& GetReferenceFilters()
	{ return m_referenceFilters; }

	///@brief Get all of the drivers of a given type
	const std::vector<std::string>& GetDriverNamesForType(const std::string& type)
	{ return m_driverNamesByType[type]; }

	void CreateAndAddInstrument(
		const std::string& driver,
		SCPITransport* transport,
		const std::string& nickname);

protected:
	void CreateReferenceFilters();
	void DestroyReferenceFilters();

	std::map<std::string, Filter*> m_referenceFilters;

	///@brief Map of "type" to drivername[]
	std::map<std::string, std::vector<std::string> > m_driverNamesByType;
};

#endif
