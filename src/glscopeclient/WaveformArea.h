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
	@brief  Declaration of WaveformArea
 */
#ifndef WaveformArea_h
#define WaveformArea_h

#include "WaveformGroup.h"
#include "FilterDialog.h"
#include "EdgeTrigger.h"
#include "Rect.h"

class WaveformArea;
class EyeWaveform;
class SpectrogramWaveform;
class PacketDecoder;
class Marker;
class ComputePipeline;

extern bool g_noglint64;

/**
	@brief GL buffers etc needed to render a single waveform
 */
class WaveformRenderData
{
public:
	WaveformRenderData(StreamDescriptor channel, WaveformArea* area)
	: m_area(area)
	, m_shaderDense()
	, m_shaderSparse()
	, m_vkCmdPool()
	, m_vkCmdBuf()
	, m_channel(channel)
	, m_geometryOK(false)
	, m_count(0)
	, m_persistence(false)
	{
		if(IsAnalog() || IsDigital())
		{
			std::string shaderfn = "shaders/waveform-compute.";

			if(IsHistogram())
				shaderfn += "histogram";
			else if(IsAnalog())
				shaderfn += "analog";
			else if(IsDigital())
				shaderfn += "digital";

			int durationsSSBOs = 0;

			if(ZeroHoldFlagSet())
			{
				// TODO: Need to be able to dispatch this at runtime once we grow a UI setting for interpolation behavior
				shaderfn += ".zerohold";
				durationsSSBOs = 1;
			}

			if (GLEW_ARB_gpu_shader_int64 && !g_noglint64)
				shaderfn += ".int64";
			
			std::string denseShaderFn = shaderfn + ".dense.spv";
			std::string sparseShaderFn = shaderfn + ".spv";
			m_shaderDense = std::make_shared<ComputePipeline>(denseShaderFn, 2, sizeof(ConfigPushConstants));
			m_shaderSparse = std::make_shared<ComputePipeline>(sparseShaderFn, durationsSSBOs + 4, sizeof(ConfigPushConstants));

			vk::CommandPoolCreateInfo poolInfo(
				vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				g_computeQueueType );
			m_vkCmdPool = std::make_unique<vk::raii::CommandPool>(*g_vkComputeDevice, poolInfo);

			vk::CommandBufferAllocateInfo bufinfo(**m_vkCmdPool, vk::CommandBufferLevel::ePrimary, 1);
			m_vkCmdBuf = std::make_unique<vk::raii::CommandBuffer>(std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));
		}
	}

	~WaveformRenderData()
	{
		m_vkCmdBuf = nullptr;
		m_vkCmdPool = nullptr;
	}

	bool IsAnalog()
	{ return m_channel.GetType() == Stream::STREAM_TYPE_ANALOG; }

	bool IsDigital()
	{ return m_channel.GetType() == Stream::STREAM_TYPE_DIGITAL; }

	bool IsHistogram()
	{ return m_channel.GetYAxisUnits() == Unit(Unit::UNIT_COUNTS_SCI); }

	bool ZeroHoldFlagSet()
	{
		return m_channel.GetFlags() & Stream::STREAM_DO_NOT_INTERPOLATE;
		// TODO: Allow this to be overridden by a configuration option in the WaveformArea
	}

	bool ZeroHoldCursorBehaviour()
	{
		return ZeroHoldFlagSet() || IsHistogram();
		// Histogram included here to avoid interpolating count values inside bins
	}

	bool ShouldMapDurations()
	{
		return ZeroHoldFlagSet() && !IsDensePacked();
		// Do not need durations if dense because each duration is "1"
	}

	bool IsDensePacked()
	{
		auto data = m_channel.m_channel->GetData(0);
		if(dynamic_cast<UniformWaveformBase*>(data) != nullptr)
			return true;
		else
			return false;
	}

	WaveformArea*			m_area;

	std::shared_ptr<ComputePipeline> m_shaderDense;
	std::shared_ptr<ComputePipeline> m_shaderSparse;
	//Command pool and buffer for compute shaders
	std::unique_ptr<vk::raii::CommandPool> m_vkCmdPool;
	std::unique_ptr<vk::raii::CommandBuffer> m_vkCmdBuf;

	//The channel of interest
	StreamDescriptor		m_channel;

	//True if everything is good to render
	bool					m_geometryOK;

	//Render compute shader configuration constants
	struct ConfigPushConstants {
		int64_t innerXoff;
		uint32_t windowHeight;
		uint32_t windowWidth;
		uint32_t memDepth;
		uint32_t offset_samples;
		float alpha;
		float xoff;
		float xscale;
		float ybase;
		float yscale;
		float yoff;
		float persistScale;
	} m_config;
	
	//Indexes for rendering of spares waveforms
	AcceleratorBuffer<int64_t> m_indexBuffer;
	//Rendered waveform data, 1 float per pixel
	AcceleratorBuffer<float> m_renderedWaveform;
	//Texture to copy m_renderedWaveform into for final compositing
	Texture m_waveformTexture;

	//Number of samples in the buffer
	size_t					m_count;

	//Persistence flags
	bool					m_persistence;

	//Calculate number of points we'll need to draw (m_count)
	void UpdateCount();
};

float sinc(float x, float width);
float blackman(float x, float width);

class OscilloscopeWindow;

class WaveformArea : public Gtk::GLArea
{
public:
	WaveformArea(StreamDescriptor channel, OscilloscopeWindow* parent);
	WaveformArea(const WaveformArea* clone);
	virtual ~WaveformArea();

	void OnWaveformDataReady();

	StreamDescriptor GetChannel()
	{ return m_channel; }

	///@brief Get the top level frame for the group
	Gtk::Widget* GetGroupFrame()
	{ return &m_group->m_frame; }

	void ClearPersistence(bool geometry_dirty = true)
	{
		m_persistenceClear = true;
		if(geometry_dirty)
			SetGeometryDirty();
	}

	void SetGeometryDirty()
	{ m_geometryDirty = true; }

	void SetPositionDirty()
	{ m_positionDirty = true; }

	void SetNotDirty()
	{
		m_positionDirty = false;
		m_geometryDirty = false;
	}

	WaveformGroup* m_group;

	//Helpers for figuring out what kind of signal our primary trace is
	bool IsAnalog();
	bool IsDigital();
	bool IsSpectrogram();
	bool IsEye();
	bool IsEyeOrBathtub();
	bool IsWaterfall();
	bool IsTime();

	size_t GetOverlayCount()
	{ return m_overlays.size(); }

	StreamDescriptor GetOverlay(size_t i)
	{ return m_overlays[i]; }

	bool GetPersistenceEnabled()
	{ return m_persistence; }

	void SetPersistenceEnabled(bool enabled)
	{ m_persistence = enabled; }

	void AddOverlay(StreamDescriptor stream)
	{
		stream.m_channel->AddRef();
		m_overlays.push_back(stream);
	}

	int GetWidth()
	{ return m_width; }

	int GetHeight()
	{ return m_height; }

	float GetWidthXUnits()
	{ return PixelsToXAxisUnits(m_width); }

	float GetPlotWidthXUnits()
	{ return PixelsToXAxisUnits(m_plotRight); }

	float GetPlotWidthPixels()
	{ return m_plotRight; }

	//Helper to get all geometry that needs to be updated
	void UpdateCachedScales();
	void GetAllRenderData(std::vector<WaveformRenderData*>& data);
	static void PrepareGeometry(WaveformRenderData* wdata, bool update_waveform, float alpha, float persistDecay);
	void UpdateCounts();
	void CalculateOverlayPositions();

	void CenterPacket(int64_t time, int64_t len);
	void CenterMarker(int64_t time);

	static bool IsGLInitComplete()
	{ return m_isGlewInitialized; }

	void SyncFontPreferences();

	float GetPersistenceDecayCoefficient();

	//public so it can be called from OscilloscopeWindow because we don't get events directly for some reason
	//This should be fixed in GTK4, but that's a long ways off.
	virtual bool on_key_press_event(GdkEventKey* event);

protected:
	void SharedCtorInit();

	virtual void on_realize();
	virtual void on_unrealize();
	void CleanupGLHandles();
	virtual void on_resize (int width, int height);
	virtual bool on_render(const Glib::RefPtr<Gdk::GLContext>& context);
	virtual bool on_button_press_event(GdkEventButton* event);
	virtual bool on_button_release_event(GdkEventButton* event);
	virtual bool on_scroll_event (GdkEventScroll* ev);
	virtual bool on_motion_notify_event(GdkEventMotion* event);

	void OnSingleClick(GdkEventButton* event, int64_t timestamp, float voltage);
	void OnDoubleClick(GdkEventButton* event, int64_t timestamp);

	void RescaleEye(Filter* f, EyeWaveform* eye);

	void CreateWidgets();

	//Context menu
	Gtk::Menu m_contextMenu;
		Gtk::CheckMenuItem m_persistenceItem;
		Gtk::MenuItem m_cursorItem;
			Gtk::Menu m_cursorMenu;
				Gtk::RadioMenuItem::Group m_cursorGroup;
				Gtk::RadioMenuItem m_cursorNoneItem;
				Gtk::RadioMenuItem m_cursorSingleVerticalItem;
				Gtk::RadioMenuItem m_cursorDualVerticalItem;
				Gtk::RadioMenuItem m_cursorSingleHorizontalItem;
				Gtk::RadioMenuItem m_cursorDualHorizontalItem;
		Gtk::MenuItem m_markerItem;
			Gtk::Menu m_markerMenu;
				Gtk::MenuItem m_markerAddItem;
		Gtk::MenuItem m_moveItem;
			Gtk::Menu m_moveMenu;
				Gtk::MenuItem m_moveNewGroupBelowItem;
				Gtk::MenuItem m_moveNewGroupRightItem;
				std::set<Gtk::MenuItem*> m_moveExistingGroupItems;
		Gtk::MenuItem m_copyItem;
			Gtk::Menu m_copyMenu;
				Gtk::MenuItem m_copyNewGroupBelowItem;
				Gtk::MenuItem m_copyNewGroupRightItem;
				std::set<Gtk::MenuItem*> m_copyExistingGroupItems;
		Gtk::MenuItem m_decodeAlphabeticalItem;
			Gtk::Menu m_decodeAlphabeticalMenu;
		Gtk::MenuItem m_decodeBusItem;
			Gtk::Menu m_decodeBusMenu;
		Gtk::MenuItem m_decodeClockItem;
			Gtk::Menu m_decodeClockMenu;
		Gtk::MenuItem m_decodeRFItem;
			Gtk::Menu m_decodeRFMenu;
		Gtk::MenuItem m_decodeMathItem;
			Gtk::Menu m_decodeMathMenu;
		Gtk::MenuItem m_decodeMeasurementItem;
			Gtk::Menu m_decodeMeasurementMenu;
		Gtk::MenuItem m_decodeMemoryItem;
			Gtk::Menu m_decodeMemoryMenu;
		Gtk::MenuItem m_decodeMiscItem;
			Gtk::Menu m_decodeMiscMenu;
		Gtk::MenuItem m_decodePowerItem;
			Gtk::Menu m_decodePowerMenu;
		Gtk::MenuItem m_decodeSerialItem;
			Gtk::Menu m_decodeSerialMenu;
		Gtk::MenuItem m_decodeSignalIntegrityItem;
			Gtk::Menu m_decodeSignalIntegrityMenu;
		Gtk::MenuItem m_decodeWaveformGenerationItem;
			Gtk::Menu m_decodeWaveformGenerationMenu;
		Gtk::CheckMenuItem m_statisticsItem;
		Gtk::MenuItem m_couplingItem;
			Gtk::Menu m_couplingMenu;
				Gtk::RadioMenuItem::Group m_couplingGroup;
				Gtk::RadioMenuItem m_dc50CouplingItem;
				Gtk::RadioMenuItem m_ac50CouplingItem;
				Gtk::RadioMenuItem m_dc1MCouplingItem;
				Gtk::RadioMenuItem m_ac1MCouplingItem;
				Gtk::RadioMenuItem m_gndCouplingItem;
	void UpdateContextMenu();
	bool m_updatingContextMenu;
	void OnHide();
	void OnTogglePersistence();

	void OnCoupling(OscilloscopeChannel::CouplingType type, Gtk::RadioMenuItem* item);
	void OnMoveNewRight();
	void OnMoveNewBelow();
	void OnMoveToExistingGroup(WaveformGroup* group);
	void OnCopyNewRight();
	void OnCopyNewBelow();
	void OnCopyToExistingGroup(WaveformGroup* group);
	void OnCursorConfig(WaveformGroup::CursorConfig config, Gtk::RadioMenuItem* item);
	void OnStatistics();
	void OnMarkerAdd();

	void RefreshMeasurements();

	//Pending protocol decodes
	void OnProtocolDecode(std::string name, bool forceStats);
	FilterDialog* m_decodeDialog;
	Filter* m_pendingDecode;
	bool m_showPendingDecodeAsStats;
	bool OnDecodeDialogClosed(GdkEventAny* ignored);
	bool OnDecodeReconfigureDialogClosed(GdkEventAny* ignored);
	void OnDecodeSetupComplete();

	int m_width;
	int m_height;

	float m_cursorX;
	float m_cursorY;

	//Display options
	bool m_persistence;
	bool m_persistenceClear;

	// Whether GLEW is already initialized
	static bool m_isGlewInitialized;

	Framebuffer m_windowFramebuffer;

	//Trace rendering
	void RenderTrace(WaveformRenderData* wdata);
	WaveformRenderData*						m_waveformRenderData;
	std::map<StreamDescriptor, WaveformRenderData*>	m_overlayRenderData;

	//Final compositing
	void RenderMainTrace();
	void RenderOverlayTraces();

	//Color correction
	void RenderTraceColorCorrection(WaveformRenderData* wdata);
	void InitializeColormapPass();
	VertexArray m_colormapVAO;
	VertexBuffer m_colormapVBO;
	Program m_colormapProgram;

	//Eye pattern rendering
	void RenderEye();
	void InitializeEyePass();
	Program m_eyeProgram;
	VertexArray m_eyeVAO;
	VertexBuffer m_eyeVBO;
	Texture m_eyeTexture;
	std::map<std::string, Texture> m_eyeColorRamp;

	//Spectrogram rendering
	void RenderSpectrogram();
	void InitializeSpectrogramPass();
	VertexArray m_spectrogramVAO;
	VertexBuffer m_spectrogramVBO;
	Program m_spectrogramProgram;

	//Waterfall rendering
	void RenderWaterfall();

	//Cairo overlay rendering for text and protocol decode overlays
	void ComputeAndDownloadCairoUnderlays();
	void ComputeAndDownloadCairoOverlays();
	void RenderCairoUnderlays();
	void DoRenderCairoUnderlays(Cairo::RefPtr< Cairo::Context > cr);
	void RenderBackgroundGradient(Cairo::RefPtr< Cairo::Context > cr);
	void RenderGrid(Cairo::RefPtr< Cairo::Context > cr);
	void RenderTriggerArrow(Cairo::RefPtr< Cairo::Context > cr, float voltage, bool dragging, Gdk::Color color);
	void RenderTriggerTimeLine(Cairo::RefPtr< Cairo::Context > cr, int64_t time);
	void RenderTriggerLevelLine(Cairo::RefPtr< Cairo::Context > cr, float voltage);
	void RenderCairoOverlays();
	void DoRenderCairoOverlays(Cairo::RefPtr< Cairo::Context > cr);
	void RenderCursors(Cairo::RefPtr< Cairo::Context > cr);
	void RenderMarkers(Cairo::RefPtr< Cairo::Context > cr);
	void RenderInBandPower(Cairo::RefPtr< Cairo::Context > cr);
	void RenderInsertionBar(Cairo::RefPtr< Cairo::Context > cr);
	void RenderVerticalCursor(Cairo::RefPtr< Cairo::Context > cr, int64_t pos, Gdk::Color color, bool label_to_left);
	void RenderHorizontalCursor(
		Cairo::RefPtr< Cairo::Context > cr,
		float pos,
		Gdk::Color color,
		bool upper,
		bool show_delta);
	void RenderChannelLabel(Cairo::RefPtr< Cairo::Context > cr);
	void RenderEyeMask(Cairo::RefPtr< Cairo::Context > cr);
	void RenderDecodeOverlays(Cairo::RefPtr< Cairo::Context > cr);
	void RenderFFTPeaks(Cairo::RefPtr< Cairo::Context > cr);
	void InitializeCairoPass();
	Texture m_cairoTexture;
	Texture m_cairoTextureOver;
	VertexArray m_cairoVAO;
	VertexBuffer m_cairoVBO;
	Program m_cairoProgram;

	//Helpers for rendering and such
	void RenderChannelInfoBox(
		StreamDescriptor chan,
		Cairo::RefPtr< Cairo::Context > cr,
		int bottom,
		std::string text,
		Rect& box,
		int labelmargin = 3);
	void RenderChannelInfoIssueBox(
		StreamDescriptor chan,
		Cairo::RefPtr< Cairo::Context > cr,
		std::string text,
		Rect& box,
		int labelmargin = 3);
	void MakePathRoundedRect(
		Cairo::RefPtr< Cairo::Context > cr,
		Rect& box,
		int rounding);
	void RenderComplexSignal(
		const Cairo::RefPtr<Cairo::Context>& cr,
		int visleft, int visright,
		float xstart, float xend, float xoff,
		float ystart, float ymid, float ytop,
		std::string str,
		Gdk::Color color);
	void MakePathSignalBody(
		const Cairo::RefPtr<Cairo::Context>& cr,
		float xstart, float xoff, float xend, float ybot, float ymid, float ytop);
	void RemoveOverlaps(std::vector<Rect>& rects, std::vector<vec2f>& peaks);

	void ResetTextureFiltering();

	//Math helpers
	float PixelToYAxisUnits(float pix);
	float YAxisUnitsToPixels(float volt);
	float YAxisUnitsToYPosition(float volt);
	float YPositionToYAxisUnits(float y);
	int64_t XPositionToXAxisUnits(float pix);
	int64_t PixelsToXAxisUnits(float pix);
	float XAxisUnitsToPixels(int64_t t);
	float XAxisUnitsToXPosition(int64_t t);
	float PickStepSize(float volts_per_half_span, int min_steps = 2, int max_steps = 5);
	template<class T> static size_t BinarySearchForGequal(T* buf, size_t len, T value);
	std::pair<bool, float> GetValueAtTime(int64_t time_fs);

	float GetDPIScale()
	{ return get_pango_context()->get_resolution() / 96; }

	void OnRemoveOverlay(StreamDescriptor filter);

	StreamDescriptor m_channel;						//The main waveform for this view
	StreamDescriptor m_selectedChannel;				//The selected channel (either m_channel or an overlay)
	OscilloscopeWindow* m_parent;

	std::vector<StreamDescriptor> m_overlays;		//List of protocol decoders drawn on top of the signal
	std::map<StreamDescriptor, int> m_overlayPositions;

	float m_pixelsPerYAxisUnit;
	float m_padding;
	float m_plotRight;
	int m_overlaySpacing;

	//Positions of various UI elements used by hit testing
	Rect m_infoBoxRect;
	std::map<StreamDescriptor, Rect> m_overlayBoxRects;

	///Clickable UI elements
	enum ClickLocation
	{
		LOC_PLOT,
		LOC_VSCALE,
		LOC_TRIGGER,
		LOC_TRIGGER_SECONDARY,	//lower spot for window trigger or similar
		LOC_CHAN_NAME,
		LOC_XCURSOR_0,
		LOC_XCURSOR_1,
		LOC_YCURSOR_0,
		LOC_YCURSOR_1,
		LOC_MARKER
	} m_clickLocation;

	ClickLocation HitTest(double x, double y);
	StreamDescriptor GetWaveformAtPoint(int x, int y);
	int64_t SnapX(int64_t time, int x, int y);

	//Location of what the mouse was over last time it moved
	ClickLocation m_mouseElementPosition;

	enum DragStates
	{
		DRAG_NONE,
		DRAG_TRIGGER,
		DRAG_TRIGGER_SECONDARY,
		DRAG_CURSOR_0,
		DRAG_CURSOR_1,
		DRAG_OFFSET,
		DRAG_WAVEFORM_AREA,
		DRAG_OVERLAY,
		DRAG_MARKER
	} m_dragState;

	void OnCursorMoved(bool notifySiblings = true);
	void OnMarkerMoved(bool notifySiblings = true);
	void HighlightPacketAtTime(PacketDecoder* p, int64_t time);

	//Start voltage of a drag (only used in DRAG_OFFSET mode)
	double	m_dragStartVoltage;

	enum DragInsertionBar
	{
		INSERT_NONE,
		INSERT_BOTTOM,
		INSERT_BOTTOM_SPLIT,
		INSERT_TOP,
		INSERT_RIGHT_SPLIT
	} m_insertionBarLocation;

	//Destination of a drag (only used in DRAG_WAVEFORM_AREA mode)
	WaveformArea* m_dropTarget;

	//Destination of a drag (only used in DRAG_OVERLAY mode)
	int m_dragOverlayPosition;

	//Marker being dragged (only used in DRAG_MARKER mode)
	Marker* m_selectedMarker;
	std::vector<Marker*> GetMarkersForActiveWaveform();

	bool	m_firstFrame;
	bool	m_geometryDirty;
	bool	m_positionDirty;

	//Fonts used for drawing various UI elements
	Pango::FontDescription m_axisLabelFont;
	Pango::FontDescription m_infoBoxFont;
	Pango::FontDescription m_cursorLabelFont;
	Pango::FontDescription m_decodeFont;
};

#endif
