/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2025 Andrew D. Zonenberg and contributors                                                         *
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
	@brief Declaration of StreamBrowserDialog
 */
#ifndef StreamBrowserDialog_h
#define StreamBrowserDialog_h

#include <map>

#include "Dialog.h"
#include "Session.h"

class MainWindow;

class StreamBrowserTimebaseInfo
{
public:
	StreamBrowserTimebaseInfo(std::shared_ptr<Oscilloscope> scope);

	bool m_interleaving;

	//Sample rate
	std::vector<uint64_t> m_rates;
	std::vector<std::string> m_rateNames;
	int m_rate;

	uint64_t GetRate()
	{
		if(m_rates.empty())
			return 0;
		return m_rates[m_rate];
	}

	//Memory depth
	std::vector<uint64_t> m_depths;
	std::vector<std::string> m_depthNames;
	int m_depth;

	//Sampling mode
	//(only valid if both RT and equivalent are available)
	int m_samplingMode;

	//Resolution Bandwidth
	std::string m_rbwText;
	int64_t m_rbw;

	//Frequency domain controls
	std::string m_centerText;
	double m_center;

	std::string m_spanText;
	double m_span;

	std::string m_startText;
	double m_start;

	std::string m_endText;
	double m_end;

	//Spectrometer controls
	std::string m_integrationText;
	double m_integrationTime;

	//ADC mode controls
	std::vector<std::string> m_adcmodeNames;
	int m_adcmode;
};

class StreamBrowserDialog : public Dialog
{
public:
	StreamBrowserDialog(Session& session, MainWindow* parent);
	virtual ~StreamBrowserDialog();

	virtual bool DoRender() override;

	void FlushConfigCache();

protected:
	/**
	   @brief State of badges used in intrument node rendering
	 */
	enum InstrumentBadge
	{
		BADGE_ARMED,
		BADGE_STOPPED,
		BADGE_TRIGGERED,
		BADGE_BUSY,
		BADGE_AUTO
	};

	void DoItemHelp();

	// Block handling
	bool BeginBlock(const char *label, bool withButton = false);
	void EndBlock();

	// Rendeding of StreamBrowserDialog elements
	void renderInfoLink(const char *label, const char *linktext, bool &clicked, bool &hovered);
	void startBadgeLine();
	void renderBadge(ImVec4 color, ... /* labels, ending in NULL */);
	void renderInstrumentBadge(std::shared_ptr<Instrument> inst, bool latched, InstrumentBadge badge);
	bool renderCombo(
		const char* label,
		bool alignRight,
		ImVec4 color,
		int &selected,
		const std::vector<std::string>& values,
		bool useColorForText = false,
		uint8_t cropTextTo = 0,
		bool hideArrow = true,
		int paddingRight = 0);
	bool renderCombo(
		const char* label,
		bool alignRight,
		ImVec4 color,
		int* selected,
		...);
	bool renderToggle(
		const char* label,
		bool alignRight,
		ImVec4 color,
		bool& curValue, 
		const char* valueOff = "OFF", 
		const char* valueOn = "ON", 
		uint8_t cropTextTo = 0);
	bool renderOnOffToggle(const char* label, bool alignRight, bool& curValue, const char* valueOff = "OFF", const char* valueOn = "ON", uint8_t cropTextTo = 0);
	void renderNumericValue(const std::string& value, bool &clicked, bool &hovered, ImVec4 color = ImVec4(1, 1, 1, 1), bool allow7SegmentDisplay = false, float digitHeight = 0, bool clickable = true);
	template<typename T>
	bool renderEditableNumericValue(const std::string& label, std::string& currentValue, T& committedValue, Unit unit, ImVec4 color = ImVec4(1, 1, 1, 1), bool allow7SegmentDisplay = false, bool explicitApply = false);
	template<typename T>
	bool renderEditableNumericValueWithExplicitApply(const std::string& label, std::string& currentValue, T& committedValue, Unit unit, ImVec4 color = ImVec4(1, 1, 1, 1), bool allow7SegmentDisplay = false);
	void renderDownloadProgress(std::shared_ptr<Instrument> inst, InstrumentChannel *chan, bool isLast);
	bool renderPsuRows(bool isVoltage, bool cc, PowerSupplyChannel* chan, std::string& currentValue, float& committedValue, std::string& measuredValue, bool &clicked, bool &hovered);
	void renderAwgProperties(std::shared_ptr<FunctionGenerator> awg, FunctionGeneratorChannel* awgchan);
	void renderDmmProperties(std::shared_ptr<Multimeter> dmm, MultimeterChannel* dmmchan, bool isMain, bool &clicked, bool &hovered);

	// Rendering of an instrument node
	void renderInstrumentNode(std::shared_ptr<Instrument> instrument);
	std::shared_ptr<StreamBrowserTimebaseInfo> GetTimebaseInfoFor(std::shared_ptr<Oscilloscope>& scope);
	void DoTimebaseSettings(std::shared_ptr<Oscilloscope> scope);
	void DoFrequencySettings(std::shared_ptr<Oscilloscope> scope);
	void DoSpectrometerSettings(std::shared_ptr<SCPISpectrometer> spec);

	// Rendering of a channel node
	void renderChannelNode(std::shared_ptr<Instrument> instrument, size_t channelIndex, bool isLast);

	// Rendering of a stream node
	void renderStreamNode(std::shared_ptr<Instrument> instrument, InstrumentChannel* channel, size_t streamIndex, bool renderName, bool renderProps, bool isLast);

	// Rendering of an Filter node
	void renderFilterNode(Filter* filter);

	Session& m_session;
	MainWindow* m_parent;

	///@brief Positions for badge display
	float m_badgeXMin; // left edge over which we must not overrun
	float m_badgeXCur; // right edge to render the next badge against

	///@brief Id of the item currently beeing edited
	ImGuiID m_editedItemId = 0;
	///@brief Id of the last edited item
	ImGuiID m_lastEditedItemId = 0;

	std::map<std::shared_ptr<Instrument>, bool> m_instrumentDownloadIsSlow;
	///@brief Store the last state of an intrument badge (used for badge state latching)
	std::map<std::shared_ptr<Instrument>, std::pair<double, InstrumentBadge>> m_instrumentLastBadge;

	///@brief Map of instruments to timebase settings
	std::map<std::shared_ptr<Instrument>, std::shared_ptr<StreamBrowserTimebaseInfo> > m_timebaseConfig;

	///@brief Helper to render a small button that's non-interactive
	void SmallDisabledButton(const char* label)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1);
		ImGui::BeginDisabled();
		ImGui::SmallButton(label);
		ImGui::EndDisabled();
		ImGui::PopStyleVar();
	}
};

#endif
