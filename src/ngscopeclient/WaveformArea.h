/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg                                                                          *
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
	@brief Declaration of WaveformArea
 */
#ifndef WaveformArea_h
#define WaveformArea_h

class WaveformArea;
class WaveformGroup;
class MainWindow;

#include "TextureManager.h"
#include "Marker.h"

class WaveformToneMapArgs
{
public:
	WaveformToneMapArgs(ImVec4 channelColor, uint32_t w, uint32_t h)
	: m_red(channelColor.x)
	, m_green(channelColor.y)
	, m_blue(channelColor.z)
	, m_width(w)
	, m_height(h)
	{}

	float m_red;
	float m_green;
	float m_blue;
	uint32_t m_width;
	uint32_t m_height;
};

class EyeToneMapArgs
{
public:
	EyeToneMapArgs(uint32_t w, uint32_t h)
	: m_width(w)
	, m_height(h)
	{}

	uint32_t m_width;
	uint32_t m_height;
};

class ConstellationToneMapArgs
{
public:
	ConstellationToneMapArgs(uint32_t w, uint32_t h)
	: m_width(w)
	, m_height(h)
	{}

	uint32_t m_width;
	uint32_t m_height;
};

class WaterfallToneMapArgs
{
public:
	WaterfallToneMapArgs(uint32_t w, uint32_t h, uint32_t outwidth, uint32_t outheight, uint32_t o, float x)
	: m_width(w)
	, m_height(h)
	, m_outwidth(outwidth)
	, m_outheight(outheight)
	, m_offsetSamples(o)
	, m_xscale(x)
	{}

	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_outwidth;
	uint32_t m_outheight;
	uint32_t m_offsetSamples;
	float m_xscale;
};

class SpectrogramToneMapArgs
{
public:
	SpectrogramToneMapArgs(uint32_t w, uint32_t h, uint32_t outwidth, uint32_t outheight, uint32_t xo,
		float x, int32_t yo, float y)
	: m_width(w)
	, m_height(h)
	, m_outwidth(outwidth)
	, m_outheight(outheight)
	, m_xoff(xo)
	, m_xscale(x)
	, m_yoff(yo)
	, m_yscale(y)
	{}

	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_outwidth;
	uint32_t m_outheight;
	uint32_t m_xoff;
	float m_xscale;
	int32_t m_yoff;
	float m_yscale;
};

struct ConfigPushConstants
{
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
};

/**
	@brief State for a single peak label

	All positions/sizes are in waveform units, not screen units, so that they scale/move correctly with the waveform
 */
struct PeakLabel
{
	///@brief X axis position of the label's centroid
	int64_t m_labelXpos;

	///@brief Y axis position of the label's centroid
	float m_labelYpos;

	///@brief X axis position of the peak last refresh
	int64_t m_peakXpos;

	///@brief Y axis position of the peak last refresh
	float m_peakYpos;

	///@brief X axis size of the label (for collision detection)
	int64_t m_labelXsize;

	///@brief Y axis size of the label (for collision detection)
	int64_t m_labelYsize;

	/**
		@brief Alpha decay. Decays by a small amount per frame after the peak disappears

		255 = fully visible
		0 = invisible
		Negative = invisible for several frames (eventually will be garbage collected)
	 */
	int m_peakAlpha;

	///@brief Calculated FWHM of the peak
	float m_fwhm;
};

/**
	@brief Context data for a single channel being displayed within a WaveformArea
 */
class DisplayedChannel
{
public:
	DisplayedChannel(StreamDescriptor stream, Session& session);

	~DisplayedChannel();

	YAML::Node Serialize(IDTable& table) const;

	std::string GetName()
	{ return m_stream.GetName(); }

	StreamDescriptor GetStream()
	{ return m_stream; }

	std::shared_ptr<Texture> GetTexture()
	{ return m_texture; }

	void SetTexture(std::shared_ptr<Texture> tex)
	{ m_texture = tex; }

	void PrepareToRasterize(size_t x, size_t y);

	bool UpdateSize(ImVec2 newSize, MainWindow* top);

	AcceleratorBuffer<float>& GetRasterizedWaveform()
	{ return m_rasterizedWaveform; }

	/**
		@brief Return the X axis size of the rasterized waveform
	 */
	size_t GetRasterizedX()
	{ return m_rasterizedX; }

	/**
		@brief Return the Y axis size of the rasterized waveform
	 */
	size_t GetRasterizedY()
	{ return m_rasterizedY; }

	/**
		@brief Gets the pipeline for drawing uniform analog waveforms, creating it if necessary
	*/
	__attribute__((noinline))
	std::shared_ptr<ComputePipeline> GetUniformAnalogPipeline()
	{
		if(m_uniformAnalogComputePipeline == nullptr)
		{
			std::string base = "shaders/waveform-compute.";
			std::string suffix;
			if(ZeroHoldFlagSet())
				suffix += ".zerohold";
			if(g_hasShaderInt64)
				suffix += ".int64";
			m_uniformAnalogComputePipeline = std::make_shared<ComputePipeline>(
				base + "analog" + suffix + ".dense.spv", 2, sizeof(ConfigPushConstants));
		}

		return m_uniformAnalogComputePipeline;
	}

	/**
		@brief Gets the pipeline for drawing histogram waveforms, creating it if necessary
	*/
	__attribute__((noinline))
	std::shared_ptr<ComputePipeline> GetHistogramPipeline()
	{
		if(m_histogramComputePipeline == nullptr)
		{
			std::string base = "shaders/waveform-compute.";
			std::string suffix;
			if(g_hasShaderInt64)
				suffix += ".int64";
			m_histogramComputePipeline = std::make_shared<ComputePipeline>(
				base + "histogram" + suffix + ".dense.spv", 2, sizeof(ConfigPushConstants));
		}

		return m_histogramComputePipeline;
	}

	/**
		@brief Gets the pipeline for drawing sparse analog waveforms, creating it if necessary
	*/
	__attribute__((noinline))
	std::shared_ptr<ComputePipeline> GetSparseAnalogPipeline()
	{
		if(m_sparseAnalogComputePipeline == nullptr)
		{
			std::string base = "shaders/waveform-compute.";
			std::string suffix;
			int durationSSBOs = 0;
			if(ZeroHoldFlagSet())
			{
				suffix += ".zerohold";
				durationSSBOs++;
			}
			if(g_hasShaderInt64)
				suffix += ".int64";
			m_sparseAnalogComputePipeline = std::make_shared<ComputePipeline>(
				base + "analog" + suffix + ".spv", durationSSBOs + 4, sizeof(ConfigPushConstants));
		}

		return m_sparseAnalogComputePipeline;
	}

	/**
		@brief Gets the pipeline for drawing uniform digital waveforms, creating it if necessary
	*/
	__attribute__((noinline))
	std::shared_ptr<ComputePipeline> GetUniformDigitalPipeline()
	{
		if(m_uniformDigitalComputePipeline == nullptr)
		{
			std::string base = "shaders/waveform-compute.";
			std::string suffix;
			if(g_hasShaderInt64)
				suffix += ".int64";
			m_uniformDigitalComputePipeline = std::make_shared<ComputePipeline>(
				base + "digital" + suffix + ".dense.spv", 2, sizeof(ConfigPushConstants));
		}

		return m_uniformDigitalComputePipeline;
	}

	/**
		@brief Gets the pipeline for drawing sparse digital waveforms, creating it if necessary
	*/
	__attribute__((noinline))
	std::shared_ptr<ComputePipeline> GetSparseDigitalPipeline()
	{
		if(m_sparseDigitalComputePipeline == nullptr)
		{
			std::string base = "shaders/waveform-compute.";
			std::string suffix;
			int durationSSBOs = 0;	//TODO: support gaps
			if(g_hasShaderInt64)
				suffix += ".int64";
			m_sparseDigitalComputePipeline = std::make_shared<ComputePipeline>(
				base + "digital" + suffix + ".spv", durationSSBOs + 4, sizeof(ConfigPushConstants));
		}

		return m_sparseDigitalComputePipeline;
	}

	std::shared_ptr<ComputePipeline> GetToneMapPipeline()
	{ return m_toneMapPipe; }

	bool ZeroHoldFlagSet()
	{
		return m_stream.GetFlags() & Stream::STREAM_DO_NOT_INTERPOLATE;
		// TODO: Allow this to be overridden by a configuration option in the WaveformArea
	}

	bool IsDensePacked()
	{
		auto data = m_stream.GetData();
		if(dynamic_cast<UniformWaveformBase*>(data) != nullptr)
			return true;
		else
			return false;
	}

	bool ShouldFillUnder()
	{ return m_stream.GetFlags() & Stream::STREAM_FILL_UNDER; }

	bool ZeroHoldCursorBehaviour()
	{
		return ZeroHoldFlagSet();
	}

	bool ShouldMapDurations()
	{
		return ZeroHoldFlagSet() && !IsDensePacked();
		// Do not need durations if dense because each duration is "1"
	}

	bool IsPersistenceEnabled()
	{ return m_persistenceEnabled; }

	void SetPersistenceEnabled(bool b)
	{ m_persistenceEnabled = b; }

	AcceleratorBuffer<uint32_t>& GetIndexBuffer()
	{ return m_indexBuffer; }

	void SetYButtonPos(float y)
	{ m_yButtonPos = y; }

	float GetYButtonPos()
	{ return m_yButtonPos; }

	///@brief Active labels for peaks associated with the current waveform
	std::vector<PeakLabel> m_peakLabels;

	std::string m_colorRamp;

protected:
	StreamDescriptor m_stream;

	///@brief Parent session object
	Session& m_session;

	///@brief Buffer storing our rasterized waveform, prior to tone mapping
	AcceleratorBuffer<float> m_rasterizedWaveform;

	///@brief Buffer for X axis indexes (only used for sparse waveforms)
	AcceleratorBuffer<uint32_t> m_indexBuffer;

	///@brief X axis size of rasterized waveform
	size_t m_rasterizedX;

	///@brief Y axis size of rasterized waveform
	size_t m_rasterizedY;

	///@brief The texture storing our final rendered waveform
	std::shared_ptr<Texture> m_texture;

	///@brief X axis size of the texture as of last UpdateSize() call
	size_t m_cachedX;

	///@brief Y axis size of the texture as of last UpdateSize() call
	size_t m_cachedY;

	///@brief Persistence enable flag
	bool m_persistenceEnabled;

	///@brief Compute pipeline for tone mapping fp32 images to RGBA
	std::shared_ptr<ComputePipeline> m_toneMapPipe;

	///@brief Compute pipeline for rendering uniform analog waveforms
	std::shared_ptr<ComputePipeline> m_uniformAnalogComputePipeline;

	///@brief Compute pipeline for rendering histogram waveforms
	std::shared_ptr<ComputePipeline> m_histogramComputePipeline;

	///@brief Compute pipeline for rendering sparse analog waveforms
	std::shared_ptr<ComputePipeline> m_sparseAnalogComputePipeline;

	///@brief Compute pipeline for rendering uniform digital waveforms
	std::shared_ptr<ComputePipeline> m_uniformDigitalComputePipeline;

	///@brief Compute pipeline for rendering sparse digital waveforms
	std::shared_ptr<ComputePipeline> m_sparseDigitalComputePipeline;

	///@brief Y axis position of our button within the view
	float m_yButtonPos;
};

/**
	@brief A WaveformArea is a plot that displays one or more OscilloscopeChannel's worth of data

	WaveformArea's auto resize, and will collectively fill the entire client area of their parent window.
 */
class WaveformArea
{
public:
	WaveformArea(StreamDescriptor stream, std::shared_ptr<WaveformGroup> group, MainWindow* parent);
	virtual ~WaveformArea();

	bool Render(int iArea, int numAreas, ImVec2 clientArea);
	void RenderWaveformTextures(
		vk::raii::CommandBuffer& cmdbuf,
		std::vector<std::shared_ptr<DisplayedChannel> >& channels,
		bool clearPersistence);
	void ReferenceWaveformTextures();
	void ToneMapAllWaveforms(vk::raii::CommandBuffer& cmdbuf);

	size_t GetStreamCount()
	{ return m_displayedChannels.size(); }

	StreamDescriptor GetStream(size_t i)
	{ return m_displayedChannels[i]->GetStream(); }

	std::shared_ptr<DisplayedChannel> GetDisplayedChannel(size_t i)
	{ return m_displayedChannels[i]; }

	bool IsStreamBeingDisplayed(StreamDescriptor target);

	void AddStream(StreamDescriptor desc, bool persistence = false, const std::string& ramp = "eye-gradient-viridis");

	bool IsCompatible(StreamDescriptor desc);

	void RemoveStream(size_t i);

	void ClearPersistence();
	void ClearPersistenceOfChannel(OscilloscopeChannel* chan);

	bool IsChannelBeingDragged();
	StreamDescriptor GetChannelBeingDragged();

	/**
		@brief Gets the WaveformGroup for this area
	 */
	std::shared_ptr<WaveformGroup> GetGroup()
	{ return m_group; }

	TimePoint GetWaveformTimestamp();

protected:
	void ChannelButton(std::shared_ptr<DisplayedChannel> chan, size_t index);
	void RenderBackgroundGradient(ImVec2 start, ImVec2 size);
	void RenderGrid(ImVec2 start, ImVec2 size, std::map<float, float>& gridmap, float& vbot, float& vtop);
	void RenderYAxis(ImVec2 size, std::map<float, float>& gridmap, float vbot, float vtop);
	void RenderTriggerLevelArrows(ImVec2 start, ImVec2 size);
	void RenderBERLevelArrows(ImVec2 start, ImVec2 size);
	void RenderCursors(ImVec2 start, ImVec2 size);
	void RenderBERSamplingPoint(ImVec2 start, ImVec2 size);
	void CheckForScaleMismatch(ImVec2 start, ImVec2 size);
	void RenderEyePatternTooltip(ImVec2 start, ImVec2 size);
	void RenderWaveforms(ImVec2 start, ImVec2 size);
	void RenderAnalogWaveform(std::shared_ptr<DisplayedChannel> channel, ImVec2 start, ImVec2 size);
	void RenderEyeWaveform(std::shared_ptr<DisplayedChannel> channel, ImVec2 start, ImVec2 size);
	void RenderConstellationWaveform(std::shared_ptr<DisplayedChannel> channel, ImVec2 start, ImVec2 size);
	void RenderWaterfallWaveform(std::shared_ptr<DisplayedChannel> channel, ImVec2 start, ImVec2 size);
	void RenderSpectrogramWaveform(std::shared_ptr<DisplayedChannel> channel, ImVec2 start, ImVec2 size);
	void RenderSpectrumPeaks(ImDrawList* list, std::shared_ptr<DisplayedChannel> channel);
	void RenderDigitalWaveform(std::shared_ptr<DisplayedChannel> channel, ImVec2 start, ImVec2 size);
	void RenderProtocolWaveform(std::shared_ptr<DisplayedChannel> channel, ImVec2 start, ImVec2 size);
	void RenderComplexSignal(
		ImDrawList* list,
		int visleft, int visright,
		float xstart, float xend, float xoff,
		float ybot, float ymid, float ytop,
		std::string str,
		ImU32 color);
	void MakePathSignalBody(ImDrawList* list, float xstart, float xend, float ybot, float ymid, float ytop);
	void ToneMapAnalogOrDigitalWaveform(std::shared_ptr<DisplayedChannel> channel, vk::raii::CommandBuffer& cmdbuf);
	void ToneMapEyeWaveform(std::shared_ptr<DisplayedChannel> channel, vk::raii::CommandBuffer& cmdbuf);
	void ToneMapConstellationWaveform(std::shared_ptr<DisplayedChannel> channel, vk::raii::CommandBuffer& cmdbuf);
	void ToneMapWaterfallWaveform(std::shared_ptr<DisplayedChannel> channel, vk::raii::CommandBuffer& cmdbuf);
	void ToneMapSpectrogramWaveform(std::shared_ptr<DisplayedChannel> channel, vk::raii::CommandBuffer& cmdbuf);
	void RasterizeAnalogOrDigitalWaveform(
		std::shared_ptr<DisplayedChannel> channel,
		vk::raii::CommandBuffer& cmdbuf,
		bool clearPersistence);
	void PlotContextMenu();

	void DrawDropRangeMismatchMessage(
		ImDrawList* list,
		ImVec2 center,
		StreamDescriptor ourStream,
		StreamDescriptor theirStream);

	void DragDropOverlays(ImVec2 start, ImVec2 size, int iArea, int numAreas);
	void CenterLeftDropArea(ImVec2 start, ImVec2 size);
	void CenterRightDropArea(ImVec2 start, ImVec2 size);
	void EdgeDropArea(const std::string& name, ImVec2 start, ImVec2 size, ImGuiDir splitDir);

	void FilterMenu(std::shared_ptr<DisplayedChannel> chan);
	void FilterSubmenu(std::shared_ptr<DisplayedChannel> chan, const std::string& name, Filter::Category cat);

	float PixelsToYAxisUnits(float pix);
	float YAxisUnitsToPixels(float volt);
	float YAxisUnitsToYPosition(float volt);
	float YPositionToYAxisUnits(float y);
	float PickStepSize(float volts_per_half_span, int min_steps = 2, int max_steps = 5);

public:
	StreamDescriptor GetFirstAnalogStream();
	StreamDescriptor GetFirstEyeStream();
	StreamDescriptor GetFirstConstellationStream();
	StreamDescriptor GetFirstDensityFunctionStream();
	StreamDescriptor GetFirstAnalogOrDensityStream();

	/**
		@brief Returns true if the mouse is over a button, rather than the plot area

		Only valid after rendering is complete before start of the next render cycle
	 */
	bool IsMouseOverButtonAtEndOfRender()
	{ return m_mouseOverButton; }

protected:
	///@brief Cached plot width (excluding Y axis)
	float m_width;

	///@brief Cached plot height
	float m_height;

	///@brief Cached Y axis offset
	float m_yAxisOffset;

	///@brief Cached midpoint of the plot
	float m_ymid;

	///@brief Cached Y axis scale
	float m_pixelsPerYAxisUnit;

	///@brief Cached Y axis unit
	Unit m_yAxisUnit;

	///@brief Drag and drop of UI elements
	enum DragState
	{
		DRAG_STATE_NONE,
		DRAG_STATE_CHANNEL,
		DRAG_STATE_CHANNEL_LAST,
		DRAG_STATE_Y_AXIS,
		DRAG_STATE_TRIGGER_LEVEL,
		DRAG_STATE_TRIGGER_SECONDARY_LEVEL,
		DRAG_STATE_BER_LEVEL,
		DRAG_STATE_BER_BOTH,
		DRAG_STATE_PEAK_MARKER
	} m_dragState;

	///@brief The stream currently being dragged (invalid if m_dragState != DRAG_STATE_CHANNEL)
	StreamDescriptor m_dragStream;

	void OnMouseWheelPlotArea(float delta, float delta_h);
	void OnMouseWheelYAxis(float delta);
	void OnMouseUp();
	void OnDragUpdate();

	/**
		@brief The channels currently living within this WaveformArea

		TODO: make this a FlowGraphNode and just hook up inputs??
	 */
	std::vector<std::shared_ptr<DisplayedChannel>> m_displayedChannels;

	///@brief Waveform group containing us
	std::shared_ptr<WaveformGroup> m_group;

	///@brief Top level window object containing us
	MainWindow* m_parent;

	///@brief Time of last mouse movement
	double m_tLastMouseMove;

	///@brief True if mouse is over a trigger level arrow
	bool m_mouseOverTriggerArrow;

	///@brief True if mouse is over the BER sampling location
	bool m_mouseOverBERTarget;

	///@brief Current trigger level, if dragging
	float m_triggerLevelDuringDrag;

	///@brief Current X axis position during drag
	int64_t m_xAxisPosDuringDrag;

	///@brief The trigger we're configuring
	Trigger* m_triggerDuringDrag;

	///@brief The BERT channel we're configuring
	BERTInputChannel* m_bertChannelDuringDrag;

	///@brief Channels we're in the process of removing
	std::vector<std::shared_ptr<DisplayedChannel> > m_channelsToRemove;

	///@brief X axis position of the mouse at the most recent right click
	int64_t m_lastRightClickOffset;

	///@brief True if clearing persistence next render
	std::atomic<bool> m_clearPersistence;

	///@brief Height of a channel button
	float m_channelButtonHeight;

	///@brief Peak label being dragged, if any
	PeakLabel* m_dragPeakLabel;

	///@brief Offset, in pixels, from mouse to anchor point of peak being dragged
	ImVec2 m_dragPeakAnchorOffset;

	ImVec2 ClosestPointOnLineSegment(ImVec2 lineA, ImVec2 lineB, ImVec2 pt);

	///@brief True if the mouse cursor is over a channel button
	bool m_mouseOverButton;
};

typedef std::pair<WaveformArea*, size_t> DragDescriptor;

#endif


