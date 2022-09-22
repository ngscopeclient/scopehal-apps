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
	@brief Implementation of AddInstrumentDialog
 */

#include "ngscopeclient.h"
#include "AddInstrumentDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

AddInstrumentDialog::AddInstrumentDialog(const string& title, const std::string& nickname, Session& session)
	: Dialog(title, ImVec2(600, 150))
	, m_nickname(nickname)
	, m_session(session)
	, m_selectedDriver(0)
	, m_selectedTransport(0)
{
	SCPITransport::EnumTransports(m_transports);
}

AddInstrumentDialog::~AddInstrumentDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool AddInstrumentDialog::DoRender()
{
	ImGui::InputText("Nickname", &m_nickname);
	HelpMarker(
		"Text nickname for this instrument so you can distinguish between multiple similar devices.\n"
		"\n"
		"This is shown on the list of recent instruments, to disambiguate channel names in multi-instrument setups, etc.");

	Combo("Driver", m_drivers, m_selectedDriver);
	HelpMarker(
		"Select the instrument driver to use.\n"
		"\n"
		"Most commonly there is one driver supporting all hardware of a given type from a given vendor (e.g. Siglent oscilloscopes),"
		"however there may be multiple drivers to choose from if a given vendor has several product lines with very different "
		"software stacks.\n"
		"\n"
		"Check the user manual for details of what driver to use with a given instrument.");

	Combo("Transport", m_transports, m_selectedTransport);
	HelpMarker(
		"Select the SCPI transport for the connection between your computer and the instrument.\n"
		"\n"
		"This controls how remote control commands and waveform data get to/from the instrument (USB, Ethernet, GPIB, etc).\n"
		"\n"
		"Note that there are four different transports which run over TCP/IP, since instruments vary greatly:\n",
			{
				"lan: raw SCPI over TCP socket with no framing",
				"lxi: LXI VXI-11",
				"twinlan: separate sockets for SCPI text control commands and raw binary waveforms.\n"
				"Commonly used with bridge servers for interfacing to USB instruments (Digilent, DreamSourceLabs, Pico).",
				"vicp: Teledyne LeCroy Virtual Instrument Control Protocol"
			}
		);

	ImGui::InputText("Path", &m_path);
	HelpMarker(
		"Transport-specific description of how to connect to the instrument.\n",
			{
				"GPIB: board index and primary address (0:7)",
				"TCP/IP transports: IP or hostname : port (localhost:5025).\n"
				"Note that for twinlan, two port numbers are required (localhost:5025:5026) for SCPI and data ports respectively.",
				"UART: device path and baud rate (/dev/ttyUSB0:9600, COM1). Default id 115200 if not specified. ",
				"USBTMC: Linux device path (/dev/usbtmcX)"
			}
		);

	if(ImGui::Button("Add"))
	{
		if(m_nickname.empty())
		{
			ShowErrorPopup(
			"Nickname error",
			"Nickname shall be not empty");
		}
		else
		{
			if(DoConnect())
				return false;
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers
