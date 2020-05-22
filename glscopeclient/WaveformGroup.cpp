/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2019 Andrew D. Zonenberg                                                                          *
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
	@brief Implementation of WaveformGroup
 */
#include "glscopeclient.h"
#include "WaveformGroup.h"
#include "MeasurementDialog.h"

using namespace std;

int WaveformGroup::m_numGroups = 1;

WaveformGroup::WaveformGroup(OscilloscopeWindow* parent)
	: m_timeline(parent, this)
	, m_pixelsPerXUnit(0.05)
	, m_xAxisOffset(0)
	, m_cursorConfig(CURSOR_NONE)
	, m_parent(parent)
{
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Initial GUI hierarchy, title, etc

	m_frame.add(m_vbox);
		m_vbox.pack_start(m_timeline, Gtk::PACK_SHRINK);
		m_vbox.pack_start(m_waveformBox, Gtk::PACK_EXPAND_WIDGET);

	char tmp[64];
	snprintf(tmp, sizeof(tmp), "Waveform Group %d", m_numGroups);
	m_numGroups ++;
	m_frame.set_label(tmp);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Measurements

	m_vbox.pack_start(m_measurementFrame, Gtk::PACK_SHRINK, 5);
	m_measurementFrame.set_label("Measurements");

	m_measurementFrame.add(m_measurementBox);

	m_measurementBox.set_spacing(30);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// New measurements

	m_vbox.pack_start(m_measurementView, Gtk::PACK_SHRINK);
		m_treeModel = Gtk::TreeStore::create(m_treeColumns);
		m_measurementView.set_model(m_treeModel);
		m_measurementView.append_column("", m_treeColumns.m_filterColumn);
		for(int i=1; i<32; i++)
			m_measurementView.append_column("", m_treeColumns.m_columns[i]);
		m_measurementView.set_size_request(1, 90);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Context menu

	m_contextMenu.append(m_removeMeasurementItem);
		m_removeMeasurementItem.set_label("Remove measurement");
		m_removeMeasurementItem.signal_activate().connect(
			sigc::mem_fun(*this, &WaveformGroup::OnRemoveMeasurementItem));
	m_contextMenu.show_all();

	m_selectedColumn = NULL;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Cursors

	m_xCursorPos[0] = 0;
	m_xCursorPos[1] = 0;

	m_yCursorPos[0] = 0;
	m_yCursorPos[1] = 0;
}

WaveformGroup::~WaveformGroup()
{
	//Free each of our channels
	for(size_t i=1; i<32; i++)
	{
		if(m_indexToColumnMap.find(i) != m_indexToColumnMap.end())
			ToggleOff(m_indexToColumnMap[i]);
	}

	auto children = m_treeModel->children();
	for(auto row : children)
	{
		Statistic* stat = row[m_treeColumns.m_statColumn];
		delete stat;
	}

	for(auto c : m_measurementColumns)
		delete c;
}

void WaveformGroup::ToggleOn(OscilloscopeChannel* chan)
{
	//If the channel is already active, do nothing
	if(m_columnToIndexMap.find(chan) != m_columnToIndexMap.end())
		return;

	//If we have no rows, add the initial set of stats
	if(m_treeModel->children().empty())
	{
		AddStatistic(Statistic::CreateStatistic("Maximum"));
		AddStatistic(Statistic::CreateStatistic("Average"));
		AddStatistic(Statistic::CreateStatistic("Minimum"));
	}

	//Use the first free column
	size_t ncol=1;
	for(; ncol<32; ncol ++)
	{
		if(m_indexToColumnMap.find(ncol) == m_indexToColumnMap.end())
			break;
	}

	m_columnToIndexMap[chan] = ncol;
	m_indexToColumnMap[ncol] = chan;

	//Set up the column
	auto col = m_measurementView.get_column(ncol);
	col->set_title(chan->m_displayname);
	col->get_first_cell()->property_xalign() = 1.0;
	col->set_alignment(Gtk::ALIGN_END);

	RefreshMeasurements();

	chan->AddRef();

	m_measurementView.show_all();
}

void WaveformGroup::ToggleOff(OscilloscopeChannel* chan)
{
	int index = m_columnToIndexMap[chan];

	//Delete the current contents of the channel
	m_measurementView.get_column(index)->set_title("");
	auto children = m_treeModel->children();
	for(auto row : children)
		row[m_treeColumns.m_columns[index]] = "";

	//Remove everything from our column records and free the channel
	m_columnToIndexMap.erase(chan);
	m_indexToColumnMap.erase(index);
	chan->Release();

	//If no channels are visible hide the frame
	if(m_columnToIndexMap.empty())
		m_measurementView.hide();
}

bool WaveformGroup::IsShowingStats(OscilloscopeChannel* chan)
{
	return (m_columnToIndexMap.find(chan) != m_columnToIndexMap.end());
}

void WaveformGroup::AddStatistic(Statistic* stat)
{
	auto row = *m_treeModel->append();
	row[m_treeColumns.m_statColumn] = stat;
	row[m_treeColumns.m_filterColumn] = stat->GetStatisticDisplayName();
}

void WaveformGroup::ClearStatistics()
{
	auto children = m_treeModel->children();
	for(auto row : children)
		static_cast<Statistic*>(row[m_treeColumns.m_statColumn])->Clear();
}

void WaveformGroup::RefreshMeasurements()
{
	//Old stuff
	char tmp[256];
	for(auto m : m_measurementColumns)
	{
		//Run the measurement once, then update our text
		m->m_measurement->Refresh();
		snprintf(
			tmp,
			sizeof(tmp),
			"<span font-weight='bold' underline='single'>%s</span>\n"
			"<span rise='-5' font-family='monospace'>%s</span>",
			m->m_title.c_str(), m->m_measurement->GetValueAsString().c_str());
		m->m_label.set_markup(tmp);
	}

	//New tree view
	auto children = m_treeModel->children();
	for(auto row : children)
	{
		Statistic* stat = row[m_treeColumns.m_statColumn];
		for(size_t i=1; i<32; i++)
		{
			//Figure out the input
			if(m_indexToColumnMap.find(i) == m_indexToColumnMap.end())
				continue;
			auto chan = m_indexToColumnMap[i];

			//Evaluate the statistic
			double value;
			if(!stat->Calculate(chan, value))
				row[m_treeColumns.m_columns[i]] = "(error)";
			else
				row[m_treeColumns.m_columns[i]] = chan->GetYAxisUnits().PrettyPrint(value);
		}
	}

	//Update column titles in case a channel got renamed
	for(size_t i=1; i<32; i++)
	{
		if(m_indexToColumnMap.find(i) == m_indexToColumnMap.end())
			continue;

		auto col = m_measurementView.get_column(i);
		auto name = m_indexToColumnMap[i]->m_displayname;
		if(col->get_title() != name)
			col->set_title(name);
	}
}

void WaveformGroup::AddColumn(string name, OscilloscopeChannel* chan, string color)
{
	//Create the measurement itself
	auto m = Measurement::CreateMeasurement(name);
	if(m->GetInputCount() > 1)
	{
		MeasurementDialog dialog(m_parent, m, chan);
		if(dialog.run() != Gtk::RESPONSE_OK)
		{
			delete m;
			return;
		}
		dialog.ConfigureMeasurement();
	}
	else
		m->SetInput(0, chan);

	//Short name of the channel (truncate if too long)
	string shortname = chan->m_displayname;
	if(shortname.length() > 12)
	{
		shortname.resize(9);
		shortname += "...";
	}

	//Name the measurement
	char tmp[256];
	snprintf(tmp, sizeof(tmp), "%s: %s", shortname.c_str(), name.c_str());

	AddColumn(m, color, tmp);
}

void WaveformGroup::AddColumn(Measurement* meas, string color, string label)
{
	//Make sure the measurements can actually be seen
	m_measurementFrame.show();

	//TODO: Don't allow adding the same measurement twice

	//Create the column and figure out the title
	auto col = new MeasurementColumn;
	col->m_title = label;
	m_measurementColumns.emplace(col);
	col->m_measurement = meas;

	//Add to the box and show it
	m_measurementBox.pack_start(col->m_label, Gtk::PACK_SHRINK, 5);
	col->m_label.override_color(Gdk::RGBA(color));
	col->m_label.set_justify(Gtk::JUSTIFY_RIGHT);
	col->m_label.add_events(Gdk::BUTTON_PRESS_MASK);
	col->m_label.show();
	col->m_label.set_selectable();
	col->m_label.signal_button_press_event().connect(
		sigc::bind<MeasurementColumn*>(sigc::mem_fun(*this, &WaveformGroup::OnMeasurementContextMenu), col),
		false);

	//Recalculate stuff now that we have more measurements to look at
	RefreshMeasurements();
}

bool WaveformGroup::OnMeasurementContextMenu(GdkEventButton* event, MeasurementColumn* col)
{
	//SKip anything not right click
	if(event->button != 3)
		return true;

	m_selectedColumn = col;

	m_contextMenu.popup(event->button, event->time);
	return true;
}

void WaveformGroup::OnRemoveMeasurementItem()
{
	m_measurementBox.remove(m_selectedColumn->m_label);
	m_measurementColumns.erase(m_selectedColumn);
	delete m_selectedColumn;
	m_selectedColumn = NULL;

	if(m_measurementColumns.empty())
		m_measurementFrame.hide();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

string WaveformGroup::SerializeConfiguration(IDTable& table)
{
	char tmp[1024];

	snprintf(tmp, sizeof(tmp), "        : \n");
	string config = tmp;
	snprintf(tmp, sizeof(tmp), "            id:             %d\n", table.emplace(&m_frame));
	config += tmp;

	config += "            name:           \"" + m_frame.get_label() + "\"\n";

	snprintf(tmp, sizeof(tmp), "            pixelsPerXUnit: %f\n", m_pixelsPerXUnit);
	config += tmp;
	snprintf(tmp, sizeof(tmp), "            xAxisOffset:    %ld\n", m_xAxisOffset);
	config += tmp;

	switch(m_cursorConfig)
	{
		case WaveformGroup::CURSOR_NONE:
			config += "            cursorConfig:   none\n";
			break;

		case WaveformGroup::CURSOR_X_SINGLE:
			config += "            cursorConfig:   x_single\n";
			break;

		case WaveformGroup::CURSOR_Y_SINGLE:
			config += "            cursorConfig:   y_single\n";
			break;

		case WaveformGroup::CURSOR_X_DUAL:
			config += "            cursorConfig:   x_dual\n";
			break;

		case WaveformGroup::CURSOR_Y_DUAL:
			config += "            cursorConfig:   y_dual\n";
			break;
	}

	snprintf(tmp, sizeof(tmp), "            xcursor0:       %ld\n", m_xCursorPos[0]);
	config += tmp;
	snprintf(tmp, sizeof(tmp), "            xcursor1:       %ld\n", m_xCursorPos[1]);
	config += tmp;
	snprintf(tmp, sizeof(tmp), "            ycursor0:       %f\n", m_yCursorPos[0]);
	config += tmp;
	snprintf(tmp, sizeof(tmp), "            ycursor1:       %f\n", m_yCursorPos[1]);
	config += tmp;

	//Measurements
	if(!m_measurementColumns.empty())
	{
		config += "            measurements: \n";

		for(auto col : m_measurementColumns)
			config += col->m_measurement->SerializeConfiguration(table, col->m_title);
	}

	//Waveform areas
	config += "            areas: \n";
	auto children = m_waveformBox.get_children();
	for(size_t i=0; i<children.size(); i++)
	{
		config += "                : \n";
		snprintf(tmp, sizeof(tmp), "                    id: %d\n", table[children[i]]);
		config += tmp;
	}

	return config;
}

int WaveformGroup::GetIndexOfChild(Gtk::Widget* child)
{
	auto children = m_waveformBox.get_children();
	for(int i=0; i<(int)children.size(); i++)
	{
		if(children[i] == child)
			return i;
	}

	return -1;
}

bool WaveformGroup::IsLastChild(Gtk::Widget* child)
{
	auto children = m_waveformBox.get_children();
	if(children[children.size() - 1] == child)
		return true;

	return false;
}
