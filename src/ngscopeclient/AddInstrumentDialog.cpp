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
	@brief Implementation of AddInstrumentDialog
 */

#include "ngscopeclient.h"
#include "AddInstrumentDialog.h"
#include "MainWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

AddInstrumentDialog::AddInstrumentDialog(
	const string& title,
	const string& nickname,
	Session* session,
	MainWindow* parent,
	const string& driverType,
	const std::string& driver,
	const std::string& transport,
	const std::string& path)
	: Dialog(
		title,
		string("AddInstrument") + to_string_hex(reinterpret_cast<uintptr_t>(this)),
		ImVec2(600, 200),
		session,
		parent)
	, m_nickname(nickname)
	, m_selectedDriver(0)
	, m_selectedTransport(0)
	, m_selectedTransportType(SCPITransportType::TRANSPORT_HID)
	, m_selectedEndpoint(0)
	, m_selectedModel(0)
	, m_path(path)
{
	SCPITransport::EnumTransports(m_transports);
	m_supportedTransports.insert(m_transports.begin(), m_transports.end());
	m_pathEdited = false;
	m_defaultNickname = nickname;
	m_originalNickname = nickname;
	m_nicknameEdited = false;

	m_drivers = session->GetDriverNamesForType(driverType);
	if(!driver.empty())
	{
		int i = 0;
		for(auto driverName: m_drivers)
		{
			if(driverName == driver)
			{
				m_selectedDriver = i;
				break;
			}
			i++;
		}
	}
	// Update combo now to have the right list of transports according to the selected driver
	UpdateCombos();

	if(!transport.empty())
	{
		int i = 0;
		for(auto transportName: m_transports)
		{
			if(transportName == transport)
			{
				m_selectedTransport = i;
				break;
			}
			i++;
		}
	}
	// Update again to setup path and nickenae
	UpdateCombos();
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
	//Get the tutorial wizard and see if we're on the "connect to scope" page
	auto tutorial = m_parent->GetTutorialWizard();
	if(tutorial && (tutorial->GetCurrentStep() != TutorialWizard::TUTORIAL_02_CONNECT) )
		tutorial = nullptr;

	if(ImGui::InputText("Nickname", &m_nickname))
		m_nicknameEdited = !(m_nickname.empty() || (m_nickname == m_defaultNickname));
	HelpMarker(
		"Text nickname for this instrument so you can distinguish between multiple similar devices.\n"
		"\n"
		"This is shown on the list of recent instruments, to disambiguate channel names in multi-instrument setups, etc.");

	bool dropdownOpen = false;
	if(Combo("Driver", m_drivers, m_selectedDriver,&dropdownOpen))
	{
		m_selectedModel = 0;
		m_selectedTransport = 0;
		UpdateCombos();
	}
	HelpMarker(
		"Select the instrument driver to use.\n"
		"\n"
		"Most commonly there is one driver supporting all hardware of a given type from a given vendor (e.g. Siglent oscilloscopes),"
		"however there may be multiple drivers to choose from if a given vendor has several product lines with very different "
		"software stacks.\n"
		"\n"
		"Check the user manual for details of what driver to use with a given instrument.");

	//Show speech bubble for tutorial
	bool showedBubble = false;
	if(tutorial && (m_drivers[m_selectedDriver] != "demo") && !dropdownOpen )
	{
		auto pos = ImGui::GetCursorScreenPos();
		ImVec2 anchorPos(pos.x + 10*ImGui::GetFontSize(), pos.y);
		tutorial->DrawSpeechBubble(anchorPos, ImGuiDir_Up, "Select the \"demo\" driver");
		showedBubble = true;
	}
	else if(dropdownOpen)	//suppress further bubbles if dropdown is active
		showedBubble = true;

	if(m_models.size() > 1)
	{	// Only show model combo if there is more than one model
		if(Combo("Model", m_models, m_selectedModel))
			UpdateCombos();
		HelpMarker(
			"Select the model of your instrument.\n"
			"\n"
			"The selected driver supports several models from the manufacturer,"
			"Selecting the model will adapt the instrument nickname and connection string.");
	}

	if(Combo("Transport", m_transports, m_selectedTransport, &dropdownOpen))
		UpdateCombos();

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

	//Show speech bubble for tutorial
	if(tutorial && (m_transports[m_selectedTransport] != "null") && !dropdownOpen && !showedBubble)
	{
		auto pos = ImGui::GetCursorScreenPos();
		ImVec2 anchorPos(pos.x + 10*ImGui::GetFontSize(), pos.y);
		tutorial->DrawSpeechBubble(anchorPos, ImGuiDir_Up, "Select the \"null\" transport");
		showedBubble = true;
	}
	else if(dropdownOpen)	//suppress further bubbles if dropdown is active
		showedBubble = true;

	if(!m_endpoints.empty())
	{	// Endpoint discovery available: create endpoint combo
		if(Combo("Endpoint", m_endpointNames, m_selectedEndpoint, &dropdownOpen))
		{
			UpdatePath();
		}
		HelpMarker("Select the transport endpoint from the list and/or edit the path manually.");
		ImGui::SameLine();
		if(ImGui::Button("⟳"))
		{
			UpdateCombos();
		}
	}
	if(ImGui::InputText("Path", &m_path))
		m_pathEdited = !(m_path.empty() || (m_path == m_defaultPath));
	HelpMarker(
		"Transport-specific description of how to connect to the instrument.\n",
			{
				"GPIB: board index and primary address (0:7)",
				"TCP/IP transports: IP or hostname : port (localhost:5025).\n"
				"Note that for twinlan, two port numbers are required (localhost:5025:5026) for SCPI and data ports respectively.",
				"UART: device path and baud rate (/dev/ttyUSB0:9600, COM1). Default is 115200 if not specified. ",
				"USBTMC: Linux device path (/dev/usbtmcX)",
				"USB-HID: Device vendor id, product id (and optionnaly serial number): <vendorId(hex)>:<productId(hex)>:<serialNumber> (e.g.: 2e3c:af01)"
			}
		);

	if(ImGui::Button("Add"))
	{
		if(m_nickname.empty())
		{
			ShowErrorPopup(
				"Nickname error",
				"The nickname cannot be left blank");
		}
		else
		{
			auto transport = MakeTransport();
			if(transport)
			{
				if(DoConnect(transport))
				{
					if(tutorial)
						tutorial->AdvanceToNextStep();

					return false;
				}
			}
		}
	}

	if(tutorial && !dropdownOpen && !showedBubble)
	{
		auto pos = ImGui::GetCursorScreenPos();
		ImVec2 anchorPos(pos.x + 2*ImGui::GetFontSize(), pos.y);
		tutorial->DrawSpeechBubble(anchorPos, ImGuiDir_Up, "Add the scope to your session");
		showedBubble = true;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers

/**
	@brief Create and return a new transport with the specified path
 */
SCPITransport* AddInstrumentDialog::MakeTransport()
{
	//Create the transport
	auto transport = SCPITransport::CreateTransport(m_transports[m_selectedTransport], m_path);
	if(transport == nullptr)
	{
		ShowErrorPopup(
			"Transport error",
			"Failed to create transport of type \"" + m_transports[m_selectedTransport] + "\"");
		return nullptr;
	}

	//Make sure we connected OK
	if(!transport->IsConnected())
	{
		delete transport;
		ShowErrorPopup("Connection error", "Failed to connect to \"" + m_path + "\"");
		return nullptr;
	}

	return transport;
}

bool AddInstrumentDialog::DoConnect(SCPITransport* transport)
{
	return m_session->CreateAndAddInstrument(m_drivers[m_selectedDriver], transport, m_nickname);
}

void AddInstrumentDialog::UpdatePath()
{
	if(m_selectedTransportType == SCPITransportType::TRANSPORT_HID)
	{	// Special handling for HID transport: replace the whole path with endpoint value
		m_path = m_endpoints[m_selectedEndpoint].path;
	}
	else
	{
		size_t pos = m_path.find(':');
		string suffix = (pos == std::string::npos) ? "" : m_path.substr(pos);
		m_path = m_endpoints[m_selectedEndpoint].path + suffix;
	}
}

void AddInstrumentDialog::UpdateCombos()
{
	// Update transoport list according to selected driver an connection string according to transport
	string driver = m_drivers[m_selectedDriver];
	auto supportedModels = SCPIInstrument::GetSupportedModels(driver);
	m_endpoints.clear();
	m_endpointNames.clear();
	if(!supportedModels.empty())
	{
		m_models.clear();
		m_transports.clear();
		int modelIndex = 0;
		auto selectedModel = supportedModels[0];
		// Model list
		for(auto model : supportedModels)
		{
			m_models.push_back(model.modelName);
			if(modelIndex == m_selectedModel)
			{
				selectedModel = model;
			}
			modelIndex++;
		}
		// Nick name
		if(!m_nicknameEdited)
		{
			m_nickname = selectedModel.modelName;
			m_defaultNickname = m_nickname;
		}

		// Transport list
		int transportIndex = 0;
		for(auto transport : selectedModel.supportedTransports)
		{
			string transportName = to_string(transport.transportType);
			// Prepare transport type for default value
			if(transportIndex == 0) m_selectedTransportType = transport.transportType;
			if(m_supportedTransports.find(transportName) != m_supportedTransports.end())
			{
				m_transports.push_back(transportName);
				if(transportIndex == m_selectedTransport)
				{
					m_selectedTransportType = transport.transportType;
					if(!m_pathEdited)
					{
						m_path = transport.connectionString;
						m_defaultPath = m_path;
					}
				}
				transportIndex++;
			}
		}
		if(m_selectedTransport >= (int)m_transports.size())
		{
			m_selectedTransport = 0;
			if(!m_pathEdited)
				m_path = "";
		}
		// Update endpoint list
		auto endpoints = SCPITransport::EnumEndpoints(m_transports[m_selectedTransport]);
		int endpointIndex = 0;
		for(auto endpoint : endpoints)
		{
			m_endpoints.push_back(endpoint);
			m_endpointNames.push_back(endpoint.path + " ("+ endpoint.description +")");
			if(m_selectedTransportType == SCPITransportType::TRANSPORT_HID && (endpoint.path.rfind(m_path) == 0))
			{	// Special handling for HID : select the endpoint matching the path provided by the driver
				m_selectedEndpoint = endpointIndex;
			}
			endpointIndex++;
		}
		if(m_selectedEndpoint >= (int)m_endpoints.size())
		{
			m_selectedEndpoint = 0;
		}
		if(m_endpoints.size()>0)
			UpdatePath();
	}
	else
	{	// Supported transports not provided => add all transports
		m_transports.clear();
		m_models.clear();
		if(!m_nicknameEdited)
			m_nickname = m_originalNickname;
		SCPITransport::EnumTransports(m_transports);
		if(!m_pathEdited)
			m_path = "";
	}
}
