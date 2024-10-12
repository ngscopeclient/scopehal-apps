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
	auto renderDownloadProgress = [this, &badgeXMin, &badgeXCur](std::shared_ptr<Instrument> inst, InstrumentChannel *chan, bool isLast)
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
		static const char* const ready[]  	= {"COMPLETE"   , "DL OK"    ,"OK","C", NULL};

		/* Let's use active for fast download channels to display when data is available
		 */
		static const char* const active[]   = {"ACTIVE"      , "ACTV"    ,"ACT","A", NULL};

		static const char* const* labels;

		ImVec4 color;
		bool shouldRender = true;
		bool hasProgress = false;
		double elapsed = GetTime() - chan->GetDownloadStartTime();
		auto& prefs = m_session.GetPreferences();


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
				if (isLast && (elapsed < CHANNEL_DOWNLOAD_THRESHOLD_FAST_SECONDS))
					m_instrumentDownloadIsSlow[inst] = false;
				/* FALLTHRU */
			case InstrumentChannel::DownloadState::DOWNLOAD_UNKNOWN:
				/* There is nothing to say about this --
				 * either there is nothing pending at all on
				 * the system, or this scope doesn't know
				 * how to report it, and in either case, we
				 * don't need to render a badge about it.
				 */
				shouldRender = false;
				break;
			case InstrumentChannel::DownloadState::DOWNLOAD_WAITING:
				labels = pend;
				if (elapsed > CHANNEL_DOWNLOAD_THRESHOLD_SLOW_SECONDS)
					m_instrumentDownloadIsSlow[inst] = true;
				hasProgress = m_instrumentDownloadIsSlow[inst];
				color = ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.download_wait_badge_color"));
				break;
			case InstrumentChannel::DownloadState::DOWNLOAD_IN_PROGRESS:
				labels = download;
				if (elapsed > CHANNEL_DOWNLOAD_THRESHOLD_SLOW_SECONDS)
					m_instrumentDownloadIsSlow[inst] = true;
				hasProgress = m_instrumentDownloadIsSlow[inst];
				color = ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.download_progress_badge_color"));
				break;
			case InstrumentChannel::DownloadState::DOWNLOAD_FINISHED:
				labels = ready;
				if (isLast && (elapsed < CHANNEL_DOWNLOAD_THRESHOLD_FAST_SECONDS))
					m_instrumentDownloadIsSlow[inst] = false;
				color = ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.download_finished_badge_color"));
				break;
			default:
				shouldRender = false;
				break;
		}

		// For fast channels, show a constant green badge when a
		// download has started "recently" -- even if we're not
		// downloading at this moment.  This could be slightly
		// misleading (i.e., after a channel goes into STOP mode, we
		// will remain ACTIVE for up to THRESHOLD_SLOW time) but the
		// period of time for which it is misleading is short!
		if(!m_instrumentDownloadIsSlow[inst] && elapsed < CHANNEL_DOWNLOAD_THRESHOLD_SLOW_SECONDS)
		{
			labels = active;
			color = ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.download_active_badge_color"));
			shouldRender = true;
			hasProgress = false;
		}

		if (!shouldRender)
			return;

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

	auto renderPsuRows = [this](bool isVoltage, bool cc, PowerSupplyChannel* chan,const char *setValue, const char *measuredValue, bool &clicked, bool &hovered)
	{	
		auto& prefs = m_session.GetPreferences();
		// Row 1
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text(isVoltage ? "Voltage:" : "Current:");
		ImGui::TableSetColumnIndex(1);
		StreamDescriptor sv(chan, isVoltage ? 1 : 3);
		ImGui::PushID(isVoltage ? "sV" :  "sC");
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.psu_set_badge_color")));
		ImGui::Selectable("- Set");
		ImGui::PopStyleColor();
		if(ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload("Scalar", &sv, sizeof(sv));
			ImGui::TextUnformatted((isVoltage ? "Voltage set value" : "Current set value")); // TODO WTF !
			ImGui::EndDragDropSource();
		}
		else
			DoItemHelp();
		ImGui::PopID();
		ImGui::TableSetColumnIndex(2);
		clicked |= ImGui::TextLink(setValue);
		hovered |= ImGui::IsItemHovered();
		// Row 2
		ImGui::TableNextRow();
		if((isVoltage && !cc) || (!isVoltage && cc))
		{
			ImGui::TableSetColumnIndex(0);
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(prefs.GetColor(isVoltage ? "Appearance.Stream Browser.psu_cv_badge_color" : "Appearance.Stream Browser.psu_cc_badge_color")));
			ImGui::SmallButton(isVoltage ? "CV" : "CC");
			ImGui::PopStyleColor();
		}
		ImGui::TableSetColumnIndex(1);
		StreamDescriptor mv(chan, isVoltage ? 0 : 2);
		ImGui::PushID(isVoltage ? "mV" :  "mC");
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.psu_meas_badge_color")));
		ImGui::Selectable("- Meas.");
		ImGui::PopStyleColor();
		if(ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload("Scalar", &mv, sizeof(mv));
			ImGui::TextUnformatted((isVoltage ? "Voltage measured value" : "Current measured value"));
			ImGui::EndDragDropSource();
		}
		else
			DoItemHelp();
		ImGui::PopID();
		ImGui::TableSetColumnIndex(2);
		clicked |= ImGui::TextLink(measuredValue);
		hovered |= ImGui::IsItemHovered();
	};


	// Get preferences for colors
	auto& prefs = m_session.GetPreferences();
	//Add all instruments
	auto insts = m_session.GetInstruments();
	for(auto inst : insts)
	{
		bool instIsOpen = ImGui::TreeNodeEx(inst->m_nickname.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
		startBadgeLine();

		auto state = m_session.GetInstrumentConnectionState(inst);

		size_t channelCount = inst->GetChannelCount();

		// Render ornaments for this scope: offline, trigger status, ...
		auto scope = std::dynamic_pointer_cast<Oscilloscope>(inst);
		if (scope) 
		{
			if (scope->IsOffline())
				renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.instrument_offline_badge_color")), "OFFLINE", "OFFL", NULL);
			else
			{
				Oscilloscope::TriggerMode mode = state ? state->m_lastTriggerState : Oscilloscope::TRIGGER_MODE_STOP;
				switch (mode) {
				case Oscilloscope::TRIGGER_MODE_RUN:
					/* prefer language "ARMED" to "RUN":
					 * "RUN" could mean either "waiting
					 * for trigger" or "currently
					 * capturing samples post-trigger",
					 * "ARMED" is unambiguous */
					renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.trigger_armed_badge_color")), "ARMED", "A", NULL);
					break;
				case Oscilloscope::TRIGGER_MODE_STOP:
					renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.trigger_stopped_badge_color")), "STOPPED", "STOP", "S", NULL);
					break;
				case Oscilloscope::TRIGGER_MODE_TRIGGERED:
					renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.trigger_triggered_badge_color")), "TRIGGERED", "TRIG'D", "T'D", "T", NULL);
					break;
				case Oscilloscope::TRIGGER_MODE_WAIT:
					/* prefer language "BUSY" to "WAIT":
					 * "WAIT" could mean "waiting for
					 * trigger", "BUSY" means "I am
					 * doing something internally and am
					 * not ready for some reason" */
					renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.trigger_busy_badge_color")), "BUSY", "B", NULL);
					break;
				case Oscilloscope::TRIGGER_MODE_AUTO:
					renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.trigger_auto_badge_color")), "AUTO", "A", NULL);
					break;
				default:
					break;
				}
			}
		}

		// Render ornaments for this PSU: on/off status, ...
		auto psu = std::dynamic_pointer_cast<SCPIPowerSupply>(inst);
		if (psu) 
		{
			bool allOn = false;
			bool someOn = false;
			if(psu->SupportsMasterOutputSwitching())
			{
				allOn = psu->GetMasterPowerEnable();
			}
			else
			{
				allOn = true;
				for(size_t i = 0 ; i < channelCount ; i++)
				{
					if(psu->GetPowerChannelActive(i))
					{
						someOn = true;
					}
					else
					{
						allOn = false;
					}
				}
			}
			if(allOn || someOn)
			{
				renderBadge(allOn ? ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.instrument_on_badge_color")) : ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.instrument_partial_badge_color")), "ON", "I", NULL);
			}
			else
			{
				renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.instrument_off_badge_color")), "OFF", "O", NULL);
			}
		}

		if(instIsOpen)
		{
			size_t lastEnabledChannelIndex = 0;
			if (scope)
			{
				ImGui::BeginChild("sample_params", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border);

				auto srate_txt = Unit(Unit::UNIT_SAMPLERATE).PrettyPrint(scope->GetSampleRate());
				auto sdepth_txt = Unit(Unit::UNIT_SAMPLEDEPTH).PrettyPrint(scope->GetSampleDepth());

				bool clicked = false;
				bool hovered = false;
				renderInfoLink("Sample rate", srate_txt.c_str(), clicked, hovered);
				renderInfoLink("Sample depth", sdepth_txt.c_str(), clicked, hovered);
				if (clicked)
					m_parent->ShowTimebaseProperties();
				if (hovered)
					m_parent->AddStatusHelp("mouse_lmb", "Open timebase properties");
				for(size_t i = 0; i<channelCount; i++)
				{
					if(scope->IsChannelEnabled(i))
					{
						lastEnabledChannelIndex = i;
					}
				}

				ImGui::EndChild();
			}

			for(size_t i=0; i<channelCount; i++)
			{
				auto chan = inst->GetChannel(i);

				bool singleStream = chan->GetStreamCount() == 1;
				auto scopechan = dynamic_cast<OscilloscopeChannel *>(chan);
				auto psuchan = dynamic_cast<PowerSupplyChannel *>(chan);
				bool renderScopeProps = false;
				bool isDigital = false;
				if (scopechan)
				{
					renderScopeProps = scopechan->IsEnabled();
					isDigital = scopechan->GetType(0) == Stream::STREAM_TYPE_DIGITAL;
				}

				bool hasChildren = !singleStream || renderScopeProps;

				if (chan->m_displaycolor != "")
					ImGui::PushStyleColor(ImGuiCol_Text, ColorFromString(chan->m_displaycolor));
				bool open = ImGui::TreeNodeEx(chan->GetDisplayName().c_str(), isDigital ? 0 : ImGuiTreeNodeFlags_DefaultOpen | (!hasChildren ? ImGuiTreeNodeFlags_Leaf : 0));
				if (chan->m_displaycolor != "")
					ImGui::PopStyleColor();

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
				if (scopechan)
				{
					if(!scopechan->IsEnabled())
					{
						renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.instrument_disabled_badge_color")), "DISABLED", "DISA","--", NULL);
					}
					else
					{
						renderDownloadProgress(inst, chan, (i == lastEnabledChannelIndex));
					}
				}
				else if(psu)
				{
					if(psu->GetPowerChannelActive(i))
					{
						renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.instrument_on_badge_color")), "ON", "I", NULL);
					}
					else
					{
						renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.instrument_off_badge_color")), "OFF", "O", NULL);
					}
				}

				if(open)
				{
					ImGui::PushID(inst.get());
					if(psu)
					{	// For PSU we will have a special handling for the 4 streams associated to a PSU channel
						ImGui::BeginChild("psu_params", ImVec2(0, 0),
							ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border);
						auto svoltage_txt = Unit(Unit::UNIT_VOLTS).PrettyPrint(psuchan->GetVoltageSetPoint ());
						auto mvoltage_txt = Unit(Unit::UNIT_VOLTS).PrettyPrint(psuchan->GetVoltageMeasured());
						auto scurrent_txt = Unit(Unit::UNIT_AMPS).PrettyPrint(psuchan->GetCurrentSetPoint ());
						auto mcurrent_txt = Unit(Unit::UNIT_AMPS).PrettyPrint(psuchan->GetCurrentMeasured ());

						bool cc = false;
						auto psuState = m_session.GetPSUState(psu);
						if(psuState)
							cc = psuState->m_channelConstantCurrent[i].load();

						bool clicked = false;
						bool hovered = false;

						if (ImGui::BeginTable("table1", 3))
						{
							// Voltage
							renderPsuRows(true,cc,psuchan,svoltage_txt.c_str(),mvoltage_txt.c_str(),clicked,hovered);
							// Current
							renderPsuRows(false,cc,psuchan,scurrent_txt.c_str(),mcurrent_txt.c_str(),clicked,hovered);
							// End table
							ImGui::EndTable();
							if (clicked)
							{
								m_parent->ShowInstrumentProperties(psu);
							}
							if (hovered)
								m_parent->AddStatusHelp("mouse_lmb", "Open channel properties");
						}						
						ImGui::EndChild();
					}
					else
					{
						for(size_t j=0; j<chan->GetStreamCount(); j++)
						{
							ImGui::PushID(j);

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
							{	// Scope channel
								ImGui::BeginChild("scope_params", ImVec2(0, 0),
									ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border);

								if(isDigital)
								{
									auto threshold_txt = Unit(Unit::UNIT_VOLTS).PrettyPrint(scope->GetDigitalThreshold(i));

									bool clicked = false;
									bool hovered = false;
									renderInfoLink("Threshold", threshold_txt.c_str(), clicked, hovered);
									if (clicked)
									{
										/* XXX: refactor to be more like FilterGraphEditor::HandleNodeProperties? */
										m_parent->ShowChannelProperties(scopechan);
									}
									if (hovered)
										m_parent->AddStatusHelp("mouse_lmb", "Open channel properties");
								}
								else
								{
									auto offset_txt = Unit(Unit::UNIT_VOLTS).PrettyPrint(scopechan->GetOffset(j));
									auto range_txt = Unit(Unit::UNIT_VOLTS).PrettyPrint(scopechan->GetVoltageRange(j));

									bool clicked = false;
									bool hovered = false;
									renderInfoLink("Offset", offset_txt.c_str(), clicked, hovered);
									renderInfoLink("Voltage range", range_txt.c_str(), clicked, hovered);
									if (clicked)
									{
										/* XXX: refactor to be more like FilterGraphEditor::HandleNodeProperties? */
										m_parent->ShowChannelProperties(scopechan);
									}
									if (hovered)
										m_parent->AddStatusHelp("mouse_lmb", "Open channel properties");
								}

								ImGui::EndChild();
							}
							ImGui::PopID();
						}
					}
					ImGui::PopID();
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
