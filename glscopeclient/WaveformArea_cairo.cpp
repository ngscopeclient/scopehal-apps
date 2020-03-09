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
	@brief  Cairo rendering code for WaveformArea
 */

#include "glscopeclient.h"
#include "WaveformArea.h"
#include "OscilloscopeWindow.h"
#include <random>
#include <map>
#include "ProfileBlock.h"
#include "../../lib/scopehal/TextRenderer.h"
#include "../../lib/scopehal/DigitalRenderer.h"
#include "../../lib/scopeprotocols/EyeDecoder2.h"
#include "../../lib/scopeprotocols/WaterfallDecoder.h"

using namespace std;
using namespace glm;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cairo rendering

void WaveformArea::DoRenderCairoUnderlays(Cairo::RefPtr< Cairo::Context > cr)
{
	RenderBackgroundGradient(cr);
	RenderGrid(cr);
}

void WaveformArea::RenderBackgroundGradient(Cairo::RefPtr< Cairo::Context > cr)
{
	//Draw the background gradient
	float ytop = m_padding;
	float ybot = m_height - 2*m_padding;
	float top_brightness = 0.1;
	float bottom_brightness = 0.0;

	Gdk::Color color(m_channel->m_displaycolor);

	Cairo::RefPtr<Cairo::LinearGradient> background_gradient = Cairo::LinearGradient::create(0, ytop, 0, ybot);
	background_gradient->add_color_stop_rgb(
		0,
		color.get_red_p() * top_brightness,
		color.get_green_p() * top_brightness,
		color.get_blue_p() * top_brightness);
	background_gradient->add_color_stop_rgb(
		1,
		color.get_red_p() * bottom_brightness,
		color.get_green_p() * bottom_brightness,
		color.get_blue_p() * bottom_brightness);
	cr->set_source(background_gradient);
	cr->rectangle(0, 0, m_plotRight, m_height);
	cr->fill();
}

void WaveformArea::RenderGrid(Cairo::RefPtr< Cairo::Context > cr)
{
	//If we're a digital channel, no grid or anything else makes sense
	if(m_channel->GetType() == OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
		return;

	//Calculate width of right side axis label
	int twidth;
	int theight;
	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create (cr);
	Pango::FontDescription font("monospace normal 10");
	font.set_weight(Pango::WEIGHT_NORMAL);
	tlayout->set_font_description(font);
	tlayout->set_text("500 mV_xxx");
	tlayout->get_pixel_size(twidth, theight);
	m_plotRight = m_width - twidth;

	if(IsWaterfall())
		return;

	cr->save();

	Gdk::Color color(m_channel->m_displaycolor);

	float ytop = m_height - m_padding;
	float ybot = m_padding;
	float plotheight = m_height - 2*m_padding;
	float halfheight = plotheight/2;
	//float ymid = halfheight + ybot;

	std::map<float, float> gridmap;

	//Spectra are printed on a logarithmic scale
	if(IsFFT())
	{
		for(float db=0; db >= -60; db -= 10)
			gridmap[db] = DbToYPosition(db);
	}

	//Normal analog waveform
	else
	{
		//Volts from the center line of our graph to the top. May not be the max value in the signal.
		float volts_per_half_span = PixelsToVolts(halfheight);

		//Decide what voltage step to use. Pick from a list (in volts)
		float selected_step = AnalogRenderer::PickStepSize(volts_per_half_span);

		//Calculate grid positions
		for(float dv=0; ; dv += selected_step)
		{
			float yt = VoltsToYPosition(dv);
			float yb = VoltsToYPosition(-dv);

			if(dv != 0)
			{
				if(yb <= (ytop - theight/2) )
					gridmap[-dv] = yb;
				if(yt >= (ybot + theight/2) )
					gridmap[dv] = yt;
			}
			else
				gridmap[dv] = yt;

			//Stop if we're off the edge
			if( (yb > ytop) && (yt < ybot) )
				break;
		}

		//Center line is solid
		cr->set_source_rgba(0.7, 0.7, 0.7, 1.0);
		cr->move_to(0, VoltsToYPosition(0));
		cr->line_to(m_plotRight, VoltsToYPosition(0));
		cr->stroke();
	}

	//Dimmed lines above and below
	cr->set_source_rgba(0.7, 0.7, 0.7, 0.25);
	for(auto it : gridmap)
	{
		if(it.first == 0)	//don't over-draw the center line
			continue;
		cr->move_to(0, it.second);
		cr->line_to(m_plotRight, it.second);
	}
	cr->stroke();
	cr->unset_dash();

	//Draw background for the Y axis labels
	cr->set_source_rgba(0, 0, 0, 0.5);
	cr->rectangle(m_plotRight, 0, twidth, plotheight);
	cr->fill();

	//Draw text for the Y axis labels
	cr->set_source_rgba(1.0, 1.0, 1.0, 1.0);
	for(auto it : gridmap)
	{
		float v = it.first;

		if(IsFFT())
		{
			char tmp[32];
			snprintf(tmp, sizeof(tmp), "%.0f dB", v);
			tlayout->set_text(tmp);
		}
		else
			tlayout->set_text(m_channel->GetYAxisUnits().PrettyPrint(v));

		float y = it.second;
		if(!IsFFT())
			y -= theight/2;
		if(y < ybot)
			continue;
		if(y > ytop)
			continue;

		tlayout->get_pixel_size(twidth, theight);
		cr->move_to(m_width - twidth - 5, y);
		tlayout->update_from_cairo_context(cr);
		tlayout->show_in_cairo_context(cr);
	}
	cr->begin_new_path();

	//See if we're the active trigger
	if( (m_scope != NULL) && (m_channel->GetIndex() == m_scope->GetTriggerChannelIndex()) )
	{
		float v = m_scope->GetTriggerVoltage();
		float y = VoltsToYPosition(v);

		float trisize = 5;

		if(m_dragState == DRAG_TRIGGER)
		{
			cr->set_source_rgba(1, 0, 0, 1);
			y = m_cursorY;
		}
		else
		{
			cr->set_source_rgba(
				color.get_red_p(),
				color.get_green_p(),
				color.get_blue_p(),
				1);
		}
		cr->move_to(m_plotRight, y);
		cr->line_to(m_plotRight + trisize, y + trisize);
		cr->line_to(m_plotRight + trisize, y - trisize);
		cr->fill();
	}

	cr->restore();
}

void WaveformArea::DoRenderCairoOverlays(Cairo::RefPtr< Cairo::Context > cr)
{
	RenderDecodeOverlays(cr);
	RenderChannelLabel(cr);
	RenderCursors(cr);
}

void WaveformArea::RenderDecodeOverlays(Cairo::RefPtr< Cairo::Context > cr)
{
	//TODO: adjust height/spacing depending on font sizes etc
	int height = 20;
	int spacing = 30;
	int midline = spacing / 2;

	//Render digital bus waveforms in the main channel here (TODO: GL stuff)
	auto bus = dynamic_cast<DigitalBusCapture*>(m_channel->GetData());
	if(bus != NULL)
	{
		int ymid = m_height - 15;
		int ytop = ymid - 8;
		int ybot = ymid + 8;

		Gdk::Color color(m_channel->m_displaycolor);

		for(size_t i=0; i<bus->GetDepth(); i++)
		{
			double start = (bus->GetSampleStart(i) * bus->m_timescale) + bus->m_triggerPhase;
			double end = start + (bus->GetSampleLen(i) * bus->m_timescale);

			//Merge with subsequent samples if they have the same value
			for(size_t j=i+1; j<bus->GetDepth(); j++)
			{
				if(bus->m_samples[i].m_sample != bus->m_samples[j].m_sample)
					break;

				//Merge this sample with the next one
				i++;
				end = (bus->GetSampleStart(i) + bus->GetSampleLen(i)) * bus->m_timescale + bus->m_triggerPhase;
			}

			double xs = XAxisUnitsToXPosition(start);
			double xe = XAxisUnitsToXPosition(end);

			if( (xe < m_infoBoxRect.get_right()) || (xs > m_plotRight) )
				continue;

			auto sample = bus->m_samples[i].m_sample;

			uint64_t value = 0;
			for(size_t j=0; j<sample.size(); j++)
			{
				if(sample[j])
					value |= (1LU << j);
			}

			char tmp[128];
			if(sample.size() <= 4)
				snprintf(tmp, sizeof(tmp), "%01lx", value);
			else if(sample.size() <= 8)
				snprintf(tmp, sizeof(tmp), "%02lx", value);
			else if(sample.size() <= 12)
				snprintf(tmp, sizeof(tmp), "%03lx", value);
			else if(sample.size() <= 16)
				snprintf(tmp, sizeof(tmp), "%04lx", value);
			else if(sample.size() <= 20)
				snprintf(tmp, sizeof(tmp), "%05lx", value);
			else if(sample.size() <= 24)
				snprintf(tmp, sizeof(tmp), "%06lx", value);
			else if(sample.size() <= 28)
				snprintf(tmp, sizeof(tmp), "%07lx", value);
			else if(sample.size() <= 32)
				snprintf(tmp, sizeof(tmp), "%08lx", value);
			else
				snprintf(tmp, sizeof(tmp), "%lx", value);

			ChannelRenderer::RenderComplexSignal(
				cr,
				m_infoBoxRect.get_right(), m_plotRight,
				xs, xe, 5,
				ybot, ymid, ytop,
				tmp,
				color);
		}
	}

	//Find which overlay slots are in use
	const int max_overlays = 10;
	bool overlayPositionsUsed[max_overlays] = {0};
	for(auto o : m_overlays)
	{
		if(m_overlayPositions.find(o) == m_overlayPositions.end())
			continue;

		int pos = m_overlayPositions[o];
		int index = (pos - midline) / spacing;
		if( (pos >= 0) && (index < max_overlays) )
			overlayPositionsUsed[index] = true;
	}

	//Assign first unused position to all overlays
	for(auto o : m_overlays)
	{
		if(m_overlayPositions.find(o) == m_overlayPositions.end())
		{
			for(int i=0; i<max_overlays; i++)
			{
				if(!overlayPositionsUsed[i])
				{
					overlayPositionsUsed[i] = true;
					m_overlayPositions[o] = midline + spacing*i;
					break;
				}
			}
		}
	}

	for(auto o : m_overlays)
	{
		auto render = o->CreateRenderer();
		auto data = o->GetData();

		bool digital = dynamic_cast<DigitalRenderer*>(render) != NULL;

		double ymid = m_overlayPositions[o];
		double ytop = ymid - height/2;
		double ybot = ymid + height/2;

		if(!digital)
		{
			//Render the grayed-out background
			cr->set_source_rgba(0,0,0, 0.6);
			cr->move_to(0, 				ytop);
			cr->line_to(m_plotRight, 	ytop);
			cr->line_to(m_plotRight,	ybot);
			cr->line_to(0,				ybot);
			cr->fill();
		}

		Rect chanbox;
		RenderChannelInfoBox(o, cr, ybot, o->m_displayname, chanbox, 2);
		m_overlayBoxRects[o] = chanbox;

		int textright = chanbox.get_right() + 4;

		if(data == NULL)
			continue;

		//Handle text
		auto tr = dynamic_cast<TextRenderer*>(render);
		if(tr != NULL)
		{
			for(size_t i=0; i<data->GetDepth(); i++)
			{
				double start = (data->GetSampleStart(i) * data->m_timescale) + data->m_triggerPhase;
				double end = start + (data->GetSampleLen(i) * data->m_timescale);

				double xs = XAxisUnitsToXPosition(start);
				double xe = XAxisUnitsToXPosition(end);

				if( (xe < textright) || (xs > m_plotRight) )
					continue;

				auto text = tr->GetText(i);
				auto color = tr->GetColor(i);

				render->RenderComplexSignal(
					cr,
					textright, m_plotRight,
					xs, xe, 5,
					ybot, ymid, ytop,
					text,
					color);
			}
		}

		delete render;
	}
}

void WaveformArea::RenderChannelLabel(Cairo::RefPtr< Cairo::Context > cr)
{
	//Add sample rate info to physical analog channels
	//TODO: do this to some decodes too?
	string label = m_channel->m_displayname;
	auto data = m_channel->GetData();
	if(m_channel->IsPhysicalChannel() && (data != NULL))
	{
		//Do not render sample rate on digital signals unless we have overlays, because this ~doubles the height
		//of the channel and hurts packing density.
		if( (m_channel->GetType() == OscilloscopeChannel::CHANNEL_TYPE_DIGITAL) && m_overlays.empty() )
		{}

		else
		{
			label += " : ";

			//Format sample depth
			char tmp[256];
			size_t len = data->GetDepth();
			if(len > 1e6)
				snprintf(tmp, sizeof(tmp), "%.0f MS", len * 1e-6f);
			else if(len > 1e3)
				snprintf(tmp, sizeof(tmp), "%.0f kS", len * 1e-3f);
			else
				snprintf(tmp, sizeof(tmp), "%zu S", len);
			label += tmp;
			label += "\n";

			//Format timebase
			double gsps = 1000.0f / data->m_timescale;
			if(gsps > 1)
				snprintf(tmp, sizeof(tmp), "%.0f GS/s", gsps);
			else if(gsps > 0.001)
				snprintf(tmp, sizeof(tmp), "%.0f MS/s", gsps * 1000);
			else
				snprintf(tmp, sizeof(tmp), "%.1f kS/s", gsps * 1000 * 1000);
			label += tmp;
		}
	}

	//Do the actual drawing
	RenderChannelInfoBox(m_channel, cr, m_height, label, m_infoBoxRect);
}

void WaveformArea::RenderChannelInfoBox(
		OscilloscopeChannel* chan,
		Cairo::RefPtr< Cairo::Context > cr,
		int bottom,
		string text,
		Rect& box,
		int labelmargin)
{
	//Figure out text size
	int twidth;
	int theight;
	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create (cr);
	Pango::FontDescription font("sans normal 10");
	font.set_weight(Pango::WEIGHT_NORMAL);
	tlayout->set_font_description(font);
	tlayout->set_text(text);
	tlayout->get_pixel_size(twidth, theight);

	//Channel-colored rounded outline
	cr->save();

		int labelheight = theight + labelmargin*2;

		box.set_x(2);
		box.set_y(bottom - labelheight - 1);
		box.set_width(twidth + labelmargin*2);
		box.set_height(labelheight);

		Rect innerBox = box;
		innerBox.shrink(labelmargin, labelmargin);

		//Path for the outline
		cr->begin_new_sub_path();
		cr->arc(innerBox.get_left(), innerBox.get_bottom(), labelmargin, M_PI_2, M_PI);		//bottom left
		cr->line_to(box.get_left(), innerBox.get_y());
		cr->arc(innerBox.get_left(), innerBox.get_top(), labelmargin, M_PI, 1.5*M_PI);		//top left
		cr->line_to(innerBox.get_right(), box.get_top());
		cr->arc(innerBox.get_right(), innerBox.get_top(), labelmargin, 1.5*M_PI, 2*M_PI);	//top right
		cr->line_to(box.get_right(), innerBox.get_bottom());
		cr->arc(innerBox.get_right(), innerBox.get_bottom(), labelmargin, 2*M_PI, M_PI_2);	//bottom right
		cr->line_to(innerBox.get_left(), box.get_bottom());

		//Fill it
		cr->set_source_rgba(0, 0, 0, 0.75);
		cr->fill_preserve();

		//Draw the outline
		Gdk::Color color(chan->m_displaycolor);
		cr->set_source_rgba(color.get_red_p(), color.get_green_p(), color.get_blue_p(), 1);
		cr->set_line_width(1);
		cr->stroke();

	cr->restore();

	//White text
	cr->save();
		cr->set_source_rgba(1, 1, 1, 1);
		cr->move_to(labelmargin, bottom - theight - labelmargin);
		tlayout->update_from_cairo_context(cr);
		tlayout->show_in_cairo_context(cr);
	cr->restore();
}

void WaveformArea::RenderCursors(Cairo::RefPtr< Cairo::Context > cr)
{
	int ytop = m_height;
	int ybot = 0;

	Gdk::Color yellow("yellow");
	Gdk::Color orange("orange");

	if( (m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL) ||
		(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_SINGLE) )
	{
		//Draw first vertical cursor
		double x = XAxisUnitsToXPosition(m_group->m_xCursorPos[0]);
		cr->move_to(x, ytop);
		cr->line_to(x, ybot);
		cr->set_source_rgb(yellow.get_red_p(), yellow.get_green_p(), yellow.get_blue_p());
		cr->stroke();

		//Dual cursors
		if(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL)
		{
			//Draw second vertical cursor
			double x2 = XAxisUnitsToXPosition(m_group->m_xCursorPos[1]);
			cr->move_to(x2, ytop);
			cr->line_to(x2, ybot);
			cr->set_source_rgb(orange.get_red_p(), orange.get_green_p(), orange.get_blue_p());
			cr->stroke();

			//Draw filled area between them
			cr->set_source_rgba(yellow.get_red_p(), yellow.get_green_p(), yellow.get_blue_p(), 0.2);
			cr->move_to(x, ytop);
			cr->line_to(x2, ytop);
			cr->line_to(x2, ybot);
			cr->line_to(x, ybot);
			cr->fill();
		}
	}
}
