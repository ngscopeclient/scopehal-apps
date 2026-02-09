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
	@brief Declaration of AddInstrumentDialog
 */
#ifndef AddInstrumentDialog_h
#define AddInstrumentDialog_h

#include "Dialog.h"
#include "Session.h"
#include <unordered_set>

class AddInstrumentDialog : public Dialog
{
public:
	AddInstrumentDialog(
		const std::string& title,
		const std::string& nickname,
		Session* session,
		MainWindow* parent,
		const std::string& driverType,
		const std::string& driver = "",
		const std::string& transport = "",
		const std::string& path = "");
	virtual ~AddInstrumentDialog();

	virtual bool DoRender();

protected:
	SCPITransport* MakeTransport();

	virtual bool DoConnect(SCPITransport* transport);

	void UpdateCombos();

	void UpdatePath();
	
	//GUI widget values
	std::string m_nickname;
	std::string m_originalNickname;
	std::string m_defaultNickname;
	bool m_nicknameEdited;
	int m_selectedDriver;
	std::vector<std::string> m_drivers;
	int m_selectedTransport;
	SCPITransportType m_selectedTransportType;
	std::vector<std::string> m_transports;
	int m_selectedEndpoint;
	std::vector<TransportEndpoint> m_endpoints;
	std::vector<std::string> m_endpointNames;
	int m_selectedModel;
	std::vector<std::string> m_models;
	std::unordered_set<std::string> m_supportedTransports;
	std::string m_path;
	std::string m_defaultPath;
	bool m_pathEdited;
};

#endif
