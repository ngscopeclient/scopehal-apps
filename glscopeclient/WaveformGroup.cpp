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

int WaveformGroup::m_numGroups = 1;

WaveformGroup::WaveformGroup()
	: m_measurementView(1, false, Gtk::SELECTION_NONE)
	, m_pixelsPerPicosecond(0.05)
	, m_cursorConfig(CURSOR_NONE)
{
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Initial GUI hierarchy, title, etc

	m_frame.add(m_vbox);
		m_vbox.pack_start(m_timeline, Gtk::PACK_SHRINK);
			m_timeline.m_group = this;
		m_vbox.pack_start(m_waveformBox, Gtk::PACK_EXPAND_WIDGET);

	char tmp[64];
	snprintf(tmp, sizeof(tmp), "Waveform Group %d", m_numGroups);
	m_numGroups ++;
	m_frame.set_label(tmp);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Measurements

	m_vbox.pack_start(m_measurementFrame, Gtk::PACK_SHRINK);
	m_measurementFrame.add(m_measurementView);
	m_measurementFrame.set_label("Measurements");
	m_measurementFrame.show_all();

	m_measurementView.set_column_title(0, "Measure");
	m_measurementView.append("value");

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Cursors

	m_xCursorPos[0] = 0;
	m_xCursorPos[1] = 0;

	m_yCursorPos[0] = 0;
	m_yCursorPos[1] = 0;
}

WaveformGroup::~WaveformGroup()
{
}
