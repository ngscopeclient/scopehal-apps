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
	@brief Implementation of ChannelPropertiesDialog
 */
#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "ChannelPropertiesDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ChannelPropertiesDialog::ChannelPropertiesDialog(
	OscilloscopeWindow* parent,
	OscilloscopeChannel* chan)
	: Gtk::Dialog(string("Channel properties"), *parent, Gtk::DIALOG_MODAL)
	, m_groupList(1)
	, m_chan(chan)
	, m_hasThreshold(false)
	, m_hasHysteresis(false)
	, m_hasFrequency(false)
	, m_hasBandwidth(false)
	, m_hasDeskew(false)
	, m_hasAttenuation(false)
	, m_hasInvert(false)
	, m_hasMux(false)
{
	add_button("OK", Gtk::RESPONSE_OK);
	add_button("Cancel", Gtk::RESPONSE_CANCEL);

	auto scope = chan->GetScope();
	auto index = chan->GetIndex();

	Unit fs(Unit::UNIT_FS);
	Unit volts(Unit::UNIT_VOLTS);
	Unit hz(Unit::UNIT_HZ);

	get_vbox()->pack_start(m_grid, Gtk::PACK_EXPAND_WIDGET);
		m_grid.attach(m_scopeNameLabel, 0, 0, 1, 1);
			m_scopeNameLabel.set_text("Scope");
			m_scopeNameLabel.set_halign(Gtk::ALIGN_START);
		m_grid.attach_next_to(m_scopeNameEntry, m_scopeNameLabel, Gtk::POS_RIGHT, 1, 1);
			m_scopeNameEntry.set_halign(Gtk::ALIGN_START);
			m_scopeNameEntry.set_text(
				scope->m_nickname + " (" + scope->GetName() + ", serial " + scope->GetSerial() + ")");

		m_grid.attach_next_to(m_channelNameLabel, m_scopeNameLabel, Gtk::POS_BOTTOM, 1, 1);
			m_channelNameLabel.set_text("Channel");
			m_channelNameLabel.set_halign(Gtk::ALIGN_START);
		m_grid.attach_next_to(m_channelNameEntry, m_channelNameLabel, Gtk::POS_RIGHT, 1, 1);

			//TODO: revise this if anything supports multiple active digital probe types
			if(chan->GetType() == OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
				m_channelNameEntry.set_text(chan->GetHwname());
			else
			{
				string probename = chan->GetProbeName();
				if(probename.empty())
					m_channelNameEntry.set_text(chan->GetHwname() + " (passive or no probe connected)");
				else
					m_channelNameEntry.set_text(chan->GetHwname() + " (" + probename + ")");
			}
			m_channelNameEntry.set_halign(Gtk::ALIGN_START);

		m_grid.attach_next_to(m_channelDisplayNameLabel, m_channelNameLabel, Gtk::POS_BOTTOM, 1, 1);
			m_channelDisplayNameLabel.set_text("Display Name");
			m_channelDisplayNameLabel.set_halign(Gtk::ALIGN_START);
		m_grid.attach_next_to(m_channelDisplayNameEntry, m_channelDisplayNameLabel, Gtk::POS_RIGHT, 1, 1);
			m_channelDisplayNameEntry.set_text(chan->GetDisplayName());

		m_grid.attach_next_to(m_channelColorLabel, m_channelDisplayNameLabel, Gtk::POS_BOTTOM, 1, 1);
			m_channelColorLabel.set_text("Waveform Color");
			m_channelColorLabel.set_halign(Gtk::ALIGN_START);
		m_grid.attach_next_to(m_channelColorButton, m_channelColorLabel, Gtk::POS_RIGHT, 1, 1);
			m_channelColorButton.set_color(Gdk::Color(chan->m_displaycolor));

		Gtk::Label* anchorLabel = &m_channelColorLabel;

	if(chan->IsPhysicalChannel())
	{
		switch(chan->GetType())
		{
			case OscilloscopeChannel::CHANNEL_TYPE_ANALOG:
				{
					//Deskew - only on physical analog channels for now
					m_grid.attach_next_to(m_deskewLabel, m_channelColorLabel, Gtk::POS_BOTTOM, 1, 1);
						m_deskewLabel.set_text("Deskew");
						m_deskewLabel.set_halign(Gtk::ALIGN_START);
					m_grid.attach_next_to(m_deskewEntry, m_deskewLabel, Gtk::POS_RIGHT, 1, 1);
					m_hasDeskew = true;
					m_deskewEntry.set_text(fs.PrettyPrint(chan->GetDeskew()));

					//Attenuation
					m_grid.attach_next_to(m_attenuationLabel, m_deskewLabel, Gtk::POS_BOTTOM, 1, 1);
						m_attenuationLabel.set_text("Attenuation");
						m_attenuationLabel.set_halign(Gtk::ALIGN_START);
					m_grid.attach_next_to(m_attenuationEntry, m_attenuationLabel, Gtk::POS_RIGHT, 1, 1);
					m_hasAttenuation = true;
					m_attenuationEntry.set_text(to_string(chan->GetAttenuation()));

					anchorLabel = &m_attenuationLabel;

					//Bandwidth limiters
					m_grid.attach_next_to(m_bandwidthLabel, *anchorLabel, Gtk::POS_BOTTOM, 1, 1);
							m_bandwidthLabel.set_text("BW Limit");
						m_bandwidthLabel.set_halign(Gtk::ALIGN_START);
					m_grid.attach_next_to(m_bandwidthBox, m_bandwidthLabel, Gtk::POS_RIGHT, 1, 1);
					m_hasBandwidth = true;

					//Populate bandwidth limiter box
					auto limits = scope->GetChannelBandwidthLimiters(index);
					for(auto limit : limits)
					{
						if(limit == 0)
							m_bandwidthBox.append("Full");
						else
							m_bandwidthBox.append(hz.PrettyPrint(limit * 1e6));
					}
					unsigned int limit = scope->GetChannelBandwidthLimit(index);
					if(limit == 0)
						m_bandwidthBox.set_active_text("Full");
					else
						m_bandwidthBox.set_active_text(hz.PrettyPrint(limit * 1e6));

					anchorLabel = &m_bandwidthLabel;

					//Inversion
					if(chan->CanInvert())
					{
						m_grid.attach_next_to(m_invertLabel, *anchorLabel, Gtk::POS_BOTTOM, 1, 1);
								m_invertLabel.set_text("Invert");
							m_invertLabel.set_halign(Gtk::ALIGN_START);
						m_grid.attach_next_to(m_invertButton, m_invertLabel, Gtk::POS_RIGHT, 1, 1);
							m_invertButton.set_active(chan->IsInverted());

						anchorLabel = &m_invertLabel;

						m_hasInvert = true;
					}

					//Mux setting
					if(scope->HasInputMux(index))
					{
						auto names = scope->GetInputMuxNames(index);

						//Add input mux box
						m_grid.attach_next_to(m_muxLabel, *anchorLabel, Gtk::POS_BOTTOM, 1, 1);
								m_muxLabel.set_text("Input Mux");
							m_muxLabel.set_halign(Gtk::ALIGN_START);
						m_grid.attach_next_to(m_muxBox, m_muxLabel, Gtk::POS_RIGHT, 1, 1);

						//Fill it
						for(auto n : names)
							m_muxBox.append(n);
						m_muxBox.set_active_text(names[scope->GetInputMuxSetting(index)]);

						anchorLabel = &m_muxLabel;
						m_hasMux = true;
					}

					//ADC configuration
					if(scope->IsADCModeConfigurable())
					{
						//Populate bank
						auto bank = scope->GetAnalogBank(index);
						if(bank.size() > 1)
						{
							m_grid.attach_next_to(m_groupLabel, *anchorLabel, Gtk::POS_BOTTOM, 1, 1);
								m_groupLabel.set_text("Bank");
								m_groupLabel.set_halign(Gtk::ALIGN_START);
							m_grid.attach_next_to(m_groupList, m_groupLabel, Gtk::POS_RIGHT, 1, 1);

							for(auto c : bank)
							{
								if(c == chan)
									continue;

								m_groupList.append(c->GetDisplayName());
							}

							m_groupList.set_headers_visible(false);

							anchorLabel = &m_groupLabel;
						}

						//Add ADC mode box
						m_grid.attach_next_to(m_adcModeLabel, *anchorLabel, Gtk::POS_BOTTOM, 1, 1);
								m_adcModeLabel.set_text("ADC Mode");
							m_adcModeLabel.set_halign(Gtk::ALIGN_START);
						m_grid.attach_next_to(m_adcModeBox, m_adcModeLabel, Gtk::POS_RIGHT, 1, 1);
						m_hasBandwidth = true;

						//Populate mode box
						auto modes = scope->GetADCModeNames(index);
						for(auto mode : modes)
							m_adcModeBox.append(mode);
						m_adcModeBox.set_active_text(modes[scope->GetADCMode(index)]);
						anchorLabel = &m_adcModeLabel;
						m_hasAdcMode = true;
					}

					if(scope->CanAutoZero(index))
					{
						m_grid.attach_next_to(m_autoZeroLabel, *anchorLabel, Gtk::POS_BOTTOM, 1, 1);
						m_grid.attach_next_to(m_autoZeroButton, m_autoZeroLabel, Gtk::POS_RIGHT, 1, 1);
							m_autoZeroButton.set_label("Auto Zero");
						anchorLabel = &m_autoZeroLabel;

						m_autoZeroButton.signal_clicked().connect(
							sigc::mem_fun(*this, &ChannelPropertiesDialog::OnAutoZero));
					}
				}
				break;

			//Logic properties - only on physical digital channels
			case OscilloscopeChannel::CHANNEL_TYPE_DIGITAL:
				{
					if(scope->IsDigitalThresholdConfigurable())
					{
						m_grid.attach_next_to(m_thresholdLabel, *anchorLabel, Gtk::POS_BOTTOM, 1, 1);
								m_thresholdLabel.set_text("Threshold");
							m_thresholdLabel.set_halign(Gtk::ALIGN_START);
						m_grid.attach_next_to(m_thresholdEntry, m_thresholdLabel, Gtk::POS_RIGHT, 1, 1);

						m_thresholdEntry.set_text(volts.PrettyPrint(scope->GetDigitalThreshold(index)));

						anchorLabel = &m_thresholdLabel;

						m_hasThreshold = true;
					}

					if(scope->IsDigitalHysteresisConfigurable())
					{
						m_grid.attach_next_to(m_hysteresisLabel, *anchorLabel, Gtk::POS_BOTTOM, 1, 1);
							m_hysteresisLabel.set_text("Hysteresis");
							m_hysteresisLabel.set_halign(Gtk::ALIGN_START);
						m_grid.attach_next_to(m_hysteresisEntry, m_hysteresisLabel, Gtk::POS_RIGHT, 1, 1);

						m_hysteresisEntry.set_text(volts.PrettyPrint(scope->GetDigitalHysteresis(index)));

						anchorLabel = &m_hysteresisLabel;

						m_hasHysteresis = true;
					}

					//See what else is in the bank
					auto bank = scope->GetDigitalBank(index);
					if(bank.size() > 1)
					{
						m_grid.attach_next_to(m_groupLabel, *anchorLabel, Gtk::POS_BOTTOM, 1, 1);
							m_groupLabel.set_text("Bank");
							m_groupLabel.set_halign(Gtk::ALIGN_START);
						m_grid.attach_next_to(m_groupList, m_groupLabel, Gtk::POS_RIGHT, 1, 1);

						for(auto c : bank)
						{
							if(c == chan)
								continue;

							m_groupList.append(c->GetDisplayName());
						}

						m_groupList.set_headers_visible(false);

						anchorLabel = &m_groupLabel;
					}
				}
				break;

			default:
				break;
		}

		//Spectrum properties - only on physical frequency domain channels
		if(chan->GetXAxisUnits() == hz)
		{
			m_grid.attach_next_to(m_centerLabel, *anchorLabel, Gtk::POS_BOTTOM, 1, 1);
				m_centerLabel.set_text("Center Frequency");
				m_centerLabel.set_halign(Gtk::ALIGN_START);
			m_grid.attach_next_to(m_centerEntry, m_centerLabel, Gtk::POS_RIGHT, 1, 1);

			//Commented out to prevent compile warning about unused value.
			//Uncomment if adding new widgets later in the dialog.
			//anchorLabel = &m_centerLabel;

			m_centerEntry.set_text(hz.PrettyPrint(scope->GetCenterFrequency(index)));

			m_hasFrequency = true;
		}
	}

	show_all();
}

ChannelPropertiesDialog::~ChannelPropertiesDialog()
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Output

void ChannelPropertiesDialog::ConfigureChannel()
{
	m_chan->SetDisplayName(m_channelDisplayNameEntry.get_text());
	m_chan->m_displaycolor = m_channelColorButton.get_color().to_string();

	Unit volts(Unit::UNIT_VOLTS);
	Unit fs(Unit::UNIT_FS);
	Unit hz(Unit::UNIT_HZ);

	if(m_hasThreshold)
		m_chan->SetDigitalThreshold(volts.ParseString(m_thresholdEntry.get_text()));

	if(m_hasHysteresis)
		m_chan->SetDigitalHysteresis(volts.ParseString(m_hysteresisEntry.get_text()));

	if(m_hasFrequency)
		m_chan->SetCenterFrequency(hz.ParseString(m_centerEntry.get_text()));

	if(m_hasDeskew)
		m_chan->SetDeskew(fs.ParseString(m_deskewEntry.get_text()));

	if(m_hasAttenuation)
		m_chan->SetAttenuation(stof(m_attenuationEntry.get_text()));

	if(m_hasAdcMode)
		m_chan->GetScope()->SetADCMode(m_chan->GetIndex(), m_adcModeBox.get_active_row_number());

	if(m_hasInvert)
		m_chan->Invert(m_invertButton.get_active());

	if(m_hasBandwidth)
	{
		auto sbw = m_bandwidthBox.get_active_text();
		if(sbw == "Full")
			m_chan->SetBandwidthLimit(0);
		else
			m_chan->SetBandwidthLimit(Unit(Unit::UNIT_HZ).ParseString(sbw)/1e6);
	}

	if(m_hasMux)
		m_chan->SetInputMux(m_muxBox.get_active_row_number());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

void ChannelPropertiesDialog::OnAutoZero()
{
	m_chan->AutoZero();
}
