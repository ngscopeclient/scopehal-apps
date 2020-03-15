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
	@brief  Declaration of HistoryWindow
 */

#ifndef HistoryWindow_h
#define HistoryWindow_h

class OscilloscopeWindow;

typedef std::map<OscilloscopeChannel*, CaptureChannelBase*> WaveformHistory;

class HistoryColumns : public Gtk::TreeModel::ColumnRecord
{
public:
	HistoryColumns();

	Gtk::TreeModelColumn<Glib::ustring>		m_timestamp;
	Gtk::TreeModelColumn<TimePoint>			m_capturekey;
	Gtk::TreeModelColumn<WaveformHistory>	m_history;
};

/**
	@brief Window containing a protocol analyzer
 */
class HistoryWindow : public Gtk::Window
{
public:
	HistoryWindow(OscilloscopeWindow* parent, Oscilloscope* scope);
	~HistoryWindow();

	void OnWaveformDataReady();
	void JumpToHistory(TimePoint timestamp);

	void SetMaxWaveforms(int n);

	void SerializeWaveforms(std::string dir, IDTable& table);

protected:
	virtual bool on_delete_event(GdkEventAny* ignored);
	virtual void OnSelectionChanged();

	Gtk::VBox m_vbox;
		Gtk::HBox m_hbox;
			Gtk::Label m_maxLabel;
			Gtk::Entry m_maxBox;
		Gtk::ScrolledWindow m_scroller;
			Gtk::TreeView m_tree;
		Glib::RefPtr<Gtk::TreeStore> m_model;
		Gtk::HBox m_status;
			Gtk::Label m_memoryLabel;
	HistoryColumns m_columns;

	OscilloscopeWindow* m_parent;
	Oscilloscope* m_scope;
	bool m_updating;
};

#endif
