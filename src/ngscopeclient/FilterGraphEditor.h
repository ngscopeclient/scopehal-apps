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
	@brief Declaration of FilterGraphEditor
 */
#ifndef FilterGraphEditor_h
#define FilterGraphEditor_h

#include "Dialog.h"
#include "Session.h"
#include "Bijection.h"
class ChannelPropertiesDialog;

#include <imgui_node_editor.h>

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

class FilterGraphEditor : public Dialog
{
public:
	FilterGraphEditor(Session& session, MainWindow* parent);
	virtual ~FilterGraphEditor();

	virtual bool DoRender();

protected:
	void OutputPortTooltip(StreamDescriptor stream);
	void DoNodeForChannel(OscilloscopeChannel* channel);
	void HandleNodeProperties();
	void HandleLinkCreationRequests(Filter*& fReconfigure);
	void HandleLinkDeletionRequests(Filter*& fReconfigure);
	void HandleBackgroundContextMenu();
	void DoAddMenu();
	bool IsBackEdge(OscilloscopeChannel* src, OscilloscopeChannel* dst);
	void HandleOverlaps();
	void ClearOldPropertiesDialogs();

	void FilterMenu(StreamDescriptor src);
	void FilterSubmenu(StreamDescriptor src, const std::string& name, Filter::Category cat);

	///@brief Session being manipuulated
	Session& m_session;

	///@brief Top level window
	MainWindow* m_parent;

	///@brief Graph editor setup
	ax::NodeEditor::Config m_config;

	///@brief Context containing current state of the graph editor
	ax::NodeEditor::EditorContext* m_context;

	///@brief Map of channels / filters to IDs
	Bijection<
		OscilloscopeChannel*,
		ax::NodeEditor::NodeId,
		std::less<OscilloscopeChannel*>,
		lessID<ax::NodeEditor::NodeId> > m_channelIDMap;

	///@brief Map of streams to output port IDs
	Bijection<
		StreamDescriptor,
		ax::NodeEditor::PinId,
		std::less<StreamDescriptor>,
		lessID<ax::NodeEditor::PinId> > m_streamIDMap;

	///@brief Map of (channel, input number) to input port IDs
	Bijection<
		std::pair<OscilloscopeChannel*, int>,
		ax::NodeEditor::PinId,
		std::less< std::pair<OscilloscopeChannel*, int> >,
		lessID<ax::NodeEditor::PinId> > m_inputIDMap;

	///@brief Map of (ID, ID) to link IDs
	Bijection<
		std::pair<ax::NodeEditor::PinId, ax::NodeEditor::PinId>,
		ax::NodeEditor::LinkId,
		lessIDPair,
		lessID<ax::NodeEditor::LinkId> > m_linkMap;

	///@brief Next ID to be allocated
	int m_nextID;

	ax::NodeEditor::NodeId GetID(OscilloscopeChannel* chan);
	ax::NodeEditor::PinId GetID(StreamDescriptor stream);
	ax::NodeEditor::PinId GetID(std::pair<OscilloscopeChannel*, size_t> input);
	ax::NodeEditor::LinkId GetID(std::pair<ax::NodeEditor::PinId, ax::NodeEditor::PinId> link);

	///@brief Source stream of the newly created filter
	StreamDescriptor m_newFilterSourceStream;

	///@brief Properties dialogs for channels to be displayed inside nodes
	std::map<
		ax::NodeEditor::NodeId,
		std::shared_ptr<ChannelPropertiesDialog>,
		lessID<ax::NodeEditor::NodeId> > m_propertiesDialogs;

	///@brief Node whose properties we're currently interacting with
	ax::NodeEditor::NodeId m_selectedProperties;

	ImVec2 m_createMousePos;
};

#endif
