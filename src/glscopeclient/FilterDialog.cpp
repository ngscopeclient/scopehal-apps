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
	@brief Implementation of FilterDialog
 */
#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "FilterDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ParameterRowBase

ParameterRowBase::ParameterRowBase()
{
}

ParameterRowBase::~ParameterRowBase()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ParameterRowString

ParameterRowString::ParameterRowString()
{
	m_entry.set_size_request(500, 1);
}

ParameterRowString::~ParameterRowString()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ParameterRowEnum

ParameterRowEnum::ParameterRowEnum()
{
	m_box.set_size_request(500, 1);
}

ParameterRowEnum::~ParameterRowEnum()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ParameterRowFilename

ParameterRowFilename::ParameterRowFilename(Gtk::Dialog* parent, FilterParameter& param)
	: m_parent(parent)
	, m_param(param)
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

	m_entry.set_text(dlg.get_filename());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ParameterRowFilenames

ParameterRowFilenames::ParameterRowFilenames(Gtk::Dialog* parent, FilterParameter& param)
	: m_parent(parent)
	, m_list(1)
	, m_param(param)
{
	m_list.set_size_request(500, 200);
	m_list.set_column_title(0, "Filename");

	m_buttonAdd.set_label("+");
	m_buttonRemove.set_label("-");

	m_buttonAdd.signal_clicked().connect(sigc::mem_fun(*this, &ParameterRowFilenames::OnAdd));
	m_buttonRemove.signal_clicked().connect(sigc::mem_fun(*this, &ParameterRowFilenames::OnRemove));
}

ParameterRowFilenames::~ParameterRowFilenames()
{
}

void ParameterRowFilenames::OnAdd()
{
	Gtk::FileChooserDialog dlg(*m_parent, "Open", Gtk::FILE_CHOOSER_ACTION_OPEN);

	auto filter = Gtk::FileFilter::create();
	filter->add_pattern(m_param.m_fileFilterMask);
	filter->set_name(m_param.m_fileFilterName);
	dlg.add_filter(filter);
	dlg.add_button("Open", Gtk::RESPONSE_OK);
	dlg.add_button("Cancel", Gtk::RESPONSE_CANCEL);
	auto response = dlg.run();

	if(response != Gtk::RESPONSE_OK)
		return;

	m_list.append(dlg.get_filename());
}

void ParameterRowFilenames::OnRemove()
{
	auto store = Glib::RefPtr<Gtk::ListStore>::cast_dynamic(m_list.get_model());
	store->erase(m_list.get_selection()->get_selected());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

FilterDialog::FilterDialog(
	OscilloscopeWindow* parent,
	Filter* filter,
	StreamDescriptor chan)
	: Gtk::Dialog(filter->GetProtocolDisplayName(), *parent, Gtk::DIALOG_MODAL)
	, m_filter(filter)
{
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

	Gtk::Widget* last_label = &m_channelColorLabel;
	for(size_t i=0; i<filter->GetInputCount(); i++)
	{
		//Add the row
		auto row = new ChannelSelectorRow;
		m_grid.attach_next_to(row->m_label, *last_label, Gtk::POS_BOTTOM, 1, 1);
		m_grid.attach_next_to(row->m_chans, row->m_label, Gtk::POS_RIGHT, 1, 1);
		m_rows.push_back(row);
		last_label = &row->m_label;

		//Label is just the channel name
		row->m_label.set_label(filter->GetInputName(i));

		//Allow NULL for optional inputs
		auto din = filter->GetInput(i);
		if(filter->ValidateChannel(i, StreamDescriptor(NULL, 0)))
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
					if(filter->ValidateChannel(i, desc))
					{
						auto name = desc.GetName();
						row->m_chans.append(name);
						row->m_chanptrs[name] = desc;
						if( (desc == chan && i==0) || (desc == din) )
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
				if(filter->ValidateChannel(i, desc))
				{
					string name = desc.GetName();

					row->m_chans.append(name);
					row->m_chanptrs[name] = desc;
					if( (desc == chan && i==0) || (desc == din) )
						row->m_chans.set_active_text(name);
				}
			}
		}
	}

	//Add parameters
	for(auto it = filter->GetParamBegin(); it != filter->GetParamEnd(); it ++)
		m_prows.push_back(CreateRow(m_grid, it->first, it->second, last_label, this));
	show_all();
}

FilterDialog::~FilterDialog()
{
	for(auto r : m_rows)
		delete r;
	m_rows.clear();
	for(auto r : m_prows)
		delete r;
	m_prows.clear();
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
	Gtk::Widget*& last_label,
	Gtk::Dialog* parent)
{
	int width = 100;

	switch(param.GetType())
	{
		case FilterParameter::TYPE_FILENAME:
			{
				auto row = new ParameterRowFilename(parent, param);
				if(!last_label)
					grid.attach(row->m_label, 0, 0, 1, 1);
				else
					grid.attach_next_to(row->m_label, *last_label, Gtk::POS_BOTTOM, 1, 1);
				row->m_label.set_size_request(width, 1);

				grid.attach_next_to(row->m_entry, row->m_label, Gtk::POS_RIGHT, 1, 1);
				grid.attach_next_to(row->m_clearButton, row->m_entry, Gtk::POS_RIGHT, 1, 1);
				grid.attach_next_to(row->m_browserButton, row->m_clearButton, Gtk::POS_RIGHT, 1, 1);
				last_label = &row->m_label;

				row->m_label.set_label(name);

				//Set initial value
				row->m_entry.set_text(param.ToString());

				return row;
			}

		case FilterParameter::TYPE_FILENAMES:
			{
				auto row = new ParameterRowFilenames(parent, param);
				if(!last_label)
					grid.attach(row->m_label, 0, 0, 1, 1);
				else
					grid.attach_next_to(row->m_label, *last_label, Gtk::POS_BOTTOM, 1, 1);
				row->m_label.set_size_request(width, 1);

				grid.attach_next_to(row->m_list, row->m_label, Gtk::POS_RIGHT, 1, 2);
				grid.attach_next_to(row->m_buttonAdd, row->m_list, Gtk::POS_RIGHT, 1, 1);
				grid.attach_next_to(row->m_buttonRemove, row->m_buttonAdd, Gtk::POS_BOTTOM, 1, 1);
				last_label = &row->m_label;

				row->m_label.set_label(name);

				//Set initial value
				auto files = param.GetFileNames();
				for(auto f : files)
					row->m_list.append(f);

				return row;
			}

		case FilterParameter::TYPE_ENUM:
			{
				auto row = new ParameterRowEnum;
				if(!last_label)
					grid.attach(row->m_label, 0, 0, 1, 1);
				else
					grid.attach_next_to(row->m_label, *last_label, Gtk::POS_BOTTOM, 1, 1);
				row->m_label.set_size_request(width, 1);

				grid.attach_next_to(row->m_box, row->m_label, Gtk::POS_RIGHT, 1, 1);
				last_label = &row->m_label;

				row->m_label.set_label(name);

				//Populate box
				vector<string> names;
				param.GetEnumValues(names);
				for(auto ename : names)
					row->m_box.append(ename);

				//Set initial value
				row->m_box.set_active_text(param.ToString());

				return row;
			}

		default:
			{
				auto row = new ParameterRowString;
				if(!last_label)
					grid.attach(row->m_label, 0, 0, 1, 1);
				else
					grid.attach_next_to(row->m_label, *last_label, Gtk::POS_BOTTOM, 1, 1);
				row->m_label.set_size_request(width, 1);

				grid.attach_next_to(row->m_entry, row->m_label, Gtk::POS_RIGHT, 1, 1);
				last_label = &row->m_label;

				row->m_label.set_label(name);

				//Set initial value
				row->m_entry.set_text(param.ToString());

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

	ConfigureInputs(m_filter, m_rows);
	ConfigureParameters(m_filter, m_prows);

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

void FilterDialog::ConfigureParameters(FlowGraphNode* node, vector<ParameterRowBase*>& rows)
{
	for(auto row : rows)
	{
		auto srow = dynamic_cast<ParameterRowString*>(row);
		auto erow = dynamic_cast<ParameterRowEnum*>(row);
		auto frow = dynamic_cast<ParameterRowFilenames*>(row);
		auto name = row->m_label.get_label();

		//Strings are easy
		if(srow)
			node->GetParameter(name).ParseString(srow->m_entry.get_text());

		//Enums
		else if(erow)
			node->GetParameter(name).ParseString(erow->m_box.get_active_text());

		//List of file names
		else if(frow)
		{
			vector<string> paths;
			for(size_t j=0; j<frow->m_list.size(); j++)
				paths.push_back(frow->m_list.get_text(j));
			node->GetParameter(name).SetFileNames(paths);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers
