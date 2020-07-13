/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
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

#ifndef HaltConditionsDialog_h
#define HaltConditionsDialog_h

/**
	@brief Dialog for configuring halt conditions
 */
class HaltConditionsDialog	: public Gtk::Dialog
{
public:
	HaltConditionsDialog(OscilloscopeWindow* parent);
	virtual ~HaltConditionsDialog();

	void RefreshChannels();

	bool ShouldHalt(int64_t& timestamp);
	bool ShouldMoveToHalt()
	{ return m_moveToEventButton.get_active(); }

	OscilloscopeChannel* GetHaltChannel()
	{ return m_chanptrs[m_channelNameBox.get_active_text()]; }

protected:
	Gtk::Grid m_grid;
		Gtk::CheckButton m_haltEnabledButton;
		Gtk::CheckButton m_moveToEventButton;
		Gtk::Label m_channelNameLabel;
			Gtk::ComboBoxText m_channelNameBox;
			Gtk::ComboBoxText m_operatorBox;
			Gtk::Entry m_targetEntry;

	OscilloscopeWindow* m_parent;
	std::map<std::string, OscilloscopeChannel*> m_chanptrs;
};

#endif
