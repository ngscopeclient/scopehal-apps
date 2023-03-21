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
	@brief Implementation of WaveformGroup
 */
#include "glscopeclient.h"
#include "WaveformGroup.h"
#include "ChannelPropertiesDialog.h"
#include "WaveformGroupPropertiesDialog.h"
#include "FilterDialog.h"
#include <cinttypes>

using namespace std;

int WaveformGroup::m_numGroups = 1;

WaveformGroup::WaveformGroup(OscilloscopeWindow* parent)
	: m_timeline(parent, this)
	, m_pixelsPerXUnit(0.00005)
	, m_xAxisOffset(0)
	, m_cursorConfig(CURSOR_NONE)
	, m_parent(parent)
	, m_propertiesDialog(NULL)
	, m_measurementContextMenuChannel(NULL)
{
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Initial GUI hierarchy, title, etc

	//TODO: preference for waveform area icon size
	int size = 16;
	string testfname = "dialog-close.png";
	string base_path = FindDataFile("icons/" + to_string(size) + "x" + to_string(size) + "/" + testfname);
	base_path = base_path.substr(0, base_path.length() - testfname.length());

	m_frame.add(m_realframe);
		m_realframe.set_label_widget(m_framelabelbox);
			m_framelabelbox.pack_start(m_framelabel, Gtk::PACK_SHRINK);
			m_framelabelbox.pack_start(m_closebutton, Gtk::PACK_SHRINK);
				m_closebutton.set_image(*Gtk::manage(new Gtk::Image(base_path + "dialog-close.png")));
				m_closebutton.set_relief(Gtk::RELIEF_NONE);
				m_closebutton.signal_clicked().connect(sigc::mem_fun(*this, &WaveformGroup::OnCloseRequest));
		m_realframe.add(m_vbox);
			m_vbox.pack_start(m_timeline, Gtk::PACK_SHRINK);
			m_vbox.pack_start(m_waveformBox, Gtk::PACK_EXPAND_WIDGET);

	char tmp[64];
	snprintf(tmp, sizeof(tmp), "Waveform Group %d", m_numGroups);
	m_numGroups ++;
	m_framelabel.set_label(tmp);

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

	m_frame.add_events(Gdk::BUTTON_PRESS_MASK);
	m_frame.signal_button_press_event().connect_notify(
		sigc::mem_fun(*this, &WaveformGroup::OnTitleButtonPressEvent));

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
	if(m_propertiesDialog)
		delete m_propertiesDialog;

	//Free each of our channels
	for(size_t i=1; i<32; i++)
	{
		if(m_indexToColumnMap.find(i) != m_indexToColumnMap.end())
			DisableStats(m_indexToColumnMap[i]);
	}

	auto children = m_treeModel->children();
	for(auto row : children)
	{
		Statistic* stat = row[m_treeColumns.m_statColumn];
		delete stat;
	}
}

void WaveformGroup::OnCloseRequest()
{
	//If we have no children, there's nothing to do - we're a solo, empty group
	//TODO: maybe hide close button in this scenario?
	auto children = m_waveformBox.get_children();
	if(children.empty())
		return;

	//Confirm
	auto parent = GetParent();
	Gtk::MessageDialog dlg(
		*parent,
		string("Delete waveform group \"") + m_framelabel.get_label() + "\"?",
		false,
		Gtk::MESSAGE_QUESTION,
		Gtk::BUTTONS_YES_NO,
		true);
	if(dlg.run() != Gtk::RESPONSE_YES)
		return;

	//Close each child waveform in sequence
	for(size_t i=0; i<children.size(); i++)
	{
		auto w = dynamic_cast<WaveformArea*>(children[i]);
		if(w)
			parent->OnRemoveChannel(w);
	}

	//NOTE: We cannot call any methods that access member variables after the last OnRemoveChannel() call
	//as the final call will result in this group being empty, and possibly deleted
}

void WaveformGroup::HideInactiveColumns()
{
	//col 0 is stat name, always shown
	for(size_t ncol=1; ncol<31; ncol ++)
	{
		if(m_indexToColumnMap.find(ncol) == m_indexToColumnMap.end())
			m_measurementView.get_column(ncol)->set_visible(false);
		else
			m_measurementView.get_column(ncol)->set_visible();
	}

	//col 31 always shown as padding
	m_measurementView.get_column(31)->set_visible();
}

void WaveformGroup::EnableStats(StreamDescriptor stream, size_t index)
{
	//If the channel is already active, do nothing
	if(m_columnToIndexMap.find(stream) != m_columnToIndexMap.end())
		return;

	//If we have no rows, add the initial set of stats
	if(m_treeModel->children().empty())
	{
		AddStatistic(Statistic::CreateStatistic("Maximum"));
		AddStatistic(Statistic::CreateStatistic("Average"));
		AddStatistic(Statistic::CreateStatistic("Minimum"));
	}

	//Use the first free column if an index wasn't specified
	//(normally only during loading)
	size_t ncol = index;
	if(ncol == 0)
	{
		for(ncol=1; ncol<32; ncol ++)
		{
			if(m_indexToColumnMap.find(ncol) == m_indexToColumnMap.end())
				break;
		}
	}

	m_columnToIndexMap[stream] = ncol;
	m_indexToColumnMap[ncol] = stream;

	//Set up the column
	auto col = m_measurementView.get_column(ncol);
	col->set_title(stream.GetName());
	col->get_first_cell()->property_xalign() = 1.0;
	col->set_alignment(Gtk::ALIGN_END);

	RefreshMeasurements();

	dynamic_cast<OscilloscopeChannel*>(stream.m_channel)->AddRef();

	m_measurementView.show_all();
	HideInactiveColumns();
}

void WaveformGroup::DisableStats(StreamDescriptor stream)
{
	int index = m_columnToIndexMap[stream];

	//Delete the current contents of the channel
	m_measurementView.get_column(index)->set_title("");
	auto children = m_treeModel->children();
	for(auto row : children)
		row[m_treeColumns.m_columns[index]] = "";

	//Remove everything from our column records and free the channel
	m_columnToIndexMap.erase(stream);
	m_indexToColumnMap.erase(index);
	dynamic_cast<OscilloscopeChannel*>(stream.m_channel)->Release();

	HideInactiveColumns();

	//If no channels are visible hide the frame
	if(m_columnToIndexMap.empty())
		m_measurementView.hide();
}

bool WaveformGroup::IsShowingStats(StreamDescriptor stream)
{
	return (m_columnToIndexMap.find(stream) != m_columnToIndexMap.end());
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

			//Make sure the inputs of the block are on the CPU
			auto data = chan.GetData();
			if(data == nullptr)
				continue;
			data->PrepareForCpuAccess();

			//Evaluate the statistic
			double value;
			if(!stat->Calculate(chan, value))
				row[m_treeColumns.m_columns[i]] = "(error)";
			else
				row[m_treeColumns.m_columns[i]] = chan.GetYAxisUnits().PrettyPrint(value);
		}
	}

	//Update column titles in case a channel got renamed
	for(size_t i=1; i<32; i++)
	{
		if(m_indexToColumnMap.find(i) == m_indexToColumnMap.end())
			continue;

		auto col = m_measurementView.get_column(i);
		auto name = m_indexToColumnMap[i].GetName();

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

YAML::Node WaveformGroup::SerializeConfiguration(IDTable& table)
{
	YAML::Node node;
	YAML::Node groupNode;

	int id = table.emplace(&m_frame);

	groupNode["id"] = id;
	groupNode["name"] = string(m_framelabel.get_label());
	groupNode["timebaseResolution"] = "fs";
	groupNode["pixelsPerXUnit"] = m_pixelsPerXUnit;
	groupNode["xAxisOffset"] = m_xAxisOffset;

	switch(m_cursorConfig)
	{
		case WaveformGroup::CURSOR_NONE:
			groupNode["cursorConfig"] = "none";
			break;

		case WaveformGroup::CURSOR_X_SINGLE:
			groupNode["cursorConfig"] = "x_single";
			break;

		case WaveformGroup::CURSOR_Y_SINGLE:
			groupNode["cursorConfig"] = "y_single";
			break;

		case WaveformGroup::CURSOR_X_DUAL:
			groupNode["cursorConfig"] = "x_dual";
			break;

		case WaveformGroup::CURSOR_Y_DUAL:
			groupNode["cursorConfig"] = "y_dual";
			break;
	}

	groupNode["xcursor0"] = m_xCursorPos[0];
	groupNode["xcursor1"] = m_xCursorPos[1];
	groupNode["ycursor0"] = m_yCursorPos[0];
	groupNode["ycursor1"] = m_yCursorPos[1];

	//Waveform areas
	auto children = m_waveformBox.get_children();
	for(size_t i=0; i<children.size(); i++)
	{
		int aid = table[children[i]];
		groupNode["areas"]["area" + to_string(aid)]["id"] = aid;
	}

	//Statistics
	for(auto it : m_indexToColumnMap)
	{
		YAML::Node statNode;
		statNode["index"] = it.first;
		statNode["channel"] = table[it.second.m_channel];
		statNode["stream"] = it.second.m_stream;
		groupNode["stats"]["stat" + to_string(it.first)] = statNode;
	}

	node["group" + to_string(id)] = groupNode;

	return node;
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

void WaveformGroup::OnTitleButtonPressEvent(GdkEventButton* event)
{
	if(event->type == GDK_2BUTTON_PRESS)
	{
		m_propertiesDialog = new WaveformGroupPropertiesDialog(m_parent, this);
		m_propertiesDialog->signal_response().connect(
			sigc::mem_fun(*this, &WaveformGroup::OnPropertiesDialogResponse));
		m_propertiesDialog->show();
	}
}

void WaveformGroup::OnPropertiesDialogResponse(int response)
{
	if(response == Gtk::RESPONSE_OK)
		m_propertiesDialog->ConfigureGroup();

	delete m_propertiesDialog;
	m_propertiesDialog = NULL;
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
				m_measurementContextMenuChannel = m_indexToColumnMap[i].m_channel;
				break;
			}
		}
		if(!m_measurementContextMenuChannel.m_channel)
			return;

		//Show the context menu
		m_contextMenu.popup(event->button, event->time);
	}
}

void WaveformGroup::OnStatisticProperties()
{
	auto oldname = m_measurementContextMenuChannel.GetName();
	auto ochan = dynamic_cast<OscilloscopeChannel*>(m_measurementContextMenuChannel.m_channel);

	//Show the properties
	if(ochan->IsPhysicalChannel())
	{
		ChannelPropertiesDialog dialog(m_parent, ochan);
		if(dialog.run() != Gtk::RESPONSE_OK)
			return;

		dialog.ConfigureChannel();
	}

	else
	{
		auto decode = dynamic_cast<Filter*>(m_measurementContextMenuChannel.m_channel);
		FilterDialog dialog(m_parent, decode, StreamDescriptor(NULL, 0));
		dialog.run();
		dialog.ConfigureDecoder();
	}

	if(m_measurementContextMenuChannel.GetName() != oldname)
		m_parent->OnChannelRenamed(ochan);

	m_parent->RefreshChannelsMenu();
}

void WaveformGroup::OnHideStatistic()
{
	DisableStats(m_measurementContextMenuChannel);
}

void WaveformGroup::OnChannelRenamed(StreamDescriptor stream)
{
	if(IsShowingStats(stream))
		m_measurementView.get_column(m_columnToIndexMap[stream])->set_title(stream.GetName());
}

Unit WaveformGroup::GetXAxisUnits()
{
	auto children = m_waveformBox.get_children();
	if(!children.empty())
	{
		auto view = dynamic_cast<WaveformArea*>(children[0]);
		if(view != NULL)
			return view->GetChannel().GetXAxisUnits();
	}
	return Unit(Unit::UNIT_FS);
}

/**
	@brief Returns a pointer to the first WaveformArea in the group
 */
WaveformArea* WaveformGroup::GetFirstArea()
{
	auto children = m_waveformBox.get_children();
	if(!children.empty())
	{
		auto view = dynamic_cast<WaveformArea*>(children[0]);
		if(view != nullptr)
			return view;
	}

	return nullptr;
}

/**
	@brief Returns the stream being displayed by the first channel of the first WaveformArea in the group
 */
StreamDescriptor WaveformGroup::GetFirstChannel()
{
	auto children = m_waveformBox.get_children();
	if(!children.empty())
	{
		auto view = dynamic_cast<WaveformArea*>(children[0]);
		if(view != nullptr)
			return view->GetChannel();
	}

	return StreamDescriptor(nullptr, 0);
}
