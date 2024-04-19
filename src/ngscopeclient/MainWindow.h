/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg and contributors                                                         *
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

#include "FilterGraphEditor.h"
#include "ManageInstrumentsDialog.h"
#include "ProtocolAnalyzerDialog.h"
#include "TimebasePropertiesDialog.h"
#include "TriggerPropertiesDialog.h"

#include "../scopehal/PacketDecoder.h"

class MeasurementsDialog;
class MultimeterDialog;
class HistoryDialog;
class FileBrowser;

class SplitGroupRequest
{
public:
	SplitGroupRequest(std::shared_ptr<WaveformGroup> group, ImGuiDir direction, StreamDescriptor stream)
	: m_group(group)
	, m_direction(direction)
	, m_stream(stream)
	{
		auto schan = dynamic_cast<OscilloscopeChannel*>(stream.m_channel);
		if(schan)
			schan->AddRef();
	}

	SplitGroupRequest(const SplitGroupRequest& rhs)
	: m_group(rhs.m_group)
	, m_direction(rhs.m_direction)
	, m_stream(rhs.m_stream)
	{
		auto schan = dynamic_cast<OscilloscopeChannel*>(rhs.m_stream.m_channel);
		if(schan)
			schan->AddRef();
	}

	SplitGroupRequest& operator=(const SplitGroupRequest& /*rhs*/) =delete;

	~SplitGroupRequest()
	{
		auto schan = dynamic_cast<OscilloscopeChannel*>(m_stream.m_channel);
		if(schan)
			schan->Release();
	}

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
	MainWindow(std::shared_ptr<QueueHandle> queue);
	virtual ~MainWindow();

	void AddDialog(std::shared_ptr<Dialog> dlg);
	void RemoveFunctionGenerator(SCPIFunctionGenerator* gen);

	void OnScopeAdded(Oscilloscope* scope, bool createViews);

	void QueueSplitGroup(std::shared_ptr<WaveformGroup> group, ImGuiDir direction, StreamDescriptor stream)
	{ m_splitRequests.push_back(SplitGroupRequest(group, direction, stream)); }

	void ShowChannelProperties(OscilloscopeChannel* channel);
	void ShowTimebaseProperties();
	void ShowTriggerProperties();
	void ShowManageInstruments();
	void ShowSyncWizard(std::shared_ptr<TriggerGroup> group, Oscilloscope* secondary);

	bool IsChannelBeingDragged();
	StreamDescriptor GetChannelBeingDragged();

	void OnCursorMoved(int64_t offset);

	void NavigateToTimestamp(
		int64_t stamp,
		int64_t duration = 0,
		StreamDescriptor target = StreamDescriptor(nullptr, 0));

	/**
		@brief Update the timebase properties dialog
	 */
	void RefreshTimebasePropertiesDialog()
	{
		if(m_timebaseDialog)
			m_timebaseDialog->Refresh();
	}

	/**
		@brief Update the trigger properties dialog
	 */
	void RefreshTriggerPropertiesDialog()
	{
		if(m_triggerDialog)
			m_triggerDialog->Refresh();
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

	Filter* CreateFilter(
		const std::string& name,
		WaveformArea* area,
		StreamDescriptor initialStream,
		bool showProperties = true,
		bool addToArea = true);
	void FindAreaForStream(WaveformArea* area, StreamDescriptor stream);

	void OnFilterReconfigured(Filter* f);

	const std::vector<std::string>& GetEyeGradients()
	{ return m_eyeGradients; }

	std::string GetEyeGradientFriendlyName(std::string internalName)
	{ return m_eyeGradientFriendlyNames[internalName]; }

	/**
		@brief Return the measurements dialog, if we have one.
	 */
	std::shared_ptr<MeasurementsDialog> GetMeasurementsDialog(bool createIfNotExisting);

	void AddAreaForStreamIfNotAlreadyVisible(StreamDescriptor stream);

	/**
		@brief Returns the groups we have configured for our graph editor
	 */
	std::map<uintptr_t, std::string> GetGraphEditorGroups()
	{ return m_graphEditorGroups; }

protected:
	virtual void DoRender(vk::raii::CommandBuffer& cmdBuf);

	void CloseSession();

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// GUI handlers

	virtual void RenderUI();
		void MainMenu();
			void FileMenu();
				void FileRecentMenu();
			void ViewMenu();
			void AddMenu();
				void AddBERTMenu(
					std::vector<time_t>& timestamps,
					std::map<time_t, std::vector<std::string> >& reverseMap);
				void AddGeneratorMenu(
					std::vector<time_t>& timestamps,
					std::map<time_t, std::vector<std::string> >& reverseMap);
				void AddLoadMenu(
					std::vector<time_t>& timestamps,
					std::map<time_t, std::vector<std::string> >& reverseMap);
				void AddMiscMenu(
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
				void AddSDRMenu(
					std::vector<time_t>& timestamps,
					std::map<time_t, std::vector<std::string> >& reverseMap);
				void AddSpectrometerMenu(
					std::vector<time_t>& timestamps,
					std::map<time_t, std::vector<std::string> >& reverseMap);
				void AddVNAMenu(
					std::vector<time_t>& timestamps,
					std::map<time_t, std::vector<std::string> >& reverseMap);
				void AddChannelsMenu();
				void AddImportMenu();
				void AddGenerateMenu();
			void SetupMenu();
			void WindowMenu();
				void WindowAnalyzerMenu();
				void WindowGeneratorMenu();
				void WindowPSUMenu();
				void WindowMultimeterMenu();
				void WindowSCPIConsoleMenu();
			void DebugMenu();
			void HelpMenu();
		void Toolbar();
			void LoadToolbarIcons();
			void ToolbarButtons();
				void TriggerStartDropdown(float buttonsize);
				void TriggerSingleDropdown(float buttonsize);
				void TriggerForceDropdown(float buttonsize);
				void TriggerStopDropdown(float buttonsize);
				void DoTriggerDropdown(const char* action, std::shared_ptr<TriggerGroup>& group, bool& all);
		void DockingArea();

	void LoadGradients();
	void LoadGradient(const std::string& friendlyName, const std::string& internalName);
	std::map<std::string, std::string> m_eyeGradientFriendlyNames;
	std::vector<std::string> m_eyeGradients;

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

	///@brief Map of PSUs to power supply control dialogs
	std::map<SCPIPowerSupply*, std::shared_ptr<Dialog> > m_psuDialogs;

	///@brief Map of generators to generator control dialogs
	std::map<SCPIFunctionGenerator*, std::shared_ptr<Dialog> > m_generatorDialogs;

	///@brief Map of BERTs to BERT control dialogs
	std::map<std::shared_ptr<SCPIBERT>, std::shared_ptr<Dialog> > m_bertDialogs;

	///@brief Map of RF generators to generator control dialogs
	std::map<SCPIRFSignalGenerator*, std::shared_ptr<Dialog> > m_rfgeneratorDialogs;

	///@brief Map of loads to control dialogs
	std::map<SCPILoad*, std::shared_ptr<Dialog> > m_loadDialogs;

	///@brief Map of instruments to SCPI console dialogs
	std::map<SCPIInstrument*, std::shared_ptr<Dialog> > m_scpiConsoleDialogs;

	///@brief Map of channels to properties dialogs
	std::map<OscilloscopeChannel*, std::shared_ptr<Dialog> > m_channelPropertiesDialogs;

	///@brief Map of filters to analyzer dialogs
	std::map<PacketDecoder*, std::shared_ptr<ProtocolAnalyzerDialog> > m_protocolAnalyzerDialogs;

	///@brief Waveform groups
	std::vector<std::shared_ptr<WaveformGroup> > m_waveformGroups;

	///@brief Mutex for controlling access to m_waveformGroups
	std::recursive_mutex m_waveformGroupsMutex;

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

	///@brief Trigger properties
	std::shared_ptr<TriggerPropertiesDialog> m_triggerDialog;

	///@brief Manage instruments
	std::shared_ptr<ManageInstrumentsDialog> m_manageInstrumentsDialog;

	///@brief Persistence settings
	std::shared_ptr<Dialog> m_persistenceDialog;

	///@brief Lab notes
	std::shared_ptr<Dialog> m_notesDialog;

	///@brief Filter graph editor
	std::shared_ptr<FilterGraphEditor> m_graphEditor;

	///@brief Config blob for filter graph editor
	std::string m_graphEditorConfigBlob;

	///@brief Group IDs and names for the graph editor
	std::map<uintptr_t, std::string> m_graphEditorGroups;

	///@brief Measurements dialog
	std::shared_ptr<MeasurementsDialog> m_measurementsDialog;

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

	///@brief Pending requests to display a channel in a waveform area (from CreateFilter())
	std::set< std::pair<OscilloscopeChannel*, WaveformArea*> > m_pendingChannelDisplayRequests;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Session state

	///@brief Our session object
	Session m_session;

	///@brief True if a close-session request came in this frame
	bool m_sessionClosing;

	SCPITransport* MakeTransport(const std::string& trans, const std::string& args);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Serialization

	void OnOpenFile(bool online);
	void DoOpenFile(const std::string& sessionPath, bool online);
	bool PreLoadSessionFromYaml(const YAML::Node& node, const std::string& dataDir, bool online);
	bool LoadSessionFromYaml(const YAML::Node& node, const std::string& dataDir, bool online);
public:
	bool LoadUIConfiguration(int version, const YAML::Node& node);

	void OnGraphEditorConfigModified(const std::string& blob)
	{ m_graphEditorConfigBlob = blob; }

	const std::string& GetGraphEditorConfigBlob()
	{ return m_graphEditorConfigBlob; }

protected:
	void OnSaveAs();
	void DoSaveFile(const std::string& sessionPath);
	bool SaveSessionToYaml(YAML::Node& node, const std::string& dataDir);
	void SaveLabNotes(const std::string& dataDir);
	void LoadLabNotes(const std::string& dataDir);
	bool SetupDataDirectory(const std::string& dataDir);
	YAML::Node SerializeUIConfiguration();
	YAML::Node SerializeDialogs();
	bool LoadDialogs(const YAML::Node& node);

	void RenderFileBrowser();

	enum
	{
		BROWSE_OPEN_SESSION,
		BROWSE_SAVE_SESSION
	} m_fileBrowserMode;

	///@brief Browser for pending file loads
	std::shared_ptr<FileBrowser> m_fileBrowser;

	///@brief YAML structure for file we're currently loading
	std::vector<YAML::Node> m_fileBeingLoaded;

	///@brief True if we're actively loading a file
	bool m_fileLoadInProgress;

	///@brief Current session file path
	std::string m_sessionFileName;

	///@brief Current session data directory
	std::string m_sessionDataDir;

public:
	std::string GetDataDir()
	{ return m_sessionDataDir; }

protected:

	///@brief True if the pending file is to be opened online
	bool m_openOnline;

protected:

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Recent item lists

	/**
		@brief List of recently used instruments
	 */
	std::map<std::string, time_t> m_recentInstruments;

	void LoadRecentInstrumentList();
	void SaveRecentInstrumentList();

	/**
		@brief List of recently used files
	 */
	std::map<std::string, time_t> m_recentFiles;

	void LoadRecentFileList();
	void SaveRecentFileList();

public:
	void AddToRecentInstrumentList(SCPIInstrument* inst);

protected:

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Error handling

	std::string m_errorPopupTitle;
	std::string m_errorPopupMessage;

	bool m_showingLoadWarnings;
	bool m_loadConfirmationChecked;

	void RenderErrorPopup();
	void RenderLoadWarningPopup();
public:
	void ShowErrorPopup(const std::string& title, const std::string& msg);

protected:
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

	bool DropdownButton(const char* id, float height);

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
