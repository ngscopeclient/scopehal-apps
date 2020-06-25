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
	@author Katharina B.
	@brief  A window that allows manipulation of preference values
 */
 
#ifndef PreferenceDialog_h
#define PreferenceDialog_h

#include <memory>
#include <vector>
#include "glscopeclient.h"
#include "PreferenceManager.h"

class OscilloscopeWindow;

struct BooleanRow
{
    std::string m_identifier;
    Gtk::Label m_label;
    Gtk::CheckButton m_check;
};

struct StringRealRow
{
    std::string m_identifier;
    Gtk::Label m_label;
    Gtk::Entry m_value;
};


class PreferenceDialog : public Gtk::Dialog
{
public:
    PreferenceDialog(OscilloscopeWindow* parent, PreferenceManager& preferences);
 
    void SaveChanges();
 
protected:
    void CreateWidgets();
 
protected:
    PreferenceManager& m_preferences;
    std::vector<std::unique_ptr<BooleanRow>> m_booleanRows;
    std::vector<std::unique_ptr<StringRealRow>> m_stringRealRows;
    
    Gtk::Grid m_grid;
};


#endif // PreferenceDialog_h
