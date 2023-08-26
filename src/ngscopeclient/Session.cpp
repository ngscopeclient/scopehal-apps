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

	lock_guard<shared_mutex> lock(m_waveformDataMutex);

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

	//Remove any existing IDs
	m_idtable.clear();

	//Reset state
	m_triggerOneShot = false;
	m_multiScopeFreeRun = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scopesession management

/**
	@brief Deserialize a YAML::Node (and associated data directory) to the current session

	@param node		Root YAML node of the file
	@param dataDir	Path to the _data directory associated with the session
	@param online	True if we should reconnect to instruments

	TODO: do we want some kind of popup to warn about reconfiguring instruments into potentially dangerous states?
	Examples include:
	* changing V/div significantly on a scope channel
	* enabling output of a signal generator or power supply

	@return			True if successful, false on error
 */
bool Session::LoadFromYaml(const YAML::Node& node, const string& dataDir, bool online)
{
	LogTrace("Loading saved session from YAML node\n");
	LogIndenter li;

	//Figure out file version
	int version;
	if (node["version"].IsDefined())
	{
		version = node["version"].as<int>();
		LogTrace("File format version %d\n", version);
	}
	else
	{
		LogTrace("No file format version specified, assuming version 0\n");
		version = 0;
	}

	if(!LoadInstruments(version, node["instruments"], online))
		return false;
	if(!LoadFilters(version, node["decodes"]))
		return false;
	if(!m_mainWindow->LoadUIConfiguration(version, node["ui_config"]))
		return false;
	if(!LoadWaveformData(version, dataDir))
		return false;

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

	//Load data for each scope
	for(size_t i=0; i<m_oscilloscopes.size(); i++)
	{
		auto scope = m_oscilloscopes[i];
		int id = m_idtable[scope];

		char tmp[512];
		snprintf(tmp, sizeof(tmp), "%s/scope_%d_metadata.yml", dataDir.c_str(), id);
		auto docs = YAML::LoadAllFromFile(tmp);

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

	//auto window = m_historyWindows[scope];
	auto wavenode = node["waveforms"];
	int scope_id = m_idtable[scope];

	//Clear out any old waveforms the instrument may have
	for(size_t i=0; i<scope->GetChannelCount(); i++)
	{
		auto chan = scope->GetOscilloscopeChannel(i);
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
			WaveformBase* cap = NULL;
			SparseAnalogWaveform* sacap = NULL;
			UniformAnalogWaveform* uacap = NULL;
			SparseDigitalWaveform* sdcap = NULL;
			UniformDigitalWaveform* udcap = NULL;
			if(chan->GetType(0) == Stream::STREAM_TYPE_ANALOG)
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
		for(size_t i=0; i<nchans; i++)
		{
			DoLoadWaveformDataForScope(
				channels[i].first,
				channels[i].second,
				scope,
				dataDir,
				scope_id,
				waveform_id,
				formats[i]);
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

void Session::DoLoadWaveformDataForScope(
	int channel_index,
	int stream,
	Oscilloscope* scope,
	string datadir,
	int scope_id,
	int waveform_id,
	string format
	)
{
	auto chan = scope->GetOscilloscopeChannel(channel_index);

	auto cap = chan->GetData(stream);
	auto sacap = dynamic_cast<SparseAnalogWaveform*>(cap);
	auto uacap = dynamic_cast<UniformAnalogWaveform*>(cap);
	auto sdcap = dynamic_cast<SparseDigitalWaveform*>(cap);
	auto udcap = dynamic_cast<UniformDigitalWaveform*>(cap);

	cap->PrepareForCpuAccess();

	//Load the actual sample data
	char tmp[512];
	if(stream == 0)
	{
		snprintf(tmp, sizeof(tmp), "%s/scope_%d_waveforms/waveform_%d/channel_%d.bin",
			datadir.c_str(),
			scope_id,
			waveform_id,
			channel_index);
	}
	else
	{
		snprintf(tmp, sizeof(tmp), "%s/scope_%d_waveforms/waveform_%d/channel_%d_stream%d.bin",
			datadir.c_str(),
			scope_id,
			waveform_id,
			channel_index,
			stream);
	}

	//Load samples into memory
	unsigned char* buf = NULL;

	//Windows: use generic file reads for now
	#ifdef _WIN32
		FILE* fp = fopen(tmp, "rb");
		if(!fp)
		{
			LogError("couldn't open %s\n", tmp);
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
		int fd = open(tmp, O_RDONLY);
		if(fd < 0)
		{
			LogError("couldn't open %s\n", tmp);
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
		else
			samplesize += sizeof(bool);
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

			else
			{
				sdcap->m_samples[j] = *reinterpret_cast<bool*>(buf+offset);
				sdcap->m_offsets[j] = stime[0];
				sdcap->m_durations[j] = stime[1];
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
			"Unknown waveform format \"%s\", perhaps this file was created by a newer version of glscopeclient?\n",
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

bool Session::LoadInstruments(int version, const YAML::Node& node, bool online)
{
	LogTrace("Loading saved instruments\n");
	LogIndenter li;

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
			if(!LoadOscilloscope(version, inst, online))
				return false;
		}

		//Check other types
		else if(inst["type"].as<string>() == "multimeter")
		{
			if(!LoadMultimeter(version, inst, online))
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

bool Session::LoadOscilloscope(int version, const YAML::Node& node, bool online)
{
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

			if(transport)
			{
				scope = Oscilloscope::CreateOscilloscope(driver, transport);
				if(!VerifyInstrument(node, scope))
				{
					delete scope;
					scope = nullptr;
				}
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
	m_idtable.emplace(node["id"].as<uintptr_t>(), scope);

	//Configure the scope
	scope->LoadConfiguration(version, node, m_idtable);

	//Load trigger deskew
	if(node["triggerdeskew"])
		m_scopeDeskewCal[scope] = node["triggerdeskew"].as<int64_t>();

	return true;
}

bool Session::LoadMultimeter(int version, const YAML::Node& node, bool online)
{
	SCPIMultimeter* meter = nullptr;

	auto transtype = node["transport"].as<string>();
	auto driver = node["driver"].as<string>();

	if(online)
	{
		if(transtype == "null" /*&& (driver != "demo")*/ )
		{
			m_mainWindow->ShowErrorPopup(
				"Unable to reconnect",
				"The session file does not contain any connection information.\n\n"
				"Loading in offline mode.");
		}

		else
		{
			//Create the meter
			auto transport = CreateTransportForNode(node);
			if(transport)
			{
				meter = SCPIMultimeter::CreateMultimeter(driver, transport);
				if(!VerifyInstrument(node, meter))
				{
					delete meter;
					meter = nullptr;
				}
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

		//placeholder: there's no MockMultimeter yet
		return true;
	}

	//Make any config settings to the instrument from our preference settings, then add it and we're good to go
	//ApplyPreferences(meter);
	m_idtable.emplace(node["meterid"].as<uintptr_t>(), meter);
	meter->LoadConfiguration(version, node, m_idtable);
	AddMultimeter(meter, false);

	return true;
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

		//Save type fields so we know how to recreate the instrument
		auto scope = dynamic_cast<Oscilloscope*>(inst);
		auto meter = dynamic_cast<SCPIMultimeter*>(inst);
		if(scope)
		{
			if(m_scopeDeskewCal.find(scope) != m_scopeDeskewCal.end())
				config["triggerdeskew"] = m_scopeDeskewCal[scope];
			config["type"] = "oscilloscope";
		}
		else if(meter)
		{
			config["type"] = "multimeter";
			config["id"] = config["meterid"].as<int>();
		}

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
	node["appdate"] = __DATE__ __TIME__;

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
			string scopedir = dataDir + "/scope_" + to_string(m_idtable[scope]) + "_waveforms";
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
		string fname = dataDir + "/scope_" + to_string(m_idtable[scope]) + "_metadata.yml";

		ofstream outfs(fname);
		if(!outfs)
			return false;
		outfs << metadataNodes[scope];
		outfs.close();
	}

	//TODO: how/when do we serialize data from filters that have cached state (eye patterns, memories, etc)?

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
void Session::AddBERT(SCPIBERT* bert)
{
	m_modifiedSinceLastSave = true;

	//Create shared BERT state
	auto state = make_shared<BERTState>(bert->GetChannelCount());
	m_berts[bert] = make_unique<BERTConnectionState>(bert, state, this);

	//Add the dialog to view/control it
	m_mainWindow->AddDialog(make_shared<BERTDialog>(bert, state, this));

	m_mainWindow->AddToRecentInstrumentList(bert);
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

	lock_guard<shared_mutex> lock(m_waveformDataMutex);
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
			shared_lock<shared_mutex> lock2(m_waveformDataMutex);
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

	//Update statistic displays after the filter graph update is complete
	//for(auto g : m_waveformGroups)
	//	g->RefreshMeasurements();
	LogTrace("TODO: refresh statistics\n");

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

	//Update statistic displays after the filter graph update is complete
	//for(auto g : m_waveformGroups)
	//	g->RefreshMeasurements();
	LogTrace("TODO: refresh statistics\n");

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
