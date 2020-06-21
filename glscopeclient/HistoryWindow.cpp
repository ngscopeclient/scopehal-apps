/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2020 Andrew D. Zonenberg                                                                          *
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
	@brief  Implementation of HistoryWindow
 */
#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "HistoryWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HistoryColumns

HistoryColumns::HistoryColumns()
{
	add(m_timestamp);
	add(m_capturekey);
	add(m_history);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

HistoryWindow::HistoryWindow(OscilloscopeWindow* parent, Oscilloscope* scope)
	: Gtk::Dialog(string("History: ") + scope->m_nickname, *parent)
	, m_parent(parent)
	, m_scope(scope)
	, m_updating(false)
{
	set_skip_taskbar_hint();
	set_type_hint(Gdk::WINDOW_TYPE_HINT_TOOLBAR);

	set_default_size(320, 800);

	//Set up the tree view
	m_model = Gtk::TreeStore::create(m_columns);
	m_tree.set_model(m_model);
	m_tree.get_selection()->signal_changed().connect(
		sigc::mem_fun(*this, &HistoryWindow::OnSelectionChanged));

	//Add the columns
	m_tree.append_column("Time", m_columns.m_timestamp);

	//Set up the widgets
	get_vbox()->pack_start(m_hbox, Gtk::PACK_SHRINK);
		m_hbox.pack_start(m_maxLabel, Gtk::PACK_SHRINK);
			m_maxLabel.set_label("Max waveforms");
		m_hbox.pack_start(m_maxBox, Gtk::PACK_EXPAND_WIDGET);
			SetMaxWaveforms(10);
	get_vbox()->pack_start(m_scroller, Gtk::PACK_EXPAND_WIDGET);
		m_scroller.add(m_tree);
		m_scroller.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
			m_tree.get_selection()->set_mode(Gtk::SELECTION_BROWSE);
	get_vbox()->pack_start(m_status, Gtk::PACK_SHRINK);
		m_status.pack_end(m_memoryLabel, Gtk::PACK_SHRINK);
			m_memoryLabel.set_text("");
	show_all();

	//not shown by default
	hide();
}

HistoryWindow::~HistoryWindow()
{
	//Delete old waveform data
	auto children = m_model->children();
	for(auto it : children)
	{
		WaveformHistory hist = it[m_columns.m_history];
		for(auto w : hist)
		{
			//Do *not* delete the channel's current data!
			if(w.second != w.first->GetData())
				delete w.second;
		}
	}
}

void HistoryWindow::SetMaxWaveforms(int n)
{
	char tmp[128];
	snprintf(tmp, sizeof(tmp), "%d", n);
	m_maxBox.set_text(tmp);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

void HistoryWindow::OnWaveformDataReady()
{
	//Use the timestamp from the first enabled channel
	OscilloscopeChannel* chan = NULL;
	WaveformBase* data = NULL;
	for(size_t i=0; i<m_scope->GetChannelCount(); i++)
	{
		chan = m_scope->GetChannel(i);
		if(chan->IsEnabled())
		{
			data = chan->GetData();
			break;
		}
	}

	//No channels at all? Nothing to do
	if(chan == NULL)
		return;
	if(data == NULL)
		return;

	m_updating = true;

	//Format timestamp
	char tmp[128];
	struct tm ltime;
	
#ifdef _WIN32
	localtime_s(&ltime, &data->m_startTimestamp);
#else
	localtime_r(&data->m_startTimestamp, &ltime);
#endif

	strftime(tmp, sizeof(tmp), "%H:%M:%S.", &ltime);
	string stime = tmp;
	snprintf(tmp, sizeof(tmp), "%010zu", data->m_startPicoseconds / 100);	//round to nearest 100ps for display
	stime += tmp;

	//Create the row
	auto row = *m_model->append();
	row[m_columns.m_timestamp] = stime;
	TimePoint key(data->m_startTimestamp, data->m_startPicoseconds);
	row[m_columns.m_capturekey] = key;

	//Add waveform data
	WaveformHistory hist;
	for(size_t i=0; i<m_scope->GetChannelCount(); i++)
	{
		auto c = m_scope->GetChannel(i);
		auto dat = c->GetData();
		if(!c->IsEnabled())		//don't save historical waveforms from disabled channels
		{
			hist[c] = NULL;
			continue;
		}
		if(!dat)
			continue;
		hist[c] = dat;

		//Clear excess space out of the waveform buffer
		auto adat = dynamic_cast<AnalogWaveform*>(data);
		if(adat)
			adat->m_samples.shrink_to_fit();
	}
	row[m_columns.m_history] = hist;

	//auto scroll to bottom
	auto adj = m_scroller.get_vadjustment();
	adj->set_value(adj->get_upper());

	//Select the newly added row
	m_tree.get_selection()->select(row);

	//Remove extra waveforms, if we have any.
	string smax = m_maxBox.get_text();
	size_t nmax = atoi(smax.c_str());
	auto children = m_model->children();
	while(children.size() > nmax)
	{
		//Delete any protocol decodes from this waveform
		auto it = children.begin();
		key = (*it)[m_columns.m_capturekey];
		m_parent->RemoveHistory(key);

		//Delete the saved waveform data
		hist = (*it)[m_columns.m_history];
		for(auto w : hist)
			delete w.second;

		m_model->erase(it);
	}

	//Calculate our RAM usage (rough estimate)
	size_t bytes_used = 0;
	for(auto it : children)
	{
		hist = (*it)[m_columns.m_history];
		for(auto jt : hist)
		{
			auto acap = dynamic_cast<AnalogWaveform*>(jt.second);
			if(acap != NULL)
			{
				//Add static size of the capture object
				bytes_used += sizeof(AnalogWaveform);

				//Add size of each sample
				bytes_used += sizeof(float) * acap->m_samples.capacity();
				bytes_used += sizeof(int64_t) * acap->m_offsets.capacity();
				bytes_used += sizeof(int64_t) * acap->m_durations.capacity();
			}

			auto dcap = dynamic_cast<DigitalWaveform*>(jt.second);
			if(dcap != NULL)
			{
				//Add static size of the capture object
				bytes_used += sizeof(DigitalWaveform);

				//Add size of each sample
				bytes_used += sizeof(bool) * dcap->m_samples.capacity();
				bytes_used += sizeof(int64_t) * dcap->m_offsets.capacity();
				bytes_used += sizeof(int64_t) * dcap->m_durations.capacity();
			}

			auto bcap = dynamic_cast<DigitalBusWaveform*>(jt.second);
			if(bcap != NULL)
			{
				//Add static size of the capture object
				bytes_used += sizeof(DigitalBusWaveform);

				//Add size of each sample
				bytes_used +=
					(bcap->m_samples[0].size() * sizeof(bool) + sizeof(vector<bool>))
					* bcap->m_samples.capacity();
				bytes_used += sizeof(int64_t) * bcap->m_offsets.capacity();
				bytes_used += sizeof(int64_t) * bcap->m_durations.capacity();
			}
		}
	}

	//Convert to MB/GB
	float mb = bytes_used / (1024.0f * 1024.0f);
	float gb = mb / 1024;
	if(gb > 1)
		snprintf(tmp, sizeof(tmp), "%u WFM / %.2f GB", children.size(), gb);
	else
		snprintf(tmp, sizeof(tmp), "%u WFM / %.0f MB", children.size(), mb);
	m_memoryLabel.set_label(tmp);

	m_updating = false;
}

bool HistoryWindow::on_delete_event(GdkEventAny* /*ignored*/)
{
	m_parent->HideHistory();
	return true;
}

void HistoryWindow::OnSelectionChanged()
{
	//If we're updating with a new waveform we're already on the newest waveform.
	//No need to refresh anything.
	if(m_updating)
		return;

	auto row = *m_tree.get_selection()->get_selected();
	WaveformHistory hist = row[m_columns.m_history];

	//Reload the scope with the saved waveforms
	for(auto it : hist)
	{
		it.first->Detach();
		it.first->SetData(it.second);
	}

	//Tell the window to refresh everything
	m_parent->OnHistoryUpdated();
}

void HistoryWindow::JumpToHistory(TimePoint timestamp)
{
	//TODO: is there a way to binary search a tree view?
	//Or get *stable* iterators that aren't invalidated by adding/removing items?
	auto children = m_model->children();
	for(auto it : children)
	{
		TimePoint key = (*it)[m_columns.m_capturekey];
		if(key == timestamp)
		{
			m_tree.get_selection()->select(it);
			break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Serialization

void HistoryWindow::SerializeWaveforms(string dir, IDTable& table)
{
	//Figure out file name, and make the waveform directory
	char tmp[512];
	snprintf(tmp, sizeof(tmp), "%s/scope_%d_metadata.yml", dir.c_str(), table[m_scope]);
	string fname = tmp;
	snprintf(tmp, sizeof(tmp), "%s/scope_%d_waveforms", dir.c_str(), table[m_scope]);
	
#ifdef _WIN32
	mkdir(tmp);
#else
	mkdir(tmp, 0755);
#endif
	
	string dname = tmp;

	//Serialize waveforms
	string config = "waveforms:\n";
	auto children = m_model->children();
	int id = 1;
	for(auto it : children)
	{
		TimePoint key = (*it)[m_columns.m_capturekey];

		//Save metadata
		config += "    :\n";
		snprintf(tmp, sizeof(tmp), "        timestamp: %ld\n", key.first);
		config += tmp;
		snprintf(tmp, sizeof(tmp), "        time_psec: %ld\n", key.second);
		config += tmp;
		snprintf(tmp, sizeof(tmp), "        id:        %d\n", id);
		config += tmp;
		config += "        channels:\n";

		//Format directory for this waveform
		snprintf(tmp, sizeof(tmp), "%s/waveform_%d", dname.c_str(), id);
		
#ifdef _WIN32
	mkdir(tmp);
#else
	mkdir(tmp, 0755);
#endif

		string wname = tmp;

		//Save waveform data
		WaveformHistory history = (*it)[m_columns.m_history];
		for(auto jt : history)
		{
			int index = jt.first->GetIndex();
			auto chan = jt.second;
			if(chan == NULL)		//trigger, disabled, etc
				continue;

			snprintf(tmp, sizeof(tmp), "%s/channel_%d.bin", wname.c_str(), index);
			FILE* fp = fopen(tmp, "wb");

			//Save channel metadata
			config += "            :\n";
			snprintf(tmp, sizeof(tmp), "                index:        %d\n", index);
			config += tmp;
			snprintf(tmp, sizeof(tmp), "                timescale:    %ld\n", chan->m_timescale);
			config += tmp;
			snprintf(tmp, sizeof(tmp), "                trigphase:    %f\n", chan->m_triggerPhase);
			config += tmp;

			//Save channel data
			auto achan = dynamic_cast<AnalogWaveform*>(chan);
			auto dchan = dynamic_cast<DigitalWaveform*>(chan);
			size_t len = chan->m_offsets.size();
			for(size_t i=0; i<len; i++)
			{
				int64_t times[2] = { chan->m_offsets[i], chan->m_durations[i] };
				if(2 != fwrite(times, sizeof(int64_t), 2, fp))
					LogError("file write error\n");
				if(achan)
				{
					if(1 != fwrite(&achan->m_samples[i], sizeof(float), 1, fp))
						LogError("file write error\n");
				}
				else if(dchan)
				{
					bool b = dchan->m_samples[i];
					if(1 != fwrite(&b, sizeof(bool), 1, fp))
						LogError("file write error\n");
				}
			}
			//TODO: support other waveform types (buses, eyes, etc)
			fclose(fp);
		}

		id ++;
	}

	//Save waveform metadata
	FILE* fp = fopen(fname.c_str(), "w");
	if(!fp)
	{
		string msg = string("The data file ") + fname + " could not be created!";
		Gtk::MessageDialog errdlg(msg, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		errdlg.set_title("Cannot save session\n");
		errdlg.run();
		return;
	}
	if(config.length() != fwrite(config.c_str(), 1, config.length(), fp))
	{
		string msg = string("Error writing to session file ") + fname + "!";
		Gtk::MessageDialog errdlg(msg, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		errdlg.set_title("Cannot save session\n");
		errdlg.run();
	}
	fclose(fp);
}

void HistoryWindow::ReplayHistory()
{
	//TODO: is there a way to binary search a tree view?
	//Or get *stable* iterators that aren't invalidated by adding/removing items?
	auto children = m_model->children();
	for(auto it : children)
	{
		//Select will update all the protocol decoders etc
		m_tree.get_selection()->select(it);

		//Update analyzers
		m_parent->OnHistoryUpdated(true);
	}
}
