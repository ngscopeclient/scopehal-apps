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
	@brief Declaration of StreamBrowserDialog
 */
#ifndef StreamBrowserDialog_h
#define StreamBrowserDialog_h

#include <map>

#include "Dialog.h"
#include "Session.h"

using namespace std;

class MainWindow;

class StreamBrowserDialog : public Dialog
{
public:
	StreamBrowserDialog(Session& session, MainWindow* parent);
	virtual ~StreamBrowserDialog();

	virtual bool DoRender() override;

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
	
	// Rendeding of StreamBorwserDialog elements
	void renderInfoLink(const char *label, const char *linktext, bool &clicked, bool &hovered);
	void startBadgeLine();
	bool renderBadge(ImVec4 color, ... /* labels, ending in NULL */);
	bool renderInstrumentBadge(std::shared_ptr<Instrument> inst, bool latched, InstrumentBadge badge);
	bool renderCombo(ImVec4 color,int &selected, const std::vector<string> &values, bool useColorForText = false, uint8_t cropTextTo = 0);
	bool renderCombo(ImVec4 color,int* selected, ... /* values, ending in NULL */);
	bool renderToggle(ImVec4 color, bool curValue, const char* valueOff = "OFF", const char* valueOn = "ON", uint8_t cropTextTo = 0);
	bool renderOnOffToggle(bool curValue, const char* valueOff = "OFF", const char* valueOn = "ON", uint8_t cropTextTo = 0);
	void renderDownloadProgress(std::shared_ptr<Instrument> inst, InstrumentChannel *chan, bool isLast);
	void renderPsuRows(bool isVoltage, bool cc, PowerSupplyChannel* chan,const char *setValue, const char *measuredValue, bool &clicked, bool &hovered);
	void renderAwgProperties(std::shared_ptr<FunctionGenerator> awg, FunctionGeneratorChannel* awgchan, bool &clicked, bool &hovered);
	void renderDmmProperties(std::shared_ptr<Multimeter> dmm, MultimeterChannel* dmmchan, bool isMain, bool &clicked, bool &hovered);

	// Rendering of an instrument node
	void renderInstrumentNode(shared_ptr<Instrument> instrument);

	// Rendering of a channel node
	void renderChannelNode(shared_ptr<Instrument> instrument, size_t channelIndex, bool isLast);

	// Rendering of a stream node
	void renderStreamNode(shared_ptr<Instrument> instrument, InstrumentChannel* channel, size_t streamIndex, bool renderName, bool renderProps, bool isLast);

	// Rendering of an Filter node
	void renderFilterNode(Filter* filter);

	Session& m_session;
	MainWindow* m_parent;

	// @brief Positions for badge display
	float m_badgeXMin; // left edge over which we must not overrun
	float m_badgeXCur; // right edge to render the next badge against

	std::map<std::shared_ptr<Instrument>, bool> m_instrumentDownloadIsSlow;
	// @brief Store the last state of an intrument badge (used for badge state latching)
	std::map<std::shared_ptr<Instrument>, pair<double, InstrumentBadge>> m_instrumentLastBadge;
};

#endif
