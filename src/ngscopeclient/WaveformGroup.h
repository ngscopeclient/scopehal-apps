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

	bool Render();

	const std::string& GetTitle()
	{ return m_title; }

	void AddArea(std::shared_ptr<WaveformArea>& area);

	void OnZoomInHorizontal(int64_t target, float step);
	void OnZoomOutHorizontal(int64_t target, float step);

	/**
		@brief Converts a position in X axis units (relative to time zero) to pixels (relative to left side of plot)
	 */
	int64_t XPositionToXAxisUnits(float pix)
	{ return m_xAxisOffset + PixelsToXAxisUnits(pix); }

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

	void ClearPersistence();

protected:
	void RenderTimeline(float width, float height);
	int64_t GetRoundingDivisor(int64_t width_xunits);
	void OnMouseWheel(float delta);

	///@brief Top level window we're attached to
	MainWindow* m_parent;

	///@brief Display scale factor
	float m_pixelsPerXUnit;

	///@brief X axis position of the left edge of our view
	int64_t m_xAxisOffset;

	///@brief Display title of the group
	std::string m_title;

	///@brief X axis unit
	Unit m_xAxisUnit;

	///@brief The set of waveform areas within this group
	std::vector< std::shared_ptr<WaveformArea> > m_areas;

	///@brief True if dragging timeline
	bool m_draggingTimeline;

	///@brief Time of last mouse movement
	double m_tLastMouseMove;
};

#endif

