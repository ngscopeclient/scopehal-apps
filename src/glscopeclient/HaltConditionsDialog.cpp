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
	@author Andrew D. Zonenberg
	@brief Implementation of HaltConditionsDialog
 */
#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "HaltConditionsDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

HaltConditionsDialog::HaltConditionsDialog(
	OscilloscopeWindow* parent)
	: Gtk::Dialog("Halt Conditions", *parent)
	, m_parent(parent)
{
	get_vbox()->pack_start(m_grid, Gtk::PACK_EXPAND_WIDGET);
		m_grid.attach(m_haltEnabledButton, 0, 0, 1, 1);
			m_haltEnabledButton.set_label("Halt Enabled");
		m_grid.attach_next_to(m_moveToEventButton, m_haltEnabledButton, Gtk::POS_BOTTOM, 1, 1);
			m_moveToEventButton.set_label("Move To Event");

		m_grid.attach_next_to(m_channelNameLabel, m_moveToEventButton, Gtk::POS_BOTTOM, 1, 1);
			m_channelNameLabel.set_label("Halt when");
		m_grid.attach_next_to(m_channelNameBox, m_channelNameLabel, Gtk::POS_RIGHT, 1, 1);
		m_grid.attach_next_to(m_operatorBox, m_channelNameBox, Gtk::POS_RIGHT, 1, 1);
			m_operatorBox.append("<");
			m_operatorBox.append("<=");
			m_operatorBox.append("==");
			m_operatorBox.append(">");
			m_operatorBox.append(">=");
			m_operatorBox.append("!=");
			m_operatorBox.append("starts with");
			m_operatorBox.append("contains");
		m_grid.attach_next_to(m_targetEntry, m_operatorBox, Gtk::POS_RIGHT, 1, 1);

	show_all();
}

HaltConditionsDialog::~HaltConditionsDialog()
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

void HaltConditionsDialog::RefreshChannels()
{
	string old_chan = m_channelNameBox.get_active_text();

	m_channelNameBox.remove_all();
	m_chanptrs.clear();

	//Populate channel list
	//Assume hardware channels only have one output for now
	bool first = true;
	for(size_t j=0; j<m_parent->GetScopeCount(); j++)
	{
		auto scope = m_parent->GetScope(j);
		for(size_t k=0; k<scope->GetChannelCount(); k++)
		{
			auto c = scope->GetChannel(k);
			m_channelNameBox.append(c->GetDisplayName());
			m_chanptrs[c->GetDisplayName()] = StreamDescriptor(c, 0);

			if(first && (old_chan == ""))
			{
				first = false;
				m_channelNameBox.set_active_text(c->GetDisplayName());
			}
		}
	}

	//Populate filters
	auto filters = Filter::GetAllInstances();
	for(auto d : filters)
	{
		auto nstreams = d->GetStreamCount();
		for(size_t i=0; i<nstreams; i++)
		{
			string name = d->GetDisplayName();
			if(nstreams > 1)
				name += string(".") + d->GetStreamName(i);

			m_channelNameBox.append(name);
			m_chanptrs[name] = StreamDescriptor(d, i);
		}
	}

	if(old_chan != "")
		m_channelNameBox.set_active_text(old_chan);
}

/**
	@brief Check if we should halt the trigger
 */
bool HaltConditionsDialog::ShouldHalt(int64_t& timestamp)
{
	//If conditional halt is not enabled, no sense checking conditions
	if(!m_haltEnabledButton.get_active())
		return false;

	//Get the channel we're looking at
	auto chan = GetHaltChannel();
	auto filter = dynamic_cast<Filter*>(chan.m_channel);

	//Don't check if no data to look at
	auto data = chan.m_channel->GetData(chan.m_stream);
	auto adata = dynamic_cast<AnalogWaveform*>(data);
	if(data->m_offsets.empty())
		return false;

	//Target for matching
	auto text = m_targetEntry.get_text();
	double value = chan.GetYAxisUnits().ParseString(text);

	//Figure out the match filter and check
	auto sfilter = m_operatorBox.get_active_text();
	size_t len = data->m_offsets.size();
	if(sfilter == "<")
	{
		//Expect analog data
		if(adata == NULL)
			return false;

		for(size_t i=0; i<len; i++)
		{
			if(adata->m_samples[i] < value)
			{
				timestamp = data->m_offsets[i] * data->m_timescale;
				return true;
			}
		}
	}

	else if(sfilter == "<=")
	{
		//Expect analog data
		if(adata == NULL)
			return false;

		for(size_t i=0; i<len; i++)
		{
			if(adata->m_samples[i] <= value)
			{
				timestamp = data->m_offsets[i] * data->m_timescale;
				return true;
			}
		}
	}

	else if(sfilter == "==")
	{
		//Match analog data
		if(adata != NULL)
		{
			for(size_t i=0; i<len; i++)
			{
				if(adata->m_samples[i] == value)
				{
					timestamp = data->m_offsets[i] * data->m_timescale;
					return true;
				}
			}
		}

		//TODO: match digital data

		//Match filters
		else if(filter != NULL)
		{
			for(size_t i=0; i<len; i++)
			{
				if(filter->GetText(i) == text)
				{
					timestamp = data->m_offsets[i] * data->m_timescale;
					return true;
				}
			}
		}
	}
	else if(sfilter == ">=")
	{
		//Expect analog data
		if(adata == NULL)
			return false;

		for(size_t i=0; i<len; i++)
		{
			if(adata->m_samples[i] >= value)
			{
				timestamp = data->m_offsets[i] * data->m_timescale;
				return true;
			}
		}
	}

	else if(sfilter == ">")
	{
		//Expect analog data
		if(adata == NULL)
			return false;

		for(size_t i=0; i<len; i++)
		{
			if(adata->m_samples[i] > value)
			{
				timestamp = data->m_offsets[i] * data->m_timescale;
				return true;
			}
		}
	}

	else if(sfilter == "!=")
	{
		//Match analog data
		if(adata != NULL)
		{
			for(size_t i=0; i<len; i++)
			{
				if(adata->m_samples[i] != value)
				{
					timestamp = data->m_offsets[i] * data->m_timescale;
					return true;
				}
			}
		}

		//TODO: match digital data

		//Match filters
		else if(filter != NULL)
		{
			for(size_t i=0; i<len; i++)
			{
				if(filter->GetText(i) != text)
				{
					timestamp = data->m_offsets[i] * data->m_timescale;
					return true;
				}
			}
		}
	}

	else if(sfilter == "starts with")
	{
		//Expect filter data
		if(filter == NULL)
			return false;

		for(size_t i=0; i<len; i++)
		{
			if(filter->GetText(i).find(text) == 0)
			{
				timestamp = data->m_offsets[i] * data->m_timescale;
				return true;
			}
		}
	}

	else if(sfilter == "contains")
	{
		//Expect filter data
		if(filter == NULL)
			return false;

		for(size_t i=0; i<len; i++)
		{
			if(filter->GetText(i).find(text) != string::npos)
			{
				timestamp = data->m_offsets[i] * data->m_timescale;
				return true;
			}
		}
	}

	return false;
}
