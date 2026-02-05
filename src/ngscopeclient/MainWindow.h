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
#include "StreamBrowserDialog.h"
#include "TriggerPropertiesDialog.h"
#include "TutorialWizard.h"
#include "Workspace.h"
#include "imgui_markdown.h"

#include "../scopehal/PacketDecoder.h"

class MeasurementsDialog;
class HistoryDialog;
class FileBrowser;
class CreateFilterBrowser;

class SplitGroupRequest
{
public:
	SplitGroupRequest(
		std::shared_ptr<WaveformGroup> group,
		ImGuiDir direction,
		StreamDescriptor stream,
		std::string ramp)
	: m_group(group)
	, m_direction(direction)
	, m_stream(stream)
	, m_ramp(ramp)
	{
		auto schan = dynamic_cast<OscilloscopeChannel*>(stream.m_channel);
		if(schan)
			schan->AddRef();
	}

	SplitGroupRequest(const SplitGroupRequest& rhs)
	: m_group(rhs.m_group)
	, m_direction(rhs.m_direction)
	, m_stream(rhs.m_stream)
	, m_ramp(rhs.m_ramp)
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

	///@brief Color ramp request (may be blank if unspecified)
	std::string m_ramp;
};

/**
	@brief Pending request to dock a dialog as a top level tab (TODO other options)
 */
class DockDialogRequest
{
public:
	DockDialogRequest(std::shared_ptr<Dialog> dlg)
	: m_dlg(dlg)
	{}

	std::shared_ptr<Dialog> m_dlg;
};

/**
	@brief Top level application window
 */
class MainWindow : public VulkanWindow
{
public:
	MainWindow(std::shared_ptr<QueueHandle> queue);
	virtual ~MainWindow();

	static bool OnMemoryPressureStatic(MemoryPressureLevel level, MemoryPressureType type, size_t requestedSize);
	bool OnMemoryPressure(MemoryPressureLevel level, MemoryPressureType type, size_t requestedSize);
	void LogMemoryUsage();

	void AddDialog(std::shared_ptr<Dialog> dlg);
	void RemoveFunctionGenerator(std::shared_ptr<SCPIFunctionGenerator> gen);

	void OnScopeAdded(std::shared_ptr<Oscilloscope> scope, bool createViews);

	void QueueSplitGroup(
		std::shared_ptr<WaveformGroup> group,
		ImGuiDir direction,
		StreamDescriptor stream,
		std::string colorRamp)
	{ m_splitRequests.push_back(SplitGroupRequest(group, direction, stream, colorRamp)); }

	void ShowChannelProperties(OscilloscopeChannel* channel);
	void ShowInstrumentProperties(std::shared_ptr<Instrument> instrument);
	void ShowTriggerProperties();
	void ShowManageInstruments();
	void ShowSyncWizard(std::shared_ptr<TriggerGroup> group, std::shared_ptr<Oscilloscope> secondary);

	void OnCursorMoved(int64_t offset);

	///@brief Helper for making sure tooltips aren't obscured by the mouse
	static void SetTooltipPosition()
	{
		auto pos = ImGui::GetIO().MousePos;
		ImGui::SetNextWindowPos(ImVec2(pos.x + ImGui::GetFontSize(), pos.y), ImGuiCond_Always, ImVec2(0.0, 0.5));
	}

	void NavigateToTimestamp(
		int64_t stamp,
		int64_t duration = 0,
		StreamDescriptor target = StreamDescriptor(nullptr, 0));

	/**
		@brief Update the timebase properties dialog
	 */
	void RefreshStreamBrowserDialog()
	{
		if(m_streamBrowser)
			m_streamBrowser->FlushConfigCache();
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

	void SetStartupSession(const std::string& path)
	{ m_startupSession = path; }

	///@brief Gets a pointer to the tutorial wizard (if we have one open)
	std::shared_ptr<TutorialWizard> GetTutorialWizard()
	{ return m_tutorialDialog; }

protected:
	virtual void DoRender(vk::raii::CommandBuffer& cmdBuf);

	void CloseSession();
	void InitializeDefaultSession();

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// GUI handlers

	virtual void RenderUI();
		void MainMenu();
			void FileMenu();
				void FileRecentMenu();
			void ViewMenu();
			void AddMenu();
				void DoAddSubMenu(
					std::vector<time_t>& timestamps,
					std::map<time_t, std::vector<std::string> >& reverseMap,
					const std::string& typePretty,
					const std::string& defaultName,
					const std::string& typeInternal
					);
				void AddChannelsMenu();
				void AddImportMenu();
				void AddGenerateMenu();
			void SetupMenu();
			void WindowMenu();
				void WindowAnalyzerMenu();
				void WindowPSUMenu();
			void DebugMenu();
				void DebugSCPIConsoleMenu();
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
			void StatusBar(float height);

	void LoadGradients();
	void LoadGradient(const std::string& friendlyName, const std::string& internalName);
	std::map<std::string, std::string> m_eyeGradientFriendlyNames;
	std::vector<std::string> m_eyeGradients;

	void LoadFilterIcons();
	void LoadMiscIcons();
	void LoadStatusBarIcons();
	void LoadWaveformShapeIcons();
	void LoadAppIcon();

	///@brief Enable flag for main imgui demo window
	bool m_showDemo;

	///@brief Start position of the viewport minus the menu and toolbar
	ImVec2 m_workPos;

	///@brief Size position of the viewport minus the menu and toolbar
	ImVec2 m_workSize;

	///@brief Workspaces (can't be with other dialogs because they can contain other stuff)
	std::set< std::shared_ptr<Workspace> > m_workspaces;

	///@brief All dialogs and other pop-up UI elements
	std::set< std::shared_ptr<Dialog> > m_dialogs;

	///@brief Map of multimeters to meter control dialogs
	std::map<std::shared_ptr<SCPIMultimeter>, std::shared_ptr<Dialog> > m_meterDialogs;

	///@brief Map of PSUs to power supply control dialogs
	std::map<std::shared_ptr<SCPIPowerSupply>, std::shared_ptr<Dialog> > m_psuDialogs;

	///@brief Map of generators to generator control dialogs
	std::map<std::shared_ptr<SCPIFunctionGenerator>, std::shared_ptr<Dialog> > m_generatorDialogs;

	///@brief Map of BERTs to BERT control dialogs
	std::map<std::shared_ptr<SCPIBERT>, std::shared_ptr<Dialog> > m_bertDialogs;

	///@brief Map of RF generators to generator control dialogs
	std::map<std::shared_ptr<SCPIRFSignalGenerator>, std::shared_ptr<Dialog> > m_rfgeneratorDialogs;

	///@brief Map of loads to control dialogs
	std::map<std::shared_ptr<SCPILoad>, std::shared_ptr<Dialog> > m_loadDialogs;

	///@brief Map of instruments to SCPI console dialogs
	std::map<std::shared_ptr<SCPIInstrument>, std::shared_ptr<Dialog> > m_scpiConsoleDialogs;

	///@brief Map of channels to properties dialogs
	std::map<InstrumentChannel*, std::shared_ptr<Dialog> > m_channelPropertiesDialogs;

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

	///@brief Trigger properties
	std::shared_ptr<TriggerPropertiesDialog> m_triggerDialog;

	///@brief Manage instruments
	std::shared_ptr<ManageInstrumentsDialog> m_manageInstrumentsDialog;

	///@brief Persistence settings
	std::shared_ptr<Dialog> m_persistenceDialog;

	///@brief Lab notes
	std::shared_ptr<Dialog> m_notesDialog;

	///@brief Tutorial flow
	std::shared_ptr<TutorialWizard> m_tutorialDialog;

	///@brief Filter graph editor
	std::shared_ptr<FilterGraphEditor> m_graphEditor;

	///@brief Stream browser
	std::shared_ptr<StreamBrowserDialog> m_streamBrowser;

	///@brief Filter palette
	std::shared_ptr<CreateFilterBrowser> m_filterPalette;

	///@brief Config blob for filter graph editor
	std::string m_graphEditorConfigBlob;

	///@brief Group IDs and names for the graph editor
	std::map<uintptr_t, std::string> m_graphEditorGroups;

	///@brief Measurements dialog
	std::shared_ptr<MeasurementsDialog> m_measurementsDialog;

	void OnDialogClosed(const std::shared_ptr<Dialog>& dlg);

	///@brief Pending requests to split waveform groups
	std::vector<SplitGroupRequest> m_splitRequests;

	///@brief Pending requests to dock initial stuff
	std::shared_ptr<Workspace> m_initialWorkspaceDockRequest;

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

	///@brief Pending request to open a session
	std::string m_startupSession;

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
	void DoSaveFile(std::string sessionPath);
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

	///@brief Last window title set (glfw doesnt let us get this)
	std::string m_lastWindowTitle;

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
	void AddToRecentInstrumentList(std::shared_ptr<SCPIInstrument> inst);
	void RenameRecentInstrument(std::shared_ptr<SCPIInstrument> inst, const std::string& oldName);
	void RepathRecentInstrument(std::shared_ptr<SCPIInstrument> inst, const std::string& oldPath);

protected:

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Error handling

	std::string m_errorPopupTitle;
	std::string m_errorPopupMessage;

	bool m_showingLoadWarnings;
	bool m_loadConfirmationChecked;

	void RenderReconnectPopup();
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

	void ResetStyle();

	/**
		@brief Returns a font, given the name of a preference setting
	 */
	FontWithSize GetFontPref(const std::string& name)
	{
		auto desc = m_session.GetPreferences().GetFont(name.c_str());
		return std::pair<ImFont*, float>(m_fontmgr.GetFont(desc), desc.second);
	}

	ImU32 GetColorPref(const std::string& name)
	{ return m_session.GetPreferences().GetColor(name); }

	ImTextureID GetTexture(const std::string& name)
	{ return m_texmgr.GetTexture(name); }

	TextureManager* GetTextureManager()
	{ return &m_texmgr; }

	std::string GetIconForFilter(Filter* f);

	std::string GetIconForWaveformShape(FunctionGenerator::WaveShape shape);

	ImGui::MarkdownConfig GetMarkdownConfig();

protected:
	FontManager m_fontmgr;

	///@brief Map of filter types to class names
	std::map<std::type_index, std::string> m_filterIconMap;

	///@brief Map of Waveform Shapes to icons
	std::map<FunctionGenerator::WaveShape, std::string> m_waveformShapeIconMap;

	void UpdateFonts();

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Status bar
public:

	void AddStatusHelp(const std::string& icon, const std::string& text)
	{ m_statusHelp[icon] = text; }

protected:
	std::map<std::string, std::string> m_statusHelp;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Performance counters

protected:
	int64_t m_toneMapTime;

public:
	int64_t GetToneMapTime()
	{ return m_toneMapTime; }
};

#endif
