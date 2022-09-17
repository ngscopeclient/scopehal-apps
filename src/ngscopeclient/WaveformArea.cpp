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
	@brief Implementation of WaveformArea
 */
#include "ngscopeclient.h"
#include "WaveformArea.h"
#include "MainWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

WaveformArea::WaveformArea(StreamDescriptor stream, shared_ptr<WaveformGroup> group, MainWindow* parent)
	: m_dragContext(this)
	, m_group(group)
	, m_parent(parent)
{
	m_displayedChannels.push_back(make_shared<DisplayedChannel>(stream));
}

WaveformArea::~WaveformArea()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stream management

/**
	@brief Adds a new stream to this plot
 */
void WaveformArea::AddStream(StreamDescriptor desc)
{
	m_displayedChannels.push_back(make_shared<DisplayedChannel>(desc));
}

/**
	@brief Removes the stream at a specified index
 */
void WaveformArea::RemoveStream(size_t i)
{
	m_displayedChannels.erase(m_displayedChannels.begin() + i);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GUI widget rendering

/**
	@brief Renders a waveform area

	Returns false if the area should be closed (no more waveforms visible in it)
 */
bool WaveformArea::Render(int iArea, int numAreas, ImVec2 clientArea)
{
	float totalHeightAvailable = clientArea.y - ImGui::GetFrameHeightWithSpacing();
	float spacing = ImGui::GetFrameHeightWithSpacing() - ImGui::GetFrameHeight();
	float heightPerArea = totalHeightAvailable / numAreas;
	float unspacedHeightPerArea = heightPerArea - spacing;

	if(ImGui::BeginChild(ImGui::GetID(this), ImVec2(clientArea.x, unspacedHeightPerArea)))
	{
		auto csize = ImGui::GetContentRegionAvail();
		auto start = ImGui::GetWindowContentRegionMin();

		//Draw texture for the actual waveform
		//(todo: repeat for each channel)
		ImTextureID my_tex_id = /*m_parent->GetTexture("foo");*/ImGui::GetIO().Fonts->TexID;
		ImGui::Image(my_tex_id, ImVec2(csize.x, csize.y), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
		ImGui::SetItemAllowOverlap();

		//Drag/drop areas for splitting
		float widthOfVerticalEdge = csize.x*0.25;
		float leftOfMiddle = start.x + widthOfVerticalEdge;
		float rightOfMiddle = start.x + csize.x*0.75;
		float topOfMiddle = start.y;
		float bottomOfMiddle = start.y + csize.y;
		float widthOfMiddle = rightOfMiddle - leftOfMiddle;
		if(iArea == 0)
		{
			EdgeDropArea("top", ImVec2(leftOfMiddle, start.y), ImVec2(widthOfMiddle, csize.y*0.125), ImGuiDir_Up);
			topOfMiddle += csize.y * 0.125;
		}
		if(iArea == (numAreas-1))
		{
			bottomOfMiddle -= csize.y * 0.125;
			ImVec2 pos(leftOfMiddle, bottomOfMiddle);
			ImVec2 size(widthOfMiddle, csize.y*0.125);
			EdgeDropArea("bottom", pos, size, ImGuiDir_Down);
		}
		float heightOfMiddle = bottomOfMiddle - topOfMiddle;
		CenterDropArea(ImVec2(leftOfMiddle, topOfMiddle), ImVec2(widthOfMiddle, heightOfMiddle));
		ImVec2 edgeSize(widthOfVerticalEdge, heightOfMiddle);
		EdgeDropArea("left", ImVec2(start.x, topOfMiddle), edgeSize, ImGuiDir_Left);
		EdgeDropArea("right", ImVec2(rightOfMiddle, topOfMiddle), edgeSize, ImGuiDir_Right);

		//Draw control widgets
		ImGui::SetCursorPos(ImGui::GetWindowContentRegionMin());
		ImGui::BeginGroup();

			for(size_t i=0; i<m_displayedChannels.size(); i++)
				DraggableButton(m_displayedChannels[i], i);

		ImGui::EndGroup();
		ImGui::SetItemAllowOverlap();
	}
	ImGui::EndChild();

	if(m_displayedChannels.empty())
		return false;
	return true;
}

/**
	@brief Drop area for edge of the plot

	Dropping a waveform in here splits and forms a new group
 */
void WaveformArea::EdgeDropArea(const string& name, ImVec2 start, ImVec2 size, ImGuiDir splitDir)
{
	ImGui::SetCursorPos(start);
	ImGui::InvisibleButton(name.c_str(), size);
	ImGui::SetItemAllowOverlap();

	//Add drop target
	if(ImGui::BeginDragDropTarget())
	{
		auto payload = ImGui::AcceptDragDropPayload("Waveform");
		if( (payload != nullptr) && (payload->DataSize == sizeof(WaveformDragContext*)) )
		{
			auto context = reinterpret_cast<WaveformDragContext*>(payload->Data);
			auto stream = context->m_sourceArea->GetStream(context->m_streamIndex);

			//Add request to split our current group
			m_parent->QueueSplitGroup(m_group, splitDir, stream);

			//Remove the stream from the originating waveform area
			context->m_sourceArea->RemoveStream(context->m_streamIndex);
		}

		ImGui::EndDragDropTarget();
	}
}

/**
	@brief Drop area for the middle of the plot

	Dropping a waveform in here adds it to the plot
 */
void WaveformArea::CenterDropArea(ImVec2 start, ImVec2 size)
{
	ImGui::SetCursorPos(start);
	ImGui::InvisibleButton("center", size);
	ImGui::SetItemAllowOverlap();

	//Add drop target
	if(ImGui::BeginDragDropTarget())
	{
		auto payload = ImGui::AcceptDragDropPayload("Waveform");
		if( (payload != nullptr) && (payload->DataSize == sizeof(WaveformDragContext*)) )
		{
			auto context = reinterpret_cast<WaveformDragContext*>(payload->Data);
			auto stream = context->m_sourceArea->GetStream(context->m_streamIndex);

			//Add the new stream to us
			//TODO: copy view settings from the DisplayedChannel over?
			AddStream(stream);

			//Remove the stream from the originating waveform area
			context->m_sourceArea->RemoveStream(context->m_streamIndex);
		}

		ImGui::EndDragDropTarget();
	}
}


void WaveformArea::DraggableButton(shared_ptr<DisplayedChannel> chan, size_t index)
{
	ImGui::Button(chan->GetName().c_str());

	if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
	{
		m_dragContext.m_streamIndex = index;
		ImGui::SetDragDropPayload("Waveform", &m_dragContext, sizeof(WaveformDragContext*));

		//Preview of what we're dragging
		ImGui::Text("Drag %s", chan->GetName().c_str());

		ImGui::EndDragDropSource();
	}
}
