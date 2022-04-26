/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
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
	@author Louis A. Goessling
	@brief Implementation of ScopeInfoWindow
 */
#include "glscopeclient.h"
#include "ScopeInfoWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ScopeInfoWindow::ScopeInfoWindow(OscilloscopeWindow* oscWindow, Oscilloscope* scope)
	: Gtk::Dialog( string("Scope Info: ") + scope->m_nickname )
	, m_oscWindow(oscWindow)
	, m_scope(scope)
{
	set_skip_taskbar_hint();
	set_type_hint(Gdk::WINDOW_TYPE_HINT_DIALOG);

	set_default_size(640, 520);

	get_vbox()->add(m_grid);
	m_grid.set_hexpand(true);

	m_grid.attach(m_commonValuesGrid, 0, 0, 1, 1);
		m_valuesGrid.set_hexpand(true);

	m_grid.attach_next_to(m_valuesGrid, m_commonValuesGrid, Gtk::POS_BOTTOM, 1, 1);
		m_valuesGrid.set_hexpand(true);

	m_grid.attach_next_to(m_consoleFrame, m_valuesGrid, Gtk::POS_BOTTOM, 1, 1);
		m_consoleFrame.set_min_content_height(300);
		m_consoleFrame.set_max_content_height(300);
		// m_consoleFrame.set_has_frame(true);
		m_consoleFrame.set_hexpand(true);
		m_consoleFrame.add(m_console);
			m_console.set_editable(false);
			m_console.set_monospace(true);
			m_console.set_hexpand(true);
			m_consoleBuffer = Gtk::TextBuffer::create();
			m_console.set_buffer(m_consoleBuffer);
		m_consoleFrame.set_margin_top(10);
			
		// m_consoleFrame.override_background_color(Gdk::RGBA("#ff0000"));

	SetGridEntry(m_commonValuesLabels, m_commonValuesGrid, "Driver", m_scope->GetDriverName());
	SetGridEntry(m_commonValuesLabels, m_commonValuesGrid, "Transport", m_scope->GetTransportConnectionString());
	
	OnWaveformDataReady();

	show_all();
}

ScopeInfoWindow::~ScopeInfoWindow()
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

void ScopeInfoWindow::OnWaveformDataReady()
{
	int depth = m_scope->GetPendingWaveformCount();
	int ms = m_oscWindow->m_framesClock.GetAverageMs() * depth;

	SetGridEntry(m_commonValuesLabels, m_commonValuesGrid, "Buffered Waveforms", to_string(depth) + " WFMs / " + to_string(ms) + " ms");

	Oscilloscope::DiagnosticValueIterator i = m_scope->GetDiagnosticValuesBegin();
	while (i != m_scope->GetDiagnosticValuesEnd())
	{
		string name = (*i).first;
		string value = (*i).second.ToString();

		SetGridEntry(m_valuesLabels, m_valuesGrid, name, value);

		i++;
	}

	if (m_scope->HasPendingDiagnosticLogMessages())
	{
		do
		{
			m_consoleText.push_back(m_scope->PopPendingDiagnosticLogMessage());
		}
		while (m_scope->HasPendingDiagnosticLogMessages());

		while (m_consoleText.size() > 50)
		{
			m_consoleText.pop_front();
		}

		m_consoleBuffer->set_text("");
		for (auto line : m_consoleText)
		{
			m_consoleBuffer->insert_at_cursor(line);
			m_consoleBuffer->insert_at_cursor("\n");
		}

		Glib::RefPtr<Gtk::Adjustment> adj = m_consoleFrame.get_vadjustment();
		adj->set_value(adj->get_upper());
	}

	// m_stdDevLabel.set_text("FPS Jitter: " + to_string(stddev) + " (stddev)" + extraInfo);
}

void ScopeInfoWindow::SetGridEntry(std::map<std::string, Gtk::Label*>& map, Gtk::Grid& container, std::string name, std::string value)
{
	if (map.find(name) != map.end())
	{
		map[name]->set_text(value);
	}
	else
	{
		auto nameLabel = Gtk::make_managed<Gtk::Label>(name + ":");
		auto valueLabel = Gtk::make_managed<Gtk::Label>(value);
		nameLabel->set_halign(Gtk::ALIGN_START);
		nameLabel->set_hexpand(true);
		valueLabel->set_halign(Gtk::ALIGN_END);

		int row = map.size();
		container.attach(*nameLabel, 0, row, 1, 1);
		container.attach(*valueLabel, 1, row, 1, 1);
		map[name] = valueLabel;
	}
}
