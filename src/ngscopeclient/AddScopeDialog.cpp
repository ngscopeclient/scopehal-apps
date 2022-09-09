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
	@brief Implementation of AddScopeDialog
 */

#include "ngscopeclient.h"
#include "AddScopeDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

AddScopeDialog::AddScopeDialog(Session& session)
	: Dialog("Add Oscilloscope", ImVec2(400, 150))
	, m_session(session)
	, m_selectedDriver(0)
	, m_selectedTransport(0)
{
	Oscilloscope::EnumDrivers(m_drivers);
	SCPITransport::EnumTransports(m_transports);
}

AddScopeDialog::~AddScopeDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool AddScopeDialog::DoRender()
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
		if(DoConnect())
			return false;
	}

	RenderErrorPopup();

	return true;
}

/**
	@brief Popup message when we fail to connect
 */
void AddScopeDialog::RenderErrorPopup()
{
	if(ImGui::BeginPopupModal("Connection error", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text(m_errorPopupMessage.c_str());
		ImGui::Separator();
		if(ImGui::Button("OK"))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers

/**
	@brief Connects to a scope

	@return True if successful
 */
bool AddScopeDialog::DoConnect()
{
	//Create the transport
	auto transport = SCPITransport::CreateTransport(m_transports[m_selectedTransport], m_path);
	if(transport == nullptr)
	{
		ShowErrorPopup("Failed to create transport of type \"" + m_transports[m_selectedTransport] + "\"");
		return false;
	}

	//Make sure we connected OK
	if(!transport->IsConnected())
	{
		delete transport;
		ShowErrorPopup("Failed to connect to \"" + m_path + "\"");
		return false;
	}

	//Create the scope
	auto scope = Oscilloscope::CreateOscilloscope(m_drivers[m_selectedDriver], transport);
	if(scope == nullptr)
	{
		ShowErrorPopup("Failed to instantiate oscilloscope driver of type \"" + m_drivers[m_selectedDriver] + "\"");
		delete transport;
		return false;
	}

	//TODO: apply preferences
	LogDebug("FIXME: apply PreferenceManager settings to newly created scope\n");

	scope->m_nickname = m_nickname;
	m_session.AddOscilloscope(scope);

	return true;
}

/**
	@brief Opens the error popup
 */
void AddScopeDialog::ShowErrorPopup(const string& msg)
{
	ImGui::OpenPopup("Connection error");
	m_errorPopupMessage = msg;
}
