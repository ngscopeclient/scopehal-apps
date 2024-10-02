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

/* @brief Width used to display progress bars (e.g. download progress bar)*/
#define PROGRESS_BAR_WIDTH	90

using namespace std;

// TODO move this to OscilloscopeChannel class along with GetDwnwloadProgress() API
enum DownloadState : int {
	DOWNLOAD_PROGRESS_DISABLED = -3,
	DOWNLOAD_NONE = -2,
	DOWNLOAD_WAITING = -1,
	DOWNLOAD_STARTED = 0,
	DOWNLOAD_FINISHED = 100
};

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
	auto renderDownloadProgress = [&badgeXMin, &badgeXCur](int progress)
	{
		static const char* const download[] = {"DOWNLOADING", "DOWNLOAD" ,"DL","D", "", NULL};
		static const char* const wait[]     = {"WAITING..." , "WAITING"  ,"WA","W", "", NULL};
		static const char* const stop[]     = {"STOPPED"    , "STOP"     ,"ST","S", "", NULL};
		static const char* const ready[]    = {"READY"      , "RDY"      ,"RY","R", "", NULL};
		static const char* const* labels;
		ImVec4 color;
		switch(progress)
		{
			case DownloadState::DOWNLOAD_NONE:
				labels = stop;
				color.x = 0.8 ; color.y=0.3 ; color.z=0.3; color.w=1.0;
				break;
			case DownloadState::DOWNLOAD_WAITING:
				labels = wait;
				color.x = 0.8 ; color.y=0.3 ; color.z=0.3; color.w=1.0;
				break;
			case DownloadState::DOWNLOAD_FINISHED:
				labels = ready;
				color.x = 0.3 ; color.y=0.8 ; color.z=0.3; color.w=1.0;
				break;
			default:
				labels = download;
				color.x = 0.7 ; color.y=0.7 ; color.z=0.3; color.w=1.0;
		}
		// Only show progress bar if an download is proceeding
		bool hasProgress = (progress >= DownloadState::DOWNLOAD_WAITING) && (progress <= DownloadState::DOWNLOAD_FINISHED);
		bool hasLabel;
		int labelIndex = 0;
		while (const char *label = labels[labelIndex]) 
		{
			hasLabel = strlen(label)>0;
			float xsz = ImGui::CalcTextSize(label).x + (hasProgress ? PROGRESS_BAR_WIDTH : 0) + (ImGui::GetStyle().ItemSpacing.x) * ((hasProgress && hasLabel ? 1 : 0)+(hasLabel ? 1 : 0)) + ImGui::GetStyle().FramePadding.x * 2;
			if ((badgeXCur - xsz) < badgeXMin) {
				labelIndex++;
				continue;
			}
			
			// ok, we have enough space -- commit to it!
			badgeXCur -= xsz - ImGui::GetStyle().ItemSpacing.x;
			ImGui::SameLine(badgeXCur);
			if(hasLabel)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, color);
				ImGui::SmallButton(labels[labelIndex]);
				ImGui::PopStyleColor();
				if(hasProgress) 
					ImGui::SameLine();
			}
			if(hasProgress)
			{
				ImGui::ProgressBar(((float)progress)/100, ImVec2(PROGRESS_BAR_WIDTH, ImGui::GetFontSize()));		
			}

			break;
		}
	};
	
	//Add all instruments
	auto insts = m_session.GetInstruments();
	for(auto inst : insts)
	{
		bool instIsOpen = ImGui::TreeNodeEx(inst->m_nickname.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
		startBadgeLine();

		auto state = m_session.GetInstrumentConnectionState(inst);
		
		// Render ornaments for this instrument: offline, trigger status, ...
		auto scope = std::dynamic_pointer_cast<Oscilloscope>(inst);
		if (scope) {
			if (scope->IsOffline()) {
				renderBadge(ImVec4(0.8, 0.3, 0.3, 1.0) /* XXX: pull color from prefs */, "OFFLINE", "OFFL", NULL);
			} else {
				Oscilloscope::TriggerMode mode = state ? state->m_lastTriggerState : Oscilloscope::TRIGGER_MODE_STOP;
				switch (mode) {
				case Oscilloscope::TRIGGER_MODE_RUN:
					/* prefer language "ARMED" to "RUN":
					 * "RUN" could mean either "waiting
					 * for trigger" or "currently
					 * capturing samples post-trigger",
					 * "ARMED" is unambiguous */
					renderBadge(ImVec4(0.3, 0.8, 0.3, 1.0), "ARMED", "A", NULL);
					break;
				case Oscilloscope::TRIGGER_MODE_STOP:
					renderBadge(ImVec4(0.8, 0.3, 0.3, 1.0), "STOPPED", "STOP", "S", NULL);
					break;
				case Oscilloscope::TRIGGER_MODE_TRIGGERED:
					renderBadge(ImVec4(0.7, 0.7, 0.3, 1.0), "TRIGGERED", "TRIG'D", "T'D", "T", NULL);
					break;
				case Oscilloscope::TRIGGER_MODE_WAIT:
					/* prefer language "BUSY" to "WAIT":
					 * "WAIT" could mean "waiting for
					 * trigger", "BUSY" means "I am
					 * doing something internally and am
					 * not ready for some reason" */
					renderBadge(ImVec4(0.8, 0.3, 0.3, 1.0), "BUSY", "B", NULL);
					break;
				case Oscilloscope::TRIGGER_MODE_AUTO:
					renderBadge(ImVec4(0.3, 0.8, 0.3, 1.0), "AUTO", "A", NULL);
					break;
				default:
					break;
				}
			}
		}
		
		if(instIsOpen)
		{
			if (scope) {
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
			
			size_t channelCount = inst->GetChannelCount();
			for(size_t i=0; i<channelCount; i++)
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
				bool triggerArmed = scope ? scope->IsTriggerArmed() : false;

				if (chan->m_displaycolor != "") {
					ImGui::PushStyleColor(ImGuiCol_Text, ColorFromString(chan->m_displaycolor));
				}
				bool open = ImGui::TreeNodeEx(chan->GetDisplayName().c_str(), ImGuiTreeNodeFlags_DefaultOpen | (!hasChildren ? ImGuiTreeNodeFlags_Leaf : 0));
				if (chan->m_displaycolor != "") {
					ImGui::PopStyleColor();
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
				
				// Channel decoration
				startBadgeLine();
				auto scopechan = dynamic_cast<OscilloscopeChannel *>(chan);
				if (scopechan) {
					if (!scopechan->IsEnabled()) {
						renderBadge(ImVec4(0.4, 0.4, 0.4, 1.0) /* XXX: pull color from prefs */, "disabled", "disa", NULL);
					}
					else if(scope) {
						int progress = DownloadState::DOWNLOAD_NONE;
						// TODO get this out of GetDownloadState() channel API when implemented
						// For now, we simulate it for demonstration purpose
						uint64_t sampleDepth = scope->GetSampleDepth();
						if(sampleDepth <= 100000)
						{
							progress = DownloadState::DOWNLOAD_PROGRESS_DISABLED;
						}
						if(progress != DownloadState::DOWNLOAD_PROGRESS_DISABLED)
						{
							if(state)
							{
								progress = state->GetChannelDownloadState(i);
								if(triggerArmed)
								{
									// Render method is called 60 times per second
									// We want to simulate a download time of 1 MSample/S
									uint64_t downloadIncrement = std::max(((1000000ULL*100/sampleDepth)/60),1ULL);
									if(i == 0)
									{	// Start with first channel
										if(progress<=DownloadState::DOWNLOAD_STARTED)
										{	// Restart all channels
											for(size_t j = 0 ; j < channelCount ; j++)
											{
												state->SetChannelDownloadState(j,DownloadState::DOWNLOAD_WAITING);
											}
										}
										if(progress<DownloadState::DOWNLOAD_FINISHED)
										{
											progress+=downloadIncrement;
										}
									}
									else if(state->GetChannelDownloadState(i-1)>=DOWNLOAD_FINISHED)
									{
										if(progress<DownloadState::DOWNLOAD_FINISHED)
										{
											progress+=downloadIncrement;
										}
									}
									if(i == (channelCount-1) && progress>=DownloadState::DOWNLOAD_FINISHED)
									{	// Start over
										for(size_t j = 0 ; j < channelCount ; j++)
										{
											state->SetChannelDownloadState(j,DownloadState::DOWNLOAD_WAITING);
										}
									}
									else
									{
										if(progress>DownloadState::DOWNLOAD_FINISHED)
											progress = DownloadState::DOWNLOAD_FINISHED;
										state->SetChannelDownloadState(i,progress);
									}
								}
								else if(progress != DownloadState::DOWNLOAD_NONE)
								{	// Set download state to non
									state->SetChannelDownloadState(i,DownloadState::DOWNLOAD_NONE);
								}
							}
							renderDownloadProgress(progress);
						}
					}
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
						// Channel/stram properties
						if (renderScopeProps && scopechan)
						{
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
