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
	@brief Dialog for configuring channel properties
 */

#ifndef ChannelPropertiesDialog_h
#define ChannelPropertiesDialog_h

/**
	@brief Dialog for configuring a single scope channel
 */
class ChannelPropertiesDialog	: public Gtk::Dialog
{
public:
	ChannelPropertiesDialog(OscilloscopeWindow* parent, OscilloscopeChannel* chan);
	virtual ~ChannelPropertiesDialog();

	void ConfigureChannel();

protected:
	Gtk::Grid m_grid;

		//General properties shared by everything
		Gtk::Label m_scopeNameLabel;
			Gtk::Label m_scopeNameEntry;
		Gtk::Label m_channelNameLabel;
			Gtk::Label m_channelNameEntry;
		Gtk::Label m_channelDisplayNameLabel;
			Gtk::Entry m_channelDisplayNameEntry;
		Gtk::Label m_channelColorLabel;
			Gtk::ColorButton m_channelColorButton;
		Gtk::Label m_deskewLabel;
			Gtk::Entry m_deskewEntry;

		//Analog channel configuration
		Gtk::Label m_bandwidthLabel;
			Gtk::ComboBoxText m_bandwidthBox;

		//Logic channel configuration
		Gtk::Label m_thresholdLabel;
			Gtk::Entry m_thresholdEntry;
		Gtk::Label m_hysteresisLabel;
			Gtk::Entry m_hysteresisEntry;
		Gtk::Label m_groupLabel;
			Gtk::ListViewText m_groupList;

		//Frequency domain channel configuration
		Gtk::Label m_centerLabel;
			Gtk::Entry m_centerEntry;

	OscilloscopeChannel* m_chan;

	bool m_hasThreshold;
	bool m_hasHysteresis;
	bool m_hasFrequency;
	bool m_hasBandwidth;
	bool m_hasDeskew;
};

#endif
