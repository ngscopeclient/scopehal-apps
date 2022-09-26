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

class ToneMapArgs
{
public:
	ToneMapArgs(uint32_t w, uint32_t h)
	: m_width(w)
	, m_height(h)
	{}

	uint32_t m_width;
	uint32_t m_height;
};

/**
	@brief Context data for a single channel being displayed within a WaveformArea
 */
class DisplayedChannel
{
public:
	DisplayedChannel(StreamDescriptor stream)
		: m_stream(stream)
	{
		stream.m_channel->AddRef();
	}

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

	ImTextureID GetTextureHandle()
	{ return m_texture->GetTexture(); }

	void SetTexture(std::shared_ptr<Texture> tex)
	{ m_texture = tex; }

	/**
		@brief Handles a change in size of the displayed waveform

		@param newSize	New size of WaveformArea

		@return true if size has changed, false otherwose
	 */
	bool UpdateSize(ImVec2 newSize)
	{
		if( (m_cachedSize.x != newSize.x) || (m_cachedSize.y != newSize.y) )
		{
			m_cachedSize = newSize;
			return true;
		}
		return false;
	}

protected:
	StreamDescriptor m_stream;

	///@brief The texture storing our rendered waveform
	std::shared_ptr<Texture> m_texture;

	///@brief Size of the texture
	ImVec2 m_cachedSize;
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

	StreamDescriptor GetStream(size_t i)
	{ return m_displayedChannels[i]->GetStream(); }

	void AddStream(StreamDescriptor desc);

	void RemoveStream(size_t i);

	void ClearPersistence();

	bool IsChannelBeingDragged();

protected:
	void DraggableButton(std::shared_ptr<DisplayedChannel> chan, size_t index);
	void RenderBackgroundGradient(ImVec2 start, ImVec2 size);
	void RenderGrid(ImVec2 start, ImVec2 size, std::map<float, float>& gridmap, float& vbot, float& vtop);
	void RenderYAxis(ImVec2 size, std::map<float, float>& gridmap, float vbot, float vtop);
	void RenderTriggerLevelArrows(ImVec2 start, ImVec2 size);
	void RenderCursors(ImVec2 start, ImVec2 size);
	void RenderWaveforms(ImVec2 start, ImVec2 size);
	void RenderAnalogWaveform(std::shared_ptr<DisplayedChannel> channel, ImVec2 start, ImVec2 size);
	void ToneMapAnalogWaveform(std::shared_ptr<DisplayedChannel> channel, ImVec2 size);

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

	DragState m_lastDragState;

	void OnMouseWheelPlotArea(float delta);
	void OnMouseWheelYAxis(float delta);
	void OnMouseUp();
	void OnDragUpdate();

	/**
		@brief The channels currently living within this WaveformArea

		TODO: make this a FlowGraphNode and just hook up inputs
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

	///@brief Compute pipeline for tone mapping fp32 images to RGBA
	ComputePipeline m_toneMapPipe;

	///@brief Command pool for allocating our command buffers
	std::unique_ptr<vk::raii::CommandPool> m_cmdPool;

	///@brief Command buffer used during rendering operations
	std::unique_ptr<vk::raii::CommandBuffer> m_cmdBuffer;
};

typedef std::pair<WaveformArea*, size_t> DragDescriptor;

#endif


