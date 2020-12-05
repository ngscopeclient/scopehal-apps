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
	@brief Editor for filter graphs
 */

#ifndef FilterGraphEditorWidget_h
#define FilterGraphEditorWidget_h

class FilterGraphEditor;
class FilterGraphEditorWidget;

class FilterGraphEditorPort
{
public:
	std::string m_label;
	Glib::RefPtr<Pango::Layout> m_layout;
	Rect m_rect;
};

/**
	@brief Graphical representation of a single FlowGraphNode
 */
class FilterGraphEditorNode
{
public:
	FilterGraphEditorNode(FilterGraphEditorWidget* parent, OscilloscopeChannel* chan);

	void UpdateSize();
	void Render(const Cairo::RefPtr<Cairo::Context>& cr);

	FilterGraphEditorWidget* m_parent;
	OscilloscopeChannel* m_channel;
	Rect m_rect;
	bool m_positionValid;
	int m_margin;

protected:
	Glib::RefPtr<Pango::Layout> m_titleLayout;

	Rect m_titleRect;
	std::vector<FilterGraphEditorPort> m_inputPorts;
	std::vector<FilterGraphEditorPort> m_outputPorts;
};

/**
	@brief Editor for a filter graph
 */
class FilterGraphEditorWidget	: public Gtk::DrawingArea
{
public:
	FilterGraphEditorWidget(FilterGraphEditor* parent);
	virtual ~FilterGraphEditorWidget();

	void Refresh();

	virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);

	PreferenceManager& GetPreferences();

protected:
	void RemoveStaleNodes();
	void CreateNodes();
	void UpdateSizes();
	void UpdatePositions();
	void AssignInitialPositions(std::set<FilterGraphEditorNode*>& nodes);

protected:
	FilterGraphEditor* m_parent;

	std::map<OscilloscopeChannel*, FilterGraphEditorNode*> m_nodes;
};

#endif
