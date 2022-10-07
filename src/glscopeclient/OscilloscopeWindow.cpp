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
	@brief Implementation of main application window class
 */

#include "glscopeclient.h"
#include "glscopeclient-version.h"
#include "../scopehal/Instrument.h"
#include "../scopehal/MockOscilloscope.h"
#include "OscilloscopeWindow.h"
#include "PreferenceDialog.h"
#include "InstrumentConnectionDialog.h"
#include "MultimeterConnectionDialog.h"
#include "TriggerPropertiesDialog.h"
#include "TimebasePropertiesDialog.h"
#include "FileProgressDialog.h"
#include "MultimeterDialog.h"
#include "ScopeInfoWindow.h"
#include "FunctionGeneratorDialog.h"
#include "SCPIConsoleDialog.h"
#include "FileSystem.h"
#include <unistd.h>
#include <fcntl.h>
#include "../../lib/scopeprotocols/EyePattern.h"
#include "../../lib/scopeprotocols/SpectrogramFilter.h"
#include "../scopehal/LeCroyOscilloscope.h"

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#else
#include <sys/mman.h>
#endif

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

/**
	@brief Initializes the main window
 */
OscilloscopeWindow::OscilloscopeWindow(const vector<Oscilloscope*>& scopes)
	: m_exportWizard(nullptr)
	, m_scopes(scopes)
	, m_fullscreen(false)
	, m_multiScopeFreeRun(false)
	, m_scopeSyncWizard(NULL)
	, m_syncComplete(false)
	, m_graphEditor(NULL)
	, m_haltConditionsDialog(this)
	, m_timebasePropertiesDialog(NULL)
	, m_addFilterDialog(NULL)
	, m_pendingGenerator(NULL)
	, m_triggerArmed(false)
	, m_triggerOneShot(false)
	, m_shuttingDown(false)
	, m_loadInProgress(false)
	, m_waveformProcessingThread(WaveformProcessingThread, this)
	, m_cursorX(0)
	, m_cursorY(0)
	, m_nextMarker(1)
	, m_vkQueue()
{
	SetTitle();
	FindScopeFuncGens();

	m_vkQueue = std::make_unique<vk::raii::Queue>(*g_vkComputeDevice, g_computeQueueType, AllocateVulkanComputeQueue());

	//Initial setup
	set_reallocate_redraws(true);
	set_default_size(1280, 800);

	//Load list of recently opened stuff
	LoadRecentFileList();
	LoadRecentInstrumentList();

	//Add widgets
	CreateWidgets();

	//Update recently used instrument list
	AddCurrentToRecentInstrumentList();
	SaveRecentInstrumentList();
	RefreshInstrumentMenus();

	ArmTrigger(TRIGGER_TYPE_NORMAL);
	m_toggleInProgress = false;

	m_tLastFlush = GetTime();

	m_totalWaveforms = 0;

	//Start a timer for polling for scope updates
	//TODO: can we use signals of some sort to avoid busy polling until a trigger event?
	Glib::signal_timeout().connect(sigc::bind(sigc::mem_fun(*this, &OscilloscopeWindow::OnTimer), 1), 5);

	add_events(Gdk::POINTER_MOTION_MASK);
}

void OscilloscopeWindow::SetTitle()
{
	//Collect all of the instruments in the current session
	set<Instrument*> instruments;
	for(auto s : m_scopes)
		instruments.emplace(s);
	for(auto m : m_meters)
		instruments.emplace(m);

	//Main string formatting
	string title;
	bool redact = GetPreferences().GetBool("Privacy.redact_serial_in_title");
	for(auto inst : instruments)
	{
		//Redact serial number upon request
		string serial = inst->GetSerial();
		if(redact)
		{
			for(int j=serial.length()-3; j >= 0; j--)
				serial[j] = '*';
		}

		char tt[256];
		snprintf(tt, sizeof(tt), "%s (%s %s, serial %s)",
			inst->m_nickname.c_str(),
			inst->GetVendor().c_str(),
			inst->GetName().c_str(),
			serial.c_str()
			);

		if(title != "")
			title += ", ";
		title += tt;

		if(dynamic_cast<MockOscilloscope*>(inst) != NULL)
			title += "[OFFLINE]";
	}
	if(title.empty())
		title += "[OFFLINE]";

	#ifdef _DEBUG
		title += " [DEBUG BUILD]";
	#endif

	set_title(string("glscopeclient: ") + title);
}

/**
	@brief Application cleanup
 */
OscilloscopeWindow::~OscilloscopeWindow()
{
	//Terminate the waveform processing thread
	g_waveformProcessedEvent.Signal();
	m_waveformProcessingThread.join();

	m_vkQueue = nullptr;
}

/**
	@brief Helper function for creating widgets and setting up signal handlers
 */
void OscilloscopeWindow::CreateWidgets()
{
	//Initialize filter colors from preferences
	SyncFilterColors();

	//Initialize color ramps
	m_eyeColor = "KRain";
	m_eyeFiles["CRT"] = FindDataFile("gradients/eye-gradient-crt.rgba");
	m_eyeFiles["Ironbow"] = FindDataFile("gradients/eye-gradient-ironbow.rgba");
	m_eyeFiles["Rainbow"] = FindDataFile("gradients/eye-gradient-rainbow.rgba");
	m_eyeFiles["Reverse Rainbow"] = FindDataFile("gradients/eye-gradient-reverse-rainbow.rgba");
	m_eyeFiles["Viridis"] = FindDataFile("gradients/eye-gradient-viridis.rgba");
	m_eyeFiles["Grayscale"] = FindDataFile("gradients/eye-gradient-grayscale.rgba");
	m_eyeFiles["KRain"] = FindDataFile("gradients/eye-gradient-krain.rgba");

	//Set up window hierarchy
	add(m_vbox);
		m_vbox.pack_start(m_menu, Gtk::PACK_SHRINK);
			m_menu.append(m_fileMenuItem);
				m_fileMenuItem.set_label("File");
				m_fileMenuItem.set_submenu(m_fileMenu);

					auto item = Gtk::manage(new Gtk::MenuItem("Open Online...", false));
					item->signal_activate().connect(
						sigc::bind<bool>(sigc::mem_fun(*this, &OscilloscopeWindow::OnFileOpen), true));
					m_fileMenu.append(*item);
					item = Gtk::manage(new Gtk::MenuItem("Open Offline...", false));
					item->signal_activate().connect(
						sigc::bind<bool>(sigc::mem_fun(*this, &OscilloscopeWindow::OnFileOpen), false));
					m_fileMenu.append(*item);

					m_fileMenu.append(m_fileRecentMenuItem);
						m_fileRecentMenuItem.set_label("Recent Files");
						m_fileRecentMenuItem.set_submenu(m_fileRecentMenu);

					item = Gtk::manage(new Gtk::SeparatorMenuItem);
					m_fileMenu.append(*item);

					item = Gtk::manage(new Gtk::MenuItem("Save", false));
					item->signal_activate().connect(
						sigc::bind<bool>(sigc::mem_fun(*this, &OscilloscopeWindow::OnFileSave), true));
					m_fileMenu.append(*item);
					item = Gtk::manage(new Gtk::MenuItem("Save As...", false));
					item->signal_activate().connect(
						sigc::bind<bool>(sigc::mem_fun(*this, &OscilloscopeWindow::OnFileSave), false));
					m_fileMenu.append(*item);

					item = Gtk::manage(new Gtk::SeparatorMenuItem);
					m_fileMenu.append(*item);

					m_exportMenuItem.set_label("Export");
					m_exportMenuItem.set_submenu(m_exportMenu);
					m_fileMenu.append(m_exportMenuItem);

					item = Gtk::manage(new Gtk::SeparatorMenuItem);
					m_fileMenu.append(*item);

					item = Gtk::manage(new Gtk::MenuItem("Close", false));
					item->signal_activate().connect(
						sigc::mem_fun(*this, &OscilloscopeWindow::CloseSession));
					m_fileMenu.append(*item);

					item = Gtk::manage(new Gtk::SeparatorMenuItem);
					m_fileMenu.append(*item);

					item = Gtk::manage(new Gtk::MenuItem("Quit", false));
					item->signal_activate().connect(
						sigc::mem_fun(*this, &OscilloscopeWindow::OnQuit));
					m_fileMenu.append(*item);
			m_menu.append(m_setupMenuItem);
				m_setupMenuItem.set_label("Setup");
				m_setupMenuItem.set_submenu(m_setupMenu);
				m_setupMenu.append(m_setupSyncMenuItem);
					m_setupSyncMenuItem.set_label("Instrument Sync...");
					m_setupSyncMenuItem.signal_activate().connect(
						sigc::mem_fun(*this, &OscilloscopeWindow::OnScopeSync));
				m_setupMenu.append(m_setupTriggerMenuItem);
					m_setupTriggerMenuItem.set_label("Trigger");
					m_setupTriggerMenuItem.set_submenu(m_setupTriggerMenu);
				m_setupMenu.append(m_setupHaltMenuItem);
					m_setupHaltMenuItem.set_label("Halt Conditions...");
					m_setupHaltMenuItem.signal_activate().connect(
						sigc::mem_fun(*this, &OscilloscopeWindow::OnHaltConditions));
				m_setupMenu.append(m_preferencesMenuItem);
					m_preferencesMenuItem.set_label("Preferences");
					m_preferencesMenuItem.signal_activate().connect(
						sigc::mem_fun(*this, &OscilloscopeWindow::OnPreferences));
			m_menu.append(m_viewMenuItem);
				m_viewMenuItem.set_label("View");
				m_viewMenuItem.set_submenu(m_viewMenu);
					m_viewMenu.append(m_viewEyeColorMenuItem);
					m_viewEyeColorMenuItem.set_label("Color ramp");
					m_viewEyeColorMenuItem.set_submenu(m_viewEyeColorMenu);
						auto names = GetEyeColorNames();
						for(auto n : names)
						{
							auto eitem = Gtk::manage(new Gtk::RadioMenuItem);
							m_viewEyeColorMenu.append(*eitem);
							eitem->set_label(n);
							eitem->set_group(m_eyeColorGroup);
							eitem->signal_activate().connect(sigc::bind<std::string, Gtk::RadioMenuItem*>(
								sigc::mem_fun(*this, &OscilloscopeWindow::OnEyeColorChanged), n, eitem));
						}
						m_viewEyeColorMenu.show_all();
			m_menu.append(m_addMenuItem);
				m_addMenuItem.set_label("Add");
				m_addMenuItem.set_submenu(m_addMenu);
					m_addMenu.append(m_channelsMenuItem);
						m_channelsMenuItem.set_label("Channels");
						m_channelsMenuItem.set_submenu(m_channelsMenu);
					m_addMenu.append(m_generateMenuItem);
						m_generateMenuItem.set_label("Generate");
						m_generateMenuItem.set_submenu(m_generateMenu);
					m_addMenu.append(m_importMenuItem);
						m_importMenuItem.set_label("Import");
						m_importMenuItem.set_submenu(m_importMenu);
					RefreshGenerateAndImportMenu();
					item = Gtk::manage(new Gtk::SeparatorMenuItem);
					m_addMenu.append(*item);
					m_addMenu.append(m_addMultimeterMenuItem);
						m_addMultimeterMenuItem.set_label("Multimeter");
						m_addMultimeterMenuItem.set_submenu(m_addMultimeterMenu);
					m_addMenu.append(m_addScopeMenuItem);
						m_addScopeMenuItem.set_label("Oscilloscope");
						m_addScopeMenuItem.set_submenu(m_addScopeMenu);
			m_menu.append(m_windowMenuItem);
				m_windowMenuItem.set_label("Window");
				m_windowMenuItem.set_submenu(m_windowMenu);
					m_windowMenu.append(m_windowFilterGraphItem);
						m_windowFilterGraphItem.set_label("Filter Graph");
						m_windowFilterGraphItem.signal_activate().connect(
							sigc::mem_fun(*this, &OscilloscopeWindow::OnFilterGraph));
					m_windowMenu.append(m_windowAnalyzerMenuItem);
						m_windowAnalyzerMenuItem.set_label("Analyzer");
						m_windowAnalyzerMenuItem.set_submenu(m_windowAnalyzerMenu);
					m_windowMenu.append(m_windowGeneratorMenuItem);
						m_windowGeneratorMenuItem.set_label("Generator");
						m_windowGeneratorMenuItem.set_submenu(m_windowGeneratorMenu);
					m_windowMenu.append(m_windowMultimeterMenuItem);
						m_windowMultimeterMenuItem.set_label("Multimeter");
						m_windowMultimeterMenuItem.set_submenu(m_windowMultimeterMenu);

					item = Gtk::manage(new Gtk::SeparatorMenuItem);
					m_windowMenu.append(*item);

					m_windowMenu.append(m_windowScopeInfoMenuItem);
						m_windowScopeInfoMenuItem.set_label("Scope Info");
						m_windowScopeInfoMenuItem.set_submenu(m_windowScopeInfoMenu);

					m_windowMenu.append(m_windowScpiConsoleMenuItem);
						m_windowScpiConsoleMenuItem.set_label("SCPI Console");
						m_windowScpiConsoleMenuItem.set_submenu(m_windowScpiConsoleMenu);

			m_menu.append(m_helpMenuItem);
				m_helpMenuItem.set_label("Help");
				m_helpMenuItem.set_submenu(m_helpMenu);
					m_helpMenu.append(m_aboutMenuItem);
					m_aboutMenuItem.set_label("About...");
					m_aboutMenuItem.signal_activate().connect(
						sigc::mem_fun(*this, &OscilloscopeWindow::OnAboutDialog));

		m_vbox.pack_start(m_toolbox, Gtk::PACK_SHRINK);
			m_vbox.get_style_context()->add_class("toolbar");
			m_toolbox.pack_start(m_toolbar, Gtk::PACK_EXPAND_WIDGET);
				PopulateToolbar();
			m_toolbox.pack_start(m_alphalabel, Gtk::PACK_SHRINK);
				m_alphalabel.set_label("Opacity ");
				m_alphalabel.get_style_context()->add_class("toolbar");
			m_toolbox.pack_start(m_alphaslider, Gtk::PACK_SHRINK);
				m_alphaslider.set_size_request(200, 10);
				m_alphaslider.set_round_digits(3);
				m_alphaslider.set_draw_value(false);
				m_alphaslider.set_range(0, 0.75);
				m_alphaslider.set_increments(0.01, 0.01);
				m_alphaslider.set_margin_left(10);
				m_alphaslider.set_value(0.5);
				m_alphaslider.signal_value_changed().connect(
					sigc::mem_fun(*this, &OscilloscopeWindow::OnAlphaChanged));
				m_alphaslider.get_style_context()->add_class("toolbar");

		auto split = new Gtk::VPaned;
			m_vbox.pack_start(*split);
			m_splitters.emplace(split);

		m_vbox.pack_start(m_statusbar, Gtk::PACK_SHRINK);
			m_statusbar.get_style_context()->add_class("status");
			m_statusbar.pack_end(m_triggerConfigLabel, Gtk::PACK_SHRINK);
			m_triggerConfigLabel.set_size_request(75, 1);
			m_statusbar.pack_end(m_waveformRateLabel, Gtk::PACK_SHRINK);
			m_waveformRateLabel.set_size_request(175, 1);

	//Reconfigure menus
	RefreshChannelsMenu();
	RefreshMultimeterMenu();
	RefreshScopeInfoMenu();
	RefreshTriggerMenu();
	RefreshExportMenu();
	RefreshGeneratorsMenu();
	RefreshScpiConsoleMenu();
	RefreshInstrumentMenus();
	RefreshRecentFileMenu();

	//History isn't shown by default
	for(auto it : m_historyWindows)
		it.second->hide();

	//Create the waveform areas for all enabled channels
	CreateDefaultWaveformAreas(split);

	//Don't show measurements or wizards by default
	m_haltConditionsDialog.hide();

	//Initialize the style sheets
	m_css = Gtk::CssProvider::create();
	m_css->load_from_path(FindDataFile("styles/glscopeclient.css"));
	get_style_context()->add_provider_for_screen(
		Gdk::Screen::get_default(), m_css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

/**
	@brief Populates the toolbar
 */
void OscilloscopeWindow::PopulateToolbar()
{
	//Remove all existing toolbar items
	auto children = m_toolbar.get_children();
	for(auto c : children)
		m_toolbar.remove(*c);

	int size = m_preferences.GetEnum<int>("Appearance.Toolbar.icon_size");

	//FindDataFile() assumes a file name, not a directory. Need to search for a specific file.
	//Then assume all other data files are in the same directory.
	//TODO: might be better to FindDataFile each one separately so we can override?
	string testfname = "fullscreen-enter.png";
	string base_path = FindDataFile("icons/" + to_string(size) + "x" + to_string(size) + "/" + testfname);
	base_path = base_path.substr(0, base_path.length() - testfname.length());

	m_iconEnterFullscreen = Gtk::Image(base_path + "fullscreen-enter.png");
	m_iconExitFullscreen = Gtk::Image(base_path + "fullscreen-exit.png");

	m_toolbar.set_toolbar_style(m_preferences.GetEnum<Gtk::ToolbarStyle>("Appearance.Toolbar.button_style"));

	m_toolbar.append(m_btnStart, sigc::mem_fun(*this, &OscilloscopeWindow::OnStart));
		m_btnStart.set_tooltip_text("Start (normal trigger)");
		m_btnStart.set_label("Start");
		m_btnStart.set_icon_widget(*Gtk::manage(new Gtk::Image(base_path + "trigger-start.png")));
	m_toolbar.append(m_btnStartSingle, sigc::mem_fun(*this, &OscilloscopeWindow::OnStartSingle));
		m_btnStartSingle.set_tooltip_text("Start (single trigger)");
		m_btnStartSingle.set_label("Single");
		m_btnStartSingle.set_icon_widget(*Gtk::manage(new Gtk::Image(base_path + "trigger-single.png")));
	m_toolbar.append(m_btnStartForce, sigc::mem_fun(*this, &OscilloscopeWindow::OnForceTrigger));
		m_btnStartForce.set_tooltip_text("Force trigger");
		m_btnStartForce.set_label("Force");
		m_btnStartForce.set_icon_widget(*Gtk::manage(new Gtk::Image(base_path + "trigger-single.png")));	//TODO
																											//draw icon
	m_toolbar.append(m_btnStop, sigc::mem_fun(*this, &OscilloscopeWindow::OnStop));
		m_btnStop.set_tooltip_text("Stop trigger");
		m_btnStop.set_label("Stop");
		m_btnStop.set_icon_widget(*Gtk::manage(new Gtk::Image(base_path + "trigger-stop.png")));
	m_toolbar.append(*Gtk::manage(new Gtk::SeparatorToolItem));
	m_toolbar.append(m_btnHistory, sigc::mem_fun(*this, &OscilloscopeWindow::OnHistory));
		m_btnHistory.set_tooltip_text("History");
		m_btnHistory.set_label("History");
		m_btnHistory.set_icon_widget(*Gtk::manage(new Gtk::Image(base_path + "history.png")));
	m_toolbar.append(*Gtk::manage(new Gtk::SeparatorToolItem));
	m_toolbar.append(m_btnRefresh, sigc::mem_fun(*this, &OscilloscopeWindow::OnRefreshConfig));
		m_btnRefresh.set_tooltip_text("Reload configuration from scope");
		m_btnRefresh.set_label("Reload Config");
		m_btnRefresh.set_icon_widget(*Gtk::manage(new Gtk::Image(base_path + "refresh-settings.png")));
	m_toolbar.append(m_btnClearSweeps, sigc::mem_fun(*this, &OscilloscopeWindow::OnClearSweeps));
		m_btnClearSweeps.set_tooltip_text("Clear sweeps");
		m_btnClearSweeps.set_label("Clear Sweeps");
		m_btnClearSweeps.set_icon_widget(*Gtk::manage(new Gtk::Image(base_path + "clear-sweeps.png")));
	m_toolbar.append(m_btnFullscreen, sigc::mem_fun(*this, &OscilloscopeWindow::OnFullscreen));
		m_btnFullscreen.set_tooltip_text("Fullscreen");
		m_btnFullscreen.set_label("Fullscreen");
		m_btnFullscreen.set_icon_widget(m_iconEnterFullscreen);
	m_toolbar.append(*Gtk::manage(new Gtk::SeparatorToolItem));

	m_toolbar.show_all();
}

/**
	@brief Creates the waveform areas for a new scope.
 */
void OscilloscopeWindow::CreateDefaultWaveformAreas(Gtk::Paned* split)
{
	//Create top level waveform group
	auto defaultGroup = new WaveformGroup(this);
	m_waveformGroups.emplace(defaultGroup);
	split->pack1(defaultGroup->m_frame);

	//Create history windows
	for(auto scope : m_scopes)
		m_historyWindows[scope] = new HistoryWindow(this, scope);

	//Process all of the channels
	WaveformGroup* timeDomainGroup = NULL;
	WaveformGroup* frequencyDomainGroup = NULL;
	for(auto scope : m_scopes)
	{
		for(size_t i=0; i<scope->GetChannelCount(); i++)
		{
			auto chan = scope->GetChannel(i);

			//Qualify the channel name by the scope name if we have >1 scope enabled
			if(m_scopes.size() > 1)
				chan->SetDisplayName(scope->m_nickname + ":" + chan->GetHwname());

			auto type = chan->GetType(0);

			//Ignore any channels that aren't analog or digital for now
			if( (type != Stream::STREAM_TYPE_ANALOG) &&
				(type != Stream::STREAM_TYPE_DIGITAL) )
			{
				continue;
			}

			//Only enable digital channels if they're already on
			if(type == Stream::STREAM_TYPE_DIGITAL)
			{
				if(!chan->IsEnabled())
					continue;
			}

			//Skip channels we can't enable
			if(!scope->CanEnableChannel(i))
				continue;

			//Put time and frequency domain channels in different groups
			bool freqDomain = chan->GetXAxisUnits() == Unit(Unit::UNIT_HZ);
			WaveformGroup* wg = NULL;
			if(freqDomain)
			{
				wg = frequencyDomainGroup;

				//Do not show spectrum channels unless they're already on
				if(!chan->IsEnabled())
					continue;
			}
			else
				wg = timeDomainGroup;

			//If the group doesn't exist yet, create/assign it
			if(wg == NULL)
			{
				//Both groups unassigned. Use default group for our current domain
				if( (timeDomainGroup == NULL) && (frequencyDomainGroup == NULL) )
					wg = defaultGroup;

				//Default group assigned, make a secondary one
				else
				{
					auto secondaryGroup = new WaveformGroup(this);
					m_waveformGroups.emplace(secondaryGroup);
					split->pack2(secondaryGroup->m_frame);
					wg = secondaryGroup;
				}

				//Either way, our domain now has a group
				if(freqDomain)
					frequencyDomainGroup = wg;
				else
					timeDomainGroup = wg;
			}

			//Create a waveform area for each stream in the output
			for(size_t j=0; j<chan->GetStreamCount(); j++)
			{
				//For now, assume all instrument channels have only one output stream
				auto w = new WaveformArea(StreamDescriptor(chan, j), this);
				w->m_group = wg;
				m_waveformAreas.emplace(w);
				if(type == Stream::STREAM_TYPE_DIGITAL)
					wg->m_waveformBox.pack_start(*w, Gtk::PACK_SHRINK);
				else
					wg->m_waveformBox.pack_start(*w);
			}
		}
	}

	//Done. Show everything except the measurement views
	show_all();
	if(frequencyDomainGroup)
		frequencyDomainGroup->m_measurementView.hide();
	if(timeDomainGroup)
		timeDomainGroup->m_measurementView.hide();
	defaultGroup->m_measurementView.hide();		//When starting up the application with no scope connected,
												//the default group is not yet committed to time or frequency domain.
												//So we have to hide the measurements regardless.
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Message handlers

bool OscilloscopeWindow::OnTimer(int /*timer*/)
{
	//Don't process any trigger events, etc during file load
	if(m_loadInProgress)
		return true;

	if(m_shuttingDown)
	{
		for(auto it : m_historyWindows)
			it.second->close();
		return false;
	}

	if(m_triggerArmed)
	{
		if(g_waveformReadyEvent.Peek())
		{
			m_framesClock.Tick();

			//Crunch the new waveform
			{
				lock_guard<recursive_mutex> lock2(m_waveformDataMutex);

				//Update the history windows
				for(auto scope : m_scopes)
				{
					if(!scope->IsOffline())
						m_historyWindows[scope]->OnWaveformDataReady();
				}

				//Update filters etc once every instrument has been updated
				OnAllWaveformsUpdated(false, false);
			}

			//Release the waveform processing thread
			g_waveformProcessedEvent.Signal();

			//In multi-scope free-run mode, re-arm every instrument's trigger after we've processed all data
			if(m_multiScopeFreeRun)
				ArmTrigger(TRIGGER_TYPE_NORMAL);
		}
	}

	//Discard all pending waveform data if the trigger isn't armed.
	//Failure to do this can lead to a spurious trigger after we wanted to stop.
	else
	{
		for(auto scope : m_scopes)
			scope->ClearPendingWaveforms();
	}

	//Clean up the scope sync wizard if it's completed
	if(m_syncComplete && (m_scopeSyncWizard != NULL) )
	{
		delete m_scopeSyncWizard;
		m_scopeSyncWizard = NULL;
	}

	return true;
}

void OscilloscopeWindow::OnPreferences()
{
    if(m_preferenceDialog)
        delete m_preferenceDialog;

    m_preferenceDialog = new PreferenceDialog{ this, m_preferences };
    m_preferenceDialog->show();
    m_preferenceDialog->signal_response().connect(sigc::mem_fun(*this, &OscilloscopeWindow::OnPreferenceDialogResponse));
}

/**
	@brief Update filter colors from the preferences manager
 */
void OscilloscopeWindow::SyncFilterColors()
{
	//Filter colors
	StandardColors::colors[StandardColors::COLOR_DATA] =
		m_preferences.GetColor("Appearance.Decodes.data_color").to_string();
	StandardColors::colors[StandardColors::COLOR_CONTROL] =
		m_preferences.GetColor("Appearance.Decodes.control_color").to_string();
	StandardColors::colors[StandardColors::COLOR_ADDRESS] =
		m_preferences.GetColor("Appearance.Decodes.address_color").to_string();
	StandardColors::colors[StandardColors::COLOR_PREAMBLE] =
		m_preferences.GetColor("Appearance.Decodes.preamble_color").to_string();
	StandardColors::colors[StandardColors::COLOR_CHECKSUM_OK] =
		m_preferences.GetColor("Appearance.Decodes.checksum_ok_color").to_string();
	StandardColors::colors[StandardColors::COLOR_CHECKSUM_BAD] =
		m_preferences.GetColor("Appearance.Decodes.checksum_bad_color").to_string();
	StandardColors::colors[StandardColors::COLOR_ERROR] =
		m_preferences.GetColor("Appearance.Decodes.error_color").to_string();
	StandardColors::colors[StandardColors::COLOR_IDLE] =
		m_preferences.GetColor("Appearance.Decodes.idle_color").to_string();

	//Protocol analyzer colors
	PacketDecoder::m_backgroundColors[PacketDecoder::PROTO_COLOR_DEFAULT] =
		m_preferences.GetColor("Appearance.Protocol Analyzer.default_color").to_string();
	PacketDecoder::m_backgroundColors[PacketDecoder::PROTO_COLOR_ERROR] =
		m_preferences.GetColor("Appearance.Protocol Analyzer.error_color").to_string();
	PacketDecoder::m_backgroundColors[PacketDecoder::PROTO_COLOR_STATUS] =
		m_preferences.GetColor("Appearance.Protocol Analyzer.status_color").to_string();
	PacketDecoder::m_backgroundColors[PacketDecoder::PROTO_COLOR_CONTROL] =
		m_preferences.GetColor("Appearance.Protocol Analyzer.control_color").to_string();
	PacketDecoder::m_backgroundColors[PacketDecoder::PROTO_COLOR_DATA_READ] =
		m_preferences.GetColor("Appearance.Protocol Analyzer.data_read_color").to_string();
	PacketDecoder::m_backgroundColors[PacketDecoder::PROTO_COLOR_DATA_WRITE] =
		m_preferences.GetColor("Appearance.Protocol Analyzer.data_write_color").to_string();
	PacketDecoder::m_backgroundColors[PacketDecoder::PROTO_COLOR_COMMAND] =
		m_preferences.GetColor("Appearance.Protocol Analyzer.command_color").to_string();
}

void OscilloscopeWindow::OnPreferenceDialogResponse(int response)
{
	if(response == Gtk::RESPONSE_OK)
	{
		m_preferenceDialog->SaveChanges();

		//Update the UI since we might have changed colors or other display settings
		SyncFilterColors();
		PopulateToolbar();
		SetTitle();
		for(auto w : m_waveformAreas)
		{
			w->SyncFontPreferences();
			w->queue_draw();
		}
		for(auto g : m_waveformGroups)
			g->m_timeline.queue_draw();
	}

	//Clean up the dialog
	delete m_preferenceDialog;
	m_preferenceDialog = NULL;
}

/**
	@brief Clean up when we're closed
 */
bool OscilloscopeWindow::on_delete_event(GdkEventAny* /*any_event*/)
{
	m_shuttingDown = true;

	CloseSession();
	return false;
}

/**
	@brief Shuts down the current session in preparation for opening a saved file etc
 */
void OscilloscopeWindow::CloseSession()
{
	lock_guard<recursive_mutex> lock(m_waveformDataMutex);

	//Stop the trigger so there's no pending waveforms
	OnStop();

	//Clear our trigger state
	//Important to signal the WaveformProcessingThread so it doesn't block waiting on response that's not going to come
	m_triggerArmed = false;
	g_waveformReadyEvent.Clear();
	g_waveformProcessedEvent.Signal();

    //Close popup dialogs, if they exist
    if(m_preferenceDialog)
    {
        m_preferenceDialog->hide();
        delete m_preferenceDialog;
        m_preferenceDialog = nullptr;
    }
    if(m_timebasePropertiesDialog)
    {
		m_timebasePropertiesDialog->hide();
		delete m_timebasePropertiesDialog;
		m_timebasePropertiesDialog = nullptr;
	}
	if(m_addFilterDialog)
    {
		m_addFilterDialog->hide();
		delete m_addFilterDialog;
		m_addFilterDialog = nullptr;
	}
	if(m_exportWizard)
	{
		m_exportWizard->hide();
		delete m_exportWizard;
		m_exportWizard = nullptr;
	}

    //Save preferences
    m_preferences.SavePreferences();

	//Need to clear the analyzers before we delete waveform areas.
	//Otherwise waveform areas will try to delete them too
	for(auto a : m_analyzers)
		delete a;
	m_analyzers.clear();

	//Close all of our UI elements
	for(auto it : m_historyWindows)
		delete it.second;
	for(auto s : m_splitters)
		delete s;
	for(auto g : m_waveformGroups)
		delete g;
	for(auto w : m_waveformAreas)
		delete w;
	for(auto it : m_meterDialogs)
		delete it.second;
	for(auto it : m_scopeInfoWindows)
		delete it.second;
	for(auto it : m_functionGeneratorDialogs)
		delete it.second;
	for(auto it : m_scpiConsoleDialogs)
		delete it.second;
	for(auto it : m_triggerPropertiesDialogs)
		delete it.second;

	//Clear our records of them
	m_historyWindows.clear();
	m_splitters.clear();
	m_waveformGroups.clear();
	m_waveformAreas.clear();
	m_meterDialogs.clear();
	m_scopeInfoWindows.clear();
	m_functionGeneratorDialogs.clear();
	m_scpiConsoleDialogs.clear();

	delete m_scopeSyncWizard;
	m_scopeSyncWizard = NULL;

	delete m_graphEditor;
	m_graphEditor = NULL;

	for(auto it : m_markers)
	{
		auto& markers = it.second;
		for(auto m : markers)
			delete m;
	}
	m_markers.clear();
	m_nextMarker = 1;

	m_multiScopeFreeRun = false;

	//Delete stuff from our UI
	auto children = m_setupTriggerMenu.get_children();
	for(auto c : children)
		m_setupTriggerMenu.remove(*c);

	//Close stuff in the application, terminate threads, etc
	g_app->ShutDownSession();

	//Get rid of function generators
	//(but only delete them if they're not also a scope)
	for(auto gen : m_funcgens)
	{
		if(0 == (gen->GetInstrumentTypes() & Instrument::INST_OSCILLOSCOPE) )
			delete gen;
	}
	m_funcgens.clear();

	//Get rid of multimeters
	//(but only delete them if they're not also a scope)
	for(auto meter : m_meters)
	{
		if(0 == (meter->GetInstrumentTypes() & Instrument::INST_DMM) )
			delete meter;
	}
	m_meters.clear();

	//Get rid of scopes
	for(auto scope : m_scopes)
		delete scope;
	m_scopes.clear();
	m_scopeDeskewCal.clear();

	SetTitle();
}

/**
	@brief Browse for and connect to a scope
 */
void OscilloscopeWindow::OnAddOscilloscope()
{
	InstrumentConnectionDialog dlg;
	while(true)
	{
		if(dlg.run() != Gtk::RESPONSE_OK)
			return;

		//If the user requested an illegal configuration, retry
		if(!dlg.ValidateConfig())
		{
			Gtk::MessageDialog mdlg(
				"Invalid configuration specified.\n"
				"\n"
				"A driver and transport must always be selected.\n"
				"\n"
				"The NULL transport is only legal with the \"demo\" driver.",
				false,
				Gtk::MESSAGE_ERROR,
				Gtk::BUTTONS_OK,
				true);
			mdlg.run();
		}

		else
			break;
	}

	ConnectToScope(dlg.GetConnectionString());
}

void OscilloscopeWindow::ConnectToScope(string path)
{
	//Brand-new session with nothing in it (no filter, no scopes)?
	//Need to set up the whole UI for a new instrument.
	if( m_scopes.empty() && Filter::GetAllInstances().empty() )
	{
		vector<string> paths;
		paths.push_back(path);

		//Connect to the new scope
		CloseSession();
		m_loadInProgress = true;
		m_scopes = g_app->ConnectToScopes(paths);

		//Clear performance counters
		m_totalWaveforms = 0;
		m_framesClock.Reset();

		//Add the top level splitter right before the status bar
		auto split = new Gtk::VPaned;
		m_splitters.emplace(split);
		m_vbox.remove(m_statusbar);
		m_vbox.pack_start(*split, Gtk::PACK_EXPAND_WIDGET);
		m_vbox.pack_start(m_statusbar, Gtk::PACK_SHRINK);

		//Add all of the UI stuff
		CreateDefaultWaveformAreas(split);

		//Done
		SetTitle();
		OnLoadComplete();

		//Arm the trigger
		OnStart();
	}

	//Adding a new instrument to an existing session.
	else
	{
		//Pause any existing triggers
		OnStop();

		//Connect to the new scope
		vector<string> paths;
		paths.push_back(path);
		auto scopes = g_app->ConnectToScopes(paths);
		for(auto s : scopes)
		{
			m_scopes.push_back(s);
			m_historyWindows[s] = new HistoryWindow(this, s);
		}

		//Create waveform areas for all enabled channels.
		//If no channels enabled, add an area for the first channel
		for(auto s : scopes)
		{
			bool addedSomething = false;

			for(size_t i=0; i<s->GetChannelCount(); i++)
			{
				auto chan = s->GetChannel(i);

				//Qualify the channel name by the scope name
				chan->SetDisplayName(s->m_nickname + ":" + chan->GetHwname());

				if(s->IsChannelEnabled(i))
				{
					for(size_t j=0; j<chan->GetStreamCount(); j++)
					{
						OnAddChannel(StreamDescriptor(chan, j));
						addedSomething = true;
					}
				}
			}

			if(!addedSomething)
				OnAddChannel(StreamDescriptor(s->GetChannel(0), 0));
		}

		//Start scope thread for the new instrument
		g_app->StartScopeThreads(scopes);

		//Update the title
		SetTitle();

		//Done, refresh the UI and such
		OnInstrumentAdded();
	}
}

/**
	@brief Open a saved configuration
 */
void OscilloscopeWindow::OnFileOpen(bool reconnect)
{
	//TODO: prompt to save changes to the current session

	Gtk::FileChooserDialog dlg(*this, "Open", Gtk::FILE_CHOOSER_ACTION_OPEN);
	auto filter = Gtk::FileFilter::create();
	filter->add_pattern("*.scopesession");
	filter->set_name("glscopeclient sessions (*.scopesession)");
	dlg.add_filter(filter);
	dlg.add_button("Open", Gtk::RESPONSE_OK);
	dlg.add_button("Cancel", Gtk::RESPONSE_CANCEL);
	auto response = dlg.run();

	if(response != Gtk::RESPONSE_OK)
		return;

	DoFileOpen(dlg.get_filename(), true, reconnect);
}

/**
	@brief Open a saved file
 */
void OscilloscopeWindow::DoFileOpen(const string& filename, bool loadWaveform, bool reconnect)
{
	lock_guard<recursive_mutex> lock(m_waveformDataMutex);

	m_currentFileName = filename;
	m_recentFiles[filename] = time(nullptr);

	m_loadInProgress = true;

	CloseSession();

	//Clear performance counters
	m_totalWaveforms = 0;
	m_framesClock.Reset();

	try
	{
		auto docs = YAML::LoadAllFromFile(m_currentFileName);

		//Only open the first doc, our file format doesn't ever generate multiple docs in a file.
		//Ignore any trailing stuff at the end
		auto node = docs[0];

		//Load various sections of the file
		IDTable table;
		LoadInstruments(node["instruments"], reconnect, table);
		LoadDecodes(node["decodes"], table);
		LoadUIConfiguration(node["ui_config"], table);

		//Create history windows for all of our scopes
		for(auto scope : m_scopes)
		{
			auto hist = new HistoryWindow(this, scope);
			hist->hide();
			m_historyWindows[scope] = hist;
		}

		//Re-title the window for the new scope
		SetTitle();

		//Load data
		try
		{
			if(loadWaveform)
				LoadWaveformData(filename, table);
		}
		catch(const YAML::BadFile& ex)
		{
			Gtk::MessageDialog dlg(
				*this,
				"Failed to load saved waveform data",
				false,
				Gtk::MESSAGE_ERROR,
				Gtk::BUTTONS_OK,
				true);
			dlg.run();
		}
	}
	catch(const YAML::BadFile& ex)
	{
		Gtk::MessageDialog dlg(
			*this,
			string("Unable to open file ") + filename + ".",
			false,
			Gtk::MESSAGE_ERROR,
			Gtk::BUTTONS_OK,
			true);
		dlg.run();
		return;
	}

	OnLoadComplete();
}

/**
	@brief Refresh everything in the UI when a new instrument has been added
 */
void OscilloscopeWindow::OnInstrumentAdded()
{
	//Update all of our menus etc
	FindScopeFuncGens();
	AddCurrentToRecentInstrumentList();
	SaveRecentInstrumentList();
	RefreshInstrumentMenus();
	RefreshChannelsMenu();
	RefreshAnalyzerMenu();
	RefreshMultimeterMenu();
	RefreshScopeInfoMenu();
	RefreshTriggerMenu();
	RefreshGeneratorsMenu();
	RefreshScpiConsoleMenu();

	//Make sure all resize etc events have been handled.
	//Otherwise eye patterns don't refresh right.
	show_all();
	GarbageCollectGroups();
	g_app->DispatchPendingEvents();

	RedrawAfterLoad();
}

/**
	@brief Redraw everything after loading has completed
 */
void OscilloscopeWindow::RedrawAfterLoad()
{
	//Mark all waveform geometry as dirty
	for(auto w : m_waveformAreas)
		w->SetGeometryDirty();

	//Done loading, we can render everything for good now.
	//Issue 2 render calls since the very first render does some setup stuff
	m_loadInProgress = false;
	ClearAllPersistence();
	g_app->DispatchPendingEvents();
	ClearAllPersistence();
}

/**
	@brief Refresh everything in the UI when a new file has been loaded
 */
void OscilloscopeWindow::OnLoadComplete()
{
	OnInstrumentAdded();

	//TODO: refresh measurements and protocol decodes

	//Create protocol analyzers
	for(auto area : m_waveformAreas)
	{
		for(size_t i=0; i<area->GetOverlayCount(); i++)
		{
			auto pdecode = dynamic_cast<PacketDecoder*>(area->GetOverlay(i).m_channel);
			if(pdecode != NULL)
			{
				char title[256];
				snprintf(title, sizeof(title), "Protocol Analyzer: %s", pdecode->GetDisplayName().c_str());

				auto analyzer = new ProtocolAnalyzerWindow(title, this, pdecode, area);
				m_analyzers.emplace(analyzer);

				//Done
				analyzer->show();
			}
		}
	}

	//TODO: make this work properly if we have decodes spanning multiple scopes
	for(auto it : m_historyWindows)
		it.second->ReplayHistory();

	//Add our markers point to each history window
	for(auto it : m_markers)
	{
		auto timestamp = it.first;
		auto markers = it.second;
		for(auto m : markers)
		{
			for(auto w : m_historyWindows)
				w.second->AddMarker(timestamp, m->m_offset, m->m_name, m);
		}
	}

	//Filters are refreshed by ReplayHistory(), but if we have no scopes (all waveforms created by filters)
	//then nothing will happen. In this case, a manual refresh of the filter graph is necessary.
	if(m_scopes.empty())
	{
		RefreshAllFilters();
		RefreshProtocolAnalyzers();
	}

	//Start threads to poll scopes etc
	else
		g_app->StartScopeThreads(m_scopes);

	RedrawAfterLoad();
	RefreshRecentFileMenu();
}

/**
	@brief Loads waveform data for a save file
 */
void OscilloscopeWindow::LoadWaveformData(string filename, IDTable& table)
{
	//Create and show progress dialog
	FileProgressDialog progress;
	progress.show();

	//Figure out data directory
	string base = filename.substr(0, filename.length() - strlen(".scopesession"));
	string datadir = base + "_data";

	//Load data for each scope
	float progress_per_scope = 1.0f / m_scopes.size();
	for(size_t i=0; i<m_scopes.size(); i++)
	{
		auto scope = m_scopes[i];
		int id = table[scope];

		char tmp[512];
		snprintf(tmp, sizeof(tmp), "%s/scope_%d_metadata.yml", datadir.c_str(), id);
		auto docs = YAML::LoadAllFromFile(tmp);

		LoadWaveformDataForScope(docs[0], scope, datadir, table, progress, i*progress_per_scope, progress_per_scope);
	}
}

/**
	@brief Loads waveform data for a single instrument
 */
void OscilloscopeWindow::LoadWaveformDataForScope(
	const YAML::Node& node,
	Oscilloscope* scope,
	string datadir,
	IDTable& table,
	FileProgressDialog& progress,
	float base_progress,
	float progress_range
	)
{
	progress.Update("Loading oscilloscope configuration", base_progress);

	TimePoint time;
	time.first = 0;
	time.second = 0;

	TimePoint newest;
	newest.first = 0;
	newest.second = 0;

	auto window = m_historyWindows[scope];
	int scope_id = table[scope];

	//Clear out any old waveforms the instrument may have
	for(size_t i=0; i<scope->GetChannelCount(); i++)
	{
		auto chan = scope->GetChannel(i);
		for(size_t j=0; j<chan->GetStreamCount(); j++)
			chan->SetData(NULL, j);
	}

	//Preallocate size
	auto wavenode = node["waveforms"];
	window->SetMaxWaveforms(wavenode.size());

	//Load the data for each waveform
	float waveform_progress = progress_range / wavenode.size();
	size_t iwave = 0;
	for(auto it : wavenode)
	{
		iwave ++;

		//Top level metadata
		bool timebase_is_ps = true;
		auto wfm = it.second;
		time.first = wfm["timestamp"].as<long long>();
		if(wfm["time_psec"])
		{
			time.second = wfm["time_psec"].as<long long>() * 1000;
			timebase_is_ps = true;
		}
		else
		{
			time.second = wfm["time_fsec"].as<long long>();
			timebase_is_ps = false;
		}
		int waveform_id = wfm["id"].as<int>();
		bool pinned = false;
		if(wfm["pinned"])
			pinned = wfm["pinned"].as<int>();
		string label;
		if(wfm["label"])
			label = wfm["label"].as<string>();

		//Set up channel metadata first (serialized)
		auto chans = wfm["channels"];
		vector<pair<int, int>> channels;	//pair<channel, stream>
		vector<string> formats;
		for(auto jt : chans)
		{
			auto ch = jt.second;
			int channel_index = ch["index"].as<int>();
			int stream = 0;
			if(ch["stream"])
				stream = ch["stream"].as<int>();
			auto chan = scope->GetChannel(channel_index);
			channels.push_back(pair<int, int>(channel_index, stream));

			//Waveform format defaults to sparsev1 as that's what was used before
			//the metadata file contained a format ID at all
			string format = "sparsev1";
			if(ch["format"])
				format = ch["format"].as<string>();
			formats.push_back(format);

			bool dense = (format == "densev1");

			//TODO: support non-analog/digital captures (eyes, spectrograms, etc)
			WaveformBase* cap = NULL;
			SparseAnalogWaveform* sacap = NULL;
			UniformAnalogWaveform* uacap = NULL;
			SparseDigitalWaveform* sdcap = NULL;
			UniformDigitalWaveform* udcap = NULL;
			if(chan->GetType(0) == Stream::STREAM_TYPE_ANALOG)
			{
				if(dense)
					cap = uacap = new UniformAnalogWaveform;
				else
					cap = sacap = new SparseAnalogWaveform;
			}
			else
			{
				if(dense)
					cap = udcap = new UniformDigitalWaveform;
				else
					cap = sdcap = new SparseDigitalWaveform;
			}

			//Channel waveform metadata
			cap->m_timescale = ch["timescale"].as<long>();
			cap->m_startTimestamp = time.first;
			cap->m_startFemtoseconds = time.second;
			if(timebase_is_ps)
			{
				cap->m_timescale *= 1000;
				cap->m_triggerPhase = ch["trigphase"].as<float>() * 1000;
			}
			else
				cap->m_triggerPhase = ch["trigphase"].as<long long>();

			chan->Detach(stream);
			chan->SetData(cap, stream);
		}

		//Kick off a thread to load data for each channel
		vector<thread*> threads;
		size_t nchans = channels.size();
		volatile float* channel_progress = new float[nchans];
		volatile int* channel_done = new int[nchans];
		for(size_t i=0; i<channels.size(); i++)
		{
			channel_progress[i] = 0;
			channel_done[i] = 0;

			threads.push_back(new thread(
				&OscilloscopeWindow::DoLoadWaveformDataForScope,
				channels[i].first,
				channels[i].second,
				scope,
				datadir,
				scope_id,
				waveform_id,
				formats[i],
				channel_progress + i,
				channel_done + i
				));
		}

		//Process events and update the display with each thread's progress
		while(true)
		{
			//Figure out total progress across each channel. Stop if all threads are done
			bool done = true;
			float frac = 0;
			for(size_t i=0; i<nchans; i++)
			{
				if(!channel_done[i])
					done = false;
				frac += channel_progress[i];
			}
			if(done)
				break;
			frac /= nchans;

			//Update the UI
			char tmp[256];
			snprintf(
				tmp,
				sizeof(tmp),
				"Loading waveform %zu/%zu for instrument %s: %.0f %% complete",
				iwave,
				wavenode.size(),
				scope->m_nickname.c_str(),
				frac * 100);
			progress.Update(tmp, base_progress + frac*waveform_progress);
			std::this_thread::sleep_for(std::chrono::microseconds(1000 * 50));

			g_app->DispatchPendingEvents();
		}

		delete[] channel_progress;
		delete[] channel_done;

		//Wait for threads to complete
		for(auto t : threads)
		{
			t->join();
			delete t;
		}

		//Add to history
		window->OnWaveformDataReady(true, pinned, label);

		//Keep track of the newest waveform (may not be in time order)
		if( (time.first > newest.first) ||
			( (time.first == newest.first) &&  (time.second > newest.second) ) )
		{
			newest = time;
		}

		base_progress += waveform_progress;
	}

	window->JumpToHistory(newest);
}

void OscilloscopeWindow::DoLoadWaveformDataForScope(
	int channel_index,
	int stream,
	Oscilloscope* scope,
	string datadir,
	int scope_id,
	int waveform_id,
	string format,
	volatile float* progress,
	volatile int* done
	)
{
	auto chan = scope->GetChannel(channel_index);

	auto cap = chan->GetData(stream);
	auto sacap = dynamic_cast<SparseAnalogWaveform*>(cap);
	auto uacap = dynamic_cast<UniformAnalogWaveform*>(cap);
	auto sdcap = dynamic_cast<SparseDigitalWaveform*>(cap);
	auto udcap = dynamic_cast<UniformDigitalWaveform*>(cap);

	cap->PrepareForCpuAccess();

	//Load the actual sample data
	char tmp[512];
	if(stream == 0)
	{
		snprintf(tmp, sizeof(tmp), "%s/scope_%d_waveforms/waveform_%d/channel_%d.bin",
			datadir.c_str(),
			scope_id,
			waveform_id,
			channel_index);
	}
	else
	{
		snprintf(tmp, sizeof(tmp), "%s/scope_%d_waveforms/waveform_%d/channel_%d_stream%d.bin",
			datadir.c_str(),
			scope_id,
			waveform_id,
			channel_index,
			stream);
	}

	//Load samples into memory
	unsigned char* buf = NULL;

	//Windows: use generic file reads for now
	#ifdef _WIN32
		FILE* fp = fopen(tmp, "rb");
		if(!fp)
		{
			LogError("couldn't open %s\n", tmp);
			return;
		}

		//Read the whole file into a buffer a megabyte at a time
		fseek(fp, 0, SEEK_END);
		long len = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		buf = new unsigned char[len];
		long len_remaining = len;
		long blocksize = 1024*1024;
		long read_offset = 0;
		while(len_remaining > 0)
		{
			if(blocksize > len_remaining)
				blocksize = len_remaining;

			//Most time is spent on the fread's when using this path
			*progress = read_offset * 1.0 / len;
			fread(buf + read_offset, 1, blocksize, fp);

			len_remaining -= blocksize;
			read_offset += blocksize;
		}
		fclose(fp);

	//On POSIX, just memory map the file
	#else
		int fd = open(tmp, O_RDONLY);
		if(fd < 0)
		{
			LogError("couldn't open %s\n", tmp);
			return;
		}
		size_t len = lseek(fd, 0, SEEK_END);
		buf = (unsigned char*)mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);

		//For now, report progress complete upon the file being fully read
		*progress = 1;
	#endif

	//Sparse interleaved
	if(format == "sparsev1")
	{
		//Figure out how many samples we have
		size_t samplesize = 2*sizeof(int64_t);
		if(sacap)
			samplesize += sizeof(float);
		else
			samplesize += sizeof(bool);
		size_t nsamples = len / samplesize;
		cap->Resize(nsamples);

		//TODO: AVX this?
		for(size_t j=0; j<nsamples; j++)
		{
			size_t offset = j*samplesize;

			//Read start time and duration
			int64_t* stime = reinterpret_cast<int64_t*>(buf+offset);
			offset += 2*sizeof(int64_t);

			//Read sample data
			if(sacap)
			{
				//The file format assumes "float" is IEEE754 32-bit float.
				//If your platform doesn't do that, good luck.
				//cppcheck-suppress invalidPointerCast
				sacap->m_samples[j] = *reinterpret_cast<float*>(buf+offset);

				sacap->m_offsets[j] = stime[0];
				sacap->m_durations[j] = stime[1];
			}

			else
			{
				sdcap->m_samples[j] = *reinterpret_cast<bool*>(buf+offset);
				sdcap->m_offsets[j] = stime[0];
				sdcap->m_durations[j] = stime[1];
			}

			//TODO: progress updates
		}

		//Quickly check if the waveform is dense packed, even if it was stored as sparse.
		//Since we know samples must be monotonic and non-overlapping, we don't have to check every single one!
		int64_t nlast = nsamples - 1;
		if(sacap)
		{
			if( (sacap->m_offsets[0] == 0) &&
				(sacap->m_offsets[nlast] == nlast) &&
				(sacap->m_durations[nlast] == 1) )
			{
				//Waveform was actually uniform, so convert it
				cap = new UniformAnalogWaveform(*sacap);
				chan->SetData(cap, stream);
			}
		}
	}

	//Dense packed
	else if(format == "densev1")
	{
		//Figure out length
		size_t nsamples = 0;
		if(uacap)
			nsamples = len / sizeof(float);
		else if(udcap)
			nsamples = len / sizeof(bool);
		cap->Resize(nsamples);

		//Read sample data
		if(uacap)
			memcpy(uacap->m_samples.GetCpuPointer(), buf, nsamples*sizeof(float));
		else
			memcpy(udcap->m_samples.GetCpuPointer(), buf, nsamples*sizeof(bool));
	}

	else
	{
		LogError(
			"Unknown waveform format \"%s\", perhaps this file was created by a newer version of glscopeclient?\n",
			format.c_str());
	}

	cap->MarkSamplesModifiedFromCpu();

	#ifdef _WIN32
		delete[] buf;
	#else
		munmap(buf, len);
		::close(fd);
	#endif

	*done = 1;
	*progress = 1;
}

/**
	@brief Apply driver preferences to an instrument
 */
void OscilloscopeWindow::ApplyPreferences(Oscilloscope* scope)
{
	//Apply driver-specific preference settings
	auto lecroy = dynamic_cast<LeCroyOscilloscope*>(scope);
	if(lecroy)
	{
		if(GetPreferences().GetBool("Drivers.Teledyne LeCroy.force_16bit"))
			lecroy->ForceHDMode(true);

		//else auto resolution depending on instrument type
	}
}

/**
	@brief Reconnect to existing instruments and reconfigure them
 */
void OscilloscopeWindow::LoadInstruments(const YAML::Node& node, bool reconnect, IDTable& table)
{
	if(!node)
	{
		LogError("Save file missing instruments node\n");
		return;
	}

	//Load each instrument
	for(auto it : node)
	{
		auto inst = it.second;

		Oscilloscope* scope = NULL;

		auto transtype = inst["transport"].as<string>();
		auto driver = inst["driver"].as<string>();

		if(reconnect)
		{
			if( (transtype == "null") && (driver != "demo") )
			{
				Gtk::MessageDialog dlg(
					*this,
					"Cannot reconnect to instrument because the .scopesession file does not contain any connection "
					"information.\n\n"
					"Loading file in offline mode.",
					false,
					Gtk::MESSAGE_ERROR,
					Gtk::BUTTONS_OK,
					true);
				dlg.run();
			}
			else
			{
				//Create the scope
				auto transport = SCPITransport::CreateTransport(transtype, inst["args"].as<string>());

				//Check if the transport failed to initialize
				if((transport == NULL) || !transport->IsConnected())
				{
					Gtk::MessageDialog dlg(
						*this,
						string("Failed to connect to instrument using connection string ") + inst["args"].as<string>(),
						false,
						Gtk::MESSAGE_ERROR,
						Gtk::BUTTONS_OK,
						true);
					dlg.run();
				}

				//All good, try to connect
				else
				{
					scope = Oscilloscope::CreateOscilloscope(driver, transport);

					//Sanity check make/model/serial. If mismatch, stop
					string message;
					bool fail = false;
					if(inst["name"].as<string>() != scope->GetName())
					{
						message = string("Unable to connect to oscilloscope: instrument has model name \"") +
							scope->GetName() + "\", save file has model name \"" + inst["name"].as<string>()  + "\"";
						fail = true;
					}
					else if(inst["vendor"].as<string>() != scope->GetVendor())
					{
						message = string("Unable to connect to oscilloscope: instrument has vendor \"") +
							scope->GetVendor() + "\", save file has vendor \"" + inst["vendor"].as<string>()  + "\"";
						fail = true;
					}
					else if(inst["serial"].as<string>() != scope->GetSerial())
					{
						message = string("Unable to connect to oscilloscope: instrument has serial \"") +
							scope->GetSerial() + "\", save file has serial \"" + inst["serial"].as<string>()  + "\"";
						fail = true;
					}
					if(fail)
					{
						Gtk::MessageDialog dlg(*this, message, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
						dlg.run();
						delete scope;
						scope = NULL;
					}
				}
			}
		}

		if(!scope)
		{
			//Create the mock scope
			scope = new MockOscilloscope(
				inst["name"].as<string>(),
				inst["vendor"].as<string>(),
				inst["serial"].as<string>(),
				transtype,
				driver,
				inst["args"].as<string>()
				);
		}

		//Make any config settings to the instrument from our preference settings
		ApplyPreferences(scope);

		//All good. Add to our list of scopes etc
		m_scopes.push_back(scope);
		table.emplace(inst["id"].as<int>(), scope);

		//Configure the scope
		scope->LoadConfiguration(inst, table);

		//Load trigger deskew
		if(inst["triggerdeskew"])
			m_scopeDeskewCal[scope] = inst["triggerdeskew"].as<int64_t>();
	}
}

/**
	@brief Load protocol decoder configuration
 */
void OscilloscopeWindow::LoadDecodes(const YAML::Node& node, IDTable& table)
{
	//No protocol decodes? Skip this section
	if(!node)
		return;

	//Load each decode
	for(auto it : node)
	{
		auto dnode = it.second;

		//Create the decode
		auto proto = dnode["protocol"].as<string>();
		auto filter = Filter::CreateFilter(proto, dnode["color"].as<string>());
		if(filter == NULL)
		{
			Gtk::MessageDialog dlg(
				string("Unable to create filter \"") + proto + "\". Skipping...\n",
				false,
				Gtk::MESSAGE_ERROR,
				Gtk::BUTTONS_OK,
				true);
			dlg.run();
			continue;
		}

		table.emplace(dnode["id"].as<int>(), filter);

		//Load parameters during the first pass.
		//Parameters can't have dependencies on other channels etc.
		//More importantly, parameters may change bus width etc
		filter->LoadParameters(dnode, table);
	}

	//Make a second pass to configure the filter inputs, once all of them have been instantiated.
	//Filters may depend on other filters as inputs, and serialization is not guaranteed to be a topological sort.
	for(auto it : node)
	{
		auto dnode = it.second;
		auto filter = static_cast<Filter*>(table[dnode["id"].as<int>()]);
		if(filter)
			filter->LoadInputs(dnode, table);
	}
}

/**
	@brief Load user interface configuration
 */
void OscilloscopeWindow::LoadUIConfiguration(const YAML::Node& node, IDTable& table)
{
	//Window configuration
	auto wnode = node["window"];
	resize(wnode["width"].as<int>(), wnode["height"].as<int>());

	//Waveform areas
	auto areas = node["areas"];
	for(auto it : areas)
	{
		//Load the area itself
		auto an = it.second;
		auto channel = static_cast<OscilloscopeChannel*>(table[an["channel"].as<int>()]);
		if(!channel)	//don't crash on bad IDs or missing decodes
			continue;
		size_t stream = 0;
		if(an["stream"])
			stream = an["stream"].as<int>();
		WaveformArea* area = new WaveformArea(StreamDescriptor(channel, stream), this);
		table.emplace(an["id"].as<int>(), area);
		area->SetPersistenceEnabled(an["persistence"].as<int>() ? true : false);
		m_waveformAreas.emplace(area);

		//Add any overlays
		auto overlays = an["overlays"];
		for(auto jt : overlays)
		{
			auto filter = static_cast<Filter*>(table[jt.second["id"].as<int>()]);
			stream = 0;
			if(jt.second["stream"])
				stream = jt.second["stream"].as<int>();
			if(filter)
				area->AddOverlay(StreamDescriptor(filter, stream));
		}
	}

	//Waveform groups
	auto groups = node["groups"];
	for(auto it : groups)
	{
		//Create the group
		auto gn = it.second;
		WaveformGroup* group = new WaveformGroup(this);
		table.emplace(gn["id"].as<int>(), &group->m_frame);
		group->m_framelabel.set_label(gn["name"].as<string>());

		//Scale if needed
		bool timestamps_are_ps = true;
		if(gn["timebaseResolution"])
		{
			if(gn["timebaseResolution"].as<string>() == "fs")
				timestamps_are_ps = false;
		}

		group->m_pixelsPerXUnit = gn["pixelsPerXUnit"].as<float>();
		group->m_xAxisOffset = gn["xAxisOffset"].as<long long>();
		m_waveformGroups.emplace(group);

		//Cursor config
		string cursor = gn["cursorConfig"].as<string>();
		if(cursor == "none")
			group->m_cursorConfig = WaveformGroup::CURSOR_NONE;
		else if(cursor == "x_single")
			group->m_cursorConfig = WaveformGroup::CURSOR_X_SINGLE;
		else if(cursor == "x_dual")
			group->m_cursorConfig = WaveformGroup::CURSOR_X_DUAL;
		else if(cursor == "y_single")
			group->m_cursorConfig = WaveformGroup::CURSOR_Y_SINGLE;
		else if(cursor == "y_dual")
			group->m_cursorConfig = WaveformGroup::CURSOR_Y_DUAL;
		group->m_xCursorPos[0] = gn["xcursor0"].as<long long>();
		group->m_xCursorPos[1] = gn["xcursor1"].as<long long>();
		group->m_yCursorPos[0] = gn["ycursor0"].as<float>();
		group->m_yCursorPos[1] = gn["ycursor1"].as<float>();

		if(timestamps_are_ps)
		{
			group->m_pixelsPerXUnit /= 1000;
			group->m_xAxisOffset *= 1000;
			group->m_xCursorPos[0] *= 1000;
			group->m_xCursorPos[1] *= 1000;
		}

		auto stats = gn["stats"];
		if(stats)
		{
			for(auto s : stats)
			{
				auto statnode = s.second;
				int stream = 0;
				if(statnode["stream"])
					stream = statnode["stream"].as<long>();

				group->EnableStats(
					StreamDescriptor(
						static_cast<OscilloscopeChannel*>(table[statnode["channel"].as<long>()]),
						stream),
					statnode["index"].as<long>());
			}
		}

		//Waveform areas
		areas = gn["areas"];
		for(auto at : areas)
		{
			auto area = static_cast<WaveformArea*>(table[at.second["id"].as<int>()]);
			if(!area)
				continue;
			area->m_group = group;
			if(area->GetChannel().GetType() == Stream::STREAM_TYPE_DIGITAL)
				group->m_waveformBox.pack_start(*area, Gtk::PACK_SHRINK);
			else
				group->m_waveformBox.pack_start(*area);
		}
	}

	//Markers
	auto markers = node["markers"];
	if(markers)
	{
		for(auto it : markers)
		{
			auto inode = it.second;
			TimePoint timestamp;
			timestamp.first = inode["timestamp"].as<int64_t>();
			timestamp.second = inode["time_fsec"].as<int64_t>();
			for(auto jt : inode["markers"])
			{
				m_markers[timestamp].push_back(new Marker(
					timestamp,
					jt.second["offset"].as<int64_t>(),
					jt.second["name"].as<string>()));
			}
		}
	}

	//Splitters
	auto splitters = node["splitters"];
	for(auto it : splitters)
	{
		//Create the splitter
		auto sn = it.second;
		auto dir = sn["dir"].as<string>();
		Gtk::Paned* split = NULL;
		if(dir == "h")
			split = new Gtk::HPaned;
		else
			split = new Gtk::VPaned;
		m_splitters.emplace(split);
		table.emplace(sn["id"].as<int>(), split);
	}
	for(auto it : splitters)
	{
		auto sn = it.second;
		Gtk::Paned* split = static_cast<Gtk::Paned*>(table[sn["id"].as<int>()]);

		auto a = static_cast<Gtk::Widget*>(table[sn["child0"].as<int>()]);
		auto b = static_cast<Gtk::Widget*>(table[sn["child1"].as<int>()]);
		if(a)
			split->pack1(*a);
		if(b)
			split->pack2(*b);
		split->set_position(sn["split"].as<int>());
	}

	//Add the top level splitter right before the status bar
	m_vbox.remove(m_statusbar);
	m_vbox.pack_start(*static_cast<Gtk::Paned*>(table[node["top"].as<int>()]), Gtk::PACK_EXPAND_WIDGET);
	m_vbox.pack_start(m_statusbar, Gtk::PACK_SHRINK);
}

/**
	@brief Common handler for save/save as commands
 */
void OscilloscopeWindow::OnFileSave(bool saveToCurrentFile)
{
	bool creatingNew = false;

	static const char* extension = ".scopesession";

	//Pop up the dialog if we asked for a new file.
	//But if we don't have a current file, we need to prompt regardless
	if(m_currentFileName.empty() || !saveToCurrentFile)
	{
		creatingNew = true;

		Gtk::FileChooserDialog dlg(*this, "Save", Gtk::FILE_CHOOSER_ACTION_SAVE);

		auto filter = Gtk::FileFilter::create();
		filter->add_pattern("*.scopesession");
		filter->set_name("glscopeclient sessions (*.scopesession)");
		dlg.add_filter(filter);
		dlg.add_button("Save", Gtk::RESPONSE_OK);
		dlg.add_button("Cancel", Gtk::RESPONSE_CANCEL);
		dlg.set_uri(m_currentFileName);
		dlg.set_do_overwrite_confirmation();
		auto response = dlg.run();

		if(response != Gtk::RESPONSE_OK)
			return;

		m_currentFileName = dlg.get_filename();
	}

	//Add the extension if not present
	if(m_currentFileName.find(extension) == string::npos)
		m_currentFileName += extension;

	//Format the directory name
	m_currentDataDirName = m_currentFileName.substr(0, m_currentFileName.length() - strlen(extension)) + "_data";

	//See if the directory exists
	bool dir_exists = false;

#ifndef _WIN32
	int hfile = open(m_currentDataDirName.c_str(), O_RDONLY);
	if(hfile >= 0)
	{
		//It exists as a file. Reopen and check if it's a directory
		::close(hfile);
		hfile = open(m_currentDataDirName.c_str(), O_RDONLY | O_DIRECTORY);

		//If this open works, it's a directory.
		if(hfile >= 0)
		{
			::close(hfile);
			dir_exists = true;
		}

		//Data dir exists, but it's something else! Error out
		else
		{
			string msg = string("The data directory ") + m_currentDataDirName + " already exists, but is not a directory!";
			Gtk::MessageDialog errdlg(msg, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
			errdlg.set_title("Cannot save session\n");
			errdlg.run();
			return;
		}
	}
#else
	auto fileType = GetFileAttributes(m_currentDataDirName.c_str());

	// Check if any file exists at this path
	if(fileType != INVALID_FILE_ATTRIBUTES)
	{
		if(fileType & FILE_ATTRIBUTE_DIRECTORY)
		{
			// directory exists
			dir_exists = true;
		}
		else
		{
			// Its some other file
			string msg = string("The data directory ") + m_currentDataDirName + " already exists, but is not a directory!";
			Gtk::MessageDialog errdlg(msg, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
			errdlg.set_title("Cannot save session\n");
			errdlg.run();
			return;
		}
	}
#endif

	//See if the file exists
	bool file_exists = false;

#ifndef _WIN32
	hfile = open(m_currentFileName.c_str(), O_RDONLY);
	if(hfile >= 0)
	{
		file_exists = true;
		::close(hfile);
	}
#else
	auto fileAttr = GetFileAttributes(m_currentFileName.c_str());

	file_exists = (fileAttr != INVALID_FILE_ATTRIBUTES
		&& !(fileAttr & FILE_ATTRIBUTE_DIRECTORY));

#endif

	//If we are trying to create a new file, warn if the directory exists but the file does not
	//If the file exists GTK will warn, and we don't want to prompt the user twice if both exist!
	if(creatingNew && (dir_exists && !file_exists))
	{
		string msg = string("The data directory ") + m_currentDataDirName +
			" already exists. Overwrite existing contents?";
		Gtk::MessageDialog errdlg(msg, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_YES_NO, true);
		errdlg.set_title("Save session\n");
		if(errdlg.run() != Gtk::RESPONSE_YES)
			return;
	}

	//Create the directory we're saving to (if needed)
	if(!dir_exists)
	{
#ifdef _WIN32
		auto result = mkdir(m_currentDataDirName.c_str());
#else
		auto result = mkdir(m_currentDataDirName.c_str(), 0755);
#endif

		if(0 != result)
		{
			string msg = string("The data directory ") + m_currentDataDirName + " could not be created!";
			Gtk::MessageDialog errdlg(msg, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
			errdlg.set_title("Cannot save session\n");
			errdlg.run();
			return;
		}
	}

	//If we're currently capturing, stop.
	//This prevents waveforms from changing under our nose as we're serializing.
	OnStop();

	//Serialize our configuration and save to the file
	IDTable table;
	string config = SerializeConfiguration(table);
	FILE* fp = fopen(m_currentFileName.c_str(), "w");
	if(!fp)
	{
		string msg = string("The session file ") + m_currentFileName + " could not be created!";
		Gtk::MessageDialog errdlg(msg, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		errdlg.set_title("Cannot save session\n");
		errdlg.run();
		return;
	}
	if(config.length() != fwrite(config.c_str(), 1, config.length(), fp))
	{
		string msg = string("Error writing to session file ") + m_currentFileName + "!";
		Gtk::MessageDialog errdlg(msg, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		errdlg.set_title("Cannot save session\n");
		errdlg.run();
	}
	fclose(fp);

	//Serialize waveform data
	SerializeWaveforms(table);

	//Add to recent list
	m_recentFiles[m_currentFileName] = time(nullptr);
	RefreshRecentFileMenu();
}

string OscilloscopeWindow::SerializeConfiguration(IDTable& table)
{
	string config = "";

	//Save metadata
	config += SerializeMetadata();

	//Save instrument config regardless, since data etc needs it
	config += SerializeInstrumentConfiguration(table);

	//Decodes depend on scope channels, but need to happen before UI elements that use them
	if(!Filter::GetAllInstances().empty())
		config += SerializeFilterConfiguration(table);

	//UI config
	config += SerializeUIConfiguration(table);

	return config;
}

/**
	@brief Adds a write-only metadata block to a scopesession
 */
string OscilloscopeWindow::SerializeMetadata()
{
	string config = "metadata:\n";
	char tmp[256];
	snprintf(tmp, sizeof(tmp), "    appver:  \"glscopeclient %s\"\n", GLSCOPECLIENT_VERSION);
	config += tmp;
	snprintf(tmp, sizeof(tmp), "    appdate: \"%s %s\"\n", __DATE__, __TIME__);
	config += tmp;

	//Format timestamp
	time_t now = time(nullptr);
	struct tm ltime;
#ifdef _WIN32
	localtime_s(&ltime, &now);
#else
	localtime_r(&now, &ltime);
#endif
	char sdate[32];
	char stime[32];
	strftime(stime, sizeof(stime), "%X", &ltime);
	strftime(sdate, sizeof(sdate), "%Y-%m-%d", &ltime);

	snprintf(tmp, sizeof(tmp), "    created: \"%s %s\"\n", sdate, stime);
	config += tmp;

	return config;
}

/**
	@brief Serialize the configuration for all oscilloscopes
 */
string OscilloscopeWindow::SerializeInstrumentConfiguration(IDTable& table)
{
	string config = "instruments:\n";

	for(auto scope : m_scopes)
	{
		config += scope->SerializeConfiguration(table);
		if(m_scopeDeskewCal.find(scope) != m_scopeDeskewCal.end())
			config += string("        triggerdeskew: ") + to_string(m_scopeDeskewCal[scope]) + "\n";
	}

	return config;
}

/**
	@brief Serialize the configuration for all protocol decoders
 */
string OscilloscopeWindow::SerializeFilterConfiguration(IDTable& table)
{
	string config = "decodes:\n";

	auto set = Filter::GetAllInstances();
	for(auto d : set)
		config += d->SerializeConfiguration(table);

	return config;
}

string OscilloscopeWindow::SerializeUIConfiguration(IDTable& table)
{
	char tmp[1024];
	string config = "ui_config:\n";

	config += "    window:\n";
	snprintf(tmp, sizeof(tmp), "        width: %d\n", get_width());
	config += tmp;
	snprintf(tmp, sizeof(tmp), "        height: %d\n", get_height());
	config += tmp;

	//Waveform areas
	config += "    areas:\n";
	for(auto area : m_waveformAreas)
		table.emplace(area);
	for(auto area : m_waveformAreas)
	{
		int id = table[area];
		snprintf(tmp, sizeof(tmp), "        area%d:\n", id);
		config += tmp;
		snprintf(tmp, sizeof(tmp), "            id:          %d\n", id);
		config += tmp;
		snprintf(tmp, sizeof(tmp), "            persistence: %d\n", area->GetPersistenceEnabled());
		config += tmp;

		//Channels
		//By the time we get here, all channels should be accounted for.
		//So there should be no reason to assign names to channels at this point - just use what's already there
		auto chan = area->GetChannel();
		snprintf(tmp, sizeof(tmp), "            channel:     %d\n", table[chan.m_channel]);
		config += tmp;
		snprintf(tmp, sizeof(tmp), "            stream:      %zu\n", chan.m_stream);
		config += tmp;

		//Overlays
		if(area->GetOverlayCount() != 0)
		{
			snprintf(tmp, sizeof(tmp), "            overlays:\n");
			config += tmp;

			for(size_t i=0; i<area->GetOverlayCount(); i++)
			{
				int oid = table[area->GetOverlay(i).m_channel];

				snprintf(tmp, sizeof(tmp), "                overlay%d:\n", oid);
				config += tmp;
				snprintf(tmp, sizeof(tmp), "                    id:      %d\n", oid);
				config += tmp;
				snprintf(tmp, sizeof(tmp), "                    stream:  %zu\n", area->GetOverlay(i).m_stream);
				config += tmp;
			}
		}
	}

	//Waveform groups
	config += "    groups: \n";
	for(auto group : m_waveformGroups)
		table.emplace(&group->m_frame);
	for(auto group : m_waveformGroups)
		config += group->SerializeConfiguration(table);

	//Markers
	config += "    markers: \n";
	int nmarker = 0;
	int nwfm = 0;
	for(auto it : m_markers)
	{
		auto key = it.first;
		auto& markers = it.second;
		if(markers.empty())
			continue;

		snprintf(tmp, sizeof(tmp), "        wfm%d:\n", nwfm);
		config += tmp;
		snprintf(tmp, sizeof(tmp), "            timestamp: %ld\n", key.first);
		config += tmp;
		snprintf(tmp, sizeof(tmp), "            time_fsec: %ld\n", key.second);
		config += tmp;
		snprintf(tmp, sizeof(tmp), "            markers:\n");
		config += tmp;

		for(auto m : markers)
		{
			snprintf(tmp, sizeof(tmp), "                marker%d:\n", nmarker);
			config += tmp;

			snprintf(tmp, sizeof(tmp), "                    offset: %ld\n", m->m_offset);
			config += tmp;
			string name = str_replace("\"", "\\\"", m->m_name);
			snprintf(tmp, sizeof(tmp), "                    name:   \"%s\"\n", name.c_str());
			config += tmp;

			nmarker ++;
		}

		nwfm ++;
	}

	//Splitters
	config += "    splitters: \n";
	for(auto split : m_splitters)
		table.emplace(split);
	for(auto split : m_splitters)
	{
		//Splitter config
		int sid = table[split];
		snprintf(tmp, sizeof(tmp), "        split%d: \n", sid);
		config += tmp;
		snprintf(tmp, sizeof(tmp), "            id:     %d\n", sid);
		config += tmp;

		if(split->get_orientation() == Gtk::ORIENTATION_HORIZONTAL)
			config +=  "            dir:    h\n";
		else
			config +=  "            dir:    v\n";

		//Splitter position
		snprintf(tmp, sizeof(tmp), "            split:  %d\n", split->get_position());
		config += tmp;

		//Children
		snprintf(tmp, sizeof(tmp), "            child0: %d\n", table[split->get_child1()]);
		config += tmp;
		snprintf(tmp, sizeof(tmp), "            child1: %d\n", table[split->get_child2()]);
		config += tmp;
	}

	//Top level splitter
	for(auto split : m_splitters)
	{
		if(split->get_parent() == &m_vbox)
		{
			snprintf(tmp, sizeof(tmp), "    top: %d\n", table[split]);
			config += tmp;
		}
	}

	return config;
}

/**
	@brief Serialize all waveforms for the session
 */
void OscilloscopeWindow::SerializeWaveforms(IDTable& table)
{
	lock_guard<recursive_mutex> lock(m_waveformDataMutex);

	char cwd[PATH_MAX];
	getcwd(cwd, PATH_MAX);
	chdir(m_currentDataDirName.c_str());

	const auto directories = ::Glob("scope_*", true);

	for(const auto& directory: directories)
		::RemoveDirectory(directory);

	chdir(cwd);

	//Create and show progress dialog
	FileProgressDialog progress;
	progress.show();
	float progress_per_scope = 1.0f / m_scopes.size();

	//Serialize waveforms for each of our instruments
	for(size_t i=0; i<m_scopes.size(); i++)
	{
		m_historyWindows[m_scopes[i]]->SerializeWaveforms(
			m_currentDataDirName,
			table,
			progress,
			i*progress_per_scope,
			progress_per_scope);
	}
}

void OscilloscopeWindow::OnAlphaChanged()
{
	ClearAllPersistence();
}

void OscilloscopeWindow::OnTriggerProperties(Oscilloscope* scope)
{
	//Did we have a dialog for the meter already?
	if(m_triggerPropertiesDialogs.find(scope) != m_triggerPropertiesDialogs.end())
		m_triggerPropertiesDialogs[scope]->show();

	//Need to create it
	else
	{
		auto dlg = new TriggerPropertiesDialog(this, scope);
		m_triggerPropertiesDialogs[scope] = dlg;
		dlg->show();
	}
}

void OscilloscopeWindow::OnEyeColorChanged(string color, Gtk::RadioMenuItem* item)
{
	if(!item->get_active())
		return;

	m_eyeColor = color;
	for(auto v : m_waveformAreas)
		v->queue_draw();
}

/**
	@brief Returns a list of named color ramps
 */
vector<string> OscilloscopeWindow::GetEyeColorNames()
{
	vector<string> ret;
	for(auto it : m_eyeFiles)
		ret.push_back(it.first);
	sort(ret.begin(), ret.end());
	return ret;
}

void OscilloscopeWindow::OnHistory()
{
	if(m_btnHistory.get_active())
	{
		for(auto it : m_historyWindows)
		{
			it.second->show();
			it.second->grab_focus();
		}
	}
	else
	{
		for(auto it : m_historyWindows)
			it.second->hide();
	}
}

/**
	@brief Moves a waveform to the "best" group.

	Current heuristics:
		Eye pattern:
			Always make a new group below the current one
		Otherwise:
			Move to the first group with the same X axis unit.
			If none found, move below current
 */
void OscilloscopeWindow::MoveToBestGroup(WaveformArea* w)
{
	auto stream = w->GetChannel();
	auto eye = dynamic_cast<EyePattern*>(stream.m_channel);

	if(!eye)
	{
		for(auto g : m_waveformGroups)
		{
			g->m_timeline.RefreshUnits();
			if(stream.GetXAxisUnits() == g->m_timeline.GetXAxisUnits())
			{
				OnMoveToExistingGroup(w, g);
				return;
			}
		}
	}

	OnMoveNewBelow(w);
}

void OscilloscopeWindow::OnMoveNewRight(WaveformArea* w)
{
	OnMoveNew(w, true);
}

void OscilloscopeWindow::OnMoveNewBelow(WaveformArea* w)
{
	OnMoveNew(w, false);
}

void OscilloscopeWindow::SplitGroup(Gtk::Widget* frame, WaveformGroup* group, bool horizontal)
{
	//Hierarchy is WaveformArea -> WaveformGroup waveform box -> WaveformGroup box ->
	//WaveformGroup frame -> WaveformGroup event box -> splitter
	auto split = dynamic_cast<Gtk::Paned*>(frame->get_parent());
	if(split == NULL)
	{
		LogError("parent isn't a splitter\n");
		return;
	}

	//See what the widget's current parenting situation is.
	//We might have a free splitter area free already!
	Gtk::Paned* csplit = NULL;
	if(horizontal)
		csplit = dynamic_cast<Gtk::HPaned*>(split);
	else
		csplit = dynamic_cast<Gtk::VPaned*>(split);
	if( (csplit != NULL) && (split->get_child2() == NULL) )
	{
		split->pack2(group->m_frame);
		split->show_all();
	}

	//Split the current parent
	else
	{
		//Create a new splitter
		Gtk::Paned* nsplit;
		if(horizontal)
			nsplit = new Gtk::HPaned;
		else
			nsplit = new Gtk::VPaned;
		m_splitters.emplace(nsplit);

		//Take the current frame out of the parent group so we have room for the splitter
		if(frame == split->get_child1())
		{
			split->remove(*frame);
			split->pack1(*nsplit);
		}
		else
		{
			split->remove(*frame);
			split->pack2(*nsplit);
		}

		nsplit->pack1(*frame);
		nsplit->pack2(group->m_frame);
		split->show_all();
	}
}

void OscilloscopeWindow::OnMoveNew(WaveformArea* w, bool horizontal)
{
	//Make a new group
	auto group = new WaveformGroup(this);
	group->m_pixelsPerXUnit = w->m_group->m_pixelsPerXUnit;
	m_waveformGroups.emplace(group);

	//Split the existing group and add the new group to it
	SplitGroup(w->GetGroupFrame(), group, horizontal);

	//Move the waveform into the new group
	OnMoveToExistingGroup(w, group);
}

void OscilloscopeWindow::OnCopyNew(WaveformArea* w, bool horizontal)
{
	//Make a new group
	auto group = new WaveformGroup(this);
	group->m_pixelsPerXUnit = w->m_group->m_pixelsPerXUnit;
	m_waveformGroups.emplace(group);

	//Split the existing group and add the new group to it
	SplitGroup(w->GetGroupFrame(), group, horizontal);

	//Make a copy of the current waveform view and add to that group
	OnCopyToExistingGroup(w, group);
}

void OscilloscopeWindow::OnMoveToExistingGroup(WaveformArea* w, WaveformGroup* ngroup)
{
	auto oldgroup = w->m_group;

	w->m_group = ngroup;
	w->get_parent()->remove(*w);

	if(w->GetChannel().GetType() == Stream::STREAM_TYPE_DIGITAL)
		ngroup->m_waveformBox.pack_start(*w, Gtk::PACK_SHRINK);
	else
		ngroup->m_waveformBox.pack_start(*w);

	//Move stats related to this trace to the new group
	set<StreamDescriptor> chans;
	chans.emplace(w->GetChannel());
	for(size_t i=0; i<w->GetOverlayCount(); i++)
		chans.emplace(w->GetOverlay(i));
	for(auto chan : chans)
	{
		if(oldgroup->IsShowingStats(chan))
		{
			oldgroup->DisableStats(chan);
			ngroup->EnableStats(chan);
		}
	}

	//Remove any groups that no longer have any waveform views in them,
	//or splitters that only have one child
	GarbageCollectGroups();
}

void OscilloscopeWindow::OnCopyNewRight(WaveformArea* w)
{
	OnCopyNew(w, true);
}

void OscilloscopeWindow::OnCopyNewBelow(WaveformArea* w)
{
	OnCopyNew(w, false);
}

void OscilloscopeWindow::OnCopyToExistingGroup(WaveformArea* w, WaveformGroup* ngroup)
{
	//Create a new waveform area that looks like the existing one (not an exact copy)
	WaveformArea* nw = new WaveformArea(w);
	m_waveformAreas.emplace(nw);

	//Then add it like normal
	nw->m_group = ngroup;
	if(nw->GetChannel().GetType() == Stream::STREAM_TYPE_DIGITAL)
		ngroup->m_waveformBox.pack_start(*nw, Gtk::PACK_SHRINK);
	else
		ngroup->m_waveformBox.pack_start(*nw);
	nw->show();

	//Add stats if needed
	set<StreamDescriptor> chans;
	chans.emplace(w->GetChannel());
	for(size_t i=0; i<w->GetOverlayCount(); i++)
		chans.emplace(w->GetOverlay(i));
	for(auto chan : chans)
	{
		if(w->m_group->IsShowingStats(chan))
			ngroup->EnableStats(chan);
	}
}

void OscilloscopeWindow::GarbageCollectGroups()
{
	//Remove groups with no waveforms (any attached measurements will be deleted)
	std::set<WaveformGroup*> groupsToRemove;
	for(auto g : m_waveformGroups)
	{
		if(g->m_waveformBox.get_children().empty())
			groupsToRemove.emplace(g);
	}
	for(auto g : groupsToRemove)
	{
		auto parent = g->m_frame.get_parent();
		parent->remove(g->m_frame);
		delete g;
		m_waveformGroups.erase(g);
	}

	//If a splitter only has a group in the second half, move it to the first
	for(auto s : m_splitters)
	{
		auto first = s->get_child1();
		auto second = s->get_child2();
		if( (first == NULL) && (second != NULL) )
		{
			s->remove(*second);
			s->pack1(*second);
		}
	}

	//If a splitter only has a group in the first half, move it to the parent splitter and delete it
	//(if there is one)
	set<Gtk::Paned*> splittersToRemove;
	for(auto s : m_splitters)
	{
		auto first = s->get_child1();
		auto second = s->get_child2();
		if( (first != NULL) && (second == NULL) )
		{
			//Child of another splitter, move us to it
			auto parent = s->get_parent();
			if(parent != &m_vbox)
			{
				//Move our child to the empty half of our parent
				auto pparent = dynamic_cast<Gtk::Paned*>(parent);
				if(pparent->get_child1() == s)
				{
					s->remove(*first);
					pparent->remove(*s);
					pparent->pack1(*first);
				}
				else
				{
					s->remove(*first);
					pparent->remove(*s);
					pparent->pack2(*first);
				}

				//Delete us
				splittersToRemove.emplace(s);
			}

			//If this is the top level splitter, we have no higher level to move it to
			//so no action required? or do we delete the splitter entirely and only have us in the vbox?
		}
	}

	for(auto s : splittersToRemove)
	{
		m_splitters.erase(s);
		delete s;
	}

	//Hide stat display if there's no stats in the group
	for(auto g : m_waveformGroups)
	{
		if(g->m_columnToIndexMap.empty())
			g->m_measurementView.hide();
		else
			g->m_measurementView.show_all();
	}
}

void OscilloscopeWindow::OnFullscreen()
{
	m_fullscreen = !m_fullscreen;

	//Enter fullscreen mode
	if(m_fullscreen)
	{
		//Update toolbar button icon
		m_btnFullscreen.set_icon_widget(m_iconExitFullscreen);
		m_iconExitFullscreen.show();

		int x;
		int y;
		get_position(x, y);
		m_originalRect = Gdk::Rectangle(x, y, get_width(), get_height());

		//Figure out the size we need to be in order to become fullscreen
		auto screen = get_screen();
		int mon = screen->get_monitor_at_window(get_window());
		Gdk::Rectangle rect;
		screen->get_monitor_geometry(mon, rect);

		//Make us fake-fullscreen (on top of everything else and occupying the entire monitor).
		//We can't just use Gtk::Window::fullscreen() because this messes with popup dialogs
		//like protocol analyzers.
		set_keep_above();
		set_decorated(false);
		move(rect.get_x(), rect.get_y());
		resize(rect.get_width(), rect.get_height());
	}

	//Revert to our old setup
	else
	{
		set_keep_above(false);
		set_decorated();
		resize(m_originalRect.get_width(), m_originalRect.get_height());
		move(m_originalRect.get_x(), m_originalRect.get_y());

		//Update toolbar button icon
		m_btnFullscreen.set_icon_widget(m_iconEnterFullscreen);
	}
}

void OscilloscopeWindow::OnClearSweeps()
{
	lock_guard<recursive_mutex> lock(m_waveformDataMutex);

	//TODO: clear regular waveform data and history too?

	//Clear integrated data from all pfilters
	auto filters = Filter::GetAllInstances();
	for(auto f : filters)
		f->ClearSweeps();

	//Clear persistence on all groups
	for(auto g : m_waveformGroups)
	{
		g->ClearStatistics();
		ClearPersistence(g);
	}
}

void OscilloscopeWindow::OnRefreshConfig()
{
	for(auto scope : m_scopes)
		scope->FlushConfigCache();

	//Redraw the timeline and all waveform areas to reflect anything changed from the scope
	for(auto g : m_waveformGroups)
		g->m_timeline.queue_draw();
	for(auto a : m_waveformAreas)
		a->queue_draw();

}

/**
	@brief Returns the duration of the longest waveform in the group
 */
int64_t OscilloscopeWindow::GetLongestWaveformDuration(WaveformGroup* group)
{
	auto areas = GetAreasInGroup(group);

	//Find all waveforms visible in any area within the group
	set<WaveformBase*> wfms;
	for(auto a : areas)
	{
		auto data = a->GetChannel().GetData();
		if(data != NULL)
			wfms.emplace(data);
		for(size_t i=0; i < a->GetOverlayCount(); i++)
		{
			auto o = a->GetOverlay(i);
			data = o.GetData();
			if(data != NULL)
				wfms.emplace(data);
		}
	}

	//Find how long the longest waveform is.
	//Horizontal displacement doesn't matter for now, only total length.
	int64_t duration = 0;
	for(auto w : wfms)
	{
		auto spec = dynamic_cast<SpectrogramWaveform*>(w);
		auto u = dynamic_cast<UniformWaveformBase*>(w);
		auto s = dynamic_cast<SparseWaveformBase*>(w);

		//Spectrograms need special treatment
		if(spec)
			duration = max(duration, spec->GetDuration());

		else
		{
			size_t len = w->size();
			if(len < 2)
				continue;
			size_t end = len - 1;

			int64_t delta = GetOffsetScaled(s, u, end) - GetOffsetScaled(s, u, 0);
			duration = max(duration, delta);
		}
	}

	return duration;
}

void OscilloscopeWindow::OnAutofitHorizontal(WaveformGroup* group)
{
	auto areas = GetAreasInGroup(group);

	//Figure out how wide the widest waveform in the group is, in pixels
	float width = 0;
	for(auto a : areas)
		width = max(width, a->GetPlotWidthPixels());

	auto duration = GetLongestWaveformDuration(group);

	//Change the zoom
	group->m_pixelsPerXUnit = width / duration;
	group->m_xAxisOffset = 0;

	ClearPersistence(group, false, true);
}

/**
	@brief Zoom in, keeping timestamp "target" at the same position within the group
 */
void OscilloscopeWindow::OnZoomInHorizontal(WaveformGroup* group, int64_t target)
{
	//Calculate the *current* position of the target within the window
	float delta = target - group->m_xAxisOffset;

	//Change the zoom
	float step = 1.5;
	group->m_pixelsPerXUnit *= step;
	group->m_xAxisOffset = target - (delta/step);

	ClearPersistence(group, false, true);
}

/**
	@brief Zoom out, keeping timestamp "target" at the same position within the group
 */
void OscilloscopeWindow::OnZoomOutHorizontal(WaveformGroup* group, int64_t target)
{
	//Figure out how wide the widest waveform in the group is, in X axis units
	float width = 0;
	auto areas = GetAreasInGroup(group);
	for(auto a : areas)
		width = max(width, a->GetPlotWidthXUnits());

	auto duration = GetLongestWaveformDuration(group);

	//If the view is already wider than the longest waveform, don't allow further zooming
	if(width > duration)
		return;

	//Calculate the *current* position of the target within the window
	float delta = target - group->m_xAxisOffset;

	//Change the zoom
	float step = 1.5;
	group->m_pixelsPerXUnit /= step;
	group->m_xAxisOffset = target - (delta*step);

	ClearPersistence(group, false, true);
}

vector<WaveformArea*> OscilloscopeWindow::GetAreasInGroup(WaveformGroup* group)
{
	auto children = group->m_vbox.get_children();


	vector<WaveformArea*> areas;
	for(auto w : children)
	{
		//Redraw all views in the waveform box
		auto box = dynamic_cast<Gtk::Box*>(w);
		if(box)
		{
			auto bchildren = box->get_children();
			for(auto a : bchildren)
			{
				auto area = dynamic_cast<WaveformArea*>(a);
				if(area != NULL && w->get_realized())
					areas.push_back(area);
			}
		}
	}
	return areas;
}

void OscilloscopeWindow::ClearPersistence(WaveformGroup* group, bool geometry_dirty, bool position_dirty)
{
	auto areas = GetAreasInGroup(group);

	//Mark each area as dirty and map the buffers needed for update
	for(auto w : areas)
	{
		w->CalculateOverlayPositions();
		w->ClearPersistence(false);
		w->UpdateCounts();
	}

	//Do the actual updates
	float alpha = GetTraceAlpha();
	if(geometry_dirty || position_dirty)
	{
		lock_guard<recursive_mutex> lock(m_waveformDataMutex);

		//Make the list of data to update
		vector<WaveformRenderData*> data;
		float coeff = -1;
		for(auto w : areas)
		{
			if(coeff < 0)
				coeff = w->GetPersistenceDecayCoefficient();

			w->UpdateCachedScales();
			w->GetAllRenderData(data);
		}

		//Do the updates in parallel
		#pragma omp parallel for
		for(size_t i=0; i<data.size(); i++)
			WaveformArea::PrepareGeometry(data[i], geometry_dirty, alpha, coeff);

		//Clean up
		for(auto w : areas)
		{
			w->SetNotDirty();
		}
	}

	//Submit update requests for each area (and the timeline)
	auto children = group->m_vbox.get_children();
	for(auto w : children)
		w->queue_draw();
}

void OscilloscopeWindow::ClearAllPersistence()
{
	for(auto g : m_waveformGroups)
		ClearPersistence(g, true, false);
}

void OscilloscopeWindow::OnQuit()
{
	close();
}

void OscilloscopeWindow::OnAddChannel(StreamDescriptor chan)
{
	//If we have no splitters, make one
	if(m_splitters.empty())
	{
		auto split = new Gtk::VPaned;
		m_vbox.pack_start(*split);
		m_splitters.emplace(split);
	}

	//If all waveform groups were closed, recreate one
	if(m_waveformGroups.empty())
	{
		auto split = *m_splitters.begin();
		auto group = new WaveformGroup(this);
		m_waveformGroups.emplace(group);
		split->pack1(group->m_frame);
		split->show_all();
		group->m_measurementView.hide();
	}

	auto w = DoAddChannel(chan, *m_waveformGroups.begin());
	MoveToBestGroup(w);

	RefreshTimebasePropertiesDialog();
}

void OscilloscopeWindow::RefreshTimebasePropertiesDialog()
{
	if(m_timebasePropertiesDialog)
	{
		if(m_timebasePropertiesDialog->is_visible())
			m_timebasePropertiesDialog->RefreshAll();

		else
		{
			delete m_timebasePropertiesDialog;
			m_timebasePropertiesDialog = nullptr;
		}
	}
}

WaveformArea* OscilloscopeWindow::DoAddChannel(StreamDescriptor chan, WaveformGroup* ngroup, WaveformArea* ref)
{
	//Create the viewer
	auto w = new WaveformArea(chan, this);
	w->m_group = ngroup;
	m_waveformAreas.emplace(w);

	if(chan.GetType() == Stream::STREAM_TYPE_DIGITAL)
		ngroup->m_waveformBox.pack_start(*w, Gtk::PACK_SHRINK);
	else
		ngroup->m_waveformBox.pack_start(*w);

	//Move the new trace after the reference trace, if one was provided
	if(ref != NULL)
	{
		auto children = ngroup->m_waveformBox.get_children();
		for(size_t i=0; i<children.size(); i++)
		{
			if(children[i] == ref)
				ngroup->m_waveformBox.reorder_child(*w, i+1);
		}
	}

	//Refresh the channels menu since the newly added channel might create new banking conflicts
	RefreshChannelsMenu();

	w->show();
	return w;
}

void OscilloscopeWindow::OnRemoveChannel(WaveformArea* w)
{
	//Get rid of the channel
	w->get_parent()->remove(*w);
	m_waveformAreas.erase(w);
	delete w;

	//Clean up in case it was the last channel in the group
	GarbageCollectGroups();
	RefreshFilterGraphEditor();

	RefreshTimebasePropertiesDialog();
}

void OscilloscopeWindow::GarbageCollectAnalyzers()
{
	//Check out our analyzers and see if any of them now have no references other than the analyzer window itself.
	//If the analyzer is hidden, and there's no waveform views for it, get rid of it
	set<ProtocolAnalyzerWindow*> garbage;
	for(auto a : m_analyzers)
	{
		//It's visible. Still active.
		if(a->get_visible())
			continue;

		//If there is only one reference, it's to the analyzer itself.
		//Which is hidden, so we want to get rid of it.
		auto chan = a->GetDecoder();
		if(chan->GetRefCount() == 1)
			garbage.emplace(a);
	}

	for(auto a : garbage)
	{
		m_analyzers.erase(a);
		delete a;
	}

	//Need to reload the menu in case we deleted the last reference to something
	RefreshChannelsMenu();
	RefreshAnalyzerMenu();
}

/**
	@brief Returns true if we have at least one scope that isn't offline
 */
bool OscilloscopeWindow::HasOnlineScopes()
{
	for(auto scope : m_scopes)
	{
		if(!scope->IsOffline())
			return true;
	}
	return false;
}

/**
	@brief See if we have waveforms ready to process
 */
bool OscilloscopeWindow::CheckForPendingWaveforms()
{
	//No online scopes to poll? Re-run the filter graph
	if(!HasOnlineScopes())
		return m_triggerArmed;

	//Wait for every online scope to have triggered
	for(auto scope : m_scopes)
	{
		if(scope->IsOffline())
			continue;
		if(!scope->HasPendingWaveforms())
			return false;
	}

	//Keep track of when the primary instrument triggers.
	if(m_multiScopeFreeRun)
	{
		//See when the primary triggered
		if( (m_tPrimaryTrigger < 0) && m_scopes[0]->HasPendingWaveforms() )
			m_tPrimaryTrigger = GetTime();

		//All instruments should trigger within 1 sec (arbitrary threshold) of the primary.
		//If it's been longer than that, something went wrong. Discard all pending data and re-arm the trigger.
		double twait = GetTime() - m_tPrimaryTrigger;
		if( (m_tPrimaryTrigger > 0) && ( twait > 1 ) )
		{
			LogWarning("Timed out waiting for one or more secondary instruments to trigger (%.2f ms). Resetting...\n",
				twait*1000);

			//Cancel any pending triggers
			OnStop();

			//Discard all pending waveform data
			for(auto scope : m_scopes)
			{
				//Don't touch anything offline
				if(scope->IsOffline())
					continue;

				scope->IDPing();
				scope->ClearPendingWaveforms();
			}

			//Re-arm the trigger and get back to polling
			OnStart();
			return false;
		}
	}

	//If we get here, we had waveforms on all instruments
	return true;
}

/**
	@brief Pull the waveform data out of the queue and make it current
 */
void OscilloscopeWindow::DownloadWaveforms()
{
	lock_guard<recursive_mutex> lock(m_waveformDataMutex);

	//Process the waveform data from each instrument
	for(auto scope : m_scopes)
	{
		//Don't touch anything offline
		if(scope->IsOffline())
			continue;

		//Make sure we don't free the old waveform data
		for(size_t i=0; i<scope->GetChannelCount(); i++)
		{
			auto chan = scope->GetChannel(i);
			for(size_t j=0; j<chan->GetStreamCount(); j++)
				chan->Detach(j);
		}

		//Download the data
		scope->PopPendingWaveform();
	}

	//If we're in offline one-shot mode, disarm the trigger
	if( (m_scopes.empty()) && m_triggerOneShot)
		m_triggerArmed = false;

	//In multi-scope mode, retcon the timestamps of secondary scopes' waveforms so they line up with the primary.
	if(m_scopes.size() > 1)
	{
		LogTrace("Multi scope: patching timestamps\n");
		LogIndenter li;

		//Get the timestamp of the primary scope's first waveform
		bool hit = false;
		time_t timeSec = 0;
		int64_t timeFs  = 0;
		auto prim = m_scopes[0];
		for(size_t i=0; i<prim->GetChannelCount(); i++)
		{
			auto chan = prim->GetChannel(i);
			for(size_t j=0; j<chan->GetStreamCount(); j++)
			{
				auto data = chan->GetData(j);
				if(data != nullptr)
				{
					timeSec = data->m_startTimestamp;
					timeFs = data->m_startFemtoseconds;
					hit = true;
					break;
				}
			}
			if(hit)
				break;
		}

		//Patch all secondary scopes
		for(size_t i=1; i<m_scopes.size(); i++)
		{
			auto sec = m_scopes[i];

			for(size_t j=0; j<sec->GetChannelCount(); j++)
			{
				auto chan = sec->GetChannel(j);
				for(size_t k=0; k<chan->GetStreamCount(); k++)
				{
					auto data = chan->GetData(k);
					if(data == nullptr)
						continue;

					auto skew = m_scopeDeskewCal[sec];

					data->m_startTimestamp = timeSec;
					data->m_startFemtoseconds = timeFs;
					data->m_triggerPhase -= skew;
				}
			}
		}
	}
}

/**
	@brief Handles updating things after all instruments have downloaded their new waveforms
 */
void OscilloscopeWindow::OnAllWaveformsUpdated(bool reconfiguring, bool updateFilters)
{
	lock_guard<recursive_mutex> lock(m_waveformDataMutex);

	m_totalWaveforms ++;

	//Update the status
	UpdateStatusBar();
	if(updateFilters)
		RefreshAllFilters();

	//Update protocol analyzers
	//TODO: ideal would be to delete all old packets from analyzers then update them with current ones.
	//This would allow changing settings on a protocol to update correctly.
	if(!reconfiguring)
	{
		for(auto a : m_analyzers)
			a->OnWaveformDataReady();

		for(auto a : m_scopeInfoWindows)
			a.second->OnWaveformDataReady();
	}

	//Update waveform areas.
	//Skip this if loading a file from the command line and loading isn't done
	if(WaveformArea::IsGLInitComplete())
	{
		//Map all of the buffers we need to update in each area
		for(auto w : m_waveformAreas)
		{
			w->OnWaveformDataReady();
			w->CalculateOverlayPositions();
			w->UpdateCounts();
		}

		float alpha = GetTraceAlpha();

		//Make the list of data to update (waveforms plus overlays)
		vector<WaveformRenderData*> data;
		float coeff = -1;
		for(auto w : m_waveformAreas)
		{
			w->GetAllRenderData(data);
			w->UpdateCachedScales();

			if(coeff < 0)
				coeff = w->GetPersistenceDecayCoefficient();
		}

		//Do the updates in parallel
		#pragma omp parallel for
		for(size_t i=0; i<data.size(); i++)
			WaveformArea::PrepareGeometry(data[i], true, alpha, coeff);

		//Clean up
		for(auto w : m_waveformAreas)
		{
			w->SetNotDirty();
		}

		//Submit update requests for each area
		for(auto w : m_waveformAreas)
			w->queue_draw();
	}

	if(!reconfiguring)
	{
		//Redraw timeline in case trigger config was updated during the waveform download
		for(auto g : m_waveformGroups)
			g->m_timeline.queue_draw();

		//Update the trigger sync wizard, if it's active
		if(m_scopeSyncWizard && m_scopeSyncWizard->is_visible())
			m_scopeSyncWizard->OnWaveformDataReady();

		//Check if a conditional halt applies
		int64_t timestamp;
		if(m_haltConditionsDialog.ShouldHalt(timestamp))
		{
			auto chan = m_haltConditionsDialog.GetHaltChannel();

			OnStop();

			if(m_haltConditionsDialog.ShouldMoveToHalt())
			{
				//Find the waveform area(s) for this channel
				for(auto a : m_waveformAreas)
				{
					if(a->GetChannel() == chan)
					{
						a->m_group->m_xAxisOffset = timestamp;
						a->m_group->m_frame.queue_draw();
					}

					for(size_t i=0; i<a->GetOverlayCount(); i++)
					{
						if(a->GetOverlay(i) == chan)
						{
							a->m_group->m_xAxisOffset = timestamp;
							a->m_group->m_frame.queue_draw();
						}
					}
				}
			}
		}
	}
}

void OscilloscopeWindow::RefreshAllFilters()
{
	lock_guard<recursive_mutex> lock(m_waveformDataMutex);

	SyncFilterColors();

	set<Filter*> filters;
	{
		lock_guard<mutex> lock2(m_filterUpdatingMutex);
		filters = Filter::GetAllInstances();
	}
	m_graphExecutor.RunBlocking(filters);

	//Update statistic displays after the filter graph update is complete
	for(auto g : m_waveformGroups)
		g->RefreshMeasurements();
}

void OscilloscopeWindow::RefreshAllViews()
{
	for(auto g : m_waveformGroups)
		g->m_timeline.queue_draw();
	for(auto a : m_waveformAreas)
		a->queue_draw();
}

void OscilloscopeWindow::UpdateStatusBar()
{
	char tmp[256];
	if(m_scopes.empty())
		return;

	//TODO: redo this for multiple scopes
	auto scope = m_scopes[0];
	auto trig = scope->GetTrigger();
	if(trig)
	{
		auto chan = trig->GetInput(0).m_channel;
		if(chan == NULL)
		{
			LogWarning("Trigger channel is NULL\n");
			return;
		}
		string name = chan->GetHwname();
		Unit volts(Unit::UNIT_VOLTS);
		m_triggerConfigLabel.set_label(volts.PrettyPrint(trig->GetLevel()));
	}

	//Update counters
	if(m_totalWaveforms > 0)
	{
		double fps = m_framesClock.GetAverageHz();
		snprintf(tmp, sizeof(tmp), "%zu WFMs, %.2f FPS. ", m_totalWaveforms, fps);
		m_waveformRateLabel.set_label(tmp);
	}
}

void OscilloscopeWindow::OnStart()
{
	ArmTrigger(TRIGGER_TYPE_NORMAL);
}

void OscilloscopeWindow::OnStartSingle()
{
	ArmTrigger(TRIGGER_TYPE_SINGLE);
}

void OscilloscopeWindow::OnForceTrigger()
{
	ArmTrigger(TRIGGER_TYPE_FORCED);
}

void OscilloscopeWindow::OnStop()
{
	m_multiScopeFreeRun = false;
	m_triggerArmed = false;

	for(auto scope : m_scopes)
	{
		scope->Stop();

		//Clear out any pending data (the user doesn't want it, and we don't want stale stuff hanging around)
		scope->ClearPendingWaveforms();
	}
}

void OscilloscopeWindow::ArmTrigger(TriggerType type)
{
	bool oneshot = (type == TRIGGER_TYPE_FORCED) || (type == TRIGGER_TYPE_SINGLE);
	m_triggerOneShot = oneshot;

	if(!HasOnlineScopes())
	{
		m_tArm = GetTime();
		m_triggerArmed = true;
		return;
	}

	/*
		If we have multiple scopes, always use single trigger to keep them synced.
		Multi-trigger can lead to race conditions and dropped triggers if we're still downloading a secondary
		instrument's waveform and the primary re-arms.

		Also, order of arming is critical. Secondaries must be completely armed before the primary (instrument 0) to
		ensure that the primary doesn't trigger until the secondaries are ready for the event.
	*/
	m_tPrimaryTrigger = -1;
	if(!oneshot && (m_scopes.size() > 1) )
		m_multiScopeFreeRun = true;
	else
		m_multiScopeFreeRun = false;

	//In multi-scope mode, make sure all scopes are stopped with no pending waveforms
	if(m_scopes.size() > 1)
	{
		for(ssize_t i=m_scopes.size()-1; i >= 0; i--)
		{
			if(m_scopes[i]->PeekTriggerArmed())
				m_scopes[i]->Stop();

			if(m_scopes[i]->HasPendingWaveforms())
			{
				LogWarning("Scope %s had pending waveforms before arming\n", m_scopes[i]->m_nickname.c_str());
				m_scopes[i]->ClearPendingWaveforms();
			}
		}
	}

	for(ssize_t i=m_scopes.size()-1; i >= 0; i--)
	{
		//If we have >1 scope, all secondaries always use single trigger synced to the primary's trigger output
		if(i > 0)
			m_scopes[i]->StartSingleTrigger();

		else
		{
			switch(type)
			{
				//Normal trigger: all scopes lock-step for multi scope
				//for single scope, use normal trigger
				case TRIGGER_TYPE_NORMAL:
					if(m_scopes.size() > 1)
						m_scopes[i]->StartSingleTrigger();
					else
						m_scopes[i]->Start();
					break;

				case TRIGGER_TYPE_AUTO:
					LogError("ArmTrigger(TRIGGER_TYPE_AUTO) not implemented\n");
					break;

				case TRIGGER_TYPE_SINGLE:
					m_scopes[i]->StartSingleTrigger();
					break;

				case TRIGGER_TYPE_FORCED:
					m_scopes[i]->ForceTrigger();
					break;

				default:
					break;
			}
		}

		//If we have multiple scopes, ping the secondaries to make sure the arm command went through
		if(i != 0)
		{
			double start = GetTime();

			while(!m_scopes[i]->PeekTriggerArmed())
			{
				//After 3 sec of no activity, time out
				//(must be longer than the default 2 sec socket timeout)
				double now = GetTime();
				if( (now - start) > 3)
				{
					LogWarning("Timeout waiting for scope %s to arm\n",  m_scopes[i]->m_nickname.c_str());
					m_scopes[i]->Stop();
					m_scopes[i]->StartSingleTrigger();
					start = now;
				}
			}

			//Scope is armed. Clear any garbage in the pending queue
			m_scopes[i]->ClearPendingWaveforms();
		}
	}
	m_tArm = GetTime();
	m_triggerArmed = true;
}

/**
	@brief Called when the history view selects an old waveform
 */
void OscilloscopeWindow::OnHistoryUpdated()
{
	lock_guard<recursive_mutex> lock(m_waveformDataMutex);

	//Stop triggering if we select a saved waveform
	OnStop();

	RefreshAllFilters();

	//Update the views
	for(auto w : m_waveformAreas)
	{
		if(w->get_realized())
			w->OnWaveformDataReady();
	}
	ClearAllPersistence();
}

void OscilloscopeWindow::RefreshProtocolAnalyzers()
{
	for(auto a : m_analyzers)
		a->OnWaveformDataReady();
}

/**
	@brief Remove protocol analyzer history from to a given timestamp
 */
void OscilloscopeWindow::RemoveProtocolHistoryFrom(TimePoint timestamp)
{
	for(auto a : m_analyzers)
		a->RemoveHistoryFrom(timestamp);
}

void OscilloscopeWindow::JumpToHistory(TimePoint timestamp, HistoryWindow* src)
{
	for(auto it : m_historyWindows)
	{
		if(it.second != src)
			it.second->JumpToHistory(timestamp);
	}
}

void OscilloscopeWindow::OnTimebaseSettings()
{
	if(!m_timebasePropertiesDialog)
		m_timebasePropertiesDialog = new TimebasePropertiesDialog(this, m_scopes);
	m_timebasePropertiesDialog->show();
}

/**
	@brief Shows the synchronization dialog for connecting multiple scopes.
 */
void OscilloscopeWindow::OnScopeSync()
{
	if(m_scopes.size() > 1)
	{
		//Stop triggering
		OnStop();

		//Prepare sync
		if(!m_scopeSyncWizard)
			m_scopeSyncWizard = new ScopeSyncWizard(this);

		m_scopeSyncWizard->show();
		m_syncComplete = false;
	}
}

void OscilloscopeWindow::OnSyncComplete()
{
	m_syncComplete = true;
}

/**
	@brief Propagate name changes from one channel to filters that use it as input
 */
void OscilloscopeWindow::OnChannelRenamed(OscilloscopeChannel* chan)
{
	//Check all filters to see if they use this as input
	auto filters = Filter::GetAllInstances();
	for(auto f : filters)
	{
		//If using a custom name, don't change that
		if(!f->IsUsingDefaultName())
			continue;

		for(size_t i=0; i<f->GetInputCount(); i++)
		{
			//We matched!
			if(f->GetInput(i).m_channel == chan)
			{
				f->SetDefaultName();
				OnChannelRenamed(f);
				break;
			}
		}
	}

	//Check if we have any groups that are showing stats for it
	for(auto g : m_waveformGroups)
	{
		if(g->IsShowingStats(chan))
			g->OnChannelRenamed(chan);
	}
}

/**
	@brief Shows the halt conditions dialog
 */
void OscilloscopeWindow::OnHaltConditions()
{
	m_haltConditionsDialog.show();
	m_haltConditionsDialog.RefreshChannels();
}

/**
	@brief Generate a new waveform using a filter
 */
void OscilloscopeWindow::OnGenerateFilter(string name)
{
	//need to modeless dialog
	string color = GetDefaultChannelColor(g_numDecodes);
	m_pendingGenerator = Filter::CreateFilter(name, color);

	if(m_addFilterDialog)
		delete m_addFilterDialog;
	m_addFilterDialog = new FilterDialog(this, m_pendingGenerator, NULL);
	m_addFilterDialog->show();
	m_addFilterDialog->signal_delete_event().connect(sigc::mem_fun(*this, &OscilloscopeWindow::OnGenerateDialogClosed));

	//Add initial streams
	g_numDecodes ++;
	for(size_t i=0; i<m_pendingGenerator->GetStreamCount(); i++)
		OnAddChannel(StreamDescriptor(m_pendingGenerator, i));
}

/**
	@brief Handles a filter that was updated in such a way that the stream count changed
 */
void OscilloscopeWindow::OnStreamCountChanged(Filter* filter)
{
	//Step 1: Remove any views for streams that no longer exist
	set<WaveformArea*> areasToRemove;
	for(auto w : m_waveformAreas)
	{
		auto c = w->GetChannel();
		if( (c.m_channel == filter) && (c.m_stream >= filter->GetStreamCount() ) )
			areasToRemove.emplace(w);
	}
	for(auto w : areasToRemove)
		OnRemoveChannel(w);

	//Step 2: Create views for streams that were newly created
	for(size_t i=0; i<filter->GetStreamCount(); i++)
	{
		StreamDescriptor stream(filter, i);

		//TODO: can we do this faster than O(n^2) with a hash table or something?
		//Probably a non-issue for now because number of waveform areas isn't going to be too massive given
		//limitations on available screen real estate
		bool found = false;
		for(auto w : m_waveformAreas)
		{
			if(w->GetChannel() == stream)
			{
				found = true;
				break;
			}
		}

		if(!found)
			OnAddChannel(stream);
	}
}

bool OscilloscopeWindow::OnGenerateDialogClosed(GdkEventAny* /*ignored*/)
{
	//Commit any remaining pending changes
	m_addFilterDialog->ConfigureDecoder();

	//Done with the dialog
	delete m_addFilterDialog;
	m_addFilterDialog = NULL;
	return false;
}

/**
	@brief Update the generate / import waveform menus
 */
void OscilloscopeWindow::RefreshGenerateAndImportMenu()
{
	//Remove old ones
	auto children = m_generateMenu.get_children();
	for(auto c : children)
		m_generateMenu.remove(*c);
	children = m_importMenu.get_children();
	for(auto c : children)
		m_importMenu.remove(*c);

	//Add all filters that have no inputs
	vector<string> names;
	Filter::EnumProtocols(names);
	for(auto p : names)
	{
		//Create a test filter
		auto d = Filter::CreateFilter(p, "");
		if(d->GetInputCount() == 0)
		{
			auto item = Gtk::manage(new Gtk::MenuItem(p, false));

			//Add to the generate menu if the filter name doesn't contain "import"
			if(p.find("Import") == string::npos)
				m_generateMenu.append(*item);

			//Otherwise, add to the import menu (and trim "import" off the filter name)
			else
			{
				item->set_label(p.substr(0, p.length() - strlen(" Import")));
				m_importMenu.append(*item);
			}

			item->signal_activate().connect(
				sigc::bind<string>(sigc::mem_fun(*this, &OscilloscopeWindow::OnGenerateFilter), p));
		}
		delete d;
	}
}

/**
	@brief Update the channels menu when we connect to a new instrument
 */
void OscilloscopeWindow::RefreshChannelsMenu()
{
	//Remove the old items
	auto children = m_channelsMenu.get_children();
	for(auto c : children)
		m_channelsMenu.remove(*c);

	vector<OscilloscopeChannel*> chans;

	//Add new ones
	for(auto scope : m_scopes)
	{
		for(size_t i=0; i<scope->GetChannelCount(); i++)
		{
			auto chan = scope->GetChannel(i);

			//Skip channels that can't be enabled for some reason
			if(!scope->CanEnableChannel(i))
				continue;

			//Add a menu item - but not for the external trigger(s)
			if(chan->GetType(0) != Stream::STREAM_TYPE_TRIGGER)
				chans.push_back(chan);
		}
	}

	//Add filters
	auto filters = Filter::GetAllInstances();
	for(auto f : filters)
		chans.push_back(f);

	//Create a menu item for each stream of each channel
	for(auto chan : chans)
	{
		auto nstreams = chan->GetStreamCount();
		for(size_t i=0; i<nstreams; i++)
		{
			StreamDescriptor desc(chan, i);

			auto item = Gtk::manage(new Gtk::MenuItem(desc.GetName(), false));
			item->signal_activate().connect(
				sigc::bind<StreamDescriptor>(sigc::mem_fun(*this, &OscilloscopeWindow::OnAddChannel), desc));
			m_channelsMenu.append(*item);
		}
	}

	m_channelsMenu.show_all();
}

/**
	@brief Refresh the trigger menu when we connect to a new instrument
 */
void OscilloscopeWindow::RefreshTriggerMenu()
{
	//Remove the old items
	auto children = m_setupTriggerMenu.get_children();
	for(auto c : children)
		m_setupTriggerMenu.remove(*c);

	for(auto scope : m_scopes)
	{
		auto item = Gtk::manage(new Gtk::MenuItem(scope->m_nickname, false));
		item->signal_activate().connect(
			sigc::bind<Oscilloscope*>(sigc::mem_fun(*this, &OscilloscopeWindow::OnTriggerProperties), scope));
		m_setupTriggerMenu.append(*item);
	}
}

/**
	@brief Refresh the export menu (for now, only done at startup)
 */
void OscilloscopeWindow::RefreshExportMenu()
{
	//Remove the old items
	auto children = m_exportMenu.get_children();
	for(auto c : children)
		m_exportMenu.remove(*c);

	vector<string> names;
	ExportWizard::EnumExportWizards(names);
	for(auto name : names)
	{
		auto item = Gtk::manage(new Gtk::MenuItem(name, false));
		item->signal_activate().connect(
			sigc::bind<std::string>(sigc::mem_fun(*this, &OscilloscopeWindow::OnExport), name));
		m_exportMenu.append(*item);
	}
}

/**
	@brief Update the protocol analyzer menu when we create or destroy an analyzer
 */
void OscilloscopeWindow::RefreshAnalyzerMenu()
{
	//Remove the old items
	auto children = m_windowAnalyzerMenu.get_children();
	for(auto c : children)
		m_windowAnalyzerMenu.remove(*c);

	//Add new ones
	for(auto a : m_analyzers)
	{
		auto item = Gtk::manage(new Gtk::MenuItem(a->GetDecoder()->GetDisplayName(), false));
		item->signal_activate().connect(
			sigc::bind<ProtocolAnalyzerWindow*>(sigc::mem_fun(*this, &OscilloscopeWindow::OnShowAnalyzer), a ));
		m_windowAnalyzerMenu.append(*item);
	}

	m_windowAnalyzerMenu.show_all();
}

/**
	@brief Update the multimeter menu when we load a new session
 */
void OscilloscopeWindow::RefreshMultimeterMenu()
{
	//Remove the old items
	auto children = m_windowMultimeterMenu.get_children();
	for(auto c : children)
		m_windowMultimeterMenu.remove(*c);

	//Add new stuff
	//TODO: support pure multimeters
	for(auto scope : m_scopes)
	{
		// May be a Multimeter instance (because the driver supports it) but the instance may not support it
		if (!(scope->GetInstrumentTypes() & Instrument::INST_DMM))
			continue;

		auto meter = dynamic_cast<Multimeter*>(scope);
		if(!meter)
			continue;

		auto item = Gtk::manage(new Gtk::MenuItem(meter->m_nickname, false));
		item->signal_activate().connect(
			sigc::bind<Multimeter*>(sigc::mem_fun(*this, &OscilloscopeWindow::OnShowMultimeter), meter ));
		m_windowMultimeterMenu.append(*item);
	}
}

/**
	@brief Update the scope info menu when we load a new session
 */
void OscilloscopeWindow::RefreshScopeInfoMenu()
{
	//Remove the old items
	auto children = m_windowScopeInfoMenu.get_children();
	for(auto c : children)
		m_windowScopeInfoMenu.remove(*c);

	//Add new stuff
	for(auto scope : m_scopes)
	{
		auto item = Gtk::manage(new Gtk::MenuItem(scope->m_nickname, false));
		item->signal_activate().connect(
			sigc::bind<Oscilloscope*>(sigc::mem_fun(*this, &OscilloscopeWindow::OnShowScopeInfo), scope ));
		m_windowScopeInfoMenu.append(*item);
	}
}

void OscilloscopeWindow::OnShowAnalyzer(ProtocolAnalyzerWindow* window)
{
	window->show();
}

void OscilloscopeWindow::OnShowMultimeter(Multimeter* meter)
{
	//Did we have a dialog for the meter already?
	if(m_meterDialogs.find(meter) != m_meterDialogs.end())
		m_meterDialogs[meter]->show();

	//Need to create it
	else
	{
		auto dlg = new MultimeterDialog(meter, this);
		m_meterDialogs[meter] = dlg;
		dlg->show();
	}
}

void OscilloscopeWindow::OnShowScopeInfo(Oscilloscope* scope)
{
	//Did we have a dialog for the meter already?
	if(m_scopeInfoWindows.find(scope) != m_scopeInfoWindows.end())
		m_scopeInfoWindows[scope]->show();

	//Need to create it
	else
	{
		auto dlg = new ScopeInfoWindow(this, scope);
		m_scopeInfoWindows[scope] = dlg;
		dlg->show();
	}
}

bool OscilloscopeWindow::on_key_press_event(GdkEventKey* key_event)
{
	//Hotkeys for various special commands.
	//TODO: make this configurable

	switch(key_event->keyval)
	{
		case GDK_KEY_TouchpadToggle:
			OnStartSingle();
			break;

		case GDK_KEY_F20:
			OnStart();
			break;

		case GDK_KEY_F21:
			OnStartSingle();
			break;

		case GDK_KEY_F22:
			OnForceTrigger();
			break;

		case GDK_KEY_F23:
			OnStop();
			break;

		case GDK_KEY_F24:
			OnTimebaseSettings();
			break;

		default:
			break;
	}

	//Forward events to WaveformArea under the cursor
	//FIXME: shouldn't be necessary in gtk4
	int x;
	int y;
	for(auto w : m_waveformAreas)
	{
		translate_coordinates(*w, m_cursorX, m_cursorY, x, y);

		if( (x < 0) || (y < 0) )
			continue;
		if( (x >= w->GetWidth()) || (y >= w->GetHeight() ) )
			continue;

		w->on_key_press_event(key_event);
		break;
	}

	return false;
}

bool OscilloscopeWindow::on_motion_notify_event(GdkEventMotion* event)
{
	m_cursorX = event->x;
	m_cursorY = event->y;
	return false;
}

/**
	@brief Runs an export wizard
 */
void OscilloscopeWindow::OnExport(string format)
{
	//Stop triggering
	OnStop();

	//Make a list of all the channels (both scope channels and filters)
	vector<OscilloscopeChannel*> channels;
	auto filters = Filter::GetAllInstances();
	for(auto f : filters)
		channels.push_back(f);
	for(auto scope : m_scopes)
	{
		for(size_t i=0; i<scope->GetChannelCount(); i++)
			channels.push_back(scope->GetChannel(i));
	}

	//If we already have an export wizard, get rid of it
	if(m_exportWizard)
		delete m_exportWizard;

	//Run the actual wizard once we have a list of all channels we might want to export
	m_exportWizard = ExportWizard::CreateExportWizard(format, channels);
	m_exportWizard->show();
}

void OscilloscopeWindow::OnAboutDialog()
{
	Gtk::AboutDialog aboutDialog;

	aboutDialog.set_logo_default();
	aboutDialog.set_version(string("Version ") + GLSCOPECLIENT_VERSION);
	aboutDialog.set_copyright("Copyright © 2012-2022 Andrew D. Zonenberg and contributors");
	aboutDialog.set_license(
		"Redistribution and use in source and binary forms, with or without modification, "
		"are permitted provided that the following conditions are met:\n\n"
		"* Redistributions of source code must retain the above copyright notice, this list "
		"of conditions, and the following disclaimer.\n\n"
		"* Redistributions in binary form must reproduce the above copyright notice, this list "
		"of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.\n\n"
		"* Neither the name of the author nor the names of any contributors may be used to "
		"endorse or promote products derived from this software without specific prior written permission.\n\n"
		"THIS SOFTWARE IS PROVIDED BY THE AUTHORS \"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED "
		"TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL "
		"THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES "
		"(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR "
		"BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT "
		"(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE "
		"POSSIBILITY OF SUCH DAMAGE.    "
	);
	aboutDialog.set_wrap_license(true);

	vector<Glib::ustring> authors
	{
		"9names",
		"Andres Manelli",
		"Andrew D. Zonenberg",
		"antikerneldev",
		"Benjamin Vernoux",
		"Dave Marples",
		"four0four",
		"Francisco Sedano",
		"Katharina B",
		"Kenley Cheung",
		"Mike Walters",
		"noopwafel",
		"Pepijn De Vos",
		"pd0wm"
		"randomplum",
		"rqou",
		"RX14",
		"sam210723",
		"smunaut",
		"tarunik",
		"Tom Verbeuere",
		"whitequark",
		"x44203"
	};
	aboutDialog.set_authors(authors);

	vector<Glib::ustring> artists
	{
		"Collateral Damage Studios"
	};
	aboutDialog.set_artists(artists);

	vector<Glib::ustring> hardware
	{
		"Andrew D. Zonenberg",
		"whitequark",
		"and several anonymous donors"
	};
	aboutDialog.add_credit_section("Hardware Contributions", hardware);

	aboutDialog.set_website("https://www.github.com/azonenberg/scopehal-apps");
	aboutDialog.set_website_label("Visit us on GitHub");

	aboutDialog.run();
}

void OscilloscopeWindow::OnFilterGraph()
{
	if(!m_graphEditor)
	{
		m_graphEditor = new FilterGraphEditor(this);
		m_graphEditor->Refresh();
		m_graphEditor->show();
	}
	else if(m_graphEditor->is_visible())
		m_graphEditor->hide();
	else
	{
		m_graphEditor->Refresh();
		m_graphEditor->show();
	}
}

void OscilloscopeWindow::LoadRecentInstrumentList()
{
	try
	{
		auto docs = YAML::LoadAllFromFile(m_preferences.GetConfigDirectory() + "/recent.yml");
		if(docs.empty())
			return;
		auto node = docs[0];

		for(auto it : node)
		{
			auto inst = it.second;
			m_recentInstruments[inst["path"].as<string>()] = inst["timestamp"].as<long long>();
		}
	}
	catch(const YAML::BadFile& ex)
	{
		LogDebug("Unable to open recently used instruments file\n");
		return;
	}

}

void OscilloscopeWindow::SaveRecentInstrumentList()
{
	auto path = m_preferences.GetConfigDirectory() + "/recent.yml";
	FILE* fp = fopen(path.c_str(), "w");

	for(auto it : m_recentInstruments)
	{
		auto nick = it.first.substr(0, it.first.find(":"));
		fprintf(fp, "%s:\n", nick.c_str());
		fprintf(fp, "    path: \"%s\"\n", it.first.c_str());
		fprintf(fp, "    timestamp: %ld\n", it.second);
	}

	fclose(fp);
}

void OscilloscopeWindow::AddCurrentToRecentInstrumentList()
{
	//Add our current entry to the recently-used list
	auto now = time(NULL);

	set<SCPIInstrument*> devices;
	for(auto scope : m_scopes)
		devices.emplace(dynamic_cast<SCPIInstrument*>(scope));
	for(auto gen : m_funcgens)
		devices.emplace(dynamic_cast<SCPIInstrument*>(gen));
	for(auto meter : m_meters)
		devices.emplace(dynamic_cast<SCPIInstrument*>(meter));

	for(auto inst : devices)
	{
		if(inst == nullptr)
			continue;

		auto connectionString =
			inst->m_nickname + ":" +
			inst->GetDriverName() + ":" +
			inst->GetTransportName() + ":" +
			inst->GetTransportConnectionString();

		m_recentInstruments[connectionString] = now;
	}

	//Delete anything old
	const int maxRecentInstruments = 15;
	while(m_recentInstruments.size() > maxRecentInstruments)
	{
		string oldestPath = "";
		time_t oldestTime = now;

		for(auto it : m_recentInstruments)
		{
			if(it.second < oldestTime)
			{
				oldestTime = it.second;
				oldestPath = it.first;
			}
		}

		m_recentInstruments.erase(oldestPath);
	}
}

void OscilloscopeWindow::RefreshInstrumentMenus()
{
	//Remove the old items
	auto children = m_addScopeMenu.get_children();
	for(auto c : children)
		m_addScopeMenu.remove(*c);
	children = m_addMultimeterMenu.get_children();
	for(auto c : children)
		m_addMultimeterMenu.remove(*c);

	//Make a reverse mapping
	std::map<time_t, vector<string> > reverseMap;
	for(auto it : m_recentInstruments)
		reverseMap[it.second].push_back(it.first);

	//Deduplicate timestamps
	set<time_t> timestampsDeduplicated;
	for(auto it : m_recentInstruments)
		timestampsDeduplicated.emplace(it.second);

	//Sort the list by most recent
	vector<time_t> timestamps;
	for(auto t : timestampsDeduplicated)
		timestamps.push_back(t);
	std::sort(timestamps.begin(), timestamps.end());

	//Find all scope drivers
	vector<string> scopedrivers;
	Oscilloscope::EnumDrivers(scopedrivers);
	set<string> scopedriverset;
	for(auto s : scopedrivers)
		scopedriverset.emplace(s);

	//Find all multimeter drivers
	vector<string> meterdrivers;
	SCPIMultimeter::EnumDrivers(meterdrivers);
	set<string> meterdriverset;
	for(auto s : meterdrivers)
		meterdriverset.emplace(s);

	//Add "connect" menu items stuff
	auto item = Gtk::manage(new Gtk::MenuItem("Connect...", false));
	item->signal_activate().connect(
		sigc::mem_fun(*this, &OscilloscopeWindow::OnAddMultimeter));
	m_addMultimeterMenu.append(*item);
	item = Gtk::manage(new Gtk::SeparatorMenuItem);
	m_addMultimeterMenu.append(*item);

	item = Gtk::manage(new Gtk::MenuItem("Connect...", false));
	item->signal_activate().connect(
		sigc::mem_fun(*this, &OscilloscopeWindow::OnAddOscilloscope));
	m_addScopeMenu.append(*item);
	item = Gtk::manage(new Gtk::SeparatorMenuItem);
	m_addScopeMenu.append(*item);

	//Add new ones
	for(int i=timestamps.size()-1; i>=0; i--)
	{
		auto t = timestamps[i];
		auto paths = reverseMap[t];
		for(auto path : paths)
		{
			auto fields = explode(path, ':');
			auto nick = fields[0];
			auto drivername = fields[1];

			item = Gtk::manage(new Gtk::MenuItem(nick, false));

			//Add to recent scopes menu iff it's a scope
			if(scopedriverset.find(drivername) != scopedriverset.end())
			{
				item->signal_activate().connect(
					sigc::bind<std::string>(sigc::mem_fun(*this, &OscilloscopeWindow::ConnectToScope), path));
				m_addScopeMenu.append(*item);
			}

			//Add to recent meters menu iff it's a meter
			if(meterdriverset.find(drivername) != meterdriverset.end())
			{
				item->signal_activate().connect(
					sigc::bind<std::string>(sigc::mem_fun(*this, &OscilloscopeWindow::ConnectToMultimeter), path));
				m_addMultimeterMenu.append(*item);
			}
		}
	}

	m_addScopeMenu.show_all();
	m_addMultimeterMenu.show_all();

	//Enable or disable the sync wizard menu item depending on how many instruments we have
	m_setupSyncMenuItem.set_sensitive(m_scopes.size() > 1);
}

/**
	@brief Search our set of oscilloscopes to see which ones have function generator capability
 */
void OscilloscopeWindow::FindScopeFuncGens()
{
	for(auto scope : m_scopes)
	{
		if(!(scope->GetInstrumentTypes() & Instrument::INST_FUNCTION))
			continue;
		m_funcgens.push_back(dynamic_cast<FunctionGenerator*>(scope));
	}
}

/**
	@brief Refresh the menu of available signal generators
 */
void OscilloscopeWindow::RefreshGeneratorsMenu()
{
	//Remove the old items
	auto children = m_windowGeneratorMenu.get_children();
	for(auto c : children)
		m_windowGeneratorMenu.remove(*c);

	//Add new stuff
	for(auto gen : m_funcgens)
	{
		auto item = Gtk::manage(new Gtk::MenuItem(gen->m_nickname, false));
		item->signal_activate().connect(
			sigc::bind<FunctionGenerator*>(sigc::mem_fun(*this, &OscilloscopeWindow::OnShowFunctionGenerator), gen));
		m_windowGeneratorMenu.append(*item);
	}
}

void OscilloscopeWindow::OnShowFunctionGenerator(FunctionGenerator* gen)
{
	//Did we have a dialog for the meter already?
	if(m_functionGeneratorDialogs.find(gen) != m_functionGeneratorDialogs.end())
		m_functionGeneratorDialogs[gen]->show();

	//Need to create it
	else
	{
		auto dlg = new FunctionGeneratorDialog(gen);
		m_functionGeneratorDialogs[gen] = dlg;
		dlg->show();
	}
}

void OscilloscopeWindow::OnAddMultimeter()
{
	MultimeterConnectionDialog dlg;
	while(true)
	{
		if(dlg.run() != Gtk::RESPONSE_OK)
			return;

		if(!dlg.ValidateConfig())
		{
			Gtk::MessageDialog mdlg(
				"Invalid configuration specified.\n"
				"\n"
				"A driver and transport must always be selected.\n",
				false,
				Gtk::MESSAGE_ERROR,
				Gtk::BUTTONS_OK,
				true);
			mdlg.run();
		}

		else
			break;
	}

	ConnectToMultimeter(dlg.GetConnectionString());
}

SCPITransport* OscilloscopeWindow::ConnectToTransport(const string& name, const string& args)
{
	//Create the transport
	auto transport = SCPITransport::CreateTransport(name, args);
	if(transport == NULL)
		return NULL;

	//Check if the transport failed to initialize
	if(!transport->IsConnected())
	{
		Gtk::MessageDialog dlg(
			string("Failed to connect to instrument using transport ") + name + " and arguments " + args,
			false,
			Gtk::MESSAGE_ERROR,
			Gtk::BUTTONS_OK,
			true);
		dlg.run();

		delete transport;
		transport = NULL;
	}

	return transport;
}

void OscilloscopeWindow::ConnectToMultimeter(string path)
{
	//Format: name:driver:transport:args
	char nick[128];
	char driver[128];
	char trans[128];
	char args[128];
	if(4 != sscanf(path.c_str(), "%127[^:]:%127[^:]:%127[^:]:%127s", nick, driver, trans, args))
	{
		args[0] = '\0';
		if(3 != sscanf(path.c_str(), "%127[^:]:%127[^:]:%127[^:]", nick, driver, trans))
		{
			LogError("Invalid scope string %s\n", path.c_str());
			return;
		}
	}

	//Create the transport
	auto transport = ConnectToTransport(trans, args);
	if(!transport)
		return;

	//Create the meter
	auto meter = SCPIMultimeter::CreateMultimeter(driver, transport);
	meter->m_nickname = nick;

	//Done making the meter, add it everywhere we need it to be
	m_meters.push_back(meter);
	OnShowMultimeter(meter);
	RefreshMultimeterMenu();
	RefreshScpiConsoleMenu();
	RefreshChannelsMenu();
	AddCurrentToRecentInstrumentList();
	SaveRecentInstrumentList();
	RefreshInstrumentMenus();
	SetTitle();
}

void OscilloscopeWindow::RefreshScpiConsoleMenu()
{
	//Remove the old items
	auto children = m_windowScpiConsoleMenu.get_children();
	for(auto c : children)
		m_windowScpiConsoleMenu.remove(*c);

	//Make a list of all known instruments
	set<SCPIDevice*> devices;
	for(auto g : m_meters)
		devices.emplace(dynamic_cast<SCPIDevice*>(g));
	for(auto g : m_scopes)
		devices.emplace(dynamic_cast<SCPIDevice*>(g));
	for(auto g : m_funcgens)
		devices.emplace(dynamic_cast<SCPIDevice*>(g));

	//Add new stuff
	for(auto d : devices)
	{
		auto inst = dynamic_cast<Instrument*>(d);
		if(inst == nullptr)
			continue;

		auto item = Gtk::manage(new Gtk::MenuItem(inst->m_nickname, false));
		item->signal_activate().connect(
			sigc::bind<SCPIDevice*>(sigc::mem_fun(*this, &OscilloscopeWindow::OnShowSCPIConsole), d));
		m_windowScpiConsoleMenu.append(*item);
	}

	m_windowScpiConsoleMenu.show_all();
}

void OscilloscopeWindow::OnShowSCPIConsole(SCPIDevice* device)
{
	if(m_scpiConsoleDialogs[device] == nullptr)
		m_scpiConsoleDialogs[device] = new SCPIConsoleDialog(device);

	m_scpiConsoleDialogs[device]->show();
}

void OscilloscopeWindow::RemoveMarkersFrom(TimePoint timestamp)
{
	auto& markers = m_markers[timestamp];
	for(auto m : markers)
		delete m;

	m_markers.erase(timestamp);
}

void OscilloscopeWindow::AddMarker(TimePoint timestamp, int64_t offset)
{
	string mname = string("M") + to_string(m_nextMarker);
	m_nextMarker ++;
	AddMarker(timestamp, offset, mname);
}

void OscilloscopeWindow::AddMarker(TimePoint timestamp, int64_t offset, const string& name)
{
	//Make the marker
	auto m = new Marker(timestamp, offset, name);
	m_markers[timestamp].push_back(m);

	//Add the point to each history window
	for(auto w : m_historyWindows)
		w.second->AddMarker(timestamp, offset, name, m);

	//Redraw viewports with the new marker
	RefreshAllViews();
}

void OscilloscopeWindow::OnMarkerMoved(Marker* m)
{
	for(auto it : m_historyWindows)
		it.second->OnMarkerMoved(m);
}

void OscilloscopeWindow::DeleteMarker(Marker* m)
{
	//Remove it from all history windows
	for(auto it : m_historyWindows)
		it.second->OnMarkerDeleted(m);

	//Get rid of it
	auto& markers = m_markers[m->m_point];
	for(size_t i=0; i<markers.size(); i++)
	{
		if(markers[i] == m)
		{
			delete m;
			markers.erase(markers.begin() + i);
			break;
		}
	}

	//Redraw viewports with the new marker
	RefreshAllViews();
}

void OscilloscopeWindow::JumpToMarker(Marker* m)
{
	//Check each group to see if it's showing waveforms from the marker's timestamp
	for(auto g : m_waveformGroups)
	{
		auto data = g->GetFirstChannel().GetData();
		if(data == nullptr)
			continue;
		if(TimePoint(data->m_startTimestamp, data->m_startFemtoseconds) != m->m_point)
			continue;

		g->GetFirstArea()->CenterMarker(m->m_offset);
	}
}

void OscilloscopeWindow::OnMarkerNameChanged(Marker* m)
{
	for(auto it : m_historyWindows)
		it.second->OnMarkerNameChanged(m);
}

void OscilloscopeWindow::OnTriggerOffsetChanged(Oscilloscope* scope, int64_t oldpos, int64_t newpos)
{
	//Skip if unchanged
	if(oldpos == newpos)
		return;

	//Nothing to do unless we're in a multiscope setup
	size_t nscopes = m_scopes.size();
	if(nscopes <= 1)
		return;

	int64_t delta = newpos - oldpos;

	//If this is the primary, shift all secondaries to compensate
	if(scope == m_scopes[0])
	{
		for(size_t i=1; i<nscopes; i++)
			m_scopeDeskewCal[m_scopes[i]] -= delta;
	}

	//If this is a secondary, shift our waveform
	else
		m_scopeDeskewCal[scope] += delta;
}

void OscilloscopeWindow::RefreshRecentFileMenu()
{
	SaveRecentFileList();

	//Remove the old items
	auto children =  m_fileRecentMenu.get_children();
	for(auto c : children)
		m_fileRecentMenu.remove(*c);

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
	int nleft = m_preferences.GetInt("Files.max_recent_files");
	for(auto t : timestamps)
	{
		auto paths = reverseMap[t];
		for(auto path : paths)
		{
			auto item = Gtk::manage(new Gtk::MenuItem(path, false));
			m_fileRecentMenu.append(*item);

			auto menu = Gtk::manage(new Gtk::Menu);
			item->set_submenu(*menu);

			item = Gtk::manage(new Gtk::MenuItem("Open Online", false));
			item->signal_activate().connect(
				sigc::bind<std::string, bool, bool>(sigc::mem_fun(*this, &OscilloscopeWindow::DoFileOpen), path, true, true));
			menu->append(*item);

			item = Gtk::manage(new Gtk::MenuItem("Open Offline", false));
			item->signal_activate().connect(
				sigc::bind<std::string, bool, bool>(sigc::mem_fun(*this, &OscilloscopeWindow::DoFileOpen), path, true, false));
			menu->append(*item);
		}

		nleft --;
		if(nleft == 0)
			break;
	}

	m_fileRecentMenu.show_all();
}

void OscilloscopeWindow::SaveRecentFileList()
{
	auto path = m_preferences.GetConfigDirectory() + "/recentfiles.yml";
	FILE* fp = fopen(path.c_str(), "w");

	int j = 0;

	//Remove the old items
	auto children =  m_fileRecentMenu.get_children();
	for(auto c : children)
		m_fileRecentMenu.remove(*c);

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
	int nleft = m_preferences.GetInt("Files.max_recent_files");
	for(auto t : timestamps)
	{
		auto paths = reverseMap[t];
		for(auto fpath : paths)
		{
			string escpath = str_replace("\\", "\\\\", fpath);
			fprintf(fp, "file%d:\n", j);
			fprintf(fp, "    path: \"%s\"\n", escpath.c_str());
			fprintf(fp, "    timestamp: %ld\n", t);
			j++;
		}

		nleft --;
		if(nleft == 0)
			break;
	}

	fclose(fp);
}

void OscilloscopeWindow::LoadRecentFileList()
{
	try
	{
		auto docs = YAML::LoadAllFromFile(m_preferences.GetConfigDirectory() + "/recentfiles.yml");
		if(docs.empty())
			return;
		auto node = docs[0];

		for(auto it : node)
		{
			auto inst = it.second;
			m_recentFiles[inst["path"].as<string>()] = inst["timestamp"].as<long long>();
		}
	}
	catch(const YAML::BadFile& ex)
	{
		LogDebug("Unable to open recently used files list (bad file)\n");
		return;
	}
	catch(const YAML::ParserException& ex)
	{
		LogDebug("Unable to open recently used files list (parser exception)\n");
		return;
	}

}
