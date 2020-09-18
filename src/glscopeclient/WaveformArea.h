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
	@brief  Declaration of WaveformArea
 */
#ifndef WaveformArea_h
#define WaveformArea_h

#include "WaveformGroup.h"
#include "FilterDialog.h"
#include "EdgeTrigger.h"

/**
	@brief Slightly more capable rectangle class
 */
class Rect : public Gdk::Rectangle
{
public:
	int get_left()
	{ return get_x(); }

	int get_top()
	{ return get_y(); }

	int get_right()
	{ return get_x() + get_width(); }

	int get_bottom()
	{ return get_y() + get_height(); }

	/**
		@brief moves all corners in by (dx, dy)
	 */
	void shrink(int dx, int dy)
	{
		set_x(get_x() + dx);
		set_y(get_y() + dy);
		set_width(get_width() - 2*dx);
		set_height(get_height() - 2*dy);
	}

	bool HitTest(int x, int y)
	{
		if( (x < get_left()) || (x > get_right()) )
			return false;
		if( (y < get_top()) || (y > get_bottom()) )
			return false;

		return true;
	}
};

class WaveformArea;

/**
	@brief GL buffers etc needed to render a single waveform
 */
class WaveformRenderData
{
public:
	WaveformRenderData(StreamDescriptor channel, WaveformArea* area)
	: m_area(area)
	, m_channel(channel)
	, m_geometryOK(false)
	, m_count(0)
	{}

	bool IsDigital()
	{ return m_channel.m_channel->GetType() == OscilloscopeChannel::CHANNEL_TYPE_DIGITAL; }

	WaveformArea*			m_area;

	//The channel of interest
	StreamDescriptor		m_channel;

	//True if everything is good to render
	bool					m_geometryOK;

	//SSBOs with waveform data
	ShaderStorageBuffer		m_waveformXBuffer;
	ShaderStorageBuffer		m_waveformYBuffer;
	ShaderStorageBuffer		m_waveformConfigBuffer;
	ShaderStorageBuffer		m_waveformIndexBuffer;

	//RGBA32 but only alpha actually used
	Texture					m_waveformTexture;

	//Number of samples in the buffer
	size_t					m_count;

	//OpenGL-mapped buffers for the data
	int64_t*				m_mappedXBuffer;
	float*					m_mappedYBuffer;
	bool*					m_mappedDigitalYBuffer;
	uint32_t*				m_mappedIndexBuffer;
	uint32_t*				m_mappedConfigBuffer;
	int64_t*				m_mappedConfigBuffer64;
	float*					m_mappedFloatConfigBuffer;

	//Map all buffers for download
	void MapBuffers(size_t width, bool update_waveform = true);
	void UnmapBuffers(bool update_waveform = true);
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

	//Helper to get all geometry that needs to be updated
	void GetAllRenderData(std::vector<WaveformRenderData*>& data);
	static void PrepareGeometry(WaveformRenderData* wdata, bool update_waveform, float alpha);
	void MapAllBuffers(bool update_y);
	void UnmapAllBuffers(bool update_y);

	void CenterTimestamp(int64_t time);

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

	void OnSingleClick(GdkEventButton* event, int64_t timestamp);
	void OnDoubleClick(GdkEventButton* event, int64_t timestamp);

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
		Gtk::CheckMenuItem m_statisticsItem;
		Gtk::MenuItem m_triggerItem;
			Gtk::Menu m_triggerMenu;
			Gtk::RadioMenuItem::Group m_triggerGroup;
			Gtk::RadioMenuItem m_risingTriggerItem;
			Gtk::RadioMenuItem m_fallingTriggerItem;
			Gtk::RadioMenuItem m_bothTriggerItem;
		Gtk::MenuItem m_couplingItem;
			Gtk::Menu m_couplingMenu;
				Gtk::RadioMenuItem::Group m_couplingGroup;
				Gtk::RadioMenuItem m_dc50CouplingItem;
				Gtk::RadioMenuItem m_dc1MCouplingItem;
				Gtk::RadioMenuItem m_ac1MCouplingItem;
				Gtk::RadioMenuItem m_gndCouplingItem;
		Gtk::MenuItem m_attenItem;
			Gtk::Menu m_attenMenu;
				Gtk::RadioMenuItem::Group m_attenGroup;
				Gtk::RadioMenuItem m_atten1xItem;
				Gtk::RadioMenuItem m_atten10xItem;
				Gtk::RadioMenuItem m_atten20xItem;
		Gtk::MenuItem m_bwItem;
			Gtk::Menu m_bwMenu;
				Gtk::RadioMenuItem::Group m_bwGroup;
				Gtk::RadioMenuItem m_bwFullItem;
				Gtk::RadioMenuItem m_bw200Item;
				Gtk::RadioMenuItem m_bw20Item;
	void UpdateContextMenu();
	bool m_updatingContextMenu;
	void OnHide();
	void OnTogglePersistence();

	void OnTriggerMode(EdgeTrigger::EdgeType type, Gtk::RadioMenuItem* item);
	void OnBandwidthLimit(int mhz, Gtk::RadioMenuItem* item);
	void OnCoupling(OscilloscopeChannel::CouplingType type, Gtk::RadioMenuItem* item);
	void OnAttenuation(double atten, Gtk::RadioMenuItem* item);
	void OnMoveNewRight();
	void OnMoveNewBelow();
	void OnMoveToExistingGroup(WaveformGroup* group);
	void OnCopyNewRight();
	void OnCopyNewBelow();
	void OnCopyToExistingGroup(WaveformGroup* group);
	void OnCursorConfig(WaveformGroup::CursorConfig config, Gtk::RadioMenuItem* item);
	void OnStatistics();

	void RefreshMeasurements();

	//Pending protocol decodes
	void OnProtocolDecode(std::string name);
	FilterDialog* m_decodeDialog;
	Filter* m_pendingDecode;
	void OnDecodeDialogResponse(int response);
	void OnDecodeReconfigureDialogResponse(int response);
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
	void InitializeWaveformPass();
	Program m_analogWaveformComputeProgram;
	Program m_digitalWaveformComputeProgram;
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

	//Persistence
	void RenderPersistenceOverlay();
	void InitializePersistencePass();
	Program m_persistProgram;
	VertexArray m_persistVAO;
	VertexBuffer m_persistVBO;

	//Eye pattern rendering
	void RenderEye();
	void InitializeEyePass();
	Program m_eyeProgram;
	VertexArray m_eyeVAO;
	VertexBuffer m_eyeVBO;
	Texture m_eyeTexture;
	Texture m_eyeColorRamp[6];

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
	void RenderCairoOverlays();
	void DoRenderCairoOverlays(Cairo::RefPtr< Cairo::Context > cr);
	void RenderCursors(Cairo::RefPtr< Cairo::Context > cr);
	void RenderInBandPower(Cairo::RefPtr< Cairo::Context > cr);
	void RenderInsertionBar(Cairo::RefPtr< Cairo::Context > cr);
	void RenderCursor(Cairo::RefPtr< Cairo::Context > cr, int64_t pos, Gdk::Color color, bool label_to_left);
	void RenderChannelLabel(Cairo::RefPtr< Cairo::Context > cr);
	void RenderEyeMask(Cairo::RefPtr< Cairo::Context > cr);
	void RenderDecodeOverlays(Cairo::RefPtr< Cairo::Context > cr);
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

	void ResetTextureFiltering();

	//Math helpers
	float PixelsToVolts(float pix);
	float VoltsToPixels(float volt);
	float VoltsToYPosition(float volt);
	float YPositionToVolts(float y);
	int64_t XPositionToXAxisUnits(float pix);
	int64_t PixelsToXAxisUnits(float pix);
	float XAxisUnitsToPixels(int64_t t);
	float XAxisUnitsToXPosition(int64_t t);
	float PickStepSize(float volts_per_half_span, int min_steps = 2, int max_steps = 5);
	template<class T> static size_t BinarySearchForGequal(T* buf, size_t len, T value);
	float GetValueAtTime(int64_t time_ps);

	void OnRemoveOverlay(StreamDescriptor filter);

	StreamDescriptor m_channel;						//The main waveform for this view
	StreamDescriptor m_selectedChannel;				//The selected channel (either m_channel or an overlay)
	OscilloscopeWindow* m_parent;

	std::vector<StreamDescriptor> m_overlays;		//List of protocol decoders drawn on top of the signal
	std::map<StreamDescriptor, int> m_overlayPositions;

	float m_pixelsPerVolt;
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
		LOC_XCURSOR_1
	} m_clickLocation;

	ClickLocation HitTest(double x, double y);

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
		DRAG_OVERLAY
	} m_dragState;

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
