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
	@brief Implementation of FilterGraphEditorWidget
 */
#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "FilterGraphEditorWidget.h"
#include "FilterGraphEditor.h"

using namespace std;

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

FilterGraphEditorNode::FilterGraphEditorNode(FilterGraphEditorWidget* parent, OscilloscopeChannel* chan)
	: m_parent(parent)
	, m_channel(chan)
	, m_positionValid(false)
	, m_margin(2)
	, m_column(0)
{

}

FilterGraphEditorNode::~FilterGraphEditorNode()
{
	m_parent->OnNodeDeleted(this);
}

void FilterGraphEditorNode::UpdateSize()
{
	auto headerfont = m_parent->GetPreferences().GetFont("Appearance.Filter Graph.node_name_font");
	auto portfont = m_parent->GetPreferences().GetFont("Appearance.Filter Graph.port_font");

	//Channel name text
	int twidth, theight;
	m_titleLayout = Pango::Layout::create(m_parent->get_pango_context());
	m_titleLayout->set_font_description(headerfont);
	m_titleLayout->set_text(m_channel->GetDisplayName());
	m_titleLayout->get_pixel_size(twidth, theight);

	//Title box
	m_titleRect.set_x(m_margin);
	m_titleRect.set_y(m_margin);
	m_titleRect.set_width(twidth + 2*m_margin);
	m_titleRect.set_height(theight + 2*m_margin);

	int bottom = m_titleRect.get_bottom();
	int right = twidth + 2*m_margin;

	//Input ports
	auto filter = dynamic_cast<Filter*>(m_channel);
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
			port.m_rect.set_y(bottom);
			port.m_rect.set_width(twidth + 2*m_margin);
			port.m_rect.set_height(theight + 2*m_margin);

			bottom = port.m_rect.get_bottom() + 2*m_margin;

			m_inputPorts.push_back(port);
		}
	}

	//Normalize input ports to all have the same width
	int w = 0;
	for(auto& p : m_inputPorts)
		w = max(w, p.m_rect.get_width());
	for(auto& p : m_inputPorts)
		p.m_rect.set_width(w);

	//Output ports
	int y = m_titleRect.get_bottom();
	for(size_t i=0; i<m_channel->GetStreamCount(); i++)
	{
		FilterGraphEditorPort port;
		port.m_label = m_channel->GetStreamName(i);
		port.m_layout = Pango::Layout::create(m_parent->get_pango_context());
		port.m_layout->set_font_description(portfont);
		port.m_layout->set_text(port.m_label);
		port.m_layout->get_pixel_size(twidth, theight);

		port.m_rect.set_x(0);
		port.m_rect.set_y(y);
		port.m_rect.set_width(twidth + 2*m_margin);
		port.m_rect.set_height(theight + 2*m_margin);

		y = port.m_rect.get_bottom() + 2*m_margin;

		m_outputPorts.push_back(port);
	}
	bottom = max(y, bottom);

	//Normalize output ports to the same width
	int rw = 0;
	for(auto& p : m_outputPorts)
		rw = max(rw, p.m_rect.get_width());
	for(auto& p : m_outputPorts)
		p.m_rect.set_width(rw);

	//Calculate overall width
	const int in_out_spacing = 25;
	int width1 = w + rw + in_out_spacing;
	right = max(right, width1);
	int outleft = right - rw;

	//TODO: display of parameters?

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

	auto analog_color = m_parent->GetPreferences().GetColor("Appearance.Filter Graph.analog_port_color");
	auto complex_color = m_parent->GetPreferences().GetColor("Appearance.Filter Graph.complex_port_color");
	auto digital_color = m_parent->GetPreferences().GetColor("Appearance.Filter Graph.digital_port_color");

	//This is a bit messy... but there's no other good way to figure out what type of input a port wants!
	OscilloscopeChannel dummy_analog(NULL, "", OscilloscopeChannel::CHANNEL_TYPE_ANALOG, "");
	OscilloscopeChannel dummy_digital(NULL, "", OscilloscopeChannel::CHANNEL_TYPE_DIGITAL, "");

	cr->save();
		cr->translate(m_rect.get_left(), m_rect.get_top());
		cr->set_line_width(2);

		//Draw the box
		cr->move_to(0,					0);
		cr->line_to(m_rect.get_width(),	0);
		cr->line_to(m_rect.get_width(),	m_rect.get_height());
		cr->line_to(0, 					m_rect.get_height());
		cr->line_to(0, 					0);

		cr->set_source_rgba(fill_color.get_red_p(), fill_color.get_green_p(), fill_color.get_blue_p(), 1);
		cr->fill_preserve();

		cr->set_source_rgba(outline_color.get_red_p(), outline_color.get_green_p(), outline_color.get_blue_p(), 1);
		cr->stroke();

		//Draw input ports
		for(size_t i=0; i<m_inputPorts.size(); i++)
		{
			auto& port = m_inputPorts[i];
			auto f = dynamic_cast<Filter*>(m_channel);

			//Draw the box
			cr->move_to(port.m_rect.get_left(),		port.m_rect.get_top());
			cr->line_to(port.m_rect.get_right(),	port.m_rect.get_top());
			cr->line_to(port.m_rect.get_right(),	port.m_rect.get_bottom());
			cr->line_to(port.m_rect.get_left(), 	port.m_rect.get_bottom());
			cr->line_to(port.m_rect.get_left(), 	port.m_rect.get_top());

			//See what type of port it is
			if(f->ValidateChannel(i, StreamDescriptor(&dummy_analog, 0)))
			{
				cr->set_source_rgba(
					analog_color.get_red_p(), analog_color.get_green_p(), analog_color.get_blue_p(), 1);
			}
			else if(f->ValidateChannel(i, StreamDescriptor(&dummy_digital, 0)))
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
			if(m_channel->GetType() == OscilloscopeChannel::CHANNEL_TYPE_ANALOG)
			{
				cr->set_source_rgba(
					analog_color.get_red_p(), analog_color.get_green_p(), analog_color.get_blue_p(), 1);
			}
			else if(m_channel->GetType() == OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
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

		//Draw the title
		cr->set_source_rgba(text_color.get_red_p(), text_color.get_green_p(), text_color.get_blue_p(), 1);
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
{

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
}

PreferenceManager& FilterGraphEditorWidget::GetPreferences()
{
	return m_parent->GetParent()->GetPreferences();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Top level updating

void FilterGraphEditorWidget::Refresh()
{
	//Place
	RemoveStaleNodes();
	CreateNodes();
	UpdateSizes();
	UpdatePositions();

	//Route
	RemoveStalePaths();
	CreatePaths();

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
	set<OscilloscopeChannel*> channelsToRemove;
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
		m_nodes.erase(chan);
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
			if(chan->GetType() == OscilloscopeChannel::CHANNEL_TYPE_TRIGGER)
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
	@brief Assigns initial positions to each graph node
 */
void FilterGraphEditorWidget::UpdatePositions()
{
	const int left_margin = 5;
	const int column_spacing = 75;
	const int routing_margin = 10;

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

	//First, place physical analog channels
	set<FilterGraphEditorNode*> assignedNodes;
	for(auto node : unassignedNodes)
	{
		if(node->m_channel->IsPhysicalChannel() &&
			(node->m_channel->GetType() == OscilloscopeChannel::CHANNEL_TYPE_ANALOG) )
		{
			node->m_rect.set_x(left_margin);
			node->m_column = 0;
			m_columns[0]->m_nodes.emplace(node);
			assignedNodes.emplace(node);
		}
	}
	AssignInitialPositions(assignedNodes);
	for(auto node : assignedNodes)
		unassignedNodes.erase(node);
	assignedNodes.clear();

	//Then other physical channels
	for(auto node : unassignedNodes)
	{
		if(node->m_channel->IsPhysicalChannel() )
		{
			node->m_rect.set_x(left_margin);
			node->m_column = 0;
			m_columns[0]->m_nodes.emplace(node);
			assignedNodes.emplace(node);
		}
	}
	AssignInitialPositions(assignedNodes);
	for(auto node : assignedNodes)
		unassignedNodes.erase(node);
	assignedNodes.clear();

	//Figure out the width of our widest physical node
	int physical_right = 0;
	for(auto it : m_nodes)
	{
		if(!it.second->m_positionValid || !it.second->m_channel->IsPhysicalChannel())
			continue;
		physical_right = max(physical_right, it.second->m_rect.get_right());
	}
	int next_left = physical_right + column_spacing;

	//Create/update routing column
	m_columns[0]->m_left = physical_right + routing_margin;
	m_columns[0]->m_right = next_left - routing_margin;

	//Filters left to assign
	set<OscilloscopeChannel*> unassignedChannels;
	for(auto node : unassignedNodes)
		unassignedChannels.emplace(node->m_channel);

	size_t ncol = 1;
	while(!unassignedNodes.empty())
	{
		//Make a new column if needed
		if(m_columns.size() <= ncol)
			m_columns.push_back(new FilterGraphRoutingColumn);

		//Find all nodes which live exactly one column to our right.
		set<FilterGraphEditorNode*> nextNodes;
		for(auto node : unassignedNodes)
		{
			//Check if we have any inputs that are still in the working set.
			bool ok = true;
			auto d = dynamic_cast<Filter*>(node->m_channel);
			for(size_t i=0; i<d->GetInputCount(); i++)
			{
				auto in = d->GetInput(i).m_channel;
				if(unassignedChannels.find(in) != unassignedChannels.end())
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
			node->m_rect.set_x(next_left);
			node->m_column = ncol;
			m_columns[ncol]->m_nodes.emplace(node);
		}
		AssignInitialPositions(nextNodes);

		//Update width
		int colwidth = 0;
		for(auto node : nextNodes)
			colwidth = max(colwidth, node->m_rect.get_width());
		next_left += colwidth;

		m_columns[ncol]->m_left = next_left + routing_margin;
		next_left += column_spacing;
		m_columns[ncol]->m_right = next_left - routing_margin;

		//Remove working set
		for(auto node : nextNodes)
		{
			unassignedNodes.erase(node);
			unassignedChannels.erase(node->m_channel);
		}

		ncol ++;
	}
}

void FilterGraphEditorWidget::AssignInitialPositions(set<FilterGraphEditorNode*>& nodes)
{
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
		auto input = dynamic_cast<Filter*>(path->m_toNode->m_channel)->GetInput(path->m_toPort);
		if(input != StreamDescriptor(path->m_fromNode->m_channel, path->m_fromPort))
			pathsToDelete.emplace(it.first);
	}

	//Remove them
	for(auto p : pathsToDelete)
	{
		delete m_paths[p];
		m_paths.erase(p);
	}
}

void FilterGraphEditorWidget::CreatePaths()
{
	//Loop over all nodes and figure out what the inputs go to.
	for(auto it : m_nodes)
	{
		auto node = it.second;
		auto& ports = node->GetInputPorts();
		for(size_t i=0; i<ports.size(); i++)
		{
			//If there's nothing connected, nothing to do
			auto input = dynamic_cast<Filter*>(node->m_channel)->GetInput(i);
			if(input.m_channel == NULL)
				continue;

			//We have an input. Add a path for it.
			auto path = new FilterGraphEditorPath(m_nodes[input.m_channel], input.m_stream, node, i);
			m_paths[NodePort(node, i)] = path;
			RoutePath(path);
		}
	}
}

/**
	@brief Simple greedy pathfinding algorithm, one column at a time
 */
void FilterGraphEditorWidget::RoutePath(FilterGraphEditorPath* path)
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

	//Begin at the starting point
	path->m_polyline.push_back(start);

	int y = start.y;
	for(int col = path->m_fromNode->m_column; col < path->m_toNode->m_column; col++)
	{
		//TODO: Find a vertical space in the channel to use
		int x = m_columns[col]->m_left;

		//Horizontal segment into the column
		path->m_polyline.push_back(vec2f(x, y));

		//If we have more hops to do, find a horizontal routing channel
		if(col+1 < path->m_toNode->m_column)
		{
			//Find the set of nodes we could potentially collide with
			auto& targets = m_columns[col+1]->m_nodes;

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

			int x2 = m_columns[col+1]->m_left;

			//Vertical segment to the horizontal leg
			path->m_polyline.push_back(vec2f(x, ychan));

			//Horizontal segment to the next column
			path->m_polyline.push_back(vec2f(x2, ychan));

			y = ychan;
		}

		//Last column: vertical segment to the destination node
		else
			path->m_polyline.push_back(vec2f(x, end.y));
	}

	//Route the path
	path->m_polyline.push_back(end);
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

	//Draw all paths
	auto linecolor = GetPreferences().GetColor("Appearance.Filter Graph.line_color");
	cr->set_source_rgba(linecolor.get_red_p(), linecolor.get_green_p(), linecolor.get_blue_p(), 1);
	for(auto it : m_paths)
	{
		auto path = it.second;
		cr->move_to(path->m_polyline[0].x, path->m_polyline[0].y);
		for(size_t i=1; i<path->m_polyline.size(); i++)
			cr->line_to(path->m_polyline[i].x, path->m_polyline[i].y);
		cr->stroke();
	}

	return true;
}
