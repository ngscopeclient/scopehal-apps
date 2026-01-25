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
	@brief Implementation of MainWindow
 */
#include "ngscopeclient.h"
#include "MainWindow.h"

#include "RemoteBridgeOscilloscope.h"

//Dock builder API is not yet public, so might change...
#include "imgui_internal.h"

//Dialogs
#include "AboutDialog.h"
#include "AddInstrumentDialog.h"
#include "BERTDialog.h"
#include "CreateFilterBrowser.h"
#include "FilterGraphEditor.h"
#include "HistoryDialog.h"
#include "LoadDialog.h"
#include "LogViewerDialog.h"
#include "MeasurementsDialog.h"
#include "MemoryLeakerDialog.h"
#include "MetricsDialog.h"
#include "NotesDialog.h"
#include "PersistenceSettingsDialog.h"
#include "PowerSupplyDialog.h"
#include "PreferenceDialog.h"
#include "ProtocolAnalyzerDialog.h"
#include "RFGeneratorDialog.h"
#include "SCPIConsoleDialog.h"
#include "Workspace.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Top level menu

void MainWindow::AddDialog(shared_ptr<Dialog> dlg)
{
	m_dialogs.emplace(dlg);

	auto pdlg = dynamic_cast<PowerSupplyDialog*>(dlg.get());
	if(pdlg != nullptr)
		m_psuDialogs[pdlg->GetPSU()] = dlg;

	auto bdlg = dynamic_cast<BERTDialog*>(dlg.get());
	if(bdlg != nullptr)
		m_bertDialogs[bdlg->GetBERT()] = dlg;

	auto rdlg = dynamic_cast<RFGeneratorDialog*>(dlg.get());
	if(rdlg != nullptr)
		m_rfgeneratorDialogs[rdlg->GetGenerator()] = dlg;

	auto ldlg = dynamic_cast<LoadDialog*>(dlg.get());
	if(ldlg != nullptr)
		m_loadDialogs[ldlg->GetLoad()] = dlg;
}

/**
	@brief Run the top level menu bar
 */
void MainWindow::MainMenu()
{
	if(ImGui::BeginMainMenuBar())
	{
		FileMenu();
		ViewMenu();
		AddMenu();
		SetupMenu();
		WindowMenu();
		DebugMenu();
		HelpMenu();
		ImGui::EndMainMenuBar();
	}
}

/**
	@brief Run the File menu
 */
void MainWindow::FileMenu()
{
	if(ImGui::BeginMenu("File"))
	{
		bool hasFileBrowser = (m_fileBrowser != nullptr);

		//Don't allow opening a second file browser if we already have one open
		if(hasFileBrowser)
			ImGui::BeginDisabled();
		if(ImGui::MenuItem("Open Online..."))
			OnOpenFile(true);
		if(ImGui::MenuItem("Open Offline..."))
			OnOpenFile(false);
		if(hasFileBrowser)
			ImGui::EndDisabled();

		FileRecentMenu();

		ImGui::Separator();

		bool alreadyHaveSession = (m_sessionFileName != "");
		if(!alreadyHaveSession)
			ImGui::BeginDisabled();
		if(ImGui::MenuItem("Save"))
			DoSaveFile(m_sessionFileName);
		if(!alreadyHaveSession)
			ImGui::EndDisabled();

		//Don't allow opening a second file browser if we already have one open
		if(hasFileBrowser)
			ImGui::BeginDisabled();
		if(ImGui::MenuItem("Save As..."))
			OnSaveAs();
		if(hasFileBrowser)
			ImGui::EndDisabled();

		ImGui::Separator();

		if(ImGui::MenuItem("Close"))
			QueueCloseSession();

		ImGui::Separator();

		if(ImGui::MenuItem("Exit"))
			glfwSetWindowShouldClose(m_window, 1);

		ImGui::EndMenu();
	}
}

/**
	@brief Runs the File | Recent menu
 */
void MainWindow::FileRecentMenu()
{
	if(ImGui::BeginMenu("Recent Files"))
	{
		//Make a reverse mapping
		std::map<time_t, vector<string> > reverseMap;
		for(auto it : m_recentFiles)
			reverseMap[it.second].push_back(it.first);

		//Deduplicate timestamps
		set<time_t> timestampsDeduplicated;
		for(auto it : m_recentFiles)
			timestampsDeduplicated.emplace(it.second);

		//Sort the list by most recent
		vector<time_t> timestamps;
		for(auto t : timestampsDeduplicated)
			timestamps.push_back(t);
		std::sort(timestamps.rbegin(), timestamps.rend());

		//Add new ones
		int nleft = m_session.GetPreferences().GetInt("Files.max_recent_files");
		for(auto t : timestamps)
		{
			auto paths = reverseMap[t];
			for(auto path : paths)
			{
				if(ImGui::BeginMenu(path.c_str()))
				{
					if(ImGui::MenuItem("Open Online"))
						DoOpenFile(path, true);

					if(ImGui::MenuItem("Open Offline"))
						DoOpenFile(path, false);

					ImGui::EndMenu();
				}
			}

			nleft --;
			if(nleft == 0)
				break;
		}

		ImGui::EndMenu();
	}
}

/**
	@brief Run the View menu
 */
void MainWindow::ViewMenu()
{
	if(ImGui::BeginMenu("View"))
	{
		if(ImGui::MenuItem("Fullscreen"))
			SetFullscreen(!m_fullscreen);

		ImGui::Separator();

		if(ImGui::MenuItem("Persistence Setup"))
		{
			m_persistenceDialog = make_shared<PersistenceSettingsDialog>(*this);
			AddDialog(m_persistenceDialog);
		}

		ImGui::EndMenu();
	}
}

/**
	@brief Run the Add menu
 */
void MainWindow::AddMenu()
{
	auto menuStartPos = ImGui::GetCursorScreenPos();

	if(ImGui::BeginMenu("Add"))
	{
		//Make a reverse mapping: timestamp -> instruments last used at that time
		map<time_t, vector<string> > reverseMap;
		for(auto it : m_recentInstruments)
			reverseMap[it.second].push_back(it.first);

		//Get a sorted list of timestamps, most recent first, with no duplicates
		set<time_t> timestampsDeduplicated;
		for(auto it : m_recentInstruments)
			timestampsDeduplicated.emplace(it.second);
		vector<time_t> timestamps;
		for(auto t : timestampsDeduplicated)
			timestamps.push_back(t);
		std::sort(timestamps.begin(), timestamps.end());

		DoAddSubMenu(timestamps, reverseMap, "BERT", "bert", "bert");
		DoAddSubMenu(timestamps, reverseMap, "Function Generator", "funcgen", "funcgen");
		DoAddSubMenu(timestamps, reverseMap, "Load", "load", "load");
		DoAddSubMenu(timestamps, reverseMap, "Misc", "inst", "misc");
		DoAddSubMenu(timestamps, reverseMap, "Multimeter", "meter", "multimeter");
		DoAddSubMenu(timestamps, reverseMap, "Oscilloscope", "scope", "oscilloscope");
		DoAddSubMenu(timestamps, reverseMap, "Power Supply", "psu", "psu");
		DoAddSubMenu(timestamps, reverseMap, "RF Generator", "rfgen", "rfgen");
		DoAddSubMenu(timestamps, reverseMap, "SDR", "sdr", "sdr");
		DoAddSubMenu(timestamps, reverseMap, "Spectrometer", "spec", "spectrometer");
		DoAddSubMenu(timestamps, reverseMap, "VNA", "vna", "vna");

		ImGui::Separator();

		AddChannelsMenu();
		AddGenerateMenu();
		AddImportMenu();

		ImGui::EndMenu();
	}

	//Add hint bubble here during the tutorial, but only if menu is not open
	//(we don't want to block the user's view of said menu)
	else if(m_tutorialDialog && (m_tutorialDialog->GetCurrentStep() == TutorialWizard::TUTORIAL_01_ADDINSTRUMENT) )
	{
		auto menuEndPos = ImGui::GetCursorScreenPos();

		ImVec2 anchorPos(
			(menuStartPos.x + menuEndPos.x)/2,
			menuStartPos.y + 2*ImGui::GetFontSize());

		m_tutorialDialog->DrawSpeechBubble(anchorPos, ImGuiDir_Up, "Add an oscilloscope to your session");
	}
}

/**
	@brief Run the Add | (instrument type) submenu
 */
void MainWindow::DoAddSubMenu(
	vector<time_t>& timestamps,
	map<time_t, vector<string> >& reverseMap,
	const string& typePretty,
	const string& defaultName,
	const string& typeInternal
	)
{
	if(ImGui::BeginMenu(typePretty.c_str()))
	{
		//Spawn the connect dialog
		if(ImGui::MenuItem("Connect..."))
		{
			m_dialogs.emplace(make_shared<AddInstrumentDialog>(
				string("Add ") + typePretty,
				defaultName,
				&m_session,
				this,
				typeInternal));

			//Move to the next step of the tutorial if needed
			if((typeInternal == "oscilloscope") &&
				m_tutorialDialog &&
				(m_tutorialDialog->GetCurrentStep() == TutorialWizard::TUTORIAL_01_ADDINSTRUMENT) )
			{
				m_tutorialDialog->AdvanceToNextStep();
			}
		}
		ImGui::Separator();

		//Find all known drivers for this instrument type
		auto drivers = m_session.GetDriverNamesForType(typeInternal);
		set<string> driverset;
		for(auto s : drivers)
			driverset.emplace(s);

		//Recent instruments
		for(int i=timestamps.size()-1; i>=0; i--)
		{
			auto t = timestamps[i];
			auto cstrings = reverseMap[t];
			for(auto cstring : cstrings)
			{
				auto fields = explode(cstring, ':');

				//make sure it's well formed
				if(fields.size() < 4)
				{
					//Special case: null transport allows 3 fields
					if( (fields.size() == 3) && (fields[2] == "null") )
					{}

					else
						continue;
				}

				auto nick = fields[0];
				auto drivername = fields[1];
				auto transname = fields[2];

				if(driverset.find(drivername) != driverset.end())
				{
					if(ImGui::MenuItem(nick.c_str()))
					{
						string path;
						if(fields.size() >= 4)
						{
							path = fields[3];
							for(size_t j=4; j<fields.size(); j++)
								path = path + ":" + fields[j];
						}

						bool success = true;
						auto transport = MakeTransport(transname, path);
						if(transport != nullptr)
						{
							if(!m_session.CreateAndAddInstrument(drivername, transport, nick))
							{
								success = false;
							}
						}
						else
						{
							success = false;
						}
						if(!success)
						{	// Spawn an AddInstrument dialog here, prefilled with intrument informations, to allow changing connection path
							m_dialogs.emplace(make_shared<AddInstrumentDialog>(
								string("Update ") + typePretty,
								nick,
								&m_session,
								this,
								typeInternal,
								drivername,
								transname,
								path));				
						}
					}
				}
			}
		}

		ImGui::EndMenu();
	}
}

/**
	@brief Run the Add | Channels menu
 */
void MainWindow::AddChannelsMenu()
{
	if(ImGui::BeginMenu("Channels"))
	{
		auto insts = m_session.GetInstruments();
		for(auto inst : insts)
		{
			if(ImGui::BeginMenu(inst->m_nickname.c_str()))
			{
				for(size_t i=0; i<inst->GetChannelCount(); i++)
				{
					auto chan = dynamic_cast<OscilloscopeChannel*>(inst->GetChannel(i));
					if(!chan)
						continue;
					auto scope = dynamic_pointer_cast<Oscilloscope>(inst);
					for(size_t j=0; j<chan->GetStreamCount(); j++)
					{
						//skip trigger channels, those can't be displayed
						if(chan->GetType(j) == Stream::STREAM_TYPE_TRIGGER)
							continue;

						//Skip channels we can't enable
						if(scope && !scope->CanEnableChannel(i))
							continue;

						StreamDescriptor stream(chan, j);
						if(ImGui::MenuItem(stream.GetName().c_str()))
							FindAreaForStream(nullptr, stream);
					}
				}

				ImGui::EndMenu();
			}
		}

		auto filters = Filter::GetAllInstances();
		for(auto f : filters)
		{
			for(size_t j=0; j<f->GetStreamCount(); j++)
			{
				StreamDescriptor stream(f, j);
				if(ImGui::MenuItem(stream.GetName().c_str()))
					FindAreaForStream(nullptr, stream);
			}
		}

		ImGui::EndMenu();
	}
}

/**
	@brief Run the Add | Import menu
 */
void MainWindow::AddImportMenu()
{
	auto& refs = m_session.GetReferenceFilters();

	if(ImGui::BeginMenu("Import"))
	{
		//Find all filters in this category and sort them alphabetically
		vector<string> sortedNames;
		for(auto it : refs)
		{
			if(it.second->GetCategory() == Filter::CAT_GENERATION)
				sortedNames.push_back(it.first);
		}
		std::sort(sortedNames.begin(), sortedNames.end());

		//Do all of the menu items
		for(auto fname : sortedNames)
		{
			//Hide everything but import filters
			if(fname.find("Import") == string::npos)
				continue;

			string shortname = fname.substr(0, fname.size() - strlen(" Import"));

			if(ImGui::MenuItem(shortname.c_str()))
				CreateFilter(fname, nullptr, StreamDescriptor(nullptr, 0));
		}

		ImGui::EndMenu();
	}
}

/**
	@brief Run the Add | Generate menu
 */
void MainWindow::AddGenerateMenu()
{
	auto& refs = m_session.GetReferenceFilters();

	if(ImGui::BeginMenu("Generate"))
	{
		//Find all filters in this category and sort them alphabetically
		vector<string> sortedNames;
		for(auto it : refs)
		{
			if(it.second->GetCategory() == Filter::CAT_GENERATION)
				sortedNames.push_back(it.first);
		}
		std::sort(sortedNames.begin(), sortedNames.end());

		//Do all of the menu items
		for(auto fname : sortedNames)
		{
			//Hide import filters
			if(fname.find("Import") != string::npos)
				continue;

			//Hide filters that have inputs
			if(refs.find(fname)->second->GetInputCount() != 0)
				continue;

			if(ImGui::MenuItem(fname.c_str()))
				CreateFilter(fname, nullptr, StreamDescriptor(nullptr, 0));
		}

		ImGui::EndMenu();
	}
}

/**
	@brief Run the Setup menu
 */
void MainWindow::SetupMenu()
{
	if(ImGui::BeginMenu("Setup"))
	{
		bool manageVisible = (m_manageInstrumentsDialog != nullptr);
		if(manageVisible)
			ImGui::BeginDisabled();
		if(ImGui::MenuItem("Manage Instruments..."))
			ShowManageInstruments();
		if(manageVisible)
			ImGui::EndDisabled();

		bool triggerVisible = (m_triggerDialog != nullptr);
		if(triggerVisible)
			ImGui::BeginDisabled();
		if(ImGui::MenuItem("Trigger..."))
			ShowTriggerProperties();
		if(triggerVisible)
			ImGui::EndDisabled();

		ImGui::Separator();

		bool prefsVisible = (m_preferenceDialog != nullptr);
		if(prefsVisible)
			ImGui::BeginDisabled();
		if(ImGui::MenuItem("Preferences..."))
		{
			m_preferenceDialog = make_shared<PreferenceDialog>(m_session.GetPreferences());
			AddDialog(m_preferenceDialog);
		}
		if(prefsVisible)
			ImGui::EndDisabled();

		ImGui::EndMenu();
	}
}

/**
	@brief Run the Window menu
 */
void MainWindow::WindowMenu()
{
	if(ImGui::BeginMenu("Window"))
	{
		WindowAnalyzerMenu();
		WindowPSUMenu();

		bool hasLabNotes = m_notesDialog != nullptr;
		if(hasLabNotes)
			ImGui::BeginDisabled();

		if(ImGui::MenuItem("Lab Notes"))
		{
			m_notesDialog = make_shared<NotesDialog>(this);
			AddDialog(m_notesDialog);
		}

		if(hasLabNotes)
			ImGui::EndDisabled();

		bool hasLogViewer = m_logViewerDialog != nullptr;
		if(hasLogViewer)
			ImGui::BeginDisabled();

		if(ImGui::MenuItem("Log Viewer"))
		{
			m_logViewerDialog = make_shared<LogViewerDialog>(this);
			AddDialog(m_logViewerDialog);
		}

		if(hasLogViewer)
			ImGui::EndDisabled();

		bool hasMeasurements = m_measurementsDialog != nullptr;
		if(hasMeasurements)
			ImGui::BeginDisabled();

		if(ImGui::MenuItem("Measurements"))
		{
			m_measurementsDialog = make_shared<MeasurementsDialog>(m_session);
			AddDialog(m_measurementsDialog);
		}

		if(hasMeasurements)
			ImGui::EndDisabled();

		bool hasMetrics = m_metricsDialog != nullptr;
		if(hasMetrics)
			ImGui::BeginDisabled();

		if(ImGui::MenuItem("Performance Metrics"))
		{
			m_metricsDialog = make_shared<MetricsDialog>(&m_session);
			AddDialog(m_metricsDialog);
		}

		if(hasMetrics)
			ImGui::EndDisabled();

		bool hasHistory = m_historyDialog != nullptr;
		if(hasHistory)
			ImGui::BeginDisabled();
		if(ImGui::MenuItem("History"))
		{
			m_historyDialog = make_shared<HistoryDialog>(m_session.GetHistory(), m_session, *this);
			AddDialog(m_historyDialog);
		}
		if(hasHistory)
			ImGui::EndDisabled();

		bool hasGraphEditor = m_graphEditor != nullptr;
		if(hasGraphEditor)
			ImGui::BeginDisabled();
		if(ImGui::MenuItem("Filter Graph"))
		{
			m_graphEditor = make_shared<FilterGraphEditor>(m_session, this);
			AddDialog(m_graphEditor);
		}
		if(hasGraphEditor)
			ImGui::EndDisabled();

		bool hasStreamBrowser = m_streamBrowser != nullptr;
		if(hasStreamBrowser)
			ImGui::BeginDisabled();
		if(ImGui::MenuItem("Stream Browser"))
		{
			m_streamBrowser = make_shared<StreamBrowserDialog>(m_session, this);
			AddDialog(m_streamBrowser);
		}
		if(hasStreamBrowser)
			ImGui::EndDisabled();

		bool hasFilterPalette = m_filterPalette != nullptr;
		if(hasFilterPalette)
			ImGui::BeginDisabled();
		if(ImGui::MenuItem("Filter Palette"))
		{
			m_filterPalette = make_shared<CreateFilterBrowser>(m_session, this);
			AddDialog(m_filterPalette);
		}
		if(hasFilterPalette)
			ImGui::EndDisabled();

		if(ImGui::MenuItem("New Workspace"))
			m_workspaces.emplace(make_shared<Workspace>(m_session, this));

		ImGui::EndMenu();
	}
}

/**
	@brief Run the Window | Analyzer menu

	This menu is used for displaying protocol analyzers
 */
void MainWindow::WindowAnalyzerMenu()
{
	//Find all protocol analyzer filters
	set<PacketDecoder*> decoders;
	auto instances = Filter::GetAllInstances();
	for(auto f : instances)
	{
		auto pd = dynamic_cast<PacketDecoder*>(f);
		if(pd)
			decoders.emplace(pd);
	}

	if(decoders.empty())
		ImGui::BeginDisabled();
	if(ImGui::BeginMenu("Analyzer"))
	{
		//Make a list of all filters
		for(auto pd : decoders)
		{
			//Do we already have a dialog open for it? If so, we don't want to open another
			bool open = (m_protocolAnalyzerDialogs.find(pd) != m_protocolAnalyzerDialogs.end());

			//Add it to the menu
			if(open)
				ImGui::BeginDisabled();
			if(ImGui::MenuItem(pd->GetDisplayName().c_str()))
			{
				auto dlg = make_shared<ProtocolAnalyzerDialog>(pd, m_session.GetPacketManager(pd), &m_session, this);
				m_protocolAnalyzerDialogs[pd] = dlg;
				AddDialog(dlg);
			}
			if(open)
				ImGui::EndDisabled();
		}

		ImGui::EndMenu();
	}

	if(decoders.empty())
		ImGui::EndDisabled();
}

/**
	@brief Run the Window | Power Supply menu

	This menu is used for controlling a power supply that is already open in the session but has had the dialog closed.
 */
void MainWindow::WindowPSUMenu()
{
	//Make a list of PSUs
	vector< shared_ptr<SCPIPowerSupply> > psus;
	auto insts = m_session.GetSCPIInstruments();
	for(auto inst : insts)
	{
		//Skip anything that's not a PSU
		if( (inst->GetInstrumentTypes() & Instrument::INST_PSU) == 0)
			continue;

		//Do we already have a dialog open for it? If so, don't make another
		auto psu = dynamic_pointer_cast<SCPIPowerSupply>(inst);
		if(m_psuDialogs.find(psu) != m_psuDialogs.end())
			continue;

		psus.push_back(psu);
	}

	ImGui::BeginDisabled(psus.empty());
	if(ImGui::BeginMenu("Power Supply"))
	{
		for(auto psu : psus)
		{
			//Add it to the menu
			if(ImGui::MenuItem(psu->m_nickname.c_str()))
				AddDialog(make_shared<PowerSupplyDialog>(psu, m_session.GetPSUState(psu), &m_session));
		}

		ImGui::EndMenu();
	}
	ImGui::EndDisabled();
}

/**
	@brief Runs the Debug | SCPI Console menu
 */
void MainWindow::DebugSCPIConsoleMenu()
{
	vector<shared_ptr<SCPIInstrument> > targets;
	auto insts = m_session.GetSCPIInstruments();
	for(auto inst : insts)
	{
		//If we already have a dialog, don't show the menu
		if(m_scpiConsoleDialogs.find(inst) != m_scpiConsoleDialogs.end())
			continue;
		targets.push_back(inst);
	}

	ImGui::BeginDisabled(targets.empty());

	if(ImGui::BeginMenu("SCPI Console"))
	{
		for(auto inst : targets)
		{
			if(ImGui::MenuItem(inst->m_nickname.c_str()))
			{
				auto dlg = make_shared<SCPIConsoleDialog>(this, inst);
				m_scpiConsoleDialogs[inst] = dlg;
				AddDialog(dlg);
			}
		}

		ImGui::EndMenu();
	}

	ImGui::EndDisabled();
}

/**
	@brief Run the Debug menu
 */
void MainWindow::DebugMenu()
{
	if(ImGui::BeginMenu("Debug"))
	{
		DebugSCPIConsoleMenu();

		bool showDemo = m_showDemo;
		if(showDemo)
			ImGui::BeginDisabled();
		if(ImGui::MenuItem("ImGui Demo"))
			m_showDemo = true;
		if(showDemo)
			ImGui::EndDisabled();

		if(ImGui::MenuItem("Memory Leaker"))
			AddDialog(make_shared<MemoryLeakerDialog>(this));

		ImGui::EndMenu();
	}
}

/**
	@brief Run the Help menu
 */
void MainWindow::HelpMenu()
{
	if(ImGui::BeginMenu("Help"))
	{
		ImGui::BeginDisabled(m_tutorialDialog != nullptr);
			if(ImGui::MenuItem("Tutorial..."))
			{
				m_tutorialDialog = make_shared<TutorialWizard>(&m_session, this);
				AddDialog(m_tutorialDialog);
			}
		ImGui::EndDisabled();

		ImGui::Separator();

		if(ImGui::MenuItem("About..."))
			AddDialog(make_shared<AboutDialog>(this));

		ImGui::EndMenu();
	}
}
