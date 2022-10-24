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
	@brief Implementation of FilterGraphEditor
 */

#include "ngscopeclient.h"
#include "FilterGraphEditor.h"
#include "MainWindow.h"
#include "ChannelPropertiesDialog.h"
#include "FilterPropertiesDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

FilterGraphEditor::FilterGraphEditor(Session& session, MainWindow* parent)
	: Dialog("Filter Graph Editor", ImVec2(800, 600))
	, m_session(session)
	, m_parent(parent)
	, m_nextID(1)
{
	m_config.SettingsFile = "";
	m_context = ax::NodeEditor::CreateEditor(&m_config);
}

FilterGraphEditor::~FilterGraphEditor()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool FilterGraphEditor::DoRender()
{
	ax::NodeEditor::SetCurrentEditor(m_context);
	ax::NodeEditor::Begin("Filter Graph", ImVec2(0, 0));

	//Make nodes for each channel and filter
	auto& scopes = m_session.GetScopes();
	for(auto scope : scopes)
	{
		for(size_t i=0; i<scope->GetChannelCount(); i++)
			DoNodeForChannel(scope->GetChannel(i));
	}
	auto filters = Filter::GetAllInstances();
	for(auto f : filters)
		DoNodeForChannel(f);
	ClearOldPropertiesDialogs();

	//Add links from each filter input to the stream it's fed by
	for(auto f : filters)
	{
		for(size_t i=0; i<f->GetInputCount(); i++)
		{
			auto stream = f->GetInput(i);
			if(stream)
			{
				auto srcid = GetID(stream);
				auto dstid = GetID(pair<OscilloscopeChannel*, size_t>(f, i));
				auto linkid = GetID(pair<ax::NodeEditor::PinId, ax::NodeEditor::PinId>(srcid, dstid));
				ax::NodeEditor::Link(linkid, srcid, dstid);
			}
		}
	}

	//Handle creation or deletion requests
	Filter* fReconfigure = nullptr;
	HandleLinkCreationRequests(fReconfigure);
	HandleLinkDeletionRequests(fReconfigure);

	//Look for and avoid overlaps
	HandleOverlaps();

	ax::NodeEditor::End();
	ax::NodeEditor::SetCurrentEditor(nullptr);

	//Run deferred processing for combo boxes
	for(auto it : m_propertiesDialogs)
		it.second->RunDeferredComboBoxes();

	//If any filters were reconfigured, dispatch events accordingly
	if(fReconfigure)
	{
		//Update auto generated name
		if(fReconfigure->IsUsingDefaultName())
			fReconfigure->SetDefaultName();

		m_parent->OnFilterReconfigured(fReconfigure);
	}

	return true;
}

/**
	@brief Delete old properties dialogs for no-longer-extant nodes
 */
void FilterGraphEditor::ClearOldPropertiesDialogs()
{
	//Get all of the node IDs
	int nnodes = ax::NodeEditor::GetNodeCount();
	vector<ax::NodeEditor::NodeId> nodes;
	nodes.resize(nnodes);
	ax::NodeEditor::GetOrderedNodeIds(&nodes[0], nnodes);

	//Make a set we can quickly search
	set<ax::NodeEditor::NodeId, lessID<ax::NodeEditor::NodeId> > nodeset;
	for(auto n : nodes)
		nodeset.emplace(n);

	//Find any node IDs that no longer are in use
	vector<ax::NodeEditor::NodeId> idsToRemove;
	for(auto it : m_propertiesDialogs)
	{
		if(nodeset.find(it.first) == nodeset.end())
			idsToRemove.push_back(it.first);
	}

	//Remove them
	for(auto i : idsToRemove)
		m_propertiesDialogs.erase(i);
}

/**
	@brief Display tooltips when mousing over interesting stuff
 */
void FilterGraphEditor::OutputPortTooltip(StreamDescriptor stream)
{
	ImGui::BeginTooltip();
		switch(stream.GetType())
		{
			case Stream::STREAM_TYPE_ANALOG:
				ImGui::TextUnformatted("Analog output channel");
				break;

			case Stream::STREAM_TYPE_DIGITAL:
				ImGui::TextUnformatted("Digital output channel");
				break;

			case Stream::STREAM_TYPE_DIGITAL_BUS:
				ImGui::TextUnformatted("Digital bus output channel");
				break;

			case Stream::STREAM_TYPE_EYE:
				ImGui::TextUnformatted("Eye pattern");
				break;

			case Stream::STREAM_TYPE_SPECTROGRAM:
				ImGui::TextUnformatted("Spectrogram");
				break;

			case Stream::STREAM_TYPE_WATERFALL:
				ImGui::TextUnformatted("Waterfall");
				break;

			case Stream::STREAM_TYPE_PROTOCOL:
				ImGui::TextUnformatted("Protocol data");
				break;

			default:
				ImGui::TextUnformatted("Unknown channel type");
				break;
		}
	ImGui::EndTooltip();
}

/**
	@brief Find nodes that are intersecting, and apply forces to avoid collisions
 */
void FilterGraphEditor::HandleOverlaps()
{
	//Early out: if left mouse button is down we are probably dragging an item
	//Do nothing
	if(ImGui::IsMouseDown(ImGuiMouseButton_Left))
		return;

	//Get all of the node IDs
	int nnodes = ax::NodeEditor::GetNodeCount();
	vector<ax::NodeEditor::NodeId> nodes;
	nodes.resize(nnodes);
	ax::NodeEditor::GetOrderedNodeIds(&nodes[0], nnodes);

	//Loop over all nodes and find potential collisions
	for(int i=0; i<nnodes; i++)
	{
		auto nodeA = nodes[i];
		auto posA = ax::NodeEditor::GetNodePosition(nodeA);
		auto sizeA = ax::NodeEditor::GetNodeSize(nodeA);

		for(int j=0; j<nnodes; j++)
		{
			//Don't check for self intersection
			if(i == j)
				continue;

			auto nodeB = nodes[j];
			auto posB = ax::NodeEditor::GetNodePosition(nodeB);
			auto sizeB = ax::NodeEditor::GetNodeSize(nodeB);

			//If no overlap, no action required
			if(!RectIntersect(posA, sizeA, posB, sizeB))
				continue;

			//We have an overlap!
			//Find the unit vector between the node positions
			float dx = posB.x - posA.x;
			float dy = posB.y - posA.y;
			float mag = sqrt(dx*dx + dy*dy);

			//Shift both nodes away from each other
			//If magnitude is ~zero (nodes are at exactly the same position), arbitrarily move second one down or right at random
			if(mag < 1e-2f)
			{
				if(rand() & 10)
					posB.x ++;
				else
					posB.y ++;
			}

			else
			{
				float distance = 10;
				float scale = distance / mag;
				posB.x += scale * dx;
				posB.y += scale * dy;
			}

			//TODO: take paths into account?

			//Set the new node position
			ax::NodeEditor::SetNodePosition(nodeB, posB);
		}
	}
}

/**
	@brief Check if two rectangles intersect
 */
bool FilterGraphEditor::RectIntersect(ImVec2 posA, ImVec2 sizeA, ImVec2 posB, ImVec2 sizeB)
{
	//Enlarge hitboxes by a small margin to keep spacing between nodes
	float margin = 5;
	posA.x -= margin;
	posA.y -= margin;
	posB.x -= margin;
	posB.y -= margin;
	sizeA.x += 2*margin;
	sizeA.y += 2*margin;
	sizeB.x += 2*margin;
	sizeB.y += 2*margin;

	//A completely above B? No intersection
	if( (posA.y + sizeA.y) < posB.y)
		return false;

	//B completely above A? No intersection
	if( (posB.y + sizeB.y) < posA.y)
		return false;

	//A completely left of B? No intersection
	if( (posA.x + sizeA.x) < posB.x)
		return false;

	//B completely left of A? No intersection
	if( (posB.x + sizeB.x) < posA.x)
		return false;

	//If we get here, they overlap
	return true;
}

/**
	@brief Handle requests to create a new link
 */
void FilterGraphEditor::HandleLinkCreationRequests(Filter*& fReconfigure)
{
	//for some reason node editor wants colors as vec4 not ImU32
	auto& prefs = m_session.GetPreferences();
	auto validcolor = ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Filter Graph.valid_link_color"));
	auto invalidcolor = ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Filter Graph.invalid_link_color"));

	if(ax::NodeEditor::BeginCreate())
	{
		ax::NodeEditor::PinId startId, endId;
		if(ax::NodeEditor::QueryNewLink(&startId, &endId))
		{
			//If both IDs are valid, consider making the path
			if(startId && endId)
			{
				//Link creation code doesn't know start vs dest
				//If we started from the input, swap the pins
				if(m_inputIDMap.HasEntry(startId))
				{
					ax::NodeEditor::PinId tmp = startId;
					startId = endId;
					endId = tmp;
				}

				//Make sure both paths exist and it's a path from output to input
				if(m_inputIDMap.HasEntry(endId) && m_streamIDMap.HasEntry(startId))
				{
					//Get the stream and port we want to look at
					auto inputPort = m_inputIDMap[endId];
					auto stream = m_streamIDMap[startId];

					//Check for and reject back edges (creates cycles)
					if(IsBackEdge(stream.m_channel, inputPort.first))
					{
						ax::NodeEditor::RejectNewItem(invalidcolor);

						ImGui::BeginTooltip();
							ImGui::TextColored(invalidcolor, "x Cannot create loops in filter graph");
						ImGui::EndTooltip();
					}

					//See if the path is valid
					else if(inputPort.first->ValidateChannel(inputPort.second, stream))
					{
						//Yep, looks good
						ImGui::BeginTooltip();
							ImGui::TextColored(validcolor, "+ Connect Port");
						ImGui::EndTooltip();

						if(ax::NodeEditor::AcceptNewItem(validcolor))
						{
							//Hook it up
							inputPort.first->SetInput(inputPort.second, stream);

							//Update names, if needed
							fReconfigure = dynamic_cast<Filter*>(inputPort.first);
						}
					}

					//Not valid
					else
					{
						ax::NodeEditor::RejectNewItem(invalidcolor);

						ImGui::BeginTooltip();
							ImGui::TextColored(invalidcolor, "x Incompatible stream type for input");
						ImGui::EndTooltip();
					}
				}

				//Complain if both ports are input or both output
				if(m_inputIDMap.HasEntry(endId) && m_inputIDMap.HasEntry(startId))
				{
					ax::NodeEditor::RejectNewItem(invalidcolor);

					ImGui::BeginTooltip();
						ImGui::TextColored(invalidcolor, "x Cannot connect two input ports");
					ImGui::EndTooltip();
				}
				if(m_streamIDMap.HasEntry(endId) && m_streamIDMap.HasEntry(startId))
				{
					ax::NodeEditor::RejectNewItem(invalidcolor);

					ImGui::BeginTooltip();
						ImGui::TextColored(invalidcolor, "x Cannot connect two output ports");
					ImGui::EndTooltip();
				}
			}
		}

		if(ax::NodeEditor::QueryNewNode(&startId))
		{
			if(startId && m_streamIDMap.HasEntry(startId))
			{
				ImGui::BeginTooltip();
					ImGui::TextColored(validcolor, "+ Create Filter");
				ImGui::EndTooltip();

				if(ax::NodeEditor::AcceptNewItem())
				{
					ax::NodeEditor::Suspend();
					m_newFilterSourceStream = m_streamIDMap[startId];
					m_createMousePos = ImGui::GetMousePos();
					ImGui::OpenPopup("Create Filter");
					ax::NodeEditor::Resume();
				}
			}
		}
	}
	ax::NodeEditor::EndCreate();

	//Create-filter menu
	ax::NodeEditor::Suspend();
	if(ImGui::BeginPopup("Create Filter"))
	{
		FilterMenu(m_newFilterSourceStream);
		ImGui::EndPopup();
	}
	ax::NodeEditor::Resume();
}

/**
	@brief Determine if a proposed edge in the filter graph is a back edge (one whose creation would lead to a cycle)

	@param src	Source node
	@param dst	Destination node

	@return True if dst is equal to src, or if dst is directly or indirectly used as an input by src.
 */
bool FilterGraphEditor::IsBackEdge(OscilloscopeChannel* src, OscilloscopeChannel* dst)
{
	if(src == dst)
		return true;

	//Check each input of src
	auto fsrc = dynamic_cast<Filter*>(src);
	if(!fsrc)
		return false;
	for(size_t i=0; i<fsrc->GetInputCount(); i++)
	{
		auto stream = fsrc->GetInput(i);
		if(IsBackEdge(stream.m_channel, dst))
			return true;
	}

	return false;
}

/**
	@brief Runs the "create filter" menu
 */
void FilterGraphEditor::FilterMenu(StreamDescriptor stream)
{
	FilterSubmenu(stream, "Bus", Filter::CAT_BUS);
	FilterSubmenu(stream, "Clocking", Filter::CAT_CLOCK);
	FilterSubmenu(stream, "Generation", Filter::CAT_GENERATION);
	FilterSubmenu(stream, "Math", Filter::CAT_MATH);
	FilterSubmenu(stream, "Measurement", Filter::CAT_MEASUREMENT);
	FilterSubmenu(stream, "Memory", Filter::CAT_MEMORY);
	FilterSubmenu(stream, "Miscellaneous", Filter::CAT_MISC);
	FilterSubmenu(stream, "Power", Filter::CAT_POWER);
	FilterSubmenu(stream, "RF", Filter::CAT_RF);
	FilterSubmenu(stream, "Serial", Filter::CAT_SERIAL);
	FilterSubmenu(stream, "Signal integrity", Filter::CAT_ANALYSIS);
}

/**
	@brief Run the submenu for a single filter category
 */
void FilterGraphEditor::FilterSubmenu(StreamDescriptor stream, const string& name, Filter::Category cat)
{
	auto& refs = m_parent->GetSession().GetReferenceFilters();

	if(ImGui::BeginMenu(name.c_str()))
	{
		//Find all filters in this category and sort them alphabetically
		vector<string> sortedNames;
		for(auto it : refs)
		{
			if(it.second->GetCategory() == cat)
				sortedNames.push_back(it.first);
		}
		std::sort(sortedNames.begin(), sortedNames.end());

		//Do all of the menu items
		for(auto fname : sortedNames)
		{
			auto it = refs.find(fname);
			bool valid = false;
			if(it->second->GetInputCount() == 0)		//No inputs? Always valid
				valid = true;
			else
				valid = it->second->ValidateChannel(0, stream);

			//Hide import filters to avoid cluttering the UI
			if( (cat == Filter::CAT_GENERATION) && (fname.find("Import") != string::npos))
				continue;

			//TODO: measurements should have summary option

			if(ImGui::MenuItem(fname.c_str(), nullptr, false, valid))
			{
				//Make the filter but don't spawn a properties dialog for it
				auto f = m_parent->CreateFilter(fname, nullptr, stream, false);

				//Get relative mouse position
				auto mousePos = ax::NodeEditor::ScreenToCanvas(m_createMousePos);

				//Assign initial positions
				ax::NodeEditor::SetNodePosition(GetID(f), mousePos);
			}
		}

		ImGui::EndMenu();
	}
}

/**
	@brief Handle requests to delete a link
 */
void FilterGraphEditor::HandleLinkDeletionRequests(Filter*& fReconfigure)
{
	if(ax::NodeEditor::BeginDelete())
	{
		ax::NodeEditor::LinkId lid;
		while(ax::NodeEditor::QueryDeletedLink(&lid))
		{
			//All paths are deleteable for now
			if(ax::NodeEditor::AcceptDeletedItem())
			{
				//All paths are from stream to input port
				//so second ID in the link should be the input, which is now connected to nothing
				auto pins = m_linkMap[lid];
				auto inputPort = m_inputIDMap[pins.second];
				inputPort.first->SetInput(inputPort.second, StreamDescriptor(nullptr, 0), true);

				fReconfigure = dynamic_cast<Filter*>(inputPort.first);
			}
		}
	}
	ax::NodeEditor::EndDelete();

}

/**
	@brief Make a node for a single channel (may be instrument channel or filter)
 */
void FilterGraphEditor::DoNodeForChannel(OscilloscopeChannel* channel)
{
	auto& prefs = m_session.GetPreferences();

	//Get some configuration / style settings
	auto tsize = ImGui::GetFontSize();
	//float bgmul = 0.2;
	//float hmul = 0.4;
	//float amul = 0.6;
	auto color = ColorFromString(channel->m_displaycolor);
	auto headercolor = prefs.GetColor("Appearance.Filter Graph.header_text_color");
	auto headerfont = m_parent->GetFontPref("Appearance.Filter Graph.header_font");
	float headerheight = headerfont->FontSize * 1.5;
	//auto fcolor = ImGui::ColorConvertU32ToFloat4(color);
	//auto bcolor = ImGui::ColorConvertFloat4ToU32(ImVec4(fcolor.x*bgmul, fcolor.y*bgmul, fcolor.z*bgmul, fcolor.w) );
	float rounding = ax::NodeEditor::GetStyle().NodeRounding;

	auto id = GetID(channel);
	ax::NodeEditor::BeginNode(id);
	ImGui::PushID(id.AsPointer());

	//Get node info
	auto pos = ax::NodeEditor::GetNodePosition(id);
	auto size = ax::NodeEditor::GetNodeSize(id);

	//Reserve space for the node header
	ImGui::Dummy(ImVec2(0, headerheight));
	//auto nsize = ax::NodeEditor::GetNodeSize(id);

	//Table of inputs at left and outputs at right
	static ImGuiTableFlags flags = 0;
	auto f = dynamic_cast<Filter*>(channel);
	if(ImGui::BeginTable("Ports", 2, flags, ImVec2(20*tsize, 0 ) ) )
	{
		//Input ports
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if(f)
		{
			for(size_t i=0; i<f->GetInputCount(); i++)
			{
				auto sid = GetID(pair<OscilloscopeChannel*, size_t>(f, i));

				string portname("‣ ");
				portname += f->GetInputName(i);
				ax::NodeEditor::BeginPin(sid, ax::NodeEditor::PinKind::Input);
					ax::NodeEditor::PinPivotAlignment(ImVec2(0, 0.5));
					ImGui::TextUnformatted(portname.c_str());
				ax::NodeEditor::EndPin();
			}
		}

		//Output ports
		ImGui::TableNextColumn();
		for(size_t i=0; i<channel->GetStreamCount(); i++)
		{
			StreamDescriptor stream(channel, i);
			auto sid = GetID(stream);

			string portname = channel->GetStreamName(i) + " ‣";
			ax::NodeEditor::BeginPin(sid, ax::NodeEditor::PinKind::Output);
				ax::NodeEditor::PinPivotAlignment(ImVec2(1, 0.5));
				RightJustifiedText(portname);
			ax::NodeEditor::EndPin();

			if(sid == ax::NodeEditor::GetHoveredPin())
			{
				ax::NodeEditor::Suspend();
					OutputPortTooltip(stream);
				ax::NodeEditor::Resume();
			}
		}

		ImGui::EndTable();
	}

	NodeConfig(id, channel);

	ImGui::PopID();
	ax::NodeEditor::EndNode();

	//Draw header after the node is done
	string headerText;
	if(f)
		headerText = f->GetProtocolDisplayName() + ": " + channel->GetDisplayName();
	else
		headerText = string("Channel: ") + channel->GetDisplayName();
	auto bgList = ax::NodeEditor::GetNodeBackgroundDrawList(id);
	bgList->AddRectFilled(
		ImVec2(pos.x + 1, pos.y + 1),
		ImVec2(pos.x + size.x - 1, pos.y + headerheight - 1),
		color,
		rounding,
		ImDrawFlags_RoundCornersTop);
	bgList->AddText(
		headerfont,
		headerfont->FontSize,
		ImVec2(pos.x + headerfont->FontSize*0.5, pos.y + headerfont->FontSize*0.25),
		headercolor,
		headerText.c_str());
}

/**
	@brief Do channel configuration stuff
 */
void FilterGraphEditor::NodeConfig(ax::NodeEditor::NodeId id, OscilloscopeChannel* channel)
{
	//Create a fixed size table to put everything else inside of
	auto tsize = ImGui::GetFontSize();
	if(ImGui::BeginTable("nodeconfig", 1, 0, ImVec2(20*tsize, 0 ) ) )
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		auto f = dynamic_cast<Filter*>(channel);

		if(m_propertiesDialogs.find(id) == m_propertiesDialogs.end())
		{
			if(f)
				m_propertiesDialogs[id] = make_shared<FilterPropertiesDialog>(f, m_parent, true);
			else
				m_propertiesDialogs[id] = make_shared<ChannelPropertiesDialog>(channel, true);
		}

		m_propertiesDialogs[id]->RenderAsChild();

		ImGui::EndTable();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ID allocation

ax::NodeEditor::NodeId FilterGraphEditor::GetID(OscilloscopeChannel* chan)
{
	//If it's in the table already, just return the ID
	if(m_channelIDMap.HasEntry(chan))
		return m_channelIDMap[chan];

	//Not in the table, allocate an ID
	int id = m_nextID;
	m_nextID ++;
	m_channelIDMap.emplace(chan, id);
	return id;
}

ax::NodeEditor::PinId FilterGraphEditor::GetID(StreamDescriptor stream)
{
	//If it's in the table already, just return the ID
	if(m_streamIDMap.HasEntry(stream))
		return m_streamIDMap[stream];

	//Not in the table, allocate an ID
	int id = m_nextID;
	m_nextID ++;
	m_streamIDMap.emplace(stream, id);
	return id;
}

ax::NodeEditor::PinId FilterGraphEditor::GetID(pair<OscilloscopeChannel*, size_t> input)
{
	//If it's in the table already, just return the ID
	if(m_inputIDMap.HasEntry(input))
		return m_inputIDMap[input];

	//Not in the table, allocate an ID
	int id = m_nextID;
	m_nextID ++;
	m_inputIDMap.emplace(input, id);
	return id;
}

ax::NodeEditor::LinkId FilterGraphEditor::GetID(pair<ax::NodeEditor::PinId, ax::NodeEditor::PinId> link)
{
	//If it's in the table already, just return the ID
	if(m_linkMap.HasEntry(link))
		return m_linkMap[link];

	//Not in the table, allocate an ID
	int id = m_nextID;
	m_nextID ++;
	m_linkMap.emplace(link, id);
	return id;
}
