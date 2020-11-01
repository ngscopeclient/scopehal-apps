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

class MeasurementColumns : public Gtk::TreeModel::ColumnRecord
{
public:
	MeasurementColumns()
	{
		//column 0 is never used, we reserve the index for m_filterColumn
		m_columns.push_back(Gtk::TreeModelColumn<std::string>());
		add(m_filterColumn);

		for(size_t i=1; i<32; i++)
		{
			m_columns.push_back(Gtk::TreeModelColumn<std::string>());
			add(m_columns[i]);
		}
		add(m_statColumn);
	}

	Gtk::TreeModelColumn<std::string> m_filterColumn;
	std::vector<Gtk::TreeModelColumn<std::string>> m_columns;

	Gtk::TreeModelColumn<Statistic*> m_statColumn;
};

class WaveformGroup
{
public:
	WaveformGroup(OscilloscopeWindow* parent);
	virtual ~WaveformGroup();

	void RefreshMeasurements();

	bool IsShowingStats(OscilloscopeChannel* chan);

	MeasurementColumns m_treeColumns;
	Glib::RefPtr<Gtk::TreeStore> m_treeModel;
	void ToggleOn(OscilloscopeChannel* chan);
	void ToggleOff(OscilloscopeChannel* chan);

	void AddStatistic(Statistic* stat);
	void ClearStatistics();

	int GetIndexOfChild(Gtk::Widget* child);
	bool IsLastChild(Gtk::Widget* child);

	void OnChannelRenamed(OscilloscopeChannel* chan);

	//map of scope channels to measurement column indexes
	std::map<OscilloscopeChannel*, int> m_columnToIndexMap;
	std::map<int, OscilloscopeChannel*> m_indexToColumnMap;

	Gtk::Frame m_frame;
		Gtk::VBox m_vbox;
			Timeline m_timeline;
			Gtk::VBox m_waveformBox;
			Gtk::TreeView m_measurementView;

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
	void OnMeasurementButtonPressEvent(GdkEventButton* event);

	static int m_numGroups;

	OscilloscopeWindow* m_parent;

	Gtk::Menu m_contextMenu;
		Gtk::MenuItem m_hideItem;

	void OnHideStatistic();

	OscilloscopeChannel* m_measurementContextMenuChannel;
};

#endif
