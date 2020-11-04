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
	@brief Dialog for configuring trigger properties
 */

#ifndef TriggerPropertiesDialog_h
#define TriggerPropertiesDialog_h

#include "FilterDialog.h"

/**
	@brief Dialog for configuring trigger settings for a scope
 */
class TriggerPropertiesDialog	: public Gtk::Dialog
{
public:
	TriggerPropertiesDialog(OscilloscopeWindow* parent, Oscilloscope* scope);
	virtual ~TriggerPropertiesDialog();

	void ConfigureTrigger();

protected:
	void OnTriggerTypeChanged();
	void AddRows(Trigger* trig);

	void Clear();

	Gtk::Grid m_grid;
		Gtk::Label m_scopeNameLabel;
			Gtk::Label m_scopeNameEntry;
		Gtk::Label m_triggerTypeLabel;
			Gtk::ComboBoxText m_triggerTypeBox;
		Gtk::Label m_triggerOffsetLabel;
			Gtk::Entry m_triggerOffsetEntry;
	Gtk::Grid m_contentGrid;

	Oscilloscope* m_scope;

	std::vector<ChannelSelectorRow*> m_rows;
	std::vector<ParameterRowBase*> m_prows;
};

#endif
