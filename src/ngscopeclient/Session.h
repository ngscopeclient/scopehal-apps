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

extern std::atomic<int64_t> g_lastWaveformRenderTime;

class Session;

/**
	@brief Internal state for a connection to an RF signal generator
 */
class RFSignalGeneratorConnectionState
{
public:
	RFSignalGeneratorConnectionState(SCPIRFSignalGenerator* gen, std::shared_ptr<RFSignalGeneratorState> state)
		: m_gen(gen)
		, m_shuttingDown(false)
	{
		RFSignalGeneratorThreadArgs args(gen, &m_shuttingDown, state);
		m_thread = std::make_unique<std::thread>(RFSignalGeneratorThread, args);
	}

	~RFSignalGeneratorConnectionState()
	{
		//Terminate the thread
		m_shuttingDown = true;
		m_thread->join();

		//Disconnect once the thread has terminated
		delete m_gen;
	}

	///@brief The signal generator
	SCPIRFSignalGenerator* m_gen;

	///@brief Termination flag for shutting down the polling thread
	std::atomic<bool> m_shuttingDown;

	///@brief Thread for polling the generator
	std::unique_ptr<std::thread> m_thread;
};

/**
	@brief Internal state for a connection to a PSU
 */
class PowerSupplyConnectionState
{
public:
	PowerSupplyConnectionState(SCPIPowerSupply* psu, std::shared_ptr<PowerSupplyState> state, Session* session)
		: m_psu(psu)
		, m_shuttingDown(false)
	{
		PowerSupplyThreadArgs args(psu, &m_shuttingDown, state, session);
		m_thread = std::make_unique<std::thread>(PowerSupplyThread, args);
	}

	~PowerSupplyConnectionState()
	{
		//Terminate the thread
		m_shuttingDown = true;
		m_thread->join();

		//Disconnect once the thread has terminated
		delete m_psu;
	}

	///@brief The power supply
	SCPIPowerSupply* m_psu;

	///@brief Termination flag for shutting down the polling thread
	std::atomic<bool> m_shuttingDown;

	///@brief Thread for polling the PSU
	std::unique_ptr<std::thread> m_thread;
};

/**
	@brief Internal state for a connection to a multimeter
 */
class MultimeterConnectionState
{
public:
	MultimeterConnectionState(SCPIMultimeter* meter, std::shared_ptr<MultimeterState> state, Session* session)
		: m_meter(meter)
		, m_shuttingDown(false)
	{
		MultimeterThreadArgs args(meter, &m_shuttingDown, state, session);
		m_thread = std::make_unique<std::thread>(MultimeterThread, args);
	}

	~MultimeterConnectionState()
	{
		//Terminate the thread
		m_shuttingDown = true;
		m_thread->join();

		//Delete the meter once the thread has terminated unless it's also an oscilloscope
		if(dynamic_cast<Oscilloscope*>(m_meter) == nullptr)
			delete m_meter;
	}

	///@brief The meter
	SCPIMultimeter* m_meter;

	///@brief Termination flag for shutting down the polling thread
	std::atomic<bool> m_shuttingDown;

	///@brief Thread for polling the meter
	std::unique_ptr<std::thread> m_thread;
};

/**
	@brief Internal state for a connection to a load
 */
class LoadConnectionState
{
public:
	LoadConnectionState(SCPILoad* load, std::shared_ptr<LoadState> state, Session* session)
		: m_load(load)
		, m_shuttingDown(false)
	{
		LoadThreadArgs args(load, &m_shuttingDown, state, session);
		m_thread = std::make_unique<std::thread>(LoadThread, args);
	}

	~LoadConnectionState()
	{
		//Terminate the thread
		m_shuttingDown = true;
		m_thread->join();
	}

	///@brief The load
	SCPILoad* m_load;

	///@brief Termination flag for shutting down the polling thread
	std::atomic<bool> m_shuttingDown;

	///@brief Thread for polling the load
	std::unique_ptr<std::thread> m_thread;
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

	enum TriggerType
	{
		TRIGGER_TYPE_SINGLE,
		TRIGGER_TYPE_FORCED,
		TRIGGER_TYPE_AUTO,
		TRIGGER_TYPE_NORMAL
	};
	void ArmTrigger(TriggerType type);
	void StopTrigger();
	bool HasOnlineScopes();
	void DownloadWaveforms();
	bool CheckForWaveforms(vk::raii::CommandBuffer& cmdbuf);
	void RefreshAllFilters();
	void RefreshAllFiltersNonblocking();
	void RefreshDirtyFiltersNonblocking();
	void RefreshDirtyFilters();

	void MarkChannelDirty(InstrumentChannel* chan);

	void RenderWaveformTextures(
		vk::raii::CommandBuffer& cmdbuf,
		std::vector<std::shared_ptr<DisplayedChannel> >& channels);

	void Clear();
	void ClearBackgroundThreads();

	bool LoadFromYaml(const YAML::Node& node, const std::string& dataDir, bool online);
	YAML::Node SerializeInstrumentConfiguration(IDTable& table);
	YAML::Node SerializeMetadata();
	YAML::Node SerializeFilterConfiguration(IDTable& table);
	YAML::Node SerializeMarkers();
	bool SerializeWaveforms(IDTable& table, const std::string& dataDir);
	bool SerializeSparseWaveform(SparseWaveformBase* wfm, const std::string& path);
	bool SerializeUniformWaveform(UniformWaveformBase* wfm, const std::string& path);

	void AddFunctionGenerator(SCPIFunctionGenerator* generator);
	void RemoveFunctionGenerator(SCPIFunctionGenerator* generator);
	void AddLoad(SCPILoad* generator);
	void RemoveLoad(SCPILoad* generator);
	void AddMultimeter(SCPIMultimeter* meter);
	void RemoveMultimeter(SCPIMultimeter* meter);
	void AddOscilloscope(Oscilloscope* scope, bool createViews = true);
	void AddPowerSupply(SCPIPowerSupply* psu);
	void RemovePowerSupply(SCPIPowerSupply* psu);
	void AddRFGenerator(SCPIRFSignalGenerator* generator);
	void RemoveRFGenerator(SCPIRFSignalGenerator* generator);
	std::shared_ptr<PacketManager> AddPacketFilter(PacketDecoder* filter);

	/**
		@brief Returns a pointer to the existing packet manager for a protocol decode filter
	 */
	std::shared_ptr<PacketManager> GetPacketManager(PacketDecoder* filter)
	{
		std::lock_guard<std::mutex> lock(m_packetMgrMutex);
		return m_packetmgrs[filter];
	}

	void ApplyPreferences(Oscilloscope* scope);

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
	const std::vector<Oscilloscope*> GetScopes()
	{
		std::lock_guard<std::mutex> lock(m_scopeMutex);
		return m_oscilloscopes;
	}

	/**
		@brief Gets the set of all SCPI instruments we're connect to (regardless of type)
	 */
	std::set<SCPIInstrument*> GetSCPIInstruments();

	/**
		@brief Gets the set of all instruments we're connect to (regardless of type)
	 */
	std::set<Instrument*> GetInstruments();

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
	void AddMarker(Marker m)
	{ m_markers[m.m_timestamp].push_back(m); }

	void StartWaveformThreadIfNeeded();

	void ClearSweeps();

	/**
		@brief Get the mutex controlling access to rasterized waveforms
	 */
	std::mutex& GetRasterizedWaveformMutex()
	{ return m_rasterizedWaveformMutex; }

protected:
	void UpdatePacketManagers(const std::set<FlowGraphNode*>& nodes);

	bool LoadInstruments(int version, const YAML::Node& node, bool online, IDTable& table);
	bool LoadOscilloscope(int version, const YAML::Node& node, bool online, IDTable& table);
	bool LoadFilters(int version, const YAML::Node& node, IDTable& table);
	bool LoadWaveformData(int version, const std::string& dataDir, IDTable& table);
	bool LoadWaveformDataForScope(
		int version,
		const YAML::Node& node,
		Oscilloscope* scope,
		const std::string& dataDir,
		IDTable& table);
	void DoLoadWaveformDataForScope(
		int channel_index,
		int stream,
		Oscilloscope* scope,
		std::string datadir,
		int scope_id,
		int waveform_id,
		std::string format);

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
	std::vector<Oscilloscope*> m_oscilloscopes;

	///@brief Deskew correction coefficients for multi-scope
	std::map<Oscilloscope*, int64_t> m_scopeDeskewCal;

	///@brief Power supplies we are currently connected to
	std::map<PowerSupply*, std::unique_ptr<PowerSupplyConnectionState> > m_psus;

	///@brief Multimeters we are currently connected to
	std::map<Multimeter*, std::unique_ptr<MultimeterConnectionState> > m_meters;

	///@brief Loads we are currently connected to
	std::map<Load*, std::unique_ptr<LoadConnectionState> > m_loads;

	///@brief RF generators we are currently connected to
	std::map<SCPIRFSignalGenerator*, std::unique_ptr<RFSignalGeneratorConnectionState> > m_rfgenerators;

	///@brief Function generators we are currently connected to
	std::vector<SCPIFunctionGenerator*> m_generators;

	///@brief Processing threads for polling and processing scope waveforms
	std::vector< std::unique_ptr<std::thread> > m_threads;

	///@brief Processing thread for waveform data
	std::unique_ptr<std::thread> m_waveformThread;

	///@brief Time we last armed the global trigger
	double m_tArm;

	///@brief Time that the primary scope triggered (in multi-scope setups)
	double m_tPrimaryTrigger;

	///@brief Indicates trigger is armed (incoming waveforms are ignored if not armed)
	bool m_triggerArmed;

	///@brief If true, trigger is currently armed in single-shot mode
	bool m_triggerOneShot;

	///@brief True if we have multiple scopes and are in normal trigger mode
	bool m_multiScopeFreeRun;

	///@brief Context for filter graph evaluation
	FilterGraphExecutor m_graphExecutor;

	///@brief Time spent on the last filter graph execution
	std::atomic<int64_t> m_lastFilterGraphExecTime;

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
		@brief Deletes markers for a waveform timestamp
	 */
	void RemoveMarkers(TimePoint t)
	{ m_markers.erase(t); }

	void RemovePackets(TimePoint t);

	std::set<FlowGraphNode*> GetAllGraphNodes();
protected:

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

protected:
	void CreateReferenceFilters();
	void DestroyReferenceFilters();

	std::map<std::string, Filter*> m_referenceFilters;
};

#endif
