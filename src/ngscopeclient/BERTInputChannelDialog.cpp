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
	@brief Implementation of BERTInputChannelDialog
 */

#include "ngscopeclient.h"
#include "MainWindow.h"
#include "BERTInputChannelDialog.h"
#include <imgui_node_editor.h>
#include "FileBrowser.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

BERTInputChannelDialog::BERTInputChannelDialog(BERTInputChannel* chan, MainWindow* parent, bool graphEditorMode)
	: EmbeddableDialog(chan->GetHwname(), string("Channel properties: ") + chan->GetHwname(), ImVec2(300, 400), graphEditorMode)
	, m_channel(chan)
	, m_parent(parent)
{
	m_committedDisplayName = m_channel->GetDisplayName();
	m_displayName = m_committedDisplayName;

	//Color
	auto color = ColorFromString(m_channel->m_displaycolor);
	m_color[0] = ((color >> IM_COL32_R_SHIFT) & 0xff) / 255.0f;
	m_color[1] = ((color >> IM_COL32_G_SHIFT) & 0xff) / 255.0f;
	m_color[2] = ((color >> IM_COL32_B_SHIFT) & 0xff) / 255.0f;

	m_invert = chan->GetInvert();
	auto bert = chan->GetBERT().lock();

	//Receive pattern
	BERT::Pattern pat = chan->GetPattern();
	m_patternValues = chan->GetAvailablePatterns();
	m_patternIndex = 0;
	for(size_t i=0; i<m_patternValues.size(); i++)
	{
		auto p = m_patternValues[i];
		m_patternNames.push_back(bert->GetPatternName(p));
		if(p == pat)
			m_patternIndex = i;
	}

	//Receive CTLE config
	m_ctleIndex = chan->GetCTLEGainStep();
	auto steps = chan->GetCTLEGainSteps();
	Unit db(Unit::UNIT_DB);
	for(auto step : steps)
		m_ctleNames.push_back(db.PrettyPrint(step));

	//Scan depth
	int64_t depth = chan->GetScanDepth();
	m_scanValues = chan->GetScanDepths();
	m_scanIndex = 0;
	Unit sd(Unit::UNIT_SAMPLEDEPTH);
	for(size_t i=0; i<m_scanValues.size(); i++)
	{
		m_scanNames.push_back(sd.PrettyPrint(m_scanValues[i]));

		if(m_scanValues[i] <= depth)
			m_scanIndex = i;
	}

	//Rescale fs to ps for display
	int64_t tmp;
	chan->GetBERSamplingPoint(tmp, m_sampleY);
	m_sampleX = tmp * 1e-3;
	m_committedSampleX = m_sampleX;
	m_committedSampleY = m_sampleY;

	m_tempMaskFile = chan->GetMaskFile();
	m_committedMaskFile = m_tempMaskFile;

	//Data rate
	auto currentRate = chan->GetDataRate();
	m_dataRateIndex = 0;
	m_dataRates = bert->GetAvailableDataRates();
	Unit bps(Unit::UNIT_BITRATE);
	m_dataRateNames.clear();
	for(size_t i=0; i<m_dataRates.size(); i++)
	{
		auto rate = m_dataRates[i];
		if(rate == currentRate)
			m_dataRateIndex = i;

		m_dataRateNames.push_back(bps.PrettyPrint(rate));
	}
}

BERTInputChannelDialog::~BERTInputChannelDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool BERTInputChannelDialog::Render()
{
	RunFileDialog();
	return Dialog::Render();
}

void BERTInputChannelDialog::RunFileDialog()
{
	//Run file browser dialog
	if(m_fileDialog)
	{
		m_fileDialog->Render();

		if(m_fileDialog->IsClosedOK())
		{
			m_committedMaskFile = m_fileDialog->GetFileName();
			m_tempMaskFile = m_committedMaskFile;
			m_channel->SetMaskFile(m_committedMaskFile);
		}

		if(m_fileDialog->IsClosed())
			m_fileDialog = nullptr;
	}
}

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool BERTInputChannelDialog::DoRender()
{
	//Flags for a header that should be open by default EXCEPT in the graph editor
	ImGuiTreeNodeFlags defaultOpenFlags = m_graphEditorMode ? 0 : ImGuiTreeNodeFlags_DefaultOpen;

	float width = 10 * ImGui::GetFontSize();

	auto bert = m_channel->GetBERT().lock();
	if(!bert)
		return false;
	if(ImGui::CollapsingHeader("Info"))
	{
		auto nickname = bert->m_nickname;
		auto index = to_string(m_channel->GetIndex() + 1);	//use one based index for display

		ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(width);
			ImGui::InputText("Instrument", &nickname);
		ImGui::EndDisabled();
		HelpMarker("The instrument this channel was measured by");

		ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(width);
			ImGui::InputText("Hardware Channel", &index);
		ImGui::EndDisabled();
		HelpMarker("Physical channel number (starting from 1) on the instrument front panel");
	}

	//All channels have display settings
	if(ImGui::CollapsingHeader("Display", defaultOpenFlags))
	{
		ImGui::SetNextItemWidth(width);
		if(TextInputWithImplicitApply("Nickname", m_displayName, m_committedDisplayName))
			m_channel->SetDisplayName(m_committedDisplayName);

		HelpMarker("Display name for the channel");

		if(ImGui::ColorEdit3(
			"Color",
			m_color,
			ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Uint8))
		{
			char tmp[32];
			snprintf(tmp, sizeof(tmp), "#%02x%02x%02x",
				static_cast<int>(round(m_color[0] * 255)),
				static_cast<int>(round(m_color[1] * 255)),
				static_cast<int>(round(m_color[2] * 255)));
			m_channel->m_displaycolor = tmp;
		}
	}

	if(ImGui::CollapsingHeader("Receiver", defaultOpenFlags))
	{
		ImGui::SetNextItemWidth(width);
		if(ImGui::Checkbox("Invert", &m_invert))
			m_channel->SetInvert(m_invert);
		HelpMarker("Inverts the polarity of the input");

		if(m_channel->HasCTLE())
		{
			ImGui::SetNextItemWidth(width);
			if(Dialog::Combo("CTLE Gain", m_ctleNames, m_ctleIndex))
				m_channel->SetCTLEGainStep(m_ctleIndex);
			HelpMarker("Gain step for the continuous-time linear equalizer");
		}
	}

	if(ImGui::CollapsingHeader("CDR", defaultOpenFlags))
	{
		ImGui::BeginDisabled();
			auto lock = m_channel->GetCdrLockState();
			ImGui::Checkbox("Lock", &lock);
		ImGui::EndDisabled();
		HelpMarker(
			"Indicates whether the clock recovery loop and PRBS checker are locked to incoming data.\n"
			"If not locked, no measurements can be made.");
	}

	if(ImGui::CollapsingHeader("Pattern Checker", defaultOpenFlags))
	{
		ImGui::SetNextItemWidth(width);
		if(Dialog::Combo("Pattern", m_patternNames, m_patternIndex))
			m_channel->SetPattern(m_patternValues[m_patternIndex]);
		HelpMarker("Expected PRBS pattern");
	}

	if(bert->IsDataRatePerChannel())
	{
		if(ImGui::CollapsingHeader("Timebase", defaultOpenFlags))
		{
			ImGui::SetNextItemWidth(width);
			if(Dialog::Combo("Data Rate", m_dataRateNames, m_dataRateIndex))
				m_channel->SetDataRate(m_dataRates[m_dataRateIndex]);
			HelpMarker("PHY signaling rate for this transmit port");
		}
	}

	if(ImGui::CollapsingHeader("Measurements", defaultOpenFlags))
	{
		auto state = m_parent->GetSession().GetBERTState(m_channel->GetBERT().lock());

		float freq = m_channel->GetDataRate();
		float uiWidth = FS_PER_SECOND / (1000 * freq);

		if(bert->HasConfigurableScanDepth())
		{
			ImGui::SetNextItemWidth(width);
			if(Dialog::Combo("Integration Depth", m_scanNames, m_scanIndex))
				m_channel->SetScanDepth(m_scanValues[m_scanIndex]);
			HelpMarker(
				"Maximum number of UIs to integrate at each point in the scan.\n"
				"Higher values give better accuracy at lower BER values, but increase scan time.");
		}

		//See if sampling point moved outside our dialog
		//If so, move the sliders
		int64_t tmpX;
		float tmpY;
		m_channel->GetBERSamplingPoint(tmpX, tmpY);
		tmpX *= 1e-3;
		if( (tmpX != m_committedSampleX) || (fabs(tmpY - m_committedSampleY) > 0.001) )
		{
			m_sampleX = m_committedSampleX = tmpX;
			m_sampleY = m_committedSampleY = tmpY;
		}

		ImGui::SetNextItemWidth(width);
		if(ImGui::SliderFloat("Sample X", &m_sampleX, -uiWidth/2, uiWidth/2))
		{
			m_channel->SetBERSamplingPoint(m_sampleX * 1e3, m_sampleY);
			m_committedSampleX = m_sampleX;
		}
		HelpMarker("Sampling time for BER measurements, in ps relative to center of UI");

		ImGui::SetNextItemWidth(width);
		if(ImGui::SliderFloat("Sample Y", &m_sampleY, -0.2, 0.2))
		{
			m_channel->SetBERSamplingPoint(m_sampleX * 1e3, m_sampleY);
			m_committedSampleY = m_sampleY;
		}
		HelpMarker("Sampling offset for BER measurements, in V relative to center of UI");

		ImGui::SetNextItemWidth(width);
		if(ImGui::Button("Horz Bathtub"))
		{
			//Make sure we have a plot to see the data in
			m_parent->AddAreaForStreamIfNotAlreadyVisible(m_channel->GetHBathtubStream());

			//Request the bathtub measurement
			state->m_horzBathtubScanPending[m_channel->GetIndex()] = true;
		}

		Unit fs(Unit::UNIT_FS);
		ImGui::SameLine();

		//Scan progress or estimated run time
		if(m_channel->IsHBathtubScanInProgress())
			ImGui::ProgressBar(m_channel->GetScanProgress(), ImVec2(2*width, 0));
		else
			ImGui::Text("Estimated %s", fs.PrettyPrint(m_channel->GetExpectedBathtubCaptureTime(), 5).c_str());

		HelpMarker("Acquire a single horizontal bathtub measurement");

		if(ImGui::Button("Eye"))
		{
			//Make sure we have a plot to see the data in
			m_parent->AddAreaForStreamIfNotAlreadyVisible(m_channel->GetEyeStream());

			//Request the eye measurement
			state->m_eyeScanPending[m_channel->GetIndex()] = true;
		}
		ImGui::SameLine();

		//Scan progress or estimated run time
		if(m_channel->IsEyeScanInProgress())
			ImGui::ProgressBar(m_channel->GetScanProgress(), ImVec2(2*width, 0));
		else
			ImGui::Text("Estimated %s", fs.PrettyPrint(m_channel->GetExpectedEyeCaptureTime(), 5).c_str());
		HelpMarker("Acquire a single eye pattern measurement");

		//Input path
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
		if(TextInputWithImplicitApply("###pathmask", m_tempMaskFile, m_committedMaskFile))
			m_channel->SetMaskFile(m_committedMaskFile);

		//Browser button
		ImGui::SameLine();
		if(ImGui::Button("...###maskbrowser"))
		{
			if(!m_fileDialog)
			{
				m_fileDialog = MakeFileBrowser(
					m_parent,
					m_committedMaskFile,
					"Select File",
					"YAML files (*.yml)",
					"*.yml",
					false);
			}
			else
				LogTrace("file dialog is already open, ignoring additional button click\n");
		}
		ImGui::SameLine();
		ImGui::TextUnformatted("Mask file");
		HelpMarker("Mask data file for pass/fail testing");
	}

	return true;
}
