/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2020 Andrew D. Zonenberg                                                                          *
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
#include "../scopehal/Instrument.h"
#include "../scopehal/MockOscilloscope.h"
#include "OscilloscopeWindow.h"
#include "PreferenceDialog.h"
#include "TriggerPropertiesDialog.h"
#include "TimebasePropertiesDialog.h"
#include <unistd.h>
#include <fcntl.h>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#endif

using namespace std;


////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

/**
	@brief Initializes the main window
 */
OscilloscopeWindow::OscilloscopeWindow(vector<Oscilloscope*> scopes, bool nodigital)
	: m_scopes(scopes)
	, m_fullscreen(false)
	, m_multiScopeFreeRun(false)
	, m_scopeSyncWizard(NULL)
{
	SetTitle();

	//Initial setup
	set_reallocate_redraws(true);
	set_default_size(1280, 800);

	//Add widgets
	CreateWidgets(nodigital);

	ArmTrigger(false);
	m_toggleInProgress = false;

	m_tLastFlush = GetTime();

	m_eyeColor = EYE_CRT;

	m_tAcquire = 0;
	m_tDecode = 0;
	m_tView = 0;
	m_tHistory = 0;
	m_tPoll = 0;
	m_totalWaveforms = 0;
	m_syncComplete = false;

	//Start a timer for polling for scope updates
	//TODO: can we use signals of some sort to avoid busy polling until a trigger event?
	sigc::slot<bool> slot = sigc::bind(sigc::mem_fun(*this, &OscilloscopeWindow::OnTimer), 1);
	sigc::connection conn = Glib::signal_timeout().connect(slot, 5);
}

void OscilloscopeWindow::SetTitle()
{
	//Set title
	string title = "Oscilloscope: ";
	for(size_t i=0; i<m_scopes.size(); i++)
	{
		auto scope = m_scopes[i];

		char tt[256];
		snprintf(tt, sizeof(tt), "%s (%s %s, serial %s)",
			scope->m_nickname.c_str(),
			scope->GetVendor().c_str(),
			scope->GetName().c_str(),
			scope->GetSerial().c_str()
			);

		if(i > 0)
			title += ", ";
		title += tt;

		if(dynamic_cast<MockOscilloscope*>(scope) != NULL)
			title += "[OFFLINE]";
	}

	set_title(title);
}

/**
	@brief Application cleanup
 */
OscilloscopeWindow::~OscilloscopeWindow()
{
	//Print stats
	LogDebug("ACQUIRE: %10.3f ms\n", m_tAcquire * 1000);
	LogDebug("DECODE:  %10.3f ms\n", m_tDecode * 1000);
	LogDebug("VIEW:    %10.3f ms\n", m_tView * 1000);
	LogDebug("HISTORY: %10.3f ms\n", m_tHistory * 1000);
	LogDebug("POLL:    %10.3f ms\n", m_tPoll * 1000);
}

/**
	@brief Helper function for creating widgets and setting up signal handlers
 */
void OscilloscopeWindow::CreateWidgets(bool nodigital)
{
	//Set up window hierarchy
	add(m_vbox);
		m_vbox.pack_start(m_menu, Gtk::PACK_SHRINK);
			m_menu.append(m_fileMenuItem);
				m_fileMenuItem.set_label("File");
				m_fileMenuItem.set_submenu(m_fileMenu);
					Gtk::MenuItem* item = Gtk::manage(new Gtk::MenuItem("Save Layout Only", false));
					item->signal_activate().connect(
						sigc::bind<bool, bool, bool>(
							sigc::mem_fun(*this, &OscilloscopeWindow::OnFileSave),
							true, true, false));
					m_fileMenu.append(*item);
					item = Gtk::manage(new Gtk::MenuItem("Save Layout Only As...", false));
					item->signal_activate().connect(
						sigc::bind<bool, bool, bool>(
							sigc::mem_fun(*this, &OscilloscopeWindow::OnFileSave),
							false, true, false));
					m_fileMenu.append(*item);
					item = Gtk::manage(new Gtk::MenuItem("Save Layout and Waveforms", false));
					item->signal_activate().connect(
						sigc::bind<bool, bool, bool>(
							sigc::mem_fun(*this, &OscilloscopeWindow::OnFileSave),
							true, true, true));
					m_fileMenu.append(*item);
					item = Gtk::manage(new Gtk::MenuItem("Save Layout and Waveforms As...", false));
					item->signal_activate().connect(
						sigc::bind<bool, bool, bool>(
							sigc::mem_fun(*this, &OscilloscopeWindow::OnFileSave),
							false, true, true));
					m_fileMenu.append(*item);
					item = Gtk::manage(new Gtk::SeparatorMenuItem);
					m_fileMenu.append(*item);
					item = Gtk::manage(new Gtk::MenuItem("Open...", false));
					item->signal_activate().connect(
						sigc::mem_fun(*this, &OscilloscopeWindow::OnFileOpen));
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
						m_viewEyeColorMenu.append(m_eyeColorCrtItem);
							m_eyeColorCrtItem.set_label("CRT");
							m_eyeColorCrtItem.set_group(m_eyeColorGroup);
							m_eyeColorCrtItem.signal_activate().connect(
								sigc::bind<OscilloscopeWindow::EyeColor, Gtk::RadioMenuItem*>(
									sigc::mem_fun(*this, &OscilloscopeWindow::OnEyeColorChanged),
									OscilloscopeWindow::EYE_CRT,
									&m_eyeColorCrtItem));
						m_viewEyeColorMenu.append(m_eyeColorGrayscaleItem);
							m_eyeColorGrayscaleItem.set_label("Grayscale");
							m_eyeColorGrayscaleItem.set_group(m_eyeColorGroup);
							m_eyeColorGrayscaleItem.signal_activate().connect(
								sigc::bind<OscilloscopeWindow::EyeColor, Gtk::RadioMenuItem*>(
									sigc::mem_fun(*this, &OscilloscopeWindow::OnEyeColorChanged),
									OscilloscopeWindow::EYE_GRAYSCALE,
									&m_eyeColorGrayscaleItem));
						m_viewEyeColorMenu.append(m_eyeColorIronbowItem);
							m_eyeColorIronbowItem.set_label("Ironbow");
							m_eyeColorIronbowItem.set_group(m_eyeColorGroup);
							m_eyeColorIronbowItem.signal_activate().connect(
								sigc::bind<OscilloscopeWindow::EyeColor, Gtk::RadioMenuItem*>(
									sigc::mem_fun(*this, &OscilloscopeWindow::OnEyeColorChanged),
									OscilloscopeWindow::EYE_IRONBOW,
									&m_eyeColorIronbowItem));
						m_viewEyeColorMenu.append(m_eyeColorKRainItem);
							m_eyeColorKRainItem.set_label("KRain");
							m_eyeColorKRainItem.set_group(m_eyeColorGroup);
							m_eyeColorKRainItem.signal_activate().connect(
								sigc::bind<OscilloscopeWindow::EyeColor, Gtk::RadioMenuItem*>(
									sigc::mem_fun(*this, &OscilloscopeWindow::OnEyeColorChanged),
									OscilloscopeWindow::EYE_KRAIN,
									&m_eyeColorKRainItem));
						m_viewEyeColorMenu.append(m_eyeColorRainbowItem);
							m_eyeColorRainbowItem.set_label("Rainbow");
							m_eyeColorRainbowItem.set_group(m_eyeColorGroup);
							m_eyeColorRainbowItem.signal_activate().connect(
								sigc::bind<OscilloscopeWindow::EyeColor, Gtk::RadioMenuItem*>(
									sigc::mem_fun(*this, &OscilloscopeWindow::OnEyeColorChanged),
									OscilloscopeWindow::EYE_RAINBOW,
									&m_eyeColorRainbowItem));
						m_viewEyeColorMenu.append(m_eyeColorViridisItem);
							m_eyeColorViridisItem.set_label("Viridis");
							m_eyeColorViridisItem.set_group(m_eyeColorGroup);
							m_eyeColorViridisItem.signal_activate().connect(
								sigc::bind<OscilloscopeWindow::EyeColor, Gtk::RadioMenuItem*>(
									sigc::mem_fun(*this, &OscilloscopeWindow::OnEyeColorChanged),
									OscilloscopeWindow::EYE_VIRIDIS,
									&m_eyeColorViridisItem));
			m_menu.append(m_channelsMenuItem);
				m_channelsMenuItem.set_label("Add");
				m_channelsMenuItem.set_submenu(m_channelsMenu);
		m_vbox.pack_start(m_toolbox, Gtk::PACK_SHRINK);
			m_vbox.get_style_context()->add_class("toolbar");
			m_toolbox.pack_start(m_toolbar, Gtk::PACK_EXPAND_WIDGET);
				m_toolbar.append(m_btnStart, sigc::mem_fun(*this, &OscilloscopeWindow::OnStart));
					m_btnStart.set_tooltip_text("Start (normal trigger)");
					m_btnStart.set_icon_name("media-playback-start");
				m_toolbar.append(m_btnStartSingle, sigc::mem_fun(*this, &OscilloscopeWindow::OnStartSingle));
					m_btnStartSingle.set_tooltip_text("Start (single trigger)");
					m_btnStartSingle.set_icon_name("media-skip-forward");
				m_toolbar.append(m_btnStop, sigc::mem_fun(*this, &OscilloscopeWindow::OnStop));
					m_btnStop.set_tooltip_text("Stop trigger");
					m_btnStop.set_icon_name("media-playback-stop");
				m_toolbar.append(*Gtk::manage(new Gtk::SeparatorToolItem));
				m_toolbar.append(m_btnHistory, sigc::mem_fun(*this, &OscilloscopeWindow::OnHistory));
					m_btnHistory.set_tooltip_text("History");
					m_btnHistory.set_icon_name("search");
				m_toolbar.append(*Gtk::manage(new Gtk::SeparatorToolItem));
				m_toolbar.append(m_btnRefresh, sigc::mem_fun(*this, &OscilloscopeWindow::OnRefreshConfig));
					m_btnRefresh.set_tooltip_text("Reload configuration from scope");
					m_btnRefresh.set_icon_name("reload");
				m_toolbar.append(m_btnClearSweeps, sigc::mem_fun(*this, &OscilloscopeWindow::OnClearSweeps));
					m_btnClearSweeps.set_tooltip_text("Clear sweeps");
					m_btnClearSweeps.set_icon_name("user-trash");
				m_toolbar.append(m_btnFullscreen, sigc::mem_fun(*this, &OscilloscopeWindow::OnFullscreen));
					m_btnFullscreen.set_tooltip_text("Fullscreen");
					m_btnFullscreen.set_icon_name("view-fullscreen");
				m_toolbar.append(*Gtk::manage(new Gtk::SeparatorToolItem));
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

		auto split = new Gtk::HPaned;
			m_vbox.pack_start(*split);
			m_splitters.emplace(split);
			auto group = new WaveformGroup(this);
			m_waveformGroups.emplace(group);
			split->pack1(group->m_frame);

		m_vbox.pack_start(m_statusbar, Gtk::PACK_SHRINK);
			m_statusbar.get_style_context()->add_class("status");
			m_statusbar.pack_end(m_triggerConfigLabel, Gtk::PACK_SHRINK);
			m_triggerConfigLabel.set_size_request(75, 1);
			m_statusbar.pack_end(m_waveformRateLabel, Gtk::PACK_SHRINK);
			m_waveformRateLabel.set_size_request(175, 1);

	//Create history windows
	for(auto scope : m_scopes)
		m_historyWindows[scope] = new HistoryWindow(this, scope);

	//Process all of the channels
	for(auto scope : m_scopes)
	{
		for(size_t i=0; i<scope->GetChannelCount(); i++)
		{
			auto chan = scope->GetChannel(i);

			//Qualify the channel name by the scope name if we have >1 scope enabled
			if(m_scopes.size() > 1)
			{
				char tmp[128];
				snprintf(tmp, sizeof(tmp), "%s:%s", scope->m_nickname.c_str(), chan->GetHwname().c_str());
				chan->m_displayname = tmp;
			}

			auto type = chan->GetType();

			//Add a menu item - but not for the external trigger(s)
			if(type != OscilloscopeChannel::CHANNEL_TYPE_TRIGGER)
			{
				item = Gtk::manage(new Gtk::MenuItem(chan->m_displayname, false));
				item->signal_activate().connect(
					sigc::bind<OscilloscopeChannel*>(sigc::mem_fun(*this, &OscilloscopeWindow::OnAddChannel), chan));
				m_channelsMenu.append(*item);
			}

			//Enable all channels to save time when setting up the client
			if( (type == OscilloscopeChannel::CHANNEL_TYPE_ANALOG) ||
				( (type == OscilloscopeChannel::CHANNEL_TYPE_DIGITAL) && !nodigital ) )
			{
				auto w = new WaveformArea(chan, this);
				w->m_group = group;
				m_waveformAreas.emplace(w);
				if(type == OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
					group->m_waveformBox.pack_start(*w, Gtk::PACK_SHRINK);
				else
					group->m_waveformBox.pack_start(*w);
			}
		}
	}

	//Add trigger config menu items for each scope
	for(auto scope : m_scopes)
	{
		item = Gtk::manage(new Gtk::MenuItem(scope->m_nickname, false));
		item->signal_activate().connect(
			sigc::bind<Oscilloscope*>(sigc::mem_fun(*this, &OscilloscopeWindow::OnTriggerProperties), scope));
		m_setupTriggerMenu.append(*item);
	}

	m_channelsMenu.show_all();

	for(auto it : m_historyWindows)
		it.second->hide();

	//Done adding widgets
	show_all();

	//Don't show measurements or wizards by default
	group->m_measurementView.hide();

	//Initialize the style sheets
	m_css = Gtk::CssProvider::create();
	m_css->load_from_path("styles/glscopeclient.css");
	get_style_context()->add_provider_for_screen(
		Gdk::Screen::get_default(), m_css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Message handlers

bool OscilloscopeWindow::OnTimer(int /*timer*/)
{
	PollScopes();

	//Clean up the scope sync wizard if it's completed
	if(m_syncComplete)
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

void OscilloscopeWindow::OnPreferenceDialogResponse(int response)
{
	if(response == Gtk::RESPONSE_OK)
	{
		m_preferenceDialog->SaveChanges();
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
	CloseSession();
	return false;
}

/**
	@brief Shuts down the current session in preparation for opening a saved file etc
 */
void OscilloscopeWindow::CloseSession()
{
    //Close preferences dialog, if it exists
    if(m_preferenceDialog)
    {
        m_preferenceDialog->hide();
        delete m_preferenceDialog;
        m_preferenceDialog = nullptr;
    }

    //Save preferences
    m_preferences.SavePreferences();

	//Close all of our UI elements
	for(auto it : m_historyWindows)
		delete it.second;
	for(auto a : m_analyzers)
		delete a;
	for(auto s : m_splitters)
		delete s;
	for(auto g : m_waveformGroups)
		delete g;
	for(auto w : m_waveformAreas)
		delete w;

	//Clear our records of them
	m_historyWindows.clear();
	m_analyzers.clear();
	m_splitters.clear();
	m_waveformGroups.clear();
	m_waveformAreas.clear();

	delete m_scopeSyncWizard;
	m_scopeSyncWizard = NULL;

	m_multiScopeFreeRun = false;

	//Delete stuff from our UI
	auto children = m_setupTriggerMenu.get_children();
	for(auto c : children)
		m_setupTriggerMenu.remove(*c);
	children = m_channelsMenu.get_children();
	for(auto c : children)
		m_channelsMenu.remove(*c);

	//Purge our list of scopes (the app will delete them)
	m_scopes.clear();

	//Close stuff in the application, terminate threads, etc
	g_app->ShutDownSession();
}

/**
	@brief Open a saved configuration
 */
void OscilloscopeWindow::OnFileOpen()
{
	//TODO: prompt to save changes to the current session

	//Remove the CSS provider so the dialog isn't themed
	//TODO: how can we un-theme just this one dialog?
	get_style_context()->remove_provider_for_screen(
		Gdk::Screen::get_default(), m_css);

	Gtk::FileChooserDialog dlg(*this, "Open", Gtk::FILE_CHOOSER_ACTION_SAVE);

	dlg.add_choice("layout", "Load UI Configuration");
	dlg.add_choice("waveform", "Load Waveform Data");
	dlg.add_choice("reconnect", "Reconnect to Instrument (reconfigure using saved settings)");

	dlg.set_choice("layout", "true");
	dlg.set_choice("waveform", "true");
	dlg.set_choice("reconnect", "true");

	auto filter = Gtk::FileFilter::create();
	filter->add_pattern("*.scopesession");
	filter->set_name("glscopeclient sessions (*.scopesession)");
	dlg.add_filter(filter);
	dlg.add_button("Open", Gtk::RESPONSE_OK);
	dlg.add_button("Cancel", Gtk::RESPONSE_CANCEL);
	auto response = dlg.run();

	//Re-add the CSS provider
	get_style_context()->add_provider_for_screen(
		Gdk::Screen::get_default(), m_css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	if(response != Gtk::RESPONSE_OK)
		return;

	bool loadLayout = dlg.get_choice("layout") == "true";
	bool loadWaveform = dlg.get_choice("waveform") == "true";
	bool reconnect = dlg.get_choice("reconnect") == "true";
	DoFileOpen(dlg.get_filename(), loadLayout, loadWaveform, reconnect);
}

/**
	@brief Open a saved file
 */
void OscilloscopeWindow::DoFileOpen(string filename, bool loadLayout, bool loadWaveform, bool reconnect)
{
	m_currentFileName = filename;

	CloseSession();

	//Clear performance counters
	m_tAcquire = 0;
	m_tDecode = 0;
	m_tView = 0;
	m_tHistory = 0;
	m_tPoll = 0;
	m_totalWaveforms = 0;
	m_lastWaveformTimes.clear();

	try
	{
		auto docs = YAML::LoadAllFromFile(m_currentFileName);

		//Only open the first doc, our file format doesn't ever generate multiple docs in a file.
		//Ignore any trailing stuff at the end
		auto node = docs[0];

		//Load various sections of the file
		IDTable table;
		LoadInstruments(node["instruments"], reconnect, table);
		if(loadLayout)
		{
			LoadDecodes(node["decodes"], table);
			LoadUIConfiguration(node["ui_config"], table);
		}

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

	//TODO: refresh measurements and protocol decodes

	//Create protocol analyzers
	for(auto area : m_waveformAreas)
	{
		for(size_t i=0; i<area->GetOverlayCount(); i++)
		{
			auto pdecode = dynamic_cast<PacketDecoder*>(area->GetOverlay(i));
			if(pdecode != NULL)
			{
				char title[256];
				snprintf(title, sizeof(title), "Protocol Analyzer: %s", pdecode->m_displayname.c_str());

				auto analyzer = new ProtocolAnalyzerWindow(title, this, pdecode, area);
				m_analyzers.emplace(analyzer);

				//Done
				analyzer->show();
			}
		}
	}

	//Make sure all resize etc events have been handled before replaying history.
	//Otherwise eye patterns don't refresh right.
	show_all();
	GarbageCollectGroups();
	g_app->DispatchPendingEvents();

	//TODO: make this work properly if we have decodes spanning multiple scopes
	for(auto it : m_historyWindows)
		it.second->ReplayHistory();

	//Start threads to poll scopes etc
	g_app->StartScopeThreads();
}

/**
	@brief Loads waveform data for a save file
 */
void OscilloscopeWindow::LoadWaveformData(string filename, IDTable& table)
{
	//Figure out data directory
	string base = filename.substr(0, filename.length() - strlen(".scopesession"));
	string datadir = base + "_data";

	//Load data for each scope
	for(auto scope : m_scopes)
	{
		int id = table[scope];

		char tmp[512];
		snprintf(tmp, sizeof(tmp), "%s/scope_%d_metadata.yml", datadir.c_str(), id);
		auto docs = YAML::LoadAllFromFile(tmp);

		LoadWaveformDataForScope(docs[0], scope, datadir, table);
	}
}

/**
	@brief Loads waveform data for a single instrument
 */
void OscilloscopeWindow::LoadWaveformDataForScope(
	const YAML::Node& node,
	Oscilloscope* scope,
	string datadir,
	IDTable& table)
{
	TimePoint time;
	time.first = 0;
	time.second = 0;

	TimePoint newest;
	newest.first = 0;
	newest.second = 0;

	auto window = m_historyWindows[scope];
	int scope_id = table[scope];

	auto wavenode = node["waveforms"];
	window->SetMaxWaveforms(wavenode.size());
	for(auto it : wavenode)
	{
		//Top level metadata
		auto wfm = it.second;
		time.first = wfm["timestamp"].as<long long>();
		time.second = wfm["time_psec"].as<long long>();
		int waveform_id = wfm["id"].as<int>();

		//Detach old waveforms from our channels (if any)
		for(size_t i=0; i<scope->GetChannelCount(); i++)
			scope->GetChannel(i)->Detach();

		//Set up channel metadata first (serialized)
		auto chans = wfm["channels"];
		vector<int> channels;
		for(auto jt : chans)
		{
			auto ch = jt.second;
			int channel_index = ch["index"].as<int>();
			auto chan = scope->GetChannel(channel_index);
			channels.push_back(channel_index);

			//TODO: support non-analog/digital captures (eyes, spectrograms, etc)
			WaveformBase* cap = NULL;
			AnalogWaveform* acap = NULL;
			DigitalWaveform* dcap = NULL;
			if(chan->GetType() == OscilloscopeChannel::CHANNEL_TYPE_ANALOG)
				cap = acap = new AnalogWaveform;
			else
				cap = dcap = new DigitalWaveform;

			//Channel waveform metadata
			cap->m_timescale = ch["timescale"].as<long>();
			cap->m_startTimestamp = time.first;
			cap->m_startPicoseconds = time.second;
			cap->m_triggerPhase = ch["trigphase"].as<float>();

			chan->SetData(cap);
		}

		//Load data for each channel in parallel to speed parsing
		#pragma omp parallel for
		for(size_t i=0; i<channels.size(); i++)
		{
			int channel_index = channels[i];
			auto chan = scope->GetChannel(channel_index);
			auto cap = chan->GetData();
			auto acap = dynamic_cast<AnalogWaveform*>(cap);
			auto dcap = dynamic_cast<DigitalWaveform*>(cap);

			//Load the actual sample data
			char tmp[512];
			snprintf(tmp, sizeof(tmp), "%s/scope_%d_waveforms/waveform_%d/channel_%d.bin",
				datadir.c_str(),
				scope_id,
				waveform_id,
				channel_index);
			FILE* fp = fopen(tmp, "rb");
			if(!fp)
			{
				LogError("couldn't open %s\n", tmp);
				continue;
			}

			//Read the whole file into a buffer
			fseek(fp, 0, SEEK_END);
			long len = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			unsigned char* buf = new unsigned char[len];
			fread(buf, 1, len, fp);

			for(long offset=0; offset<len; )
			{
				long end = offset + 2*sizeof(int64_t);
				if(end > len)
					break;

				//Read start time and duration
				int64_t* stime = reinterpret_cast<int64_t*>(buf+offset);
				offset = end;

				//Read sample data
				if(acap)
				{
					end = offset + sizeof(float);
					if(end > len)
						break;
					float* f = reinterpret_cast<float*>(buf+offset);
					offset = end;

					acap->m_offsets.push_back(stime[0]);
					acap->m_durations.push_back(stime[1]);
					acap->m_samples.push_back(*f);
				}

				else
				{
					end = offset + sizeof(bool);
					if(end > len)
						break;
					bool *b = reinterpret_cast<bool*>(buf+offset);
					offset = end;

					dcap->m_offsets.push_back(stime[0]);
					dcap->m_durations.push_back(stime[1]);
					dcap->m_samples.push_back(*b);
				}
			}

			delete[] buf;
			fclose(fp);
		}

		//Add to history
		window->OnWaveformDataReady();

		//Keep track of the newest waveform (may not be in time order)
		if( (time.first > newest.first) ||
			( (time.first == newest.first) &&  (time.second > newest.second) ) )
		{
			newest = time;
		}
	}

	window->JumpToHistory(newest);
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

		if(reconnect)
		{
			//Create the scope
			auto transport = SCPITransport::CreateTransport(inst["transport"].as<string>(), inst["args"].as<string>());

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
				scope = Oscilloscope::CreateOscilloscope(inst["driver"].as<string>(), transport);

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

		if(!scope)
		{
			//Create the mock scope
			scope = new MockOscilloscope(
				inst["name"].as<string>(),
				inst["vendor"].as<string>(),
				inst["serial"].as<string>());
		}

		//All good. Add to our list of scopes etc
		g_app->m_scopes.push_back(scope);
		m_scopes.push_back(scope);
		table.emplace(inst["id"].as<int>(), scope);

		//Configure the scope
		scope->LoadConfiguration(inst, table);

		//Add menu config for the scope (only if reconnecting)
		if(reconnect)
		{
			for(size_t i=0; i<scope->GetChannelCount(); i++)
			{
				auto chan = scope->GetChannel(i);
				if(chan->GetType() != OscilloscopeChannel::CHANNEL_TYPE_TRIGGER)
				{
					auto item = Gtk::manage(new Gtk::MenuItem(chan->m_displayname, false));
					item->signal_activate().connect(
						sigc::bind<OscilloscopeChannel*>(sigc::mem_fun(*this, &OscilloscopeWindow::OnAddChannel), chan));
					m_channelsMenu.append(*item);
				}
			}
			auto item = Gtk::manage(new Gtk::MenuItem(scope->m_nickname, false));
			item->signal_activate().connect(
				sigc::bind<Oscilloscope*>(sigc::mem_fun(*this, &OscilloscopeWindow::OnTriggerProperties), scope));
			m_setupTriggerMenu.append(*item);
		}
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
		auto decode = ProtocolDecoder::CreateDecoder(proto, dnode["color"].as<string>());
		if(decode == NULL)
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

		table.emplace(dnode["id"].as<int>(), decode);

		//Load parameters during the first pass.
		//Parameters can't have dependencies on other channels etc.
		//More importantly, parameters may change bus width etc
		decode->LoadParameters(dnode, table);
	}

	//Make a second pass to configure the decoder inputs, once all of them have been instantiated.
	//Decoders may depend on other decoders as inputs, and serialization is not guaranteed to be a topological sort.
	for(auto it : node)
	{
		auto dnode = it.second;
		auto decode = static_cast<ProtocolDecoder*>(table[dnode["id"].as<int>()]);
		if(decode)
			decode->LoadInputs(dnode, table);
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
		WaveformArea* area = new WaveformArea(channel, this);
		table.emplace(an["id"].as<int>(), area);
		area->SetPersistenceEnabled(an["persistence"].as<int>() ? true : false);
		m_waveformAreas.emplace(area);

		//Add any overlays
		auto overlays = an["overlays"];
		for(auto jt : overlays)
		{
			auto decode = static_cast<ProtocolDecoder*>(table[jt.second["id"].as<int>()]);
			if(decode)
				area->AddOverlay(decode);
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
		group->m_frame.set_label(gn["name"].as<string>());
		group->m_pixelsPerXUnit = gn["pixelsPerXUnit"].as<float>();
		group->m_xAxisOffset = gn["xAxisOffset"].as<long>();
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
		group->m_xCursorPos[0] = gn["xcursor0"].as<long>();
		group->m_xCursorPos[1] = gn["xcursor1"].as<long>();
		group->m_yCursorPos[0] = gn["ycursor0"].as<float>();
		group->m_yCursorPos[1] = gn["ycursor1"].as<float>();

		//TODO: statistics

		//Waveform areas
		areas = gn["areas"];
		for(auto at : areas)
		{
			auto area = static_cast<WaveformArea*>(table[at.second["id"].as<int>()]);
			if(!area)
				continue;
			area->m_group = group;
			if(area->GetChannel()->GetType() == OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
				group->m_waveformBox.pack_start(*area, Gtk::PACK_SHRINK);
			else
				group->m_waveformBox.pack_start(*area);
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
void OscilloscopeWindow::OnFileSave(bool saveToCurrentFile, bool saveLayout, bool saveWaveforms)
{
	bool creatingNew = false;

	static const char* extension = ".scopesession";

	//Pop up the dialog if we asked for a new file.
	//But if we don't have a current file, we need to prompt regardless
	if(m_currentFileName.empty() || !saveToCurrentFile)
	{
		creatingNew = true;

		string title = "Save ";
		if(saveLayout)
		{
			title += "Layout";
			if(saveWaveforms)
				title += " and ";
		}
		if(saveWaveforms)
			title += "Waveforms";

		//Remove the CSS provider so the dialog isn't themed
		//TODO: how can we un-theme just this one dialog?
		get_style_context()->remove_provider_for_screen(
			Gdk::Screen::get_default(), m_css);

		Gtk::FileChooserDialog dlg(*this, title, Gtk::FILE_CHOOSER_ACTION_SAVE);

		auto filter = Gtk::FileFilter::create();
		filter->add_pattern("*.scopesession");
		filter->set_name("glscopeclient sessions (*.scopesession)");
		dlg.add_filter(filter);
		dlg.add_button("Save", Gtk::RESPONSE_OK);
		dlg.add_button("Cancel", Gtk::RESPONSE_CANCEL);
		dlg.set_uri(m_currentFileName);
		dlg.set_do_overwrite_confirmation();
		auto response = dlg.run();

		//Re-add the CSS provider
		get_style_context()->add_provider_for_screen(
			Gdk::Screen::get_default(), m_css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

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

	//Serialize our configuration and save to the file
	IDTable table;
	string config = SerializeConfiguration(saveLayout, table);
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

	//Serialize waveform data if needed
	if(saveWaveforms)
		SerializeWaveforms(table);
}

string OscilloscopeWindow::SerializeConfiguration(bool saveLayout, IDTable& table)
{
	string config = "";

	//TODO: save metadata

	//Save instrument config regardless, since data etc needs it
	config += SerializeInstrumentConfiguration(table);

	//Decodes depend on scope channels, but need to happen before UI elements that use them
	if(!ProtocolDecoder::EnumDecodes().empty())
		config += SerializeDecodeConfiguration(table);

	//UI config
	if(saveLayout)
		config += SerializeUIConfiguration(table);

	return config;
}

/**
	@brief Serialize the configuration for all oscilloscopes
 */
string OscilloscopeWindow::SerializeInstrumentConfiguration(IDTable& table)
{
	string config = "instruments:\n";

	for(auto scope : m_scopes)
		config += scope->SerializeConfiguration(table);

	return config;
}

/**
	@brief Serialize the configuration for all protocol decoders
 */
string OscilloscopeWindow::SerializeDecodeConfiguration(IDTable& table)
{
	string config = "decodes:\n";

	auto set = ProtocolDecoder::EnumDecodes();
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
		snprintf(tmp, sizeof(tmp), "        : \n");
		config += tmp;
		snprintf(tmp, sizeof(tmp), "            id:          %d\n", table[area]);
		config += tmp;
		snprintf(tmp, sizeof(tmp), "            persistence: %d\n", area->GetPersistenceEnabled());
		config += tmp;

		//Channels
		//By the time we get here, all channels should be accounted for.
		//So there should be no reason to assign names to channels at this point - just use what's already there
		snprintf(tmp, sizeof(tmp), "            channel:     %d\n", table[area->GetChannel()]);
		config += tmp;

		//Overlays
		if(area->GetOverlayCount() != 0)
		{
			snprintf(tmp, sizeof(tmp), "            overlays:\n");
			config += tmp;

			for(size_t i=0; i<area->GetOverlayCount(); i++)
			{
				snprintf(tmp, sizeof(tmp), "                :\n");
				config += tmp;
				snprintf(tmp, sizeof(tmp), "                    id:      %d\n", table[area->GetOverlay(i)]);
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

	//Splitters
	config += "    splitters: \n";
	for(auto split : m_splitters)
		table.emplace(split);
	for(auto split : m_splitters)
	{
		//Splitter config
		snprintf(tmp, sizeof(tmp), "        : \n");
		config += tmp;
		snprintf(tmp, sizeof(tmp), "            id:     %d\n", table[split]);
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
	//Remove all old waveforms in the data directory.
	//TODO: better way that doesn't involve system()
	char cwd[PATH_MAX];
	getcwd(cwd, PATH_MAX);
	chdir(m_currentDataDirName.c_str());
	system("rm -rf scope_*");
	chdir(cwd);

	//Serialize waveforms for each of our instruments
	for(auto it : m_historyWindows)
		it.second->SerializeWaveforms(m_currentDataDirName, table);
}

void OscilloscopeWindow::OnAlphaChanged()
{
	ClearAllPersistence();
}

void OscilloscopeWindow::OnTriggerProperties(Oscilloscope* scope)
{
	TriggerPropertiesDialog dlg(this, scope);
	if(Gtk::RESPONSE_OK != dlg.run())
		return;
	dlg.ConfigureTrigger();
}

void OscilloscopeWindow::OnEyeColorChanged(EyeColor color, Gtk::RadioMenuItem* item)
{
	if(!item->get_active())
		return;

	m_eyeColor = color;
	for(auto v : m_waveformAreas)
		v->queue_draw();
}

OscilloscopeWindow::EyeColor OscilloscopeWindow::GetEyeColor()
{
	return m_eyeColor;
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
	//Hierarchy is WaveformArea -> WaveformGroup waveform box -> WaveformGroup box -> WaveformGroup frame -> splitter
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
	SplitGroup(w->get_parent()->get_parent()->get_parent(), group, horizontal);

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
	SplitGroup(w->get_parent()->get_parent()->get_parent(), group, horizontal);

	//Make a copy of the current waveform view and add to that group
	OnCopyToExistingGroup(w, group);
}

void OscilloscopeWindow::OnMoveToExistingGroup(WaveformArea* w, WaveformGroup* ngroup)
{
	auto oldgroup = w->m_group;

	w->m_group = ngroup;
	w->get_parent()->remove(*w);

	if(w->GetChannel()->GetType() == OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
		ngroup->m_waveformBox.pack_start(*w, Gtk::PACK_SHRINK);
	else
		ngroup->m_waveformBox.pack_start(*w);

	//Move stats related to this trace to the new group
	set<OscilloscopeChannel*> chans;
	chans.emplace(w->GetChannel());
	for(size_t i=0; i<w->GetOverlayCount(); i++)
		chans.emplace(w->GetOverlay(i));
	for(auto chan : chans)
	{
		if(oldgroup->IsShowingStats(chan))
		{
			oldgroup->ToggleOff(chan);
			ngroup->ToggleOn(chan);
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
	if(nw->GetChannel()->GetType() == OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
		ngroup->m_waveformBox.pack_start(*nw, Gtk::PACK_SHRINK);
	else
		ngroup->m_waveformBox.pack_start(*nw);
	nw->show();

	//Add stats if needed
	set<OscilloscopeChannel*> chans;
	chans.emplace(w->GetChannel());
	for(size_t i=0; i<w->GetOverlayCount(); i++)
		chans.emplace(w->GetOverlay(i));
	for(auto chan : chans)
	{
		if(w->m_group->IsShowingStats(chan))
			ngroup->ToggleOn(chan);
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
	}
}

void OscilloscopeWindow::OnClearSweeps()
{
	//TODO: clear regular waveform data and history too?

	//Clear integrated data from all protocol decodes
	auto decodes = ProtocolDecoder::EnumDecodes();
	for(auto d : decodes)
		d->ClearSweeps();

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
}

void OscilloscopeWindow::OnAutofitHorizontal()
{
	LogDebug("autofit horz\n");

	//
}

void OscilloscopeWindow::OnZoomInHorizontal(WaveformGroup* group)
{
	group->m_pixelsPerXUnit *= 1.5;
	ClearPersistence(group);
}

void OscilloscopeWindow::OnZoomOutHorizontal(WaveformGroup* group)
{
	group->m_pixelsPerXUnit /= 1.5;
	ClearPersistence(group);
}

void OscilloscopeWindow::ClearPersistence(WaveformGroup* group, bool dirty)
{
	auto children = group->m_vbox.get_children();
	for(auto w : children)
	{
		//Redraw all views in the waveform box
		auto box = dynamic_cast<Gtk::Box*>(w);
		if(box)
		{
			auto bchildren = box->get_children();
			for(auto a : bchildren)
			{
				//Clear persistence on waveform areas
				auto area = dynamic_cast<WaveformArea*>(a);
				if(area != NULL)
				{
					if(dirty)
						area->SetGeometryDirty();
					area->ClearPersistence();
				}
			}
		}

		//Redraw everything (timeline included)
		w->queue_draw();
	}
}

void OscilloscopeWindow::ClearAllPersistence()
{
	for(auto w : m_waveformAreas)
	{
		w->ClearPersistence();
		w->queue_draw();
	}
}

void OscilloscopeWindow::OnQuit()
{
	close();
}

void OscilloscopeWindow::OnAddChannel(OscilloscopeChannel* chan)
{
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

	//Add to a the first group for now
	DoAddChannel(chan, *m_waveformGroups.begin());
}

WaveformArea* OscilloscopeWindow::DoAddChannel(OscilloscopeChannel* chan, WaveformGroup* ngroup, WaveformArea* ref)
{
	//Create the viewer
	auto w = new WaveformArea(chan, this);
	w->m_group = ngroup;
	m_waveformAreas.emplace(w);

	if(chan->GetType() == OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
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
}

bool OscilloscopeWindow::PollScopes()
{
	bool had_waveforms = false;

	bool pending = true;
	while(pending)
	{
		//Wait for every scope to have triggered
		bool ready = true;
		double start = GetTime();
		for(auto scope : m_scopes)
		{
			if(!scope->HasPendingWaveforms())
			{
				ready = false;
				break;
			}
		}
		m_tPoll += GetTime() - start;

		//Keep track of when the primary instrument triggers.
		if(m_multiScopeFreeRun)
		{
			//See when the primary triggered
			if( (m_tPrimaryTrigger < 0) && m_scopes[0]->HasPendingWaveforms() )
				m_tPrimaryTrigger = GetTime();

			//All instruments should trigger within 100ms (arbitrary threshold) of the primary.
			//If it's been longer than that, something went wrong. Discard all pending data and re-arm the trigger.
			double twait = GetTime() - m_tPrimaryTrigger;
			if( (m_tPrimaryTrigger > 0) && ( twait > 0.1 ) )
			{
				LogWarning("Timed out waiting for one or more secondary instruments to trigger (%.2f ms). Resetting...\n",
					twait*1000);

				//Cancel any pending triggers
				OnStop();

				//Discard all pending waveform data
				for(auto scope : m_scopes)
				{
					scope->IDPing();
					scope->ClearPendingWaveforms();
				}

				//Re-arm the trigger and get back to polling
				OnStart();
				return false;
			}
		}

		//If not ready, stop
		if(!ready)
			break;

		//We have data to download
		had_waveforms = true;

		//In multi-scope free-run mode, re-arm every instrument's trigger after we've downloaded data from all of them.
		if(m_multiScopeFreeRun)
			ArmTrigger(false);

		//Process the waveform data from each instrument
		//TODO: handle waveforms coming in faster than display framerate can keep up
		pending = true;
		for(auto scope : m_scopes)
		{
			OnWaveformDataReady(scope);

			if(!scope->HasPendingWaveforms())
				pending = false;
		}

		//Update filters etc once every instrument has been updated
		OnAllWaveformsUpdated();
	}

	return had_waveforms;
}

/**
	@brief Handles new data arriving for a specific instrument
 */
void OscilloscopeWindow::OnWaveformDataReady(Oscilloscope* scope)
{
	//TODO: handle multiple scopes better
	m_lastWaveformTimes.push_back(GetTime());
	while(m_lastWaveformTimes.size() > 10)
		m_lastWaveformTimes.erase(m_lastWaveformTimes.begin());

	//make sure we close fully
	if(!is_visible())
	{
		for(auto it : m_historyWindows)
			it.second->close();
	}

	//Make sure we don't free the old waveform data
	//LogTrace("Detaching\n");
	for(size_t i=0; i<scope->GetChannelCount(); i++)
		scope->GetChannel(i)->Detach();

	//Download the data
	//LogTrace("Acquiring\n");
	double start = GetTime();
	scope->AcquireDataFifo();
	m_tAcquire += GetTime() - start;

	//Update the history window
	start = GetTime();
	m_historyWindows[scope]->OnWaveformDataReady();
	m_tHistory += GetTime() - start;
}

/**
	@brief Handles updating things after all instruments have downloaded their new waveforms
 */
void OscilloscopeWindow::OnAllWaveformsUpdated()
{
	m_totalWaveforms ++;

	//Update the status
	UpdateStatusBar();
	RefreshAllDecoders();

	//Update protocol analyzers
	for(auto a : m_analyzers)
		a->OnWaveformDataReady();

	//Update the views
	double start = GetTime();
	for(auto w : m_waveformAreas)
		w->OnWaveformDataReady();
	m_tView += GetTime() - start;

	//Update the trigger sync wizard, if it's active
	if(m_scopeSyncWizard && m_scopeSyncWizard->is_visible())
		m_scopeSyncWizard->OnWaveformDataReady();
}

void OscilloscopeWindow::RefreshAllDecoders()
{
	double start = GetTime();

	auto decodes = ProtocolDecoder::EnumDecodes();
	for(auto d : decodes)
		d->SetDirty();

	//Prepare to topologically sort filter nodes into blocks capable of parallel evaluation.
	//Block 0 may only depend on physical scope channels.
	//Block 1 may depend on decodes in block 0 or physical channels.
	//Block 2 may depend on 1/0/physical, etc.
	typedef vector<ProtocolDecoder*> DecodeBlock;
	vector<DecodeBlock> blocks;

	//Working set starts out as all decoders
	auto working = ProtocolDecoder::EnumDecodes();

	//Each iteration, put all decodes that only depend on previous blocks into this block.
	for(int block=0; !working.empty(); block++)
	{
		DecodeBlock current_block;

		for(auto w : working)
		{
			ProtocolDecoder* d = static_cast<ProtocolDecoder*>(w);

			//Check if we have any inputs that are still in the working set.
			bool ok = true;
			for(size_t i=0; i<d->GetInputCount(); i++)
			{
				auto in = d->GetInput(i);
				if(working.find((ProtocolDecoder*)in) != working.end())
				{
					ok = false;
					break;
				}
			}

			//All inputs are in previous blocks, we're good to go for the current block
			if(ok)
				current_block.push_back(d);
		}

		//Anything we assigned this iteration shouldn't be in the working set for next time.
		//It does, however, have to get saved in the output block.
		for(auto d : current_block)
			working.erase(d);
		blocks.push_back(current_block);
	}

	//Evaluate the blocks, taking advantage of parallelism between them
	for(auto& block : blocks)
	{
		#pragma omp parallel for
		for(size_t i=0; i<block.size(); i++)
			block[i]->RefreshIfDirty();
	}

	//Update statistic displays after the filter graph update is complete
	for(auto g : m_waveformGroups)
		g->RefreshMeasurements();

	m_tDecode += GetTime() - start;
}

void OscilloscopeWindow::UpdateStatusBar()
{
	if(m_scopes.empty())
		return;

	//TODO: redo this for multiple scopes
	auto scope = m_scopes[0];
	char tmp[256];
	size_t trig_idx = scope->GetTriggerChannelIndex();
	OscilloscopeChannel* chan = scope->GetChannel(trig_idx);
	if(chan == NULL)
	{
		LogWarning("Trigger channel (index %zu) is NULL\n", trig_idx);
		return;
	}
	string name = chan->GetHwname();
	float voltage = scope->GetTriggerVoltage();
	if(voltage < 1)
		snprintf(tmp, sizeof(tmp), "%s %.0f mV", name.c_str(), voltage*1000);
	else
		snprintf(tmp, sizeof(tmp), "%s %.3f V", name.c_str(), voltage);
	m_triggerConfigLabel.set_label(tmp);

	//Update WFM/s counter
	if(m_lastWaveformTimes.size() >= 2)
	{
		double first = m_lastWaveformTimes[0];
		double last = m_lastWaveformTimes[m_lastWaveformTimes.size() - 1];
		double dt = last - first;
		double wps = m_lastWaveformTimes.size() / dt;
		snprintf(tmp, sizeof(tmp), "%zu WFMs, %.2f WFM/s", m_totalWaveforms, wps);
		m_waveformRateLabel.set_label(tmp);
	}
}

void OscilloscopeWindow::OnStart()
{
	ArmTrigger(false);
}

void OscilloscopeWindow::OnStartSingle()
{
	ArmTrigger(true);
}

void OscilloscopeWindow::OnStop()
{
	m_multiScopeFreeRun = false;

	for(auto scope : m_scopes)
		scope->Stop();
}

void OscilloscopeWindow::ArmTrigger(bool oneshot)
{
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

	for(ssize_t i=m_scopes.size()-1; i >= 0; i--)
	{
		if(oneshot || m_multiScopeFreeRun)
			m_scopes[i]->StartSingleTrigger();
		else
			m_scopes[i]->Start();

		//If we have multiple scopes, ping the scope to make sure the arm command went through
		if(i != 0)
			m_scopes[i]->IDPing();
	}
	m_tArm = GetTime();
}

/**
	@brief Called when the history view selects an old waveform
 */
void OscilloscopeWindow::OnHistoryUpdated(bool refreshAnalyzers)
{
	//Stop triggering if we select a saved waveform
	OnStop();

	RefreshAllDecoders();

	//Update the views
	for(auto w : m_waveformAreas)
	{
		w->ClearPersistence();
		w->OnWaveformDataReady();
	}

	if(refreshAnalyzers)
	{
		for(auto a : m_analyzers)
			a->OnWaveformDataReady();
	}
}

void OscilloscopeWindow::RemoveHistory(TimePoint timestamp)
{
	for(auto a : m_analyzers)
		a->RemoveHistory(timestamp);
}

void OscilloscopeWindow::JumpToHistory(TimePoint timestamp)
{
	//TODO:  this might not work too well if triggers aren't perfectly synced!
	for(auto it : m_historyWindows)
		it.second->JumpToHistory(timestamp);
}

void OscilloscopeWindow::OnTimebaseSettings()
{
	TimebasePropertiesDialog dlg(this, m_scopes);
	if(dlg.run() == Gtk::RESPONSE_OK)
		dlg.ConfigureTimebase();
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
