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
	, m_ignoreUpdates(false)
{
	m_contentbox.set_hexpand(true);
	m_contentbox.set_vexpand(true);
}

ParameterRowBase::~ParameterRowBase()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ParameterBlock8B10BSymbol

ParameterBlock8B10BSymbol::ParameterBlock8B10BSymbol()
{
	m_frame.add(m_grid);
		m_grid.set_margin_left(5);
		m_grid.set_margin_right(5);
		m_grid.set_margin_top(5);
		m_grid.set_margin_bottom(5);

		m_grid.attach(m_typeLabel, 0, 0);
			m_typeLabel.set_text("Type");
		m_grid.attach(m_typeBox, 1, 0);
			m_typeBox.append("K symbol");
			m_typeBox.append("D symbol (Dx.y format)");
			m_typeBox.append("D symbol (Hex format)");
			m_typeBox.append("Don't care");
		m_grid.attach(m_disparityLabel, 0, 1);
			m_disparityLabel.set_text("Disparity");
		m_grid.attach(m_disparityBox, 1, 1);
			m_disparityBox.append("Positive");
			m_disparityBox.append("Negative");
			m_disparityBox.append("Any");
		m_grid.attach(m_symbolLabel, 0, 2);
			m_symbolLabel.set_text("Symbol");
		//m_grid.attach(m_symbolEntry, 1, 2);
		//m_grid.attach(m_symbolBox, 1, 2);

		m_symbolBox.append("K28.0");
		m_symbolBox.append("K28.1");
		m_symbolBox.append("K28.2");
		m_symbolBox.append("K28.3");
		m_symbolBox.append("K28.4");
		m_symbolBox.append("K28.5");
		m_symbolBox.append("K28.6");
		m_symbolBox.append("K28.7");
		m_symbolBox.append("K23.7");
		m_symbolBox.append("K27.7");
		m_symbolBox.append("K29.7");
		m_symbolBox.append("K30.7");

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ParameterRow8B10BPattern

ParameterRow8B10BPattern::ParameterRow8B10BPattern(Gtk::Dialog* parent, FilterParameter& param, FlowGraphNode* node)
	: ParameterRowBase(parent, param, node)
{
	m_connection = param.signal_changed().connect(sigc::mem_fun(*this, &ParameterRow8B10BPattern::OnPatternChanged));
}

ParameterRow8B10BPattern::~ParameterRow8B10BPattern()
{
	m_connection.disconnect();
}

void ParameterRow8B10BPattern::OnKValueChanged(size_t i)
{
	if(m_ignoreEvents)
		return;

	int code5;
	int code3;
	auto str = m_blocks[i].m_symbolBox.get_active_text();
	if(2 != sscanf(str.c_str(), "K%d.%d", &code5, &code3))
		return;

	m_ignoreUpdates = true;
	auto params = m_param.Get8B10BPattern();
	params[i].value = (code3 << 5) | code5;
	m_param.Set8B10BPattern(params);
	m_ignoreUpdates = false;
}

void ParameterRow8B10BPattern::OnDValueChanged(size_t i)
{
	if(m_ignoreEvents)
		return;

	auto str = m_blocks[i].m_symbolEntry.get_text();

	auto params = m_param.Get8B10BPattern();

	if(str[0] == 'D')
	{
		int code5;
		int code3;
		if(2 != sscanf(str.c_str(), "D%d.%d", &code5, &code3))
			return;
		params[i].value = (code3 << 5) | code5;
	}

	else
	{
		unsigned int tmp;
		if(1 != sscanf(str.c_str(), "0x%x", &tmp))
			return;
		params[i].value = tmp;
	}

	m_ignoreUpdates = true;
	m_param.Set8B10BPattern(params);
	m_ignoreUpdates = false;
}

void ParameterRow8B10BPattern::SetupBlock(size_t i, T8B10BSymbol s, bool dotted)
{
	auto& b = m_blocks[i];

	b.m_typeConnection.disconnect();
	b.m_valueConnection.disconnect();
	b.m_disparityConnection.disconnect();

	//Remove symbol boxes
	if(b.m_symbolEntry.get_parent() != nullptr)
		b.m_grid.remove(b.m_symbolEntry);
	if(b.m_symbolBox.get_parent() != nullptr)
		b.m_grid.remove(b.m_symbolBox);

	//Format content
	string sym = string("D") + to_string(s.value & 0x1f) + '.' + to_string(s.value >> 5);
	if(!dotted)
		sym = "0x" + to_string_hex(s.value, true, 2);
	switch(s.ktype)
	{
		case T8B10BSymbol::KSYMBOL:
			b.m_grid.attach(b.m_symbolBox, 1, 2);
			b.m_disparityBox.set_sensitive(true);
			sym[0] = 'K';
			b.m_symbolBox.set_active_text(sym);
			break;

		case T8B10BSymbol::DSYMBOL:
			b.m_grid.attach(b.m_symbolEntry, 1, 2);
			b.m_symbolEntry.set_sensitive(true);
			b.m_disparityBox.set_sensitive(true);
			b.m_symbolEntry.set_text(sym);
			break;

		case T8B10BSymbol::DONTCARE:
			b.m_grid.attach(b.m_symbolEntry, 1, 2);
			b.m_symbolEntry.set_sensitive(false);
			b.m_disparityBox.set_sensitive(false);
			b.m_symbolEntry.set_text(sym);
			break;
	}

	b.m_typeConnection = b.m_typeBox.signal_changed().connect(sigc::bind(
		sigc::mem_fun(*this, &ParameterRow8B10BPattern::OnTypeChanged), i));
	b.m_disparityConnection = b.m_disparityBox.signal_changed().connect(sigc::bind(
		sigc::mem_fun(*this, &ParameterRow8B10BPattern::OnDisparityChanged), i));
	if(s.ktype == T8B10BSymbol::KSYMBOL)
	{
		b.m_valueConnection = b.m_symbolBox.signal_changed().connect(sigc::bind(
			sigc::mem_fun(*this, &ParameterRow8B10BPattern::OnKValueChanged), i));
	}
	else
	{
		b.m_valueConnection = b.m_symbolEntry.signal_changed().connect(sigc::bind(
			sigc::mem_fun(*this, &ParameterRow8B10BPattern::OnDValueChanged), i));
	}

	b.m_grid.show_all();
}

void ParameterRow8B10BPattern::Initialize(const vector<T8B10BSymbol>& symbols)
{
	m_ignoreEvents = true;

	//Clear content
	auto children = m_contentbox.get_children();
	for(auto c : children)
		m_contentbox.remove(*c);

	//Add box for each element in the pattern
	auto nsymbols = symbols.size();
	m_blocks.resize(nsymbols);
	m_contentbox.set_column_spacing(10);
	for(size_t i=0; i<nsymbols; i++)
	{
		auto& b = m_blocks[i];
		m_contentbox.attach(b.m_frame, i, 0);
			b.m_frame.set_label(string("Symbol ") + to_string(i+1));

		switch(symbols[i].ktype)
		{
			case T8B10BSymbol::KSYMBOL:
				b.m_typeBox.set_active_text("K symbol");
				break;

			case T8B10BSymbol::DSYMBOL:
				b.m_typeBox.set_active_text("D symbol (Dx.y format)");
				break;

			case T8B10BSymbol::DONTCARE:
				b.m_typeBox.set_active_text("Don't care");
				break;
		}

		//Format disparity
		switch(symbols[i].disparity)
		{
			case T8B10BSymbol::POSITIVE:
				b.m_disparityBox.set_active_text("Positive");
				break;

			case T8B10BSymbol::NEGATIVE:
				b.m_disparityBox.set_active_text("Negative");
				break;

			case T8B10BSymbol::ANY:
				b.m_disparityBox.set_active_text("Any");
				break;
		}

		SetupBlock(i, symbols[i], true);
	}

	m_ignoreEvents = false;
}

void ParameterRow8B10BPattern::OnDisparityChanged(size_t i)
{
	auto params = m_param.Get8B10BPattern();

	switch(m_blocks[i].m_disparityBox.get_active_row_number())
	{
		case 0:
			params[i].disparity = T8B10BSymbol::POSITIVE;
			break;

		case 1:
			params[i].disparity = T8B10BSymbol::NEGATIVE;
			break;

		case 2:
		default:
			params[i].disparity = T8B10BSymbol::ANY;
			break;
	}

	m_ignoreUpdates = true;
	m_param.Set8B10BPattern(params);
	m_ignoreUpdates = false;
}

void ParameterRow8B10BPattern::OnTypeChanged(size_t i)
{
	auto params = m_param.Get8B10BPattern();

	bool dotted = true;
	switch(m_blocks[i].m_typeBox.get_active_row_number())
	{
		//K symbol
		case 0:
			params[i].ktype = T8B10BSymbol::KSYMBOL;
			break;

		//D symbol, dotted format
		case 1:
			params[i].ktype = T8B10BSymbol::DSYMBOL;
			break;

		//D symbol, hex format
		case 2:
			params[i].ktype = T8B10BSymbol::DSYMBOL;
			dotted = false;
			break;

		//Dontcare
		case 3:
		default:
			params[i].ktype = T8B10BSymbol::DONTCARE;
			break;
	}

	m_ignoreUpdates = true;
	m_param.Set8B10BPattern(params);
	m_ignoreUpdates = false;

	SetupBlock(i, params[i], dotted);
}

void ParameterRow8B10BPattern::OnPatternChanged()
{
	if(m_ignoreUpdates)
		return;

	Initialize(m_param.Get8B10BPattern());

	m_contentbox.show_all();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ParameterRowString

ParameterRowString::ParameterRowString(Gtk::Dialog* parent, FilterParameter& param, FlowGraphNode* node)
	: ParameterRowBase(parent, param, node)
	, m_timerPending(false)
{
	m_entry.set_hexpand(true);

	if(!param.IsReadOnly())
		m_connection = m_entry.signal_changed().connect(sigc::mem_fun(*this, &ParameterRowString::OnTextChanged));

	m_connection = param.signal_changed().connect(sigc::mem_fun(*this, &ParameterRowString::OnValueChanged));
}

ParameterRowString::~ParameterRowString()
{
	m_connection.disconnect();
	m_timerConnection.disconnect();
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

	m_ignoreUpdates = true;
	m_param.ParseString(m_entry.get_text());
	m_ignoreUpdates = false;

	//This is quite ugly! But there is no GTK signal for "focus lost" on a widget, only on the root window.
	if(!m_timerPending)
	{
		m_timerPending = true;
		m_timerConnection = Glib::signal_timeout().connect(
			sigc::mem_fun(*this, &ParameterRowString::OnFocusLostTimer), 250);
	}
}

void ParameterRowString::OnValueChanged()
{
	if(m_ignoreUpdates)
		return;

	m_ignoreEvents = true;
	m_entry.set_text(m_param.ToString());
	m_ignoreEvents = false;
}

bool ParameterRowString::OnFocusLostTimer()
{
	bool focus = m_entry.has_focus();

	//If focus was lost, reformat the text
	if(!focus)
	{
		m_timerPending = false;

		m_ignoreEvents = true;
		m_entry.set_text(m_param.ToString());
		m_ignoreEvents = false;
	}
	return focus;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ParameterRowEnum

ParameterRowEnum::ParameterRowEnum(Gtk::Dialog* parent, FilterParameter& param, FlowGraphNode* node)
	: ParameterRowBase(parent, param, node)
{
	m_box.signal_changed().connect(sigc::mem_fun(*this, &ParameterRowEnum::OnChanged));
	m_box.set_hexpand(true);

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
		dlg.set_do_overwrite_confirmation();
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
	, m_inputChanging(false)
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
			auto cn = scope->GetOscilloscopeChannel(k);
			if(!cn)
				continue;
			if( !scope->CanEnableChannel(k) && (cn->GetType(0) != Stream::STREAM_TYPE_TRIGGER) )
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
		case FilterParameter::TYPE_8B10B_PATTERN:
			{
				auto row = new ParameterRow8B10BPattern(parent, param, node);
				grid.attach(row->m_label, 0, y, 1, 1);
					row->m_label.set_size_request(width, 1);
					row->m_label.set_label(name);
				grid.attach(row->m_contentbox, 1, y, 1, 1);

				row->Initialize(param.Get8B10BPattern());

				return row;
			}

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

	m_grid.set_hexpand(true);
	m_grid.set_vexpand(true);
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
	//prevent nested events from causing infinite recursion
	if(m_inputChanging)
		return;

	//Apply configuration changes made by the user
	ConfigureInputs(m_filter, m_rows);

	//Apply any automatic input changes generated by this, then propagate them to the dialog if needed
	m_inputChanging = true;
	if(ApplyAutomaticInputs())
		ConfigureInputs(m_filter, m_rows);
	m_inputChanging = false;

	m_parent->RefreshAllFilters();
	m_parent->ClearAllPersistence();
}

/**
	@brief Applies automatic values to filter inputs when an input changes

	This eliminates reptitive configuration to, for example, attach every S-parameter one at a time manually
 */
bool FilterDialog::ApplyAutomaticInputs()
{
	bool madeChanges = false;

	auto nin = m_filter->GetInputCount();
	for(size_t i=0; i<nin; i++)
	{
		//If the input is connected, do nothing
		auto stream = m_filter->GetInput(i);
		if(stream)
			continue;

		//Input is null! See if it's a S-parameter input
		//(by simple string matching for now)
		auto name = m_filter->GetInputName(i);
		if( (name[0] == 'S') && (
				(name.rfind("_mag") + 4 == name.length()) &&
				(name.rfind("_ang") + 4 == name.length())
			))
		{
			//We have a null S-parameter input.
			//See if any of our *other* inputs (for the same S-parameter set) are non-null.
			//Assume for the moment that we're dealing with <10 port S-params.
			//So parameter name is going to be S[digit][digit][optional suffix][_mag | _angle],
			//for example S21A_mag or S11_ang.
			string paramName = name.substr(3);
			paramName.resize(paramName.length() - 4);

			//Find the base S-parameter name for us
			string suffix = name.substr(name.length() - 3);
			string param = name.substr(0, 3);

			//Search all of our inputs to see if any match the same S-parameter and are non-null
			for(size_t j=0; j<nin; j++)
			{
				//If the input is not connected, skip it
				auto sstream = m_filter->GetInput(j);
				if(!sstream)
					continue;

				//Get the name and see if it's the same S-parameter
				auto sname = m_filter->GetInputName(j);
				if(sname[0] != 'S')
					continue;
				if(paramName != "")
				{
					if(sname.find(paramName, 3) != 3)
						continue;
				}

				//Look at where the input came from and find our corresponding channel
				auto match = FindCorrespondingSParameter(param, suffix, sstream);
				if(!match)
					continue;

				//Connect it
				m_rows[i]->m_chans.set_active_text(match.GetName());
				madeChanges = true;
				break;
			}
		}

		//Some filters, like channel emulation, take simple mag/angle
		else if( (name == "mag") || (name == "angle") )
		{
			//See if we have an input with the opposite name
			string counterpart;
			string suffix;
			if(name == "mag")
			{
				counterpart = "angle";
				suffix = "mag";
			}
			else
			{
				counterpart = "mag";
				suffix = "ang";
			}

			for(size_t j=0; j<nin; j++)
			{
				//If the input is not connected, skip it
				auto sstream = m_filter->GetInput(j);
				if(!sstream)
					continue;

				//Get the name and see if it's the same S-parameter
				auto sname = m_filter->GetInputName(j);
				if(sname != counterpart)
					continue;

				//We found it!
				//See what it's connected to
				auto src = sstream.m_channel->GetStreamName(sstream.m_stream);
				string param = src.substr(0, 3);

				//Look at where the input came from and find our corresponding channel
				auto match = FindCorrespondingSParameter(param, suffix, sstream);
				if(!match)
					continue;

				//Connect it
				m_rows[i]->m_chans.set_active_text(match.GetName());
				madeChanges = true;
				break;
			}
		}
	}

	return madeChanges;
}

/**
	@brief Finds a stream corresponding to a given S-parameter on a target object and stream.

	Two possible cases: can be an instrument (Sxx.mag/angle) or a filter (x.Sxx_mag/angle)

	For example:
		Given param="S21", suffix="mag", ref=Touchstone1.S11_mag, returns Touchstone1.S21_mag.
		Given param="S11", suffix="ang", ref=MyVNA S22_mag, returns MyVNA S11_ang
 */
StreamDescriptor FilterDialog::FindCorrespondingSParameter(
	const string& param,
	const string& suffix,
	StreamDescriptor ref)
{
	//See if the input is coming from an instrument or a filter
	auto f = dynamic_cast<Filter*>(ref.m_channel);
	auto scope = dynamic_cast<OscilloscopeChannel*>(ref.m_channel)->GetScope();
	if(f)
	{
		//Coming from a filter. Look for an output called Sxx_suffix
		auto target = param + "_" + suffix;

		auto len = f->GetStreamCount();
		for(size_t i=0; i<len; i++)
		{
			if(f->GetStreamName(i) == target)
				return StreamDescriptor(f, i);
		}
	}

	else if(scope)
	{
		//Coming from an instrument.
		//Look for a channel called Sxx with a stream suffix.
		auto chan = scope->GetOscilloscopeChannelByHwName(param);
		if(!chan)
			return StreamDescriptor(nullptr, 0);

		//Look at streams
		auto len = chan->GetStreamCount();
		for(size_t i=0; i<len; i++)
		{
			if(chan->GetStreamName(i) == suffix)
				return StreamDescriptor(chan, i);
		}
	}

	//If we get here, nothing found - give up
	return StreamDescriptor(nullptr, 0);
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
