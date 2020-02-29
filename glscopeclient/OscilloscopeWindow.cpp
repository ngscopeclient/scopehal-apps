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
#include "OscilloscopeWindow.h"
#include <unistd.h>
#include <fcntl.h>
#include <map>

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

/**
	@brief Initializes the main window
 */
OscilloscopeWindow::OscilloscopeWindow(vector<Oscilloscope*> scopes)
	: m_historyWindow(this)
	, m_scopes(scopes)
	// m_iconTheme(Gtk::IconTheme::get_default())
{
	//Set title
	string title = "Oscilloscope: ";
	for(size_t i=0; i<scopes.size(); i++)
	{
		auto scope = scopes[i];

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
	}
	set_title(title);

	//Initial setup
	set_reallocate_redraws(true);
	set_default_size(1280, 800);

	//Add widgets
	CreateWidgets();

	ArmTrigger(false);
	m_toggleInProgress = false;

	m_tLastFlush = GetTime();

	m_eyeColor = EYE_CRT;

	m_tAcquire = 0;
	m_tDecode = 0;
	m_tView = 0;
	m_tHistory = 0;
	m_tPoll = 0;
	m_tEvent = 0;
}

/**
	@brief Application cleanup
 */
OscilloscopeWindow::~OscilloscopeWindow()
{
	//Print stats
	LogDebug("ACQUIRE: %.3f ms\n", m_tAcquire * 1000);
	LogDebug("DECODE:  %.3f ms\n", m_tDecode * 1000);
	LogDebug("VIEW:    %.3f ms\n", m_tView * 1000);
	LogDebug("HISTORY: %.3f ms\n", m_tHistory * 1000);
	LogDebug("POLL:    %.3f ms\n", m_tPoll * 1000);
	LogDebug("EVENT:   %.3f ms\n", m_tEvent * 1000);

	for(auto a : m_analyzers)
		delete a;
	for(auto s : m_splitters)
		delete s;
	for(auto g : m_waveformGroups)
		delete g;
	for(auto w : m_waveformAreas)
		delete w;

	//decoders should self-delete when the last reference to them is removed
	//for(auto d : m_decoders)
	//	delete d;
}

/**
	@brief Helper function for creating widgets and setting up signal handlers
 */
void OscilloscopeWindow::CreateWidgets()
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
					item = Gtk::manage(new Gtk::MenuItem("Load Layout Only...", false));
					m_fileMenu.append(*item);
					item = Gtk::manage(new Gtk::MenuItem("Load Waveforms Only...", false));
					m_fileMenu.append(*item);
					item = Gtk::manage(new Gtk::MenuItem("Load Layout and Waveforms...", false));
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
				m_toolbar.append(*Gtk::manage(new Gtk::SeparatorToolItem));
			m_toolbox.pack_start(m_alphalabel, Gtk::PACK_SHRINK);
				m_alphalabel.set_label("Opacity ");
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

		auto split = new Gtk::HPaned;
			m_vbox.pack_start(*split);
			m_splitters.emplace(split);
			auto group = new WaveformGroup(this);
			m_waveformGroups.emplace(group);
			split->pack1(group->m_frame);

		m_vbox.pack_start(m_statusbar, Gtk::PACK_SHRINK);
		m_statusbar.pack_end(m_triggerConfigLabel, Gtk::PACK_SHRINK);
		m_triggerConfigLabel.set_size_request(75, 1);

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

			//Add a menu item - but not for the external trigger(s)
			if(chan->GetType() != OscilloscopeChannel::CHANNEL_TYPE_TRIGGER)
			{
				item = Gtk::manage(new Gtk::MenuItem(chan->m_displayname, false));
				item->signal_activate().connect(
					sigc::bind<OscilloscopeChannel*>(sigc::mem_fun(*this, &OscilloscopeWindow::OnAddChannel), chan));
				m_channelsMenu.append(*item);
			}

			//See which channels are currently on
			//DEBUG: enable all analog channels to save time when setting up the client
			//if(chan->IsEnabled())
			if(chan->GetType() == OscilloscopeChannel::CHANNEL_TYPE_ANALOG)
			{
				auto w = new WaveformArea(
					scope,
					chan,
					this);
				w->m_group = group;
				m_waveformAreas.emplace(w);
				group->m_waveformBox.pack_start(*w);
			}
		}
	}

	m_channelsMenu.show_all();

	m_historyWindow.hide();

	//Done adding widgets
	show_all();

	//Don't show measurements by default
	group->m_measurementFrame.hide();

	//Initialize the style sheets
	m_css = Gtk::CssProvider::create();
	m_css->load_from_path("styles/glscopeclient.css");
	get_style_context()->add_provider_for_screen(
		Gdk::Screen::get_default(), m_css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Message handlers

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

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// FIXME: a fair bit of the code below is POSIX specific and will need to be fixed for portability eventually

	//See if the directory exists
	bool dir_exists = false;
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

	//See if the file exists
	bool file_exists = false;
	hfile = open(m_currentFileName.c_str(), O_RDONLY);
	if(hfile >= 0)
	{
		file_exists = true;
		::close(hfile);
	}

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
		if(0 != mkdir(m_currentDataDirName.c_str(), 0755))
		{
			string msg = string("The data directory ") + m_currentDataDirName + " could not be created!";
			Gtk::MessageDialog errdlg(msg, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
			errdlg.set_title("Cannot save session\n");
			errdlg.run();
			return;
		}
	}

	//Serialize our configuration and save to the file
	string config = SerializeConfiguration(saveLayout);
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
}

string OscilloscopeWindow::SerializeConfiguration(bool saveLayout)
{
	string config = "";

	//Assign integer IDs to everything instead of pointers
	int nextID = 1;
	std::map<void*, int> idmap;

	//TODO: save metadata

	//Save instrument config regardless, since data etc needs it
	config += SerializeInstrumentConfiguration(idmap, nextID);

	//UI config
	if(saveLayout)
		config += SerializeUIConfiguration(idmap, nextID);

	return config;
}

string OscilloscopeWindow::SerializeInstrumentConfiguration(std::map<void*, int>& idmap, int& nextID)
{
	char tmp[1024];
	string config = "instruments:\n";

	for(auto scope : m_scopes)
	{
		//Name it
		int id = nextID ++;
		idmap[scope] = id;

		//Save basic scope info
		snprintf(tmp, sizeof(tmp), "    - id:             %d\n", id);
		config += tmp;
		snprintf(tmp, sizeof(tmp), "      nick:           \"%s\"\n", scope->m_nickname.c_str());
		config += tmp;
		snprintf(tmp, sizeof(tmp), "      name:           \"%s\"\n", scope->GetName().c_str());
		config += tmp;
		snprintf(tmp, sizeof(tmp), "      vendor:         \"%s\"\n", scope->GetVendor().c_str());
		config += tmp;
		snprintf(tmp, sizeof(tmp), "      serial:         \"%s\"\n", scope->GetSerial().c_str());
		config += tmp;
		snprintf(tmp, sizeof(tmp), "      path:           \"%s\"\n", "connection_string_not_yet_implemented");
		config += tmp;

		//Save channels
		config += "    channels:\n";
		for(size_t i=0; i<scope->GetChannelCount(); i++)
		{
			auto chan = scope->GetChannel(i);
			if(!chan->IsPhysicalChannel())
				continue;	//skip any kind of math functions etc

			id = nextID ++;
			idmap[chan] = id;

			//Basic channel info
			snprintf(tmp, sizeof(tmp), "        - id:          %d\n", id);
			config += tmp;
			snprintf(tmp, sizeof(tmp), "          index:       %zu\n", i);
			config += tmp;
			snprintf(tmp, sizeof(tmp), "          color:       \"%s\"\n", chan->m_displaycolor.c_str());
			config += tmp;
			snprintf(tmp, sizeof(tmp), "          nick:        \"%s\"\n", chan->m_displayname.c_str());
			config += tmp;
			snprintf(tmp, sizeof(tmp), "          name:        \"%s\"\n", chan->GetHwname().c_str());
			config += tmp;
			switch(chan->GetType())
			{
				case OscilloscopeChannel::CHANNEL_TYPE_ANALOG:
					config += "          type:        analog\n";
					break;
				case OscilloscopeChannel::CHANNEL_TYPE_DIGITAL:
					config += "          type:        digital\n";
					break;
				case OscilloscopeChannel::CHANNEL_TYPE_TRIGGER:
					config += "          type:        trigger\n";
					break;

				//should never get complex channels on a scope
				default:
					break;
			}

			//Current channel configuration
			if(chan->IsEnabled())
				config += "          enabled:     1\n";
			else
				config += "          enabled:     0\n";
			switch(chan->GetCoupling())
			{
				case OscilloscopeChannel::COUPLE_DC_1M:
					config += "          coupling:    dc_1M\n";
					break;
				case OscilloscopeChannel::COUPLE_AC_1M:
					config += "          coupling:    ac_1M\n";
					break;
				case OscilloscopeChannel::COUPLE_DC_50:
					config += "          coupling:    dc_50\n";
					break;
				case OscilloscopeChannel::COUPLE_GND:
					config += "          coupling:    gnd\n";
					break;

				//should never get synthetic coupling on a scope channel
				default:
					break;
			}

			if(chan->GetType() == OscilloscopeChannel::CHANNEL_TYPE_ANALOG)
			{
				snprintf(tmp, sizeof(tmp), "          attenuation: %f\n", chan->GetAttenuation());
				config += tmp;
				snprintf(tmp, sizeof(tmp), "          bwlimit:     %d\n", chan->GetBandwidthLimit());
				config += tmp;
				snprintf(tmp, sizeof(tmp), "          vrange:      %f\n", chan->GetVoltageRange());
				config += tmp;
				snprintf(tmp, sizeof(tmp), "          offset:      %f\n", chan->GetOffset());
				config += tmp;
			}
		}
	}

	return config;
}

string OscilloscopeWindow::SerializeUIConfiguration(std::map<void*, int>& idmap, int& nextID)
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
		idmap[area] = nextID++;
	for(auto area : m_waveformAreas)
	{
		int id = idmap[area];
		snprintf(tmp, sizeof(tmp), "        - id:             %d\n", id);
		config += tmp;
	}

	//Waveform groups
	config += "    groups:\n";
	for(auto group : m_waveformGroups)
		idmap[group] = nextID++;
	for(auto group : m_waveformGroups)
	{
		int id = idmap[group];
		snprintf(tmp, sizeof(tmp), "        - id:             %d\n", id);
		config += tmp;

		config += "          name:           \"" + group->m_frame.get_label() + "\"\n";

		snprintf(tmp, sizeof(tmp), "          pixelsPerXUnit: %f\n", group->m_pixelsPerXUnit);
		config += tmp;
		snprintf(tmp, sizeof(tmp), "          xAxisOffset:    %ld\n", group->m_xAxisOffset);
		config += tmp;

		switch(group->m_cursorConfig)
		{
			case WaveformGroup::CURSOR_NONE:
				config += "          cursorConfig:   none\n";
				break;

			case WaveformGroup::CURSOR_X_SINGLE:
				config += "          cursorConfig:   x_single\n";
				break;

			case WaveformGroup::CURSOR_Y_SINGLE:
				config += "          cursorConfig:   y_single\n";
				break;

			case WaveformGroup::CURSOR_X_DUAL:
				config += "          cursorConfig:   x_dual\n";
				break;

			case WaveformGroup::CURSOR_Y_DUAL:
				config += "          cursorConfig:   y_dual\n";
				break;
		}

		snprintf(tmp, sizeof(tmp), "          xcursor0:       %ld\n", group->m_xCursorPos[0]);
		config += tmp;
		snprintf(tmp, sizeof(tmp), "          xcursor1:       %ld\n", group->m_xCursorPos[1]);
		config += tmp;
		snprintf(tmp, sizeof(tmp), "          ycursor0:       %f\n", group->m_yCursorPos[0]);
		config += tmp;
		snprintf(tmp, sizeof(tmp), "          ycursor1:       %f\n", group->m_yCursorPos[1]);
		config += tmp;
	}

	//Splitters
	config += "    splitters:\n";
	for(auto split : m_splitters)
		idmap[split] = nextID++;
	for(auto split : m_splitters)
	{
		//Splitter config
		int id = idmap[split];
		snprintf(tmp, sizeof(tmp), "        - id:     %d\n", id);
		config += tmp;
		if(dynamic_cast<Gtk::HPaned*>(split) != NULL)
			config +=  "          dir:    h\n";
		else
			config +=  "          dir:    v\n";

		//Splitter position
		snprintf(tmp, sizeof(tmp), "          split:  %d\n", split->get_position());
		config += tmp;

		//Children
		snprintf(tmp, sizeof(tmp), "          child0: %d\n", idmap[split->get_child1()]);
		config += tmp;
		snprintf(tmp, sizeof(tmp), "          child1: %d\n", idmap[split->get_child2()]);
		config += tmp;
	}

	return config;
}

void OscilloscopeWindow::OnAlphaChanged()
{
	ClearAllPersistence();
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
		m_historyWindow.show();
	else
		m_historyWindow.hide();
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
	w->m_group = ngroup;
	w->get_parent()->remove(*w);
	ngroup->m_waveformBox.pack_start(*w);

	//TODO: move any measurements related to this trace to the new group?

	//Remove any groups that no longer have any waveform views in them,'
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
	ngroup->m_waveformBox.pack_start(*nw);
	nw->show();
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
	for(auto s : m_splitters)
	{
		auto first = s->get_child1();
		auto second = s->get_child2();
		if( (first != NULL) && (second == NULL) )
		{
			//If this is the top level splitter, we have no higher level to move it to
			if(s->get_parent() == &m_vbox)
			{
			}

			else
			{
				//TODO: move single occupant of empty splitter to parent
			}
		}
	}

	//Hide measurement display if there's no measurements in the group
	for(auto g : m_waveformGroups)
	{
		if(g->m_measurementColumns.empty())
			g->m_measurementFrame.hide();
		else
			g->m_measurementFrame.show_all();
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
	//Add to a random group for now
	DoAddChannel(chan, *m_waveformGroups.begin());
}

WaveformArea* OscilloscopeWindow::DoAddChannel(OscilloscopeChannel* chan, WaveformGroup* ngroup, WaveformArea* ref)
{
	auto decode = dynamic_cast<ProtocolDecoder*>(chan);
	if(decode)
		AddDecoder(decode);

	//Create the viewer
	auto w = new WaveformArea(
		chan->GetScope(),
		chan,
		this);
	w->m_group = ngroup;
	m_waveformAreas.emplace(w);

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
	//If we're about to remove the last viewer for a protocol decoder, forget about it
	auto chan = w->GetChannel();
	auto decode = dynamic_cast<ProtocolDecoder*>(chan);
	if(decode && (chan->GetRefCount() == 1) )
		m_decoders.erase(decode);

	//Get rid of the channel
	w->get_parent()->remove(*w);
	m_waveformAreas.erase(w);
	delete w;

	//Clean up in case it was the last channel in the group
	GarbageCollectGroups();
}

void OscilloscopeWindow::PollScopes()
{
	bool pending = true;
	while(pending)
	{
		pending = false;

		//TODO: better sync for multiple instruments (wait for all to trigger THEN download waveforms)
		for(auto scope : m_scopes)
		{
			double start = GetTime();
			Oscilloscope::TriggerMode status = scope->PollTriggerFifo();
			if(status > Oscilloscope::TRIGGER_MODE_COUNT)
			{
				//Invalid value, skip it
				continue;
			}
			m_tPoll += GetTime() - start;

			//If triggered, grab the data
			if(status != Oscilloscope::TRIGGER_MODE_TRIGGERED)
				continue;

			//If we have a LOT of waveforms ready, don't waste time rendering all of them.
			//Grab a big pile and only render the last.
			//TODO: batch render with persistence?
			if(scope->GetPendingWaveformCount() > 30)
			{
				for(size_t i=0; i<25; i++)
					OnWaveformDataReady(scope);
			}
			else
				OnWaveformDataReady(scope);

			//Update the views
			start = GetTime();
			for(auto w : m_waveformAreas)
			{
				if( (w->GetChannel()->GetScope() == scope) || (w->GetChannel()->GetScope() == NULL) )
					w->OnWaveformDataReady();
			}
			m_tView += GetTime() - start;

			//If there's more waveforms pending, keep going
			if(scope->HasPendingWaveforms())
				pending = true;
		}

		//Process pending draw calls before we do another polling cycle
		double start = GetTime();
		while(Gtk::Main::events_pending())
			Gtk::Main::iteration();
		m_tEvent += GetTime() - start;
	}
}

void OscilloscopeWindow::OnWaveformDataReady(Oscilloscope* scope)
{
	//make sure we close fully
	if(!is_visible())
		m_historyWindow.close();

	//Make sure we don't free the old waveform data
	//LogTrace("Detaching\n");
	for(size_t i=0; i<scope->GetChannelCount(); i++)
		scope->GetChannel(i)->Detach();

	//Download the data
	//LogTrace("Acquiring\n");
	double start = GetTime();
	scope->AcquireDataFifo();
	m_tAcquire += GetTime() - start;

	//Update the status
	UpdateStatusBar();

	//Update the measurements
	for(auto g : m_waveformGroups)
		g->RefreshMeasurements();

	//Update our protocol decoders
	start = GetTime();
	for(auto d : m_decoders)
		d->SetDirty();
	for(auto d : m_decoders)
		d->RefreshIfDirty();
	m_tDecode += GetTime() - start;

	//Update protocol analyzers
	for(auto a : m_analyzers)
		a->OnWaveformDataReady();

	//Update the history window
	m_historyWindow.OnWaveformDataReady(scope);

	m_tHistory += GetTime() - start;
}

void OscilloscopeWindow::UpdateStatusBar()
{
	//TODO: redo this for multiple scopes
	auto scope = m_scopes[0];
	char tmp[256];
	size_t trig_idx = scope->GetTriggerChannelIndex();
	OscilloscopeChannel* chan = scope->GetChannel(trig_idx);
	if(chan == NULL)
	{
		LogWarning("Trigger channel (index %zu) is NULL", trig_idx);
		return;
	}
	string name = chan->GetHwname();
	float voltage = scope->GetTriggerVoltage();
	if(voltage < 1)
		snprintf(tmp, sizeof(tmp), "%s %.0f mV", name.c_str(), voltage*1000);
	else
		snprintf(tmp, sizeof(tmp), "%s %.3f V", name.c_str(), voltage);
	m_triggerConfigLabel.set_label(tmp);
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
	for(auto scope : m_scopes)
		scope->Stop();
}

void OscilloscopeWindow::ArmTrigger(bool oneshot)
{
	for(auto scope : m_scopes)
	{
		if(oneshot)
			scope->StartSingleTrigger();
		else
			scope->Start();
	}
	m_tArm = GetTime();
}

/**
	@brief Called when the history view selects an old waveform
 */
void OscilloscopeWindow::OnHistoryUpdated()
{
	//Stop triggering if we select a saved waveform
	OnStop();

	//Update the measurements
	for(auto g : m_waveformGroups)
		g->RefreshMeasurements();

	//Update our protocol decoders
	for(auto d : m_decoders)
		d->SetDirty();
	for(auto d : m_decoders)
		d->RefreshIfDirty();

	//Update the views
	for(auto w : m_waveformAreas)
	{
		w->ClearPersistence();
		w->OnWaveformDataReady();
	}

	//Don't update the protocol analyzers, they should already have this waveform saved
}

void OscilloscopeWindow::RemoveHistory(TimePoint timestamp)
{
	for(auto a : m_analyzers)
		a->RemoveHistory(timestamp);
}

void OscilloscopeWindow::JumpToHistory(TimePoint timestamp)
{
	m_historyWindow.JumpToHistory(timestamp);
}
