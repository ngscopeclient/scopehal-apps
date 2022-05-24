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
	@brief Implementation of FilterDialog
 */
#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "FilterDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ParameterRowBase

ParameterRowBase::ParameterRowBase(Gtk::Dialog* parent, FilterParameter& param, FlowGraphNode* node)
	: m_parent(parent)
	, m_node(node)
	, m_param(param)
	, m_ignoreEvents(false)
{
}

ParameterRowBase::~ParameterRowBase()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ParameterRowString

ParameterRowString::ParameterRowString(Gtk::Dialog* parent, FilterParameter& param, FlowGraphNode* node)
	: ParameterRowBase(parent, param, node)
{
	m_entry.set_size_request(500, 1);

	if(!param.IsReadOnly())
		m_connection = m_entry.signal_changed().connect(sigc::mem_fun(*this, &ParameterRowString::OnTextChanged));
	else
		m_connection = param.signal_changed().connect(sigc::mem_fun(*this, &ParameterRowString::OnValueChanged));
}

ParameterRowString::~ParameterRowString()
{
	m_connection.disconnect();
}

void ParameterRowString::OnTextChanged()
{
	if(m_ignoreEvents)
		return;
	if(m_param.IsReadOnly())
		return;

	//When typing over a value, the text is momentarily set to the empty string.
	//We don't want to trigger updates on that.
	if(m_entry.get_text() == "")
		return;

	m_param.ParseString(m_entry.get_text());
}

void ParameterRowString::OnValueChanged()
{
	m_entry.set_text(m_param.ToString());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ParameterRowEnum

ParameterRowEnum::ParameterRowEnum(Gtk::Dialog* parent, FilterParameter& param, FlowGraphNode* node)
	: ParameterRowBase(parent, param, node)
{
	m_box.set_size_request(500, 1);
	m_box.signal_changed().connect(sigc::mem_fun(*this, &ParameterRowEnum::OnChanged));

	if(!param.IsReadOnly())
		m_connection = m_param.signal_enums_changed().connect(sigc::mem_fun(*this, &ParameterRowEnum::Refresh));
}

ParameterRowEnum::~ParameterRowEnum()
{
	//Need to disconnect signal handler since the parameter is very likely to outlive the row
	//and we don't want to call handlers on deleted rows
	m_connection.disconnect();
}

void ParameterRowEnum::OnChanged()
{
	if(m_ignoreEvents)
		return;
	if(m_param.IsReadOnly())
		return;

	m_param.ParseString(m_box.get_active_text());
}

void ParameterRowEnum::Refresh()
{
	m_ignoreEvents = true;

	//Populate box
	m_box.remove_all();
	vector<string> names;
	m_param.GetEnumValues(names);
	for(auto ename : names)
		m_box.append(ename);

	//Set initial value
	m_box.set_active_text(m_param.ToString());

	m_ignoreEvents = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ParameterRowFilename

ParameterRowFilename::ParameterRowFilename(Gtk::Dialog* parent, FilterParameter& param, FlowGraphNode* node)
	: ParameterRowString(parent, param, node)
{
	m_clearButton.set_image_from_icon_name("edit-clear");
	m_clearButton.signal_clicked().connect(sigc::mem_fun(*this, &ParameterRowFilename::OnClear));

	m_browserButton.set_image_from_icon_name("filefind");
	m_browserButton.signal_clicked().connect(sigc::mem_fun(*this, &ParameterRowFilename::OnBrowser));
}

ParameterRowFilename::~ParameterRowFilename()
{
}

void ParameterRowFilename::OnClear()
{
	m_entry.set_text("");
	m_param.ParseString("");
}

void ParameterRowFilename::OnBrowser()
{
	Gtk::FileChooserDialog dlg(
		*m_parent,
		m_param.m_fileIsOutput ? "Save" : "Open",
		m_param.m_fileIsOutput ? Gtk::FILE_CHOOSER_ACTION_SAVE : Gtk::FILE_CHOOSER_ACTION_OPEN);
	dlg.set_filename(m_entry.get_text());

	auto filter = Gtk::FileFilter::create();
	filter->add_pattern(m_param.m_fileFilterMask);
	filter->set_name(m_param.m_fileFilterName);
	dlg.add_filter(filter);
	dlg.add_button("Open", Gtk::RESPONSE_OK);
	dlg.add_button("Cancel", Gtk::RESPONSE_CANCEL);
	if(m_param.m_fileIsOutput)
		dlg.set_do_overwrite_confirmation();;
	auto response = dlg.run();

	if(response != Gtk::RESPONSE_OK)
		return;

	auto str = dlg.get_filename();
	m_entry.set_text(str);
	m_param.ParseString(str);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

FilterDialog::FilterDialog(
	OscilloscopeWindow* parent,
	Filter* filter,
	StreamDescriptor chan)
	: Gtk::Dialog(filter->GetProtocolDisplayName(), *parent, Gtk::DIALOG_MODAL)
	, m_filter(filter)
	, m_parent(parent)
	, m_refreshing(false)
{
	m_cachedStreamCount = m_filter->GetStreamCount();

	get_vbox()->pack_start(m_grid, Gtk::PACK_EXPAND_WIDGET);
		m_grid.attach(m_channelDisplayNameLabel, 0, 0, 1, 1);
			m_channelDisplayNameLabel.set_text("Display name");
		m_grid.attach_next_to(m_channelDisplayNameEntry, m_channelDisplayNameLabel, Gtk::POS_RIGHT, 1, 1);
			m_channelDisplayNameLabel.set_halign(Gtk::ALIGN_START);
			m_channelDisplayNameEntry.set_text(filter->GetDisplayName());

		m_grid.attach_next_to(m_channelColorLabel, m_channelDisplayNameLabel, Gtk::POS_BOTTOM, 1, 1);
			m_channelColorLabel.set_text("Waveform color");
			m_channelColorLabel.set_halign(Gtk::ALIGN_START);
		m_grid.attach_next_to(m_channelColorButton, m_channelColorLabel, Gtk::POS_RIGHT, 1, 1);
			m_channelColorButton.set_color(Gdk::Color(filter->m_displaycolor));

	size_t nrow = 2;
	for(size_t i=0; i<filter->GetInputCount(); i++)
	{
		//Add the row
		auto row = new ChannelSelectorRow;
		m_grid.attach(row->m_label, 0, nrow, 1, 1);
			row->m_label.set_label(filter->GetInputName(i));
		m_grid.attach(row->m_chans, 1, nrow, 1, 1);
			PopulateInputBox(parent, filter, row, i, chan);
		m_rows.push_back(row);
		nrow ++;

		row->m_chans.signal_changed().connect(sigc::mem_fun(this, &FilterDialog::OnInputChanged));
	}

	//Add parameters
	for(auto it = filter->GetParamBegin(); it != filter->GetParamEnd(); it ++)
	{
		if(it->second.IsHidden())
			continue;

		m_prows[it->first] = CreateRow(m_grid, it->first, it->second, nrow, this, filter);
		nrow ++;

		//Make signal connections for parameters changing
		if(!it->second.IsReadOnly())
		{
			m_paramConnections.push_back(it->second.signal_changed().connect(
				sigc::mem_fun(*this, &FilterDialog::OnParameterChanged)));
		}
	}

	//Add event handlers
	m_paramConnection = m_filter->signal_parametersChanged().connect(
		sigc::mem_fun(this, &FilterDialog::OnRefresh));
	m_inputConnection = m_filter->signal_inputsChanged().connect(
		sigc::mem_fun(this, &FilterDialog::OnRefresh));

	//Execute initial input changes (connecting the default input to filters)
	OnInputChanged();

	show_all();
}

FilterDialog::~FilterDialog()
{
	for(auto r : m_rows)
		delete r;
	m_rows.clear();
	for(auto r : m_prows)
		delete r.second;
	m_prows.clear();

	m_paramConnection.disconnect();
	m_inputConnection.disconnect();
}

void FilterDialog::PopulateInputBox(
	OscilloscopeWindow* parent,
	Filter* filter,
	ChannelSelectorRow* row,
	size_t ninput,
	StreamDescriptor chan)
{
	row->m_chans.remove_all();

	//Allow NULL for optional inputs
	auto din = filter->GetInput(ninput);
	if(filter->ValidateChannel(ninput, StreamDescriptor(NULL, 0)))
	{
		row->m_chans.append("NULL");
		row->m_chanptrs["NULL"] = StreamDescriptor(NULL, 0);

		//Handle null inputs
		if(din.m_channel == NULL)
			row->m_chans.set_active_text("NULL");
	}

	//Fill the channel list with all channels that are legal to use here
	//TODO: multiple streams
	for(size_t j=0; j<parent->GetScopeCount(); j++)
	{
		Oscilloscope* scope = parent->GetScope(j);
		for(size_t k=0; k<scope->GetChannelCount(); k++)
		{
			//If we can't enable the channel, don't show it.
			//Aux inputs can't be enabled, but show those if they are legal
			auto cn = scope->GetChannel(k);
			if( !scope->CanEnableChannel(k) && (cn->GetType() != OscilloscopeChannel::CHANNEL_TYPE_TRIGGER) )
				continue;

			auto nstreams = cn->GetStreamCount();
			for(size_t m=0; m<nstreams; m++)
			{
				auto desc = StreamDescriptor(cn, m);
				if(filter->ValidateChannel(ninput, desc))
				{
					auto name = desc.GetName();
					row->m_chans.append(name);
					row->m_chanptrs[name] = desc;
					if( ( (desc == chan) && (ninput == 0) ) || (desc == din) )
						row->m_chans.set_active_text(name);
				}
			}
		}
	}

	//Add filters
	auto filters = Filter::GetAllInstances();
	for(auto d : filters)
	{
		//Don't allow circular dependencies
		if(d == filter)
			continue;

		auto nstreams = d->GetStreamCount();
		for(size_t j=0; j<nstreams; j++)
		{
			auto desc = StreamDescriptor(d, j);
			if(filter->ValidateChannel(ninput, desc))
			{
				string name = desc.GetName();

				row->m_chans.append(name);
				row->m_chanptrs[name] = desc;
				if( ( (desc == chan) &&  (ninput == 0) ) || (desc == din) )
					row->m_chans.set_active_text(name);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI glue

/**
	@brief Adds a row to the dialog for a given parameter

	TODO: move this to common base class or something?
 */
ParameterRowBase* FilterDialog::CreateRow(
	Gtk::Grid& grid,
	string name,
	FilterParameter& param,
	size_t y,
	Gtk::Dialog* parent,
	FlowGraphNode* node)
{
	int width = 100;

	switch(param.GetType())
	{
		case FilterParameter::TYPE_FILENAME:
			{
				auto row = new ParameterRowFilename(parent, param, node);
				grid.attach(row->m_label, 0, y, 1, 1);
					row->m_label.set_size_request(width, 1);
					row->m_label.set_label(name);
				grid.attach(row->m_contentbox, 1, y, 1, 1);
					row->m_contentbox.attach(row->m_entry, 0, 0, 1, 1);
					row->m_contentbox.attach(row->m_clearButton, 1, 0, 1, 1);
					row->m_contentbox.attach(row->m_browserButton, 2, 0, 1, 1);

				//Set initial value
				row->m_ignoreEvents = true;
				row->m_entry.set_text(param.ToString());
				row->m_ignoreEvents = false;

				return row;
			}

		case FilterParameter::TYPE_ENUM:
			{
				auto row = new ParameterRowEnum(parent, param, node);
				grid.attach(row->m_label, 0, y, 1, 1);
					row->m_label.set_size_request(width, 1);
					row->m_label.set_label(name);
				grid.attach(row->m_contentbox, 1, y, 1, 1);
					row->m_contentbox.attach(row->m_box, 0, 0, 1, 1);
				row->Refresh();

				if(param.IsReadOnly())
					row->m_contentbox.set_sensitive(false);
				return row;
			}

		default:
			{
				auto row = new ParameterRowString(parent, param, node);
				grid.attach(row->m_label, 0, y, 1, 1);
					row->m_label.set_size_request(width, 1);
					row->m_label.set_label(name);
				grid.attach(row->m_contentbox, 1, y, 1, 1);
					row->m_contentbox.attach(row->m_entry, 0, 0, 1, 1);

				if(param.IsReadOnly())
					row->m_contentbox.set_sensitive(false);

				row->m_label.set_label(name);

				//Set initial value
				row->m_ignoreEvents = true;
				row->m_entry.set_text(param.ToString());
				row->m_ignoreEvents = false;

				return row;
			}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Output

void FilterDialog::ConfigureDecoder()
{
	//See if we're using the default name
	string old_name = m_filter->GetDisplayName();

	m_filter->m_displaycolor = m_channelColorButton.get_color().to_string();

	//Set the name of the decoder based on the input channels etc.
	m_filter->SetDefaultName();
	auto dname = m_channelDisplayNameEntry.get_text();

	//If old name was default, and we didn't change it, update.
	if(m_filter->IsUsingDefaultName() && (dname == old_name) )
		m_filter->UseDefaultName(true);

	//If new name matches the default, we're now autogenerated again
	else if(m_filter->GetDisplayName() == dname)
		m_filter->UseDefaultName(true);

	//If no name was specified, revert to the default
	else if(dname == "")
		m_filter->UseDefaultName(true);

	//Otherwise use whatever the user specified
	else
	{
		m_filter->SetDisplayName(dname);
		m_filter->UseDefaultName(false);
	}
}

void FilterDialog::ConfigureInputs(FlowGraphNode* node, vector<ChannelSelectorRow*>& rows)
{
	//Hook up input(s)
	for(size_t i=0; i<rows.size(); i++)
	{
		auto chname = rows[i]->m_chans.get_active_text();
		node->SetInput(i, rows[i]->m_chanptrs[chname]);
	}
}

void FilterDialog::ConfigureParameters(FlowGraphNode* node, std::map<string, ParameterRowBase*>& rows)
{
	for(auto it : rows)
	{
		auto row = it.second;
		auto srow = dynamic_cast<ParameterRowString*>(row);
		auto erow = dynamic_cast<ParameterRowEnum*>(row);
		auto name = it.first;

		//Strings are easy
		if(srow)
			node->GetParameter(name).ParseString(srow->m_entry.get_text());

		//Enums
		else if(erow)
			node->GetParameter(name).ParseString(erow->m_box.get_active_text());
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

void FilterDialog::OnRefresh()
{
	//ignore nested events triggered by refresh
	if(m_refreshing)
		return;

	m_refreshing = true;

	//Remove all parameters from the table before refreshing inputs, since things are going to move around
	for(auto it : m_prows)
	{
		m_grid.remove(it.second->m_label);
		m_grid.remove(it.second->m_contentbox);
	}

	OnRefreshInputs();
	OnRefreshParameters();

	m_grid.show_all();

	m_refreshing = false;
}

void FilterDialog::OnRefreshInputs()
{
	//Remove unused inputs
	size_t ncount = m_filter->GetInputCount();
	size_t ocount = m_rows.size();
	for(size_t i=ncount; i<ocount; i++)
	{
		m_grid.remove(m_rows[i]->m_label);
		m_grid.remove(m_rows[i]->m_chans);
		delete m_rows[i];
	}

	//Create new inputs
	m_rows.resize(ncount);
	size_t irow = ocount + 2;
	for(size_t i=ocount; i<ncount; i++)
	{
		m_rows[i] = new ChannelSelectorRow;
		m_rows[i]->m_label.set_label(m_filter->GetInputName(i));

		m_grid.attach(m_rows[i]->m_label, 0, irow, 1, 1);
		m_grid.attach_next_to(m_rows[i]->m_chans, m_rows[i]->m_label, Gtk::POS_RIGHT, 1, 1);
		irow ++;

		PopulateInputBox(m_parent, m_filter, m_rows[i], i, StreamDescriptor(NULL, 0));
		m_rows[i]->m_chans.set_active_text(m_filter->GetInput(i).GetName());
	}
}

void FilterDialog::OnRefreshParameters()
{
	//Remove old signal connections
	for(auto c : m_paramConnections)
		c.disconnect();
	m_paramConnections.clear();

	//Remove any parameters we have rows for that no longer exist
	vector<string> paramsToRemove;
	for(auto it : m_prows)
	{
		auto name = it.first;
		if(!m_filter->HasParameter(name))
			paramsToRemove.push_back(name);
	}
	for(auto p : paramsToRemove)
	{
		delete m_prows[p];
		m_prows.erase(p);
	}

	//Re-add existing parameters
	size_t nrow = 2 + m_filter->GetInputCount();
	for(auto it : m_prows)
	{
		m_grid.attach(it.second->m_label, 0, nrow, 1, 1);
		m_grid.attach(it.second->m_contentbox, 1, nrow, 1, 1);
		nrow ++;
	}

	//Add new parameters if needed (at the end)
	for(auto it = m_filter->GetParamBegin(); it != m_filter->GetParamEnd(); it ++)
	{
		//Do we already have an entry for this one?
		auto name = it->first;
		if(m_prows.find(name) != m_prows.end())
			continue;

		//Skip hidden ones
		if(it->second.IsHidden())
			continue;

		m_prows[name] = CreateRow(m_grid, name, it->second, nrow, this, m_filter);
		nrow ++;

		//Make new signal connections for parameters changing
		if(!it->second.IsReadOnly())
		{
			m_paramConnections.push_back(it->second.signal_changed().connect(
				sigc::mem_fun(*this, &FilterDialog::OnParameterChanged)));
		}
	}
}

void FilterDialog::OnInputChanged()
{
	ConfigureInputs(m_filter, m_rows);
	m_parent->RefreshAllFilters();
	m_parent->ClearAllPersistence();
}

void FilterDialog::OnParameterChanged()
{
	//TODO: Update the filter name?

	//Re-run the filter graph
	m_parent->RefreshAllFilters();

	//Did the number of output streams change since the filter was created?
	int streamcount = m_filter->GetStreamCount();
	if(m_cachedStreamCount != streamcount)
	{
		m_parent->OnStreamCountChanged(m_filter);
		m_cachedStreamCount = streamcount;
	}

	//Redraw everything and clear persistence
	m_parent->ClearAllPersistence();
}
