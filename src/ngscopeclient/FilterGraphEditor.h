/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg                                                                          *
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
	@brief Declaration of FilterGraphEditor
 */
#ifndef FilterGraphEditor_h
#define FilterGraphEditor_h

#include "Dialog.h"
#include "Session.h"
#include "Bijection.h"
class EmbeddableDialog;

#include <imgui_node_editor.h>
#include <imgui_node_editor_internal.h>

#include <typeinfo>
#include <typeindex>

template<class T>
class lessID
{
public:
	bool operator()(const T& a, const T& b) const
	{ return a.AsPointer() < b.AsPointer(); }
};

class lessIDPair
{
public:
	bool operator()(
		const std::pair<ax::NodeEditor::PinId, ax::NodeEditor::PinId>& a,
		const std::pair<ax::NodeEditor::PinId, ax::NodeEditor::PinId>& b) const
	{
		auto fpa = a.first.AsPointer();
		auto fpb = b.first.AsPointer();
		if(fpa < fpb)
			return true;
		else if(fpa > fpb)
			return false;
		else
			return a.second.AsPointer() < b.second.AsPointer();
	}
};

class FilterGraphEditor;

class FilterGraphGroup
{
public:

	FilterGraphGroup(FilterGraphEditor& ed);

	///@brief Display name of the group
	std::string m_name;

	///@brief ID of the group
	ax::NodeEditor::NodeId m_id;

	///@brief ID of the dummy node for output ports
	ax::NodeEditor::NodeId m_outputId;

	///@brief ID of the dummy node for input ports
	ax::NodeEditor::NodeId m_inputId;

	///@brief List of nodes we contain (by ID)
	std::set<ax::NodeEditor::NodeId, lessID<ax::NodeEditor::NodeId> > m_children;

	///@brief List of output pins we contain on our child nodes
	std::set<ax::NodeEditor::PinId, lessID<ax::NodeEditor::PinId> > m_childSourcePins;

	///@brief List of input pins we contain on our child nodes
	std::set<ax::NodeEditor::PinId, lessID<ax::NodeEditor::PinId> > m_childSinkPins;

	///@brief Map of streams to hierarchial output port IDs
	Bijection<
		StreamDescriptor,
		ax::NodeEditor::PinId,
		std::less<StreamDescriptor>,
		lessID<ax::NodeEditor::PinId> > m_hierOutputMap;

	///@brief Map of streams to hierarchial output port internal-facing port IDs
	Bijection<
		StreamDescriptor,
		ax::NodeEditor::PinId,
		std::less<StreamDescriptor>,
		lessID<ax::NodeEditor::PinId> > m_hierOutputInternalMap;

	///@brief Map of streams to internal link IDs
	Bijection<
		StreamDescriptor,
		ax::NodeEditor::LinkId,
		std::less<StreamDescriptor>,
		lessID<ax::NodeEditor::LinkId> > m_hierOutputLinkMap;

	///@brief Map of streams to hierarchial input port IDs
	Bijection<
		std::pair<FlowGraphNode*, int>,
		ax::NodeEditor::PinId,
		std::less< std::pair<FlowGraphNode*, int> >,
		lessID<ax::NodeEditor::PinId> > m_hierInputMap;

	///@brief Map of streams to hierarchial input port internal-facing port IDs
	Bijection<
		std::pair<FlowGraphNode*, int>,
		ax::NodeEditor::PinId,
		std::less< std::pair<FlowGraphNode*, int> >,
		lessID<ax::NodeEditor::PinId> > m_hierInputInternalMap;

	///@brief Map of streams to internal link IDs
	Bijection<
		std::pair<FlowGraphNode*, int>,
		ax::NodeEditor::LinkId,
		std::less< std::pair<FlowGraphNode*, int> >,
		lessID<ax::NodeEditor::LinkId> > m_hierInputLinkMap;

	void RefreshChildren();
	void RefreshLinks();
	void MoveBy(ImVec2 displacement);

protected:
	FilterGraphEditor& m_parent;
};

class FilterGraphEditor : public Dialog
{
public:
	FilterGraphEditor(Session& session, MainWindow* parent);
	virtual ~FilterGraphEditor();

	virtual bool Render();
	virtual bool DoRender();

	std::map<uintptr_t, std::string> GetGroupIDs();

protected:
	friend class FilterGraphGroup;

	std::map<Instrument*, std::vector<InstrumentChannel*> > GetAllChannels();
	std::vector<FlowGraphNode*> GetAllNodes();

	void RefreshGroupPorts();

	ax::NodeEditor::PinId CanonicalizePin(ax::NodeEditor::PinId port);

	void OutputPortTooltip(StreamDescriptor stream);
	void DoNodeForGroup(std::shared_ptr<FilterGraphGroup> group);
	void DoInternalLinksForGroup(std::shared_ptr<FilterGraphGroup> group);
	void DoNodeForGroupOutputs(std::shared_ptr<FilterGraphGroup> group);
	void DoNodeForGroupInputs(std::shared_ptr<FilterGraphGroup> group);
	void DoNodeForChannel(InstrumentChannel* channel, Instrument* inst);
	void DoNodeForTrigger(Trigger* trig);
	void HandleNodeProperties();
	void HandleLinkCreationRequests(Filter*& fReconfigure);
	void HandleLinkDeletionRequests(Filter*& fReconfigure);
	void HandleBackgroundContextMenu();
	void DoAddMenu();
	bool IsBackEdge(FlowGraphNode* src, FlowGraphNode* dst);

	void HandleOverlaps();
	void CalculateNodeForces(
		const std::vector<ax::NodeEditor::NodeId>& nodes,
		const std::vector<bool>& isgroup,
		const std::vector<bool>& dragging,
		const std::vector<bool>& nocollide,
		const std::vector<ImVec2>& positions,
		const std::vector<ImVec2>& sizes,
		std::vector<ImVec2>& forces);
	void ApplyNodeForces(
		const std::vector<ax::NodeEditor::NodeId>& nodes,
		const std::vector<bool>& isgroup,
		const std::vector<bool>& dragging,
		const std::vector<ImVec2>& positions,
		std::vector<ImVec2>& forces);

	void ClearOldPropertiesDialogs();

	void NodeIcon(InstrumentChannel* chan, ImVec2 iconpos, ImVec2 iconsize, ImDrawList* list);

	void FilterMenu(StreamDescriptor src);
	void FilterSubmenu(StreamDescriptor src, const std::string& name, Filter::Category cat);
	void CreateChannelMenu();

	///@brief Session being manipulated
	Session& m_session;

	///@brief Top level window
	MainWindow* m_parent;

	///@brief Graph editor setup
	ax::NodeEditor::Config m_config;

	///@brief Context containing current state of the graph editor
	ax::NodeEditor::EditorContext* m_context;

	///@brief Map of streams to output port IDs
	Bijection<
		StreamDescriptor,
		ax::NodeEditor::PinId,
		std::less<StreamDescriptor>,
		lessID<ax::NodeEditor::PinId> > m_streamIDMap;

	///@brief Map of (channel, input number) to input port IDs
	Bijection<
		std::pair<FlowGraphNode*, int>,
		ax::NodeEditor::PinId,
		std::less< std::pair<FlowGraphNode*, int> >,
		lessID<ax::NodeEditor::PinId> > m_inputIDMap;

	///@brief Map of (ID, ID) to link IDs
	Bijection<
		std::pair<ax::NodeEditor::PinId, ax::NodeEditor::PinId>,
		ax::NodeEditor::LinkId,
		lessIDPair,
		lessID<ax::NodeEditor::LinkId> > m_linkMap;

	///@brief Map of signal sources to the group the source node is in (if there is one)
	Bijection<FlowGraphNode*, std::shared_ptr<FilterGraphGroup> > m_nodeGroupMap;

	///@brief Next link/port ID to be allocated
	uintptr_t m_nextID;

	ax::NodeEditor::NodeId GetID(FlowGraphNode* node);

	ax::NodeEditor::NodeId GetID(InstrumentChannel* chan)
	{ return m_session.m_idtable.emplace(chan); }

	ax::NodeEditor::NodeId GetID(Trigger* trig)
	{ return m_session.m_idtable.emplace(trig); }

	ax::NodeEditor::NodeId GetID(std::shared_ptr<FilterGraphGroup> group)
	{ return m_session.m_idtable.emplace(group.get()); }

	uintptr_t AllocateID();
	ax::NodeEditor::PinId GetID(StreamDescriptor stream);
	ax::NodeEditor::PinId GetID(std::pair<FlowGraphNode*, size_t> input);
	ax::NodeEditor::LinkId GetID(std::pair<ax::NodeEditor::PinId, ax::NodeEditor::PinId> link);

	ax::NodeEditor::PinId GetSourcePinForLink(StreamDescriptor source, FlowGraphNode* sink);
	ax::NodeEditor::PinId GetSinkPinForLink(StreamDescriptor source, std::pair<FlowGraphNode*, int> sink);

	///@brief Source stream of the newly created filter
	StreamDescriptor m_newFilterSourceStream;

	///@brief Properties dialogs for channels to be displayed inside nodes
	std::map<
		ax::NodeEditor::NodeId,
		std::shared_ptr<EmbeddableDialog>,
		lessID<ax::NodeEditor::NodeId> > m_propertiesDialogs;

	///@brief Node whose properties we're currently interacting with
	ax::NodeEditor::NodeId m_selectedProperties;

	ImVec2 m_createMousePos;

	///@brief Input we're considering hooking a new channel up to
	std::pair<FlowGraphNode*, int> m_createInput;

	///@brief Groups
	Bijection<
		std::shared_ptr<FilterGraphGroup>,
		ax::NodeEditor::NodeId,
		std::less< std::shared_ptr<FilterGraphGroup> >,
		lessID<ax::NodeEditor::NodeId>
		 > m_groups;

	///@brief Map of filter types to class names
	std::map<std::type_index, std::string> m_filterIconMap;

	//DEBUG: forces for display
	std::map<
		ax::NodeEditor::NodeId,
		ImVec2,
		lessID<ax::NodeEditor::NodeId>
		> m_nodeForces;

	//DEBUG: render vector for force
	void RenderForceVector(ImDrawList* list, ImVec2 pos, ImVec2 size, ImVec2 vec);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Serialization

	static bool SaveSettingsCallback(
		const char* data,
		size_t size,
		ax::NodeEditor::SaveReasonFlags flags,
		void* pThis);

	static size_t LoadSettingsCallback(char* data, void* pThis);
};

#endif
