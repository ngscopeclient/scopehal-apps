/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
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
	@brief Implementation of WaveformGroup
 */
#include "glscopeclient.h"
#include "WaveformGroup.h"
#include "ChannelPropertiesDialog.h"
#include "FilterDialog.h"

using namespace std;

int WaveformGroup::m_numGroups = 1;

WaveformGroup::WaveformGroup(OscilloscopeWindow* parent)
	: m_timeline(parent, this)
	, m_pixelsPerXUnit(0.00005)
	, m_xAxisOffset(0)
	, m_cursorConfig(CURSOR_NONE)
	, m_parent(parent)
	, m_measurementContextMenuChannel(NULL)
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
	// Statistics

	m_vbox.pack_start(m_measurementView, Gtk::PACK_SHRINK);
		m_treeModel = Gtk::TreeStore::create(m_treeColumns);
		m_measurementView.set_model(m_treeModel);
		m_measurementView.append_column("", m_treeColumns.m_filterColumn);
		for(int i=1; i<32; i++)
			m_measurementView.append_column("", m_treeColumns.m_columns[i]);
		m_measurementView.set_size_request(1, 90);
		m_measurementView.get_selection()->set_mode(Gtk::SELECTION_NONE);

	m_measurementView.signal_button_press_event().connect_notify(
		sigc::mem_fun(*this, &WaveformGroup::OnMeasurementButtonPressEvent));

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Context menu

	m_contextMenu.append(m_propertiesItem);
		m_propertiesItem.set_label("Properties...");
		m_propertiesItem.signal_activate().connect(sigc::mem_fun(*this, &WaveformGroup::OnStatisticProperties));

	m_contextMenu.append(m_hideItem);
		m_hideItem.set_label("Hide");
		m_hideItem.signal_activate().connect(sigc::mem_fun(*this, &WaveformGroup::OnHideStatistic));

	m_contextMenu.show_all();

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
	col->set_title(chan->GetDisplayName());
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
				row[m_treeColumns.m_columns[i]] = stat->GetUnits(chan).PrettyPrint(value);
		}
	}

	//Update column titles in case a channel got renamed
	for(size_t i=1; i<32; i++)
	{
		if(m_indexToColumnMap.find(i) == m_indexToColumnMap.end())
			continue;

		auto col = m_measurementView.get_column(i);
		auto name = m_indexToColumnMap[i]->GetDisplayName();

		//Truncate names if they are too long
		if(name.length() > 20)
		{
			name.resize(17);
			name += "...";
		}

		if(col->get_title() != name)
			col->set_title(name);
	}
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

	snprintf(tmp, sizeof(tmp), "            timebaseResolution: fs\n");
	config += tmp;
	snprintf(tmp, sizeof(tmp), "            pixelsPerXUnit: %e\n", m_pixelsPerXUnit);
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

	//TODO: statistics

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

void WaveformGroup::OnMeasurementButtonPressEvent(GdkEventButton* event)
{
	//Right click
	if( (event->type == GDK_BUTTON_PRESS) && (event->button == 3) )
	{
		Gtk::TreeModel::Path ignored;
		Gtk::TreeViewColumn* col;
		int cell_x;
		int cell_y;

		if(!m_measurementView.get_path_at_pos(event->x, event->y, ignored, col, cell_x, cell_y))
			return;

		//Get the filter we clicked on
		auto cols = m_measurementView.get_columns();
		for(size_t i=0; i<cols.size(); i++)
		{
			if(cols[i] == col)
			{
				//Column 0 is the stat names
				if(col == 0)
					return;

				//See if we have a valid channel at this column
				if(m_indexToColumnMap.find(i) == m_indexToColumnMap.end())
					return;
				m_measurementContextMenuChannel = m_indexToColumnMap[i];
				break;
			}
		}
		if(!m_measurementContextMenuChannel)
			return;

		//Show the context menu
		m_contextMenu.popup(event->button, event->time);
	}
}

void WaveformGroup::OnStatisticProperties()
{
	auto oldname = m_measurementContextMenuChannel->GetDisplayName();

	//Show the properties
	if(m_measurementContextMenuChannel->IsPhysicalChannel())
	{
		ChannelPropertiesDialog dialog(m_parent, m_measurementContextMenuChannel);
		if(dialog.run() != Gtk::RESPONSE_OK)
			return;

		dialog.ConfigureChannel();
	}

	else
	{
		auto decode = dynamic_cast<Filter*>(m_measurementContextMenuChannel);
		FilterDialog dialog(m_parent, decode, StreamDescriptor(NULL, 0));
		if(dialog.run() != Gtk::RESPONSE_OK)
			return;

		dialog.ConfigureDecoder();
	}

	if(m_measurementContextMenuChannel->GetDisplayName() != oldname)
		m_parent->OnChannelRenamed(m_measurementContextMenuChannel);

	m_parent->RefreshChannelsMenu();
}

void WaveformGroup::OnHideStatistic()
{
	ToggleOff(m_measurementContextMenuChannel);
}

void WaveformGroup::OnChannelRenamed(OscilloscopeChannel* chan)
{
	if(IsShowingStats(chan))
		m_measurementView.get_column(m_columnToIndexMap[chan])->set_title(chan->GetDisplayName());
}
