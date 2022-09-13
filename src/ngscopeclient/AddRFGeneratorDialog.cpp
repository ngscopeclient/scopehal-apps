/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg                                                                          *
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
	@brief Implementation of AddRFGeneratorDialog
 */

#include "ngscopeclient.h"
#include "AddRFGeneratorDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

AddRFGeneratorDialog::AddRFGeneratorDialog(Session& session)
	: AddInstrumentDialog("Add RF Generator", session)
{
	SCPIRFSignalGenerator::EnumDrivers(m_drivers);
}

AddRFGeneratorDialog::~AddRFGeneratorDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers

/**
	@brief Connects to a scope

	@return True if successful
 */
bool AddRFGeneratorDialog::DoConnect()
{
	//Create the transport
	auto transport = SCPITransport::CreateTransport(m_transports[m_selectedTransport], m_path);
	if(transport == nullptr)
	{
		ShowErrorPopup(
			"Transport error",
			"Failed to create transport of type \"" + m_transports[m_selectedTransport] + "\"");
		return false;
	}

	//Make sure we connected OK
	if(!transport->IsConnected())
	{
		delete transport;
		ShowErrorPopup("Connection error", "Failed to connect to \"" + m_path + "\"");
		return false;
	}

	//Create the RF Generator
	auto gen = SCPIRFSignalGenerator::CreateRFSignalGenerator(m_drivers[m_selectedDriver], transport);
	if(gen == nullptr)
	{
		ShowErrorPopup(
			"Driver error",
			"Failed to create RF cenerator driver of type \"" + m_drivers[m_selectedDriver] + "\"");
		delete transport;
		return false;
	}

	//TODO: apply preferences
	LogDebug("FIXME: apply PreferenceManager settings to newly created RF generator\n");

	gen->m_nickname = m_nickname;
	m_session.AddRFGenerator(gen);
	return true;
}
