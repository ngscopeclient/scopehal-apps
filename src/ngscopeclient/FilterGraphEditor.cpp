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
	@brief Implementation of FilterGraphEditor
 */
#include "ngscopeclient.h"
#include "FilterGraphEditor.h"
#include "MainWindow.h"
#include "BERTInputChannelDialog.h"
#include "BERTOutputChannelDialog.h"
#include "ChannelPropertiesDialog.h"
#include "FilterPropertiesDialog.h"
#include "EmbeddedTriggerPropertiesDialog.h"
#include "MeasurementsDialog.h"

//Pull in a bunch of filters we have special icons for
#include "../scopeprotocols/AddFilter.h"
#include "../scopeprotocols/AreaMeasurement.h"
#include "../scopeprotocols/ClockRecoveryFilter.h"
#include "../scopeprotocols/DivideFilter.h"
#include "../scopeprotocols/EyePattern.h"
#include "../scopeprotocols/MultiplyFilter.h"
#include "../scopeprotocols/SubtractFilter.h"
#include "../scopeprotocols/ThresholdFilter.h"
#include "../scopeprotocols/ToneGeneratorFilter.h"
#include "../scopeprotocols/UpsampleFilter.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FilterGraphGroup

FilterGraphGroup::FilterGraphGroup(FilterGraphEditor& ed)
	: m_parent(ed)
{
	m_outputId = ed.AllocateID();
	m_inputId = ed.AllocateID();
}

/**
	@brief Refreshes the list of child nodes within this node
 */
void FilterGraphGroup::RefreshChildren()
{
	//Get all of the node IDs
	int nnodes = ax::NodeEditor::GetNodeCount();
	vector<ax::NodeEditor::NodeId> nodes;
	nodes.resize(nnodes);
	ax::NodeEditor::GetOrderedNodeIds(&nodes[0], nnodes);

	//Check to see which are within us
	auto pos = ax::NodeEditor::GetNodePosition(m_id);
	auto size = ax::NodeEditor::GetNodeSize(m_id);
	m_children.clear();
	for(auto nid : nodes)
	{
		auto posNode = ax::NodeEditor::GetNodePosition(nid);
		auto sizeNode = ax::NodeEditor::GetNodeSize(nid);

		if(RectContains(pos, size, posNode, sizeNode))
			m_children.emplace(nid);
	}
}

/**
	@brief Refreshes the list of links between this group and the outside world
 */
void FilterGraphGroup::RefreshLinks()
{
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Outbound links

	//Make a list of all outlinks that we currently have to the outside world
	set<StreamDescriptor> outlinks;
	for(auto it : m_parent.m_linkMap)
	{
		auto link = it.first;

		//We only care about source pins IN this group, going to sink pins OUTSIDE this group
		if(m_childSourcePins.find(link.first) == m_childSourcePins.end())
			continue;
		if(m_childSinkPins.find(link.second) != m_childSinkPins.end())
			continue;

		//Look up the stream for the source node and mark it as used
		auto stream = m_parent.m_streamIDMap[link.first];
		outlinks.emplace(stream);

		//Add to the list of hierarchical output ports if it's not there already
		if(!m_hierOutputMap.HasEntry(stream))
			m_hierOutputMap.emplace(stream, m_parent.AllocateID());
		if(!m_hierOutputInternalMap.HasEntry(stream))
			m_hierOutputInternalMap.emplace(stream, m_parent.AllocateID());
	}

	//Remove any links that are no longer in use
	vector<StreamDescriptor> outgarbage;
	for(auto it : m_hierOutputMap)
	{
		if(outlinks.find(it.first) == outlinks.end())
			outgarbage.push_back(it.first);
	}
	for(auto stream : outgarbage)
	{
		m_hierOutputMap.erase(stream);
		m_hierOutputInternalMap.erase(stream);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Inbound links

	//Make a list of all inlinks that we currently have to the outside world
	set< pair<FlowGraphNode*, int> > inlinks;
	for(auto it : m_parent.m_linkMap)
	{
		auto link = it.first;

		//We only care about source pins OUTSIDE this group, going to sink pins IN this group
		if(m_childSourcePins.find(link.first) != m_childSourcePins.end())
			continue;
		if(m_childSinkPins.find(link.second) == m_childSinkPins.end())
			continue;

		//Look up the stream for the sink node and mark it as used
		auto input = m_parent.m_inputIDMap[link.second];
		inlinks.emplace(input);

		//Add to the list of hierarchical input ports if it's not there already
		if(!m_hierInputMap.HasEntry(input))
			m_hierInputMap.emplace(input, m_parent.AllocateID());
		if(!m_hierInputInternalMap.HasEntry(input))
			m_hierInputInternalMap.emplace(input, m_parent.AllocateID());
	}

	//Remove any links that are no longer in use
	vector< pair<FlowGraphNode*, int> > ingarbage;
	for(auto it : m_hierInputMap)
	{
		if(inlinks.find(it.first) == inlinks.end())
			ingarbage.push_back(it.first);
	}
	for(auto stream : ingarbage)
	{
		m_hierInputMap.erase(stream);
		m_hierInputInternalMap.erase(stream);
	}
}

/**
	@brief Moves this node and all of its child nodes
 */
void FilterGraphGroup::MoveBy(ImVec2 displacement)
{
	auto pos = ax::NodeEditor::GetNodePosition(m_id);
	ax::NodeEditor::SetNodePosition(m_id, pos + displacement);

	for(auto nid : m_children)
	{
		auto cpos = ax::NodeEditor::GetNodePosition(nid);
		ax::NodeEditor::SetNodePosition(nid, cpos + displacement);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

FilterGraphEditor::FilterGraphEditor(Session& session, MainWindow* parent)
	: Dialog("Filter Graph Editor", "Filter Graph Editor", ImVec2(800, 600))
	, m_session(session)
	, m_parent(parent)
	, m_nextID(1)
{
	m_config.SaveSettings = &FilterGraphEditor::SaveSettingsCallback;
	m_config.LoadSettings = &FilterGraphEditor::LoadSettingsCallback;
	m_config.UserPointer = this;

	m_config.SettingsFile = "";
	m_context = ax::NodeEditor::CreateEditor(&m_config);

	//Load icons for filters
	m_parent->GetTextureManager()->LoadTexture("filter-add", FindDataFile("icons/filters/filter-add.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-cdrpll", FindDataFile("icons/filters/filter-cdrpll.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-eyepattern", FindDataFile("icons/filters/filter-eyepattern.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-multiply", FindDataFile("icons/filters/filter-multiply.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-subtract", FindDataFile("icons/filters/filter-subtract.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-threshold", FindDataFile("icons/filters/filter-threshold.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-upsample", FindDataFile("icons/filters/filter-upsample.png"));
	m_parent->GetTextureManager()->LoadTexture("input-banana-dual", FindDataFile("icons/filters/input-banana-dual.png"));
	m_parent->GetTextureManager()->LoadTexture("input-bnc", FindDataFile("icons/filters/input-bnc.png"));
	m_parent->GetTextureManager()->LoadTexture("input-k-dual", FindDataFile("icons/filters/input-k-dual.png"));
	m_parent->GetTextureManager()->LoadTexture("input-k", FindDataFile("icons/filters/input-k.png"));
	m_parent->GetTextureManager()->LoadTexture("input-sma", FindDataFile("icons/filters/input-sma.png"));
}

FilterGraphEditor::~FilterGraphEditor()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool FilterGraphEditor::Render()
{
	//If we have an open properties dialog with a file browser open, run it
	auto dlg = m_propertiesDialogs[m_selectedProperties];
	auto fdlg = dynamic_pointer_cast<FilterPropertiesDialog>(dlg);
	auto bdlg = dynamic_pointer_cast<BERTInputChannelDialog>(dlg);
	if(fdlg)
		fdlg->RunFileDialog();
	else if(bdlg)
		bdlg->RunFileDialog();

	return Dialog::Render();
}

/**
	@brief Get a list of all channels that we are displaying nodes for
 */
map<Instrument*, vector<InstrumentChannel*> > FilterGraphEditor::GetAllChannels()
{
	map<Instrument*, vector<InstrumentChannel*> > ret;

	auto insts = m_session.GetInstruments();
	for(auto inst : insts)
	{
		vector<InstrumentChannel*> chans;

		//Channels
		auto scope = dynamic_cast<Oscilloscope*>(inst);
		auto psu = dynamic_cast<PowerSupply*>(inst);
		for(size_t i=0; i<inst->GetChannelCount(); i++)
		{
			auto chan = inst->GetChannel(i);

			//Exclude scope channels that can't be, or are not, enabled
			//TODO: should CanEnableChannel become an Instrument method?
			if(scope)
			{
				if(inst->GetInstrumentTypesForChannel(i) & Instrument::INST_OSCILLOSCOPE)
				{
					//If it's a trigger channel, allow it even if it's not enabled
					//TODO: only allow if currently selected
					if(chan == scope->GetExternalTrigger())
					{
					}
					else
					{
						if(!scope->CanEnableChannel(i))
							continue;
						if(!scope->IsChannelEnabled(i))
							continue;
					}
				}
			}

			//Exclude power supply channels that lack voltage/current controls
			//TODO: still allow filter graph control of on/off?
			if(psu)
			{
				if(inst->GetInstrumentTypesForChannel(i) & Instrument::INST_PSU)
				{
					if(!psu->SupportsVoltageCurrentControl(i))
						continue;
				}
			}

			chans.push_back(chan);
		}

		ret[inst] = chans;
	}

	return ret;
}

/**
	@brief Get a list of all objects we're displaying nodes for (channels, filters, triggers, etc)
 */
vector<FlowGraphNode*> FilterGraphEditor::GetAllNodes()
{
	vector<FlowGraphNode*> ret;

	//Channels
	auto chans = GetAllChannels();
	for(auto it : chans)
	{
		for(auto node : it.second)
			ret.push_back(node);
	}

	//Triggers
	auto insts = m_session.GetInstruments();
	for(auto inst : insts)
	{
		auto scope = dynamic_cast<Oscilloscope*>(inst);
		if(scope)
		{
			auto trig = scope->GetTrigger();
			if(trig)
				ret.push_back(trig);
		}
	}

	//Filters
	auto filters = Filter::GetAllInstances();
	for(auto f : filters)
		ret.push_back(f);

	return ret;
}

/**
	@brief Gets the source pin we should use for drawing a connection

	Note that this may not be the literal source if we're sourcing from a hierarchical port
 */
ax::NodeEditor::PinId FilterGraphEditor::GetSourcePinForLink(StreamDescriptor source, FlowGraphNode* sink)
{
	//Source not in a group? Just use the actual source
	if(!m_nodeGroupMap.HasEntry(source.m_channel))
		return m_streamIDMap[source];

	//Sink in same group as source? Use the actual source
	auto srcGroup = m_nodeGroupMap[source.m_channel];
	if(m_nodeGroupMap.HasEntry(sink))
	{
		if(srcGroup == m_nodeGroupMap[sink])
			return m_streamIDMap[source];
	}

	//Source is in a group, sink is not in the same group. Use the hierarchical port
	else if(srcGroup->m_hierOutputMap.HasEntry(source))
		return srcGroup->m_hierOutputMap[source];

	//If we get here, the hierarchical port might have just been created this frame.
	//Use the original port temporarily
	return m_streamIDMap[source];
}

/**
	@brief Gets the sink pin we should use for drawing a connection

	Note that this may not be the literal sink if we're sinking to a hierarchical port
 */
ax::NodeEditor::PinId FilterGraphEditor::GetSinkPinForLink(StreamDescriptor source, pair<FlowGraphNode*, int> sink)
{
	//Sink not in a group? Use actual source
	if(!m_nodeGroupMap.HasEntry(sink.first))
		return m_inputIDMap[sink];

	//Sink in same group as source? Use actual sink
	auto sinkGroup = m_nodeGroupMap[sink.first];
	if(m_nodeGroupMap.HasEntry(source.m_channel))
	{
		if(sinkGroup == m_nodeGroupMap[source.m_channel])
			return m_inputIDMap[sink];
	}

	//Sink is in a group, source is not in the same group. Use the hierarchical port
	else if(sinkGroup->m_hierInputMap.HasEntry(sink))
		return sinkGroup->m_hierInputMap[sink];

	return m_inputIDMap[sink];
}

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool FilterGraphEditor::DoRender()
{
	ax::NodeEditor::SetCurrentEditor(m_context);
	ax::NodeEditor::Begin("Filter Graph", ImVec2(0, 0));

	//Make nodes for all groups
	RefreshGroupPorts();
	for(auto it : m_groups)
		DoNodeForGroup(it.first);

	//Make nodes for all instrument channels
	auto chans = GetAllChannels();
	for(auto it : chans)
	{
		for(auto chan : it.second)
			DoNodeForChannel(chan, it.first);
	}

	//Make nodes for all triggers
	auto insts = m_session.GetInstruments();
	for(auto inst : insts)
	{
		auto scope = dynamic_cast<Oscilloscope*>(inst);

		//Triggers (for now, only scopes have these)
		if(scope)
		{
			auto trig = scope->GetTrigger();
			if(trig)
				DoNodeForTrigger(trig);
		}
	}

	//Filters
	auto filters = Filter::GetAllInstances();
	for(auto f : filters)
		DoNodeForChannel(f, nullptr);
	ClearOldPropertiesDialogs();

	//All nodes
	auto nodes = m_session.GetAllGraphNodes();

	//Add links within groups
	for(auto it : m_groups)
		DoInternalLinksForGroup(it.first);

	//Add links from each input to the stream it's fed by
	for(auto f : nodes)
	{
		for(size_t i=0; i<f->GetInputCount(); i++)
		{
			auto stream = f->GetInput(i);
			if(stream)
			{
				auto srcid = GetSourcePinForLink(stream, f);
				auto dstid = GetSinkPinForLink(stream, pair<FlowGraphNode*, size_t>(f, i));
				auto linkid = GetID(pair<ax::NodeEditor::PinId, ax::NodeEditor::PinId>(srcid, dstid));
				ax::NodeEditor::Link(linkid, srcid, dstid);
			}
		}
	}

	//Add links from each trigger input to the stream it's fed by
	auto& scopes = m_session.GetScopes();
	for(auto scope : scopes)
	{
		auto trig = scope->GetTrigger();
		if(trig)
		{
			for(size_t i=0; i<trig->GetInputCount(); i++)
			{
				auto stream = trig->GetInput(i);
				if(stream)
				{
					auto srcid = GetSourcePinForLink(stream, trig);
					auto dstid = GetID(pair<FlowGraphNode*, size_t>(trig, i));
					auto linkid = GetID(pair<ax::NodeEditor::PinId, ax::NodeEditor::PinId>(srcid, dstid));
					ax::NodeEditor::Link(linkid, srcid, dstid);
				}
			}
		}
	}

	//Handle other user input
	Filter* fReconfigure = nullptr;
	HandleLinkCreationRequests(fReconfigure);
	HandleLinkDeletionRequests(fReconfigure);
	HandleNodeProperties();
	HandleBackgroundContextMenu();

	ax::NodeEditor::End();

	//Refresh all of our groups to have up-to-date child contents
	for(auto it : m_groups)
		it.first->RefreshChildren();

	//Look for and avoid overlaps
	//Must be after End() call which draws node boundaries, so everything is consistent.
	//If we don't do that, node content and frames can get one frame out of sync
	HandleOverlaps();

	ax::NodeEditor::SetCurrentEditor(nullptr);

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
	@brief Gets the ID for an arbitrary node
 */
ax::NodeEditor::NodeId FilterGraphEditor::GetID(FlowGraphNode* node)
{
	auto chan = dynamic_cast<InstrumentChannel*>(node);
	if(chan)
		return GetID(chan);

	auto trig = dynamic_cast<Trigger*>(node);
	if(trig)
		return GetID(trig);

	return m_session.m_idtable.emplace(node);
}

/**
	@brief Figure out which source/sink ports are within each group
 */
void FilterGraphEditor::RefreshGroupPorts()
{
	m_nodeGroupMap.clear();

	for(auto it : m_groups)
	{
		auto group = it.first;

		group->m_childSourcePins.clear();
		group->m_childSinkPins.clear();

		auto nodes = GetAllNodes();
		for(auto node : nodes)
		{
			auto id = GetID(node);
			auto chan = dynamic_cast<InstrumentChannel*>(node);

			//Skip anything outside our group
			if(group->m_children.find(id) == group->m_children.end())
				continue;

			m_nodeGroupMap.emplace(node, group);

			//Only instrument channels can source signals
			if(chan)
			{
				for(size_t i=0; i<chan->GetStreamCount(); i++)
				{
					StreamDescriptor stream(chan, i);
					group->m_childSourcePins.emplace(GetID(stream));
				}
			}

			//All flow graph nodes can sink signals
			for(size_t i=0; i<node->GetInputCount(); i++)
			{
				pair<FlowGraphNode*, int> indesc(node, i);
				group->m_childSinkPins.emplace(GetID(indesc));
			}
		}
	}
}

void FilterGraphEditor::DoNodeForGroup(std::shared_ptr<FilterGraphGroup> group)
{
	auto gid = m_groups[group];

	ImVec2 initialsize(320, 240);

	//Make the node for the group
	ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_NodeBg, ImColor(255, 255, 255, 64));
	ax::NodeEditor::BeginNode(gid);
	ImGui::PushID(gid.AsPointer());
	ImGui::TextUnformatted(group->m_name.c_str());
	ax::NodeEditor::Group(initialsize);
	ImGui::PopID();
	ax::NodeEditor::EndNode();
	ax::NodeEditor::PopStyleColor();

	//Find which of our source pins have edges to other groups
	group->RefreshLinks();

	//Groups cannot directly have ports, so make a dummy child node for the hierarchical ports
	DoNodeForGroupOutputs(group);
	DoNodeForGroupInputs(group);
}

void FilterGraphEditor::DoNodeForGroupInputs(shared_ptr<FilterGraphGroup> group)
{
	//Find parent group
	auto gid = m_groups[group];
	auto gpos = ax::NodeEditor::GetNodePosition(gid);

	//Figure out how big the port text is
	auto textfont = ImGui::GetFont();
	float oportmax = 1;
	float iportmax = textfont->CalcTextSizeA(textfont->FontSize, FLT_MAX, 0, "‣").x;
	vector<string> onames;
	for(auto it : group->m_hierInputMap)
	{
		auto sink = it.first;

		//TODO refactor into function
		string sinkname = "(unimplemented)";
		auto chan = dynamic_cast<InstrumentChannel*>(sink.first);
		auto trig = dynamic_cast<Trigger*>(sink.first);
		if(chan)
			sinkname = chan->GetDisplayName();
		else if(trig)
			sinkname = trig->GetScope()->m_nickname;

		auto name = sinkname + " ‣";
		onames.push_back(name);
		oportmax = max(oportmax,
			textfont->CalcTextSizeA(textfont->FontSize, FLT_MAX, 0, name.c_str()).x +
			textfont->FontSize * 2);
	}
	float nodewidth = oportmax + iportmax + 1*ImGui::GetStyle().ItemSpacing.x;

	//Set size/position
	auto headerfont = m_parent->GetFontPref("Appearance.Filter Graph.header_font");
	float headerheight = headerfont->FontSize * 1.5;
	auto gborder = ax::NodeEditor::GetStyle().GroupBorderWidth;
	auto gpad = ax::NodeEditor::GetStyle().NodePadding.x;
	ImVec2 pos(
		gpos.x + gborder + gpad,
		gpos.y + headerheight + gborder);
	ax::NodeEditor::SetNodePosition(group->m_inputId, pos);
	ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_NodeRounding, 0);
	ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_NodeBorderWidth, 0);
	ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_HoveredNodeBorderWidth, 0);
	ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_SelectedNodeBorderWidth, 0);
	ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_NodeBg, ImColor(0, 0, 0, 0));
	ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_HovNodeBorder, ImColor(0, 0, 0, 0));
	ax::NodeEditor::BeginNode(group->m_inputId);
	ImGui::PushID(group->m_inputId.AsPointer());

	//Get node info
	pos = ax::NodeEditor::GetNodePosition(group->m_inputId);

	//Table of input ports
	if(ImGui::BeginTable("Ports", 2, 0, ImVec2(nodewidth, 0 ) ) )
	{
		ImGui::TableSetupColumn("inputs", ImGuiTableColumnFlags_WidthFixed, iportmax + 2);
		ImGui::TableSetupColumn("outputs", ImGuiTableColumnFlags_WidthFixed, oportmax + 2);

		for(auto it : group->m_hierInputMap)
		{
			ImGui::TableNextRow();

			auto sink = it.first;
			auto sid = it.second;

			if(sink.first == nullptr)
			{
				LogWarning("null sink\n");
				continue;
			}

			//Input side (path from external node to hierarchical port)
			ImGui::TableNextColumn();
			ax::NodeEditor::BeginPin(sid, ax::NodeEditor::PinKind::Input);
				ax::NodeEditor::PinPivotAlignment(ImVec2(0, 0.5));
				ImGui::TextUnformatted("‣");
			ax::NodeEditor::EndPin();

			//TODO refactor into function
			string sinkname = "(unimplemented)";
			auto chan = dynamic_cast<InstrumentChannel*>(sink.first);
			auto trig = dynamic_cast<Trigger*>(sink.first);
			if(chan)
				sinkname = chan->GetDisplayName();
			else if(trig)
				sinkname = trig->GetScope()->m_nickname;

			//Output side (path from hierarchical port to internal node)
			ImGui::TableNextColumn();
			ax::NodeEditor::BeginPin(group->m_hierInputInternalMap[sink], ax::NodeEditor::PinKind::Output);
				ax::NodeEditor::PinPivotAlignment(ImVec2(1, 0.5));
				RightJustifiedText(sinkname + "." + sink.first->GetInputName(sink.second) + " ‣");
			ax::NodeEditor::EndPin();
		}
		ImGui::EndTable();

	}

	ImGui::PopID();
	ax::NodeEditor::EndNode();
	ax::NodeEditor::PopStyleColor(2);
	ax::NodeEditor::PopStyleVar(4);
}

void FilterGraphEditor::DoNodeForGroupOutputs(shared_ptr<FilterGraphGroup> group)
{
	//Get dimensions of the parent group node
	auto gid = m_groups[group];
	auto gpos = ax::NodeEditor::GetNodePosition(gid);
	auto gsz = ax::NodeEditor::GetNodeSize(gid);

	//Figure out how big the port text is
	auto textfont = ImGui::GetFont();
	float oportmax = 1;
	float iportmax = textfont->CalcTextSizeA(textfont->FontSize, FLT_MAX, 0, "‣").x;
	vector<string> onames;
	for(auto it : group->m_hierOutputMap)
	{
		auto stream = it.first;

		auto name = stream.GetName() + " ‣";
		onames.push_back(name);
		oportmax = max(oportmax, textfont->CalcTextSizeA(textfont->FontSize, FLT_MAX, 0, name.c_str()).x);
	}
	float nodewidth = oportmax + iportmax + 3*ImGui::GetStyle().ItemSpacing.x;

	//Set size/position
	auto headerfont = m_parent->GetFontPref("Appearance.Filter Graph.header_font");
	float headerheight = headerfont->FontSize * 1.5;
	auto gborder = ax::NodeEditor::GetStyle().GroupBorderWidth;
	auto gpad = ax::NodeEditor::GetStyle().NodePadding.x;
	ImVec2 pos(
		gpos.x + gsz.x - nodewidth - (gborder + gpad*3),
		gpos.y + headerheight + gborder);
	ax::NodeEditor::SetNodePosition(group->m_outputId, pos);

	ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_NodeRounding, 0);
	ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_NodeBorderWidth, 0);
	ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_HoveredNodeBorderWidth, 0);
	ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_SelectedNodeBorderWidth, 0);
	ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_NodeBg, ImColor(0, 0, 0, 0));
	ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_HovNodeBorder, ImColor(0, 0, 0, 0));
	ax::NodeEditor::BeginNode(group->m_outputId);
	ImGui::PushID(group->m_outputId.AsPointer());

	//Get node info
	pos = ax::NodeEditor::GetNodePosition(group->m_outputId);

	//Table of output ports
	StreamDescriptor hoveredStream(nullptr, 0);

	if(ImGui::BeginTable("Ports", 2, 0, ImVec2(nodewidth, 0 ) ) )
	{
		ImGui::TableSetupColumn("inputs", ImGuiTableColumnFlags_WidthFixed, iportmax + 2);
		ImGui::TableSetupColumn("outputs", ImGuiTableColumnFlags_WidthFixed, oportmax + 2);

		for(auto it : group->m_hierOutputMap)
		{
			ImGui::TableNextRow();

			auto stream = it.first;
			auto sid = it.second;

			//Input side (path from internal node to hierarchical port)
			ImGui::TableNextColumn();
			ax::NodeEditor::BeginPin(group->m_hierOutputInternalMap[stream], ax::NodeEditor::PinKind::Input);
				ax::NodeEditor::PinPivotAlignment(ImVec2(0, 0.5));
				ImGui::TextUnformatted("‣");
			ax::NodeEditor::EndPin();

			//Output side (path from hierarchical port to external node)
			ImGui::TableNextColumn();
			ax::NodeEditor::BeginPin(sid, ax::NodeEditor::PinKind::Output);
				ax::NodeEditor::PinPivotAlignment(ImVec2(1, 0.5));
				RightJustifiedText(stream.GetName() + " ‣");
			ax::NodeEditor::EndPin();

			if(sid == ax::NodeEditor::GetHoveredPin())
				hoveredStream = stream;
		}

		ImGui::EndTable();
	}

	//Tooltip on hovered output port
	if(hoveredStream)
	{
		//Output port
		ax::NodeEditor::Suspend();
			OutputPortTooltip(hoveredStream);
		ax::NodeEditor::Resume();
	}

	ImGui::PopID();
	ax::NodeEditor::EndNode();
	ax::NodeEditor::PopStyleColor(2);
	ax::NodeEditor::PopStyleVar(4);
}

/**
	@brief Handle links between nodes in a group and the hierarchical ports
 */
void FilterGraphEditor::DoInternalLinksForGroup(shared_ptr<FilterGraphGroup> group)
{
	//Add links from node outputs to the hierarchical port node
	for(auto it : group->m_hierOutputInternalMap)
	{
		auto fromstream = it.first;

		auto fromPin = GetID(fromstream);
		auto toPin = it.second;

		ax::NodeEditor::LinkId lid;
		if(group->m_hierOutputLinkMap.HasEntry(fromstream))
			lid = group->m_hierOutputLinkMap[fromstream];
		else
		{
			lid = AllocateID();
			group->m_hierOutputLinkMap.emplace(fromstream, lid);
		}

		ax::NodeEditor::Link(lid, fromPin, toPin);
	}

	//And then again for the inputs
	for(auto it : group->m_hierInputInternalMap)
	{
		auto toport = it.first;

		auto toPin = GetID(toport);
		auto fromPin = it.second;

		ax::NodeEditor::LinkId lid;
		if(group->m_hierInputLinkMap.HasEntry(toport))
			lid = group->m_hierInputLinkMap[toport];
		else
		{
			lid = AllocateID();
			group->m_hierInputLinkMap.emplace(toport, lid);
		}

		ax::NodeEditor::Link(lid, fromPin, toPin);
	}
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
				ImGui::TextUnformatted("Analog channel");
				break;

			case Stream::STREAM_TYPE_DIGITAL:
				ImGui::TextUnformatted("Digital channel");
				break;

			case Stream::STREAM_TYPE_DIGITAL_BUS:
				ImGui::TextUnformatted("Digital bus");
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

			case Stream::STREAM_TYPE_TRIGGER:
				ImGui::TextUnformatted("External trigger");
				break;

			case Stream::STREAM_TYPE_ANALOG_SCALAR:
				{
					ImGui::TextUnformatted("Analog value:");
					string value = stream.GetYAxisUnits().PrettyPrint(stream.GetScalarValue());
					ImGui::TextUnformatted(value.c_str());
				}
				break;

			default:
				ImGui::TextUnformatted("Unknown channel type");
				break;
		}
		ImGui::TextUnformatted("Drag from this port to create a connection.");
	ImGui::EndTooltip();
}

/**
	@brief Find nodes that are intersecting, and apply forces to avoid collisions
 */
void FilterGraphEditor::HandleOverlaps()
{
	//Get all of the node IDs
	int nnodes = ax::NodeEditor::GetNodeCount();
	vector<ax::NodeEditor::NodeId> nodes;
	nodes.resize(nnodes);
	ax::NodeEditor::GetOrderedNodeIds(&nodes[0], nnodes);

	//Need to use internal APIs to figure out if we're dragging the current node
	//in order to properly implement collision detection
	auto action = reinterpret_cast<ax::NodeEditor::Detail::EditorContext*>(m_context)->GetCurrentAction();
	ax::NodeEditor::Detail::DragAction* drag = nullptr;
	if(action)
		drag = action->AsDrag();

	//Loop over all nodes and find potential collisions
	for(int i=0; i<nnodes; i++)
	{
		auto nodeA = nodes[i];
		auto posA = ax::NodeEditor::GetNodePosition(nodeA);
		auto sizeA = ax::NodeEditor::GetNodeSize(nodeA);

		bool groupA = m_groups.HasEntry(nodeA);
		//bool selA = ax::NodeEditor::IsNodeSelected(nodeA);

		for(int j=0; j<nnodes; j++)
		{
			//Don't check for self intersection
			if(i == j)
				continue;

			auto nodeB = nodes[j];
			auto posB = ax::NodeEditor::GetNodePosition(nodeB);
			auto sizeB = ax::NodeEditor::GetNodeSize(nodeB);

			bool groupB = m_groups.HasEntry(nodeB);
			bool selB = ax::NodeEditor::IsNodeSelected(nodeB);

			//If node B is selected, don't move it (but it can push other stuff)
			if(selB)
				continue;

			//Check for node-group collisions
			//Node-node is normal code path, group-group also repels
			if( (groupA && !groupB) || (!groupA && groupB) )
			{
				auto nid = groupA ? nodeB : nodeA;
				auto gid = groupA ? nodeA : nodeB;

				auto posNode = ax::NodeEditor::GetNodePosition(nid);
				auto sizeNode = ax::NodeEditor::GetNodeSize(nid);

				auto posGroup = ax::NodeEditor::GetNodePosition(gid);
				auto sizeGroup = ax::NodeEditor::GetNodeSize(gid);

				//If node is completely INSIDE the group, don't repel
				if(RectContains(posGroup, sizeGroup, posNode, sizeNode))
					continue;

				//If node is the group's hierarchical port node, don't repel
				auto group = m_groups[gid];
				if(nid == group->m_outputId)
					continue;
				if(nid == group->m_inputId)
					continue;

				//Check if we're dragging the group
				//bool draggingGroup = false;
				bool draggingNode = false;
				if(drag)
				{
					for(auto o : drag->m_Objects)
					{
						auto n = o->AsNode();
						if(!n)
							continue;

						if(n->m_ID == gid)
						{
							//draggingGroup = true;
							break;
						}
						if(n->m_ID == nid)
						{
							draggingNode = true;
							break;
						}
					}
				}

				//If dragging group, we should push nodes away

				//But if dragging the node, allow it to go into the group
				if(draggingNode)
					continue;
			}

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
			ImVec2 shift(0, 0);
			if(mag < 1e-2f)
			{
				if(rand() & 1)
					shift.x = 1;
				else
					shift.y = 1;
			}

			else
			{
				float distance = 10;
				float scale = distance / mag;
				shift.x = scale * dx;
				shift.y = scale * dy;
			}

			//If node B is a group, we need to move all nodes inside it by the same amount we moved the group
			if(groupB)
				m_groups[nodeB]->MoveBy(shift);

			//Otherwise just move the node
			else
				ax::NodeEditor::SetNodePosition(nodeB, posB + shift);
		}
	}
}

/**
	@brief Gets the actual source/sink pin given a pin which might be a hierarchical port
 */
ax::NodeEditor::PinId FilterGraphEditor::CanonicalizePin(ax::NodeEditor::PinId port)
{
	for(auto it : m_groups)
	{
		auto group = it.first;

		//Check for hierarchical outputs
		if(group->m_hierOutputMap.HasEntry(port))
			return m_streamIDMap[group->m_hierOutputMap[port]];

		//Check for hierarchical inputs
		if(group->m_hierInputMap.HasEntry(port))
			return m_inputIDMap[group->m_hierInputMap[port]];
	}

	return port;
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
				//If start or end pin ID are hierarchical ports, re-map to the actual source port
				startId = CanonicalizePin(startId);
				endId = CanonicalizePin(endId);

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

							//Push trigger changes if needed
							auto t = dynamic_cast<Trigger*>(inputPort.first);
							if(t != nullptr)
								t->GetScope()->PushTrigger();
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

		if(ax::NodeEditor::QueryNewNode(&startId) && startId)
		{
			startId = CanonicalizePin(startId);

			//Dragging from node output - create new filter from that
			if(m_streamIDMap.HasEntry(startId))
			{
				//See what the stream is
				m_newFilterSourceStream = m_streamIDMap[startId];

				//Cannot create filters using external trigger as input
				if(m_newFilterSourceStream.GetType() == Stream::STREAM_TYPE_TRIGGER)
				{
					ImGui::BeginTooltip();
						ImGui::TextColored(invalidcolor, "x Cannot use external trigger as input to a filter");
					ImGui::EndTooltip();

					ax::NodeEditor::RejectNewItem(invalidcolor);
				}

				//All good otherwise
				else
				{
					ImGui::BeginTooltip();
						ImGui::TextColored(validcolor, "+ Create Filter");
					ImGui::EndTooltip();

					if(ax::NodeEditor::AcceptNewItem())
					{
						ax::NodeEditor::Suspend();
						m_createMousePos = ImGui::GetMousePos();
						ImGui::OpenPopup("Create Filter");
						ax::NodeEditor::Resume();
					}
				}
			}

			//Dragging from node input - display list of channels
			else if(m_inputIDMap.HasEntry(startId))
			{
				ImGui::BeginTooltip();
					ImGui::TextColored(validcolor, "+ Add Channel");
				ImGui::EndTooltip();

				if(ax::NodeEditor::AcceptNewItem())
				{
					m_createInput = m_inputIDMap[startId];

					ax::NodeEditor::Suspend();
					m_createMousePos = ImGui::GetMousePos();
					ImGui::OpenPopup("Add Input");
					ax::NodeEditor::Resume();
				}
			}
		}
	}
	ax::NodeEditor::EndCreate();

	ax::NodeEditor::Suspend();

		//Create-filter menu
		if(ImGui::BeginPopup("Create Filter"))
		{
			FilterMenu(m_newFilterSourceStream);
			ImGui::EndPopup();
		}

		//Add-input menu
		if(ImGui::BeginPopup("Add Input"))
		{
			CreateChannelMenu();
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
bool FilterGraphEditor::IsBackEdge(FlowGraphNode* src, FlowGraphNode* dst)
{
	if( (src == nullptr) || (dst == nullptr) )
		return false;

	if(src == dst)
		return true;

	//Check each input of src
	for(size_t i=0; i<src->GetInputCount(); i++)
	{
		auto stream = src->GetInput(i);
		if(IsBackEdge(stream.m_channel, dst))
			return true;
	}

	return false;
}

/**
	@brief Runs the "add input" menu
 */
void FilterGraphEditor::CreateChannelMenu()
{
	if(ImGui::BeginMenu("Channels"))
	{
		vector<StreamDescriptor> streams;

		auto& scopes = m_session.GetScopes();
		for(auto scope : scopes)
		{
			//Channels
			for(size_t i=0; i<scope->GetChannelCount(); i++)
			{
				if(!scope->CanEnableChannel(i))
					continue;

				auto chan = scope->GetOscilloscopeChannel(i);
				if(!chan)
					continue;

				for(size_t j=0; j<chan->GetStreamCount(); j++)
					streams.push_back(StreamDescriptor(chan, j));
			}
		}

		//Filters
		auto filters = Filter::GetAllInstances();
		for(auto f : filters)
		{
			for(size_t j=0; j<f->GetStreamCount(); j++)
				streams.push_back(StreamDescriptor(f, j));
		}

		//Run the actual menu
		for(auto s : streams)
		{
			//Skip anything not valid for this sink
			if(!m_createInput.first->ValidateChannel(m_createInput.second, s))
				continue;

			//Don't allow creation of back edges
			if(m_createInput.first == s.m_channel)
				continue;

			//Show menu items
			if(ImGui::MenuItem(s.GetName().c_str()))
			{
				m_createInput.first->SetInput(m_createInput.second, s);

				auto trig = dynamic_cast<Trigger*>(m_createInput.first);
				if(trig)
					trig->GetScope()->PushTrigger();
			}
		}

		ImGui::EndMenu();
	}
	if(ImGui::BeginMenu("Create"))
	{
		auto& refs = m_parent->GetSession().GetReferenceFilters();

		//Find all filters in this category and sort them alphabetically
		vector<string> sortedNames;
		for(auto it : refs)
		{
			if(it.second->GetCategory() == Filter::CAT_GENERATION)
				sortedNames.push_back(it.first);
		}
		std::sort(sortedNames.begin(), sortedNames.end());

		//Do all of the menu items
		for(auto fname : sortedNames)
		{
			auto it = refs.find(fname);

			//For now: don't allow creation of filters that take inputs if going back
			if(it->second->GetInputCount() != 0)
				continue;

			if(ImGui::MenuItem(fname.c_str()))
			{
				//Make the filter but don't spawn a properties dialog for it or add to a waveform area
				auto f = m_parent->CreateFilter(fname, nullptr, StreamDescriptor(nullptr, 0), false, false);

				//Get relative mouse position
				auto mousePos = ax::NodeEditor::ScreenToCanvas(m_createMousePos);

				//Assign initial positions
				ax::NodeEditor::SetNodePosition(GetID(f), mousePos);

				//Once the filter exists, hook it up
				m_createInput.first->SetInput(m_createInput.second, StreamDescriptor(f, 0));

				auto trig = dynamic_cast<Trigger*>(m_createInput.first);
				if(trig)
					trig->GetScope()->PushTrigger();
			}
		}

		ImGui::EndMenu();
	}
}

/**
	@brief Runs the "create filter" menu
 */
void FilterGraphEditor::FilterMenu(StreamDescriptor stream)
{
	//See if the source stream is a scalar, if so offer to add a measurement
	if(stream.GetType() == Stream::STREAM_TYPE_ANALOG_SCALAR)
	{
		//Only offer to measure if not already being measured
		auto dlg = m_parent->GetMeasurementsDialog(false);
		if(!dlg || !dlg->HasStream(stream))
		{
			if(ImGui::MenuItem("Measure"))
				m_parent->GetMeasurementsDialog(true)->AddStream(stream);
			ImGui::Separator();
		}
	}

	FilterSubmenu(stream, "Bus", Filter::CAT_BUS);
	FilterSubmenu(stream, "Clocking", Filter::CAT_CLOCK);
	FilterSubmenu(stream, "Export", Filter::CAT_EXPORT);
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

			if(ImGui::MenuItem(fname.c_str(), nullptr, false, valid))
			{
				//Make the filter but don't spawn a properties dialog for it
				//If measurement, don't add trends by default
				bool addToArea = true;
				if(cat == Filter::CAT_MEASUREMENT )
					addToArea = false;
				auto f = m_parent->CreateFilter(fname, nullptr, stream, false, addToArea);

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
	@brief Make a node for a trigger
 */
void FilterGraphEditor::DoNodeForTrigger(Trigger* trig)
{
	//TODO: special color for triggers?
	//Or use a preference?
	auto& prefs = m_session.GetPreferences();
	auto tsize = ImGui::GetFontSize();
	auto color = ColorFromString("#808080");
	auto id = GetID(trig);
	auto headercolor = prefs.GetColor("Appearance.Filter Graph.header_text_color");
	auto headerfont = m_parent->GetFontPref("Appearance.Filter Graph.header_font");
	float headerheight = headerfont->FontSize * 1.5;
	float rounding = ax::NodeEditor::GetStyle().NodeRounding;

	ax::NodeEditor::BeginNode(id);
	ImGui::PushID(id.AsPointer());

	//Get node info
	auto pos = ax::NodeEditor::GetNodePosition(id);
	auto size = ax::NodeEditor::GetNodeSize(id);
	string headerText = trig->GetTriggerDisplayName();
	if(m_session.IsMultiScope())
		headerText = trig->GetScope()->m_nickname + ": " + headerText;

	//Figure out how big the header text is and reserve space for it
	auto headerSize = headerfont->CalcTextSizeA(headerfont->FontSize, FLT_MAX, 0, headerText.c_str());
	float nodewidth = max(15*tsize, headerSize.x);
	ImGui::Dummy(ImVec2(nodewidth, headerheight));

	//Table of ports
	static ImGuiTableFlags flags = 0;
	if(ImGui::BeginTable("Ports", 2, flags, ImVec2(nodewidth, 0 ) ) )
	{
		//Input ports
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		for(size_t i=0; i<trig->GetInputCount(); i++)
		{
			auto sid = GetID(pair<FlowGraphNode*, size_t>(trig, i));

			string portname("‣ ");
			portname += trig->GetInputName(i);
			ax::NodeEditor::BeginPin(sid, ax::NodeEditor::PinKind::Input);
				ax::NodeEditor::PinPivotAlignment(ImVec2(0, 0.5));
				ImGui::TextUnformatted(portname.c_str());
			ax::NodeEditor::EndPin();
		}

		//Output ports: none,  triggers are input only
		ImGui::TableNextColumn();

		ImGui::EndTable();
	}

	//Tooltip on hovered node
	if(ax::NodeEditor::GetHoveredPin())
	{}
	else if(id == ax::NodeEditor::GetHoveredNode())
	{
		ax::NodeEditor::Suspend();
			ImGui::BeginTooltip();
				ImGui::TextUnformatted("Drag node to move.\nRight click to open node properties.");
			ImGui::EndTooltip();
		ax::NodeEditor::Resume();
	}

	//Done with node
	ImGui::PopID();
	ax::NodeEditor::EndNode();

	//Draw header after the node is done
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
	@brief Make a node for a single channel, of any type

	TODO: this seems to fail hard if we do not have at least one input OR output on the node. Why?
 */
void FilterGraphEditor::DoNodeForChannel(InstrumentChannel* channel, Instrument* inst)
{
	//If the channel has no color, make it neutral gray
	//(this is often true for e.g. external trigger)
	string displaycolor = channel->m_displaycolor;
	if(displaycolor.empty())
		displaycolor = "#808080";

	auto ochan = dynamic_cast<OscilloscopeChannel*>(channel);
	auto& prefs = m_session.GetPreferences();

	//Get some configuration / style settings
	auto color = ColorFromString(displaycolor);
	auto headercolor = prefs.GetColor("Appearance.Filter Graph.header_text_color");
	auto headerfont = m_parent->GetFontPref("Appearance.Filter Graph.header_font");
	auto textfont = ImGui::GetFont();
	float headerheight = headerfont->FontSize * 1.5;
	float rounding = ax::NodeEditor::GetStyle().NodeRounding;

	auto id = GetID(channel);
	ax::NodeEditor::BeginNode(id);
	ImGui::PushID(id.AsPointer());

	//Get node info
	auto pos = ax::NodeEditor::GetNodePosition(id);
	auto size = ax::NodeEditor::GetNodeSize(id);
	string headerText = channel->GetDisplayName();

	//If NOT an oscilloscope channel, or if a multi-scope session: scope by instrument name
	if( (!ochan && inst) || (ochan && ochan->GetScope() && m_session.IsMultiScope()) )
		headerText = inst->m_nickname + ": " + headerText;

	//Figure out how big the header text is
	auto headerSize = headerfont->CalcTextSizeA(headerfont->FontSize, FLT_MAX, 0, headerText.c_str());

	//Format block type early, even though it's not drawn until later
	//so that we know how much space to allocate
	string blocktype;
	auto f = dynamic_cast<Filter*>(channel);
	if(f)
		blocktype = f->GetProtocolDisplayName();
	else
	{
		//see if input or output
		if( (dynamic_cast<PowerSupplyChannel*>(channel)) ||
			(dynamic_cast<FunctionGeneratorChannel*>(channel)) ||
			(dynamic_cast<RFSignalGeneratorChannel*>(channel)) ||
			(dynamic_cast<BERTOutputChannel*>(channel))
			)
		{
			blocktype = "Hardware output";
		}
		else
			blocktype = "Hardware input";
	}
	ImVec2 iconsize(ImGui::GetFontSize() * 6, ImGui::GetFontSize() * 3);
	auto captionsize = textfont->CalcTextSizeA(textfont->FontSize, FLT_MAX, 0, blocktype.c_str());

	//Reserve space for the center icon and node type caption
	float iconwidth = max(iconsize.x, captionsize.x);

	//Figure out how big the port text is
	float iportmax = 1;
	float oportmax = 1;
	vector<string> inames;
	vector<string> onames;
	for(size_t i=0; i<channel->GetInputCount(); i++)
	{
		auto name = string("‣ ") + channel->GetInputName(i);
		inames.push_back(name);
		iportmax = max(iportmax, textfont->CalcTextSizeA(textfont->FontSize, FLT_MAX, 0, name.c_str()).x);
	}
	for(size_t i=0; i<channel->GetStreamCount(); i++)
	{
		auto name = channel->GetStreamName(i) + " ‣";
		onames.push_back(name);
		oportmax = max(oportmax, textfont->CalcTextSizeA(textfont->FontSize, FLT_MAX, 0, name.c_str()).x);
	}
	float colswidth = iportmax + oportmax + iconwidth;
	float nodewidth = max(colswidth, headerSize.x) + 3*ImGui::GetStyle().ItemSpacing.x;

	//For really long node names, stretch icon column
	float iconcolwidth = iconwidth;
	if(headerSize.x > colswidth)
		iconcolwidth = headerSize.x - (iportmax + oportmax);

	//Reserve space for the node header
	auto startpos = ImGui::GetCursorPos();
	ImGui::Dummy(ImVec2(nodewidth, headerheight));

	//Table of inputs at left and outputs at right
	//TODO: this should move up to base class or something?
	static ImGuiTableFlags flags = /*ImGuiTableFlags_Borders*/ 0;
	StreamDescriptor hoveredStream(nullptr, 0);
	auto bodystart = ImGui::GetCursorPos();
	ImVec2 iconpos(1, 1);
	if(ImGui::BeginTable("Ports", 3, flags, ImVec2(nodewidth, 0 ) ) )
	{
		size_t maxports = max(channel->GetInputCount(), channel->GetStreamCount());

		ImGui::TableSetupColumn("inputs", ImGuiTableColumnFlags_WidthFixed, iportmax + 2);
		ImGui::TableSetupColumn("icon", ImGuiTableColumnFlags_WidthFixed, iconcolwidth + 2);
		ImGui::TableSetupColumn("outputs", ImGuiTableColumnFlags_WidthFixed, oportmax + 2);

		for(size_t i=0; i<maxports; i++)
		{
			ImGui::TableNextRow();

			//Input ports
			ImGui::TableNextColumn();
			if(i < channel->GetInputCount())
			{
				auto sid = GetID(pair<InstrumentChannel*, size_t>(channel, i));

				ax::NodeEditor::BeginPin(sid, ax::NodeEditor::PinKind::Input);
					ax::NodeEditor::PinPivotAlignment(ImVec2(0, 0.5));
					ImGui::TextUnformatted(inames[i].c_str());
				ax::NodeEditor::EndPin();
			}

			//Icon
			ImGui::TableNextColumn();
			if(i == 0)
				iconpos = ImGui::GetCursorPos();
			ImGui::Dummy(ImVec2(iconcolwidth, 1));

			//Output ports
			ImGui::TableNextColumn();
			if(i < channel->GetStreamCount())
			{
				StreamDescriptor stream(channel, i);
				auto sid = GetID(stream);

				ax::NodeEditor::BeginPin(sid, ax::NodeEditor::PinKind::Output);
					ax::NodeEditor::PinPivotAlignment(ImVec2(1, 0.5));
					RightJustifiedText(onames[i]);
				ax::NodeEditor::EndPin();

				if(sid == ax::NodeEditor::GetHoveredPin())
					hoveredStream = stream;
			}
		}

		ImGui::EndTable();
	}

	//Reserve space for icon and caption if needed
	float contentHeight = ImGui::GetCursorPos().y - bodystart.y;
	float minHeight = iconsize.y + 3*ImGui::GetStyle().ItemSpacing.y + ImGui::GetFontSize();
	if(contentHeight < minHeight)
		ImGui::Dummy(ImVec2(1, minHeight - contentHeight));

	//Tooltip on hovered output port
	if(hoveredStream)
	{
		//TODO: input port

		//Output port
		ax::NodeEditor::Suspend();
			OutputPortTooltip(hoveredStream);
		ax::NodeEditor::Resume();
	}

	//Tooltip on hovered node
	else if(id == ax::NodeEditor::GetHoveredNode())
	{
		ax::NodeEditor::Suspend();
			ImGui::BeginTooltip();
				ImGui::TextUnformatted("Drag node to move.\nRight click to open node properties.");
			ImGui::EndTooltip();
		ax::NodeEditor::Resume();
	}

	ImGui::PopID();
	ax::NodeEditor::EndNode();

	//Draw header after the node is done
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

	//Draw icon for filter blocks
	float iconshift = (iconcolwidth - iconwidth) / 2;
	ImVec2 icondelta = (iconpos - startpos) + ImVec2(ImGui::GetStyle().ItemSpacing.x + iconshift, 0);
	NodeIcon(channel, pos + icondelta, iconsize, bgList);

	//Draw icon caption
	auto textColor = prefs.GetColor("Appearance.Filter Graph.icon_color");
	ImVec2 textpos =
		pos + icondelta +
		ImVec2(0, iconsize.y + ImGui::GetStyle().ItemSpacing.y*3);
	bgList->AddText(
		textfont,
		textfont->FontSize,
		textpos + ImVec2( (iconwidth - captionsize.x)/2, 0),
		textColor,
		blocktype.c_str());
}

/**
	@brief Draws an icon showing the function of a node

	TODO: would this make more sense as a virtual?
	We don't want too much tight coupling between rendering and backend though.
 */
void FilterGraphEditor::NodeIcon(InstrumentChannel* chan, ImVec2 pos, ImVec2 iconsize, ImDrawList* list)
{
	pos.y += ImGui::GetStyle().ItemSpacing.y*2;

	auto& prefs = m_session.GetPreferences();
	auto iconfont = m_parent->GetFontPref("Appearance.Filter Graph.icon_font");
	auto color = prefs.GetColor("Appearance.Filter Graph.icon_color");

	//Some filters get graphical icons
	//TODO: something less ugly than a big if-else cascade? hash map or something?
	string iconname = "";
	if(dynamic_cast<Filter*>(chan) == nullptr)
	{
		switch(chan->GetPhysicalConnector())
		{
			case InstrumentChannel::CONNECTOR_BANANA_DUAL:
				iconname = "input-banana-dual";
				break;

			case InstrumentChannel::CONNECTOR_K_DUAL:
				iconname = "input-k-dual";
				break;
			case InstrumentChannel::CONNECTOR_K:
				iconname = "input-k";
				break;
			case InstrumentChannel::CONNECTOR_SMA:
				iconname = "input-sma";
				break;

			//TODO: make icons for these
			case InstrumentChannel::CONNECTOR_BMA:
			case InstrumentChannel::CONNECTOR_N:

			case InstrumentChannel::CONNECTOR_BNC:
			default:
				iconname = "input-bnc";
				break;
		}
	}
	else if(dynamic_cast<AddFilter*>(chan))
		iconname = "filter-add";
	else if(dynamic_cast<ClockRecoveryFilter*>(chan))
		iconname = "filter-cdrpll";
	else if(dynamic_cast<EyePattern*>(chan))
		iconname = "filter-eyepattern";
	else if(dynamic_cast<MultiplyFilter*>(chan))
		iconname = "filter-multiply";
	else if(dynamic_cast<SubtractFilter*>(chan))
		iconname = "filter-subtract";
	else if(dynamic_cast<ThresholdFilter*>(chan))
		iconname = "filter-threshold";
	else if(dynamic_cast<UpsampleFilter*>(chan))
		iconname = "filter-upsample";

	if(iconname != "")
	{
		list->AddImage(
			m_parent->GetTextureManager()->GetTexture(iconname),
			pos,
			pos + iconsize );
		return;
	}

	//If we get here, no graphical icon. Try font-based icons instead

	//Default to no icon, then add icons for basic math blocks
	string str = "";
	if(dynamic_cast<DivideFilter*>(chan))
		str = "÷";
	else if(dynamic_cast<ToneGeneratorFilter*>(chan))
		str = "∿";
	else if(dynamic_cast<AreaMeasurement*>(chan))
		str = "∫";

	//Do nothing if no icon
	if(str.empty())
		return;

	//Calculate text size so we can draw the icon
	auto size = iconfont->CalcTextSizeA(iconfont->FontSize, FLT_MAX, 0, str.c_str());
	auto radius = max(size.x, size.y)/2 + ImGui::GetStyle().ItemSpacing.x;

	//Actually draw it
	ImVec2 circlepos = pos + ImVec2(radius, radius);
	ImVec2 textpos = circlepos - size/2;
	list->AddText(
		iconfont,
		iconfont->FontSize,
		textpos,
		color,
		str.c_str());

	//Draw boundary circle
	list->AddCircle(
		circlepos,
		radius,
		color);
}

/**
	@brief Open the properties window when a node is right clicked
 */
void FilterGraphEditor::HandleNodeProperties()
{
	//Look for context menu
	ax::NodeEditor::NodeId id;
	if(ax::NodeEditor::ShowNodeContextMenu(&id))
	{
		m_selectedProperties = id;

		auto node = static_cast<FlowGraphNode*>(m_session.m_idtable[(uintptr_t)id]);
		auto trig = dynamic_cast<Trigger*>(node);
		auto o = dynamic_cast<OscilloscopeChannel*>(node);
		auto f = dynamic_cast<Filter*>(o);
		auto bo = dynamic_cast<BERTOutputChannel*>(node);
		auto bi = dynamic_cast<BERTInputChannel*>(node);

		//Make the properties window
		if(m_propertiesDialogs.find(id) == m_propertiesDialogs.end())
		{
			if(trig)
				m_propertiesDialogs[id] = make_shared<EmbeddedTriggerPropertiesDialog>(trig->GetScope());
			else if(f)
				m_propertiesDialogs[id] = make_shared<FilterPropertiesDialog>(f, m_parent, true);
			else if(bo)
				m_propertiesDialogs[id] = make_shared<BERTOutputChannelDialog>(bo, true);
			else if(bi)
				m_propertiesDialogs[id] = make_shared<BERTInputChannelDialog>(bi, m_parent, true);

			//must be last since many other types are derived from OscilloscopeChannel
			else if(o)
				m_propertiesDialogs[id] = make_shared<ChannelPropertiesDialog>(o, true);
			else
				LogWarning("Don't know how to display properties of this node!\n");
		}

		//Create the popup
		ax::NodeEditor::Suspend();
			ImGui::OpenPopup("Node Properties");
		ax::NodeEditor::Resume();
	}

	//Run the popup
	ax::NodeEditor::Suspend();
	if(ImGui::BeginPopup("Node Properties"))
	{
		auto dlg = m_propertiesDialogs[m_selectedProperties];
		if(dlg)
			dlg->RenderAsChild();
		ImGui::EndPopup();
	}
	ax::NodeEditor::Resume();
}

/**
	@brief Show add menu when background is right clicked
 */
void FilterGraphEditor::HandleBackgroundContextMenu()
{
	if(ax::NodeEditor::ShowBackgroundContextMenu())
	{
		ax::NodeEditor::Suspend();
			ImGui::OpenPopup("Add Menu");
		ax::NodeEditor::Resume();
	}

	//Run the popup
	ax::NodeEditor::Suspend();
	if(ImGui::BeginPopup("Add Menu"))
	{
		DoAddMenu();
		ImGui::EndPopup();
	}

	//If no nodes, show help message
	//(but only if popup isn't already open)
	else
	{
		if(ax::NodeEditor::GetNodeCount() == 0)
		{
			ImGui::BeginTooltip();
				ImGui::TextUnformatted("Right click to create a waveform\nor import data from a file");
			ImGui::EndTooltip();
		}
	}
	ax::NodeEditor::Resume();
}

/**
	@brief Implement the add menu
 */
void FilterGraphEditor::DoAddMenu()
{
	//Get all generation filters, sorted alphabetically
	auto& refs = m_session.GetReferenceFilters();
	vector<string> sortedNames;
	for(auto it : refs)
	{
		if(it.second->GetCategory() == Filter::CAT_GENERATION)
			sortedNames.push_back(it.first);
	}
	std::sort(sortedNames.begin(), sortedNames.end());

	if(ImGui::BeginMenu("Import"))
	{
		//Do all of the menu items
		for(auto fname : sortedNames)
		{
			//Hide everything but import filters
			if(fname.find("Import") == string::npos)
				continue;

			string shortname = fname.substr(0, fname.size() - strlen(" Import"));

			//Unlike normal filter creation, we DO want the properties dialog shown immediately
			//since we need to specify a file name to do anything
			if(ImGui::MenuItem(shortname.c_str()))
				m_parent->CreateFilter(fname, nullptr, StreamDescriptor(nullptr, 0));
		}
		ImGui::EndMenu();
	}

	if(ImGui::BeginMenu("Generate"))
	{
		//Do all of the menu items
		for(auto fname : sortedNames)
		{
			//Hide import filters
			if(fname.find("Import") != string::npos)
				continue;

			//Hide filters that have inputs
			if(refs.find(fname)->second->GetInputCount() != 0)
				continue;

			if(ImGui::MenuItem(fname.c_str()))
				m_parent->CreateFilter(fname, nullptr, StreamDescriptor(nullptr, 0));
		}

		ImGui::EndMenu();
	}

	ImGui::Separator();

	if(ImGui::MenuItem("New Group"))
	{
		auto group = make_shared<FilterGraphGroup>(*this);
		auto id = GetID(group);
		group->m_id = id;
		group->m_name = string("Group ") + to_string((intptr_t)id.AsPointer());
		m_groups.emplace(group, id);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ID allocation

/**
	@brief Allocate an ID, avoiding collisions with the session IDTable
 */
uintptr_t FilterGraphEditor::AllocateID()
{
	//Get next ID, if it's in use try the next one
	uintptr_t id = m_nextID;
	while(m_session.m_idtable.HasID(id))
		id++;

	//Reserve the ID in the session table so nobody else will try to use it
	m_session.m_idtable.ReserveID(id);

	//We now have an ID that is not in the table, so continue from there
	m_nextID = id + 1;
	return id;
}

ax::NodeEditor::PinId FilterGraphEditor::GetID(StreamDescriptor stream)
{
	//If it's in the table already, just return the ID
	if(m_streamIDMap.HasEntry(stream))
		return m_streamIDMap[stream];

	//Not in the table, allocate an ID
	auto id = AllocateID();
	m_streamIDMap.emplace(stream, id);
	return id;
}

ax::NodeEditor::PinId FilterGraphEditor::GetID(pair<FlowGraphNode*, size_t> input)
{
	//If it's in the table already, just return the ID
	if(m_inputIDMap.HasEntry(input))
		return m_inputIDMap[input];

	//Not in the table, allocate an ID
	auto id = AllocateID();
	m_inputIDMap.emplace(input, id);
	return id;
}

ax::NodeEditor::LinkId FilterGraphEditor::GetID(pair<ax::NodeEditor::PinId, ax::NodeEditor::PinId> link)
{
	//If it's in the table already, just return the ID
	if(m_linkMap.HasEntry(link))
		return m_linkMap[link];

	//Not in the table, allocate an ID
	auto id = AllocateID();
	m_linkMap.emplace(link, id);
	return id;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Save configuration

bool FilterGraphEditor::SaveSettingsCallback(
	const char* data,
	size_t size,
	ax::NodeEditor::SaveReasonFlags /*flags*/,
	void* pThis)
{
	auto ed = reinterpret_cast<FilterGraphEditor*>(pThis);
	ed->m_parent->OnGraphEditorConfigModified(std::string(data, size));
	return true;
}

/**
	@param data		Buffer to write data into
	@param pThis	Pointer to the FilterGraphEditor object

	This function is called twice, once with a null data argument to get the required size, then again
	with a valid pointer to store the data. The size must not change between the two invocations.

	@return Number of bytes required for data
 */
size_t FilterGraphEditor::LoadSettingsCallback(
	char* data,
	void* pThis)
{
	auto ed = reinterpret_cast<FilterGraphEditor*>(pThis);
	const string& blob = ed->m_parent->GetGraphEditorConfigBlob();

	if(data != nullptr)
		memcpy(data, blob.c_str(), blob.length());

	return blob.length();
}
