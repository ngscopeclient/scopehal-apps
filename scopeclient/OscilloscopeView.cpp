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
	@brief Implementation of OscilloscopeView
 */

#include "scopeclient.h"
#include "OscilloscopeWindow.h"
#include "../scopehal/Instrument.h"
#include "../scopehal/Oscilloscope.h"
#include "../scopehal/ProtocolDecoder.h"
#include "../scopehal/TimescaleRenderer.h"
#include "../scopehal/AnalogRenderer.h"
#include "OscilloscopeView.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

OscilloscopeView::OscilloscopeView(Oscilloscope* scope, OscilloscopeWindow* parent)
	: m_scope(scope)
	, m_parent(parent)
	, m_selectedChannel(NULL)
{
	m_height = 64;
	m_width = 64;

	m_sizeDirty = true;

	add_events(
		Gdk::EXPOSURE_MASK |
		Gdk::SCROLL_MASK |
		Gdk::BUTTON_PRESS_MASK |
		Gdk::BUTTON_RELEASE_MASK);

	m_cursorpos = 0;

	//Create the context menu for right-clicking on a channel
	auto item = Gtk::manage(new Gtk::MenuItem("Autofit vertical", false));
	item->signal_activate().connect(
		sigc::mem_fun(*this, &OscilloscopeView::OnAutoFitVertical));
	m_channelContextMenu.append(*item);
	item = Gtk::manage(new Gtk::MenuItem("Decode", false));
	item->set_submenu(m_protocolDecodeMenu);
	m_channelContextMenu.append(*item);

	//Fill the protocol decoder context menu
	vector<string> names;
	ProtocolDecoder::EnumProtocols(names);
	for(auto p : names)
	{
		item = Gtk::manage(new Gtk::MenuItem(p, false));
		item->signal_activate().connect(
			sigc::bind<string>(sigc::mem_fun(*this, &OscilloscopeView::OnProtocolDecode), p));
		m_protocolDecodeMenu.append(*item);
	}

	m_protocolDecodeMenu.show_all();
	m_channelContextMenu.show_all();
}

OscilloscopeView::~OscilloscopeView()
{
	for(ChannelMap::iterator it=m_renderers.begin(); it != m_renderers.end(); ++it)
		delete it->second;
	m_renderers.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

void OscilloscopeView::SetSizeDirty()
{
	m_sizeDirty = true;
	queue_draw();
}

bool OscilloscopeView::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
	try
	{
		Glib::RefPtr<Gdk::Window> window = get_bin_window();
		if(window)
		{
			//printf("========== NEW FRAME ==========\n");

			//Get dimensions of the virtual canvas (max of requested size and window size)
			Gtk::Allocation allocation = get_allocation();
			int width = allocation.get_width();
			int height = allocation.get_height();
			if(m_width > width)
				width = m_width;
			if(m_height > height)
				m_height = height;

			//Get the visible area of the window
			int pwidth = get_width();
			//int pheight = get_height();
			int xoff = get_hadjustment()->get_value();
			int yoff = get_vadjustment()->get_value();

			//Set up drawing context
			cr->save();
			cr->translate(-xoff, -yoff);

			//Fill background
			cr->set_source_rgb(0, 0, 0);
			cr->rectangle(0, 0, width, height);
			cr->fill();

			//We do crazy stuff in which stuff moves around every time we render. As a result, partial redraws will fail
			//horribly. If the clip region isn't the full window, redraw with the full region selected.
			double clip_x1, clip_y1, clip_x2, clip_y2;
			cr->get_clip_extents(clip_x1, clip_y1, clip_x2, clip_y2);
			int clipwidth = clip_x2 - clip_x1;
			if(clipwidth != pwidth)
				queue_draw();

			//Re-calculate mappings
			vector<time_range> ranges;
			MakeTimeRanges(ranges);

			//All good, draw individual channels
			//Draw channels in numerical order.
			//This allows painters-algorithm handling of protocol decoders that wish to be drawn
			//on top of the original channel.
			m_timescaleRender->Render(cr, width, 0+xoff, pwidth+xoff, ranges);
			for(size_t i=0; i<m_scope->GetChannelCount(); i++)
			{
				auto chan = m_scope->GetChannel(i);
				auto it = m_renderers.find(chan);
				if( (it == m_renderers.end()) || (it->second == NULL) )
				{
					//LogWarning("Channel \"%s\" has no renderer\n", chan->m_displayname.c_str());
					continue;
				}
				it->second->Render(cr, width, 0 + xoff, pwidth + xoff, ranges);
			}

			//Draw zigzag lines over the channel backgrounds
			//Don't draw break at end of last range, though
			for(size_t i=0; i<ranges.size(); i++)
			{
				if((i+1) == ranges.size())
					break;

				time_range& range = ranges[i];
				float xshift = 5;
				float yshift = 5;
				float ymid = height/2;

				cr->save();

					//Set up path
					cr->move_to(range.xend,        0);
					cr->line_to(range.xend,        ymid - 2*yshift);
					cr->line_to(range.xend+xshift, ymid -   yshift);
					cr->line_to(range.xend-xshift, ymid +   yshift);
					cr->line_to(range.xend,        ymid + 2*yshift);
					cr->line_to(range.xend,        height);

					//Fill background
					cr->set_source_rgb(1,1,1);
					cr->set_line_width(10);
					cr->stroke_preserve();

					//Fill foreground
					cr->set_source_rgb(0,0,0);
					cr->set_line_width(6);
					cr->stroke();

				cr->restore();
			}

			//Figure out time scale for cursor
			float tscale = 0;
			if(m_scope->GetChannelCount() != 0)
			{
				OscilloscopeChannel* chan = m_scope->GetChannel(0);
				CaptureChannelBase* capture = chan->GetData();
				if(capture != NULL)
					 tscale = chan->m_timescale * capture->m_timescale;
			}

			//Draw cursor
			for(size_t i=0; i<ranges.size(); i++)
			{
				time_range& range = ranges[i];

				//Draw cursor (if it's in this range)
				if( (m_cursorpos >= range.tstart) && (m_cursorpos <= range.tend) )
				{
					float dt = m_cursorpos - range.tstart;
					float dx = dt * tscale;
					float xpos = range.xstart + dx;

					cr->set_source_rgb(1,1,0);
					cr->move_to(xpos, 0);
					cr->line_to(xpos, height);
					cr->stroke();
				}
			}

			//Done
			cr->restore();

			//Draw channel name overlays (constant position regardles of X scrolling, but still scroll Y if needed)
			cr->save();
				cr->translate(1, -yoff);

				int labelmargin = 2;
				for(size_t i=0; i<m_scope->GetChannelCount(); i++)
				{
					auto chan = m_scope->GetChannel(i);
					auto it = m_renderers.find(chan);
					if(it == m_renderers.end())
						continue;
					auto r = it->second;

					auto ybot = r->m_ypos + r->m_height;

					int twidth, theight;
					GetStringWidth(cr, chan->GetHwname(), true, twidth, theight);

					cr->set_source_rgba(0, 0, 0, 0.75);
					cr->rectangle(0, ybot - theight - labelmargin*2, twidth + labelmargin*2, theight + labelmargin*2);
					cr->fill();

					cr->set_source_rgba(1, 1, 1, 1);

					cr->save();
						Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create (cr);
						cr->move_to(labelmargin, ybot - theight - labelmargin);
						Pango::FontDescription font("sans normal 10");
						font.set_weight(Pango::WEIGHT_NORMAL);
						tlayout->set_font_description(font);
						tlayout->set_text(chan->GetHwname());
						tlayout->update_from_cairo_context(cr);
						tlayout->show_in_cairo_context(cr);
					cr->restore();

				}

				/*cr->set_source_rgb(1, 0, 0);
				cr->move_to(0, 50);
				cr->line_to(50, 50);
				cr->stroke();
				*/
			cr->restore();
		}

		if(m_sizeDirty)
		{
			m_sizeDirty = false;
			Resize();
			queue_draw();
		}
	}


	catch(const JtagException& ex)
	{
		printf("%s\n", ex.GetDescription().c_str());
		exit(1);
	}

	return true;
}

bool OscilloscopeView::on_button_press_event(GdkEventButton* event)
{
	//Any mouse button will change the selected channel
	//Figure out which channel the cursor position is in
	//Painter's algorithm: the most recently drawn (top) channel is higher priority in the selection
	for(size_t i=0; i<m_scope->GetChannelCount(); i++)
	{
		auto chan = m_scope->GetChannel(i);
		auto it = m_renderers.find(chan);
		if(it == m_renderers.end())		//invisible channel
			continue;
		auto render = it->second;

		if( (event->y >= render->m_ypos) && (event->y <= (render->m_ypos + render->m_height)) )
			m_selectedChannel = chan;
	}
	//LogDebug("Selected channel %s\n", m_selectedChannel->GetHwname().c_str());

	switch(event->button)
	{
		//Left
		case 1:
		{
			//Re-calculate mappings
			vector<time_range> ranges;
			MakeTimeRanges(ranges);

			//Figure out time scale
			float tscale = 0;
			if(m_scope->GetChannelCount() != 0)
			{
				OscilloscopeChannel* chan = m_scope->GetChannel(0);
				CaptureChannelBase* capture = chan->GetData();
				if(capture != NULL)
					 tscale = chan->m_timescale * capture->m_timescale;
			}

			//Figure out which range the cursor position is in
			for(size_t i=0; i<ranges.size(); i++)
			{
				time_range& range = ranges[i];
				if( (event->x >= range.xstart) && (event->x <= range.xend) )
				{
					float dx = event->x - range.xstart;
					float dt = dx / tscale;

					//Round dt to the nearest integer rather than truncating
					int64_t dt_floor = floor(dt);
					if( (dt - dt_floor) > 0.5)
						dt_floor ++;

					m_cursorpos = range.tstart + dt_floor;
					queue_draw();
				}
			}
		}
		break;

		//Middle
		case 2:
			m_parent->OnZoomFit();
			break;

		//Right
		case 3:
		{
			//Gray out decoders that don't make sense for the type of channel we've selected
			bool foundDecoder = false;

			auto children = m_protocolDecodeMenu.get_children();
			if(m_selectedChannel != NULL)
			{
				for(auto item : children)
				{
					Gtk::MenuItem* menu = dynamic_cast<Gtk::MenuItem*>(item);
					if(menu == NULL)
						continue;

					auto decoder = ProtocolDecoder::CreateDecoder(
						menu->get_label(),
						"dummy",
						"");
					if(decoder->ValidateChannel(0, m_selectedChannel))
					{
						foundDecoder = true;
						menu->set_sensitive(true);
					}
					else
						menu->set_sensitive(false);
				}
			}

			//Gray out other context items that don't make sense
			children = m_channelContextMenu.get_children();
			for(auto item : children)
			{
				Gtk::MenuItem* menu = dynamic_cast<Gtk::MenuItem*>(item);
				if(menu == NULL)
					continue;

				string label = menu->get_label();

				//Only applies to analog channels
				if(label == "Autofit vertical")
				{
					//Cannot autofit without capture data
					if(m_selectedChannel == NULL)
					{
						menu->set_sensitive(false);
						continue;
					}
					auto data = m_selectedChannel->GetData();
					if(data == NULL)
					{
						menu->set_sensitive(false);
						continue;
					}

					//We have data
					//If it's not an analog channel, skip (makes no sense to autofit a digital channel)
					auto adata = dynamic_cast<AnalogCapture*>(data);
					if(adata != NULL)
						menu->set_sensitive(true);
					else
						menu->set_sensitive(false);
				}

				//Can only decode if we have a selected channel,
				//and at least one protocol decoder is willing to touch it.
				else if(label == "Decode")
				{
					if(m_selectedChannel && foundDecoder)
						menu->set_sensitive(true);
					else
						menu->set_sensitive(false);
				}
			}

			//Show the context menu
			m_channelContextMenu.popup(event->button, event->time);
		}
		break;

		//Front and back thumb buttons: vertical zoom
		case 9:
			if(m_selectedChannel)
				OnZoomInVertical();
			break;
		case 8:
			if(m_selectedChannel)
				OnZoomOutVertical();
			break;

		//Middle thumb button: autofit
		case 10:
			OnAutoFitVertical();
			break;


		default:
			LogDebug("button %d\n", event->button);
	}

	return true;
}

/**
	@brief Channel list and/or visibility states have changed, refresh

	TODO: fix protocol decoder support here
 */
void OscilloscopeView::Refresh()
{
	//Deselect whatever channel is currently active
	m_selectedChannel = NULL;

	//Delete old renderers
	for(ChannelMap::iterator it=m_renderers.begin(); it != m_renderers.end(); ++it)
		delete it->second;
	m_renderers.clear();

	//Setup for renderer creation
	int y = 0;
	int spacing = 5;
	size_t count = m_scope->GetChannelCount();

	//Create timescale renderer
	LogTrace("Refreshing oscilloscope view\n");
	LogIndenter li;
	if(m_scope->GetChannelCount() != 0)
	{
		m_timescaleRender = new TimescaleRenderer(m_scope->GetChannel(0));
		m_timescaleRender->m_ypos = y;
		y += m_timescaleRender->m_height + spacing;
		LogTrace("%30s: y = %d - %d\n", "timescale",
			m_timescaleRender->m_ypos, m_timescaleRender->m_ypos + m_timescaleRender->m_height);
	}

	//Create renderers for each channel
	for(size_t i=0; i<count; i++)
	{
		//Skip invisible channels
		OscilloscopeChannel* chan = m_scope->GetChannel(i);
		if(!chan->m_visible)
			continue;

		ChannelRenderer* pRender = m_scope->GetChannel(i)->CreateRenderer();
		pRender->m_ypos = y;
		y += pRender->m_height + spacing;
		m_renderers[chan] = pRender;

		LogTrace("%30s: y = %d - %d\n", chan->m_displayname.c_str(), pRender->m_ypos, pRender->m_ypos + pRender->m_height);
	}

	SetSizeDirty();
}

void OscilloscopeView::Resize()
{
	m_width = 1;
	m_height = 1;

	for(ChannelMap::iterator it=m_renderers.begin(); it != m_renderers.end(); ++it)
	{
		ChannelRenderer* pRender = it->second;
		if(pRender == NULL)
			continue;

		//Height
		int bottom = pRender->m_ypos + pRender->m_height;
		if(bottom > m_height)
			m_height = bottom;

		//Width
		if(pRender->m_width > m_width)
			m_width = pRender->m_width;
	}

	set_size(m_width, m_height);
}

void OscilloscopeView::MakeTimeRanges(vector<time_range>& ranges)
{
	ranges.clear();
	if(m_scope->GetChannelCount() == 0)
		return;

	//Use the lowest numbered channel with data in it
	OscilloscopeChannel* chan = NULL;
	CaptureChannelBase* capture = NULL;
	for(size_t i=0; i<m_scope->GetChannelCount(); i++)
	{
		chan = m_scope->GetChannel(i);
		capture = chan->GetData();
		if(capture != NULL)
			break;
	}
	if(capture == NULL)
		return;
	if(m_renderers.empty())
		return;

	bool analog = (dynamic_cast<AnalogCapture*>(capture) != NULL);

	double startpos = 0;
	time_range current_range;
	current_range.xstart = 0;
	current_range.tstart = 0;
	double tscale = chan->m_timescale * capture->m_timescale;
	for(size_t i=0; i<capture->GetDepth(); i++)
	{
		//If it would show up as more than m_maxsamplewidth pixels wide, clip it.
		//If the capture is analog, any gaps are segment boundaries. If digital, use width heuristic
		int64_t len = capture->GetSampleLen(i);
		double sample_width = tscale * len;
		double msw = m_renderers.begin()->second->m_maxsamplewidth;
		if(	( analog && (len > 1) ) || (!analog && (sample_width > 500) ) )
		{
			sample_width = msw;
			double xmid = startpos + sample_width/2;

			int64_t dt = (sample_width/2)/tscale;

			//End the current range
			current_range.xend = xmid;
			current_range.tend = capture->GetSampleStart(i) + dt;
			ranges.push_back(current_range);

			//Start a new range
			current_range.xstart = xmid;
			current_range.tstart = (capture->GetSampleStart(i) + capture->GetSampleLen(i))	//end of sample
									- dt;
		}

		//Go on to the next sample
		startpos += sample_width;

		//End of capture? Push it back
		if(i == capture->GetDepth()-1)
		{
			current_range.tend = capture->GetSampleStart(i) + (sample_width/2)/tscale;
			current_range.xend = startpos+sample_width;
			ranges.push_back(current_range);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// View event handlers

bool OscilloscopeView::on_scroll_event (GdkEventScroll* ev)
{
	//Y scroll: time/div
	if(ev->delta_y != 0)
	{
		if(ev->delta_y < 0)
			m_parent->OnZoomIn();
		else
			m_parent->OnZoomOut();
	}

	//X scroll: vertical offset
	else if(ev->delta_x != 0)
	{
		if(ev->delta_x < 0)
			OnOffsetDown();
		else
			OnOffsetUp();
	}
	return true;
}

bool OscilloscopeView::IsAnalogChannelSelected()
{
	//No point if we don't have data
	if(m_selectedChannel == NULL)
		return false;
	auto data = m_selectedChannel->GetData();
	if(data == NULL)
		return false;

	//We have data - make sure it's analog
	auto adata = dynamic_cast<AnalogCapture*>(data);
	if(adata == NULL)
		return false;

	return true;
}

void OscilloscopeView::OnOffsetUp()
{
	//Update the render offset
	if(!IsAnalogChannelSelected())
		return;
	auto render = dynamic_cast<AnalogRenderer*>(m_renderers[m_selectedChannel]);
	if(!render)
		return;

	render->m_yoffset += render->m_yscale * 0.1;
	queue_draw();
}

void OscilloscopeView::OnOffsetDown()
{
	//Update the render offset
	if(!IsAnalogChannelSelected())
		return;
	auto render = dynamic_cast<AnalogRenderer*>(m_renderers[m_selectedChannel]);
	if(!render)
		return;

	render->m_yoffset -= render->m_yscale * 0.1;
	queue_draw();
}

void OscilloscopeView::OnZoomInVertical()
{
	//Update the render scale
	if(!IsAnalogChannelSelected())
		return;
	auto render = dynamic_cast<AnalogRenderer*>(m_renderers[m_selectedChannel]);
	if(!render)
		return;

	render->m_yscale *= 1.1f;
	queue_draw();
}

void OscilloscopeView::OnZoomOutVertical()
{
	//Update the render scale
	if(!IsAnalogChannelSelected())
		return;
	auto render = dynamic_cast<AnalogRenderer*>(m_renderers[m_selectedChannel]);
	if(!render)
		return;

	render->m_yscale /= 1.1f;
	queue_draw();
}

void OscilloscopeView::OnAutoFitVertical()
{
	//Sanity check that it's actually analog
	if(!IsAnalogChannelSelected())
		return;

	//Should be an analog renderer, we're very confused otherwise
	auto render = dynamic_cast<AnalogRenderer*>(m_renderers[m_selectedChannel]);
	if(!render)
		return;

	//Find the min/max values of the samples
	auto adata = dynamic_cast<AnalogCapture*>(m_selectedChannel->GetData());
	float min = 999;
	float max = -999;
	for(auto sample : *adata)
	{
		if((float)sample > max)
			max = sample;
		if((float)sample < min)
			min = sample;
	}
	float range = max - min;

	//Calculate the display scale to make it fit the available space in the renderer
	//Renderer uses normalized units of +/- 0.5, we just need a scaling factor
	render->m_yscale = 1.0f / range;

	//Calculate the offset to center our waveform in the display area
	float midpoint = range/2 + min;
	render->m_yoffset = -midpoint;

	//Done, refresh display
	queue_draw();
}

void OscilloscopeView::OnProtocolDecode(string protocol)
{
	try
	{
		//Decoding w/o a channel selected (and full of data) is nonsensical
		if(m_selectedChannel == NULL)
			return;
		auto data = m_selectedChannel->GetData();
		if(data == NULL)
			return;

		//Create the decoder
		LogDebug("Decoding current channel as %s\n", protocol.c_str());
		auto decoder = ProtocolDecoder::CreateDecoder(
			protocol,
			m_selectedChannel->GetHwname() + "/" + protocol,
			GetDefaultChannelColor(m_scope->GetChannelCount() + 1)
			);

		//Single input? Hook it up
		if(decoder->GetInputCount() == 1)
		{
			if(decoder->ValidateChannel(0, m_selectedChannel))
				decoder->SetInput(0, m_selectedChannel);
			else
			{
				LogError("Input is not valid for this decoder\n");
				delete decoder;
				return;
			}
		}

		//FIXME: If we have two inputs, use the current and next channel
		//This is temporary until we get a UI for this!
		if(decoder->GetInputCount() == 2)
		{
			if(decoder->ValidateChannel(0, m_selectedChannel))
				decoder->SetInput(0, m_selectedChannel);
			else
			{
				LogError("Input 0 is not valid for this decoder\n");
				delete decoder;
				return;
			}

			//Find the adjacent channel
			int ichan = -1;
			for(int i=0; i<(int)m_scope->GetChannelCount() - 2; i++)
			{
				if(m_selectedChannel == m_scope->GetChannel(i))
				{
					ichan = i;
					break;
				}
			}
			if(ichan < 0)
			{
				LogError("Couldn't find adjacent channel\n");
				delete decoder;
				return;
			}
			OscilloscopeChannel* next = m_scope->GetChannel(ichan + 2);
			if(decoder->ValidateChannel(1, next))
				decoder->SetInput(1, next);
			else
			{
				LogError("Input 1 is not valid for this decoder\n");
				delete decoder;
				return;
			}
		}

		//TODO: dialog for configuring stuff
		if(decoder->NeedsConfig())
		{
		}

		//Add the channel only after we've configured it successfully
		m_scope->AddChannel(decoder);

		//Create a renderer for it
		auto render = decoder->CreateRenderer();
		m_renderers[decoder] = render;

		//Configure the renderer
		//If we're an overlay, draw us on top of the original channel.
		auto original_render = m_renderers[m_selectedChannel];
		if(decoder->IsOverlay())
		{
			render->m_ypos = original_render->m_ypos;
			render->m_overlay = true;

			//If the original renderer is also an overlay, we're doing a second-level decode!
			//Move us down below them.
			//TODO: push other decoders as needed?
			if(original_render->m_overlay)
				render->m_ypos += original_render->m_height;
		}

		//NOT an overlay.
		//Insert us right after the original channel.
		else
		{
			int spacing = 5;			//TODO: this should be a member variable and not redeclared everywhere
			render->m_overlay = false;
			render->m_ypos = original_render->m_ypos + original_render->m_height + spacing;

			//Loop over all renderers and push the ones below us as needed
			for(auto it : m_renderers)
			{
				auto r = it.second;
				if(r == render)
					continue;

				if(r->m_ypos >= (render->m_ypos - 10) )	//allow for padding
					r->m_ypos += render->m_height;
			}
		}

		//Done, update things
		decoder->Refresh();
		queue_draw();
	}
	catch(const JtagException& e)
	{
		LogError(e.GetDescription().c_str());
	}
}
