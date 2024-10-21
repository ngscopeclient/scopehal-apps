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

// @brief Helper methods for rendering widgets that appear in the StreamBrowserDialog.

/*
	@brief Render a link of the "Sample rate: 4 GSa/s" type that shows up in the
	scope properties box.
*/
void StreamBrowserDialog::renderInfoLink(const char *label, const char *linktext, bool &clicked, bool &hovered)
{
	ImGui::Text("%s: ", label);
	ImGui::SameLine(0, 0);
	clicked |= ImGui::TextLink(linktext);
	hovered |= ImGui::IsItemHovered();
}

void StreamBrowserDialog::startBadgeLine()
{
	ImGuiWindow *window = ImGui::GetCurrentWindowRead();
	// roughly, what ImGui::GetCursorPosPrevLineX would be, if it existed; convert from absolute-space to window-space
	m_badgeXMin = (window->DC.CursorPosPrevLine - window->Pos + window->Scroll).x;
	m_badgeXCur = ImGui::GetWindowContentRegionMax().x;
}

bool StreamBrowserDialog::renderBadge(ImVec4 color, ... /* labels, ending in NULL */)
{
	va_list ap;
	va_start(ap, color);
	bool result = false;

	while (const char *label = va_arg(ap, const char *)) {
		float xsz = ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().FramePadding.x * 2;
		if ((m_badgeXCur - xsz) < m_badgeXMin) {
			continue;
		}

		// ok, we have enough space -- commit to it!
		m_badgeXCur -= xsz - ImGui::GetStyle().ItemSpacing.x;
		ImGui::SameLine(m_badgeXCur);
		ImGui::PushStyleColor(ImGuiCol_Button, color);
		result = ImGui::SmallButton(label);
		ImGui::PopStyleColor();
		break;
	}
	return result;
}

int  StreamBrowserDialog::renderCombo(ImVec4 color,int selected, ... /* values, ending in NULL */)
{
	va_list ap;
	va_start(ap, selected);
	int result = selected;

	const char* selectedLabel = NULL;
	int itemIndex = 0;
	while (const char *label = va_arg(ap, const char *)) 
	{
		if(itemIndex == selected)
		{
			selectedLabel = label;
			break;
		}
		itemIndex++;
	}
	if(selectedLabel == NULL)
	{
		va_start(ap, selected);
		selectedLabel = va_arg(ap, const char *);
		selected = 0;
	}
	float xsz = ImGui::CalcTextSize(selectedLabel).x + ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().FramePadding.x * 2;
	if ((m_badgeXCur - xsz) < m_badgeXMin)
		return result; // No room to display the combo
	m_badgeXCur -= xsz - ImGui::GetStyle().ItemSpacing.x;
	ImGui::SameLine(m_badgeXCur);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, color);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 0));
	if(ImGui::BeginCombo(" ", selectedLabel, ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_WidthFitPreview)) // Label cannot be emtpy for the combo to work
	{
		va_start(ap, selected);
		itemIndex = 0;
		while (const char *label = va_arg(ap, const char *)) 
		{
			const bool is_selected = (itemIndex == selected);
			if(ImGui::Selectable(label, is_selected))
				result = itemIndex;
			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (is_selected)
				ImGui::SetItemDefaultFocus();
			itemIndex++;
		}
		ImGui::EndCombo();
	}
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
	return result;
}

bool StreamBrowserDialog::renderToggle(ImVec4 color, bool curValue)
{
	return (renderCombo(color, (int)curValue, "OFF", "ON", NULL) == 1);
}
bool StreamBrowserDialog::renderOnOffToggle(bool curValue)
{
	auto& prefs = m_session.GetPreferences();
	ImVec4 color = ImGui::ColorConvertU32ToFloat4((curValue ? prefs.GetColor("Appearance.Stream Browser.instrument_on_badge_color") : prefs.GetColor("Appearance.Stream Browser.instrument_off_badge_color")));
	return renderToggle(color, curValue);
}

void StreamBrowserDialog::renderDownloadProgress(std::shared_ptr<Instrument> inst, InstrumentChannel *chan, bool isLast)
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
			if ((m_badgeXCur - xsz) < m_badgeXMin)
				continue;

			// ok, we have enough space -- commit to it!
			m_badgeXCur -= xsz - ImGui::GetStyle().ItemSpacing.x;
			ImGui::SameLine(m_badgeXCur);
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
}

void StreamBrowserDialog::renderPsuRows(bool isVoltage, bool cc, PowerSupplyChannel* chan,const char *setValue, const char *measuredValue, bool &clicked, bool &hovered)
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
		string dragText = chan->GetDisplayName() + (isVoltage ? " voltage" : " current") + " set value";
		ImGui::TextUnformatted(dragText.c_str());
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
		string dragText = chan->GetDisplayName() + (isVoltage ? " voltage" : " current") + " measured value";
		ImGui::TextUnformatted(dragText.c_str());
		ImGui::EndDragDropSource();
	}
	else
		DoItemHelp();
	ImGui::PopID();
	ImGui::TableSetColumnIndex(2);
	clicked |= ImGui::TextLink(measuredValue);
	hovered |= ImGui::IsItemHovered();
}

/* 
	@brief Rendering of an instrument node
*/
void StreamBrowserDialog::renderInstrumentNode(shared_ptr<Instrument> instrument)
{
	// Get preferences for colors
	auto& prefs = m_session.GetPreferences();

	ImGui::PushID(instrument.get());
	bool instIsOpen = ImGui::TreeNodeEx(instrument->m_nickname.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
	startBadgeLine();

	auto state = m_session.GetInstrumentConnectionState(instrument);

	size_t channelCount = instrument->GetChannelCount();

	// Render ornaments for this scope: offline, trigger status, ...
	auto scope = std::dynamic_pointer_cast<Oscilloscope>(instrument);
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
	auto psu = std::dynamic_pointer_cast<SCPIPowerSupply>(instrument);
	if (psu)
	{
		//Get the state
		auto psustate = m_session.GetPSUState(psu);

		bool allOn = false;
		bool someOn = false;
		if(psu->SupportsMasterOutputSwitching())
			allOn = psustate->m_masterEnable;
		else
		{
			allOn = true;
			for(size_t i = 0 ; i < channelCount ; i++)
			{
				if(psustate->m_channelOn[i])
					someOn = true;
				else
					allOn = false;
			}
		}
		bool result;
		if(allOn || someOn)
		{
			result = renderToggle(allOn ?
				ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.instrument_on_badge_color")) :
				ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.instrument_partial_badge_color")), true);
		}
		else
		{
			result = renderOnOffToggle(false);
		}
		if(result != allOn)
			psu->SetMasterPowerEnable(result);
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
					lastEnabledChannelIndex = i;
			}

			ImGui::EndChild();
		}

		for(size_t i=0; i<channelCount; i++)
		{
			renderChannelNode(instrument,i,(i == lastEnabledChannelIndex));
		}

		ImGui::TreePop();
	}
	ImGui::PopID();

}

/* 
	@brief Rendering of a channel node
*/
void StreamBrowserDialog::renderChannelNode(shared_ptr<Instrument> instrument, size_t channelIndex, bool isLast)
{
	// Get preferences for colors
	auto& prefs = m_session.GetPreferences();

	InstrumentChannel* channel = instrument->GetChannel(channelIndex);

	ImGui::PushID(channelIndex);

	auto psu = std::dynamic_pointer_cast<SCPIPowerSupply>(instrument);
	auto scope = std::dynamic_pointer_cast<Oscilloscope>(instrument);

	bool singleStream = channel->GetStreamCount() == 1;
	auto scopechan = dynamic_cast<OscilloscopeChannel *>(channel);
	auto psuchan = dynamic_cast<PowerSupplyChannel *>(channel);
	bool renderScopeProps = false;
	bool isDigital = false;
	if (scopechan)
	{
		renderScopeProps = scopechan->IsEnabled();
		isDigital = scopechan->GetType(0) == Stream::STREAM_TYPE_DIGITAL;
	}

	bool hasChildren = !singleStream || renderScopeProps;

	if (channel->m_displaycolor != "")
		ImGui::PushStyleColor(ImGuiCol_Text, ColorFromString(channel->m_displaycolor));
	bool open = ImGui::TreeNodeEx(channel->GetDisplayName().c_str(), isDigital ? 0 : ImGuiTreeNodeFlags_DefaultOpen | (!hasChildren ? ImGuiTreeNodeFlags_Leaf : 0));
	if (channel->m_displaycolor != "")
		ImGui::PopStyleColor();

	//Single stream: drag the stream not the channel
	if(singleStream)
	{
		StreamDescriptor s(channel, 0);
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
		ImGui::SetDragDropPayload("Channel", &channel, sizeof(channel));

		ImGui::TextUnformatted(channel->GetDisplayName().c_str());
		ImGui::EndDragDropSource();
	}

	// Channel decoration
	startBadgeLine();
	if (scopechan)
	{
		if(!scopechan->IsEnabled())
			renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.instrument_disabled_badge_color")), "DISABLED", "DISA","--", NULL);
		else
			renderDownloadProgress(instrument, channel, isLast);
	}
	else if(psu)
	{
		//Get the state
		auto psustate = m_session.GetPSUState(psu);

		bool active = psustate->m_channelOn[channelIndex];
		bool result = renderOnOffToggle(active);
		if(result != active)
			psu->SetPowerChannelActive(channelIndex,result);
	}

	if(open)
	{
		ImGui::PushID(instrument.get());
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
				cc = psuState->m_channelConstantCurrent[channelIndex].load();

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
			for(size_t j=0; j<channel->GetStreamCount(); j++)
			{
				ImGui::PushID(j);

				if (!singleStream)
				{
					ImGui::Selectable(channel->GetStreamName(j).c_str());

					StreamDescriptor s(channel, j);
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
						auto threshold_txt = Unit(Unit::UNIT_VOLTS).PrettyPrint(scope->GetDigitalThreshold(channelIndex));

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

	ImGui::PopID();
}



/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool StreamBrowserDialog::DoRender()
{
	//Add all instruments
	auto insts = m_session.GetInstruments();
	for(auto inst : insts)
	{
		renderInstrumentNode(inst);
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
