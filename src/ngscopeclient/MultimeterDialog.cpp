/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
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
	@brief Implementation of MultimeterDialog
 */

#include "ngscopeclient.h"
#include "MultimeterDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

MultimeterDialog::MultimeterDialog(SCPIMultimeter* meter, shared_ptr<MultimeterState> state, Session* session)
	: Dialog(string("Multimeter: ") + meter->m_nickname, ImVec2(500, 400))
	, m_session(session)
	, m_tstart(GetTime())
	, m_historyDepth(60)
	, m_meter(meter)
	, m_state(state)
	, m_selectedChannel(m_meter->GetCurrentMeterChannel())
	, m_autorange(m_meter->GetMeterAutoRange())
{
	m_meter->StartMeter();

	//Inputs
	for(size_t i=0; i<m_meter->GetChannelCount(); i++)
		m_channelNames.push_back(m_meter->GetMeterChannelName(i));

	//Primary operating modes
	auto modemask = m_meter->GetMeasurementTypes();
	auto primode = m_meter->GetMeterMode();
	m_primaryModeSelector = 0;
	for(unsigned int i=0; i<32; i++)
	{
		auto mode = static_cast<Multimeter::MeasurementTypes>(1 << i);
		if(modemask & mode)
		{
			m_primaryModes.push_back(mode);
			m_primaryModeNames.push_back(m_meter->ModeToText(mode));
			if(primode == mode)
				m_primaryModeSelector = m_primaryModes.size() - 1;
		}
	}

	//Secondary operating modes
	RefreshSecondaryModeList();
}

MultimeterDialog::~MultimeterDialog()
{
	m_meter->StopMeter();
	m_session->RemoveMultimeter(m_meter);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool MultimeterDialog::DoRender()
{
	//Device information
	if(ImGui::CollapsingHeader("Info"))
	{
		ImGui::BeginDisabled();

			auto name = m_meter->GetName();
			auto vendor = m_meter->GetVendor();
			auto serial = m_meter->GetSerial();
			auto driver = m_meter->GetDriverName();
			auto transport = m_meter->GetTransport();
			auto tname = transport->GetName();
			auto tstring = transport->GetConnectionString();

			ImGui::InputText("Make", &vendor[0], vendor.size());
			ImGui::InputText("Model", &name[0], name.size());
			ImGui::InputText("Serial", &serial[0], serial.size());
			ImGui::InputText("Driver", &driver[0], driver.size());
			ImGui::InputText("Transport", &tname[0], tname.size());
			ImGui::InputText("Path", &tstring[0], tstring.size());

		ImGui::EndDisabled();
	}

	//Save history
	auto etime = GetTime() - m_tstart;
	auto pri = m_state->m_primaryMeasurement.load();
	auto sec = m_state->m_secondaryMeasurement.load();
	bool firstUpdateDone = m_state->m_firstUpdateDone.load();
	bool hasSecondary = m_meter->GetSecondaryMeterMode() != Multimeter::NONE;
	if(firstUpdateDone)
	{
		m_primaryHistory.AddPoint(etime, pri);
		if(hasSecondary)
			m_secondaryHistory.AddPoint(etime, sec);

		m_primaryHistory.Span = m_historyDepth;
		m_secondaryHistory.Span = m_historyDepth;
	}

	float valueWidth = 100;
	auto primaryMode = m_meter->ModeToText(m_meter->GetMeterMode());
	auto secondaryMode = m_meter->ModeToText(m_meter->GetSecondaryMeterMode());

	if(ImGui::CollapsingHeader("Configuration", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if(ImGui::Checkbox("Autorange", &m_autorange))
			m_meter->SetMeterAutoRange(m_autorange);
		HelpMarker("Enables automatic selection of meter scale ranges.");

		//Channel selector (hide if we have only one channel)
		if(m_meter->GetChannelCount() > 1)
		{
			if(Combo("Channel", m_channelNames, m_selectedChannel))
				m_meter->SetCurrentMeterChannel(m_selectedChannel);

			HelpMarker("Select which input channel is being monitored.");
		}

		//Primary operating mode selector
		if(Combo("Mode", m_primaryModeNames, m_primaryModeSelector))
			OnPrimaryModeChanged();
		HelpMarker("Select the type of measurement to make.");

		//Secondary operating mode selector
		if(m_secondaryModeNames.empty())
			ImGui::BeginDisabled();
		if(Combo("Secondary Mode", m_secondaryModeNames, m_secondaryModeSelector))
			m_meter->SetSecondaryMeterMode(m_secondaryModes[m_secondaryModeSelector]);
		if(m_secondaryModeNames.empty())
			ImGui::EndDisabled();

		HelpMarker(
			"Select auxiliary measurement mode, if supported.\n\n"
			"The set of available auxiliary measurements depends on the current primary measurement mode.");
	}

	if(ImGui::CollapsingHeader("Measurements", ImGuiTreeNodeFlags_DefaultOpen))
	{
		string spri;
		string ssec;

		//Hide values until we get first readings back from the meter
		if(firstUpdateDone)
		{
			spri = m_meter->GetMeterUnit().PrettyPrint(pri, m_meter->GetMeterDigits());
			if(hasSecondary)
				ssec = m_meter->GetSecondaryMeterUnit().PrettyPrint(sec, m_meter->GetMeterDigits());
		}

		ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(valueWidth);
			ImGui::InputText(primaryMode.c_str(), &spri[0], spri.size());
		ImGui::EndDisabled();
		HelpMarker("Most recent value for the primary measurement");

		if(hasSecondary)
		{
			ImGui::BeginDisabled();
				ImGui::SetNextItemWidth(valueWidth);
				ImGui::InputText(secondaryMode.c_str(), &ssec[0], ssec.size());
			ImGui::EndDisabled();
			HelpMarker("Most recent value for the secondary measurement");
		}
	}

	auto csize = ImGui::GetContentRegionAvail();
	if(ImGui::CollapsingHeader("Primary Trend"))
	{
		if(ImPlot::BeginPlot("Primary Trend", ImVec2(csize.x, 200), ImPlotFlags_NoLegend) )
		{
			ImPlot::SetupAxisLimits(ImAxis_X1, etime - m_historyDepth, etime, ImGuiCond_Always);

			auto& hist = m_primaryHistory;
			ImPlot::PlotLine(
				primaryMode.c_str(),
				&hist.Data[0].x,
				&hist.Data[0].y,
				hist.Data.size(),
				0,
				0,
				2*sizeof(float));

			ImPlot::EndPlot();
		}
	}

	if(!hasSecondary)
		ImGui::BeginDisabled();

	if(ImGui::CollapsingHeader("Secondary Trend"))
	{
		if(ImPlot::BeginPlot("Secondary Trend", ImVec2(csize.x, 200), ImPlotFlags_NoLegend) )
		{
			ImPlot::SetupAxisLimits(ImAxis_X1, etime - m_historyDepth, etime, ImGuiCond_Always);

			auto& hist = m_secondaryHistory;

			ImPlot::PlotLine(
				secondaryMode.c_str(),
				&hist.Data[0].x,
				&hist.Data[0].y,
				hist.Data.size(),
				0,
				0,
				2*sizeof(float));

			ImPlot::EndPlot();
		}
	}

	if(!hasSecondary)
		ImGui::EndDisabled();

	return true;
}

void MultimeterDialog::OnPrimaryModeChanged()
{
	//Push the new mode to the meter
	m_meter->SetMeterMode(m_primaryModes[m_primaryModeSelector]);

	//Clear historical data since we're not measuring the same thing anymore
	m_primaryHistory.Clear();
	m_secondaryHistory.Clear();

	//Redo the list of available secondary meter modes
	RefreshSecondaryModeList();
}

void MultimeterDialog::RefreshSecondaryModeList()
{
	m_secondaryModes.clear();
	m_secondaryModeNames.clear();
	m_secondaryModeSelector = -1;

	auto modemask = m_meter->GetSecondaryMeasurementTypes();
	auto secmode = m_meter->GetSecondaryMeterMode();
	for(unsigned int i=0; i<32; i++)
	{
		auto mode = static_cast<Multimeter::MeasurementTypes>(1 << i);
		if(modemask & mode)
		{
			m_secondaryModes.push_back(mode);
			m_secondaryModeNames.push_back(m_meter->ModeToText(mode));
			if(secmode == mode)
				m_secondaryModeSelector = m_secondaryModes.size() - 1;
		}
	}
}
