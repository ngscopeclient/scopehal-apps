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
	@brief Declaration of MainWindow
 */
#ifndef MainWindow_h
#define MainWindow_h

#include "Dialog.h"
#include "Session.h"
#include "FontManager.h"
#include "TextureManager.h"
#include "VulkanWindow.h"
#include "WaveformGroup.h"

#include "TimebasePropertiesDialog.h"

class MultimeterDialog;
class HistoryDialog;

class SplitGroupRequest
{
public:
	SplitGroupRequest(std::shared_ptr<WaveformGroup> group, ImGuiDir direction, StreamDescriptor stream)
	: m_group(group)
	, m_direction(direction)
	, m_stream(stream)
	{}

	std::shared_ptr<WaveformGroup> m_group;
	ImGuiDir m_direction;
	StreamDescriptor m_stream;
};

/**
	@brief Top level application window
 */
class MainWindow : public VulkanWindow
{
public:
	MainWindow(vk::raii::Queue& queue);
	virtual ~MainWindow();

	void AddDialog(std::shared_ptr<Dialog> dlg);
	void RemoveFunctionGenerator(SCPIFunctionGenerator* gen);

	void OnScopeAdded(Oscilloscope* scope);

	void QueueSplitGroup(std::shared_ptr<WaveformGroup> group, ImGuiDir direction, StreamDescriptor stream)
	{ m_splitRequests.push_back(SplitGroupRequest(group, direction, stream)); }

	void ShowChannelProperties(OscilloscopeChannel* channel);
	void ShowTimebaseProperties();

	bool IsChannelBeingDragged();
	StreamDescriptor GetChannelBeingDragged();

	void NavigateToTimestamp(int64_t stamp);

	/**
		@brief Update the timebase properties dialog
	 */
	void RefreshTimebasePropertiesDialog()
	{
		if(m_timebaseDialog)
			m_timebaseDialog->Refresh();
	}

	void ToneMapAllWaveforms(vk::raii::CommandBuffer& cmdbuf);

	void RenderWaveformTextures(
		vk::raii::CommandBuffer& cmdbuf,
		std::vector<std::shared_ptr<DisplayedChannel> >& channels);

	void SetNeedRender()
	{ m_needRender = true; }

	void ClearPersistence()
	{
		m_clearPersistence = true;
		SetNeedRender();
	}

	virtual void Render();

	void QueueCloseSession()
	{ m_sessionClosing = true; }

	Session& GetSession()
	{ return m_session; }

	float GetTraceAlpha()
	{ return m_traceAlpha; }

	float GetPersistDecay()
	{ return m_persistenceDecay; }

	void SetPersistDecay(float f)
	{ m_persistenceDecay = f; }

	void CreateFilter(const std::string& name, WaveformArea* area, StreamDescriptor initialStream);
	void FindAreaForStream(WaveformArea* area, StreamDescriptor stream);

	void OnFilterReconfigured(Filter* f);

protected:
	virtual void DoRender(vk::raii::CommandBuffer& cmdBuf);

	void CloseSession();

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// GUI handlers

	virtual void RenderUI();
		void MainMenu();
			void FileMenu();
			void ViewMenu();
			void AddMenu();
				void AddGeneratorMenu(
					std::vector<time_t>& timestamps,
					std::map<time_t, std::vector<std::string> >& reverseMap);
				void AddMultimeterMenu(
					std::vector<time_t>& timestamps,
					std::map<time_t, std::vector<std::string> >& reverseMap);
				void AddOscilloscopeMenu(
					std::vector<time_t>& timestamps,
					std::map<time_t, std::vector<std::string> >& reverseMap);
				void AddPowerSupplyMenu(
					std::vector<time_t>& timestamps,
					std::map<time_t, std::vector<std::string> >& reverseMap);
				void AddRFGeneratorMenu(
					std::vector<time_t>& timestamps,
					std::map<time_t, std::vector<std::string> >& reverseMap);
				void AddChannelsMenu();
				void AddImportMenu();
				void AddGenerateMenu();
			void SetupMenu();
			void WindowMenu();
				void WindowGeneratorMenu();
				void WindowMultimeterMenu();
				void WindowSCPIConsoleMenu();
			void DebugMenu();
			void HelpMenu();
		void Toolbar();
			void LoadToolbarIcons();
			void ToolbarButtons();
		void DockingArea();

	///@brief Enable flag for main imgui demo window
	bool m_showDemo;

	///@brief Enable flag for implot demo window
	bool m_showPlot;

	///@brief Start position of the viewport minus the menu and toolbar
	ImVec2 m_workPos;

	///@brief Size position of the viewport minus the menu and toolbar
	ImVec2 m_workSize;

	///@brief All dialogs and other pop-up UI elements
	std::set< std::shared_ptr<Dialog> > m_dialogs;

	///@brief Map of multimeters to meter control dialogs
	std::map<SCPIMultimeter*, std::shared_ptr<Dialog> > m_meterDialogs;

	///@brief Map of generators to generator control dialogs
	std::map<SCPIFunctionGenerator*, std::shared_ptr<Dialog> > m_generatorDialogs;

	///@brief Map of RF generators to generator control dialogs
	std::map<SCPIRFSignalGenerator*, std::shared_ptr<Dialog> > m_rfgeneratorDialogs;

	///@brief Map of instruments to SCPI console dialogs
	std::map<SCPIInstrument*, std::shared_ptr<Dialog> > m_scpiConsoleDialogs;

	///@brief Map of channels to properties dialogs
	std::map<OscilloscopeChannel*, std::shared_ptr<Dialog> > m_channelPropertiesDialogs;

	///@brief Waveform groups
	std::vector<std::shared_ptr<WaveformGroup> > m_waveformGroups;

	///@brief Set of newly created waveform groups that aren't yet docked
	std::vector<std::shared_ptr<WaveformGroup> > m_newWaveformGroups;

	///@brief Name for next autogenerated waveform group
	int m_nextWaveformGroup;

	std::string NameNewWaveformGroup();

	///@brief Logfile viewer
	std::shared_ptr<Dialog> m_logViewerDialog;

	///@brief Performance metrics
	std::shared_ptr<Dialog> m_metricsDialog;

	///@brief Preferences
	std::shared_ptr<Dialog> m_preferenceDialog;

	///@brief History
	std::shared_ptr<HistoryDialog> m_historyDialog;

	///@brief Timebase properties
	std::shared_ptr<TimebasePropertiesDialog> m_timebaseDialog;

	///@brief Persistence settings
	std::shared_ptr<Dialog> m_persistenceDialog;

	///@brief Filter graph editor
	std::shared_ptr<Dialog> m_graphEditor;

	void OnDialogClosed(const std::shared_ptr<Dialog>& dlg);

	///@brief Pending requests to split waveform groups
	std::vector<SplitGroupRequest> m_splitRequests;

	///@brief Pending requests to close waveform groups
	std::vector<size_t> m_groupsToClose;

	std::shared_ptr<WaveformGroup> GetBestGroupForWaveform(StreamDescriptor stream);

	///@brief Cached toolbar icon size
	int m_toolbarIconSize;

	///@brief Trace alpha
	float m_traceAlpha;

	///@brief Persistence decay factor
	float m_persistenceDecay;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Session state

	///@brief Our session object
	Session m_session;

	///@brief True if a close-session request came in this frame
	bool m_sessionClosing;

	SCPITransport* MakeTransport(const std::string& trans, const std::string& args);

protected:

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Recent item lists

	/**
		@brief List of recently used instruments
	 */
	std::map<std::string, time_t> m_recentInstruments;

	void LoadRecentInstrumentList();
	void SaveRecentInstrumentList();

public:
	void AddToRecentInstrumentList(SCPIInstrument* inst);

protected:

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Error handling

	std::string m_errorPopupTitle;
	std::string m_errorPopupMessage;

	void RenderErrorPopup();
	void ShowErrorPopup(const std::string& title, const std::string& msg);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Graphics items

	TextureManager m_texmgr;

	/**
		@brief True if a resize or other event this frame requires we re-rasterize waveforms

		(even if data has not changed)
	 */
	bool m_needRender;

	/**
		@brief True if we should clear persistence on the next render pass
	 */
	std::atomic<bool> m_clearPersistence;

	///@brief Command pool for allocating our command buffers
	std::unique_ptr<vk::raii::CommandPool> m_cmdPool;

	///@brief Command buffer used during rendering operations
	std::unique_ptr<vk::raii::CommandBuffer> m_cmdBuffer;

public:

	/**
		@brief Returns a font, given the name of a preference setting
	 */
	ImFont* GetFontPref(const std::string& name)
	{ return m_fontmgr.GetFont(m_session.GetPreferences().GetFont(name.c_str())); }

	ImTextureID GetTexture(const std::string& name)
	{ return m_texmgr.GetTexture(name); }

	TextureManager* GetTextureManager()
	{ return &m_texmgr; }

protected:
	FontManager m_fontmgr;

	void UpdateFonts();

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Performance counters

protected:
	int64_t m_toneMapTime;

public:
	int64_t GetToneMapTime()
	{ return m_toneMapTime; }
};

#endif
