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
	@brief A group of one or more WaveformArea's
 */

#ifndef WaveformGroup_h
#define WaveformGroup_h

#include "Timeline.h"

class OscilloscopeWindow;

class MeasurementColumn
{
public:
	~MeasurementColumn()
	{
		delete m_measurement;
		m_measurement = NULL;
	}

	Gtk::Label m_label;
	std::string m_title;
	Measurement* m_measurement;
};

class WaveformGroup
{
public:
	WaveformGroup(OscilloscopeWindow* parent);
	virtual ~WaveformGroup();

	void RefreshMeasurements();

	void AddColumn(std::string name, OscilloscopeChannel* chan, std::string color);
	void AddColumn(Measurement* meas, std::string color, std::string label);

	Gtk::Frame m_frame;
		Gtk::VBox m_vbox;
			Timeline m_timeline;
			Gtk::VBox m_waveformBox;
			Gtk::Frame m_measurementFrame;
				Gtk::HBox m_measurementBox;
					std::set<MeasurementColumn*> m_measurementColumns;

	Gtk::Menu m_contextMenu;
		Gtk::MenuItem m_removeMeasurementItem;

	float m_pixelsPerXUnit;
	int64_t m_xAxisOffset;

	enum CursorConfig
	{
		CURSOR_NONE,
		CURSOR_X_SINGLE,
		CURSOR_X_DUAL,
		CURSOR_Y_SINGLE,
		CURSOR_Y_DUAL
	} m_cursorConfig;

	int64_t m_xCursorPos[2];
	double m_yCursorPos[2];

	OscilloscopeWindow* GetParent()
	{ return m_parent; }

	virtual std::string SerializeConfiguration(IDTable& table);

protected:
	MeasurementColumn* m_selectedColumn;
	bool OnMeasurementContextMenu(GdkEventButton* event, MeasurementColumn* col);
	void OnRemoveMeasurementItem();

	static int m_numGroups;

	OscilloscopeWindow* m_parent;
};

#endif
