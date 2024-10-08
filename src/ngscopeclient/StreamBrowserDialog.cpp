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
	: Dialog("Stream Browser", "Stream Browser", ImVec2(550, 400))
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
	auto renderDownloadProgress = [this, &badgeXMin, &badgeXCur](std::shared_ptr<Instrument> inst, InstrumentChannel *chan)
	{
		static const char* const download[] = {"DOWNLOADING", "DOWNLOAD" ,"DL","D", NULL};

		/* prefer language "PENDING" to "WAITING": "PENDING" implies
		 * that we are going to do it when we get through a list of
		 * other things, "WAITING" could mean that the channel is
		 * waiting for something else (trigger?)
		 */
		static const char* const pend[]     = {"PENDING"    , "PEND"     ,"PE","P", NULL};

		/* prefer language "COMPLETE" to "READY": "READY" implies
		 * that the channel might be ready to capture or something,
		 * but "COMPLETE" at least is not to be confused with that. 
		 * ("DOWNLOADED" is more specific but is easy to confuse
		 * with "DOWNLOADING".  If you can come up with a better
		 * mid-length abbreviation for "COMPLETE" than "DL OK" /
		 * "OK", give it a go, I guess.)
		 */
		static const char* const ready[]    = {"COMPLETE"   , "DL OK"    ,"OK","C", NULL};
		static const char* const* labels;

		ImVec4 color;
		bool hasProgress = false;
		double elapsed = GetTime() - chan->GetDownloadStartTime();

		// determine what label we should apply, and while we are at
		// it, determine if this channel appears to be slow enough
		// to need a progress bar

/// @brief hysteresis threshold for a channel finishing a download faster than this to be declared fast
#define CHANNEL_DOWNLOAD_THRESHOLD_FAST_SECONDS ((double)0.2)

/// @brief hysteresis threshold for a channel finishing a still being in progress for longer than this to be declared slow
#define CHANNEL_DOWNLOAD_THRESHOLD_SLOW_SECONDS ((double)0.4)

		switch(chan->GetDownloadState())
		{
			case InstrumentChannel::DownloadState::DOWNLOAD_NONE:
			case InstrumentChannel::DownloadState::DOWNLOAD_UNKNOWN:
				/* There is nothing to say about this --
				 * either there is nothing pending at all on
				 * the system, or this scope doesn't know
				 * how to report it, and in either case, we
				 * don't need to render a badge about it.
				 */
				return;
			case InstrumentChannel::DownloadState::DOWNLOAD_WAITING:
				labels = pend;
				if (elapsed > CHANNEL_DOWNLOAD_THRESHOLD_SLOW_SECONDS)
					m_channelDownloadIsSlow[{inst, chan}] = true;
				hasProgress = m_channelDownloadIsSlow[{inst, chan}];
				color.x = 0.8 ; color.y=0.3 ; color.z=0.3; color.w=1.0;
				break;
			case InstrumentChannel::DownloadState::DOWNLOAD_IN_PROGRESS:
				labels = download;
				if (elapsed > CHANNEL_DOWNLOAD_THRESHOLD_SLOW_SECONDS)
					m_channelDownloadIsSlow[{inst, chan}] = true;
				hasProgress = m_channelDownloadIsSlow[{inst, chan}];
				color.x = 0.7 ; color.y=0.7 ; color.z=0.3; color.w=1.0;
				break;
			case InstrumentChannel::DownloadState::DOWNLOAD_FINISHED:
				labels = ready;
				if (elapsed < CHANNEL_DOWNLOAD_THRESHOLD_FAST_SECONDS)
					m_channelDownloadIsSlow[{inst, chan}] = false;
				color.x = 0.3 ; color.y=0.8 ; color.z=0.3; color.w=1.0;
				break;
			default:
				return;
		}

/// @brief Width used to display progress bars (e.g. download progress bar)
#define PROGRESS_BAR_WIDTH	80

		// try first adding a bar, and if there isn't enough room
		// for a bar, skip it and try just putting a label
		for (int withoutBar = 0; withoutBar < 2; withoutBar++)
		{
			if (withoutBar)
				hasProgress = false;

			for (int i = 0; labels[i]; i++)
			{
				const char *label = labels[i];

				float xsz = ImGui::CalcTextSize(label).x + (hasProgress ? (ImGui::GetStyle().ItemSpacing.x + PROGRESS_BAR_WIDTH) : 0) + ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetStyle().ItemSpacing.x;
				if ((badgeXCur - xsz) < badgeXMin)
					continue;

				// ok, we have enough space -- commit to it!
				badgeXCur -= xsz - ImGui::GetStyle().ItemSpacing.x;
				ImGui::SameLine(badgeXCur);
				ImGui::PushStyleColor(ImGuiCol_Button, color);
				ImGui::SmallButton(label);
				ImGui::PopStyleColor();
				if(hasProgress)
				{
					ImGui::SameLine();
					ImGui::ProgressBar(chan->GetDownloadProgress(), ImVec2(PROGRESS_BAR_WIDTH, ImGui::GetFontSize()));
				}

				return;
			}
		}

		// well, shoot -- I guess there wasn't enough room to do *anything* useful!
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
				auto scopechan = dynamic_cast<OscilloscopeChannel *>(chan);
				bool renderScopeProps = false;
				bool isDigital = false;
				if (scopechan) 
				{
					renderScopeProps = scopechan->IsEnabled();
					isDigital = scopechan->GetType(0) == Stream::STREAM_TYPE_DIGITAL;
				}

				bool hasChildren = !singleStream || renderScopeProps;

				if (chan->m_displaycolor != "") {
					ImGui::PushStyleColor(ImGuiCol_Text, ColorFromString(chan->m_displaycolor));
				}
				bool open = ImGui::TreeNodeEx(chan->GetDisplayName().c_str(), isDigital ? 0 : ImGuiTreeNodeFlags_DefaultOpen | (!hasChildren ? ImGuiTreeNodeFlags_Leaf : 0));
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
				if (scopechan && !scopechan->IsEnabled())
				{
					renderBadge(ImVec4(0.4, 0.4, 0.4, 1.0) /* XXX: pull color from prefs */, "disabled", "disa", NULL);
				} else {
					renderDownloadProgress(inst, chan);
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
				
							if(isDigital)
							{
								auto threshold_txt = Unit(Unit::UNIT_VOLTS).PrettyPrint(scope->GetDigitalThreshold(i));
					
								bool clicked = false;
								bool hovered = false;
								renderInfoLink("Threshold", threshold_txt.c_str(), clicked, hovered);
								if (clicked) {
									/* XXX: refactor to be more like FilterGraphEditor::HandleNodeProperties? */
									m_parent->ShowChannelProperties(scopechan);
								}
								if (hovered) {
									m_parent->AddStatusHelp("mouse_lmb", "Open channel properties");
								}
							}
							else
							{
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
