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
	@brief Declaration of WaveformArea
 */
#ifndef WaveformArea_h
#define WaveformArea_h

class WaveformArea;
class WaveformGroup;
class MainWindow;

#include "TextureManager.h"
#include "Marker.h"

class ToneMapArgs
{
public:
	ToneMapArgs(ImVec4 channelColor, uint32_t w, uint32_t h)
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
	@brief Context data for a single channel being displayed within a WaveformArea
 */
class DisplayedChannel
{
public:
	DisplayedChannel(StreamDescriptor stream);

	~DisplayedChannel()
	{
		m_stream.m_channel->Release();
	}

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
				base + "analog" + suffix + ".sparse.spv", durationSSBOs + 4, sizeof(ConfigPushConstants));
		}

		return m_sparseAnalogComputePipeline;
	}

	ComputePipeline& GetToneMapPipeline()
	{ return m_toneMapPipe; }

	bool ZeroHoldFlagSet()
	{
		return m_stream.GetFlags() & Stream::STREAM_DO_NOT_INTERPOLATE;
		// TODO: Allow this to be overridden by a configuration option in the WaveformArea
	}

	bool IsDensePacked()
	{
		auto data = m_stream.m_channel->GetData(0);
		if(dynamic_cast<UniformWaveformBase*>(data) != nullptr)
			return true;
		else
			return false;
	}

	bool IsHistogram()
	{ return m_stream.GetYAxisUnits() == Unit(Unit::UNIT_COUNTS_SCI); }

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

protected:
	StreamDescriptor m_stream;

	///@brief Buffer storing our rasterized waveform, prior to tone mapping
	AcceleratorBuffer<float> m_rasterizedWaveform;

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

	///@brief Compute pipeline for tone mapping fp32 images to RGBA
	ComputePipeline m_toneMapPipe;

	///@brief Compute pipeline for rendering uniform analog waveforms
	std::shared_ptr<ComputePipeline> m_uniformAnalogComputePipeline;

	///@brief Compute pipeline for rendering sparse analog waveforms
	std::shared_ptr<ComputePipeline> m_sparseAnalogComputePipeline;
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
		std::vector<std::shared_ptr<DisplayedChannel> >& channels);
	void ReferenceWaveformTextures();
	void ToneMapAllWaveforms(vk::raii::CommandBuffer& cmdbuf);

	size_t GetStreamCount()
	{ return m_displayedChannels.size(); }

	StreamDescriptor GetStream(size_t i)
	{ return m_displayedChannels[i]->GetStream(); }

	void AddStream(StreamDescriptor desc);

	void RemoveStream(size_t i);

	void ClearPersistence();

	bool IsChannelBeingDragged();
	StreamDescriptor GetChannelBeingDragged();

	TimePoint GetWaveformTimestamp();

protected:
	void ChannelButton(std::shared_ptr<DisplayedChannel> chan, size_t index);
	void RenderBackgroundGradient(ImVec2 start, ImVec2 size);
	void RenderGrid(ImVec2 start, ImVec2 size, std::map<float, float>& gridmap, float& vbot, float& vtop);
	void RenderYAxis(ImVec2 size, std::map<float, float>& gridmap, float vbot, float vtop);
	void RenderTriggerLevelArrows(ImVec2 start, ImVec2 size);
	void RenderCursors(ImVec2 start, ImVec2 size);
	void CheckForScaleMismatch(ImVec2 start, ImVec2 size);
	void RenderWaveforms(ImVec2 start, ImVec2 size);
	void RenderAnalogWaveform(std::shared_ptr<DisplayedChannel> channel, ImVec2 start, ImVec2 size);
	void ToneMapAnalogWaveform(std::shared_ptr<DisplayedChannel> channel, vk::raii::CommandBuffer& cmdbuf);
	void RasterizeAnalogWaveform(
		std::shared_ptr<DisplayedChannel> channel,
		vk::raii::CommandBuffer& cmdbuf);
	void PlotContextMenu();

	void DrawDropRangeMismatchMessage(
		ImDrawList* list,
		ImVec2 center,
		StreamDescriptor ourStream,
		StreamDescriptor theirStream);

	void DragDropOverlays(ImVec2 start, ImVec2 size, int iArea, int numAreas);
	void CenterDropArea(ImVec2 start, ImVec2 size);
	void EdgeDropArea(const std::string& name, ImVec2 start, ImVec2 size, ImGuiDir splitDir);

	float PixelsToYAxisUnits(float pix);
	float YAxisUnitsToPixels(float volt);
	float YAxisUnitsToYPosition(float volt);
	float YPositionToYAxisUnits(float y);
	float PickStepSize(float volts_per_half_span, int min_steps = 2, int max_steps = 5);

	StreamDescriptor GetFirstAnalogStream();
	StreamDescriptor GetFirstAnalogOrEyeStream();

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
		DRAG_STATE_Y_AXIS,
		DRAG_STATE_TRIGGER_LEVEL
	} m_dragState;

	///@brief The stream currently being dragged (invalid if m_dragState != DRAG_STATE_CHANNEL)
	StreamDescriptor m_dragStream;

	DragState m_lastDragState;

	void OnMouseWheelPlotArea(float delta);
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

	///@brief Current trigger level, if dragging
	float m_triggerLevelDuringDrag;

	///@brief The trigger we're configuring
	Trigger* m_triggerDuringDrag;

	///@brief Channels we're in the process of removing
	std::vector<std::shared_ptr<DisplayedChannel> > m_channelsToRemove;

	///@brief X axis position of the mouse at the most recent right click
	int64_t m_lastRightClickOffset;
};

typedef std::pair<WaveformArea*, size_t> DragDescriptor;

#endif


