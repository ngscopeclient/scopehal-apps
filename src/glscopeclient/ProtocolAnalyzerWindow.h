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
	@brief  Declaration of ProtocolAnalyzerWindow
 */
#ifndef ProtocolAnalyzerWindow_h
#define ProtocolAnalyzerWindow_h

class OscilloscopeWindow;

#include "../../lib/scopehal/PacketDecoder.h"

typedef std::pair<time_t, int64_t> TimePoint;

class ProtocolAnalyzerColumns : public Gtk::TreeModel::ColumnRecord
{
public:
	ProtocolAnalyzerColumns(PacketDecoder* decoder);

	Gtk::TreeModelColumn<Glib::ustring>					m_timestamp;
	Gtk::TreeModelColumn<TimePoint>						m_capturekey;
	Gtk::TreeModelColumn<int64_t>						m_offset;
	std::vector< Gtk::TreeModelColumn<Glib::ustring> >	m_headers;
	Gtk::TreeModelColumn<Glib::ustring>					m_data;
	Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>>		m_image;
	Gtk::TreeModelColumn<Gdk::Color>					m_bgcolor;
	Gtk::TreeModelColumn<Gdk::Color>					m_fgcolor;
	Gtk::TreeModelColumn<int>					m_height;
	Gtk::TreeModelColumn<bool>							m_visible;
};

class ProtocolDisplayFilter;

class ProtocolDisplayFilterClause
{
public:
	ProtocolDisplayFilterClause(std::string str, size_t& i);
	ProtocolDisplayFilterClause(const ProtocolDisplayFilterClause&) =delete;
	ProtocolDisplayFilterClause& operator=(const ProtocolDisplayFilterClause&) =delete;

	virtual ~ProtocolDisplayFilterClause();

	bool Validate(std::vector<std::string> headers);

	std::string Evaluate(
		const Gtk::TreeRow& row,
		ProtocolAnalyzerColumns& cols);

	enum
	{
		TYPE_IDENTIFIER,
		TYPE_STRING,
		TYPE_NUMBER,
		TYPE_EXPRESSION,
		TYPE_ERROR
	} m_type;

	std::string m_identifier;
	std::string m_string;
	float m_number;
	ProtocolDisplayFilter* m_expression;
	bool m_invert;

	size_t m_cachedIndex;
};

class ProtocolDisplayFilter
{
public:
	ProtocolDisplayFilter(std::string str, size_t& i);
	ProtocolDisplayFilter(const ProtocolDisplayFilterClause&) =delete;
	ProtocolDisplayFilter& operator=(const ProtocolDisplayFilter&) =delete;
	virtual ~ProtocolDisplayFilter();

	static void EatSpaces(std::string str, size_t& i);

	bool Validate(std::vector<std::string> headers);

	bool Match(
		const Gtk::TreeRow& row,
		ProtocolAnalyzerColumns& cols);
	std::string Evaluate(
		const Gtk::TreeRow& row,
		ProtocolAnalyzerColumns& cols);

protected:
	std::vector<ProtocolDisplayFilterClause*> m_clauses;
	std::vector<std::string> m_operators;
};

/**
	@brief Window containing a protocol analyzer
 */
class ProtocolAnalyzerWindow : public Gtk::Dialog
{
public:
	ProtocolAnalyzerWindow(
		const std::string& title,
		OscilloscopeWindow* parent,
		PacketDecoder* decoder,
		WaveformArea* area);
	~ProtocolAnalyzerWindow();

	void OnWaveformDataReady();
	void RemoveHistory(TimePoint timestamp);

	PacketDecoder* GetDecoder()
	{ return m_decoder; }

protected:
	OscilloscopeWindow* m_parent;
	PacketDecoder* m_decoder;
	WaveformArea* m_area;

	virtual void on_hide();

	void OnApplyFilter();
	void OnFilterChanged();
	void OnFileExport();

	Gtk::MenuBar m_menu;
		Gtk::MenuItem m_fileMenuItem;
			Gtk::Menu m_fileMenu;
				Gtk::MenuItem m_fileExportMenuItem;
	Gtk::HBox m_filterRow;
		Gtk::Entry m_filterBox;
		Gtk::Button m_filterApplyButton;

	Gtk::ScrolledWindow m_scroller;
		Gtk::TreeView m_tree;
	Glib::RefPtr<Gtk::TreeStore> m_internalmodel;
	Glib::RefPtr<Gtk::TreeModelFilter> m_model;
	ProtocolAnalyzerColumns m_columns;

	void OnSelectionChanged();

	void FillOutRow(const Gtk::TreeRow& row, Packet* p, WaveformBase* data, std::vector<std::string>& headers);

	bool m_updating;
};

#endif
