/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2026 Andrew D. Zonenberg and contributors                                                         *
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
	@brief Declaration of MultimeterDialog
 */
#ifndef MultimeterDialog_h
#define MultimeterDialog_h

#include "Dialog.h"
#include "Session.h"

class MultimeterDialog : public Dialog
{
public:
	MultimeterDialog(std::shared_ptr<SCPIMultimeter> meter, std::shared_ptr<MultimeterState> state, Session* session);
	virtual ~MultimeterDialog();

	virtual bool DoRender();

	std::shared_ptr<SCPIMultimeter> GetMeter()
	{ return m_meter; }

protected:
	void OnPrimaryModeChanged();
	void RefreshSecondaryModeList();

	///@brief Session handle so we can remove the PSU when closed
	Session* m_session;

	///@brief Timestamp of when we opened the dialog
	double m_tstart;

	///@brief The meter we're controlling
	std::shared_ptr<SCPIMultimeter> m_meter;

	///@brief Current channel stats, live updated
	std::shared_ptr<MultimeterState> m_state;

	///@brief Set of channel names
	std::vector<std::string> m_channelNames;

	///@brief The currently selected input channel
	int m_selectedChannel;

	///@brief Names of primary channel operating modes
	std::vector<std::string> m_primaryModeNames;

	///@brief List of primary channel operating modes
	std::vector<Multimeter::MeasurementTypes> m_primaryModes;

	///@brief Index of primary mode
	int m_primaryModeSelector;

	///@brief Names of secondary channel operating modes
	std::vector<std::string> m_secondaryModeNames;

	///@brief List of secondary channel operating modes
	std::vector<Multimeter::MeasurementTypes> m_secondaryModes;

	///@brief Index of secondary mode
	int m_secondaryModeSelector;

	///@brief Autorange enable flag
	bool m_autorange;
};



#endif
