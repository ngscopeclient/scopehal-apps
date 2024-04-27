/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2023 Andrew D. Zonenberg                                                                          *
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
	@brief Declaration of WaveformGroup
 */
#ifndef WaveformGroup_h
#define WaveformGroup_h

#include "WaveformArea.h"

/**
	@brief A WaveformGroup is a container for one or more WaveformArea's.
 */
class WaveformGroup
{
public:
	WaveformGroup(MainWindow* parent, const std::string& title);
	virtual ~WaveformGroup();

	void Clear();

	bool Render();
	void ToneMapAllWaveforms(vk::raii::CommandBuffer& cmdbuf);
	void ReferenceWaveformTextures();

	void RenderWaveformTextures(
		vk::raii::CommandBuffer& cmdbuf,
		std::vector<std::shared_ptr<DisplayedChannel> >& channels,
		bool clearPersistence);

	const std::string GetID()
	{ return m_title + "###" + m_id; }

	const std::string GetRawID()
	{ return m_id; }

	const std::string& GetTitle()
	{ return m_title; }

	void AddArea(std::shared_ptr<WaveformArea>& area);

	void OnZoomInHorizontal(int64_t target, float step);
	void OnZoomOutHorizontal(int64_t target, float step);
	void OnPanHorizontal(float step);
	void NavigateToTimestamp(
		int64_t timestamp,
		int64_t duration = 0,
		StreamDescriptor target = StreamDescriptor(nullptr, 0));

	void ClearPersistenceOfChannel(OscilloscopeChannel* chan);

	/**
		@brief Gets the X axis unit for this group
	 */
	Unit GetXAxisUnit()
	{ return m_xAxisUnit; }

	/**
		@brief Converts a position in pixels (relative to left side of plot) to X axis units (relative to time zero)
	 */
	int64_t XPositionToXAxisUnits(float pix)
	{ return m_xAxisOffset + PixelsToXAxisUnits(pix - m_xpos); }

	/**
		@brief Converts a distance measurement in pixels to X axis units
	 */
	int64_t PixelsToXAxisUnits(float pix)
	{ return pix / m_pixelsPerXUnit; }

	/**
		@brief Converts a distance measurement in X axis units to pixels
	 */
	float XAxisUnitsToPixels(int64_t t)
	{ return t * m_pixelsPerXUnit; }

	/**
		@brief Converts a position in X axis units to pixels (in window coordinates)
	 */
	float XAxisUnitsToXPosition(int64_t t)
	{ return XAxisUnitsToPixels(t - m_xAxisOffset) + m_xpos; }

	float GetPixelsPerXUnit()
	{ return m_pixelsPerXUnit; }

	int64_t GetXAxisOffset()
	{ return m_xAxisOffset; }

	void ClearPersistence();

	bool IsChannelBeingDragged();
	StreamDescriptor GetChannelBeingDragged();

	float GetYAxisWidth()
	{ return 6 * ImGui::GetFontSize() * ImGui::GetWindowDpiScale(); }

	float GetSpacing()
	{ return ImGui::GetFrameHeightWithSpacing() - ImGui::GetFrameHeight(); }

	///@brief Gets an atomic snapshot of the waveform areas in this group
	std::vector< std::shared_ptr<WaveformArea> > GetWaveformAreas()
	{
		std::lock_guard<std::mutex> lock(m_areaMutex);
		return m_areas;
	}

	//Serialization
	bool LoadConfiguration(const YAML::Node& node);
	YAML::Node SerializeConfiguration(IDTable& table);

	bool IsDraggingTrigger()
	{ return m_dragState == DRAG_STATE_TRIGGER; }

protected:
	void RenderTimeline(float width, float height);
	void RenderTriggerPositionArrows(ImVec2 pos, float height);
	void RenderXAxisCursors(ImVec2 pos, ImVec2 size);
	void RenderMarkers(ImVec2 pos, ImVec2 size);
	void DoCursorReadouts();

	float GetInBandPower(WaveformBase* wfm, Unit yunit, int64_t t1, int64_t t2);

	bool IsMouseOverButtonInWaveformArea();

	enum DragState
	{
		DRAG_STATE_NONE,
		DRAG_STATE_TIMELINE,
		DRAG_STATE_X_CURSOR0,
		DRAG_STATE_X_CURSOR1,
		DRAG_STATE_MARKER,
		DRAG_STATE_TRIGGER
	};

	void DoCursor(int iCursor, DragState state);

	int64_t GetRoundingDivisor(int64_t width_xunits);
	void OnMouseWheel(float delta);

	///@brief Top level window we're attached to
	MainWindow* m_parent;

	///@brief X position of our child windows
	float m_xpos;

	///@brief Width of the window (used for autoscaling)
	float m_width;

	///@brief Display scale factor
	float m_pixelsPerXUnit;

	///@brief X axis position of the left edge of our view
	int64_t m_xAxisOffset;

	///@brief Display title of the group
	std::string m_title;

	///@brief Internal ImGui ID of the group
	std::string m_id;

	///@brief X axis unit
	Unit m_xAxisUnit;

	///@brief The set of waveform areas within this group
	std::vector< std::shared_ptr<WaveformArea> > m_areas;

	//Mutex for controlling access to m_areas
	std::mutex m_areaMutex;

	///@brief Description of item being dragged, if any
	DragState m_dragState;

	///@brief Marker being dragged, if any
	Marker* m_dragMarker;

	///@brief Time of last mouse movement
	double m_tLastMouseMove;

	///@brief List of waveform areas to close next frame
	std::vector<size_t> m_areasToClose;

	///@brief Height of the timeline
	float m_timelineHeight;

	///@brief True if clearing persistence
	std::atomic<bool> m_clearPersistence;

	///@brief True if mouse is over a trigger arrow
	bool m_mouseOverTriggerArrow;

	///@brief The scope whose trigger being dragged when in DRAG_STATE_TRIGGER
	Oscilloscope* m_scopeTriggerDuringDrag;

	///@brief True if we're displaying an eye pattern (fixed x axis scale)
	bool m_displayingEye;

public:

	///@brief Type of X axis cursor we're displaying
	enum CursorMode_t
	{
		X_CURSOR_NONE,
		X_CURSOR_SINGLE,
		X_CURSOR_DUAL
	} m_xAxisCursorMode;

	///@brief Position (in X axis units) of each cursor
	int64_t m_xAxisCursorPositions[2];
};

#endif

