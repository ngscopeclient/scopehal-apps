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
	, m_bufferedWaveformParam(FilterParameter::TYPE_INT, Unit(Unit::UNIT_COUNTS))
	, m_bufferedWaveformTimeParam(FilterParameter::TYPE_FLOAT, Unit(Unit::UNIT_FS))
	, m_uiDisplayRate(FilterParameter::TYPE_FLOAT, Unit(Unit::UNIT_HZ))
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

	FilterParameter param(FilterParameter::TYPE_STRING);
	param.SetStringVal(m_scope->GetDriverName());
	SetGridEntry(m_commonValuesLabels, m_commonValuesGrid, "Driver", param);

	param.SetStringVal(m_scope->GetTransportConnectionString());
	SetGridEntry(m_commonValuesLabels, m_commonValuesGrid, "Transport", param);
	
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
	double fps = m_oscWindow->m_framesClock.GetAverageMs();
	double ms = fps * depth;

	m_uiDisplayRate.SetFloatVal(fps);
	SetGridEntry(m_commonValuesLabels, m_commonValuesGrid, "Rendering Rate", m_uiDisplayRate);

	m_bufferedWaveformParam.SetIntVal(depth);
	SetGridEntry(m_commonValuesLabels, m_commonValuesGrid, "Buffered Waveforms (Count)", m_bufferedWaveformParam);

	m_bufferedWaveformTimeParam.SetFloatVal(ms * 1000000000000);
	SetGridEntry(m_commonValuesLabels, m_commonValuesGrid, "Buffered Waveforms (Time)", m_bufferedWaveformTimeParam);

	Oscilloscope::DiagnosticValueIterator i = m_scope->GetDiagnosticValuesBegin();
	while (i != m_scope->GetDiagnosticValuesEnd())
	{
		SetGridEntry(m_valuesLabels, m_valuesGrid, (*i).first, (*i).second);

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

void ScopeInfoWindow::SetGridEntry(std::map<std::string, Gtk::Label*>& map, Gtk::Grid& container, std::string name, const FilterParameter& value)
{
	if (map.find(name) != map.end())
	{
		map[name]->set_text(value.ToString());
	}
	else
	{
		auto nameLabel = Gtk::make_managed<Gtk::Label>(name + ":");
		auto valueLabel = Gtk::make_managed<Gtk::Label>(value.ToString());
		nameLabel->set_halign(Gtk::ALIGN_START);
		nameLabel->set_hexpand(true);
		valueLabel->set_halign(Gtk::ALIGN_END);

		int row = map.size();
		container.attach(*nameLabel, 0, row, 1, 1);
		container.attach(*valueLabel, 1, row, 1, 1);

		if (value.GetType() == FilterParameter::TYPE_FLOAT || value.GetType() == FilterParameter::TYPE_INT)
		{
			auto graphSwitch = Gtk::make_managed<Gtk::Switch>();
			graphSwitch->property_active().signal_changed().connect(
				sigc::bind(sigc::mem_fun(*this, &ScopeInfoWindow::OnClickGridEntry), graphSwitch, name));
			container.attach(*graphSwitch, 2, row, 1, 1);
		}
		
		map[name] = valueLabel;
	}

	if (m_graphWindows.find(name) != m_graphWindows.end())
	{
		lock_guard<recursive_mutex> lock(m_graphMutex);

		m_graphWindows[name]->OnDataUpdate(value);
	}
}

void ScopeInfoWindow::OnClickGridEntry(Gtk::Switch* graphSwitch, std::string name)
{
	lock_guard<recursive_mutex> lock(m_graphMutex);

	if (graphSwitch->get_state())
	{
		if (m_graphWindows.find(name) == m_graphWindows.end())
		{
			m_graphWindows[name] = new ScopeInfoWindowGraph("Scope Info: " + m_scope->m_nickname + ": " + name);
		}

		m_graphWindows[name]->show();
	}
	else
	{
		if (m_graphWindows.find(name) == m_graphWindows.end())
		{
			LogError("Trying to hide graph window for diagnostic %s when not shown\n", name.c_str());
			return;
		}
		
		m_graphWindows[name]->hide();
	}
}




ScopeInfoWindowGraph::ScopeInfoWindowGraph(std::string param)
	: Gtk::Dialog( param )
{
	set_skip_taskbar_hint();
	set_type_hint(Gdk::WINDOW_TYPE_HINT_DIALOG);

	set_default_size(600, 100);

	get_vbox()->add(m_graph);

	//Graph setup
	m_graph.set_size_request(600, 100);
	m_graph.m_series.push_back(&m_graphData);
	m_graph.m_seriesName = "data";
	m_graph.m_axisColor = Gdk::Color("#ffffff");
	m_graph.m_backgroundColor = Gdk::Color("#101010");
	m_graph.m_drawLegend = false;
	m_graphData.m_color = Gdk::Color("#ff0000");

	//Default values
	m_graph.m_minScale = 0;
	m_graph.m_maxScale = 1;
	m_graph.m_scaleBump = 0.1;
	m_graph.m_sigfigs = 3;

	m_minval = FLT_MAX;
	m_maxval = -FLT_MAX;

	show_all();
}

ScopeInfoWindowGraph::~ScopeInfoWindowGraph()
{

}

void ScopeInfoWindowGraph::OnDataUpdate(const FilterParameter& param)
{
	double value = param.GetFloatVal();

	if (param.GetUnit() == Unit::UNIT_FS)
		value /= 1000000000000; // Convert to ms so we don't kill the graph lib

	m_graph.m_units = param.GetUnit().ToString();

	auto series = m_graphData.GetSeries("data");
	series->push_back(GraphPoint(GetTime(), value));
	const int max_points = 4096;
	while(series->size() > max_points)
		series->pop_front();

	//Update graph limits
	m_minval = min(m_minval, value);
	m_maxval = max(m_maxval, value);

	m_graph.m_minScale = m_minval;
	m_graph.m_maxScale = m_maxval;
	double range = abs(m_maxval - m_minval);
	if (range > 5000)
		m_graph.m_scaleBump = 2500;
	else if (range > 500)
		m_graph.m_scaleBump = 250;
	else if (range > 50)
		m_graph.m_scaleBump = 25;
	else if(range > 5)
		m_graph.m_scaleBump = 2.5;
	else if(range >= 0.5)
		m_graph.m_scaleBump = 0.25;
	else if(range > 0.05)
		m_graph.m_scaleBump = 0.1;
	else
		m_graph.m_scaleBump = 0.025;
}
