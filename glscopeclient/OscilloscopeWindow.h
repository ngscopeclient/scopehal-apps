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
	@brief Top-level window for the application
 */

#ifndef OscilloscopeWindow_h
#define OscilloscopeWindow_h

#include "../scopehal/Oscilloscope.h"
#include "WaveformArea.h"
#include "WaveformGroup.h"
#include "PreferenceDialog.h"
#include "ProtocolAnalyzerWindow.h"
#include "HistoryWindow.h"
#include "ScopeSyncWizard.h"
#include "PreferenceManager.h"

class PreferenceDialog;

/**
	@brief Main application window class for an oscilloscope
 */
class OscilloscopeWindow	: public Gtk::Window
{
public:
	OscilloscopeWindow(std::vector<Oscilloscope*> scopes, bool nodigital);
	~OscilloscopeWindow();

	void OnAutofitHorizontal();
	void OnZoomInHorizontal(WaveformGroup* group);
	void OnZoomOutHorizontal(WaveformGroup* group);
	void ClearPersistence(WaveformGroup* group, bool dirty = true);
	void ClearAllPersistence();

	void OnRemoveChannel(WaveformArea* w);

	//need to be public so it can be called by WaveformArea
	void OnMoveNew(WaveformArea* w, bool horizontal);
	void OnMoveNewRight(WaveformArea* w);
	void OnMoveNewBelow(WaveformArea* w);
	void OnMoveToExistingGroup(WaveformArea* w, WaveformGroup* ngroup);

	void OnCopyNew(WaveformArea* w, bool horizontal);
	void OnCopyNewRight(WaveformArea* w);
	void OnCopyNewBelow(WaveformArea* w);
	void OnCopyToExistingGroup(WaveformArea* w, WaveformGroup* ngroup);

	void OnAddChannel(OscilloscopeChannel* w);
	WaveformArea* DoAddChannel(OscilloscopeChannel* w, WaveformGroup* ngroup, WaveformArea* ref = NULL);

	size_t GetScopeCount()
	{ return m_scopes.size(); }

	Oscilloscope* GetScope(size_t i)
	{ return m_scopes[i]; }

	void HideHistory()
	{ m_btnHistory.set_active(0); }

	void OnHistoryUpdated(bool refreshAnalyzers = false);
	void RemoveHistory(TimePoint timestamp);

	void JumpToHistory(TimePoint timestamp);

	enum EyeColor
	{
		EYE_CRT,
		EYE_IRONBOW,
		EYE_KRAIN,
		EYE_RAINBOW,
		EYE_GRAYSCALE,
		EYE_VIRIDIS,

		NUM_EYE_COLORS
	};

	EyeColor GetEyeColor();

	double GetTraceAlpha()
	{ return m_alphaslider.get_value(); }

	//has to be public so ScopeApp can call it during startup
	void OnStart();

	//has to be public so ScopeSyncWizard can call it
	void ArmTrigger(bool oneshot);
	void OnStop();

	//Clean up the sync wizard
	void OnSyncComplete();

protected:
	void SetTitle();

	void SplitGroup(Gtk::Widget* frame, WaveformGroup* group, bool horizontal);
	void GarbageCollectGroups();

	//Menu/toolbar message handlers
	void OnStartSingle();
	void OnQuit();
	void OnHistory();
	void OnAlphaChanged();
	void OnRefreshConfig();

	void UpdateStatusBar();

	//Initialization
	void CreateWidgets(bool nodigital);

	//Widgets
	Gtk::VBox m_vbox;
		Gtk::MenuBar m_menu;
			Gtk::MenuItem m_fileMenuItem;
				Gtk::Menu m_fileMenu;
			Gtk::MenuItem m_setupMenuItem;
				Gtk::Menu m_setupMenu;
					Gtk::MenuItem m_setupSyncMenuItem;
					Gtk::MenuItem m_setupTriggerMenuItem;
						Gtk::Menu m_setupTriggerMenu;
					Gtk::MenuItem m_preferencesMenuItem;
			Gtk::MenuItem m_channelsMenuItem;
				Gtk::Menu m_channelsMenu;
			Gtk::MenuItem m_viewMenuItem;
				Gtk::Menu m_viewMenu;
					Gtk::MenuItem m_viewEyeColorMenuItem;
						Gtk::Menu m_viewEyeColorMenu;
							Gtk::RadioMenuItem::Group m_eyeColorGroup;
							Gtk::RadioMenuItem m_eyeColorCrtItem;
							Gtk::RadioMenuItem m_eyeColorGrayscaleItem;
							Gtk::RadioMenuItem m_eyeColorIronbowItem;
							Gtk::RadioMenuItem m_eyeColorKRainItem;
							Gtk::RadioMenuItem m_eyeColorRainbowItem;
							Gtk::RadioMenuItem m_eyeColorViridisItem;
		Gtk::HBox m_toolbox;
			Gtk::Toolbar m_toolbar;
				Gtk::ToolButton m_btnStart;
				Gtk::ToolButton m_btnStartSingle;
				Gtk::ToolButton m_btnStop;
				Gtk::ToggleToolButton m_btnHistory;
				Gtk::ToolButton m_btnRefresh;
				Gtk::ToolButton m_btnClearSweeps;
				Gtk::ToolButton m_btnFullscreen;
			Gtk::Label  m_alphalabel;
			Gtk::HScale m_alphaslider;
		//main app windows go here
		Gtk::HBox m_statusbar;
			Gtk::Label m_waveformRateLabel;
			Gtk::Label m_triggerConfigLabel;

	//All of the splitters
	std::set<Gtk::Paned*> m_splitters;

	//shared by all scopes/channels
	std::map<Oscilloscope*, HistoryWindow*> m_historyWindows;
	
	//Preferences state
	PreferenceManager m_preferences;

public:
	//All of the waveform groups and areas, regardless of where they live
	std::set<WaveformGroup*> m_waveformGroups;
	std::set<WaveformArea*> m_waveformAreas;

	//All of the protocol analyzers
	std::set<ProtocolAnalyzerWindow*> m_analyzers;

	//Event handlers
	bool OnTimer(int timer);
	bool PollScopes();

	//Menu event handlers
	void OnFileSave(bool saveToCurrentFile, bool saveLayout, bool saveWaveforms);
	void OnFileOpen();
	void DoFileOpen(std::string filename, bool loadLayout = true, bool loadWaveform = true, bool reconnect = true);
	void LoadInstruments(const YAML::Node& node, bool reconnect, IDTable& table);
	void LoadDecodes(const YAML::Node& node, IDTable& table);
	void LoadUIConfiguration(const YAML::Node& node, IDTable& table);
	void LoadWaveformData(std::string filename, IDTable& table);
	void LoadWaveformDataForScope(const YAML::Node& node, Oscilloscope* scope, std::string datadir, IDTable& table);
	void CloseSession();
	void OnEyeColorChanged(EyeColor color, Gtk::RadioMenuItem* item);
	void OnTriggerProperties(Oscilloscope* scope);
	void OnFullscreen();
	void OnClearSweeps();
	void OnTimebaseSettings();
	void OnScopeSync();
	void OnPreferences();

    void OnPreferenceDialogResponse(int response);

	//Protocol decoding etc
	void RefreshAllDecoders();

	virtual bool on_delete_event(GdkEventAny* any_event);

	Glib::RefPtr<Gtk::CssProvider> m_css;

	//Our scope connections
	std::vector<Oscilloscope*> m_scopes;

	//Status polling
	void OnWaveformDataReady(Oscilloscope* scope);
	void OnAllWaveformsUpdated();

	//Performance profiling
	double m_tArm;
	double m_tLastFlush;

	bool m_toggleInProgress;

	//Color ramp selected for eye patterns etc
	EyeColor m_eyeColor;

	//Path of the file we are currently working on (if any)
	std::string m_currentFileName;
	std::string m_currentDataDirName;

	std::string SerializeConfiguration(bool saveLayout, IDTable& table);
	std::string SerializeInstrumentConfiguration(IDTable& table);
	std::string SerializeDecodeConfiguration(IDTable& table);
	std::string SerializeUIConfiguration(IDTable& table);
	void SerializeWaveforms(IDTable& table);

	//Performance counters
	double m_tAcquire;
	double m_tDecode;
	double m_tView;
	double m_tHistory;
	double m_tPoll;
	int64_t m_totalWaveforms;

	//WFM/s performance info
	std::vector<double> m_lastWaveformTimes;
	PreferenceDialog* m_preferenceDialog{nullptr};

	//Fullscreen state
	bool m_fullscreen;
	Gdk::Rectangle m_originalRect;

	//Special processing needed for multi-scope synchronization
	bool m_multiScopeFreeRun;
	double m_tPrimaryTrigger;

	//Instrument sync wizard
	ScopeSyncWizard* m_scopeSyncWizard;
	bool m_syncComplete;
};

#endif
