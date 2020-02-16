/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2019 Andrew D. Zonenberg                                                                          *
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
	@brief Implementation of WaveformGroup
 */
#include "glscopeclient.h"
#include "WaveformGroup.h"
#include "MeasurementDialog.h"

using namespace std;

int WaveformGroup::m_numGroups = 1;

WaveformGroup::WaveformGroup(OscilloscopeWindow* parent)
	: m_timeline(parent, this)
	, m_pixelsPerPicosecond(0.05)
	, m_timeOffset(0)
	, m_cursorConfig(CURSOR_NONE)
	, m_parent(parent)
{
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Initial GUI hierarchy, title, etc

	m_frame.add(m_vbox);
		m_vbox.pack_start(m_timeline, Gtk::PACK_SHRINK);
		m_vbox.pack_start(m_waveformBox, Gtk::PACK_EXPAND_WIDGET);

	char tmp[64];
	snprintf(tmp, sizeof(tmp), "Waveform Group %d", m_numGroups);
	m_numGroups ++;
	m_frame.set_label(tmp);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Measurements

	m_vbox.pack_start(m_measurementFrame, Gtk::PACK_SHRINK, 5);
	m_measurementFrame.set_label("Measurements");

	m_measurementFrame.add(m_measurementBox);

	m_measurementBox.set_spacing(30);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Context menu

	m_contextMenu.append(m_removeMeasurementItem);
		m_removeMeasurementItem.set_label("Remove measurement");
		m_removeMeasurementItem.signal_activate().connect(
			sigc::mem_fun(*this, &WaveformGroup::OnRemoveMeasurementItem));
	m_contextMenu.show_all();

	m_selectedColumn = NULL;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Cursors

	m_xCursorPos[0] = 0;
	m_xCursorPos[1] = 0;

	m_yCursorPos[0] = 0;
	m_yCursorPos[1] = 0;
}

WaveformGroup::~WaveformGroup()
{
	for(auto c : m_measurementColumns)
		delete c;
}

void WaveformGroup::RefreshMeasurements()
{
	char tmp[256];
	for(auto m : m_measurementColumns)
	{
		//Run the measurement once, then update our text
		m->m_measurement->Refresh();
		snprintf(
			tmp,
			sizeof(tmp),
			"<span font-weight='bold' underline='single'>%s</span>\n"
			"<span rise='-5' font-family='monospace'>%s</span>",
			m->m_title.c_str(), m->m_measurement->GetValueAsString().c_str());
		m->m_label.set_markup(tmp);
	}
}

void WaveformGroup::AddColumn(string name, OscilloscopeChannel* chan, string color)
{
	//Create the measurement itself
	auto m = Measurement::CreateMeasurement(name);
	if(m->GetInputCount() > 1)
	{
		MeasurementDialog dialog(m_parent, m, chan);
		if(dialog.run() != Gtk::RESPONSE_OK)
		{
			delete m;
			return;
		}
		dialog.ConfigureMeasurement();
	}
	else
		m->SetInput(0, chan);

	//Make sure the measurements can actually be seen
	m_measurementFrame.show();

	//TODO: Don't allow adding the same measurement twice

	//Short name of the channel (truncate if too long)
	string shortname = chan->m_displayname;
	if(shortname.length() > 12)
	{
		shortname.resize(9);
		shortname += "...";
	}

	//Create the column and figure out the title
	auto col = new MeasurementColumn;
	char tmp[256];
	snprintf(tmp, sizeof(tmp), "%s: %s", shortname.c_str(), name.c_str());
	col->m_title = tmp;
	m_measurementColumns.emplace(col);
	col->m_measurement = m;

	//Add to the box and show it
	m_measurementBox.pack_start(col->m_label, Gtk::PACK_SHRINK, 5);
	col->m_label.override_color(Gdk::RGBA(color));
	col->m_label.set_justify(Gtk::JUSTIFY_RIGHT);
	col->m_label.add_events(Gdk::BUTTON_PRESS_MASK);
	col->m_label.show();
	col->m_label.set_selectable();
	col->m_label.signal_button_press_event().connect(
		sigc::bind<MeasurementColumn*>(sigc::mem_fun(*this, &WaveformGroup::OnMeasurementContextMenu), col),
		false);

	//Recalculate stuff now that we have more measurements to look at
	RefreshMeasurements();
}

bool WaveformGroup::OnMeasurementContextMenu(GdkEventButton* event, MeasurementColumn* col)
{
	//SKip anything not right click
	if(event->button != 3)
		return true;

	m_selectedColumn = col;

	m_contextMenu.popup(event->button, event->time);
	return true;
}

void WaveformGroup::OnRemoveMeasurementItem()
{
	m_measurementBox.remove(m_selectedColumn->m_label);
	m_measurementColumns.erase(m_selectedColumn);
	delete m_selectedColumn;
	m_selectedColumn = NULL;

	if(m_measurementColumns.empty())
		m_measurementFrame.hide();
}
