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
	tlayout->set_text("500.000 mV_xx");
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
	if(m_channel->GetYAxisUnits() == Unit::UNIT_DB)
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
		float selected_step = PickStepSize(volts_per_half_span);

		//Special case a few values
		if(m_channel->GetYAxisUnits() == Unit::UNIT_LOG_BER)
			selected_step = 2;

		float bottom_edge = (ybot + theight/2);
		float top_edge = (ytop - theight/2);

		//Calculate grid positions
		float vbot = YPositionToVolts(ybot);
		float vtop = YPositionToVolts(ytop);
		float vmid = (vbot + vtop)/2;
		for(float dv=0; ; dv += selected_step)
		{
			float vp = vmid + dv;
			float vn = vmid - dv;

			float yt = VoltsToYPosition(vp);
			float yb = VoltsToYPosition(vn);

			if(dv != 0)
			{
				if( (yb >= bottom_edge) && (yb <= top_edge ) )
					gridmap[vn] = yb;

				if( (yt >= bottom_edge ) && (yt <= top_edge) )
					gridmap[vp] = yt;
			}
			else
				gridmap[vp] = yt;

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

	if(gridmap.size() > 50)
		LogFatal("gridmap way too big (%zu)\n", gridmap.size());

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
	if(m_channel->IsPhysicalChannel())
	{
		auto scope = m_channel->GetScope();
		if(m_channel->GetIndex() == scope->GetTriggerChannelIndex())
		{
			float v = m_channel->GetScope()->GetTriggerVoltage();
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
	int midline = m_overlaySpacing / 2;

	//Render digital bus waveforms in the main channel here (TODO: GL stuff)
	auto bus = dynamic_cast<DigitalBusWaveform*>(m_channel->GetData());
	if(bus != NULL)
	{
		int ymid = m_height - 15;
		int ytop = ymid - 8;
		int ybot = ymid + 8;

		Gdk::Color color(m_channel->m_displaycolor);

		size_t len = bus->m_offsets.size();
		for(size_t i=0; i<len; i++)
		{
			double start = (bus->m_offsets[i] * bus->m_timescale) + bus->m_triggerPhase;
			double end = start + (bus->m_durations[i] * bus->m_timescale);

			//Merge with subsequent samples if they have the same value
			for(size_t j=i+1; j<len; j++)
			{
				if(bus->m_samples[i] != bus->m_samples[j])
					break;

				//Merge this sample with the next one
				i++;
				end = (bus->m_offsets[i] + bus->m_durations[i]) * bus->m_timescale + bus->m_triggerPhase;
			}

			double xs = XAxisUnitsToXPosition(start);
			double xe = XAxisUnitsToXPosition(end);

			if( (xe < m_infoBoxRect.get_right()) || (xs > m_plotRight) )
				continue;

			auto sample = bus->m_samples[i];

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

			RenderComplexSignal(
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
		int index = (pos - midline) / m_overlaySpacing;
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
					m_overlayPositions[o] = midline + m_overlaySpacing*i;
					break;
				}
			}
		}
	}

	for(auto o : m_overlays)
	{
		auto data = o->GetData();

		double ymid = m_overlayPositions[o];
		double ytop = ymid - height/2;
		double ybot = ymid + height/2;

		if(o->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
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
		if(o->GetType() == OscilloscopeChannel::CHANNEL_TYPE_COMPLEX)
		{
			size_t olen = data->m_offsets.size();
			for(size_t i=0; i<olen; i++)
			{
				double start = (data->m_offsets[i] * data->m_timescale) + data->m_triggerPhase;
				double end = start + (data->m_durations[i] * data->m_timescale);

				double xs = XAxisUnitsToXPosition(start);
				double xe = XAxisUnitsToXPosition(end);

				if( (xe < textright) || (xs > m_plotRight) )
					continue;

				RenderComplexSignal(
					cr,
					textright, m_plotRight,
					xs, xe, 5,
					ybot, ymid, ytop,
					o->GetText(i),
					o->GetColor(i));
			}
		}
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
			size_t len = data->m_offsets.size();
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
			{
				//If sample rate isn't a round Gsps number, add more digits
				if(floor(gsps) != gsps)
					snprintf(tmp, sizeof(tmp), "%.2f GS/s", gsps);
				else
					snprintf(tmp, sizeof(tmp), "%.0f GS/s", gsps);
			}
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
	int ytop = 0;
	int ybot = m_height;

	Gdk::Color yellow("yellow");
	Gdk::Color orange("orange");
	Gdk::Color red("red");

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

	int barsize = 5;
	switch(m_dragState)
	{
		//Render the insertion bar, if needed
		case DRAG_WAVEFORM_AREA:
			if(m_insertionBarLocation != INSERT_NONE)
			{
				int barpos = 0;
				float alpha = 0.75;
				bool barhorz = true;
				switch(m_insertionBarLocation)
				{
					case INSERT_BOTTOM:
						cr->set_source_rgba(yellow.get_red_p(), yellow.get_green_p(), yellow.get_blue_p(), alpha);
						barpos = ybot - barsize;
						break;

					case INSERT_BOTTOM_SPLIT:
						cr->set_source_rgba(orange.get_red_p(), orange.get_green_p(), orange.get_blue_p(), alpha);
						barpos = ybot - barsize;
						break;

					case INSERT_TOP:
						cr->set_source_rgba(yellow.get_red_p(), yellow.get_green_p(), yellow.get_blue_p(), alpha);
						barpos = 0;
						break;

					case INSERT_RIGHT_SPLIT:
						cr->set_source_rgba(orange.get_red_p(), orange.get_green_p(), orange.get_blue_p(), alpha);
						barhorz = false;
						barpos = m_plotRight - barsize;
						break;

					//no bar to draw
					default:
						break;
				}

				if(barhorz)
				{
					cr->move_to(0, barpos);
					cr->line_to(m_width, barpos);
					cr->line_to(m_width, barpos + barsize);
					cr->line_to(0, barpos + barsize);
				}
				else
				{
					cr->move_to(barpos,				0);
					cr->line_to(barpos + barsize,	0);
					cr->line_to(barpos + barsize,	m_height);
					cr->line_to(barpos,				m_height);
				}
				cr->fill();
			}
			break;

		case DRAG_OVERLAY:
			cr->set_source_rgba(yellow.get_red_p(), yellow.get_green_p(), yellow.get_blue_p(), 0.75);
			cr->move_to(0, 				m_dragOverlayPosition);
			cr->line_to(m_plotRight,	m_dragOverlayPosition);
			cr->line_to(m_plotRight,	m_dragOverlayPosition + barsize);
			cr->line_to(0,				m_dragOverlayPosition + barsize);
			cr->fill();

		default:
			break;
	}
}


void WaveformArea::MakePathSignalBody(
	const Cairo::RefPtr<Cairo::Context>& cr,
	float xstart, float /*xoff*/, float xend, float ybot, float /*ymid*/, float ytop)
{
	//If the signal is really tiny, shrink the rounding to avoid going out of bounds
	float rounding = 10;
	if(xstart + 2*rounding > xend)
		rounding = (xend - xstart) / 2;

	cr->begin_new_sub_path();
	cr->arc(xstart + rounding, ytop + rounding, rounding, M_PI, M_PI*1.5f);	//top left corner
	cr->move_to(xstart + rounding, ytop);									//top edge
	cr->line_to(xend - rounding, ytop);
	cr->arc(xend - rounding, ytop + rounding, rounding, M_PI*1.5f, 0);		//top right corner
	cr->move_to(xend, ytop + rounding);										//right edge
	cr->line_to(xend, ybot - rounding);
	cr->arc(xend - rounding, ybot - rounding, rounding, 0, M_PI_2);			//bottom right corner
	cr->move_to(xend - rounding, ybot);										//bottom edge
	cr->line_to(xstart + rounding, ybot);
	cr->arc(xstart + rounding, ybot - rounding, rounding, M_PI_2, M_PI);	//bottom left corner
	cr->move_to(xstart, ybot - rounding);									//left edge
	cr->line_to(xstart, ytop + rounding);
}

void WaveformArea::RenderComplexSignal(
		const Cairo::RefPtr<Cairo::Context>& cr,
		int visleft, int visright,
		float xstart, float xend, float xoff,
		float ybot, float ymid, float ytop,
		string str,
		Gdk::Color color)
{
	Pango::FontDescription font("sans normal 10");
	int width = 0, sheight = 0;
	GetStringWidth(cr, str, width, sheight, font);

	//First-order guess of position: center of the value
	float xp = xstart + (xend-xstart)/2;

	//Width within this signal outline
	float available_width = xend - xstart - 2*xoff;

	//Minimum width (if outline ends up being smaller than this, just fill)
	float min_width = 40;
	if(width < min_width)
		min_width = width;

	//Does the string fit at all? If not, skip all of the messy math
	if(available_width < min_width)
		str = "";
	else
	{
		//Center the text by moving it left half a width
		xp -= width/2;

		//Off the left end? Push it right
		int padding = 5;
		if(xp < (visleft + padding))
		{
			xp = visleft + padding;
			available_width = xend - xp - xoff;
		}

		//Off the right end? Push it left
		else if( (xp + width + padding) > visright)
		{
			xp = visright - (width + padding + xoff);
			if(xp < xstart)
				xp = xstart + xoff;

			if(xend < visright)
				available_width = xend - xp - xoff;
			else
				available_width = visright - xp - xoff;
		}

		//If we don't fit under the new constraints, give up
		if(available_width < min_width)
			str = "";
	}

	//Draw the text
	if(str != "")
	{
		//Text is always white (TODO: only in overlays?)
		cr->set_source_rgb(1, 1, 1);

		//If we need to trim, decide which way to do it.
		//If the text is all caps and includes an underscore, it's probably a macro with a prefix.
		//Trim from the left in this case. Otherwise, trim from the right.
		bool trim_from_right = true;
		bool is_all_upper = true;
		for(size_t i=0; i<str.length(); i++)
		{
			if(islower(str[i]))
				is_all_upper = false;
		}
		if(is_all_upper && (str.find("_") != string::npos))
			trim_from_right = false;

		//Some text fits, but maybe not all of it
		//We know there's enough room for "some" text
		//Try shortening the string a bit at a time until it fits
		//(Need to do an O(n) search since character width is variable and unknown to us without knowing details
		//of the font currently in use)
		string str_render = str;
		if(width > available_width)
		{
			for(int len = str.length() - 1; len > 1; len--)
			{
				if(trim_from_right)
					str_render = str.substr(0, len) + "...";
				else
					str_render = "..." + str.substr(str.length() - len - 1);

				int twidth = 0, theight = 0;
				GetStringWidth(cr, str_render, twidth, theight, font);
				if(twidth < available_width)
				{
					//Re-center text in available space
					//TODO: Move to avoid any time-split lines
					xp += (available_width - twidth)/2;
					if(xp < (xstart + xoff))
						xp = (xstart + xoff);
					break;
				}
			}
		}

		cr->save();
			Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create (cr);
			cr->move_to(xp, ymid - sheight/2);
			font.set_weight(Pango::WEIGHT_NORMAL);
			tlayout->set_font_description(font);
			tlayout->set_text(str_render);
			tlayout->update_from_cairo_context(cr);
			tlayout->show_in_cairo_context(cr);
		cr->restore();
	}

	//If no text fit, draw filler instead
	else
	{
		cr->set_source_rgb(color.get_red_p() * 0.25, color.get_green_p() * 0.25, color.get_blue_p() * 0.25);
		MakePathSignalBody(cr, xstart, xoff, xend, ybot, ymid, ytop);
		cr->fill();
	}

	//Draw the body outline after any filler so it shows up on top
	if(xend > visright)
		xend = visright;
	cr->set_source_rgb(color.get_red_p(), color.get_green_p(), color.get_blue_p());
	MakePathSignalBody(cr, xstart, xoff, xend, ybot, ymid, ytop);
	cr->stroke();
}
