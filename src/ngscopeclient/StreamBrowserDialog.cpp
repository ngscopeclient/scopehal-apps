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
	@brief Implementation of StreamBrowserDialog
 */

#include "ngscopeclient.h"
#include "StreamBrowserDialog.h"
#include "MainWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

StreamBrowserDialog::StreamBrowserDialog(Session& session, MainWindow* parent)
	: Dialog("Stream Browser", "Stream Browser", ImVec2(300, 400))
	, m_session(session)
	, m_parent(parent)
{

}

StreamBrowserDialog::~StreamBrowserDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool StreamBrowserDialog::DoRender()
{
	// Some helpers for rendering widgets that appear in the StreamBrowserDialog.
	
	// Render a link of the "Sample rate: 4 GSa/s" type that shows up in the
	// scope properties box.
	auto renderInfoLink = [](const char *label, const char *linktext, bool &clicked, bool &hovered)
	{
		ImGui::Text("%s: ", label);
		ImGui::SameLine(0, 0);
		clicked |= ImGui::TextLink(linktext);
		hovered |= ImGui::IsItemHovered();
	};
	
	float badgeXMin; // left edge over which we must not overrun
	float badgeXCur; // right edge to render the next badge against
	auto startBadgeLine = [&badgeXMin, &badgeXCur]()
	{
		ImGuiWindow *window = ImGui::GetCurrentWindowRead();
		// roughly, what ImGui::GetCursorPosPrevLineX would be, if it existed; convert from absolute-space to window-space
		badgeXMin = (window->DC.CursorPosPrevLine - window->Pos + window->Scroll).x;
		badgeXCur = ImGui::GetWindowContentRegionMax().x;
	};
	auto renderBadge = [&badgeXMin, &badgeXCur](ImVec4 color, ... /* labels, ending in NULL */)
	{
		va_list ap;
		va_start(ap, color);
		
		/* XXX: maybe color should be a prefs string? */
		
		while (const char *label = va_arg(ap, const char *)) {
			float xsz = ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().FramePadding.x * 2;
			if ((badgeXCur - xsz) < badgeXMin) {
				continue;
			}
			
			// ok, we have enough space -- commit to it!
			badgeXCur -= xsz - ImGui::GetStyle().ItemSpacing.x;
			ImGui::SameLine(badgeXCur);
			ImGui::PushStyleColor(ImGuiCol_Button, color);
			ImGui::SmallButton(label);
			ImGui::PopStyleColor();
			break;
		}
	};
	
	//Add all instruments
	auto insts = m_session.GetInstruments();
	for(auto inst : insts)
	{
		bool instIsOpen = ImGui::TreeNodeEx(inst->m_nickname.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
		startBadgeLine();
		
		// Render ornaments for this instrument: offline, trigger status, ...
		if (auto scope = std::dynamic_pointer_cast<Oscilloscope>(inst)) {
			if (scope->IsOffline()) {
				renderBadge(ImVec4(0.8, 0.3, 0.3, 1.0) /* XXX: pull color from prefs */, "OFFLINE", "OFFL", NULL);
			}
		}
		
		if(instIsOpen)
		{
			if (auto scope = std::dynamic_pointer_cast<Oscilloscope>(inst)) {
				ImGui::BeginChild("sample_params", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border);
				
				auto srate_txt = Unit(Unit::UNIT_SAMPLERATE).PrettyPrint(scope->GetSampleRate());
				auto sdepth_txt = Unit(Unit::UNIT_SAMPLEDEPTH).PrettyPrint(scope->GetSampleDepth());
				
				bool clicked = false;
				bool hovered = false;
				renderInfoLink("Sample rate", srate_txt.c_str(), clicked, hovered);
				renderInfoLink("Sample depth", sdepth_txt.c_str(), clicked, hovered);
				if (clicked) {
					m_parent->ShowTimebaseProperties();
				}
				if (hovered) {
					m_parent->AddStatusHelp("mouse_lmb", "Open timebase properties");
				}
				
				ImGui::EndChild();
			}
			
			for(size_t i=0; i<inst->GetChannelCount(); i++)
			{
				auto chan = inst->GetChannel(i);

				bool singleStream = chan->GetStreamCount() == 1;
				bool renderScopeProps = false;
				if (auto scopechan = dynamic_cast<OscilloscopeChannel *>(chan)) {
					if (scopechan->IsEnabled()) {
						renderScopeProps = true;
					}
				}

				bool hasChildren = !singleStream || renderScopeProps;

				if (chan->m_displaycolor != "") {
					ImGui::PushStyleColor(ImGuiCol_Text, ColorFromString(chan->m_displaycolor));
				}
				bool open = ImGui::TreeNodeEx(chan->GetDisplayName().c_str(), ImGuiTreeNodeFlags_DefaultOpen | (!hasChildren ? ImGuiTreeNodeFlags_Leaf : 0));
				if (chan->m_displaycolor != "") {
					ImGui::PopStyleColor();
				}
				startBadgeLine();
				
				if (auto scopechan = dynamic_cast<OscilloscopeChannel *>(chan)) {
					if (!scopechan->IsEnabled()) {
						renderBadge(ImVec4(0.4, 0.4, 0.4, 1.0) /* XXX: pull color from prefs */, "disabled", "disa", NULL);
					}
				}

				//Single stream: drag the stream not the channel
				if(singleStream)
				{
					StreamDescriptor s(chan, 0);
					if(ImGui::BeginDragDropSource())
					{
						if(s.GetType() == Stream::STREAM_TYPE_ANALOG_SCALAR)
							ImGui::SetDragDropPayload("Scalar", &s, sizeof(s));
						else
							ImGui::SetDragDropPayload("Stream", &s, sizeof(s));

						ImGui::TextUnformatted(s.GetName().c_str());
						ImGui::EndDragDropSource();
					}
					else
						DoItemHelp();
				}

				//Drag source for the channel itself (if we have zero or >1 streams)
				else if(ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload("Channel", &chan, sizeof(chan));

					ImGui::TextUnformatted(chan->GetDisplayName().c_str());
					ImGui::EndDragDropSource();
				}
				
				if(open)
				{
					for(size_t j=0; j<chan->GetStreamCount(); j++)
					{
						if (!singleStream)
						{
							ImGui::Selectable(chan->GetStreamName(j).c_str());

							StreamDescriptor s(chan, j);
							if(ImGui::BeginDragDropSource())
							{
								if(s.GetType() == Stream::STREAM_TYPE_ANALOG_SCALAR)
									ImGui::SetDragDropPayload("Scalar", &s, sizeof(s));
								else
									ImGui::SetDragDropPayload("Stream", &s, sizeof(s));

								ImGui::TextUnformatted(s.GetName().c_str());
								ImGui::EndDragDropSource();
							}
							else
								DoItemHelp();
						}
						if (renderScopeProps)
						{
							auto scopechan = dynamic_cast<OscilloscopeChannel *>(chan);
							ImGui::BeginChild("scope_params", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border);
				
							auto offset_txt = Unit(Unit::UNIT_VOLTS).PrettyPrint(scopechan->GetOffset(j));
							auto range_txt = Unit(Unit::UNIT_VOLTS).PrettyPrint(scopechan->GetVoltageRange(j));
				
							bool clicked = false;
							bool hovered = false;
							renderInfoLink("Offset", offset_txt.c_str(), clicked, hovered);
							renderInfoLink("Voltage range", range_txt.c_str(), clicked, hovered);
							if (clicked) {
								/* XXX: refactor to be more like FilterGraphEditor::HandleNodeProperties? */
								m_parent->ShowChannelProperties(scopechan);
							}
							if (hovered) {
								m_parent->AddStatusHelp("mouse_lmb", "Open channel properties");
							}
				
							ImGui::EndChild();
						}
					}
				}

				if(open)
					ImGui::TreePop();
			}

			ImGui::TreePop();
		}
	}

	//Add all filters
	if(ImGui::TreeNodeEx("Filters", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto filters = Filter::GetAllInstances();
		for(auto f : filters)
		{
			bool open = ImGui::TreeNodeEx(f->GetDisplayName().c_str(), ImGuiTreeNodeFlags_DefaultOpen);

			//Single stream: drag the stream not the channel
			bool singleStream = f->GetStreamCount() == 1;
			if(singleStream)
			{
				StreamDescriptor s(f, 0);
				if(ImGui::BeginDragDropSource())
				{
					if(s.GetType() == Stream::STREAM_TYPE_ANALOG_SCALAR)
						ImGui::SetDragDropPayload("Scalar", &s, sizeof(s));
					else
						ImGui::SetDragDropPayload("Stream", &s, sizeof(s));

					ImGui::TextUnformatted(s.GetName().c_str());
					ImGui::EndDragDropSource();
				}
				else
					DoItemHelp();
			}

			//Drag source for the channel itself (if we have zero or >1 streams)
			else if(ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload("Channel", &f, sizeof(f));

				ImGui::TextUnformatted(f->GetDisplayName().c_str());
				ImGui::EndDragDropSource();
			}

			if(open && !singleStream)
			{
				for(size_t j=0; j<f->GetStreamCount(); j++)
				{
					ImGui::Selectable(f->GetStreamName(j).c_str());

					StreamDescriptor s(f, j);
					if(ImGui::BeginDragDropSource())
					{
						if(s.GetType() == Stream::STREAM_TYPE_ANALOG_SCALAR)
							ImGui::SetDragDropPayload("Scalar", &s, sizeof(s));
						else
							ImGui::SetDragDropPayload("Stream", &s, sizeof(s));

						ImGui::TextUnformatted(s.GetName().c_str());
						ImGui::EndDragDropSource();
					}
					else
						DoItemHelp();
				}
			}

			if(open)
				ImGui::TreePop();
		}
		ImGui::TreePop();
	}

	return true;
}

void StreamBrowserDialog::DoItemHelp()
{
	if(ImGui::IsItemHovered())
		m_parent->AddStatusHelp("mouse_lmb_drag", "Add to filter graph or plot");
}
