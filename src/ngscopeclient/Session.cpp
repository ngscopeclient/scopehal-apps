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
	@brief Implementation of Session
 */
#include "ngscopeclient.h"
#include "ngscopeclient-version.h"
#include "Session.h"
#include "../scopeprotocols/ExportFilter.h"
#include "MainWindow.h"
#include "BERTDialog.h"
#include "FunctionGeneratorDialog.h"
#include "LoadDialog.h"
#include "MultimeterDialog.h"
#include "PowerSupplyDialog.h"
#include "RFGeneratorDialog.h"
#include <fstream>

#include "../scopehal/LeCroyOscilloscope.h"
#include "../scopehal/MockOscilloscope.h"
#include "../scopeprotocols/EyePattern.h"

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#endif

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
	: m_fileLoadVersion(0)
	, m_mainWindow(wnd)
	, m_shuttingDown(false)
	, m_modifiedSinceLastSave(false)
	, m_tArm(0)
	, m_tPrimaryTrigger(0)
	, m_triggerArmed(false)
	, m_triggerOneShot(false)
	, m_graphExecutor(/*8*/1)
	, m_lastFilterGraphExecTime(0)
	, m_history(*this)
	, m_multiScope(false)
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

void Session::FlushConfigCache()
{
	LogTrace("Flushing cache\n");
	LogIndenter li;

	lock_guard<mutex> lock(m_scopeMutex);
	for(auto scope : m_oscilloscopes)
		scope->FlushConfigCache();
}

/**
	@brief Clears all session state and returns the object to an empty state
 */
void Session::Clear()
{
	LogTrace("Clearing session\n");
	LogIndenter li;

	//This includes its own mutex lock on waveform data
	//and can't happen after we hold the lock
	ClearBackgroundThreads();

	lock_guard<shared_mutex> lock(m_waveformDataMutex);

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
	m_misc.clear();
	m_meters.clear();
	m_berts.clear();
	m_misc.clear();
	m_generators.clear();
	m_scopeDeskewCal.clear();
	m_markers.clear();

	//Remove all trigger groups
	m_triggerGroups.clear();
	m_recentlyTriggeredScopes.clear();
	m_recentlyTriggeredGroups.clear();

	//We SHOULD not have any filters at this point.
	//But there have been reports that some stick around. If this happens, print an error message.
	filters = Filter::GetAllInstances();
	for(auto f : filters)
		LogWarning("Leaked filter %s (%zu refs)\n", f->GetHwname().c_str(), f->GetRefCount());

	//Remove any existing IDs
	m_idtable.clear();

	//Reset state
	m_triggerOneShot = false;
	m_multiScope = false;
}

vector<TimePoint> Session::GetMarkerTimes()
{
	vector<TimePoint> ret;
	for(auto it : m_markers)
		ret.push_back(it.first);
	sort(ret.begin(), ret.end(), less<TimePoint>() );
	return ret;
}

void Session::AddMarker(Marker m)
{
	//If we don't have history, add a dummy entry
	if(!m_history.HasHistory(m.m_timestamp))
	{
		vector<Oscilloscope*> empty;
		m_history.AddHistory(
			empty,
			false,
			true,
			"",
			m.m_timestamp);
	}

	//Add the marker
	m_markers[m.m_timestamp].push_back(m);
	OnMarkerChanged();
}

/**
	@brief Called when a marker is added, removed, or modified

	TODO: hint as to what marker and what was changed?
 */
void Session::OnMarkerChanged()
{
	//Sort our markers by timestamp
	auto times = GetMarkerTimes();
	for(auto t : times)
		sort(m_markers[t].begin(), m_markers[t].end());

	//Update the protocol analyzer views that might be displaying it
	lock_guard lock(m_packetMgrMutex);
	for(auto it : m_packetmgrs)
		it.second->OnMarkerChanged();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scopesession management

/**
	@brief Perform partial loading and check for potentially dangerous configurations

	@param node		Root YAML node of the file
	@param dataDir	Path to the _data directory associated with the session
	@param online	True if we should reconnect to instruments

	@return			True if successful, false on error
 */
bool Session::PreLoadFromYaml(const YAML::Node& node, const std::string& /*dataDir*/, bool online)
{
	LogTrace("Preloading saved session from YAML node\n");
	LogIndenter li;

	//Figure out file version
	if (node["version"].IsDefined())
	{
		m_fileLoadVersion = node["version"].as<int>();
		LogTrace("File format version %d\n", m_fileLoadVersion);
	}
	else
	{
		LogTrace("No file format version specified, assuming version 0\n");
		m_fileLoadVersion = 0;
	}

	//Preload our instruments
	if(!PreLoadInstruments(m_fileLoadVersion, node["instruments"], online))
		return false;

	return true;
}

/**
	@brief Deserialize a YAML::Node (and associated data directory) to the current session

	@param node		Root YAML node of the file
	@param dataDir	Path to the _data directory associated with the session
	@param online	True if we should reconnect to instruments

	@return			True if successful, false on error
 */
bool Session::LoadFromYaml(const YAML::Node& node, const string& dataDir, bool online)
{
	LogTrace("Loading saved session from YAML node\n");
	LogIndenter li;

	if(!LoadInstruments(m_fileLoadVersion, node["instruments"], online))
		return false;
	if(!LoadFilters(m_fileLoadVersion, node["decodes"]))
		return false;
	if(!LoadInstrumentInputs(m_fileLoadVersion, node["instruments"]))
		return false;
	if(!m_mainWindow->LoadUIConfiguration(m_fileLoadVersion, node["ui_config"]))
		return false;
	if(!LoadTriggerGroups(node["triggergroups"]))
		return false;
	if(!LoadWaveformData(m_fileLoadVersion, dataDir))
		return false;

	OnMarkerChanged();

	//If we have no waveform data (filter-only session) create a WaveformThread to do rendering,
	//then refresh the filter graph
	if(m_history.empty())
	{
		StartWaveformThreadIfNeeded();
		RefreshAllFiltersNonblocking();
	}

	return true;
}

//TODO: this should run in a background thread or something to keep the UI responsive
bool Session::LoadWaveformData(int version, const string& dataDir)
{
	LogTrace("Loading waveform data\n");

	//Load filter waveforms *before* scope data
	//(we don't want any filters to be updated from nonexistent inputs and change state prior to getting output loaded)
	string fname = dataDir + "/filter_metadata.yml";
	FILE* fp = fopen(fname.c_str(), "r");
	if(fp)
	{
		fclose(fp);

		auto docs = YAML::LoadAllFromFile(fname);
		if(docs.size())
		{
			if(!LoadWaveformDataForFilters(version, docs[0], dataDir))
				return false;
		}
	}

	//Load data for each scope
	for(size_t i=0; i<m_oscilloscopes.size(); i++)
	{
		auto scope = m_oscilloscopes[i];
		int id = m_idtable[(Instrument*)scope];

		char tmp[512] = {0};
		snprintf(tmp, sizeof(tmp), "%s/scope_%d_metadata.yml", dataDir.c_str(), id);
		auto docs = YAML::LoadAllFromFile(tmp);

		//Nothing there? No waveforms at all, skip loading
		if(docs.empty())
			return true;

		if(!LoadWaveformDataForScope(version, docs[0], scope, dataDir))
		{
			LogTrace("Waveform data loading failed\n");
			return false;
		}
	}

	m_history.SetMaxToCurrentDepth();

	return true;
}

/**
	@brief Loads waveform data for filters that need to be preserved
 */
bool Session::LoadWaveformDataForFilters(
		int /*version*/,		//ignored for now, always 2 since older formats don't support filter waveforms
		const YAML::Node& node,
		const string& dataDir)
{
	if(!node)
		return true;
	auto waveforms = node["waveforms"];
	if(!waveforms)
		return true;

	string filtdir = dataDir + "/filter_waveforms";

	for(auto it : waveforms)
	{
		auto ftag = it.second;
		auto id = ftag["id"].as<intptr_t>();

		auto timestamp = ftag["timestamp"].as<int64_t>();
		auto time_fsec = ftag["time_fsec"].as<int64_t>();

		string datdir = filtdir + "/filter_" + to_string(id);

		auto f = static_cast<OscilloscopeChannel*>(m_idtable[id]);
		if(!f)
			continue;
		for(size_t i=0; i<f->GetStreamCount(); i++)
		{
			auto stag = ftag["streams"][string("s") + to_string(i)];
			if(!stag)
				continue;

			auto fmt = stag["format"].as<string>();
			bool dense = (fmt == "densev1");

			//TODO: we need to encode a digital path in the YAML once MemoryFilter has digital channel support
			//TODO: support non-analog/digital captures (eyes, spectrograms, etc)

			WaveformBase* cap = NULL;
			SparseAnalogWaveform* sacap = NULL;
			UniformAnalogWaveform* uacap = NULL;
			//SparseDigitalWaveform* sdcap = NULL;
			//UniformDigitalWaveform* udcap = NULL;
			if(f->GetType(0) == Stream::STREAM_TYPE_ANALOG)
			{
				if(dense)
					cap = uacap = new UniformAnalogWaveform;
				else
					cap = sacap = new SparseAnalogWaveform;
			}
			else
			{
				LogError("unknown stream type loading waveform\n");
				/*
				if(dense)
					cap = udcap = new UniformDigitalWaveform;
				else
					cap = sdcap = new SparseDigitalWaveform;
				*/
			}

			//Channel waveform metadata
			cap->m_timescale = stag["timescale"].as<int64_t>();
			cap->m_startTimestamp = timestamp;
			cap->m_startFemtoseconds = time_fsec;
			cap->m_triggerPhase = stag["trigphase"].as<long long>();
			cap->m_flags = stag["flags"].as<int>();
			f->SetData(cap, i);

			//Actually load the waveform
			string fname = datdir + "/stream" + to_string(i) + ".bin";
			DoLoadWaveformDataForStream(f, i, fmt, fname);
		}
	}

	return true;
}

/**
	@brief Loads waveform data for a single scope
 */
bool Session::LoadWaveformDataForScope(
	int version,
	const YAML::Node& node,
	Oscilloscope* scope,
	const std::string& dataDir)
{
	LogTrace("Loading waveform data for scope \"%s\"\n", scope->m_nickname.c_str());
	LogIndenter li;

	TimePoint time(0, 0);
	TimePoint newest(0, 0);

	auto wavenode = node["waveforms"];
	if(!wavenode)
	{
		//No waveforms
		return true;
	}
	int scope_id = m_idtable[(Instrument*)scope];

	//Clear out any old waveforms the instrument may have
	for(size_t i=0; i<scope->GetChannelCount(); i++)
	{
		//Only delete waveforms from oscilloscope channels
		//(this avoids crashing if the scope is a multi-function device with function generator etc)
		auto chan = scope->GetOscilloscopeChannel(i);
		if(!chan)
			continue;

		for(size_t j=0; j<chan->GetStreamCount(); j++)
			chan->SetData(nullptr, j);
	}

	//Load the data for each waveform
	for(auto it : wavenode)
	{
		//Top level metadata
		bool timebase_is_ps = true;
		auto wfm = it.second;
		time.first = wfm["timestamp"].as<long long>();
		if(wfm["time_psec"])
		{
			time.second = wfm["time_psec"].as<long long>() * 1000;
			timebase_is_ps = true;
		}
		else
		{
			time.second = wfm["time_fsec"].as<long long>();
			timebase_is_ps = false;
		}
		int waveform_id = wfm["id"].as<int>();
		bool pinned = false;
		if(wfm["pinned"])
		{
			if(version <= 1)
				pinned = wfm["pinned"].as<int>();
			else
				pinned = wfm["pinned"].as<bool>();
		}
		string label;
		if(wfm["label"])
			label = wfm["label"].as<string>();

		//If we already have historical data from this timestamp, warn and drop the duplicate data
		auto hist = m_history.GetHistory(time);
		if(hist && (hist->m_history.find(scope) != hist->m_history.end()))
		{
			LogWarning("Session contains duplicate data for time %ld.%ld, discarding\n", time.first, time.second);
			continue;
		}

		//Set up channel metadata first (serialized)
		auto chans = wfm["channels"];
		vector<pair<int, int>> channels;	//pair<channel, stream>
		vector<string> formats;
		for(auto jt : chans)
		{
			auto ch = jt.second;
			int channel_index = ch["index"].as<int>();
			int stream = 0;
			if(ch["stream"])
				stream = ch["stream"].as<int>();
			auto chan = scope->GetOscilloscopeChannel(channel_index);
			channels.push_back(pair<int, int>(channel_index, stream));

			//Waveform format defaults to sparsev1 as that's what was used before
			//the metadata file contained a format ID at all
			string format = "sparsev1";
			if(ch["format"])
				format = ch["format"].as<string>();
			formats.push_back(format);

			bool dense = (format == "densev1");

			//TODO: support non-analog/digital captures (eyes, spectrograms, etc)
			WaveformBase* cap = nullptr;
			SparseAnalogWaveform* sacap = nullptr;
			UniformAnalogWaveform* uacap = nullptr;
			SparseDigitalWaveform* sdcap = nullptr;
			UniformDigitalWaveform* udcap = nullptr;
			CANWaveform* sccap = nullptr;

			//if datatype is specified, use that
			if( (format == "sparsev1") && ch["datatype"] )
			{
				auto dtype = ch["datatype"].as<string>();
				if(dtype == "analog")
					cap = sacap = new SparseAnalogWaveform;
				else if(dtype == "digital")
					cap = sdcap = new SparseDigitalWaveform;
				else if(dtype == "can")
					cap = sccap = new CANWaveform;
				else
					LogError("Unrecognized sparsev1 datatype %s\n", dtype.c_str());
			}

			//if not guess based on stream type
			else if(chan->GetType(0) == Stream::STREAM_TYPE_ANALOG)
			{
				if(dense)
					cap = uacap = new UniformAnalogWaveform;
				else
					cap = sacap = new SparseAnalogWaveform;
			}
			else
			{
				if(dense)
					cap = udcap = new UniformDigitalWaveform;
				else
					cap = sdcap = new SparseDigitalWaveform;
			}

			//Channel waveform metadata
			cap->m_timescale = ch["timescale"].as<long>();
			cap->m_startTimestamp = time.first;
			cap->m_startFemtoseconds = time.second;
			if(timebase_is_ps)
			{
				cap->m_timescale *= 1000;
				cap->m_triggerPhase = ch["trigphase"].as<float>() * 1000;
			}
			else
				cap->m_triggerPhase = ch["trigphase"].as<long long>();

			chan->Detach(stream);
			chan->SetData(cap, stream);
		}

		//Actually load the data for each channel
		size_t nchans = channels.size();
		char tmp[512];
		for(size_t i=0; i<nchans; i++)
		{
			auto nchan = channels[i].first;
			auto nstream = channels[i].second;

			if(nstream == 0)
			{
				snprintf(tmp, sizeof(tmp), "%s/scope_%d_waveforms/waveform_%d/channel_%d.bin",
					dataDir.c_str(),
					scope_id,
					waveform_id,
					nchan);
			}
			else
			{
				snprintf(tmp, sizeof(tmp), "%s/scope_%d_waveforms/waveform_%d/channel_%d_stream%d.bin",
					dataDir.c_str(),
					scope_id,
					waveform_id,
					nchan,
					nstream);
			}

			DoLoadWaveformDataForStream(
				scope->GetOscilloscopeChannel(nchan),
				nstream,
				formats[i],
				tmp);
		}

		vector<Oscilloscope*> temp;
		temp.push_back(scope);
		m_history.AddHistory(temp, false, pinned, label);

		//TODO: this is not good for multiscope
		//TODO: handle eye patterns (need to know window size for it to work right)
		RefreshAllFilters();
	}
	return true;
}

void Session::DoLoadWaveformDataForStream(
	OscilloscopeChannel* chan,
	int stream,
	string format,
	string fname
	)
{
	auto cap = chan->GetData(stream);
	auto sacap = dynamic_cast<SparseAnalogWaveform*>(cap);
	auto uacap = dynamic_cast<UniformAnalogWaveform*>(cap);
	auto sdcap = dynamic_cast<SparseDigitalWaveform*>(cap);
	auto udcap = dynamic_cast<UniformDigitalWaveform*>(cap);
	auto ccap = dynamic_cast<CANWaveform*>(cap);

	cap->PrepareForCpuAccess();

	//Load samples into memory
	unsigned char* buf = NULL;

	//Windows: use generic file reads for now
	#ifdef _WIN32
		FILE* fp = fopen(fname.c_str(), "rb");
		if(!fp)
		{
			LogError("couldn't open %s\n", fname.c_str());
			return;
		}

		//Read the whole file into a buffer a megabyte at a time
		fseek(fp, 0, SEEK_END);
		long len = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		buf = new unsigned char[len];
		long len_remaining = len;
		long blocksize = 1024*1024;
		long read_offset = 0;
		while(len_remaining > 0)
		{
			if(blocksize > len_remaining)
				blocksize = len_remaining;

			//Most time is spent on the fread's when using this path
			fread(buf + read_offset, 1, blocksize, fp);

			len_remaining -= blocksize;
			read_offset += blocksize;
		}
		fclose(fp);

	//On POSIX, just memory map the file
	#else
		int fd = open(fname.c_str(), O_RDONLY);
		if(fd < 0)
		{
			LogError("couldn't open %s\n", fname.c_str());
			return;
		}
		size_t len = lseek(fd, 0, SEEK_END);
		buf = (unsigned char*)mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
	#endif

	//Sparse interleaved
	if(format == "sparsev1")
	{
		//Figure out how many samples we have
		size_t samplesize = 2*sizeof(int64_t);
		if(sacap)
			samplesize += sizeof(float);
		else if(sdcap)
			samplesize += sizeof(bool);
		else if(ccap)
			samplesize += 2*sizeof(int32_t);
		size_t nsamples = len / samplesize;
		cap->Resize(nsamples);

		//TODO: AVX this?
		for(size_t j=0; j<nsamples; j++)
		{
			size_t offset = j*samplesize;

			//Read start time and duration
			int64_t* stime = reinterpret_cast<int64_t*>(buf+offset);
			offset += 2*sizeof(int64_t);

			//Read sample data
			if(sacap)
			{
				//The file format assumes "float" is IEEE754 32-bit float.
				//If your platform doesn't do that, good luck.
				//cppcheck-suppress invalidPointerCast
				sacap->m_samples[j] = *reinterpret_cast<float*>(buf+offset);

				sacap->m_offsets[j] = stime[0];
				sacap->m_durations[j] = stime[1];
			}

			else if(sdcap)
			{
				sdcap->m_samples[j] = *reinterpret_cast<bool*>(buf+offset);
				sdcap->m_offsets[j] = stime[0];
				sdcap->m_durations[j] = stime[1];
			}

			//CAN capture
			else if(ccap)
			{
				uint32_t* p = reinterpret_cast<uint32_t*>(buf+offset);

				ccap->m_samples[j] = CANSymbol((CANSymbol::stype)p[1], p[0]);
				ccap->m_offsets[j] = stime[0];
				ccap->m_durations[j] = stime[1];
			}
		}

		//Quickly check if the waveform is dense packed, even if it was stored as sparse.
		//Since we know samples must be monotonic and non-overlapping, we don't have to check every single one!
		int64_t nlast = nsamples - 1;
		if(sacap)
		{
			if( (sacap->m_offsets[0] == 0) &&
				(sacap->m_offsets[nlast] == nlast) &&
				(sacap->m_durations[nlast] == 1) )
			{
				//Waveform was actually uniform, so convert it
				cap = new UniformAnalogWaveform(*sacap);
				chan->SetData(cap, stream);
			}
		}
	}

	//Dense packed
	else if(format == "densev1")
	{
		//Figure out length
		size_t nsamples = 0;
		if(uacap)
			nsamples = len / sizeof(float);
		else if(udcap)
			nsamples = len / sizeof(bool);
		cap->Resize(nsamples);

		//Read sample data
		if(uacap)
			memcpy(uacap->m_samples.GetCpuPointer(), buf, nsamples*sizeof(float));
		else
			memcpy(udcap->m_samples.GetCpuPointer(), buf, nsamples*sizeof(bool));
	}

	else
	{
		LogError(
			"Unknown waveform format \"%s\", perhaps this file was created by a newer version of ngscopeclient?\n",
			format.c_str());
	}

	cap->MarkModifiedFromCpu();

	#ifdef _WIN32
		delete[] buf;
	#else
		munmap(buf, len);
		::close(fd);
	#endif
}

bool Session::PreLoadInstruments(int version, const YAML::Node& node, bool online)
{
	LogTrace("Preloading saved instruments\n");
	LogIndenter li;

	m_warnings.clear();

	if(!node)
	{
		m_mainWindow->ShowErrorPopup(
			"File load error",
			"The session file is invalid because there is no \"instruments\" section.");
		return false;
	}

	//Load each instrument
	for(auto it : node)
	{
		auto inst = it.second;
		auto nick = inst["nick"].as<string>();
		LogTrace("Loading instrument \"%s\"\n", nick.c_str());

		//See if it's a scope
		//(if no type specified, assume scope for backward compat)
		if(!inst["type"].IsDefined() || (inst["type"].as<string>() == "oscilloscope") )
		{
			if(!PreLoadOscilloscope(version, inst, online))
				return false;
		}
		else if(inst["type"].as<string>() == "psu")
		{
			if(!PreLoadPowerSupply(version, inst, online))
				return false;
		}
		else if(inst["type"].as<string>() == "rfgen")
		{
			if(!PreLoadRFSignalGenerator(version, inst, online))
				return false;
		}
		else if(inst["type"].as<string>() == "funcgen")
		{
			if(!PreLoadFunctionGenerator(version, inst, online))
				return false;
		}
		else if(inst["type"].as<string>() == "multimeter")
		{
			if(!PreLoadMultimeter(version, inst, online))
				return false;
		}
		else if(inst["type"].as<string>() == "spectrometer")
		{
			if(!PreLoadSpectrometer(version, inst, online))
				return false;
		}
		else if(inst["type"].as<string>() == "sdr")
		{
			if(!PreLoadSDR(version, inst, online))
				return false;
		}
		else if(inst["type"].as<string>() == "load")
		{
			if(!PreLoadLoad(version, inst, online))
				return false;
		}
		else if(inst["type"].as<string>() == "bert")
		{
			if(!PreLoadBERT(version, inst, online))
				return false;
		}
		else if(inst["type"].as<string>() == "misc")
		{
			if(!PreLoadMisc(version, inst, online))
				return false;
		}

		//Unknown instrument type - too new file format?
		else
		{
			m_mainWindow->ShowErrorPopup(
				"File load error",
				string("Instrument ") + nick.c_str() + " is of unknown type " + inst["type"].as<string>());
			return false;
		}
	}

	return true;
}

bool Session::LoadInstruments(int version, const YAML::Node& node, bool /*online*/)
{
	LogTrace("Loading saved instruments\n");
	LogIndenter li;

	//Load each instrument
	for(auto it : node)
	{
		auto inst = it.second;
		auto nick = inst["nick"].as<string>();
		LogTrace("Loading instrument \"%s\"\n", nick.c_str());

		auto pinst = reinterpret_cast<Instrument*>(m_idtable[inst["id"].as<uintptr_t>()]);
		if(!pinst)
			continue;

		pinst->LoadConfiguration(version, inst, m_idtable);
	}

	return true;
}

SCPITransport* Session::CreateTransportForNode(const YAML::Node& node)
{
	//Create the scope
	auto transport = SCPITransport::CreateTransport(node["transport"].as<string>(), node["args"].as<string>());

	//Check if the transport failed to initialize
	if((transport == nullptr) || !transport->IsConnected())
	{
		m_mainWindow->ShowErrorPopup(
			"Unable to reconnect",
			string("Failed to connect to instrument using connection string ") + node["args"].as<string>() +
			"Loading in offline mode.");
	}

	return transport;
}

bool Session::VerifyInstrument(const YAML::Node& node, Instrument* inst)
{
	//Sanity check make/model/serial. If mismatch, stop
	//TODO: preference to enforce serial match?
	if(node["name"].as<string>() != inst->GetName())
	{
		m_mainWindow->ShowErrorPopup(
			"Unable to reconnect",
			string("Unable to connect to oscilloscope: instrument has model name \"") +
			inst->GetName() + "\", save file has model name \"" + node["name"].as<string>()  + "\"");
		return false;
	}
	else if(node["vendor"].as<string>() != inst->GetVendor())
	{
		m_mainWindow->ShowErrorPopup(
			"Unable to reconnect",
			string("Unable to connect to oscilloscope: instrument has vendor \"") +
			inst->GetVendor() + "\", save file has vendor \"" + node["vendor"].as<string>()  + "\"");
		return false;
	}
	else if(node["serial"].as<string>() != inst->GetSerial())
	{
		m_mainWindow->ShowErrorPopup(
			"Unable to reconnect",
			string("Unable to connect to oscilloscope: instrument has serial \"") +
			inst->GetSerial() + "\", save file has serial \"" + node["serial"].as<string>()  + "\"");
		return false;
	}

	return true;
}

bool Session::PreLoadOscilloscope(int version, const YAML::Node& node, bool online)
{
	//Create the instrument
	Oscilloscope* scope = nullptr;

	auto transtype = node["transport"].as<string>();
	auto driver = node["driver"].as<string>();

	if(online)
	{
		if( (transtype == "null") && (driver != "demo") )
		{
			m_mainWindow->ShowErrorPopup(
				"Unable to reconnect",
				"The session file does not contain any connection information.\n\n"
				"Loading in offline mode.");
		}

		else
		{
			//Create the scope
			auto transport = CreateTransportForNode(node);

			if(transport && transport->IsConnected())
			{
				scope = Oscilloscope::CreateOscilloscope(driver, transport);
				if(!VerifyInstrument(node, scope))
				{
					delete scope;
					scope = nullptr;
				}
			}
			else
			{
				delete transport;

				m_mainWindow->ShowErrorPopup(
					"Unable to reconnect",
					string("Failed to reconnect to oscilloscope at ") + node["args"].as<string>() + ".\n\n"
					"Loading this instrument in offline mode.");
			}
		}
	}

	if(!scope)
	{
		//Create the mock scope
		scope = new MockOscilloscope(
			node["name"].as<string>(),
			node["vendor"].as<string>(),
			node["serial"].as<string>(),
			transtype,
			driver,
			node["args"].as<string>()
			);
	}

	//Make any config settings to the instrument from our preference settings
	ApplyPreferences(scope);

	//All good. Add to our list of scopes etc
	AddOscilloscope(scope, false);
	m_idtable.emplace(node["id"].as<uintptr_t>(), (Instrument*)scope);

	//Load trigger deskew
	if(node["triggerdeskew"])
		m_scopeDeskewCal[scope] = node["triggerdeskew"].as<int64_t>();

	//Run the preload
	scope->PreLoadConfiguration(version, node, m_idtable, m_warnings);

	return true;
}

bool Session::PreLoadLoad(int version, const YAML::Node& node, bool online)
{
	//Create the instrument
	SCPILoad* load = nullptr;

	auto transtype = node["transport"].as<string>();
	auto driver = node["driver"].as<string>();

	if(online)
	{
		if( (transtype == "null") && (driver != "demoload") )
		{
			m_mainWindow->ShowErrorPopup(
				"Unable to reconnect",
				"The session file does not contain any connection information.\n\n"
				"Loading in offline mode.");
		}

		else
		{
			//Create the PSU
			auto transport = CreateTransportForNode(node);

			if(transport && transport->IsConnected())
			{
				load = SCPILoad::CreateLoad(driver, transport);
				if(!VerifyInstrument(node, load))
				{
					delete load;
					load = nullptr;
				}
			}

			else
			{
				delete transport;

				m_mainWindow->ShowErrorPopup(
					"Unable to reconnect",
					string("Failed to reconnect to load at ") + node["args"].as<string>() + ".\n\n"
					"Loading this instrument in offline mode.");
			}
		}
	}

	if(!load)
	{
		/*
		//Create the mock scope
		scope = new MockOscilloscope(
			node["name"].as<string>(),
			node["vendor"].as<string>(),
			node["serial"].as<string>(),
			transtype,
			driver,
			node["args"].as<string>()
			);
		*/
		LogError("offline loading of loads not implemented yet\n");
		return true;
	}

	//Make any config settings to the instrument from our preference settings
	//ApplyPreferences(load);

	//All good. Add to our list of loads etc
	AddLoad(load, false);
	m_idtable.emplace(node["id"].as<uintptr_t>(), (Instrument*)load);

	//Run the preload
	load->PreLoadConfiguration(version, node, m_idtable, m_warnings);

	return true;
}

bool Session::PreLoadMisc(int version, const YAML::Node& node, bool online)
{
	//Create the instrument
	SCPIMiscInstrument* misc = nullptr;

	auto transtype = node["transport"].as<string>();
	auto driver = node["driver"].as<string>();

	if(online)
	{
		if( (transtype == "null") && (driver != "demoload") )
		{
			m_mainWindow->ShowErrorPopup(
				"Unable to reconnect",
				"The session file does not contain any connection information.\n\n"
				"Loading in offline mode.");
		}

		else
		{
			//Create the PSU
			auto transport = CreateTransportForNode(node);

			if(transport && transport->IsConnected())
			{
				misc = SCPIMiscInstrument::CreateInstrument(driver, transport);
				if(!VerifyInstrument(node, misc))
				{
					delete misc;
					misc = nullptr;
				}
			}

			else
			{
				delete transport;

				m_mainWindow->ShowErrorPopup(
					"Unable to reconnect",
					string("Failed to reconnect to miscellaneous instrument at ") + node["args"].as<string>() + ".\n\n"
					"Loading this instrument in offline mode.");
			}
		}
	}

	if(!misc)
	{
		/*
		//Create the mock scope
		scope = new MockOscilloscope(
			node["name"].as<string>(),
			node["vendor"].as<string>(),
			node["serial"].as<string>(),
			transtype,
			driver,
			node["args"].as<string>()
			);
		*/
		LogError("offline loading of misc instruments not implemented yet\n");
		return true;
	}

	//Make any config settings to the instrument from our preference settings
	//ApplyPreferences(misc);

	//All good. Add to our list of loads etc
	AddMiscInstrument(misc);
	m_idtable.emplace(node["id"].as<uintptr_t>(), (Instrument*)misc);

	//Run the preload
	misc->PreLoadConfiguration(version, node, m_idtable, m_warnings);

	return true;
}

bool Session::PreLoadBERT(int version, const YAML::Node& node, bool online)
{
	//Create the instrument
	SCPIBERT* bert = nullptr;

	auto transtype = node["transport"].as<string>();
	auto driver = node["driver"].as<string>();

	if(online)
	{
		if(transtype == "null")
		{
			m_mainWindow->ShowErrorPopup(
				"Unable to reconnect",
				"The session file does not contain any connection information.\n\n"
				"Loading in offline mode.");
		}

		else
		{
			//Create the BERT
			auto transport = CreateTransportForNode(node);

			if(transport && transport->IsConnected())
			{
				bert = SCPIBERT::CreateBERT(driver, transport);
				if(!VerifyInstrument(node, bert))
				{
					delete bert;
					bert = nullptr;
				}
			}

			else
			{
				delete transport;

				m_mainWindow->ShowErrorPopup(
					"Unable to reconnect",
					string("Failed to reconnect to BERT at ") + node["args"].as<string>() + ".\n\n"
					"Loading this instrument in offline mode.");
			}
		}
	}

	if(!bert)
	{
		/*
		//Create the mock scope
		scope = new MockOscilloscope(
			node["name"].as<string>(),
			node["vendor"].as<string>(),
			node["serial"].as<string>(),
			transtype,
			driver,
			node["args"].as<string>()
			);
		*/
		LogError("offline loading of BERTs not implemented yet\n");
		return true;
	}

	//Make any config settings to the instrument from our preference settings
	//ApplyPreferences(bert);

	//All good. Add to our list of berts etc
	AddBERT(bert, false);
	m_idtable.emplace(node["id"].as<uintptr_t>(), (Instrument*)bert);

	//Run the preload
	bert->PreLoadConfiguration(version, node, m_idtable, m_warnings);

	return true;
}

bool Session::PreLoadSDR(int version, const YAML::Node& node, bool online)
{
	//Create the instrument
	SCPISDR* sdr = nullptr;

	auto transtype = node["transport"].as<string>();
	auto driver = node["driver"].as<string>();

	if(online)
	{
		if( (transtype == "null") && (driver != "demospec") )
		{
			m_mainWindow->ShowErrorPopup(
				"Unable to reconnect",
				"The session file does not contain any connection information.\n\n"
				"Loading in offline mode.");
		}

		else
		{
			//Create the PSU
			auto transport = CreateTransportForNode(node);

			if(transport && transport->IsConnected())
			{
				sdr = SCPISDR::CreateSDR(driver, transport);
				if(!VerifyInstrument(node, sdr))
				{
					delete sdr;
					sdr = nullptr;
				}
			}

			else
			{
				delete transport;

				m_mainWindow->ShowErrorPopup(
					"Unable to reconnect",
					string("Failed to reconnect to SDR at ") + node["args"].as<string>() + ".\n\n"
					"Loading this instrument in offline mode.");
			}
		}
	}

	if(!sdr)
	{
		/*
		//Create the mock scope
		scope = new MockOscilloscope(
			node["name"].as<string>(),
			node["vendor"].as<string>(),
			node["serial"].as<string>(),
			transtype,
			driver,
			node["args"].as<string>()
			);
		*/
		LogError("offline loading of SDRs not implemented yet\n");
		return true;
	}

	//Make any config settings to the instrument from our preference settings
	//ApplyPreferences(sdr);

	//All good. Add to our list of specs etc
	AddSDR(sdr, false);
	m_idtable.emplace(node["id"].as<uintptr_t>(), (Instrument*)sdr);

	//Run the preload
	sdr->PreLoadConfiguration(version, node, m_idtable, m_warnings);

	return true;
}

bool Session::PreLoadSpectrometer(int version, const YAML::Node& node, bool online)
{
	//Create the instrument
	SCPISpectrometer* spec = nullptr;

	auto transtype = node["transport"].as<string>();
	auto driver = node["driver"].as<string>();

	if(online)
	{
		if( (transtype == "null") && (driver != "demospec") )
		{
			m_mainWindow->ShowErrorPopup(
				"Unable to reconnect",
				"The session file does not contain any connection information.\n\n"
				"Loading in offline mode.");
		}

		else
		{
			//Create the PSU
			auto transport = CreateTransportForNode(node);

			if(transport && transport->IsConnected())
			{
				spec = SCPISpectrometer::CreateSpectrometer(driver, transport);
				if(!VerifyInstrument(node, spec))
				{
					delete spec;
					spec = nullptr;
				}
			}

			else
			{
				delete transport;

				m_mainWindow->ShowErrorPopup(
					"Unable to reconnect",
					string("Failed to reconnect to spectrometer at ") + node["args"].as<string>() + ".\n\n"
					"Loading this instrument in offline mode.");
			}
		}
	}

	if(!spec)
	{
		/*
		//Create the mock scope
		scope = new MockOscilloscope(
			node["name"].as<string>(),
			node["vendor"].as<string>(),
			node["serial"].as<string>(),
			transtype,
			driver,
			node["args"].as<string>()
			);
		*/
		LogError("offline loading of spectrometers not implemented yet\n");
		return true;
	}

	//Make any config settings to the instrument from our preference settings
	//ApplyPreferences(spec);

	//All good. Add to our list of specs etc
	AddSpectrometer(spec, false);
	m_idtable.emplace(node["id"].as<uintptr_t>(), (Instrument*)spec);

	//Run the preload
	spec->PreLoadConfiguration(version, node, m_idtable, m_warnings);

	return true;
}

bool Session::PreLoadMultimeter(int version, const YAML::Node& node, bool online)
{
	//Create the instrument
	SCPIMultimeter* meter = nullptr;

	auto transtype = node["transport"].as<string>();
	auto driver = node["driver"].as<string>();

	if(online)
	{
		if( (transtype == "null") && (driver != "demometer") )
		{
			m_mainWindow->ShowErrorPopup(
				"Unable to reconnect",
				"The session file does not contain any connection information.\n\n"
				"Loading in offline mode.");
		}

		else
		{
			//Create the PSU
			auto transport = CreateTransportForNode(node);

			if(transport && transport->IsConnected())
			{
				meter = SCPIMultimeter::CreateMultimeter(driver, transport);
				if(!VerifyInstrument(node, meter))
				{
					delete meter;
					meter = nullptr;
				}
			}

			else
			{
				delete transport;

				m_mainWindow->ShowErrorPopup(
					"Unable to reconnect",
					string("Failed to reconnect to multimeter at ") + node["args"].as<string>() + ".\n\n"
					"Loading this instrument in offline mode.");
			}
		}
	}

	if(!meter)
	{
		/*
		//Create the mock scope
		scope = new MockOscilloscope(
			node["name"].as<string>(),
			node["vendor"].as<string>(),
			node["serial"].as<string>(),
			transtype,
			driver,
			node["args"].as<string>()
			);
		*/
		LogError("offline loading of multimeters not implemented yet\n");
		return true;
	}

	//Make any config settings to the instrument from our preference settings
	//ApplyPreferences(meter);

	//All good. Add to our list of meters etc
	AddMultimeter(meter, false);
	m_idtable.emplace(node["id"].as<uintptr_t>(), (Instrument*)meter);

	//Run the preload
	meter->PreLoadConfiguration(version, node, m_idtable, m_warnings);

	return true;
}

bool Session::PreLoadPowerSupply(int version, const YAML::Node& node, bool online)
{
	//Create the instrument
	SCPIPowerSupply* psu = nullptr;

	auto transtype = node["transport"].as<string>();
	auto driver = node["driver"].as<string>();

	if(online)
	{
		if( (transtype == "null") && (driver != "demopsu") )
		{
			m_mainWindow->ShowErrorPopup(
				"Unable to reconnect",
				"The session file does not contain any connection information.\n\n"
				"Loading in offline mode.");
		}

		else
		{
			//Create the PSU
			auto transport = CreateTransportForNode(node);

			if(transport && transport->IsConnected())
			{
				psu = SCPIPowerSupply::CreatePowerSupply(driver, transport);
				if(!VerifyInstrument(node, psu))
				{
					delete psu;
					psu = nullptr;
				}
			}

			else
			{
				delete transport;

				m_mainWindow->ShowErrorPopup(
					"Unable to reconnect",
					string("Failed to reconnect to power supply at ") + node["args"].as<string>() + ".\n\n"
					"Loading this instrument in offline mode.");
			}
		}
	}

	if(!psu)
	{
		/*
		//Create the mock scope
		scope = new MockOscilloscope(
			node["name"].as<string>(),
			node["vendor"].as<string>(),
			node["serial"].as<string>(),
			transtype,
			driver,
			node["args"].as<string>()
			);
		*/
		LogError("offline loading of power supplies not implemented yet\n");
		return true;
	}

	//Make any config settings to the instrument from our preference settings
	//ApplyPreferences(psu);

	//All good. Add to our list of scopes etc
	AddPowerSupply(psu, false);
	m_idtable.emplace(node["id"].as<uintptr_t>(), (Instrument*)psu);

	//Run the preload
	psu->PreLoadConfiguration(version, node, m_idtable, m_warnings);

	return true;
}

bool Session::PreLoadRFSignalGenerator(int version, const YAML::Node& node, bool online)
{
	//Create the instrument
	SCPIRFSignalGenerator* gen = nullptr;

	auto transtype = node["transport"].as<string>();
	auto driver = node["driver"].as<string>();

	if(online)
	{
		if(transtype == "null")
		{
			m_mainWindow->ShowErrorPopup(
				"Unable to reconnect",
				"The session file does not contain any connection information.\n\n"
				"Loading in offline mode.");
		}

		else
		{
			//Create the PSU
			auto transport = CreateTransportForNode(node);

			if(transport && transport->IsConnected())
			{
				gen = SCPIRFSignalGenerator::CreateRFSignalGenerator(driver, transport);
				if(!VerifyInstrument(node, gen))
				{
					delete gen;
					gen = nullptr;
				}
			}

			else
			{
				delete transport;

				m_mainWindow->ShowErrorPopup(
					"Unable to reconnect",
					string("Failed to reconnect to RF signal generator at ") + node["args"].as<string>() + ".\n\n"
					"Loading this instrument in offline mode.");
			}
		}
	}

	if(!gen)
	{
		/*
		//Create the mock scope
		scope = new MockOscilloscope(
			node["name"].as<string>(),
			node["vendor"].as<string>(),
			node["serial"].as<string>(),
			transtype,
			driver,
			node["args"].as<string>()
			);
		*/
		LogError("offline loading of RF generators not implemented yet\n");
		return true;
	}

	//Make any config settings to the instrument from our preference settings
	//ApplyPreferences(gen);

	//All good. Add to our list of generators etc
	AddRFGenerator(gen);
	m_idtable.emplace(node["id"].as<uintptr_t>(), (Instrument*)gen);

	//Run the preload
	gen->PreLoadConfiguration(version, node, m_idtable, m_warnings);

	return true;
}

bool Session::PreLoadFunctionGenerator(int version, const YAML::Node& node, bool online)
{
	//Create the instrument
	SCPIFunctionGenerator* gen = nullptr;

	auto transtype = node["transport"].as<string>();
	auto driver = node["driver"].as<string>();

	if(online)
	{
		if(transtype == "null")
		{
			m_mainWindow->ShowErrorPopup(
				"Unable to reconnect",
				"The session file does not contain any connection information.\n\n"
				"Loading in offline mode.");
		}

		else
		{
			//Create the PSU
			auto transport = CreateTransportForNode(node);

			if(transport && transport->IsConnected())
			{
				gen = SCPIFunctionGenerator::CreateFunctionGenerator(driver, transport);
				if(!VerifyInstrument(node, gen))
				{
					delete gen;
					gen = nullptr;
				}
			}

			else
			{
				delete transport;

				m_mainWindow->ShowErrorPopup(
					"Unable to reconnect",
					string("Failed to reconnect to function generator at ") + node["args"].as<string>() + ".\n\n"
					"Loading this instrument in offline mode.");
			}
		}
	}

	if(!gen)
	{
		/*
		//Create the mock scope
		scope = new MockOscilloscope(
			node["name"].as<string>(),
			node["vendor"].as<string>(),
			node["serial"].as<string>(),
			transtype,
			driver,
			node["args"].as<string>()
			);
		*/
		LogError("offline loading of function generators not implemented yet\n");
		return true;
	}

	//Make any config settings to the instrument from our preference settings
	//ApplyPreferences(gen);

	//All good. Add to our list of generators etc
	AddFunctionGenerator(gen);
	m_idtable.emplace(node["id"].as<uintptr_t>(), (Instrument*)gen);

	//Run the preload
	gen->PreLoadConfiguration(version, node, m_idtable, m_warnings);

	return true;
}

bool Session::LoadTriggerGroups(const YAML::Node& node)
{
	//No trigger groups node? Older session file, use default config (one group per scope)
	if(!node)
		return true;

	LogTrace("Loading trigger groups\n");

	lock_guard<recursive_mutex> lock(m_triggerGroupMutex);

	//Clear out any existing trigger groups
	m_triggerGroups.clear();

	for(auto it : node)
	{
		auto gnode = it.second;

		LogTrace("Loading trigger group %s\n", it.first.as<string>().c_str());

		//Load scopes
		auto pri = gnode["primary"];
		shared_ptr<TriggerGroup> group;
		if(pri)
		{
			auto scope = dynamic_cast<Oscilloscope*>(reinterpret_cast<Instrument*>(
				m_idtable[pri.as<int64_t>()]));
			group = make_shared<TriggerGroup>(scope, this);

			//Add secondaries
			auto snode = gnode["secondaries"];
			for(auto jt : snode)
			{
				scope = dynamic_cast<Oscilloscope*>(reinterpret_cast<Instrument*>(
					m_idtable[jt.second.as<int64_t>()]));
				group->m_secondaries.push_back(scope);
			}
		}

		//Load filters
		auto filters = gnode["filters"];
		if(filters)
		{
			if(!group)
				group = make_shared<TriggerGroup>(nullptr, this);

			for(auto fid : filters)
				group->m_filters.push_back(reinterpret_cast<PausableFilter*>(m_idtable[fid.as<int64_t>()]));
		}

		m_triggerGroups.push_back(group);

		//See if it's default enabled
		auto dnode = gnode["default"];
		if(dnode)
			group->m_default = dnode.as<bool>();

		//Older file without the default flag
		//Make all non-filter groups default
		else
			group->m_default = group->HasScopes();
	}

	//Check all pausable filters and see if they are in a group
	//(if not, add them to one)
	auto filters = Filter::GetAllInstances();
	for(auto f : filters)
	{
		//Skip if not a pausable filter, or not in the group
		auto pf = dynamic_cast<PausableFilter*>(f);
		if(!pf)
			continue;
		if(GetTriggerGroupForFilter(pf))
			continue;

		//If we get here we have an orphaned filter not in a group
		//Add the filter to the trend group by default
		GetTrendFilterGroup()->AddFilter(pf);
	}

	return true;
}

shared_ptr<TriggerGroup> Session::GetTrendFilterGroup()
{
	//Make sure the group is only trend filters (if it exists)
	if(m_trendTriggerGroup != nullptr)
	{
		if(m_trendTriggerGroup->HasScopes())
			m_trendTriggerGroup = nullptr;
		else
			return m_trendTriggerGroup;
	}

	//See if there is an existing filter-only group we can claim as the trend group
	else
	{
		lock_guard<recursive_mutex> lock(m_triggerGroupMutex);
		for(auto g : m_triggerGroups)
		{
			if(!g->HasScopes() && !g->empty())
			{
				m_trendTriggerGroup = g;
				return m_trendTriggerGroup;
			}
		}
	}

	//We don't have a group yet, make it
	lock_guard<recursive_mutex> lock(m_triggerGroupMutex);
	m_trendTriggerGroup = make_shared<TriggerGroup>(nullptr, this);
	m_trendTriggerGroup->m_default = false;
	m_triggerGroups.push_back(m_trendTriggerGroup);

	LogTrace("Making trend filter group\n");

	return m_trendTriggerGroup;
}

bool Session::LoadFilters(int /*version*/, const YAML::Node& node)
{
	//No protocol decodes? Skip this section
	if(!node)
		return true;

	//Load each decode
	for(auto it : node)
	{
		auto dnode = it.second;

		//Create the decode
		auto proto = dnode["protocol"].as<string>();
		auto filter = Filter::CreateFilter(proto, dnode["color"].as<string>());
		if(filter == NULL)
		{
			m_mainWindow->ShowErrorPopup(
				"Filter creation failed",
				string("Unable to create filter \"") + proto + "\". Skipping...\n");
			continue;
		}

		m_idtable.emplace(dnode["id"].as<uintptr_t>(), filter);

		//Load parameters during the first pass.
		//Parameters can't have dependencies on other channels etc.
		//More importantly, parameters may change bus width etc
		filter->LoadParameters(dnode, m_idtable);

		//Create protocol analyzers
		auto pd = dynamic_cast<PacketDecoder*>(filter);
		if(pd)
			AddPacketFilter(pd);

		//Resize eye patterns to a reasonable default size
		//TODO: ngscopeclient should save actual size
		auto eye = dynamic_cast<EyePattern*>(filter);
		if(eye)
		{
			eye->SetWidth(512);
			eye->SetHeight(512);
		}
	}

	//Make a second pass to configure the filter inputs, once all of them have been instantiated.
	//Filters may depend on other filters as inputs, and serialization is not guaranteed to be a topological sort.
	for(auto it : node)
	{
		auto dnode = it.second;
		auto filter = static_cast<Filter*>(m_idtable[dnode["id"].as<uintptr_t>()]);
		if(filter)
			filter->LoadInputs(dnode, m_idtable);
	}

	return true;
}

bool Session::LoadInstrumentInputs(int /*version*/, const YAML::Node& node)
{
	//Nothing to do? Skip this section
	if(!node)
		return true;

	//Check each instrument in the file and see if we have inputs that need to be hooked up
	//Load each instrument
	for(auto it : node)
	{
		auto inst = it.second;
		auto nick = inst["nick"].as<string>();
		LogTrace("Loading additional inputs for instrument \"%s\"\n", nick.c_str());

		auto pinst = reinterpret_cast<Instrument*>(m_idtable[inst["id"].as<uintptr_t>()]);
		if(!pinst)
			continue;

		for(size_t i=0; i<pinst->GetChannelCount(); i++)
		{
			auto chan = pinst->GetChannel(i);
			auto key = "ch" + to_string(i);
			auto channelNode = inst["channels"][key];

			if(channelNode)
				chan->LoadInputs(channelNode, m_idtable);
		}
	}

	return true;
}

/**
	@brief Serialize the configuration for all oscilloscopes
 */
YAML::Node Session::SerializeInstrumentConfiguration()
{
	YAML::Node node;

	auto instruments = GetInstruments();
	for(auto inst : instruments)
	{
		auto config = inst->SerializeConfiguration(m_idtable);

		/*
			Save type fields so we know how to recreate the instrument

			Precedence rules:
			* Scope-derived instruments are highest precedence: these will probably eventually be refactored
			  as they're not actually "real" scopes but for now we have to check for them first
			* Scopes otherwise have high precedence: any combo instrument is a scope that has some ancillary functions
			* RF gens with baseband function generators are primarily RF gens
		 */
		auto spec = dynamic_cast<SCPISpectrometer*>(inst);
		auto sdr = dynamic_cast<SCPISDR*>(inst);
		auto vna = dynamic_cast<SCPIVNA*>(inst);
		auto scope = dynamic_cast<Oscilloscope*>(inst);
		auto meter = dynamic_cast<SCPIMultimeter*>(inst);
		auto psu = dynamic_cast<SCPIPowerSupply*>(inst);
		auto rfgen = dynamic_cast<SCPIRFSignalGenerator*>(inst);
		auto funcgen = dynamic_cast<SCPIFunctionGenerator*>(inst);
		auto load = dynamic_cast<SCPILoad*>(inst);
		auto bert = dynamic_cast<SCPIBERT*>(inst);
		auto misc = dynamic_cast<SCPIMiscInstrument*>(inst);
		if(spec)
			config["type"] = "spectrometer";
		else if(sdr)
			config["type"] = "sdr";
		else if(vna)
			config["type"] = "vna";
		else if(scope)
		{
			if(m_scopeDeskewCal.find(scope) != m_scopeDeskewCal.end())
				config["triggerdeskew"] = m_scopeDeskewCal[scope];
			config["type"] = "oscilloscope";
		}
		else if(rfgen)
			config["type"] = "rfgen";
		else if(funcgen)
			config["type"] = "funcgen";
		else if(meter)
			config["type"] = "multimeter";
		else if(psu)
			config["type"] = "psu";
		else if(load)
			config["type"] = "load";
		else if(bert)
			config["type"] = "bert";
		else if(misc)
			config["type"] = "misc";

		node["inst" + config["id"].as<string>()] = config;
	}

	return node;
}

/**
	@brief Serialize the configuration for all protocol decoders
 */
YAML::Node Session::SerializeFilterConfiguration()
{
	YAML::Node node;

	auto set = Filter::GetAllInstances();
	for(auto d : set)
	{
		YAML::Node filterNode = d->SerializeConfiguration(m_idtable);
		node["filter" + filterNode["id"].as<string>()] = filterNode;
	}

	return node;
}

/**
	@brief Serializes metadata about the session / software stack

	Not currently used for anything, but might be helpful for troubleshooting etc in the future
 */
YAML::Node Session::SerializeMetadata()
{
	YAML::Node node;
	node["appver"] = "ngscopeclient " NGSCOPECLIENT_VERSION;
	node["appdate"] = __DATE__ " " __TIME__;

	//Format timestamp
	time_t now = time(nullptr);
	struct tm ltime;
#ifdef _WIN32
	localtime_s(&ltime, &now);
#else
	localtime_r(&now, &ltime);
#endif
	char sdate[32];
	char stime[32];
	strftime(stime, sizeof(stime), "%X", &ltime);
	strftime(sdate, sizeof(sdate), "%Y-%m-%d", &ltime);
	node["created"] = string(sdate) + " " + string(stime);

	return node;
}

YAML::Node Session::SerializeTriggerGroups()
{
	YAML::Node node;

	lock_guard<recursive_mutex> lock(m_triggerGroupMutex);
	LogTrace("Serializing trigger groups (%zu total)\n", m_triggerGroups.size());
	LogIndenter li;
	for(auto group : m_triggerGroups)
	{
		auto gid = m_idtable.emplace(group.get());

		//Make a node for the group
		YAML::Node gnode;

		//Primary
		if(group->m_primary)
			gnode["primary"] = m_idtable[(Instrument*)group->m_primary];

		//Secondaries
		YAML::Node secnode;
		for(size_t i=0; i<group->m_secondaries.size(); i++)
			secnode[string("sec") + to_string(i)] = m_idtable[(Instrument*)group->m_secondaries[i]];
		gnode["secondaries"] = secnode;

		//Filters
		YAML::Node fnode;
		for(size_t i=0; i<group->m_filters.size(); i++)
			fnode.push_back(m_idtable[group->m_filters[i]]);
		gnode["filters"] = fnode;

		node[string("group") + to_string(gid)] = gnode;

		gnode["default"] = group->m_default;
	}

	return node;
}

YAML::Node Session::SerializeMarkers()
{
	YAML::Node node;

	int nmarker = 0;
	int nwfm = 0;
	for(auto it : m_markers)
	{
		auto key = it.first;
		auto& markers = it.second;
		if(markers.empty())
			continue;

		YAML::Node markerNode;
		YAML::Node wfmNode;
		wfmNode["timestamp"] = key.first;
		wfmNode["time_fsec"] = key.second;

		for(auto m : markers)
		{
			YAML::Node wfmMarkerNode;
			wfmMarkerNode["offset"] = m.m_offset;
			wfmMarkerNode["name"] = m.m_name;
			wfmNode["markers"]["marker" + to_string(nmarker)] = wfmMarkerNode;

			nmarker ++;
		}

		node["wfm" + to_string(nwfm)] = wfmNode;

		nwfm ++;
	}

	return node;
}

bool Session::SerializeWaveforms(const string& dataDir)
{
	//Metadata nodes for each scope
	std::map<Oscilloscope*, YAML::Node> metadataNodes;

	//Serialize data from each history point
	size_t numwfm = 0;
	for(auto& hpoint : m_history.m_history)
	{
		auto timestamp = hpoint->m_time;

		//Save each scope
		//TODO: Do we want to change the directory hierarchy in a future file format schema?
		//For now, we stick with scope / waveform.
		//In the future we might want trigger group / waveform / scope.
		for(auto it : hpoint->m_history)
		{
			auto scope = it.first;
			auto& hist = it.second;

			//Make the directory for the scope if needed
			string scopedir = dataDir + "/scope_" + to_string(m_idtable[(Instrument*)scope]) + "_waveforms";
			#ifdef _WIN32
				mkdir(scopedir.c_str());
			#else
				mkdir(scopedir.c_str(), 0755);
			#endif

			//Make directory for this waveform
			string datdir = scopedir + "/waveform_" + to_string(numwfm);
			#ifdef _WIN32
				mkdir(datdir.c_str());
			#else
				mkdir(datdir.c_str(), 0755);
			#endif

			//Format metadata for this waveform
			YAML::Node mnode;
			mnode["timestamp"] = timestamp.first;
			mnode["time_fsec"] = timestamp.second;
			mnode["id"] = numwfm;
			mnode["pinned"] = hpoint->m_pinned;
			mnode["label"] = hpoint->m_nickname;
			for(size_t i=0; i<scope->GetChannelCount(); i++)
			{
				auto ochan = dynamic_cast<OscilloscopeChannel*>(scope->GetChannel(i));
				if(!ochan)
					continue;
				for(size_t j=0; j<scope->GetChannel(i)->GetStreamCount(); j++)
				{
					StreamDescriptor stream(ochan, j);
					if(hist.find(stream) == hist.end())
						continue;
					auto data = hist[stream];
					if(data == nullptr)
						continue;

					//Got valid data, save the configuration for the channel
					YAML::Node chnode;
					chnode["index"] = i;
					chnode["stream"] = j;
					chnode["timescale"] = data->m_timescale;
					chnode["trigphase"] = data->m_triggerPhase;
					chnode["flags"] = (int)data->m_flags;
					//don't serialize revision

					//Save the actual waveform data
					string datapath = datdir;
					if(j == 0)
						datapath += string("/channel_") + to_string(i) + ".bin";
					else
						datapath += string("/channel_") + to_string(i) + "_stream" + to_string(j) + ".bin";
					auto sparse = dynamic_cast<SparseWaveformBase*>(data);
					auto uniform = dynamic_cast<UniformWaveformBase*>(data);
					if(sparse)
					{
						chnode["format"] = "sparsev1";
						SerializeSparseWaveform(sparse, datapath);

						//Save type if it's a protocol waveform
						//so if we do an offline load, we know what type of waveform to make
						if(dynamic_cast<SparseAnalogWaveform*>(sparse) != nullptr)
							chnode["datatype"] = "analog";
						else if(dynamic_cast<SparseDigitalWaveform*>(sparse) != nullptr)
							chnode["datatype"] = "digital";
						else if(dynamic_cast<CANWaveform*>(sparse) != nullptr)
							chnode["datatype"] = "can";
					}
					else
					{
						chnode["format"] = "densev1";
						SerializeUniformWaveform(uniform, datapath);
					}

					mnode["channels"][string("ch") + to_string(i) + "s" + to_string(j)] = chnode;
				}
			}

			metadataNodes[scope]["waveforms"][string("wfm") + to_string(numwfm)] = mnode;
		}

		numwfm ++;
	}

	//Write metadata files (by this point, data directories should have been created)
	for(size_t i=0; i<m_oscilloscopes.size(); i++)
	{
		auto scope = m_oscilloscopes[i];
		string fname = dataDir + "/scope_" + to_string(m_idtable[(Instrument*)scope]) + "_metadata.yml";

		ofstream outfs(fname);
		if(!outfs)
			return false;
		outfs << metadataNodes[scope];
		outfs.close();
	}

	//Make directory for filters
	string filtdir = dataDir + "/filter_waveforms";
	#ifdef _WIN32
		mkdir(filtdir.c_str());
	#else
		mkdir(filtdir.c_str(), 0755);
	#endif

	//Find filters that need to be serialized
	YAML::Node filterNode;
	auto filters = Filter::GetAllInstances();
	for(auto f : filters)
	{
		//If it's not being persisted, drop it
		if(!f->ShouldPersistWaveform())
			continue;

		//Make directory for this filter
		auto nfilter = m_idtable.emplace(f);
		string datdir = filtdir + "/filter_" + to_string(nfilter);
		#ifdef _WIN32
			mkdir(datdir.c_str());
		#else
			mkdir(datdir.c_str(), 0755);
		#endif

		//There's no history timestamp so use timestamp of the first stream's waveform
		//If no first stream what do we do?
		auto wref = f->GetData(0);
		if(!wref)
		{
			LogWarning("Don't know how to save filters without a slot 0 waveform\n");
			continue;
		}

		//Format metadata for this waveform
		//No labels etc because we're not in history
		YAML::Node mnode;
		mnode["timestamp"] = wref->m_startTimestamp;
		mnode["time_fsec"] = wref->m_startFemtoseconds;
		mnode["id"] = nfilter;

		for(size_t j=0; j<f->GetStreamCount(); j++)
		{
			StreamDescriptor stream(f, j);
			auto data = stream.GetData();
			if(data == nullptr)
				continue;

			//Got valid data, save the configuration for the channel
			YAML::Node chnode;
			chnode["stream"] = j;
			chnode["timescale"] = data->m_timescale;
			chnode["trigphase"] = data->m_triggerPhase;
			chnode["flags"] = (int)data->m_flags;
			//don't serialize revision

			//Save the actual waveform data
			string datapath = datdir + "/stream" + to_string(j) + ".bin";
			auto sparse = dynamic_cast<SparseWaveformBase*>(data);
			auto uniform = dynamic_cast<UniformWaveformBase*>(data);
			if(sparse)
			{
				chnode["format"] = "sparsev1";
				SerializeSparseWaveform(sparse, datapath);
			}
			else
			{
				chnode["format"] = "densev1";
				SerializeUniformWaveform(uniform, datapath);
			}

			mnode["streams"][string("s") + to_string(j)] = chnode;
		}

		filterNode["waveforms"][string("filt") + to_string(nfilter)] = mnode;
	}

	string fname = dataDir + "/filter_metadata.yml";
	ofstream outfs(fname);
	if(!outfs)
		return false;
	outfs << filterNode;
	outfs.close();

	return true;
}

/**
	@brief Saves waveform sample data in the "sparsev1" file format.

	Interleaved (slow):
		int64 offset
		int64 len
		for analog
			float voltage
		for digital
			bool voltage
 */
bool Session::SerializeSparseWaveform(SparseWaveformBase* wfm, const string& path)
{
	FILE* fp = fopen(path.c_str(), "wb");
	if(!fp)
		return false;

	wfm->PrepareForCpuAccess();
	auto achan = dynamic_cast<SparseAnalogWaveform*>(wfm);
	auto dchan = dynamic_cast<SparseDigitalWaveform*>(wfm);
	auto cchan = dynamic_cast<CANWaveform*>(wfm);
	size_t len = wfm->size();

	//Analog channels
	const size_t samples_per_block = 10000;
	if(achan)
	{
		#pragma pack(push, 1)
		class asample_t
		{
		public:
			int64_t off;
			int64_t dur;
			float voltage;

			asample_t(int64_t o=0, int64_t d=0, float v=0)
			: off(o), dur(d), voltage(v)
			{}
		};
		#pragma pack(pop)

		//Copy sample data
		vector<asample_t,	AlignedAllocator<asample_t, 64 > > samples;
		samples.reserve(len);
		for(size_t i=0; i<len; i++)
			samples.push_back(asample_t(achan->m_offsets[i], achan->m_durations[i], achan->m_samples[i]));

		//Write it
		for(size_t i=0; i<len; i+= samples_per_block)
		{
			size_t blocklen = min(len-i, samples_per_block);
			if(blocklen != fwrite(&samples[i], sizeof(asample_t), blocklen, fp))
			{
				LogError("file write error\n");
				fclose(fp);
				return false;
			}
		}
	}
	else if(dchan)
	{
		#pragma pack(push, 1)
		class dsample_t
		{
		public:
			int64_t off;
			int64_t dur;
			bool voltage;

			dsample_t(int64_t o=0, int64_t d=0, bool v=0)
			: off(o), dur(d), voltage(v)
			{}
		};
		#pragma pack(pop)

		//Copy sample data
		vector<dsample_t,	AlignedAllocator<dsample_t, 64 > > samples;
		samples.reserve(len);
		for(size_t i=0; i<len; i++)
			samples.push_back(dsample_t(dchan->m_offsets[i], dchan->m_durations[i], dchan->m_samples[i]));

		//Write it
		for(size_t i=0; i<len; i+= samples_per_block)
		{
			size_t blocklen = min(len-i, samples_per_block);
			if(blocklen != fwrite(&samples[i], sizeof(dsample_t), blocklen, fp))
			{
				LogError("file write error\n");
				fclose(fp);
			}
		}
	}
	else if(cchan)
	{
		#pragma pack(push, 1)
		class csample_t
		{
		public:
			int64_t off;
			int64_t dur;
			uint32_t data;
			uint32_t type;

			csample_t(int64_t o=0, int64_t d=0, CANSymbol s = CANSymbol())
			: off(o), dur(d), data(s.m_data), type(s.m_stype)
			{}
		};
		#pragma pack(pop)

		//Copy sample data
		vector<csample_t,	AlignedAllocator<csample_t, 64 > > samples;
		samples.reserve(len);
		for(size_t i=0; i<len; i++)
			samples.push_back(csample_t(cchan->m_offsets[i], cchan->m_durations[i], cchan->m_samples[i]));

		//Write it
		for(size_t i=0; i<len; i+= samples_per_block)
		{
			size_t blocklen = min(len-i, samples_per_block);
			if(blocklen != fwrite(&samples[i], sizeof(csample_t), blocklen, fp))
			{
				LogError("file write error\n");
				fclose(fp);
				return false;
			}
		}
	}
	else
	{
		//TODO: support other waveform types (buses, eyes, etc)
		LogError("unrecognized sample type\n");
		fclose(fp);
		return false;
	}

	fclose(fp);
	return true;
}

/**
	@brief Saves waveform sample data in the "densev1" file format.

	for analog
		float[] voltage
	for digital
		bool[] voltage

	Durations are implied {1....1} and offsets are implied {0...n-1}.
 */
bool Session::SerializeUniformWaveform(UniformWaveformBase* wfm, const string& path)
{
	FILE* fp = fopen(path.c_str(), "wb");
	if(!fp)
		return false;

	wfm->PrepareForCpuAccess();
	auto achan = dynamic_cast<UniformAnalogWaveform*>(wfm);
	auto dchan = dynamic_cast<UniformDigitalWaveform*>(wfm);
	size_t len = wfm->size();

	//Analog channels
	const size_t samples_per_block = 10000;
	if(achan)
	{
		//Write it
		for(size_t i=0; i<len; i+= samples_per_block)
		{
			size_t blocklen = min(len-i, samples_per_block);

			if(blocklen != fwrite(achan->m_samples.GetCpuPointer() + i, sizeof(float), blocklen, fp))
			{
				LogError("file write error\n");
				return false;
			}
		}
	}
	else if(dchan)
	{
		//Write it
		for(size_t i=0; i<len; i+= samples_per_block)
		{
			size_t blocklen = min(len-i, samples_per_block);

			if(blocklen != fwrite(dchan->m_samples.GetCpuPointer() + i, sizeof(bool), blocklen, fp))
			{
				LogError("file write error\n");
				return false;
			}
		}
	}
	else
	{
		//TODO: support other waveform types (buses, eyes, etc)
		LogError("unrecognized sample type\n");
		return false;
	}

	fclose(fp);
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Trigger group management

/**
	@brief Remove trigger groups with no instruments in them

	Removes a maximum of one group per invocation.
 */
void Session::GarbageCollectTriggerGroups()
{
	lock_guard<recursive_mutex> lock(m_triggerGroupMutex);

	for(size_t i=0; i<m_triggerGroups.size(); i++)
	{
		if(m_triggerGroups[i]->empty())
		{
			LogTrace("Garbage collecting trigger group\n");
			m_triggerGroups.erase(m_triggerGroups.begin() + i);
			return;
		}
	}
}

/**
	@brief Creates a new trigger group containing only the selected scope
 */
void Session::MakeNewTriggerGroup(Oscilloscope* scope)
{
	lock_guard<recursive_mutex> lock(m_triggerGroupMutex);
	m_triggerGroups.push_back(make_shared<TriggerGroup>(scope, this));
}

void Session::MakeNewTriggerGroup(PausableFilter* filter)
{
	lock_guard<recursive_mutex> lock(m_triggerGroupMutex);
	auto group = make_shared<TriggerGroup>(nullptr, this);
	group->m_default = false;
	group->AddFilter(filter);
	m_triggerGroups.push_back(group);
}

/**
	@brief Check if a scope is the primary of a group containing at least one other scope
 */
bool Session::IsPrimaryOfMultiScopeGroup(Oscilloscope* scope)
{
	lock_guard<recursive_mutex> lock(m_triggerGroupMutex);
	for(auto group : m_triggerGroups)
	{
		if( (group->m_primary == scope) && !group->m_secondaries.empty())
			return true;
	}
	return false;
}

/**
	@brief Check if a scope is a secondary within a multiscope group
 */
bool Session::IsSecondaryOfMultiScopeGroup(Oscilloscope* scope)
{
	lock_guard<recursive_mutex> lock(m_triggerGroupMutex);
	for(auto group : m_triggerGroups)
	{
		//if primary we can't also be a secondary so stop looking
		if(group->m_primary == scope)
			return false;

		for(auto sec : group->m_secondaries)
		{
			if(sec == scope)
				return true;
		}
	}
	return false;
}

/**
	@brief Gets the trigger group that contains a specified scope
 */
shared_ptr<TriggerGroup> Session::GetTriggerGroupForScope(Oscilloscope* scope)
{
	lock_guard<recursive_mutex> lock(m_triggerGroupMutex);
	for(auto group : m_triggerGroups)
	{
		if(group->m_primary == scope)
			return group;

		for(auto sec : group->m_secondaries)
		{
			if(sec == scope)
				return group;
		}
	}

	//should never get here
	LogError("Scope is not part of a trigger group!\n");
	return nullptr;
}

/**
	@brief Gets the trigger group that contains a specified filter
 */
shared_ptr<TriggerGroup> Session::GetTriggerGroupForFilter(PausableFilter* filter)
{
	lock_guard<recursive_mutex> lock(m_triggerGroupMutex);
	for(auto group : m_triggerGroups)
	{
		for(auto f : group->m_filters)
		{
			if(f == filter)
				return group;
		}
	}

	return nullptr;
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

/**
	@brief Adds an oscilloscope to the session

	@param scope		The scope to add
	@param createViews	True if we should add waveform areas for each enabled channel
 */
void Session::AddOscilloscope(Oscilloscope* scope, bool createViews)
{
	lock_guard<mutex> lock(m_scopeMutex);

	m_modifiedSinceLastSave = true;
	m_oscilloscopes.push_back(scope);

	m_threads.push_back(make_unique<thread>(ScopeThread, scope, &m_shuttingDown));

	m_mainWindow->AddToRecentInstrumentList(dynamic_cast<SCPIOscilloscope*>(scope));
	m_mainWindow->OnScopeAdded(scope, createViews);

	//Make a new trigger group (if the scope is online)
	if(!scope->IsOffline())
		MakeNewTriggerGroup(scope);

	StartWaveformThreadIfNeeded();

	if(m_oscilloscopes.size() > 1)
		m_multiScope = true;
}

/**
	@brief Adds a power supply to the session
 */
void Session::AddPowerSupply(SCPIPowerSupply* psu, bool createDialog)
{
	m_modifiedSinceLastSave = true;

	//Create shared PSU state
	auto state = make_shared<PowerSupplyState>(psu->GetChannelCount());
	m_psus[psu] = make_unique<PowerSupplyConnectionState>(psu, state, this);

	//Add the dialog to view/control it
	if(createDialog)
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
void Session::AddMultimeter(SCPIMultimeter* meter, bool createDialog)
{
	m_modifiedSinceLastSave = true;

	//Create shared meter state
	auto state = make_shared<MultimeterState>();
	m_meters[meter] = make_unique<MultimeterConnectionState>(meter, state, this);

	//Add the dialog to view/control it
	if(createDialog)
		m_mainWindow->AddDialog(make_shared<MultimeterDialog>(meter, state, this));

	m_mainWindow->AddToRecentInstrumentList(meter);
}

/**
	@brief Adds a multimeter dialog to the session

	Low level helper, intended to be only used by file loading
 */
void Session::AddMultimeterDialog(SCPIMultimeter* meter)
{
	m_mainWindow->AddDialog(make_shared<MultimeterDialog>(meter, m_meters[meter]->m_state, this));
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
	@brief Adds a miscellaneous instrument
 */
void Session::AddMiscInstrument(SCPIMiscInstrument* inst)
{
	m_modifiedSinceLastSave = true;

	m_mainWindow->AddToRecentInstrumentList(inst);
	m_misc[inst] = make_unique<MiscInstrumentConnectionState>(inst, this);

	StartWaveformThreadIfNeeded();
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
	@brief Adds a BERT to the session
 */
void Session::AddBERT(SCPIBERT* bert, bool createDialog)
{
	m_modifiedSinceLastSave = true;

	//Create shared BERT state
	auto state = make_shared<BERTState>(bert->GetChannelCount());
	m_berts[bert] = make_unique<BERTConnectionState>(bert, state, this);

	//Add the dialog to view/control it
	if(createDialog)
		m_mainWindow->AddDialog(make_shared<BERTDialog>(bert, state, this));

	m_mainWindow->AddToRecentInstrumentList(bert);

	StartWaveformThreadIfNeeded();
}

/**
	@brief Removes a BERT from the session
 */
void Session::RemoveBERT(SCPIBERT* bert)
{
	m_modifiedSinceLastSave = true;

	m_berts.erase(bert);
	delete bert;
}

/**
	@brief Adds a load to the session
 */
void Session::AddLoad(SCPILoad* load, bool createDialog)
{
	m_modifiedSinceLastSave = true;

	//Create shared load state
	auto state = make_shared<LoadState>(load->GetChannelCount());
	m_loads[load] = make_unique<LoadConnectionState>(load, state, this);

	//Add the dialog to view/control it
	if(createDialog)
		m_mainWindow->AddDialog(make_shared<LoadDialog>(load, state, this));

	m_mainWindow->AddToRecentInstrumentList(load);
}

/**
	@brief Removes a load from the session
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

	m_rfgenerators[generator] = make_unique<RFSignalGeneratorConnectionState>(generator, this);
	m_mainWindow->AddDialog(make_shared<RFGeneratorDialog>(generator, this));

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
	for(auto& it : m_berts)
	{
		auto b = dynamic_cast<SCPIBERT*>(it.first);
		if(b != nullptr)
			insts.emplace(b);
	}
	for(auto gen : m_generators)
		insts.emplace(gen);
	for(auto& it : m_misc)
		insts.emplace(it.first);

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
	for(auto& it : m_berts)
		insts.emplace(it.first);
	for(auto& it : m_meters)
		insts.emplace(it.first);
	for(auto& it : m_loads)
		insts.emplace(it.first);
	for(auto& it : m_rfgenerators)
		insts.emplace(it.first);
	for(auto gen : m_generators)
		insts.emplace(gen);
	for(auto& it : m_misc)
		insts.emplace(it.first);

	return insts;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Trigger control

/**
	@brief Arms the trigger for all trigger groups

	@param type		Type of trigger to start
	@param all		If true, arm all groups in the sesison.
					If false, only stop groups with the "default" flag set
 */
void Session::ArmTrigger(TriggerGroup::TriggerType type, bool all)
{
	LogTrace("Arming trigger\n");
	LogIndenter li;

	bool oneshot = (type == TriggerGroup::TRIGGER_TYPE_FORCED) || (type == TriggerGroup::TRIGGER_TYPE_SINGLE);
	m_triggerOneShot = oneshot;

	if(!HasOnlineScopes())
	{
		m_tArm = GetTime();
		m_triggerArmed = true;
		return;
	}

	m_tPrimaryTrigger = -1;

	/*
		If we have multiple scopes, always use single trigger to keep them synced.
		Multi-trigger can lead to race conditions and dropped triggers if we're still downloading a secondary
		instrument's waveform and the primary re-arms.

		Also, order of arming is critical. Secondaries must be completely armed before the primary (instrument 0) to
		ensure that the primary doesn't trigger until the secondaries are ready for the event.
	*/

	//Arm each trigger group (if it's defaulted)
	{
		lock_guard<recursive_mutex> lock(m_triggerGroupMutex);
		for(auto& group : m_triggerGroups)
		{
			if(group->m_default || all)
				group->Arm(type);
		}
	}

	LogTrace("All instruments are armed\n");
	m_tArm = GetTime();
	m_triggerArmed = true;
}

/**
	@brief Stop the trigger for the session

	@param all		If true, stop all groups in the sesison.
					If false, only stop groups with the "default" flag set
 */
void Session::StopTrigger(bool all)
{
	m_triggerArmed = false;

	lock_guard<shared_mutex> lock(m_waveformDataMutex);
	lock_guard<recursive_mutex> lock2(m_triggerGroupMutex);
	for(auto& group : m_triggerGroups)
	{
		if(group->m_default || all)
			group->Stop();
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

	//No online scopes to poll? Re-run the filter graph if we're armed
	if(!HasOnlineScopes())
		return m_triggerArmed;

	//Return true if any group has fully triggered
	lock_guard<recursive_mutex> lock2(m_triggerGroupMutex);
	for(auto& group : m_triggerGroups)
	{
		if(group->CheckForPendingWaveforms())
			return true;
	}

	//Nothing ready
	return false;
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

	lock_guard<shared_mutex> lock(m_waveformDataMutex);
	lock_guard<mutex> lock2(m_scopeMutex);
	lock_guard<recursive_mutex> lock3(m_triggerGroupMutex);

	//Get the data from each  trigger group
	for(auto group : m_triggerGroups)
	{
		if(!group->CheckForPendingWaveforms())
			continue;

		group->DownloadWaveforms();

		//This scope has recently triggered and should be added to history
		{
			lock_guard<mutex> lock4(m_recentlyTriggeredScopeMutex);
			m_recentlyTriggeredScopes.emplace(group->m_primary);
			m_recentlyTriggeredGroups.emplace(group);
			for(auto scope : group->m_secondaries)
				m_recentlyTriggeredScopes.emplace(scope);
		}
	}

	//If we're in offline one-shot mode, disarm the trigger
	if( m_triggerGroups.empty() && m_triggerOneShot)
		m_triggerArmed = false;
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
		vector<Oscilloscope*> scopes;
		set<shared_ptr<TriggerGroup>> groups;
		{
			shared_lock<shared_mutex> lock2(m_waveformDataMutex);
			lock_guard<mutex> lock(m_recentlyTriggeredScopeMutex);
			for(auto scope : m_recentlyTriggeredScopes)
				scopes.push_back(scope);
			m_recentlyTriggeredScopes.clear();

			groups = m_recentlyTriggeredGroups;
			m_recentlyTriggeredGroups.clear();

			m_history.AddHistory(scopes);
		}

		//Tone-map all of our waveforms
		//Generally does not need waveform data locked since it only works on *rendered* data...
		//but density functions like spectrogram are an exception as those don't have a render step.
		//TODO: should we "snapshot" the waveform into a render buffer or something to avoid this sync point?
		hadNewWaveforms = true;
		{
			lock_guard<shared_mutex> lock(m_waveformDataMutex);
			m_mainWindow->ToneMapAllWaveforms(cmdbuf);
		}

		//Release the waveform processing thread
		g_waveformProcessedEvent.Signal();

		//In multi-scope free-run mode, re-arm every instrument's trigger after we've processed all data
		for(auto group : groups)
			group->RearmIfMultiScope();
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

	Avoid waking up the waveform thread if we have no dirty filters, though.
 */
void Session::RefreshDirtyFiltersNonblocking()
{
	{
		lock_guard<mutex> lock(m_dirtyChannelsMutex);
		if(m_dirtyChannels.empty())
			return;
	}

	g_partialRefilterRequestedEvent.Signal();
}

/**
	@brief Gets all of our graph nodes (filters plus instrument channels)
 */
set<FlowGraphNode*> Session::GetAllGraphNodes()
{
	//Start with all filters
	set<FlowGraphNode*> nodes;
	{
		lock_guard<mutex> lock2(m_filterUpdatingMutex);
		auto filters = Filter::GetAllInstances();
		for(auto f : filters)
			nodes.emplace(f);
	}

	//then add instrument channels
	auto insts = GetInstruments();
	for(auto inst : insts)
	{
		for(size_t i=0; i<inst->GetChannelCount(); i++)
			nodes.emplace(inst->GetChannel(i));
	}

	return nodes;
}

void Session::RefreshAllFilters()
{
	double tstart = GetTime();

	auto nodes = GetAllGraphNodes();

	{
		//Must lock mutexes in this order to avoid deadlock
		lock_guard<shared_mutex> lock(m_waveformDataMutex);
		//shared_lock<shared_mutex> lock3(g_vulkanActivityMutex);
		m_graphExecutor.RunBlocking(nodes);
		UpdatePacketManagers(nodes);
	}

	m_lastFilterGraphExecTime = (GetTime() - tstart) * FS_PER_SECOND;
}

/**
	@brief Refresh dirty filters (and anything in their downstream influence cone)

	@return True if at least one filter was refreshed, false if nothing was dirty
 */
bool Session::RefreshDirtyFilters()
{
	set<FlowGraphNode*> nodesToUpdate;

	{
		lock_guard<mutex> lock(m_dirtyChannelsMutex);
		if(m_dirtyChannels.empty())
			return false;

		//Start with all nodes
		auto nodes = GetAllGraphNodes();

		//Check each one to see if it needs updating
		for(auto f : nodes)
		{
			if(f->IsDownstreamOf(m_dirtyChannels))
				nodesToUpdate.emplace(f);
		}

		//The filter itself needs to be updated too
		for(auto node : m_dirtyChannels)
		{
			auto f = dynamic_cast<Filter*>(node);
			if(f)
				nodesToUpdate.emplace(f);
		}

		//Reset list for next round
		m_dirtyChannels.clear();
	}
	if(nodesToUpdate.empty())
		return false;

	//Refresh the dirty filters only
	double tstart = GetTime();

	{
		//Must lock mutexes in this order to avoid deadlock
		lock_guard<shared_mutex> lock(m_waveformDataMutex);
		shared_lock<shared_mutex> lock3(g_vulkanActivityMutex);
		m_graphExecutor.RunBlocking(nodesToUpdate);
		UpdatePacketManagers(nodesToUpdate);
	}

	m_lastFilterGraphExecTime = (GetTime() - tstart) * FS_PER_SECOND;

	return true;
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
	lock_guard<shared_mutex> lock(m_waveformDataMutex);

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
void Session::UpdatePacketManagers(const set<FlowGraphNode*>& nodes)
{
	lock_guard<mutex> lock(m_packetMgrMutex);

	set<PacketDecoder*> deletedFilters;
	for(auto it : m_packetmgrs)
	{
		//Remove filters that no longer exist
		if(nodes.find(it.first) == nodes.end())
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
	shared_ptr<PacketManager> ret = make_shared<PacketManager>(filter, *this);
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
