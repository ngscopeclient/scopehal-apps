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
	, m_driver(FilterParameter::TYPE_STRING)
	, m_transport(FilterParameter::TYPE_STRING)
	, m_bufferedWaveformParam(FilterParameter::TYPE_INT, Unit(Unit::UNIT_COUNTS))
	, m_bufferedWaveformTimeParam(FilterParameter::TYPE_FLOAT, Unit(Unit::UNIT_FS))
	, m_uiDisplayRate(FilterParameter::TYPE_FLOAT, Unit(Unit::UNIT_HZ))
	, m_graphWindow( string("Scope Info: ") + scope->m_nickname + string(" (Graphs)"))
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

	m_driver.SetStringVal(m_scope->GetDriverName());
	m_transport.SetStringVal(m_scope->GetTransportConnectionString());
	m_uiDisplayRate.SetFloatVal(0);
	m_bufferedWaveformParam.SetIntVal(0);
	m_bufferedWaveformTimeParam.SetFloatVal(0);

	BindValue(m_commonValuesLabels, m_commonValuesGrid, "Driver", &m_driver);
	BindValue(m_commonValuesLabels, m_commonValuesGrid, "Transport", &m_transport);
	BindValue(m_commonValuesLabels, m_commonValuesGrid, "Rendering Rate", &m_uiDisplayRate);
	BindValue(m_commonValuesLabels, m_commonValuesGrid, "Buffered Waveforms (Count)", &m_bufferedWaveformParam);
	BindValue(m_commonValuesLabels, m_commonValuesGrid, "Buffered Waveforms (Time)", &m_bufferedWaveformTimeParam);

	m_graphWindow.hide();
	
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
	double fps = m_oscWindow->m_framesClock.GetAverageHz();
	double ms = m_oscWindow->m_framesClock.GetAverageMs() * depth;

	m_uiDisplayRate.SetFloatVal(fps);
	m_bufferedWaveformParam.SetIntVal(depth);
	m_bufferedWaveformTimeParam.SetFloatVal(ms * 1000000000000);

	for (auto i : m_scope->GetDiagnosticsValues())
	{
		if (m_valuesLabels.find(i.first) == m_valuesLabels.end())
		{
			BindValue(m_valuesLabels, m_valuesGrid, i.first, i.second);
		}
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

void ScopeInfoWindow::BindValue(std::map<std::string, Gtk::Label*>& map, Gtk::Grid& container, std::string name, FilterParameter* value)
{
	auto nameLabel = Gtk::make_managed<Gtk::Label>(name + ":");
	auto valueLabel = Gtk::make_managed<Gtk::Label>(value->ToString());
	nameLabel->set_halign(Gtk::ALIGN_START);
	nameLabel->set_hexpand(true);
	valueLabel->set_halign(Gtk::ALIGN_END);

	int row = map.size();
	container.attach(*nameLabel, 0, row, 1, 1);
	container.attach(*valueLabel, 1, row, 1, 1);

	if (value->GetType() == FilterParameter::TYPE_FLOAT || value->GetType() == FilterParameter::TYPE_INT)
	{
		auto graphSwitch = Gtk::make_managed<Gtk::Switch>();
		graphSwitch->property_active().signal_changed().connect(
			sigc::bind(sigc::mem_fun(*this, &ScopeInfoWindow::OnClickGraphSwitch), graphSwitch, name, value));
		container.attach(*graphSwitch, 2, row, 1, 1);
	}
	
	map[name] = valueLabel;

	value->signal_changed().connect(
		sigc::bind(sigc::mem_fun(*this, &ScopeInfoWindow::OnValueUpdate), valueLabel, value));
	OnValueUpdate(valueLabel, value);
}

class UpdateRequest
{
public:
	UpdateRequest(Gtk::Label* label, std::string str) : m_label(label), m_str(str) {};

	Gtk::Label* m_label;
	std::string m_str;

	static int c_update_internal(void* p)
	{
		UpdateRequest* u = static_cast<UpdateRequest*>(p);
		u->m_label->set_text(u->m_str);
		delete u;
		return 0;
	}
};

void ScopeInfoWindow::OnValueUpdate(Gtk::Label* label, FilterParameter* value)
{
	UpdateRequest* u = new UpdateRequest(label, value->ToString());
	g_main_context_invoke(NULL, (GSourceFunc)&UpdateRequest::c_update_internal, u);
}

void ScopeInfoWindow::OnClickGraphSwitch(Gtk::Switch* graphSwitch, std::string name, FilterParameter* value)
{
	if (graphSwitch->get_state())
	{
		m_graphWindow.AddGraphedValue(name, value);
	}
	else
	{
		m_graphWindow.RemoveGraphedValue(name);
	}
}




ScopeInfoGraphWindow::ScopeInfoGraphWindow(std::string title)
	: Gtk::Dialog( title )
{
	set_skip_taskbar_hint();
	set_type_hint(Gdk::WINDOW_TYPE_HINT_DIALOG);

	set_default_size(600, 100);

	get_vbox()->add(m_grid);
}

ScopeInfoGraphWindow::~ScopeInfoGraphWindow()
{

}

void ScopeInfoGraphWindow::AddGraphedValue(std::string name, FilterParameter* value)
{
	if (m_graphs.find(name) != m_graphs.end())
	{
		LogWarning("State desync between info window and info graph window\n");
		return;
	}

	auto graph = Gtk::make_managed<Graph>();

	// Relying on operator[] inserting default-constructed values
	ShownGraph* shown = &m_graphs[name];
	shown->widget = graph;
	shown->minval = FLT_MAX;
	shown->maxval = -FLT_MAX;

	graph->set_size_request(600, 100);
	graph->m_series.push_back(&shown->data);
	graph->m_seriesName = "data";
	graph->m_axisColor = Gdk::Color("#ffffff");
	graph->m_backgroundColor = Gdk::Color("#101010");
	graph->m_drawLegend = false;
	shown->data.m_color = Gdk::Color("#ff0000");

	//Default values
	graph->m_minScale = 0;
	graph->m_maxScale = 1;
	graph->m_scaleBump = 0.1;
	graph->m_sigfigs = 3;

	m_grid.attach_next_to(*graph, Gtk::POS_BOTTOM, 1, 1);

	OnValueUpdate(shown, value);
	value->signal_changed().connect(
		sigc::bind(sigc::mem_fun(*this, &ScopeInfoGraphWindow::OnValueUpdate), shown, value));

	show_all();
	show();
}

void ScopeInfoGraphWindow::OnValueUpdate(ShownGraph* shown, FilterParameter* param)
{
	double value = param->GetFloatVal();

	if (param->GetUnit() == Unit::UNIT_FS)
		value /= 1000000000000; // Convert to ms so we don't kill the graph lib
	else if (param->GetUnit() == Unit::UNIT_PERCENT)
		value *= 100; // Display to user as X/100

	shown->widget->m_units = param->GetUnit().ToString();

	auto series = shown->data.GetSeries("data");
	series->push_back(GraphPoint(GetTime(), value));
	const int max_points = 4096;
	while(series->size() > max_points)
		series->pop_front();

	//Update graph limits
	shown->minval = min(shown->minval, value);
	shown->maxval = max(shown->maxval, value);

	shown->widget->m_minScale = shown->minval;
	shown->widget->m_maxScale = shown->maxval;
	double range = abs(shown->maxval - shown->minval);
	if (range > 5000)
		shown->widget->m_scaleBump = 2500;
	else if (range > 500)
		shown->widget->m_scaleBump = 250;
	else if (range > 50)
		shown->widget->m_scaleBump = 25;
	else if(range > 5)
		shown->widget->m_scaleBump = 2.5;
	else if(range >= 0.5)
		shown->widget->m_scaleBump = 0.25;
	else if(range > 0.05)
		shown->widget->m_scaleBump = 0.1;
	else
		shown->widget->m_scaleBump = 0.025;
}

void ScopeInfoGraphWindow::RemoveGraphedValue(std::string name)
{
	if (m_graphs.find(name) == m_graphs.end())
	{
		LogWarning("State desync between info window and info graph window\n");
		return;
	}

	// m_graphs.erase(name);
}
