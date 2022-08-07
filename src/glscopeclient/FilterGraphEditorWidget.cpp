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
	@brief Implementation of FilterGraphEditorWidget
 */
#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "FilterGraphEditorWidget.h"
#include "FilterGraphEditor.h"
#include "ChannelPropertiesDialog.h"
#include "FilterDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FilterGraphRoutingColumn

int FilterGraphRoutingColumn::GetVerticalChannel(StreamDescriptor stream)
{
	//Reuse an existing channel if we find one for the same source signal
	auto it = m_usedVerticalChannels.find(stream);
	if(it != m_usedVerticalChannels.end())
		return it->second;
	else
	{
		//If no channels to use, abort.
		//We need to increase column spacing and re-layout
		if(m_freeVerticalChannels.empty())
			return -1;

		//Find a vertical space in the channel to use
		int x = *m_freeVerticalChannels.begin();
		m_freeVerticalChannels.pop_front();
		m_usedVerticalChannels[stream] = x;
		return x;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FilterGraphEditorPath

FilterGraphEditorPath::FilterGraphEditorPath(
	FilterGraphEditorNode* fromnode,
	size_t fromport,
	FilterGraphEditorNode* tonode,
	size_t toport)
	: m_fromNode(fromnode)
	, m_fromPort(fromport)
	, m_toNode(tonode)
	, m_toPort(toport)
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FilterGraphEditorNode

FilterGraphEditorNode::FilterGraphEditorNode(FilterGraphEditorWidget* parent, FlowGraphNode* node)
	: m_parent(parent)
	, m_node(node)
	, m_positionValid(false)
	, m_margin(2)
	, m_column(0)
{
	auto f = dynamic_cast<Filter*>(node);
	if(f)
	{
		//start
		m_outputsChangedConnection = f->signal_outputsChanged().connect(
			sigc::mem_fun(this, &FilterGraphEditorNode::UpdateSize));
	}
}

FilterGraphEditorNode::~FilterGraphEditorNode()
{
	m_outputsChangedConnection.disconnect();

	m_parent->OnNodeDeleted(this);
}

void FilterGraphEditorNode::UpdateSize()
{
	auto headerfont = m_parent->GetPreferences().GetFont("Appearance.Filter Graph.node_name_font");
	auto portfont = m_parent->GetPreferences().GetFont("Appearance.Filter Graph.port_font");
	auto paramfont = m_parent->GetPreferences().GetFont("Appearance.Filter Graph.param_font");

	//Clear out old ports in preparation for recreating them
	m_inputPorts.clear();
	m_outputPorts.clear();

	//Channel name text
	int twidth, theight;
	m_titleLayout = Pango::Layout::create(m_parent->get_pango_context());
	m_titleLayout->set_font_description(headerfont);
	auto filter = dynamic_cast<Filter*>(m_node);
	auto chan = dynamic_cast<OscilloscopeChannel*>(m_node);
	auto trig = dynamic_cast<Trigger*>(m_node);
	if(filter != NULL)
	{
		if(filter->IsUsingDefaultName())
			m_titleLayout->set_text(filter->GetProtocolDisplayName());
		else
			m_titleLayout->set_text(filter->GetProtocolDisplayName() + ": " + filter->GetDisplayName());
	}
	else if(chan)
		m_titleLayout->set_text(chan->GetDisplayName());
	else if(trig)
		m_titleLayout->set_text(string("Trigger: ") + trig->GetScope()->m_nickname);
	m_titleLayout->get_pixel_size(twidth, theight);

	//Title box
	m_titleRect.set_x(m_margin);
	m_titleRect.set_y(m_margin);
	m_titleRect.set_width(twidth + 2*m_margin);
	m_titleRect.set_height(theight + 2*m_margin);

	int bottom = m_titleRect.get_bottom();
	int right = twidth + 2*m_margin;

	//Input ports
	if(filter != NULL)
	{
		for(size_t i=0; i<filter->GetInputCount(); i++)
		{
			FilterGraphEditorPort port;
			port.m_label = filter->GetInputName(i);
			port.m_layout = Pango::Layout::create(m_parent->get_pango_context());
			port.m_layout->set_font_description(portfont);
			port.m_layout->set_text(port.m_label);
			port.m_layout->get_pixel_size(twidth, theight);

			port.m_rect.set_x(0);
			port.m_rect.set_y(bottom + 2*m_margin);
			port.m_rect.set_width(twidth + 2*m_margin);
			port.m_rect.set_height(theight + 2*m_margin);

			port.m_index = i;

			bottom = port.m_rect.get_bottom();

			m_inputPorts.push_back(port);
		}
	}

	//Normalize input ports to all have the same width
	int w = 0;
	for(auto& p : m_inputPorts)
		w = max(w, p.m_rect.get_width());
	for(auto& p : m_inputPorts)
		p.m_rect.set_width(w);

	int y = m_titleRect.get_bottom();

	const int param_margin = 10;

	//Parameters
	m_paramLayout = Pango::Layout::create(m_parent->get_pango_context());
	m_paramLayout->set_font_description(paramfont);
	string paramText;
	Pango::TabArray tabs(1, true);
	if(filter != NULL)
	{
		tabs.set_tab(0, Pango::TAB_LEFT, 150);

		for(auto it = filter->GetParamBegin(); it != filter->GetParamEnd(); it ++)
			paramText += it->first + ": \t" + it->second.ToString() + "\n";
	}
	else if(chan && chan->IsPhysicalChannel())
	{
		tabs.set_tab(0, Pango::TAB_LEFT, 100);

		Unit v(Unit::UNIT_VOLTS);
		Unit hz(Unit::UNIT_HZ);

		paramText += string("Channel: \t") + chan->GetHwname() + "\n";

		if(chan->GetType(0) == Stream::STREAM_TYPE_ANALOG)
		{
			switch(chan->GetCoupling())
			{
				case OscilloscopeChannel::COUPLE_DC_1M:
					paramText += "Coupling:\tDC 1MΩ\n";
					break;
				case OscilloscopeChannel::COUPLE_AC_1M:
					paramText += "Coupling:\tAC 1MΩ\n";
					break;
				case OscilloscopeChannel::COUPLE_DC_50:
					paramText += "Coupling:\tDC 50Ω\n";
					break;
				default:
					break;
			}

			paramText += string("Attenuation:\t") + to_string(chan->GetAttenuation()) + "x\n";

			int bwl = chan->GetBandwidthLimit();
			if(bwl != 0)
				paramText += string("Bandwidth:\t") + hz.PrettyPrint(bwl * 1e6) + "\n";

			int nstreams = chan->GetStreamCount();
			if(nstreams == 1)
			{
				auto yunits = chan->GetYAxisUnits(0);
				paramText += string("Range:\t") + yunits.PrettyPrint(chan->GetVoltageRange(0)) + "\n";
				paramText += string("Offset:\t") + yunits.PrettyPrint(chan->GetOffset(0)) + "\n";
			}
			else
			{
				for(int i=0; i<nstreams; i++)
				{
					auto yunits = chan->GetYAxisUnits(i);
					paramText += chan->GetStreamName(i) + ":\n";
					paramText += string("    Range:\t") + yunits.PrettyPrint(chan->GetVoltageRange(i)) + "\n";
					paramText += string("    Offset:\t") + yunits.PrettyPrint(chan->GetOffset(i)) + "\n";
				}
			}
		}
	}
	m_paramLayout->set_text(paramText);
	m_paramLayout->set_tabs(tabs);
	m_paramLayout->get_pixel_size(twidth, theight);
	m_paramRect.set_x(w + param_margin);
	m_paramRect.set_width(twidth + 2*m_margin);
	m_paramRect.set_y(y + 2*m_margin);
	m_paramRect.set_height(theight + 2*m_margin);

	//Output ports
	if(chan)
	{
		for(size_t i=0; i<chan->GetStreamCount(); i++)
		{
			FilterGraphEditorPort port;
			port.m_label = chan->GetStreamName(i);
			port.m_layout = Pango::Layout::create(m_parent->get_pango_context());
			port.m_layout->set_font_description(portfont);
			port.m_layout->set_text(port.m_label);
			port.m_layout->get_pixel_size(twidth, theight);

			port.m_index = i;

			port.m_rect.set_x(0);
			port.m_rect.set_y(y + 2*m_margin);
			port.m_rect.set_width(twidth + 2*m_margin);
			port.m_rect.set_height(theight + 2*m_margin);

			y = port.m_rect.get_bottom();

			m_outputPorts.push_back(port);
		}
	}
	bottom = max(bottom, y);
	bottom = max(bottom, m_paramRect.get_bottom());

	//Normalize output ports to the same width
	int rw = 0;
	for(auto& p : m_outputPorts)
		rw = max(rw, p.m_rect.get_width());
	for(auto& p : m_outputPorts)
		p.m_rect.set_width(rw);

	//Calculate overall width
	int width1 = w + rw + 2*param_margin + m_paramRect.get_width();
	right = max(right, width1);
	int outleft = right - rw;

	//Move output ports to the right side
	for(auto& p : m_outputPorts)
		p.m_rect.set_x(outleft);

	//Center title
	m_titleRect.set_x(right/2 - m_titleRect.get_width()/2 + m_margin);

	//Set size
	m_rect.set_width(right);
	m_rect.set_height(bottom);
}

void FilterGraphEditorNode::Render(const Cairo::RefPtr<Cairo::Context>& cr)
{
	auto outline_color = m_parent->GetPreferences().GetColor("Appearance.Filter Graph.outline_color");
	auto fill_color = m_parent->GetPreferences().GetColor("Appearance.Filter Graph.node_color");
	auto text_color = m_parent->GetPreferences().GetColor("Appearance.Filter Graph.node_text_color");
	auto title_text_color = m_parent->GetPreferences().GetColor("Appearance.Filter Graph.node_title_text_color");
	Gdk::Color channel_color;
	auto trig = dynamic_cast<Trigger*>(m_node);
	auto chan = dynamic_cast<OscilloscopeChannel*>(m_node);
	if(chan)
		channel_color = Gdk::Color(chan->m_displaycolor);
	else
	{
		//Use color of input
		auto in = trig->GetInput(0);
		if(in)
			channel_color = Gdk::Color(in.m_channel->m_displaycolor);
	}

	auto analog_color = m_parent->GetPreferences().GetColor("Appearance.Filter Graph.analog_port_color");
	auto complex_color = m_parent->GetPreferences().GetColor("Appearance.Filter Graph.complex_port_color");
	auto digital_color = m_parent->GetPreferences().GetColor("Appearance.Filter Graph.digital_port_color");
	auto disabled_color = m_parent->GetPreferences().GetColor("Appearance.Filter Graph.disabled_port_color");
	auto line_highlight_color = m_parent->GetPreferences().GetColor("Appearance.Filter Graph.line_highlight_color");

	if(this == m_parent->GetSelectedNode() )
		outline_color = line_highlight_color;

	//This is a bit messy... but there's no other good way to figure out what type of input a port wants!
	OscilloscopeChannel dummy_analog(
		NULL, "", "", Unit(Unit::UNIT_FS), Unit(Unit::UNIT_VOLTS), Stream::STREAM_TYPE_ANALOG);
	OscilloscopeChannel dummy_digital(
		NULL, "", "", Unit(Unit::UNIT_COUNTS), Unit(Unit::UNIT_VOLTS), Stream::STREAM_TYPE_DIGITAL);

	cr->save();
		cr->translate(m_rect.get_left(), m_rect.get_top());
		cr->set_line_width(2);

		//Box background
		cr->set_source_rgba(fill_color.get_red_p(), fill_color.get_green_p(), fill_color.get_blue_p(), 1);
		cr->move_to(0,					0);
		cr->line_to(m_rect.get_width(),	0);
		cr->line_to(m_rect.get_width(),	m_rect.get_height());
		cr->line_to(0, 					m_rect.get_height());
		cr->line_to(0, 					0);
		cr->fill();

		//Title background (in channel color)
		cr->set_source_rgba(channel_color.get_red_p(), channel_color.get_green_p(), channel_color.get_blue_p(), 1);
		cr->move_to(0,					0);
		cr->line_to(m_rect.get_width(),	0);
		cr->line_to(m_rect.get_width(),	m_titleRect.get_bottom());
		cr->line_to(0, 					m_titleRect.get_bottom());
		cr->line_to(0, 					0);
		cr->fill();

		//Box outline
		cr->set_source_rgba(outline_color.get_red_p(), outline_color.get_green_p(), outline_color.get_blue_p(), 1);
		cr->move_to(0,					0);
		cr->line_to(m_rect.get_width(),	0);
		cr->line_to(m_rect.get_width(),	m_rect.get_height());
		cr->line_to(0, 					m_rect.get_height());
		cr->line_to(0, 					0);
		cr->stroke();

		//Draw input ports
		for(size_t i=0; i<m_inputPorts.size(); i++)
		{
			auto& port = m_inputPorts[i];

			//Draw the box
			cr->move_to(port.m_rect.get_left(),		port.m_rect.get_top());
			cr->line_to(port.m_rect.get_right(),	port.m_rect.get_top());
			cr->line_to(port.m_rect.get_right(),	port.m_rect.get_bottom());
			cr->line_to(port.m_rect.get_left(), 	port.m_rect.get_bottom());
			cr->line_to(port.m_rect.get_left(), 	port.m_rect.get_top());

			//See if we're on top of it
			auto prect = port.m_rect;
			prect += vec2f(m_rect.get_left(), m_rect.get_top());
			auto mouse = m_parent->GetMousePosition();
			bool mouseover = prect.HitTest(mouse.x, mouse.y);

			//Special coloring for dragging
			if( (m_parent->GetDragMode() == FilterGraphEditorWidget::DRAG_NET_SOURCE) &&
				!m_node->ValidateChannel(i, m_parent->GetSourceStream()) )
			{
				cr->set_source_rgba(
					disabled_color.get_red_p(), disabled_color.get_green_p(), disabled_color.get_blue_p(), 1);
			}
			else if( (m_parent->GetDragMode() == FilterGraphEditorWidget::DRAG_NET_SOURCE) && mouseover )
			{
				cr->set_source_rgba(
					line_highlight_color.get_red_p(),
					line_highlight_color.get_green_p(),
					line_highlight_color.get_blue_p(), 1);
			}

			//Color code by type
			else if(m_node->ValidateChannel(i, StreamDescriptor(&dummy_analog, 0)))
			{
				cr->set_source_rgba(
					analog_color.get_red_p(), analog_color.get_green_p(), analog_color.get_blue_p(), 1);
			}
			else if(m_node->ValidateChannel(i, StreamDescriptor(&dummy_digital, 0)))
			{
				cr->set_source_rgba(
					digital_color.get_red_p(), digital_color.get_green_p(), digital_color.get_blue_p(), 1);
			}
			else
			{
				cr->set_source_rgba(
					complex_color.get_red_p(), complex_color.get_green_p(), complex_color.get_blue_p(), 1);
			}
			cr->fill_preserve();

			cr->set_source_rgba(outline_color.get_red_p(), outline_color.get_green_p(), outline_color.get_blue_p(), 1);
			cr->stroke();

			//Text
			cr->set_source_rgba(text_color.get_red_p(), text_color.get_green_p(), text_color.get_blue_p(), 1);
				cr->save();
				cr->move_to(port.m_rect.get_left() + m_margin, port.m_rect.get_top());
				port.m_layout->update_from_cairo_context(cr);
				port.m_layout->show_in_cairo_context(cr);
			cr->restore();
		}

		//Draw output ports
		for(size_t i=0; i<m_outputPorts.size(); i++)
		{
			auto& port = m_outputPorts[i];

			//Draw the box
			cr->move_to(port.m_rect.get_left(),		port.m_rect.get_top());
			cr->line_to(port.m_rect.get_right(),	port.m_rect.get_top());
			cr->line_to(port.m_rect.get_right(),	port.m_rect.get_bottom());
			cr->line_to(port.m_rect.get_left(), 	port.m_rect.get_bottom());
			cr->line_to(port.m_rect.get_left(), 	port.m_rect.get_top());

			//See what type of port it is
			if( (m_parent->GetDragMode() == FilterGraphEditorWidget::DRAG_NET_SOURCE) &&
				(m_parent->GetSourceStream() != StreamDescriptor(dynamic_cast<OscilloscopeChannel*>(m_node), i)) )
			{
				cr->set_source_rgba(
					disabled_color.get_red_p(), disabled_color.get_green_p(), disabled_color.get_blue_p(), 1);
			}
			else if(chan && chan->GetType(i) == Stream::STREAM_TYPE_ANALOG)
			{
				cr->set_source_rgba(
					analog_color.get_red_p(), analog_color.get_green_p(), analog_color.get_blue_p(), 1);
			}
			else if(chan && chan->GetType(i) == Stream::STREAM_TYPE_DIGITAL)
			{
				cr->set_source_rgba(
					digital_color.get_red_p(), digital_color.get_green_p(), digital_color.get_blue_p(), 1);
			}
			else
			{
				cr->set_source_rgba(
					complex_color.get_red_p(), complex_color.get_green_p(), complex_color.get_blue_p(), 1);
			}
			cr->fill_preserve();

			cr->set_source_rgba(outline_color.get_red_p(), outline_color.get_green_p(), outline_color.get_blue_p(), 1);
			cr->stroke();

			//Text
			cr->set_source_rgba(text_color.get_red_p(), text_color.get_green_p(), text_color.get_blue_p(), 1);
				cr->save();
				cr->move_to(port.m_rect.get_left() + m_margin, port.m_rect.get_top());
				port.m_layout->update_from_cairo_context(cr);
				port.m_layout->show_in_cairo_context(cr);
			cr->restore();
		}

		//Draw filter parameters
		cr->set_source_rgba(text_color.get_red_p(), text_color.get_green_p(), text_color.get_blue_p(), 1);
		cr->save();
			cr->move_to(m_paramRect.get_x(), m_paramRect.get_y());
			m_paramLayout->update_from_cairo_context(cr);
			m_paramLayout->show_in_cairo_context(cr);
		cr->restore();

		//Draw the title
		cr->set_source_rgba(
			title_text_color.get_red_p(), title_text_color.get_green_p(), title_text_color.get_blue_p(), 1);
		cr->save();
			cr->move_to(m_titleRect.get_x(), m_titleRect.get_y());
			m_titleLayout->update_from_cairo_context(cr);
			m_titleLayout->show_in_cairo_context(cr);
		cr->restore();

	cr->restore();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

FilterGraphEditorWidget::FilterGraphEditorWidget(FilterGraphEditor* parent)
	: m_parent(parent)
	, m_channelPropertiesDialog(NULL)
	, m_filterDialog(NULL)
	, m_highlightedPath(NULL)
	, m_dragMode(DRAG_NONE)
	, m_selectedNode(NULL)
	, m_dragDeltaY(0)
	, m_sourcePort(0)
	, m_routingColumnWidth(100)
{
	add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::POINTER_MOTION_MASK);

	auto item = Gtk::manage(new Gtk::MenuItem("Delete", false));
	item->signal_activate().connect(
		sigc::mem_fun(*this, &FilterGraphEditorWidget::OnDelete));
	m_contextMenu.append(*item);

	m_contextMenu.show_all();
}

FilterGraphEditorWidget::~FilterGraphEditorWidget()
{
	for(auto it : m_nodes)
		delete it.second;
	m_nodes.clear();

	for(auto it : m_paths)
		delete it.second;
	m_paths.clear();

	for(auto col : m_columns)
		delete col;
	m_columns.clear();

	delete m_channelPropertiesDialog;
	delete m_filterDialog;
}

PreferenceManager& FilterGraphEditorWidget::GetPreferences()
{
	return m_parent->GetParent()->GetPreferences();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Top level updating

void FilterGraphEditorWidget::Refresh()
{
	while(true)
	{
		//Place
		RemoveStaleNodes();
		CreateNodes();
		UpdateSizes();
		UpdatePositions();

		//Route
		RemoveStalePaths();
		if(!CreatePaths())
		{
			//We ran out of vertical routing space, add more
			m_routingColumnWidth += 20;
			continue;
		}
		ResolvePathConflicts();

		//If we get here, all good
		break;
	}

	queue_draw();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Placement

/**
	@brief Remove any nodes corresponding to channels that no longer exist
 */
void FilterGraphEditorWidget::RemoveStaleNodes()
{
	//Start by assuming we're deleting all channels
	set<FlowGraphNode*> channelsToRemove;
	for(auto it : m_nodes)
		channelsToRemove.emplace(it.first);

	//Keep all filters
	auto filters = Filter::GetAllInstances();
	for(auto f : filters)
		channelsToRemove.erase(f);

	//Keep all scope channels
	auto w = m_parent->GetParent();
	for(size_t i=0; i<w->GetScopeCount(); i++)
	{
		auto scope = w->GetScope(i);
		for(size_t j=0; j<scope->GetChannelCount(); j++)
			channelsToRemove.erase(scope->GetChannel(j));
	}

	//Whatever is left needs to be deleted
	for(auto chan : channelsToRemove)
	{
		auto node = m_nodes[chan];
		for(auto c : m_columns)
			c->m_nodes.erase(node);

		delete chan;
		m_nodes.erase(chan);
	}
}

/**
	@brief Create display nodes for everything in the flow graph
 */
void FilterGraphEditorWidget::CreateNodes()
{
	//Add all filters
	auto filters = Filter::GetAllInstances();
	for(auto f : filters)
	{
		if(m_nodes.find(f) == m_nodes.end())
			m_nodes[f] = new FilterGraphEditorNode(this, f);
	}

	//Add all scope channels
	auto w = m_parent->GetParent();
	for(size_t i=0; i<w->GetScopeCount(); i++)
	{
		auto scope = w->GetScope(i);
		for(size_t j=0; j<scope->GetChannelCount(); j++)
		{
			auto chan = scope->GetChannel(j);
			if(chan->GetType(0) == Stream::STREAM_TYPE_TRIGGER)
				continue;

			//If the channel cannot be enabled, don't show it.
			if(!scope->CanEnableChannel(j))
				continue;

			if(m_nodes.find(chan) == m_nodes.end())
				m_nodes[chan] = new FilterGraphEditorNode(this, chan);
		}
	}
}

/**
	@brief Updates the size of each filter graph node
 */
void FilterGraphEditorWidget::UpdateSizes()
{
	for(auto it : m_nodes)
		it.second->UpdateSize();
}

/**
	@brief Un-place any nodes in illegal locations
 */
void FilterGraphEditorWidget::UnplaceMisplacedNodes()
{
	while(true)
	{
		bool madeChanges = false;

		for(auto it : m_nodes)
		{
			//Can't unplace anything already unplaced.
			auto node = it.second;
			if(!node->m_positionValid)
				continue;

			//Check each input
			for(size_t i=0; i<node->m_node->GetInputCount(); i++)
			{
				//If no input, we're not misplaced
				auto in = node->m_node->GetInput(i).m_channel;
				if(in == NULL)
					continue;

				//We have an input. See what column it's in
				auto inode = m_nodes[in];
				if( !inode->m_positionValid || (inode->m_column >= node->m_column) )
				{
					node->m_positionValid = false;
					madeChanges = true;
					break;
				}
			}
		}

		if(!madeChanges)
			break;
	}
}

/**
	@brief Figure out what column each node belongs in
 */
void FilterGraphEditorWidget::AssignNodesToColumns()
{
	//Figure out all nodes that do not currently have assigned positions
	set<FilterGraphEditorNode*> unassignedNodes;
	for(auto it : m_nodes)
	{
		if(!it.second->m_positionValid)
			unassignedNodes.emplace(it.second);
	}

	//Create initial routing column
	if(m_columns.empty())
		m_columns.push_back(new FilterGraphRoutingColumn);

	//First, place physical channels
	set<FilterGraphEditorNode*> assignedNodes;
	for(auto node : unassignedNodes)
	{
		auto chan = dynamic_cast<OscilloscopeChannel*>(node->m_node);
		if(chan && chan->IsPhysicalChannel() )
		{
			node->m_column = 0;
			m_columns[0]->m_nodes.emplace(node);
			assignedNodes.emplace(node);
		}
	}
	for(auto node : assignedNodes)
		unassignedNodes.erase(node);
	assignedNodes.clear();

	//Nodes left to assign
	set<FlowGraphNode*> unassignedChannels;
	for(auto node : unassignedNodes)
		unassignedChannels.emplace(node->m_node);

	//Anything that has *no* inputs goes in column 0, since they're probably signal generators or scope channels
	set<FilterGraphEditorNode*> nextNodes;
	for(auto node : unassignedNodes)
	{
		if(node->m_node->GetInputCount() == 0)
		{
			node->m_column = 0;
			m_columns[0]->m_nodes.emplace(node);
			nextNodes.emplace(node);
		}
	}
	for(auto node : nextNodes)
	{
		unassignedNodes.erase(node);
		unassignedChannels.erase(node->m_node);
	}

	int ncol = 1;
	while(!unassignedNodes.empty())
	{
		//Make a new column if needed
		if(m_columns.size() <= (size_t)ncol)
			m_columns.push_back(new FilterGraphRoutingColumn);

		//Find all nodes which live exactly one column to our right.
		nextNodes.clear();
		for(auto node : unassignedNodes)
		{
			//Check if we have any inputs that are still in the working set.
			bool ok = true;
			for(size_t i=0; i<node->m_node->GetInputCount(); i++)
			{
				//If no input, we can put it anywhere
				auto in = node->m_node->GetInput(i).m_channel;
				if(in == NULL)
					continue;

				//Check if it's in a previous column
				if(unassignedChannels.find(in) != unassignedChannels.end())
				{
					ok = false;
					break;
				}

				//Also check *assigned* inputs to see if they're in the same or a rightmost column
				if(m_nodes[in]->m_positionValid && (m_nodes[in]->m_column >= ncol) )
				{
					ok = false;
					break;
				}
			}

			//All inputs are in previous columns
			if(ok)
				nextNodes.emplace(node);
		}

		//Assign positions
		for(auto node : nextNodes)
		{
			node->m_column = ncol;
			m_columns[ncol]->m_nodes.emplace(node);
		}

		//Remove working set
		for(auto node : nextNodes)
		{
			unassignedNodes.erase(node);
			unassignedChannels.erase(node->m_node);
		}

		ncol ++;
	}
}

/**
	@brief Calculate width and spacing of each column
 */
void FilterGraphEditorWidget::UpdateColumnPositions()
{
	const int left_margin			= 5;
	const int routing_column_width	= m_routingColumnWidth;
	const int routing_margin		= 10;
	const int col_route_spacing		= 10;

	//Adjust column spacing and node widths
	int left = left_margin;
	for(size_t i=0; i<m_columns.size(); i++)
	{
		auto col = m_columns[i];

		//Find width of the nodes left of the routing column, and align them to our left edge
		int width = 0;
		for(auto node : col->m_nodes)
		{
			width = max(node->m_rect.get_width(), width);
			node->m_rect.set_x(left);
		}

		//Set the column position
		col->m_left = left + width + routing_margin;
		col->m_right = col->m_left + routing_column_width;

		//Position the next column
		left = col->m_right + routing_margin;
	}

	//Create routing channels
	for(auto col : m_columns)
	{
		col->m_freeVerticalChannels.clear();
		col->m_usedVerticalChannels.clear();

		for(int i=col->m_left; i<col->m_right; i += col_route_spacing)
			col->m_freeVerticalChannels.push_back(i);
	}

	//Assign vertical positions to any unplaced nodes
	for(auto col : m_columns)
	{
		//Analog first
		set<FilterGraphEditorNode*> nodes;
		for(auto node : col->m_nodes)
		{
			auto chan = dynamic_cast<OscilloscopeChannel*>(node->m_node);
			if(chan && !node->m_positionValid && (chan->GetType(0) == Stream::STREAM_TYPE_ANALOG))
				nodes.emplace(node);
		}
		AssignInitialPositions(nodes);

		//Then any remaining unplaced nodes
		nodes.clear();
		for(auto node : col->m_nodes)
		{
			if(!node->m_positionValid)
				nodes.emplace(node);
		}
		AssignInitialPositions(nodes);
	}
}

/**
	@brief Assigns initial positions to each graph node
 */
void FilterGraphEditorWidget::UpdatePositions()
{
	UnplaceMisplacedNodes();
	AssignNodesToColumns();
	UpdateColumnPositions();

	//Calculate overall size
	int right = 0;
	int bottom = 0;
	for(auto it : m_nodes)
	{
		auto node = it.second;
		right = max(node->m_rect.get_right(), right);
		bottom = max(node->m_rect.get_bottom(), bottom);
	}
	right += 20;
	bottom += 20;

	set_size_request(right, bottom);
}

void FilterGraphEditorWidget::AssignInitialPositions(set<FilterGraphEditorNode*>& nodes)
{
	//Start just below the top edge (with a bit of margin)
	for(auto node : nodes)
		node->m_rect.set_y(5);

	for(auto node : nodes)
	{
		while(true)
		{
			int hitpos = 0;

			//Check if we collided with anything
			bool hit = false;
			for(auto it : m_nodes)
			{
				//Don't collide with ourself, or any un-placed node
				if(it.second == node)
					continue;
				if(!it.second->m_positionValid)
					continue;

				if(it.second->m_rect.intersects(node->m_rect))
				{
					hit = true;
					hitpos = it.second->m_rect.get_bottom();
					break;
				}
			}

			//If not, done
			if(!hit)
				break;

			//We hit something. Move us down and try again
			node->m_rect.set_y(hitpos + 40);
		}

		node->m_positionValid = true;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Routing

void FilterGraphEditorWidget::RemoveStalePaths()
{
	//Find ones we don't want
	set<NodePort> pathsToDelete;
	for(auto it : m_paths)
	{
		auto path = it.second;

		//Check if we have a node for the source/dest.
		//If either node no longer exists, don't check for connectivity or deref any pointers
		//(as the nodes they refer to don't exist anymore)
		if( (m_nodes.find(path->m_fromNode->m_node) == m_nodes.end()) ||
			(m_nodes.find(path->m_toNode->m_node) == m_nodes.end()) )
		{
			pathsToDelete.emplace(it.first);
			continue;
		}

		auto input = path->m_toNode->m_node->GetInput(path->m_toPort);
		if(input != StreamDescriptor(dynamic_cast<OscilloscopeChannel*>(path->m_fromNode->m_node), path->m_fromPort))
			pathsToDelete.emplace(it.first);
	}

	//Remove them
	for(auto p : pathsToDelete)
	{
		if(m_paths[p] == m_highlightedPath)
			m_highlightedPath = NULL;
		delete m_paths[p];
		m_paths.erase(p);
	}

	//Remove existing routing from all paths (we re-autoroute everything each update)
	for(auto it : m_paths)
		it.second->m_polyline.clear();
}

bool FilterGraphEditorWidget::CreatePaths()
{
	//Loop over all nodes and figure out what the inputs go to.
	for(auto it : m_nodes)
	{
		auto node = it.second;
		auto& ports = node->GetInputPorts();
		for(size_t i=0; i<ports.size(); i++)
		{
			//If there's nothing connected, nothing to do
			auto input = node->m_node->GetInput(i);
			if(!input)
				continue;

			//We have an input. Add a path for it.
			auto path = new FilterGraphEditorPath(m_nodes[input.m_channel], input.m_stream, node, i);
			m_paths[NodePort(node, i)] = path;
			if(!RoutePath(path))
				return false;
		}
	}

	return true;
}

/**
	@brief Simple greedy pathfinding algorithm, one column at a time
 */
bool FilterGraphEditorWidget::RoutePath(FilterGraphEditorPath* path)
{
	const int clearance = 5;

	auto fromport = path->m_fromNode->GetOutputPorts()[path->m_fromPort];
	auto toport = path->m_toNode->GetInputPorts()[path->m_toPort];

	auto fromrect = fromport.m_rect;
	fromrect += vec2f(path->m_fromNode->m_rect.get_x(), path->m_fromNode->m_rect.get_y());
	auto torect = toport.m_rect;
	torect += vec2f(path->m_toNode->m_rect.get_x(), path->m_toNode->m_rect.get_y());

	//Get start/end points
	vec2f start;
	vec2f end;
	start = vec2f(fromrect.get_right(), fromrect.get_top() + fromrect.get_height()/2);
	end = vec2f(torect.get_left(), torect.get_top() + torect.get_height()/2);

	StreamDescriptor stream(dynamic_cast<OscilloscopeChannel*>(path->m_fromNode->m_node), path->m_fromPort);

	//Begin at the starting point
	path->m_polyline.push_back(start);

	int y = start.y;
	for(int col = path->m_fromNode->m_column; col < path->m_toNode->m_column; col++)
	{
		auto fromcol = m_columns[col];

		//Horizontal segment into the column
		int x = fromcol->GetVerticalChannel(stream);
		path->m_polyline.push_back(vec2f(x, y));

		if(x < 0)
			return false;

		//If we have more hops to do, find a horizontal routing channel
		if(col+1 < path->m_toNode->m_column)
		{
			//Find the set of nodes we could potentially collide with
			auto tocol = m_columns[col+1];
			auto& targets = tocol->m_nodes;

			//Find a free horizontal routing channel going from this column to the one to its right.
			//Always go down, never up.
			int ychan = start.y;
			while(true)
			{
				bool hit = false;
				for(auto target : targets)
				{
					auto expanded = target->m_rect;
					expanded.expand(clearance, clearance);

					if(expanded.HitTestY(ychan))
					{
						hit = true;
						break;
					}
				}

				if(hit)
					ychan += 5;
				else
					break;
			}

			//Vertical segment to the horizontal leg
			path->m_polyline.push_back(vec2f(x, ychan));

			//Horizontal segment to the next column
			x = tocol->GetVerticalChannel(stream);
			if(x < 0)
				return false;
			path->m_polyline.push_back(vec2f(x, ychan));

			y = ychan;
		}

		//Last column: vertical segment to the destination node
		else
			path->m_polyline.push_back(vec2f(x, end.y));
	}

	//Route the path
	path->m_polyline.push_back(end);

	return true;
}

/**
	@brief Find cases of overlapping line segments and fix them.
 */
void FilterGraphEditorWidget::ResolvePathConflicts()
{
	//We ensure that collisions in the vertical routing channels cannot happen by design.
	//The only possible collisions are horizontal between columns, or horizontal within a column.

	for(auto it : m_paths)
	{
		//Check each segment individually.
		//We always have an even number of points in the line, forming an odd number of segments.
		//The first segment is always horizontal, then we alternate vertical and horizontal.
		auto path = it.second;
		for(size_t i=0; i<path->m_polyline.size(); i+= 2)
		{
			bool collision_found = false;
			for(size_t iter=0; iter<5; iter ++)
			{
				int first_y = path->m_polyline[i].y;
				int left = path->m_polyline[i].x;
				int right = path->m_polyline[i+1].x;

				//Check against all other paths
				for(auto jt : m_paths)
				{
					auto target = jt.second;
					if(target == path)
						continue;

					for(size_t j=0; j<target->m_polyline.size(); j+= 2)
					{
						//Collisions with the same net are OK
						if( (target->m_fromNode == path->m_fromNode) && (target->m_fromPort == path->m_fromPort) )
							continue;

						//Different row? Skip.
						//Consider very close approaches to be collisions.
						int second_y = target->m_polyline[j].y;
						if(abs(second_y - first_y) > 3)
							continue;

						//Same row, check for X collision
						if( (target->m_polyline[j+1].x < left) || (target->m_polyline[j].x > right) )
							continue;

						//Found a collision. Now we need to avoid it.
						//For now, very simple strategy: move our segment down a bunch and try again.
						const int step = 10;
						int newy = first_y + step;
						path->m_polyline[i].y = newy;
						path->m_polyline[i+1].y = newy;

						//If we're the last segment in the net, add another little segment to patch up the end
						if( (i+2 >= path->m_polyline.size()) && (path->m_polyline.size() < 20) )
						{
							int cornerx = right - step;
							path->m_polyline[i+1].x = cornerx;
							path->m_polyline.push_back(vec2f(cornerx, first_y));
							path->m_polyline.push_back(vec2f(right, first_y));
						}

						collision_found = true;
						break;
					}
				}

				if(!collision_found)
					break;
			}
		}
	}
}

void FilterGraphEditorWidget::OnNodeDeleted(FilterGraphEditorNode* node)
{
	m_columns[node->m_column]->m_nodes.erase(node);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool FilterGraphEditorWidget::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
	auto w = get_width();
	auto h = get_height();

	//Clear the background
	auto bgcolor = GetPreferences().GetColor("Appearance.Filter Graph.background_color");
	cr->set_source_rgba(bgcolor.get_red_p(), bgcolor.get_green_p(), bgcolor.get_blue_p(), 1);
	cr->move_to(0, 0);
	cr->line_to(w, 0);
	cr->line_to(w, h);
	cr->line_to(0, h);
	cr->fill();

	//Draw each node
	for(auto it : m_nodes)
		it.second->Render(cr);

	//Draw the node being dragged last (if there is one) so it shows up in the foreground
	if(m_dragMode == DRAG_NODE)
		m_selectedNode->Render(cr);

	//Draw all paths
	//const int dot_radius = 3;
	auto linecolor = GetPreferences().GetColor("Appearance.Filter Graph.line_color");
	auto hlinecolor = GetPreferences().GetColor("Appearance.Filter Graph.line_highlight_color");
	for(auto it : m_paths)
	{
		auto path = it.second;

		//Draw highlighted net in a different color
		if( (m_highlightedPath != NULL) &&
			(path->m_fromNode == m_highlightedPath->m_fromNode) &&
			(path->m_fromPort == m_highlightedPath->m_fromPort) )
		{
			cr->set_source_rgba(hlinecolor.get_red_p(), hlinecolor.get_green_p(), hlinecolor.get_blue_p(), 1);
		}
		else
			cr->set_source_rgba(linecolor.get_red_p(), linecolor.get_green_p(), linecolor.get_blue_p(), 1);

		//Draw the lines
		cr->move_to(path->m_polyline[0].x, path->m_polyline[0].y);
		for(size_t i=1; i<path->m_polyline.size(); i++)
			cr->line_to(path->m_polyline[i].x, path->m_polyline[i].y);
		cr->stroke();

		//Dot joiners
		//TODO: only at positions where multiple paths meet?
		/*
		for(size_t i=1; i<path->m_polyline.size()-1; i++)
		{
			cr->arc(path->m_polyline[i].x, path->m_polyline[i].y, dot_radius, 0, 2*M_PI);
			cr->fill();
		}
		*/
	}

	//Draw the in-progress net
	if(m_dragMode == DRAG_NET_SOURCE)
	{
		vector<double> dashes;
		dashes.push_back(5);
		dashes.push_back(5);
		cr->set_source_rgba(hlinecolor.get_red_p(), hlinecolor.get_green_p(), hlinecolor.get_blue_p(), 1);
		cr->set_dash(dashes, 0);

		//Find the start position
		auto sourcerect = m_selectedNode->GetOutputPorts()[m_sourcePort].m_rect;
		cr->move_to(
			sourcerect.get_right() + m_selectedNode->m_rect.get_x(),
			sourcerect.get_top() + sourcerect.get_height()/2 + m_selectedNode->m_rect.get_y());
		cr->line_to(m_mousePosition.x, m_mousePosition.y);
		cr->stroke();

		cr->unset_dash();
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

bool FilterGraphEditorWidget::on_button_press_event(GdkEventButton* event)
{
	if(event->type == GDK_BUTTON_PRESS)
	{
		if(event->button == 1)
			OnLeftClick(event);
		else if(event->button == 3)
			OnRightClick(event);
	}

	if(event->type == GDK_2BUTTON_PRESS)
		OnDoubleClick(event);

	return true;
}

void FilterGraphEditorWidget::OnLeftClick(GdkEventButton* event)
{
	auto node = HitTestNode(event->x, event->y);

	switch(m_dragMode)
	{
		//Complete drawing a net
		case DRAG_NET_SOURCE:
			{
				//Get the source signal
				auto source = GetSourceStream();

				//Make sure we clicked on a destination port
				if(!node)
					return;
				auto f = dynamic_cast<Filter*>(node->m_node);
				auto dest = HitTestNodeInput(event->x, event->y);
				if(!dest)
					return;

				//Configure the input, if it's legal
				if(node->m_node->ValidateChannel(dest->m_index, source))
				{
					node->m_node->SetInput(dest->m_index, source);

					if(f)
					{
						if(f->IsUsingDefaultName())
							f->UseDefaultName(true);

						m_parent->GetParent()->RefreshAllFilters();
						m_parent->GetParent()->RefreshAllViews();
					}

					Refresh();
				}

				m_dragMode = DRAG_NONE;
				queue_draw();
			}
			break;

		//Normal
		default:
			{
				m_selectedNode = node;

				if(node)
				{
					//We clicked on a node. Where on it?
					auto source = HitTestNodeOutput(event->x, event->y);

					//Start drawing a net from a source
					if(source != NULL)
					{
						m_dragMode = DRAG_NET_SOURCE;
						m_sourcePort = source->m_index;
					}

					//Start dragging the node
					else
					{
						m_dragMode = DRAG_NODE;
						m_dragDeltaY = event->y - node->m_rect.get_y();
					}

					m_highlightedPath = NULL;
				}

				queue_draw();
			}
			break;

	}
}

void FilterGraphEditorWidget::OnRightClick(GdkEventButton* event)
{
	//Update display with the node we clicked on highlighted
	m_selectedNode = HitTestNode(event->x, event->y);
	m_dragMode = DRAG_NONE;
	queue_draw();

	m_contextMenu.popup(event->button, event->time);
}

bool FilterGraphEditorWidget::on_button_release_event(GdkEventButton* event)
{
	//ignore anything but left button
	if(event->button != 1)
		return true;

	switch(m_dragMode)
	{
		case DRAG_NET_SOURCE:
			//no action
			break;

		//We just finished dragging a node
		case DRAG_NODE:
			{
				//Target for collision detection (allow a bit of margin beyond the actual box bounds)
				auto target = m_selectedNode->m_rect;
				target.expand(0, 5);

				//If we collided with anything, unplace it and reassign to the first free space it fits in
				auto cols = m_columns[m_selectedNode->m_column];
				for(auto node : cols->m_nodes)
				{
					if(node == m_selectedNode)
						continue;

					if(node->m_rect.intersects(target))
						node->m_positionValid = false;
				}

				//Done
				m_dragMode = DRAG_NONE;
				Refresh();
			}
			break;

		default:
			break;
	}

	return true;
}

bool FilterGraphEditorWidget::on_motion_notify_event(GdkEventMotion* event)
{
	m_mousePosition = vec2f(event->x, event->y);

	switch(m_dragMode)
	{
		//Draw net
		case DRAG_NET_SOURCE:
			queue_draw();
			break;

		//Move the node
		case DRAG_NODE:
			m_highlightedPath = NULL;
			m_selectedNode->m_rect.set_y(event->y - m_dragDeltaY);
			Refresh();
			break;

		//Highlight paths when we mouse over them
		case DRAG_NONE:
			{
				auto path = HitTestPath(event->x, event->y);
				if(path != m_highlightedPath)
				{
					m_highlightedPath = path;
					queue_draw();
				}
			}
			break;

		default:
			break;
	}

	return true;
}

void FilterGraphEditorWidget::OnDoubleClick(GdkEventButton* event)
{
	//See what we hit
	auto node = HitTestNode(event->x, event->y);
	if(!node)
		return;

	auto f = dynamic_cast<Filter*>(node->m_node);
	auto chan = dynamic_cast<OscilloscopeChannel*>(node->m_node);
	if(f)
	{
		if(m_filterDialog)
			delete m_filterDialog;
		m_filterDialog = new FilterDialog(m_parent->GetParent(), f, StreamDescriptor(NULL, 0));
		m_filterDialog->signal_delete_event().connect(
			sigc::mem_fun(*this, &FilterGraphEditorWidget::OnFilterPropertiesDialogClosed));
		m_filterDialog->show();
	}

	else if(chan)
	{
		if(m_channelPropertiesDialog)
			delete m_channelPropertiesDialog;
		m_channelPropertiesDialog = new ChannelPropertiesDialog(m_parent->GetParent(), chan);
		m_channelPropertiesDialog->signal_response().connect(
			sigc::mem_fun(*this, &FilterGraphEditorWidget::OnChannelPropertiesDialogResponse));
		m_channelPropertiesDialog->show();
	}

	//In either case, once we open the properties dialog, we should not be dragging anything
	m_dragMode = DRAG_NONE;
}

bool FilterGraphEditorWidget::OnFilterPropertiesDialogClosed(GdkEventAny* /*ignored*/)
{
	//Apply the changes
	auto window = m_parent->GetParent();
		auto f = m_filterDialog->GetFilter();
	auto name = f->GetDisplayName();

	m_filterDialog->ConfigureDecoder();

	if(name != f->GetDisplayName())
		window->OnChannelRenamed(f);

	window->OnAllWaveformsUpdated();

	Refresh();

	delete m_filterDialog;
	m_filterDialog = NULL;

	return false;
}

void FilterGraphEditorWidget::OnChannelPropertiesDialogResponse(int response)
{
	if(response == Gtk::RESPONSE_OK)
	{
		auto window = m_parent->GetParent();
		auto chan = m_channelPropertiesDialog->GetChannel();
		auto name = chan->GetDisplayName();

		m_channelPropertiesDialog->ConfigureChannel();

		if(name != chan->GetDisplayName())
			window->OnChannelRenamed(chan);

		//TODO: redraw any waveform areas it contains
		//SetGeometryDirty();
		//queue_draw();

		Refresh();
	}

	delete m_channelPropertiesDialog;
	m_channelPropertiesDialog = NULL;
}

StreamDescriptor FilterGraphEditorWidget::GetSourceStream()
{
	if(m_dragMode != DRAG_NET_SOURCE)
		return StreamDescriptor(NULL, 0);

	else
		return StreamDescriptor(dynamic_cast<OscilloscopeChannel*>(m_selectedNode->m_node), m_sourcePort);
}

void FilterGraphEditorWidget::OnDelete()
{
	LogDebug("delete\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input helpers

/**
	@brief Find the node, if any, a position is on
 */
FilterGraphEditorNode* FilterGraphEditorWidget::HitTestNode(int x, int y)
{
	for(auto it : m_nodes)
	{
		if(it.second->m_rect.HitTest(x, y))
			return it.second;
	}

	return NULL;
}

/**
	@brief Find the path, if any, a position is on
 */
FilterGraphEditorPath* FilterGraphEditorWidget::HitTestPath(int x, int y)
{
	int clearance = 2;

	for(auto it : m_paths)
	{
		auto path = it.second;
		for(size_t i=0; i<path->m_polyline.size() - 1; i++)
		{
			//Check each segment
			int left = min(path->m_polyline[i].x, path->m_polyline[i+1].x);
			int right = max(path->m_polyline[i].x, path->m_polyline[i+1].x);
			int top = min(path->m_polyline[i].y, path->m_polyline[i+1].y);
			int bottom = max(path->m_polyline[i].y, path->m_polyline[i+1].y);

			if( (x+clearance < left) || (x-clearance > right) )
				continue;

			if( (y+clearance < top) || (y-clearance > bottom) )
				continue;

			return path;
		}
	}

	return NULL;
}

/**
	@brief Find the output port, if any, a position is on
 */
FilterGraphEditorPort* FilterGraphEditorWidget::HitTestNodeOutput(int x, int y)
{
	//Get the node-relative coordinates. If no node, we're obviously not on a port.
	auto node = HitTestNode(x, y);
	if(!node)
		return NULL;
	auto relx = x - node->m_rect.get_x();
	auto rely = y - node->m_rect.get_y();

	//Check each port in sequence
	auto& ports = node->GetOutputPorts();
	for(auto &port : ports)
	{
		if(port.m_rect.HitTest(relx, rely))
			return &port;
	}

	return NULL;
}

/**
	@brief Find the input port, if any, a position is on
 */
FilterGraphEditorPort* FilterGraphEditorWidget::HitTestNodeInput(int x, int y)
{
	//Get the node-relative coordinates. If no node, we're obviously not on a port.
	auto node = HitTestNode(x, y);
	if(!node)
		return NULL;
	auto relx = x - node->m_rect.get_x();
	auto rely = y - node->m_rect.get_y();

	//Check each port in sequence
	auto& ports = node->GetInputPorts();
	for(auto &port : ports)
	{
		if(port.m_rect.HitTest(relx, rely))
			return &port;
	}

	return NULL;
}
