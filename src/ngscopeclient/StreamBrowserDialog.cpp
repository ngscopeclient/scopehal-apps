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

//Helper methods for rendering widgets that appear in the StreamBrowserDialog.

/**
	@brief Render a link of the "Sample rate: 4 GSa/s" type that shows up in the
	scope properties box.
*/
void StreamBrowserDialog::renderInfoLink(const char *label, const char *linktext, bool &clicked, bool &hovered)
{
	ImGui::PushID(label);	// Prevent collision if several sibling links have the same linktext
	ImGui::Text("%s: ", label);
	ImGui::SameLine(0, 0);
	clicked |= ImGui::TextLink(linktext);
	hovered |= ImGui::IsItemHovered();
	ImGui::PopID();
}

/**
	@brief prepare rendering context to display a badge at the end of current line
 */
void StreamBrowserDialog::startBadgeLine()
{
	ImGuiWindow *window = ImGui::GetCurrentWindowRead();
	// roughly, what ImGui::GetCursorPosPrevLineX would be, if it existed; convert from absolute-space to window-space
	m_badgeXMin = (window->DC.CursorPosPrevLine - window->Pos + window->Scroll).x;
	m_badgeXCur = ImGui::GetWindowContentRegionMax().x;
}

/**
	@brief render a badge for an instrument node

	@param inst the instrument to render the badge for
	@param latched true if the redering of this batch should be latched (i.e. only renderd it previous badge has been here for more than a given time)
	@param badge the badge type
*/
bool StreamBrowserDialog::renderInstrumentBadge(std::shared_ptr<Instrument> inst, bool latched, InstrumentBadge badge)
{
	auto& prefs = m_session.GetPreferences();
	double now = GetTime();
	bool result = false;
	if(latched)
	{
		std::pair<double,InstrumentBadge> old = m_instrumentLastBadge[inst];
		double elapsed = now - old.first;
		if(elapsed < prefs.GetReal("Appearance.Stream Browser.instrument_badge_latch_duration"))
		{	// Keep previous badge
			badge = old.second;
		}
	}
	else
		m_instrumentLastBadge[inst] = std::pair<double,InstrumentBadge>(now,badge);

	switch (badge)
	{
		case StreamBrowserDialog::BADGE_ARMED:
			/* prefer language "ARMED" to "RUN":
				* "RUN" could mean either "waiting
				* for trigger" or "currently
				* capturing samples post-trigger",
				* "ARMED" is unambiguous */
			result = renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.trigger_armed_badge_color")), "ARMED", "A", NULL);
			break;
		case StreamBrowserDialog::BADGE_STOPPED:
			result = renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.trigger_stopped_badge_color")), "STOPPED", "STOP", "S", NULL);
			break;
		case StreamBrowserDialog::BADGE_TRIGGERED:
			result = renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.trigger_triggered_badge_color")), "TRIGGERED", "TRIG'D", "T'D", "T", NULL);
			break;
		case StreamBrowserDialog::BADGE_BUSY:
			/* prefer language "BUSY" to "WAIT":
				* "WAIT" could mean "waiting for
				* trigger", "BUSY" means "I am
				* doing something internally and am
				* not ready for some reason" */
			result = renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.trigger_busy_badge_color")), "BUSY", "B", NULL);
			break;
		case StreamBrowserDialog::BADGE_AUTO:
			result = renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.trigger_auto_badge_color")), "AUTO", "A", NULL);
			break;
		default:
			break;
	}
	return result;
}

/**
	@brief render a badge at the end of current line with provided color and text

	@param color the color of the badge
	@param ... a null terminated list of labels form the largest to the smallest to use as a badge label according to the available space
	@return true if the badge has been clicked
*/
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
	va_end(ap);
	return result;
}

/**
   @brief Render a combo box with provded color and values

   @param color the color of the combo box
   @param selected the selected value index (in/out)
   @param values the combo box values
   @param useColorForText if true, use the provided color for text (and a darker version of it for background color)
   @param cropTextTo if >0 crop the combo text up to this number of characters to have it fit the available space
   @return true true if the selected value of the combo has been changed
 */
bool StreamBrowserDialog::renderCombo(ImVec4 color, int &selected, const std::vector<string> &values, bool useColorForText, uint8_t cropTextTo)
{
	if(selected >= (int)values.size() || selected < 0)
	{
		selected = 0;
	}
	bool changed = false;
	const char* selectedLabel = values[selected].c_str();
	int padding = ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().FramePadding.x * 2;
	float xsz = ImGui::CalcTextSize(selectedLabel).x + padding;
	string resizedLabel;
	if ((m_badgeXCur - xsz) < m_badgeXMin)
	{
		if(cropTextTo == 0)
			return changed; // No room and we don't want to crop text
		resizedLabel = selectedLabel;
		while((m_badgeXCur - xsz) < m_badgeXMin)
		{
			// Try and crop text
			resizedLabel = resizedLabel.substr(0,resizedLabel.size()-1);
			if(resizedLabel.size() < cropTextTo)
				break; // We don't want to make the text that short
			xsz = ImGui::CalcTextSize((resizedLabel + "...").c_str()).x + padding;
		}
		if((m_badgeXCur - xsz) < m_badgeXMin)
			return changed; // Still no room
		// We found an acceptable size
		resizedLabel = resizedLabel + "...";
		selectedLabel = resizedLabel.c_str();
	}
	m_badgeXCur -= xsz - ImGui::GetStyle().ItemSpacing.x;
	ImGui::SameLine(m_badgeXCur);
	if(useColorForText)
	{
		// Use channel color for shape combo, but darken it to make text readable
		float bgmul = 0.4;
		auto bcolor = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x*bgmul, color.y*bgmul, color.z*bgmul, color.w) );
		ImGui::PushStyleColor(ImGuiCol_FrameBg, bcolor);
		ImGui::PushStyleColor(ImGuiCol_Text, color);
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_FrameBg, color);
	}
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 0));
	if(ImGui::BeginCombo(" ", selectedLabel, ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_WidthFitPreview)) // Label cannot be emtpy for the combo to work
	{
		for(int i = 0 ; i < (int)values.size() ; i++)
		{
			const bool is_selected = (i == selected);
			if(ImGui::Selectable(values[i].c_str(), is_selected))
			{
				selected = i;
				changed = true;
			}
			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
	if(useColorForText)
		ImGui::PopStyleColor();
	return changed;
}

/**
   @brief Render a combo box with provded color and values

   @param color the color of the combo box
   @param selected the selected value index (in/out)
   @param ... the combo box values
   @return true true if the selected value of the combo has been changed
 */
bool StreamBrowserDialog::renderCombo(ImVec4 color,int *selected, ... /* values, ending in NULL */)
{
	if(!selected)
	{
		LogError("Invalid call to renderCombo() method, 'selected' parameter must not be null.");
		return false;
	}
	va_list ap;
	va_start(ap, selected);
	std::vector<string> values;
	while (const char *label = va_arg(ap, const char *))
	{
		values.push_back(string(label));
	}
	va_end(ap);
	return renderCombo(color,(*selected),values);
}

/**
   @brief Render a toggle button combo

   @param color the color of the toggle button
   @param curValue the value of the toggle button
   @param valueOff label for value off (optionnal, defaults to "OFF")
   @param valueOn label for value on (optionnal, defaults to "ON")
   @param cropTextTo if >0 crop the combo text up to this number of characters to have it fit the available space (optionnal, defaults to 0)
   @return the selected value for the toggle button
 */
bool StreamBrowserDialog::renderToggle(ImVec4 color, bool curValue, const char* valueOff, const char* valueOn, uint8_t cropTextTo)
{
	int selection = (int)curValue;
	std::vector<string> values;
	values.push_back(string(valueOff));
	values.push_back(string(valueOn));
	renderCombo(color, selection, values, false, cropTextTo);
	return (selection == 1);
}

/**
   @brief Render an on/off toggle button combo

   @param curValue the value of the toggle button
   @param valueOff label for value off (optionnal, defaults to "OFF")
   @param valueOn label for value on (optionnal, defaults to "ON")
   @param cropTextTo if >0 crop the combo text up to this number of characters to have it fit the available space (optionnal, defaults to 0)
   @return the selected value for the toggle button
 */
bool StreamBrowserDialog::renderOnOffToggle(bool curValue, const char* valueOff, const char* valueOn, uint8_t cropTextTo)
{
	auto& prefs = m_session.GetPreferences();
	ImVec4 color = ImGui::ColorConvertU32ToFloat4((curValue ? prefs.GetColor("Appearance.Stream Browser.instrument_on_badge_color") : prefs.GetColor("Appearance.Stream Browser.instrument_off_badge_color")));
	return renderToggle(color, curValue, valueOff, valueOn, cropTextTo);
}

/**
   @brief Render a download progress bar for a given instrument channel

   @param inst the instrument to render the progress channel for
   @param chan the channel to render the progress for
   @param isLast true if it is the last channel of the instrument
 */
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

/**
   @brief Render a PSU properties row

   @param isVoltage true for voltage rows, false for current rows
   @param cc true if the PSU channel is in constant current mode, false for constant voltage mode
   @param chan the PSU channel to render properties for
   @param setValue the set value text
   @param measuredValue the measured value text
   @param clicked output param for clicked state
   @param hovered output param for hovered state
 */
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
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.psu_set_label_color")));
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
	ImGui::PushID(isVoltage ? "sV" :  "sC");

	float height = ImGui::GetFontSize();
	ImVec4 color = ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.psu_7_segment_color"));

	if(prefs.GetBool("Appearance.Stream Browser.use_7_segment_display"))
	    Render7SegmentValue(setValue,color,height,clicked,hovered);
	else
	{
		clicked |= ImGui::TextLink(setValue);
		hovered |= ImGui::IsItemHovered();
	}
	ImGui::PopID();
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
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.psu_meas_label_color")));
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
	ImGui::PushID(isVoltage ? "mV" :  "mC");

	if(prefs.GetBool("Appearance.Stream Browser.use_7_segment_display"))
	    Render7SegmentValue(measuredValue, color,height,clicked,hovered);
	else
	{
		clicked |= ImGui::TextLink(measuredValue);
		hovered |= ImGui::IsItemHovered();
	}
	ImGui::PopID();
}

/**
   @brief Render AWG channel properties

   @param awg the AWG to render channel properties for
   @param awgchan the AWG channel to render properties for
   @param clicked output param for clicked state
   @param hovered output param for hovered state
 */
void StreamBrowserDialog::renderAwgProperties(std::shared_ptr<FunctionGenerator> awg, FunctionGeneratorChannel* awgchan, bool &clicked, bool &hovered)
{
	//FunctionGenerator::WaveShape shape = FunctionGenerator::WaveShape::SHAPE_SINE;
	FunctionGenerator::OutputImpedance impedance = FunctionGenerator::OutputImpedance::IMPEDANCE_HIGH_Z;
	size_t channelIndex = awgchan->GetIndex();
	float frequency = 0;
	float amplitude = 0;
	float offset = 0;
	auto awgState = m_session.GetFunctionGeneratorState(awg);
	if(awgState)
	{
		//shape = awgState->m_channelShape[channelIndex];
		impedance = awgState->m_channelOutputImpedance[channelIndex];
		frequency = awgState->m_channelFrequency[channelIndex];
		amplitude = awgState->m_channelAmplitude[channelIndex];
		offset = awgState->m_channelOffset[channelIndex];
	}
	auto frequency_txt = Unit(Unit::UNIT_HZ).PrettyPrint(frequency);
	auto amplitude_txt = Unit(Unit::UNIT_VOLTS).PrettyPrint(amplitude);
	auto offset_txt = Unit(Unit::UNIT_VOLTS).PrettyPrint(offset);

	auto& prefs = m_session.GetPreferences();
	// Row 1
	ImGui::Text("Waveform:");
	startBadgeLine(); // Needed for shape combo
	// Shape combo
	ImGui::PushID("waveform");
	// Get current shape and  shape index
	FunctionGenerator::WaveShape shape = awgState->m_channelShape[channelIndex];
	int shapeIndex = awgState->m_channelShapeIndexes[channelIndex][shape];
	if(renderCombo(ImGui::ColorConvertU32ToFloat4(ColorFromString(awgchan->m_displaycolor)), shapeIndex, awgState->m_channelShapeNames[channelIndex],true,3))
	{
		shape = awgState->m_channelShapes[channelIndex][shapeIndex];
		awg->SetFunctionChannelShape(channelIndex, shape);
		// Update state right now to cover from slow intruments
		awgState->m_channelShape[channelIndex]=shape;
		// Tell intrument thread that the FunctionGenerator state has to be updated
		awgState->m_needsUpdate[channelIndex] = true;
	}
	ImGui::PopID();

	// Row 2
	// Frequency label
	StreamDescriptor sv(awgchan, 0);
	ImGui::PushID("frequ");
	// TODO
	const char* freqLabel = "Frequency: ";
	ImGui::Selectable(freqLabel,false,0,ImVec2(ImGui::CalcTextSize(freqLabel).x, 0));
	if(ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("Scalar", &sv, sizeof(sv));
		string dragText = awgchan->GetDisplayName() + " frequency";
		ImGui::TextUnformatted(dragText.c_str());
		ImGui::EndDragDropSource();
	}
	else
		DoItemHelp();
	ImGui::PopID();
	// Frequency text
	ImGui::SameLine(0, 0);
	clicked |= ImGui::TextLink(frequency_txt.c_str());
	hovered |= ImGui::IsItemHovered();
	// Shape preview
	startBadgeLine();
	auto height = ImGui::GetFontSize() * 2;
	auto width =  height * 2;
	if ((m_badgeXCur - width) >= m_badgeXMin)
	{
		// ok, we have enough space draw preview
		m_badgeXCur -= width;
		ImGui::SameLine(m_badgeXCur);
		ImGui::Image(
			m_parent->GetTextureManager()->GetTexture(m_parent->GetIconForWaveformShape(shape)),
			ImVec2(width,height));
		// Go back one line since preview spans on two text lines
		ImGuiWindow *window = ImGui::GetCurrentWindowRead();
		window->DC.CursorPos.y -= ImGui::GetFontSize();
	}

	// Row 3
	// Amplitude label
	renderInfoLink("Amplitude",amplitude_txt.c_str(),clicked,hovered);
	// Row 4
	// Offset label
	renderInfoLink("Offsset",offset_txt.c_str(),clicked,hovered);
	// Impedance value
	startBadgeLine(); // Needed for impedance badge
	ImGui::PushID("impedance");
	bool isHiZ = (impedance == FunctionGenerator::OutputImpedance::IMPEDANCE_HIGH_Z);
	int comboValue = isHiZ ? 0 : 1;
	bool changed = renderCombo(	ImGui::ColorConvertU32ToFloat4(prefs.GetColor(isHiZ ? "Appearance.Stream Browser.awg_hiz_badge_color" : "Appearance.Stream Browser.awg_50ohms_badge_color"))
								,&comboValue
								,"Hi-Z", "50 Oh", NULL);
	if(changed)
	{
		impedance = ((comboValue == 0) ? FunctionGenerator::OutputImpedance::IMPEDANCE_HIGH_Z : FunctionGenerator::OutputImpedance::IMPEDANCE_50_OHM);
		awg->SetFunctionChannelOutputImpedance(channelIndex,impedance);
		// Update state right now to cover from slow intruments
		awgState->m_channelOutputImpedance[channelIndex]=impedance;
		// Tell intrument thread that the FunctionGenerator state has to be updated
		awgState->m_needsUpdate[channelIndex] = true;
	}
	ImGui::PopID();
}

/**
   @brief Render DMM channel properties

   @param dmm the DMM to render channel properties for
   @param dmmchan the DMM channel to render properties for
   @param clicked output param for clicked state
   @param hovered output param for hovered state
 */
void StreamBrowserDialog::renderDmmProperties(std::shared_ptr<Multimeter> dmm, MultimeterChannel* dmmchan, bool isMain, bool &clicked, bool &hovered)
{
	auto& prefs = m_session.GetPreferences();
	size_t streamIndex = isMain ? 0 : 1;
	Unit unit = dmmchan->GetYAxisUnits(streamIndex);
	float mainValue = dmmchan->GetScalarValue(streamIndex);
	string valueText = unit.PrettyPrint(mainValue,dmm->GetMeterDigits());
	ImVec4 color = ImGui::ColorConvertU32ToFloat4(ColorFromString(dmmchan->m_displaycolor));
	string streamName = isMain ? "Main" : "Secondary";

	ImGui::PushID(streamName.c_str());

	// Get available operating and current modes
	auto modemask = isMain ? dmm->GetMeasurementTypes() : dmm->GetSecondaryMeasurementTypes();
	auto curMode = isMain ? dmm->GetMeterMode() : dmm->GetSecondaryMeterMode();

	// Stream name
	bool open = ImGui::TreeNodeEx(streamName.c_str(), (curMode > 0) ? ImGuiTreeNodeFlags_DefaultOpen : 0);

	// Mode combo
	startBadgeLine();
	ImGui::PushID(streamName.c_str());
	vector<std::string> modeNames;
	vector<Multimeter::MeasurementTypes> modes;
	if(!isMain)
	{
		// Add None for secondary measurement to be able to disable it
		modeNames.push_back("None");
		modes.push_back(Multimeter::MeasurementTypes::NONE);
	}
	int modeSelector = 0;
	for(unsigned int i=0; i<32; i++)
	{
		auto mode = static_cast<Multimeter::MeasurementTypes>(1 << i);
		if(modemask & mode)
		{
			modes.push_back(mode);
			modeNames.push_back(dmm->ModeToText(mode));
			if(curMode == mode)
				modeSelector = modes.size() - 1;
		}
	}

	if(renderCombo(color, modeSelector, modeNames,true,3))
	{
		curMode = modes[modeSelector];
		if(isMain)
			dmm->SetMeterMode(curMode);
		else
		{
			dmm->SetSecondaryMeterMode(curMode);
			// Open or close tree node according the secondary measure mode
		    ImGuiContext& g = *GImGui;
			ImGui::TreeNodeSetOpen(g.LastItemData.ID,(curMode > 0));
		}
	}
	ImGui::PopID();

	StreamDescriptor s(dmmchan, streamIndex);
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

	if(open)
		ImGui::TreePop();

	if(open)
	{
		if(prefs.GetBool("Appearance.Stream Browser.use_7_segment_display"))
			Render7SegmentValue(valueText, color,ImGui::GetFontSize()*2,clicked,hovered);
		else
		{
			clicked |= ImGui::TextLink(valueText.c_str());
			hovered |= ImGui::IsItemHovered();
		}
		if(isMain)
		{
			ImGui::PushID("autorange");
			// For main, also show the autorange combo
			startBadgeLine();
			bool autorange = dmm->GetMeterAutoRange();
			bool result = renderOnOffToggle(autorange,"Manual Range","Autorange",3);
			if(result != autorange)
				dmm->SetMeterAutoRange(result);
			ImGui::PopID();
		}
	}

	ImGui::PopID();
}

/**
   @brief Rendering of an instrument node

   @param instrument the instrument to render
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
			switch (mode)
			{
			case Oscilloscope::TRIGGER_MODE_RUN:
				renderInstrumentBadge(instrument,true,BADGE_ARMED);
				break;
			case Oscilloscope::TRIGGER_MODE_STOP:
				renderInstrumentBadge(instrument,true,BADGE_STOPPED);
				break;
			case Oscilloscope::TRIGGER_MODE_TRIGGERED:
				renderInstrumentBadge(instrument,false,BADGE_TRIGGERED);
				break;
			case Oscilloscope::TRIGGER_MODE_WAIT:
				renderInstrumentBadge(instrument,true,BADGE_BUSY);
				break;
			case Oscilloscope::TRIGGER_MODE_AUTO:
				renderInstrumentBadge(instrument,false,BADGE_AUTO);
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
		{
			if(psu->SupportsMasterOutputSwitching())
				psu->SetMasterPowerEnable(result);
			else
			{
				for(size_t i = 0 ; i < channelCount ; i++)
				{
					psu->SetPowerChannelActive(i,result);
				}
			}
		}
	}

	if(instIsOpen)
	{
		size_t lastEnabledChannelIndex = 0;
		if (scope)
		{
			ImGui::BeginChild("sample_params", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border);

			if(scope->HasTimebaseControls())
			{
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
			}
			if(scope->HasFrequencyControls())
			{
				auto sdepth_txt 	= Unit(Unit::UNIT_SAMPLEDEPTH).PrettyPrint(scope->GetSampleDepth());
				auto rbw_txt    	= Unit(Unit::UNIT_HZ).PrettyPrint(scope->GetResolutionBandwidth());
				auto centerFreq_txt = Unit(Unit::UNIT_HZ).PrettyPrint(scope->GetCenterFrequency(0));
				auto span_txt 		= Unit(Unit::UNIT_HZ).PrettyPrint(scope->GetSpan());

				bool clicked = false;
				bool hovered = false;
				if(!scope->HasTimebaseControls()) // Only render sample depth if it has not already been shown in timebase controls
					renderInfoLink("Points", sdepth_txt.c_str(), clicked, hovered);
				renderInfoLink("Rbw", rbw_txt.c_str(), clicked, hovered);
				renderInfoLink("Center freq.", centerFreq_txt.c_str(), clicked, hovered);
				renderInfoLink("Span", span_txt.c_str(), clicked, hovered);
				if (clicked)
					m_parent->ShowTimebaseProperties();
				if (hovered)
					m_parent->AddStatusHelp("mouse_lmb", "Open timebase properties");
				for(size_t i = 0; i<channelCount; i++)
				{
					if(scope->IsChannelEnabled(i))
						lastEnabledChannelIndex = i;
				}
			}

			ImGui::EndChild();
		}

		for(size_t i=0; i<channelCount; i++)
		{
			// Iterate on each channel
			renderChannelNode(instrument,i,(i == lastEnabledChannelIndex));
		}

		ImGui::TreePop();
	}
	ImGui::PopID();

}

/**
   @brief Rendering of a channel node

   @param instrument the instrument containing the instrument to render
   @param channelIndex the index of the channel to render
   @param isLast true if this is the last channel of the instrument
 */
void StreamBrowserDialog::renderChannelNode(shared_ptr<Instrument> instrument, size_t channelIndex, bool isLast)
{
	// Get preferences for colors
	auto& prefs = m_session.GetPreferences();

	InstrumentChannel* channel = instrument->GetChannel(channelIndex);

	ImGui::PushID(channelIndex);

	auto psu = std::dynamic_pointer_cast<SCPIPowerSupply>(instrument);
	auto scope = std::dynamic_pointer_cast<Oscilloscope>(instrument);
	auto awg = std::dynamic_pointer_cast<FunctionGenerator>(instrument);
	auto dmm = std::dynamic_pointer_cast<Multimeter>(instrument);

	bool singleStream = channel->GetStreamCount() == 1;
	auto scopechan = dynamic_cast<OscilloscopeChannel *>(channel);
	auto psuchan = dynamic_cast<PowerSupplyChannel *>(channel);
	auto awgchan = dynamic_cast<FunctionGeneratorChannel *>(channel);
	auto dmmchan = dynamic_cast<MultimeterChannel *>(channel);
	bool renderProps = false;
	bool isDigital = false;
	if (scopechan)
	{
		renderProps = scopechan->IsEnabled();
		isDigital = scopechan->GetType(0) == Stream::STREAM_TYPE_DIGITAL;
	}
	else if(awg && awgchan)
	{
		renderProps = m_session.GetFunctionGeneratorState(awg)->m_channelActive[channelIndex];
	}

	bool hasChildren = !singleStream || renderProps;

	if (channel->m_displaycolor != "")
		ImGui::PushStyleColor(ImGuiCol_Text, ColorFromString(channel->m_displaycolor));

	int flags = 0;
	if(!hasChildren)
		flags |= ImGuiTreeNodeFlags_Leaf;

	//Collapse digital channel nodes by default to reduce clutter
	if(isDigital)
	{}

	else
		flags |= ImGuiTreeNodeFlags_DefaultOpen;

	bool open = ImGui::TreeNodeEx(
		channel->GetDisplayName().c_str(),
		flags);
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
		//No badge on trigger inputs
		if(scopechan->GetType(0) == Stream::STREAM_TYPE_TRIGGER)
		{}

		// Scope channel
		else if (!scopechan->IsEnabled())
			renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.instrument_disabled_badge_color")), "DISABLED", "DISA","--", NULL);

		//Download in progress
		else
			renderDownloadProgress(instrument, channel, isLast);
	}
	else if(psu)
	{	// PSU Channel
		//Get the state
		auto psustate = m_session.GetPSUState(psu);

		bool active = psustate->m_channelOn[channelIndex];
		bool result = renderOnOffToggle(active);
		if(result != active)
			psu->SetPowerChannelActive(channelIndex,result);
	}
	else if(awg && awgchan)
	{
		// AWG Channel : get the state
		auto awgstate = m_session.GetFunctionGeneratorState(awg);

		bool active = awgstate->m_channelActive[channelIndex];
		bool result = renderOnOffToggle(active);
		if(result != active)
		{
			awg->SetFunctionChannelActive(channelIndex,result);
			auto awgState = m_session.GetFunctionGeneratorState(awg);
			if(awgState)
			{
				// Update state right now to cover from slow intruments
				awgState->m_channelActive[channelIndex]=result;
				// Tell intrument thread that the FunctionGenerator state has to be updated
				awgState->m_needsUpdate[channelIndex] = true;
			}

		}
	}


	if(open)
	{
		ImGui::PushID(instrument.get());
		if(psu)
		{
			// For PSU we will have a special handling for the 4 streams associated to a PSU channel
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
		else if(awg && awgchan)
		{
			// No stream for FunctionGenerator => render properties on channel node
			ImGui::BeginChild("awg_params", ImVec2(0, 0),
				ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border);
			bool clicked = false;
			bool hovered = false;
			renderAwgProperties(awg,awgchan,clicked,hovered);
			if (clicked)
			{
				m_parent->ShowInstrumentProperties(awg);
			}
			if (hovered)
				m_parent->AddStatusHelp("mouse_lmb", "Open Function Generator properties");
			ImGui::EndChild();
		}
		else if(dmm && dmmchan)
		{
			ImGui::BeginChild("dmm_params", ImVec2(0, 0),
				ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border);
			// Always 2 streams for dmm channel => render properties on channel node
			bool clicked = false;
			bool hovered = false;
			// Main measurement
			renderDmmProperties(dmm,dmmchan,true,clicked,hovered);
			// Secondary measurement
			renderDmmProperties(dmm,dmmchan,false,clicked,hovered);
			if (clicked)
			{
				m_parent->ShowInstrumentProperties(dmm);
			}
			if (hovered)
				m_parent->AddStatusHelp("mouse_lmb", "Open Multimeter properties");

			ImGui::EndChild();
		}
		else
		{
			size_t streamCount = channel->GetStreamCount();
			for(size_t j=0; j<streamCount; j++)
			{
				// Iterate on each stream
				renderStreamNode(instrument,channel,j,!singleStream,renderProps,(j==(streamCount-1)));
			}
		}
		ImGui::PopID();
	}

	if(open)
		ImGui::TreePop();

	ImGui::PopID();
}

/**
   @brief Rendering of a stream node

   @param instrument the instrument containing the stream to render (may be null if the stream is in a Filter)
   @param channel the channel or the Filter containing the stream to render
   @param streamIndex the index of the stream to render
   @param renderName true if the name of the stream should be rendred as a selectable item
   @param renderProps true if a properties block should be rendered for this stream
   @param isLast true if this is the last stream of the channel
 */
void StreamBrowserDialog::renderStreamNode(shared_ptr<Instrument> instrument, InstrumentChannel* channel, size_t streamIndex, bool renderName, bool renderProps, bool isLast)
{
	auto scope = std::dynamic_pointer_cast<Oscilloscope>(instrument);
	auto scopechan = dynamic_cast<OscilloscopeChannel *>(channel);
	Stream::StreamType type = scopechan ? scopechan->GetType(streamIndex) : Stream::StreamType::STREAM_TYPE_ANALOG;

	ImGui::PushID(streamIndex);

	// Stream name
	if (renderName)
	{
		ImGui::Selectable(channel->GetStreamName(streamIndex).c_str());

		StreamDescriptor s(channel, streamIndex);
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
	// Channel/stream properties block
	if(renderProps && scopechan)
	{
		// If no properties are available for this stream, only show a "Properties" link if it is the last stream of the channel/filter
		bool hasProps = isLast;
		switch (type)
		{
			case Stream::STREAM_TYPE_ANALOG:
				hasProps = true;
				break;
			case Stream::STREAM_TYPE_DIGITAL:
				if(scope)
				{
					hasProps = true;
				}
				break;
			default:
				break;
		}
		if(hasProps)
		{
			ImGui::BeginChild("stream_params", ImVec2(0, 0),
				ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border);

			Unit unit = channel->GetYAxisUnits(streamIndex);
			bool clicked = false;
			bool hovered = false;
			switch (type)
			{
				case Stream::STREAM_TYPE_ANALOG:
					{
						auto offset_txt = unit.PrettyPrint(scopechan->GetOffset(streamIndex));
						auto range_txt = unit.PrettyPrint(scopechan->GetVoltageRange(streamIndex));
						renderInfoLink("Offset", offset_txt.c_str(), clicked, hovered);
						renderInfoLink("Vertical range", range_txt.c_str(), clicked, hovered);
					}
					break;
				case Stream::STREAM_TYPE_DIGITAL:
					if(scope)
					{
						auto threshold_txt = unit.PrettyPrint(scope->GetDigitalThreshold(scopechan->GetIndex()));
						renderInfoLink("Threshold", threshold_txt.c_str(), clicked, hovered);
						break;
					}
					//fall through
				default:
					{
						clicked = ImGui::TextLink("Properties");
						hovered = ImGui::IsItemHovered();
					}
					break;
			}
			ImGui::EndChild();
			if (clicked)
			{
				m_parent->ShowChannelProperties(scopechan);
			}
			if (hovered)
				m_parent->AddStatusHelp("mouse_lmb", "Open properties");
		}
	}
	ImGui::PopID();
}

/**
   @brief Rendering of a Filter node

   @param filter the filter to render
 */
void StreamBrowserDialog::renderFilterNode(Filter* filter)
{
	ImGui::PushID(filter);

	bool singleStream = filter->GetStreamCount() == 1;

	if (filter->m_displaycolor != "")
		ImGui::PushStyleColor(ImGuiCol_Text, ColorFromString(filter->m_displaycolor));

	//Don't expand filters with a single stream by default
	int flags = 0;
	if(!singleStream)
		flags |= ImGuiTreeNodeFlags_DefaultOpen;

	bool open = ImGui::TreeNodeEx(filter->GetDisplayName().c_str(), flags);
	if (filter->m_displaycolor != "")
		ImGui::PopStyleColor();

	//Single stream: drag the stream not the filter
	if(singleStream)
	{
		StreamDescriptor s(filter, 0);
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
		ImGui::SetDragDropPayload("Channel", &filter, sizeof(filter));

		ImGui::TextUnformatted(filter->GetDisplayName().c_str());
		ImGui::EndDragDropSource();
	}

	// Channel decoration
	if(open)
	{
		ImGui::PushID(filter);

		size_t streamCount = filter->GetStreamCount();
		for(size_t j=0; j<streamCount; j++)
		{
			// Iterate on each stream
			renderStreamNode(nullptr,filter,j,!singleStream,true,(j==(streamCount-1)));
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
			renderFilterNode(f);
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
