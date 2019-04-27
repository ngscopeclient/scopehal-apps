/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2019 Andrew D. Zonenberg                                                                          *
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
#include "ProtocolAnalyzerWindow.h"
#include "HistoryWindow.h"

/**
	@brief Main application window class for an oscilloscope
 */
class OscilloscopeWindow	: public Gtk::Window
{
public:
	OscilloscopeWindow(std::vector<Oscilloscope*> scopes);
	~OscilloscopeWindow();

	void OnAutofitHorizontal();
	void OnZoomInHorizontal(WaveformGroup* group);
	void OnZoomOutHorizontal(WaveformGroup* group);
	void ClearPersistence(WaveformGroup* group);
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

	void AddDecoder(ProtocolDecoder* decode)
	{ m_decoders.emplace(decode); }

	size_t GetScopeCount()
	{ return m_scopes.size(); }

	Oscilloscope* GetScope(size_t i)
	{ return m_scopes[i]; }

	void HideHistory()
	{ m_btnHistory.set_active(0); }

	void OnHistoryUpdated();
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

protected:
	void ArmTrigger(bool oneshot);

	void SplitGroup(Gtk::Widget* frame, WaveformGroup* group, bool horizontal);
	void GarbageCollectGroups();

	//Menu/toolbar message handlers
	void OnStart();
	void OnStartSingle();
	void OnStop();
	void OnQuit();
	void OnHistory();
	void OnAlphaChanged();

	void UpdateStatusBar();

	//Initialization
	void CreateWidgets();

	//Widgets
	Gtk::VBox m_vbox;
		Gtk::MenuBar m_menu;
			Gtk::MenuItem m_fileMenuItem;
				Gtk::Menu m_fileMenu;
			Gtk::MenuItem m_setupMenuItem;
				Gtk::Menu m_setupMenu;
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
		Gtk::Label  m_alphalabel;
		Gtk::HScale m_alphaslider;

	//All of the splitters
	std::set<Gtk::Paned*> m_splitters;

	//shared by all scopes/channels
	HistoryWindow m_historyWindow;

public:
	//All of the waveform groups and areas, regardless of where they live
	std::set<WaveformGroup*> m_waveformGroups;
	std::set<WaveformArea*> m_waveformAreas;
	std::set<ProtocolDecoder*> m_decoders;

	//All of the protocol analyzers
	std::set<ProtocolAnalyzerWindow*> m_analyzers;

	//Event handlers
	void PollScopes();

protected:
	Gtk::HBox m_statusbar;
		Gtk::Label m_triggerConfigLabel;

	void OnEyeColorChanged(EyeColor color, Gtk::RadioMenuItem* item);

	Glib::RefPtr<Gtk::CssProvider> m_css;

	//Our scope connections
	std::vector<Oscilloscope*> m_scopes;

	//Status polling
	void OnWaveformDataReady(Oscilloscope* scope);

	int OnCaptureProgressUpdate(float progress);

	double m_tArm;
	bool m_triggerOneShot;

	bool m_toggleInProgress;

	double m_tLastFlush;

	EyeColor m_eyeColor;

	//Performance counters
	double m_tAcquire;
	double m_tDecode;
	double m_tView;
	double m_tHistory;
	double m_tPoll;
	double m_tEvent;
};

#endif
