/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2017 Andrew D. Zonenberg                                                                          *
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
	@brief Declaration of OscilloscopeView
 */

#ifndef OscilloscopeView_h
#define OscilloscopeView_h

#include "../scopehal/ChannelRenderer.h"
#include "../scopehal/TimescaleRenderer.h"

class Oscilloscope;
class OscilloscopeWindow;

/**
	@brief Viewer for oscilloscope signals
 */
class OscilloscopeView : public Gtk::Layout
{
public:
	OscilloscopeView(Oscilloscope* scope, OscilloscopeWindow* parent);
	virtual ~OscilloscopeView();

	typedef std::map<OscilloscopeChannel*, ChannelRenderer*> ChannelMap;
	ChannelMap m_renderers;

	int m_width;
	int m_height;
	void Resize();

	void Refresh();
	void SetSizeDirty();

protected:
	virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);
	virtual bool on_button_press_event(GdkEventButton* event);
	virtual bool on_scroll_event (GdkEventScroll* ev);

	bool IsAnalogChannelSelected();

	void OnOffsetUp();
	void OnOffsetDown();
	void OnZoomInVertical();
	void OnZoomOutVertical();
	void OnAutoFitVertical();
	void OnProtocolDecode(std::string protocol);

	bool m_sizeDirty;

	Oscilloscope* m_scope;
	OscilloscopeWindow* m_parent;

	void MakeTimeRanges(std::vector<time_range>& ranges);

	int64_t m_cursorpos;

	Gtk::Menu m_channelContextMenu;
	OscilloscopeChannel* m_selectedChannel;
	Gtk::Menu m_protocolDecodeMenu;
	TimescaleRenderer* m_timescaleRender;
};

#endif
