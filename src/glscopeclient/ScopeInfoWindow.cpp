/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2022 Louis A. Goessling                                                                                *
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
	, m_saveButton("Save Diagnostics")
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

	m_grid.attach_next_to(m_saveButton, m_consoleFrame, Gtk::POS_BOTTOM, 1, 1);
		m_saveButton.signal_clicked().connect(
			sigc::mem_fun(*this, &ScopeInfoWindow::OnSaveClicked));

		// m_consoleFrame.override_background_color(Gdk::RGBA("#ff0000"));

	m_driver.SetStringVal(m_scope->GetDriverName());
	m_transport.SetStringVal(m_scope->GetTransportConnectionString());
	m_uiDisplayRate.SetFloatVal(0);
	m_bufferedWaveformParam.SetIntVal(0);
	m_bufferedWaveformTimeParam.SetFloatVal(0);

	std::vector<std::pair<std::string, FilterParameter*>> to_bind = {
		{"Driver", &m_driver},
		{"Transport", &m_transport},
		{"Rendering Rate", &m_uiDisplayRate},
		{"Buffered Waveforms (Count)", &m_bufferedWaveformParam},
		{"Buffered Waveforms (Time)", &m_bufferedWaveformTimeParam}
	};

	for (auto& i : to_bind)
	{
		m_commonValuesLabels[i.second] = BindValue(m_commonValuesGrid, i.first, i.second);
	}

	m_graphWindow.hide();

	Glib::signal_timeout().connect(sigc::mem_fun(*this, &ScopeInfoWindow::OnTick), 50 /* 20Hz */);
	OnTick();

	show_all();
}

ScopeInfoWindow::~ScopeInfoWindow()
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

void ScopeInfoWindow::OnWaveformDataReady()
{
	// TODO: Show stats about last waveform pulled from this scope
}

bool ScopeInfoWindow::OnTick()
{
	int depth = m_scope->GetPendingWaveformCount();
	double fps = m_oscWindow->m_framesClock.GetAverageHz();
	double ms = m_oscWindow->m_framesClock.GetAverageMs() * depth;

	m_uiDisplayRate.SetFloatVal(fps);
	m_bufferedWaveformParam.SetIntVal(depth);
	m_bufferedWaveformTimeParam.SetFloatVal(ms * 1000000000000);

	for (auto& i : m_scope->GetDiagnosticsValues())
	{
		auto found_pair = m_valuesLabels.find(i.first);
		if (found_pair == m_valuesLabels.end())
		{
			m_valuesLabels[i.first] = BindValue(m_valuesGrid, i.first, i.second);
		}
		else
		{
			found_pair->second->set_text(i.second->ToString());
		}
	}

	for (auto& i : m_commonValuesLabels)
	{
		i.second->set_text(i.first->ToString());
	}

	if (m_scope->HasPendingDiagnosticLogMessages())
	{
		do
		{
			m_consoleText.push_back(m_scope->PopPendingDiagnosticLogMessage());
		}
		while (m_scope->HasPendingDiagnosticLogMessages());

		while (m_consoleText.size() > 500)
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

	m_graphWindow.OnTick();

	return true;
}

Gtk::Label* ScopeInfoWindow::BindValue(Gtk::Grid& container, std::string name, FilterParameter* value)
{
	auto nameLabel = Gtk::make_managed<Gtk::Label>(name + ":");
	auto valueLabel = Gtk::make_managed<Gtk::Label>(value->ToString());
	nameLabel->set_halign(Gtk::ALIGN_START);
	nameLabel->set_hexpand(true);
	valueLabel->set_halign(Gtk::ALIGN_END);

	container.attach_next_to(*nameLabel, Gtk::POS_BOTTOM, 1, 1);
	container.attach_next_to(*valueLabel, *nameLabel, Gtk::POS_RIGHT, 1, 1);

	if (value->GetType() == FilterParameter::TYPE_FLOAT || value->GetType() == FilterParameter::TYPE_INT)
	{
		auto graphSwitch = Gtk::make_managed<Gtk::Switch>();
		graphSwitch->property_active().signal_changed().connect(
			sigc::bind(sigc::mem_fun(*this, &ScopeInfoWindow::OnClickGraphSwitch), graphSwitch, name, value));
		container.attach_next_to(*graphSwitch, *valueLabel, Gtk::POS_RIGHT, 1, 1);
	}

	return valueLabel;
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

void ScopeInfoWindow::OnSaveClicked()
{
	//Prompt for the file
	Gtk::FileChooserDialog dlg(*this, "Save Diagnostics", Gtk::FILE_CHOOSER_ACTION_SAVE);
	auto filter = Gtk::FileFilter::create();
	filter->add_pattern("*.txt");
	filter->set_name("Text files (*.txt)");
	dlg.add_filter(filter);
	dlg.add_button("Save", Gtk::RESPONSE_OK);
	dlg.add_button("Cancel", Gtk::RESPONSE_CANCEL);
	dlg.set_do_overwrite_confirmation();
	auto response = dlg.run();
	if(response != Gtk::RESPONSE_OK)
		return;

	//Write initial headers
	auto fname = dlg.get_filename();
	FILE* fp = fopen(fname.c_str(), "w");
	if(!fp)
	{
		string msg = string("Output file") + fname + " cannot be opened";
		Gtk::MessageDialog errdlg(msg, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		errdlg.set_title("Cannot save diagnostics\n");
		errdlg.run();
		return;
	}

	fprintf(fp, "[Basic Info]\n");

	fprintf(fp, "Scope Driver = %s\n", m_scope->GetDriverName().c_str());
	fprintf(fp, "Scope Transport String = %s\n", m_scope->GetTransportConnectionString().c_str());
	fprintf(fp, "Scope Pending Waveforms = %ld\n", m_scope->GetPendingWaveformCount());
	fprintf(fp, "Main UI Render Rate = %f Hz\n", m_oscWindow->m_framesClock.GetAverageHz());

	fprintf(fp, "\n[Diagnostic Parameters]\n");

	for (auto& i : m_scope->GetDiagnosticsValues())
	{
		fprintf(fp, "%s = %s\n", i.first.c_str(), i.second->ToString().c_str());
	}

	fprintf(fp, "\n[Diagnostic Log]\n");

	for (auto& i : m_consoleText)
	{
		fprintf(fp, "%s\n", i.c_str());
	}

	fclose(fp);
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

	auto unit = value->GetUnit().ToString();

	if (unit == "fs")
		unit = "ms"; // We adjust below

	auto graph = Gtk::make_managed<Graph>();
	auto label = Gtk::make_managed<Gtk::Label>(name + " (" + unit + ")");

	// Relying on operator[] inserting default-constructed values
	ShownGraph* shown = &m_graphs[name];
	shown->graph = graph;
	shown->label = label;
	shown->param = value;
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
	graph->m_units = unit;
	graph->m_minRedline = -FLT_MAX;
	graph->m_maxRedline = FLT_MAX;

	m_grid.attach_next_to(*label, Gtk::POS_BOTTOM, 1, 1);
	m_grid.attach_next_to(*graph, Gtk::POS_BOTTOM, 1, 1);

	DoValueUpdate(shown);

	show_all();
	show();
}

void ScopeInfoGraphWindow::DoValueUpdate(ShownGraph* shown)
{
	double value = shown->param->GetFloatVal();

	if (shown->param->GetUnit() == Unit::UNIT_FS)
		value /= 1000000000000; // Convert to ms so we don't kill the graph lib
	else if (shown->param->GetUnit() == Unit::UNIT_PERCENT)
		value *= 100; // Display to user as X/100

	auto series = shown->data.GetSeries("data");
	series->push_back(GraphPoint(GetTime(), value));
	const int max_points = 4096;
	while(series->size() > max_points)
		series->pop_front();

	//Update graph limits
	shown->minval = min(shown->minval, value);
	shown->maxval = max(shown->maxval, value);

	double eff_min = shown->minval * 0.90;
	double eff_max = shown->maxval * 1.10;

	shown->graph->m_minScale = eff_min;
	shown->graph->m_maxScale = eff_max;
	shown->graph->m_scaleBump = abs(eff_max - eff_min) / 4.1;
}

void ScopeInfoGraphWindow::OnTick()
{
	for (auto& i : m_graphs)
	{
		DoValueUpdate(&i.second);
	}
}

void ScopeInfoGraphWindow::RemoveGraphedValue(std::string name)
{
	if (m_graphs.find(name) == m_graphs.end())
	{
		LogWarning("State desync between info window and info graph window\n");
		return;
	}

	m_grid.remove(*m_graphs[name].label);
	m_grid.remove(*m_graphs[name].graph);
	m_graphs.erase(name);

	resize(600, 100);

	if (m_graphs.size() == 0)
	{
		hide();
	}
}
