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
class ChannelPropertiesDialog;
class FilterDialog;

class FilterGraphEditorPort
{
public:
	std::string m_label;
	Glib::RefPtr<Pango::Layout> m_layout;
	Rect m_rect;
	size_t m_index;
};

/**
	@brief Graphical representation of a single OscilloscopeChannel
 */
class FilterGraphEditorNode
{
public:
	FilterGraphEditorNode(FilterGraphEditorWidget* parent, OscilloscopeChannel* chan);
	virtual ~FilterGraphEditorNode();

	void UpdateSize();
	void Render(const Cairo::RefPtr<Cairo::Context>& cr);

	FilterGraphEditorWidget* m_parent;
	OscilloscopeChannel* m_channel;
	Rect m_rect;
	bool m_positionValid;
	int m_margin;
	int m_column;

	std::vector<FilterGraphEditorPort>& GetInputPorts()
	{ return m_inputPorts; }

	std::vector<FilterGraphEditorPort>& GetOutputPorts()
	{ return m_outputPorts; }

protected:
	Glib::RefPtr<Pango::Layout> m_titleLayout;
	Glib::RefPtr<Pango::Layout> m_paramLayout;
	Rect m_paramRect;

	Rect m_titleRect;
	std::vector<FilterGraphEditorPort> m_inputPorts;
	std::vector<FilterGraphEditorPort> m_outputPorts;
};

/**
	@brief A path between two FilterGraphEditorNode's
 */
class FilterGraphEditorPath
{
public:
	FilterGraphEditorPath(
		FilterGraphEditorNode* fromnode,
		size_t fromport,
		FilterGraphEditorNode* tonode,
		size_t toport);

	FilterGraphEditorNode* m_fromNode;
	size_t m_fromPort;

	FilterGraphEditorNode* m_toNode;
	size_t m_toPort;

	//The actual polyline
	std::vector<vec2f> m_polyline;
};

/**
	@brief A column of routing space between columns of nodes
 */
class FilterGraphRoutingColumn
{
public:
	int m_left;
	int m_right;

	std::set<FilterGraphEditorNode*> m_nodes;
	std::list<int> m_freeVerticalChannels;
	std::map<StreamDescriptor, int> m_usedVerticalChannels;

	int GetVerticalChannel(StreamDescriptor stream);
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

	FilterGraphEditorNode* GetSelectedNode()
	{ return m_selectedNode; }

	PreferenceManager& GetPreferences();

	void OnNodeDeleted(FilterGraphEditorNode* node);

	//Drag mode
	enum DragMode
	{
		DRAG_NONE,
		DRAG_NODE,
		DRAG_NET_SOURCE
	};

	DragMode GetDragMode()
	{ return m_dragMode; }

	StreamDescriptor GetSourceStream();

	vec2f GetMousePosition()
	{ return m_mousePosition; }

protected:

	//Event handlers
	virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);
	virtual bool on_button_press_event(GdkEventButton* event);
	void OnLeftClick(GdkEventButton* event);
	void OnRightClick(GdkEventButton* event);
	virtual bool on_button_release_event(GdkEventButton* event);
	virtual bool on_motion_notify_event(GdkEventMotion* event);
	void OnDoubleClick(GdkEventButton* event);
	bool OnFilterPropertiesDialogClosed(GdkEventAny* ignored);
	void OnChannelPropertiesDialogResponse(int response);

	//Input helpers
	FilterGraphEditorNode* HitTestNode(int x, int y);
	FilterGraphEditorPort* HitTestNodeOutput(int x, int y);
	FilterGraphEditorPort* HitTestNodeInput(int x, int y);
	FilterGraphEditorPath* HitTestPath(int x, int y);

	//Refresh logic
	void RemoveStaleNodes();
	void CreateNodes();
	void UpdateSizes();
	void UpdatePositions();
	void UnplaceMisplacedNodes();
	void AssignNodesToColumns();
	void UpdateColumnPositions();
	void AssignInitialPositions(std::set<FilterGraphEditorNode*>& nodes);

	void RemoveStalePaths();
	void CreatePaths();
	void ResolvePathConflicts();
	void RoutePath(FilterGraphEditorPath* path);

	//Context menu handlers
	void OnDelete();

protected:
	Gtk::Menu m_contextMenu;

	FilterGraphEditor* m_parent;

	std::map<OscilloscopeChannel*, FilterGraphEditorNode*> m_nodes;

	//Paths are indexed by destination, since an input can have only one connection
	typedef std::pair<FilterGraphEditorNode*, size_t> NodePort;
	std::map<NodePort, FilterGraphEditorPath*> m_paths;

	std::vector<FilterGraphRoutingColumn*> m_columns;

	ChannelPropertiesDialog* m_channelPropertiesDialog;
	FilterDialog* m_filterDialog;

	//Path highlighted by mouseover
	FilterGraphEditorPath* m_highlightedPath;

	DragMode m_dragMode;
	FilterGraphEditorNode* m_selectedNode;
	int m_dragDeltaY;
	size_t m_sourcePort;

	//Current mouse position
	vec2f m_mousePosition;
};

#endif
