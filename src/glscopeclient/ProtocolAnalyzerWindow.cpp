/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2021 Andrew D. Zonenberg                                                                          *
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
	@brief  Implementation of ProtocolAnalyzerWindow
 */
#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "ProtocolAnalyzerWindow.h"
#include "../../lib/scopeprotocols/scopeprotocols.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ProtocolDisplayFilter

ProtocolDisplayFilter::ProtocolDisplayFilter(string str, size_t& i)
{
	//One or more clauses separated by operators
	while(i < str.length())
	{
		//Read the clause
		m_clauses.push_back(new ProtocolDisplayFilterClause(str, i));

		//Remove spaces before the operator
		EatSpaces(str, i);
		if( (i >= str.length()) || (str[i] == ')') )
			break;

		//Read the operator, if any
		string tmp;
		while(i < str.length())
		{
			if(isspace(str[i]) || (str[i] == '\"') || (str[i] == '(') || (str[i] == ')') )
				break;

			//An alphanumeric character after an operator other than text terminates it
			if( (tmp != "") && !isalnum(tmp[0]) && isalnum(str[i]) )
				break;

			tmp += str[i];
			i++;
		}
		m_operators.push_back(tmp);
	}
}

ProtocolDisplayFilter::~ProtocolDisplayFilter()
{
	for(auto c : m_clauses)
		delete c;
}

bool ProtocolDisplayFilter::Validate(vector<string> headers)
{
	//No clauses? valid all-pass filter
	if(m_clauses.empty())
		return true;

	//We should always have one more clause than operator
	if( (m_operators.size() + 1) != m_clauses.size())
		return false;

	//Operators must make sense. For now only equal/unequal and boolean and/or allowed
	for(auto op : m_operators)
	{
		if( (op != "==") &&
			(op != "!=") &&
			(op != "||") &&
			(op != "&&") &&
			(op != "startswith") &&
			(op != "contains")
		)
		{
			return false;
		}
	}

	//If any clause is invalid, we're invalid
	for(auto c : m_clauses)
	{
		if(!c->Validate(headers))
			return false;
	}

	//A single literal is not a legal filter, it has to be compared to something
	if(m_clauses.size() == 1)
	{
		if(m_clauses[0]->m_type != ProtocolDisplayFilterClause::TYPE_EXPRESSION)
			return false;
	}

	return true;
}

void ProtocolDisplayFilter::EatSpaces(string str, size_t& i)
{
	while( (i < str.length()) && isspace(str[i]) )
		i++;
}

bool ProtocolDisplayFilter::Match(
	const Gtk::TreeRow& row,
	ProtocolAnalyzerColumns& cols)
{
	if(m_clauses.empty())
		return true;
	else
		return Evaluate(row, cols) != "0";
}

std::string ProtocolDisplayFilter::Evaluate(
	const Gtk::TreeRow& row,
	ProtocolAnalyzerColumns& cols)
{
	//Calling code checks for validity so no need to verify here

	//For now, all operators have equal precedence and are evaluated left to right.
	string current = m_clauses[0]->Evaluate(row, cols);
	for(size_t i=1; i<m_clauses.size(); i++)
	{
		string rhs = m_clauses[i]->Evaluate(row, cols);
		string op = m_operators[i-1];

		bool a = (current != "0");
		bool b = (rhs != "0");

		//== and != do exact string equality checks
		bool temp = false;
		if(op == "==")
			temp = (current == rhs);
		else if(op == "!=")
			temp = (current != rhs);

		//&& and || do boolean operations
		else if(op == "&&")
			temp = (a && b);
		else if(op == "||")
			temp = (a || b);

		//String prefix
		else if(op == "startswith")
			temp = (current.find(rhs) == 0);
		else if(op == "contains")
			temp = (current.find(rhs) != string::npos);

		//done, convert back to string
		current = temp ? "1" : "0";
	}
	return current;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ProtocolDisplayFilterClause

ProtocolDisplayFilterClause::ProtocolDisplayFilterClause(string str, size_t& i)
{
	ProtocolDisplayFilter::EatSpaces(str, i);

	m_number = 0;
	m_expression = 0;
	m_invert = false;

	m_cachedIndex = 0;

	//Parenthetical expression
	if( (str[i] == '(') || (str[i] == '!') )
	{
		//Inversion
		if(str[i] == '!')
		{
			m_invert = true;
			i++;

			if(str[i] != '(')
			{
				m_type = TYPE_ERROR;
				i++;
				return;
			}
		}

		i++;
		m_type = TYPE_EXPRESSION;
		m_expression = new ProtocolDisplayFilter(str, i);

		//eat trailing spaces
		ProtocolDisplayFilter::EatSpaces(str, i);

		//expect closing parentheses
		if(str[i] != ')')
			m_type = TYPE_ERROR;
		i++;
	}

	//Quoted string
	else if(str[i] == '\"')
	{
		m_type = TYPE_STRING;
		i++;

		while( (i < str.length()) && (str[i] != '\"') )
		{
			m_string += str[i];
			i++;
		}

		if(str[i] != '\"')
			m_type = TYPE_ERROR;

		i++;
	}

	//Number
	else if(isdigit(str[i]) || (str[i] == '-') || (str[i] == '.') )
	{
		m_type = TYPE_NUMBER;

		string tmp;
		while( (i < str.length()) && (isdigit(str[i]) || (str[i] == '-')  || (str[i] == '.') ) )
		{
			tmp += str[i];
			i++;
		}

		m_number = atof(tmp.c_str());
	}

	//Identifier
	else
	{
		m_type = TYPE_IDENTIFIER;

		while( (i < str.length()) && isalnum(str[i]) )
		{
			m_identifier += str[i];
			i++;
		}

		if(m_identifier == "")
		{
			i++;
			m_type = TYPE_ERROR;
		}
	}
}

string ProtocolDisplayFilterClause::Evaluate(
	const Gtk::TreeRow& row,
	ProtocolAnalyzerColumns& cols)
{
	char tmp[32];

	switch(m_type)
	{
		case TYPE_IDENTIFIER:
			return (Glib::ustring)row[cols.m_headers[m_cachedIndex]];

		case TYPE_STRING:
			return m_string;

		case TYPE_NUMBER:
			snprintf(tmp, sizeof(tmp), "%f", m_number);
			return tmp;

		case TYPE_EXPRESSION:
			if(m_invert)
			{
				if(m_expression->Evaluate(row, cols) == "1")
					return "0";
				else
					return "1";
			}
			else
				return m_expression->Evaluate(row, cols);

		case TYPE_ERROR:
		default:
			return "NaN";
	}
}

ProtocolDisplayFilterClause::~ProtocolDisplayFilterClause()
{
	if(m_expression)
		delete m_expression;
}

bool ProtocolDisplayFilterClause::Validate(vector<string> headers)
{
	switch(m_type)
	{
		case TYPE_ERROR:
			return false;

		//If we're an identifier, we must be a valid header field
		//TODO: support comparisons on data
		case TYPE_IDENTIFIER:
			for(size_t i=0; i<headers.size(); i++)
			{
				//Match, removing spaces from header names if needed
				string h;
				string header = headers[i];
				for(size_t j=0; j<header.length(); j++)
				{
					char ch = header[j];
					if(!isspace(ch))
						h += ch;
				}

				if(h == m_identifier)
				{
					m_cachedIndex = i;
					return true;
				}
			}

			return false;

		//If we're an expression, it must be valid
		case TYPE_EXPRESSION:
			return m_expression->Validate(headers);

		default:
			return true;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ProtocolAnalyzerColumns

ProtocolAnalyzerColumns::ProtocolAnalyzerColumns(PacketDecoder* decoder)
{
	add(m_visible);
	add(m_bgcolor);
	add(m_fgcolor);
	add(m_height);
	add(m_timestamp);
	add(m_capturekey);
	add(m_offset);

	auto headers = decoder->GetHeaders();
	for(size_t i=0; i<headers.size(); i++)
	{
		m_headers.push_back(Gtk::TreeModelColumn<Glib::ustring>());
		add(m_headers[m_headers.size()-1]);
	}

	add(m_image);
	add(m_data);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ProtocolAnalyzerWindow::ProtocolAnalyzerWindow(
	const string& title,
	OscilloscopeWindow* parent,
	PacketDecoder* decoder,
	WaveformArea* area)
	: Gtk::Dialog(title, *parent)
	, m_parent(parent)
	, m_decoder(decoder)
	, m_area(area)
	, m_columns(decoder)
	, m_imageWidth(0)
	, m_updating(false)
{
	set_skip_taskbar_hint();
	set_type_hint(Gdk::WINDOW_TYPE_HINT_DIALOG);

	m_decoder->AddRef();

	set_default_size(1024, 600);

	//Set up the tree view
	m_internalmodel = ProtocolTreeModel::create(m_columns);
	m_model = Gtk::TreeModelFilter::create(m_internalmodel);
	m_tree.set_model(m_model);
	m_model->set_visible_column(m_columns.m_visible);

	//Add the columns
	m_tree.append_column("Time", m_columns.m_timestamp);
	auto headers = decoder->GetHeaders();
	for(size_t i=0; i<headers.size(); i++)
		m_tree.append_column(headers[i], m_columns.m_headers[i]);

	int ncols = headers.size() + 1;
	if(decoder->GetShowImageColumn())
	{
		m_tree.append_column("Image", m_columns.m_image);
		m_tree.get_style_context()->add_class("video");
		ncols ++;
	}

	if(decoder->GetShowDataColumn())
	{
		m_tree.append_column("Data", m_columns.m_data);
		ncols ++;
	}

	//Set up colors and images
	for(int col=0; col<ncols; col ++)
	{
		auto pcol = m_tree.get_column(col);
		vector<Gtk::CellRenderer*> cells = pcol->get_cells();
		for(auto c : cells)
		{
			//Pixbuf cells don't have fg/bg attributes
			if(dynamic_cast<Gtk::CellRendererPixbuf*>(c) == NULL)
			{
				pcol->add_attribute(*c, "background-gdk", 1);	//column 1 is bg color
				pcol->add_attribute(*c, "foreground-gdk", 2);	//column 2 is fg color
			}

			if(decoder->GetShowImageColumn())
				pcol->add_attribute(*c, "height", 3);		//column 3 is height
		}
	}

	m_tree.get_selection()->signal_changed().connect(
		sigc::mem_fun(*this, &ProtocolAnalyzerWindow::OnSelectionChanged));
	m_filterApplyButton.signal_clicked().connect(
		sigc::mem_fun(*this, &ProtocolAnalyzerWindow::OnApplyFilter));
	m_filterBox.signal_changed().connect(
		sigc::mem_fun(*this, &ProtocolAnalyzerWindow::OnFilterChanged));

	//Set up the widgets
	get_vbox()->pack_start(m_menu, Gtk::PACK_SHRINK);
		m_menu.append(m_fileMenuItem);
			m_fileMenuItem.set_label("File");
			m_fileMenuItem.set_submenu(m_fileMenu);
			m_fileMenu.append(m_fileExportMenuItem);
				m_fileExportMenuItem.set_label("Export...");
				m_fileExportMenuItem.signal_activate().connect(sigc::mem_fun(
					*this, &ProtocolAnalyzerWindow::OnFileExport));

	get_vbox()->pack_start(m_filterRow, Gtk::PACK_SHRINK);
		m_filterRow.pack_start(m_filterBox, Gtk::PACK_EXPAND_WIDGET);
		m_filterRow.pack_start(m_filterApplyButton, Gtk::PACK_SHRINK);
			m_filterApplyButton.set_label("Apply");
	get_vbox()->pack_start(m_scroller, Gtk::PACK_EXPAND_WIDGET);
		m_scroller.add(m_tree);
			m_tree.get_selection()->set_mode(Gtk::SELECTION_BROWSE);
		m_scroller.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	show_all();
}

ProtocolAnalyzerWindow::~ProtocolAnalyzerWindow()
{
	m_decoder->Release();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

void ProtocolAnalyzerWindow::OnWaveformDataReady()
{
	auto data = m_decoder->GetData(0);
	if(data == NULL)
		return;
	auto packets = m_decoder->GetPackets();
	if(packets.empty())
		return;

	auto headers = m_decoder->GetHeaders();

	m_updating = true;

	//Get ready to filter new packets
	size_t j = 0;
	ProtocolDisplayFilter filter(m_filterBox.get_text(), j);
	bool filtering = filter.Validate(m_decoder->GetHeaders());

	Packet* first_packet_in_group = NULL;
	Packet* last_packet = NULL;
	Gtk::TreeModel::iterator last_top_row = m_model->children().end();

	auto npackets = packets.size();
	for(size_t i=0; i<npackets; i++)
	{
		auto p = packets[i];

		//See if we should start a new merge group
		bool starting_new_group;
		if(i+1 >= npackets)									//No next packet to merge with
			starting_new_group = false;
		else if(!m_decoder->CanMerge(p, p, packets[i+1]))	//This packet isn't compatible with the next
			starting_new_group = false;
		else if(first_packet_in_group == NULL)				//If we get here, we're merging. But are we already?
			starting_new_group = true;
		else												//Already in a group, but it's not the same as the new one
			starting_new_group = !m_decoder->CanMerge(first_packet_in_group, last_packet, p);

		if(starting_new_group)
		{
			//Create the summary packet
			first_packet_in_group = p;
			auto parent_packet = m_decoder->CreateMergedHeader(p, i);

			//Add it
			last_top_row = *m_internalmodel->append();
			FillOutRow(*last_top_row, parent_packet, data, headers);
			delete parent_packet;

			//Default to not being shown
			(*last_top_row)[m_columns.m_visible] = false;
		}

		//End a merge group
		else if( (first_packet_in_group != NULL) && !m_decoder->CanMerge(first_packet_in_group, last_packet, p) )
			first_packet_in_group = NULL;

		//Create a row for the new packet. This might be top level or under a merge group
		Gtk::TreeModel::iterator row;
		if(first_packet_in_group != NULL)
			row = m_internalmodel->append(last_top_row->children());
		else
		{
			row = m_internalmodel->append();
			last_top_row = row;
		}

		//Populate the row
		FillOutRow(*row, p, data, headers);

		//Check against filters
		if(filtering)
		{
			bool visible = filter.Match(*row, m_columns);
			(*row)[m_columns.m_visible] = visible;

			//Show expandable rows if at least one is visible
			if(visible && first_packet_in_group != NULL)
				(*last_top_row)[m_columns.m_visible] = true;
		}
		else
		{
			(*row)[m_columns.m_visible] = true;

			//If not filtering, show the parent row
			if(first_packet_in_group != NULL)
				(*last_top_row)[m_columns.m_visible] = true;
		}

		last_packet = p;
	}

	//Select the last row
	auto len = m_model->children().size();
	if(len != 0)
	{
		Gtk::TreePath path;
		path.push_back(len - 1);
		m_tree.expand_to_path(path);
		m_tree.scroll_to_row(path);
	}

	m_updating = false;
}

void ProtocolAnalyzerWindow::FillOutRow(
	const Gtk::TreeRow& row,
	Packet* p,
	WaveformBase* data,
	vector<string>& headers)
{
	//Need a bit of math in case the capture is >1 second long
	time_t capstart = data->m_startTimestamp;
	int64_t fs = data->m_startFemtoseconds + p->m_offset;
	if(fs > FS_PER_SECOND)
	{
		capstart += (fs / FS_PER_SECOND);
		fs %= (int64_t)FS_PER_SECOND;
	}

	//Format timestamp
	char tmp[128];
	strftime(tmp, sizeof(tmp), "%H:%M:%S.", localtime(&capstart));
	string stime = tmp;
	snprintf(tmp, sizeof(tmp), "%010zu", static_cast<size_t>(fs / 100000));	//round to nearest 100ps for display
	stime += tmp;

	//Create the row
	row[m_columns.m_bgcolor] = p->m_displayBackgroundColor;
	row[m_columns.m_fgcolor] = p->m_displayForegroundColor;
	row[m_columns.m_timestamp] = stime;
	row[m_columns.m_capturekey] = TimePoint(data->m_startTimestamp, data->m_startFemtoseconds);
	row[m_columns.m_offset] = p->m_offset;

	//Just copy headers without any processing
	for(size_t i=0; i<headers.size(); i++)
		row[m_columns.m_headers[i]] = p->m_headers[headers[i]];

	//Convert data to hex
	string sdata;
	for(auto b : p->m_data)
	{
		char t[4];
		snprintf(t, sizeof(t), "%02x ", b);
		sdata += t;

		//Truncate really long packets to keep UI from going nuts
		if(sdata.length() > 2048)
			break;
	}
	row[m_columns.m_data] = sdata;

	//Add the image for video packets
	auto vp = dynamic_cast<VideoScanlinePacket*>(p);
	if(vp != NULL)
	{
		size_t rowsize = p->m_data.size();
		size_t width = rowsize / 3;
		size_t rowsize_rounded = width*3;
		m_imageWidth = max(m_imageWidth, width);
		if(width > 0)
		{
			size_t height = 12;

			Glib::RefPtr<Gdk::Pixbuf> image = Gdk::Pixbuf::create(
				Gdk::COLORSPACE_RGB,
				false,
				8,
				m_imageWidth,
				height);

			//Stretch it into a 2D image
			uint8_t* pixels = image->get_pixels();
			size_t stride = image->get_rowstride();
			for(size_t y=0; y<height; y++)
			{
				//Copy the pixel data for this row
				auto rowpix = pixels + y*stride;
				memcpy(rowpix, &p->m_data[0], rowsize_rounded);

				//If this scanline is truncated, pad with a light/dark gray checkerboard
				if(width < m_imageWidth)
				{
					uint8_t a = 0x80;
					uint8_t b = 0xc0;

					for(size_t x=width; x<m_imageWidth; x++)
					{
						if( (x/6 ^ y/6) & 0x1)
						{
							rowpix[x*3 + 0] = a;
							rowpix[x*3 + 1] = a;
							rowpix[x*3 + 2] = a;
						}
						else
						{
							rowpix[x*3 + 0] = b;
							rowpix[x*3 + 1] = b;
							rowpix[x*3 + 2] = b;
						}
					}
				}
			}

			row[m_columns.m_image] = image;
			row[m_columns.m_height] = height;
		}
	}
}

void ProtocolAnalyzerWindow::OnSelectionChanged()
{
	//If we're updating with a new waveform we're already on the newest waveform.
	//No need to refresh anything.
	if(m_updating)
		return;

	auto sel = m_tree.get_selection();
	if(sel->count_selected_rows() == 0)
		return;
	auto row = *sel->get_selected();

	//Select the waveform
	m_parent->JumpToHistory(row[m_columns.m_capturekey]);

	//Set the offset of the decoder's group
	m_area->CenterTimestamp(row[m_columns.m_offset]);
	m_area->m_group->m_frame.queue_draw();
}

/**
	@brief Remove history before a certain point
 */
void ProtocolAnalyzerWindow::RemoveHistory(TimePoint timestamp)
{
	m_updating = true;

	//This always happens from the start of time, so just remove from the beginning of our list
	//until we have nothing that matches.
	auto children = m_internalmodel->children();
	while(!children.empty())
	{
		//Stop if the timestamp is before our first point
		auto it = children.begin();
		TimePoint reftime = (*it)[m_columns.m_capturekey];
		if(timestamp.first < reftime.first)
			break;
		if( (timestamp.first == reftime.first) && (timestamp.second <= reftime.second) )
			break;

		//Remove it
		m_internalmodel->erase(it);
	}

	m_updating = false;
}

void ProtocolAnalyzerWindow::OnApplyFilter()
{
	//Parse the filter
	size_t i = 0;
	auto text = m_filterBox.get_text();
	ProtocolDisplayFilter filter(text, i);

	//If filter is invalid, can't do anything!
	auto headers = m_decoder->GetHeaders();
	if(!filter.Validate(headers))
		return;

	auto children = m_internalmodel->children();
	size_t len = children.size();
	size_t j=0;
	for(auto& row : children)
	{
		//FIXME: something is messed up in ProtocolTreeModel causing this to not exit properly
		if(j >= len)
			break;

		//No children? Filter this row
		auto rowchildren = row->children();
		if(rowchildren.empty())
			row[m_columns.m_visible] = filter.Match(row, m_columns);

		//Children? Visible if any child is visible
		else
		{
			row[m_columns.m_visible] = false;
			for(auto child : rowchildren)
			{
				if(filter.Match(child, m_columns))
				{
					row[m_columns.m_visible] = true;
					break;
				}
			}
		}

		j++;
	}

	//Done
	if(text == "")
		m_filterBox.set_name("");
	else
		m_filterBox.set_name("activefilter");
	m_filterApplyButton.set_sensitive(false);
}

void ProtocolAnalyzerWindow::OnFilterChanged()
{
	//Parse the filter
	size_t i = 0;
	auto text = m_filterBox.get_text();
	ProtocolDisplayFilter filter(text, i);

	if(filter.Validate(m_decoder->GetHeaders()))
	{
		if(text == "")
			m_filterBox.set_name("");
		else
			m_filterBox.set_name("validfilter");
		m_filterApplyButton.set_sensitive();
	}
	else
	{
		m_filterBox.set_name("invalidfilter");
		m_filterApplyButton.set_sensitive(false);
	}
}

void ProtocolAnalyzerWindow::OnFileExport()
{
	//Prompt for the file
	Gtk::FileChooserDialog dlg(*this, "Export CSV", Gtk::FILE_CHOOSER_ACTION_SAVE);
	auto filter = Gtk::FileFilter::create();
	filter->add_pattern("*.csv");
	filter->set_name("CSV files (*.csv)");
	dlg.add_filter(filter);
	dlg.add_button("Save", Gtk::RESPONSE_OK);
	dlg.add_button("Cancel", Gtk::RESPONSE_CANCEL);
	dlg.set_do_overwrite_confirmation();
	auto response = dlg.run();
	if(response != Gtk::RESPONSE_OK)
		return;

	//Write initial headers
	auto fname = dlg.get_filename();
	FILE* fp = fopen(fname.c_str(), "w");
	if(!fp)
	{
		string msg = string("Output file") + fname + " cannot be opened";
		Gtk::MessageDialog errdlg(msg, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		errdlg.set_title("Cannot export protocol data\n");
		errdlg.run();
		return;
	}
	auto headers = m_decoder->GetHeaders();
	fprintf(fp,"Time,");
	for(auto h : headers)
		fprintf(fp, "%s,", h.c_str());
	fprintf(fp, "Data\n");

	//Write packet data
	auto children = m_internalmodel->children();
	for(auto row : children)
	{
		//TODO: output individual sub-rows for child nodes?
		//For now, just output top level rows
		fprintf(fp, "%s,", static_cast<Glib::ustring>(row[m_columns.m_timestamp]).c_str());

		for(size_t i=0; i<headers.size(); i++)
			fprintf(fp, "%s,", static_cast<Glib::ustring>(row[m_columns.m_headers[i]]).c_str());

		fprintf(fp, "%s\n", static_cast<Glib::ustring>(row[m_columns.m_data]).c_str());
	}

	//Done
	fclose(fp);
}

void ProtocolAnalyzerWindow::on_hide()
{
	Gtk::Widget::on_hide();
	m_parent->GarbageCollectAnalyzers();
}

void ProtocolAnalyzerWindow::SelectPacket(TimePoint cap, int64_t offset)
{
	auto& rows = m_internalmodel->GetRows();
	auto len = rows.size();

	//Loop over packets from last to to first, looking for this packet
	//TODO: binary search on capture key, then offset?
	//But that would be hard if we don't know which rows are visible. For now, stay linear.
	size_t ivis = 0;
	auto sel = m_tree.get_selection();
	for(size_t i=0; i<len; i++)
	{
		auto& row = rows[i];
		if(!row.m_visible)
			continue;

		if(cap != row.m_capturekey)
		{
			ivis ++;
			continue;
		}

		if(row.m_offset > offset)
			break;

		Gtk::TreePath path;
		path.push_back(ivis);

		//Check child nodes
		if(!row.m_children.empty())
		{
			size_t jvis = 0;
			for(size_t j=0; j<row.m_children.size(); j++)
			{
				auto& child = rows[i];
				if(!child.m_visible)
					continue;
				if(offset != child.m_offset)
				{
					jvis ++;
					continue;
				}

				path.push_back(jvis);
				m_updating = true;
				sel->select(path);
				m_tree.scroll_to_row(path);
				m_updating = false;
				return;
			}
		}
		else if(offset != row.m_offset)
		{
			ivis ++;
			continue;
		}

		m_updating = true;
		sel->select(path);
		m_tree.scroll_to_row(path);
		m_updating = false;
		break;
	}
}
