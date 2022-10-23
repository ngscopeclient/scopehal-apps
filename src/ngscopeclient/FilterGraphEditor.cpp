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

	//for some reason node editor wants colors as vec4 not ImU32
	auto& prefs = m_session.GetPreferences();
	auto validcolor = ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Filter Graph.valid_link_color"));
	auto invalidcolor = ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Filter Graph.invalid_link_color"));

	//Handle creation requests
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

				//Make sure both paths exist
				if(m_inputIDMap.HasEntry(endId) && m_streamIDMap.HasEntry(startId))
				{
					//See if the path is valid
					auto inputPort = m_inputIDMap[endId];
					auto stream = m_streamIDMap[startId];
					if(inputPort.first->ValidateChannel(inputPort.second, stream))
					{
						if(ax::NodeEditor::AcceptNewItem(validcolor))
						{
							//Hook it up
							inputPort.first->SetInput(inputPort.second, stream);

							//Update names, if needed
							auto f = dynamic_cast<Filter*>(inputPort.first);
							if(f)
							{
								if(f->IsUsingDefaultName())
									f->SetDefaultName();
							}
						}
					}

					//Not valid
					else
						ax::NodeEditor::RejectNewItem(invalidcolor);
				}
			}
		}
	}
	ax::NodeEditor::EndCreate();

	ax::NodeEditor::End();
	ax::NodeEditor::SetCurrentEditor(nullptr);

	return true;
}

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

				ax::NodeEditor::BeginPin(sid, ax::NodeEditor::PinKind::Input);
					ImGui::TextUnformatted(f->GetInputName(i).c_str());
				ax::NodeEditor::EndPin();
			}
		}

		//Output ports
		ImGui::TableNextColumn();
		for(size_t i=0; i<channel->GetStreamCount(); i++)
		{
			auto sid = GetID(StreamDescriptor(channel, i));

			ax::NodeEditor::BeginPin(sid, ax::NodeEditor::PinKind::Output);
				ImGui::TextUnformatted(channel->GetStreamName(i).c_str());
			ax::NodeEditor::EndPin();
		}

		ImGui::EndTable();
	}

	//Table of properties TODO

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
		channel->GetDisplayName().c_str());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ID allocation

ax::NodeEditor::NodeId FilterGraphEditor::GetID(OscilloscopeChannel* chan)
{
	//If it's in the table already, just return the ID
	auto it = m_channelIDMap.find(chan);
	if(it != m_channelIDMap.end())
		return it->second;

	//Not in the table, allocate an ID
	int id = m_nextID;
	m_nextID ++;
	m_channelIDMap[chan] = id;
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
