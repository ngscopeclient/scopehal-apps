/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2021 Andrew D. Zonenberg                                                                          *
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

#ifdef _WIN32
#define _USE_MATH_DEFINES
#include <cmath>
#endif

#include "glscopeclient.h"
#include "WaveformArea.h"
#include "OscilloscopeWindow.h"
#include <random>
#include <map>
#include "../scopeprotocols/EyePattern.h"
#include "../scopeprotocols/FFTFilter.h"
#include "../../lib/scopehal/TwoLevelTrigger.h"

using namespace std;

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

	Gdk::Color color(m_channel.m_channel->m_displaycolor);

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
	//Calculate width of right side axis label
	int twidth;
	int theight;
	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create(get_pango_context());
	tlayout->set_font_description(m_axisLabelFont);
	tlayout->set_text("500.000 mV_xx");
	tlayout->get_pixel_size(twidth, theight);
	m_plotRight = m_width - twidth;

	//If we're a digital channel, no grid or anything else makes sense.
	if(m_channel.m_channel->GetType() == OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
		return;

	if(IsWaterfall())
		return;

	cr->save();

	Gdk::Color color(m_channel.m_channel->m_displaycolor);

	float ytop = m_height - m_padding;
	float ybot = m_padding;
	float plotheight = m_height - 2*m_padding;
	float halfheight = plotheight/2;
	//float ymid = halfheight + ybot;

	std::map<float, float> gridmap;

	//Volts from the center line of our graph to the top. May not be the max value in the signal.
	float volts_per_half_span = PixelToYAxisUnits(halfheight);

	//Sanity check invalid values
	if( (volts_per_half_span < -FLT_MAX/2) || (volts_per_half_span > FLT_MAX/2) )
	{
		LogWarning("WaveformArea: invalid grid span (%f)\n", volts_per_half_span);
		return;
	}

	//Decide what voltage step to use. Pick from a list (in volts)
	float selected_step = PickStepSize(volts_per_half_span);

	//Special case a few values
	if(m_channel.GetYAxisUnits() == Unit::UNIT_LOG_BER)
		selected_step = 2;

	float bottom_edge = (ybot + theight/2);
	float top_edge = (ytop - theight/2);

	//Calculate grid positions
	float vbot = YPositionToYAxisUnits(ybot);
	float vtop = YPositionToYAxisUnits(ytop);
	float vmid = (vbot + vtop)/2;
	for(float dv=0; ; dv += selected_step)
	{
		float vp = vmid + dv;
		float vn = vmid - dv;

		float yt = YAxisUnitsToYPosition(vp);
		float yb = YAxisUnitsToYPosition(vn);

		if(dv != 0)
		{
			if( (yb >= bottom_edge) && (yb <= top_edge ) )
				gridmap[vn] = yb;

			if( (yt >= bottom_edge ) && (yt <= top_edge) )
				gridmap[vp] = yt;
		}
		else
			gridmap[vp] = yt;

		if(gridmap.size() > 50)
			break;

		//Stop if we're off the edge
		if( (yb > ytop) && (yt < ybot) )
			break;
	}

	//Center line is solid
	cr->set_source_rgba(0.7, 0.7, 0.7, 1.0);
	cr->move_to(0, YAxisUnitsToYPosition(0));
	cr->line_to(m_plotRight, YAxisUnitsToYPosition(0));
	cr->stroke();

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
		tlayout->set_text(m_channel.GetYAxisUnits().PrettyPrint(v));
		float y = it.second - theight/2;
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

	//Render arrow for trigger
	if(m_channel.m_channel->IsPhysicalChannel())
	{
		auto scope = m_channel.m_channel->GetScope();
		auto trig = scope->GetTrigger();
		if( (trig != NULL) && (trig->GetInput(0) == m_channel) )
		{
			//Main arrow
			RenderTriggerArrow(cr, trig->GetLevel(), (m_dragState == DRAG_TRIGGER), color );

			//Secondary arrow for two-level triggers
			auto wt = dynamic_cast<TwoLevelTrigger*>(trig);
			if(wt)
				RenderTriggerArrow(cr, wt->GetLowerBound(), (m_dragState == DRAG_TRIGGER_SECONDARY), color );
		}
	}

	cr->restore();
}

void WaveformArea::RenderTriggerTimeLine(Cairo::RefPtr< Cairo::Context > cr, int64_t time)
{
	float x = XAxisUnitsToXPosition(time);
	Gdk::Color color = m_parent->GetPreferences().GetColor("Appearance.Windows.trigger_bar_color");

	float scale = GetDPIScale() * get_window()->get_scale_factor();
	vector<double> dots;
	dots.push_back(4 * scale);
	dots.push_back(4 * scale);

	//Dotted line
	cr->set_source_rgba(color.get_red_p(), color.get_green_p(), color.get_blue_p(), 1.0);
	cr->save();
		cr->set_dash(dots, 0);
		cr->move_to(x, 0);
		cr->line_to(x, m_height);
		cr->stroke();
	cr->restore();
}

void WaveformArea::RenderTriggerLevelLine(Cairo::RefPtr< Cairo::Context > cr, float voltage)
{
	float y = YAxisUnitsToYPosition(voltage);
	Gdk::Color color = m_parent->GetPreferences().GetColor("Appearance.Windows.trigger_bar_color");

	int twidth;
	int theight;
	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create(get_pango_context());
	tlayout->set_font_description(m_axisLabelFont);
	tlayout->set_text(m_channel.GetYAxisUnits().PrettyPrint(voltage).c_str());
	tlayout->get_pixel_size(twidth, theight);

	//Label background
	cr->set_source_rgba(0, 0, 0, 0.5);
	cr->move_to(m_plotRight,	y);
	cr->line_to(m_width,		y);
	cr->line_to(m_width,		y+theight+4);
	cr->line_to(m_plotRight,	y+theight+4);
	cr->fill();

	//Label
	cr->set_source_rgba(color.get_red_p(), color.get_green_p(), color.get_blue_p(), 1.0);
	float scale = GetDPIScale() * get_window()->get_scale_factor();
	vector<double> dots;
	dots.push_back(4 * scale);
	dots.push_back(4 * scale);
	float trisize = 5 * scale;
	cr->move_to(m_plotRight + trisize*1.1, y + 2);
	tlayout->update_from_cairo_context(cr);
	tlayout->show_in_cairo_context(cr);

	//Dotted line
	cr->save();
		cr->set_dash(dots, 0);
		cr->move_to(0, y);
		cr->line_to(m_plotRight, y);
		cr->stroke();
	cr->restore();
}

void WaveformArea::RenderTriggerArrow(
	Cairo::RefPtr< Cairo::Context > cr,
	float voltage,
	bool dragging,
	Gdk::Color color)
{
	float y = YAxisUnitsToYPosition(voltage);

	float trisize = 5 * GetDPIScale() * get_window()->get_scale_factor();

	//Dragging? Arrow follows mouse
	if(dragging)
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
		cr->set_line_width(1);
		float x = m_plotRight + trisize*2;

		//If the trigger is outside the displayed area, clip to the edge of the displayed area
		//and display an "out of range" symbol
		float tbottom = m_height - trisize;
		float ttop = trisize;
		if(y > tbottom)
		{
			y = tbottom;

			cr->move_to(x,					y - trisize);
			cr->line_to(x,					y + trisize);
			cr->line_to(x - trisize*0.5,	y);
			cr->move_to(x,					y + trisize);
			cr->line_to(x + trisize*0.5,	y);
			cr->stroke();
		}
		else if(y < ttop)
		{
			y = ttop;

			cr->move_to(x,					y + trisize);
			cr->line_to(x,					y - trisize);
			cr->line_to(x - trisize*0.5,	y);
			cr->move_to(x,					y - trisize);
			cr->line_to(x + trisize*0.5,	y);
			cr->stroke();
		}
	}

	cr->move_to(m_plotRight, y);
	cr->line_to(m_plotRight + trisize, y + trisize);
	cr->line_to(m_plotRight + trisize, y - trisize);
	cr->fill();
}

void WaveformArea::DoRenderCairoOverlays(Cairo::RefPtr< Cairo::Context > cr)
{
	//Eye mask should be under channel infobox and other stuff
	if(IsEye())
		RenderEyeMask(cr);

	RenderDecodeOverlays(cr);
	RenderCursors(cr);

	//Render arrow and dotted line for trigger level
	auto scope = m_channel.m_channel->GetScope();
	if(scope != NULL)
	{
		if(m_dragState == DRAG_TRIGGER)
			RenderTriggerLevelLine(cr, scope->GetTrigger()->GetLevel());
		else if(m_dragState == DRAG_TRIGGER_SECONDARY)
		{
			auto wt = dynamic_cast<TwoLevelTrigger*>(scope->GetTrigger());
			RenderTriggerLevelLine(cr, wt->GetLowerBound());
		}

		//Render dotted line for trigger position
		if(m_group->m_timeline.IsDraggingTrigger())
			RenderTriggerTimeLine(cr, scope->GetTriggerOffset());
	}

	RenderFFTPeaks(cr);
	RenderInsertionBar(cr);
	RenderChannelLabel(cr);
}

void WaveformArea::RenderEyeMask(Cairo::RefPtr< Cairo::Context > cr)
{
	//Make sure it's really an eye
	auto eye = dynamic_cast<EyePattern*>(m_channel.m_channel);
	if(!eye)
		return;
	auto waveform = dynamic_cast<EyeWaveform*>(m_channel.GetData());
	if(!waveform)
		return;

	//If no mask is selected, we have nothing to draw
	auto& mask = eye->GetMask();
	if(mask.GetMaskName() == "")
		return;

	//Clip mask to the plot area
	cr->save();
	cr->move_to(0, 0);
	cr->line_to(m_plotRight, 0);
	cr->line_to(m_plotRight, m_height);
	cr->line_to(0, m_height);
	cr->clip();

	//Do the actual drawing
	mask.RenderForDisplay(
		cr,
		waveform,
		m_group->m_pixelsPerXUnit,
		m_group->m_xAxisOffset,
		m_pixelsPerVolt,
		m_channel.m_channel->GetOffset(),
		m_height);

	cr->restore();
}

void WaveformArea::RenderDecodeOverlays(Cairo::RefPtr< Cairo::Context > cr)
{
	int height = 20 * GetDPIScale();

	//Render digital bus waveforms in the main channel here (TODO: GL stuff)
	auto bus = dynamic_cast<DigitalBusWaveform*>(m_channel.GetData());
	if(bus != NULL)
	{
		int ymid = m_height - 15;
		int ytop = ymid - 8;
		int ybot = ymid + 8;

		Gdk::Color color(m_channel.m_channel->m_displaycolor);

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
			size_t nbits = sample.size();
			string str;

			char tmp[128];
			for(size_t base=0; base < nbits; base += 32)
			{
				size_t blockbits = min(nbits - base, (size_t)32);

				uint32_t value = 0;
				for(size_t j=0; j<blockbits; j++)
				{
					if(sample[j + base])
						value |= (1LU << j);
				}

				if(blockbits <= 4)
					snprintf(tmp, sizeof(tmp), "%01x", value);
				else if(blockbits <= 8)
					snprintf(tmp, sizeof(tmp), "%02x", value);
				else if(blockbits <= 12)
					snprintf(tmp, sizeof(tmp), "%03x", value);
				else if(blockbits <= 16)
					snprintf(tmp, sizeof(tmp), "%04x", value);
				else if(blockbits <= 20)
					snprintf(tmp, sizeof(tmp), "%05x", value);
				else if(blockbits <= 24)
					snprintf(tmp, sizeof(tmp), "%06x", value);
				else if(blockbits <= 28)
					snprintf(tmp, sizeof(tmp), "%07x", value);
				else
					snprintf(tmp, sizeof(tmp), "%08x", value);

				str = string(tmp) + str;
			}

			RenderComplexSignal(
				cr,
				m_infoBoxRect.get_right(), m_plotRight,
				xs, xe, 5,
				ybot, ymid, ytop,
				str,
				color);
		}
	}

	for(auto o : m_overlays)
	{
		auto data = o.GetData();

		double ymid = m_overlayPositions[o];
		double ytop = ymid - height/2;
		double ybot = ymid + height/2;

		if(o.m_channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
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
		RenderChannelInfoBox(o, cr, ybot, o.m_channel->GetDisplayName(), chanbox, 2);
		m_overlayBoxRects[o] = chanbox;

		int textright = chanbox.get_right() + 4;

		if(data == NULL)
			continue;

		//Handle text
		if(o.m_channel->GetType() == OscilloscopeChannel::CHANNEL_TYPE_COMPLEX)
		{
			size_t olen = data->m_offsets.size();

			for(size_t i=0; i<olen; i++)
			{
				double start = (data->m_offsets[i] * data->m_timescale) + data->m_triggerPhase;
				double end = start + (data->m_durations[i] * data->m_timescale);

				double xs = XAxisUnitsToXPosition(start);
				double xe = XAxisUnitsToXPosition(end);

				if(xe < m_infoBoxRect.get_right())
					continue;
				if(xs > m_plotRight)
					break;

				auto f = dynamic_cast<Filter*>(o.m_channel);

				double cellwidth = xe - xs;
				auto color = f->GetColor(i);
				if(cellwidth < 2)
				{
					//This sample is really skinny. There's no text to render so don't waste time with that.

					//Average the color of all samples touching this pixel
					size_t nmerged = 1;
					float sum_red = color.get_red_p();
					float sum_green = color.get_green_p();
					float sum_blue = color.get_blue_p();
					for(size_t j=i+1; j<olen; j++)
					{
						double cellstart = (data->m_offsets[j] * data->m_timescale) + data->m_triggerPhase;
						double cellxs = XAxisUnitsToXPosition(cellstart);

						if(cellxs > xs+2)
							break;

						auto c = f->GetColor(j);
						sum_red += c.get_red_p();
						sum_green += c.get_green_p();
						sum_blue += c.get_blue_p();
						nmerged ++;

						//Skip these samples in the outer loop
						i = j;
					}

					//Render a single box for them all
					color.set_rgb_p(sum_red / nmerged, sum_green / nmerged, sum_blue / nmerged);
					RenderComplexSignal(
						cr,
						textright, m_plotRight,
						xs, xe, 5,
						ybot, ymid, ytop,
						"",
						color);
				}
				else
				{
					RenderComplexSignal(
						cr,
						textright, m_plotRight,
						xs, xe, 5,
						ybot, ymid, ytop,
						f->GetText(i),
						color);
				}
			}
		}
	}
}

void WaveformArea::CalculateOverlayPositions()
{
	int midline = m_overlaySpacing / 2;

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
}

void WaveformArea::RenderChannelLabel(Cairo::RefPtr< Cairo::Context > cr)
{
	string label = m_channel.GetName();
	auto data = m_channel.GetData();
	auto scope = m_channel.m_channel->GetScope();

	auto eye = dynamic_cast<EyeWaveform*>(data);
	auto ed = dynamic_cast<EyePattern*>(m_channel.m_channel);

	//Add sample rate info to physical analog channels
	//and filters with no inputs (signal generators)
	auto f = dynamic_cast<Filter*>(m_channel.m_channel);
	bool printSampleRate = m_channel.m_channel->IsPhysicalChannel();
	if(f && f->GetInputCount() == 0)
		printSampleRate = true;

	//Add RBW to frequency domain channels
	char tmp[256];
	auto xunits = m_channel.m_channel->GetXAxisUnits();
	if( (xunits == Unit::UNIT_HZ) && (data != NULL) )
	{
		double rbw = data->m_timescale;
		if(scope)
			rbw = scope->GetResolutionBandwidth();

		snprintf(tmp, sizeof(tmp), "\nRBW: %s", xunits.PrettyPrint(rbw).c_str());
		label += tmp;
	}

	//Add count info to eye channels
	else if( (eye != NULL) && (ed != NULL) )
	{
		label += string("\n") + Unit(Unit::UNIT_UI).PrettyPrint(eye->GetTotalUIs()) + "\t" +
					Unit(Unit::UNIT_BITRATE).PrettyPrint(FS_PER_SECOND / eye->GetUIWidth());

		auto mask = ed->GetMask();
		auto maskname = mask.GetMaskName();
		if(maskname != "")
		{
			//Mask information
			snprintf(tmp, sizeof(tmp), "\nMask: %-10s", maskname.c_str());
			label += tmp;

			//Hit rate
			auto rate = eye->GetMaskHitRate();
			snprintf(tmp, sizeof(tmp), "\tHit rate: %.2e", rate);
			label += tmp;

			if(rate > mask.GetAllowedHitRate())
				label += "(FAIL)";
			else
				label += "(PASS)";
		}
	}

	//Print sample rate
	else if(printSampleRate && (data != NULL))
	{
		//Do not render sample rate on digital signals unless we have overlays, because this ~doubles the height
		//of the channel and hurts packing density.
		if( (m_channel.m_channel->GetType() == OscilloscopeChannel::CHANNEL_TYPE_DIGITAL) && m_overlays.empty() )
		{}

		else
		{
			//Format sample rate and timebase
			Unit depth(Unit::UNIT_SAMPLEDEPTH);
			Unit rate(Unit::UNIT_SAMPLERATE);
			label +=
				" : " +
				depth.PrettyPrint(data->m_offsets.size()) + "\n" +
				rate.PrettyPrint(FS_PER_SECOND / data->m_timescale);
		}
	}

	//Do the actual drawing
	RenderChannelInfoBox(m_channel, cr, m_height, label, m_infoBoxRect);
}

void WaveformArea::MakePathRoundedRect(
		Cairo::RefPtr< Cairo::Context > cr,
		Rect& box,
		int rounding)
{
	Rect innerBox = box;
	innerBox.shrink(rounding, rounding);

	cr->begin_new_sub_path();
	cr->arc(	innerBox.get_left(), 	innerBox.get_bottom(),	rounding, M_PI_2, M_PI);		//bottom left
	cr->line_to(box.get_left(), 		innerBox.get_y());
	cr->arc(	innerBox.get_left(), 	innerBox.get_top(), 	rounding, M_PI, 1.5*M_PI);		//top left
	cr->line_to(innerBox.get_right(),	box.get_top());
	cr->arc(	innerBox.get_right(),	innerBox.get_top(), 	rounding, 1.5*M_PI, 2*M_PI);	//top right
	cr->line_to(box.get_right(),		innerBox.get_bottom());
	cr->arc(	innerBox.get_right(),	innerBox.get_bottom(),	rounding, 2*M_PI, M_PI_2);		//bottom right
	cr->line_to(innerBox.get_left(),	box.get_bottom());
}

void WaveformArea::RenderChannelInfoBox(
		StreamDescriptor chan,
		Cairo::RefPtr< Cairo::Context > cr,
		int bottom,
		string text,
		Rect& box,
		int labelmargin)
{
	//Set up tab stops for eye labels etc
	Pango::TabArray tabs(1, true);
	tabs.set_tab(0, Pango::TAB_LEFT, 300);

	//Figure out text size
	int twidth;
	int theight;
	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create(get_pango_context());
	tlayout->set_tabs(tabs);
	tlayout->set_font_description(m_infoBoxFont);
	tlayout->set_text(text);
	tlayout->get_pixel_size(twidth, theight);

	int left = 2;

	//Channel-colored rounded outline
	cr->save();

		int labelheight = theight + labelmargin*2;

		box.set_x(left);
		box.set_y(bottom - labelheight - 1);
		box.set_width(twidth + labelmargin*2);
		box.set_height(labelheight);

		//Fill background
		MakePathRoundedRect(cr, box, labelmargin);
		cr->set_source_rgba(0, 0, 0, 0.75);
		cr->fill_preserve();

		//Draw the outline
		Gdk::Color color(chan.m_channel->m_displaycolor);
		cr->set_source_rgba(color.get_red_p(), color.get_green_p(), color.get_blue_p(), 1);
		cr->set_line_width(1);
		cr->stroke();

	cr->restore();

	//White text
	cr->save();
		cr->set_source_rgba(1, 1, 1, 1);
		cr->move_to(labelmargin + left, bottom - theight - labelmargin);
		tlayout->update_from_cairo_context(cr);
		tlayout->show_in_cairo_context(cr);
	cr->restore();
}

void WaveformArea::RenderVerticalCursor(
	Cairo::RefPtr< Cairo::Context > cr,
	int64_t pos,
	Gdk::Color color,
	bool label_to_left)
{
	//Only draw the cursor if it's on screen
	double x = XAxisUnitsToXPosition(pos);
	if( (x < 0) || (x >= m_plotRight) )
		return;

	//Draw the actual cursor
	cr->set_source_rgb(color.get_red_p(), color.get_green_p(), color.get_blue_p());
	cr->move_to(x, 0);
	cr->line_to(x, m_height);
	cr->stroke();

	//For now, only display labels on analog channels
	if(!IsAnalog() && !IsWaterfall())
		return;

	//Draw the value label at the bottom
	string text = m_channel.GetYAxisUnits().PrettyPrint(GetValueAtTime(pos));

	//Figure out text size
	int twidth;
	int theight;
	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create(get_pango_context());
	tlayout->set_font_description(m_cursorLabelFont);
	tlayout->set_text(text);
	tlayout->get_pixel_size(twidth, theight);

	//Draw background
	int labelmargin = 2;
	int left;
	int right;
	if(label_to_left)
	{
		right = x - labelmargin;
		left = x - (twidth + 2*labelmargin);
	}
	else
	{
		left = x + labelmargin;
		right = x + (twidth + 2*labelmargin);
	}
	int bottom = m_height;
	int top = m_height - (theight + 2*labelmargin);
	cr->set_source_rgba(0, 0, 0, 0.75);
	cr->move_to(left, bottom);
	cr->line_to(right, bottom);
	cr->line_to(right, top);
	cr->line_to(left, top);
	cr->fill();

	//Draw text
	cr->set_source_rgb(color.get_red_p(), color.get_green_p(), color.get_blue_p());
	cr->save();
		cr->move_to(labelmargin + left, top + labelmargin);
		tlayout->update_from_cairo_context(cr);
		tlayout->show_in_cairo_context(cr);
	cr->restore();
}

void WaveformArea::RenderHorizontalCursor(
	Cairo::RefPtr< Cairo::Context > cr,
	float pos,
	Gdk::Color color,
	bool label_to_top,
	bool show_delta)
{
	//Don't draw offscreen cursors
	double y = YAxisUnitsToYPosition(pos);
	if( (y >= m_height) || (y <= 0) )
		return;

	//Draw the actual cursor
	cr->set_source_rgb(color.get_red_p(), color.get_green_p(), color.get_blue_p());
	cr->move_to(0, y);
	cr->line_to(m_width, y);
	cr->stroke();

	//Draw the value label at the right side
	auto unit = m_channel.GetYAxisUnits();
	string text = unit.PrettyPrint(pos);
	if(show_delta)
	{
		text += "\nΔY = ";
		text += unit.PrettyPrint(m_group->m_yCursorPos[0] - m_group->m_yCursorPos[1]);
	}

	//Figure out text size
	int twidth;
	int theight;
	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create(get_pango_context());
	tlayout->set_font_description(m_cursorLabelFont);
	tlayout->set_text(text);
	tlayout->get_pixel_size(twidth, theight);

	//Draw background
	int labelmargin = 2;
	int top;
	int bottom;
	if(label_to_top)
	{
		bottom = y - labelmargin;
		top = y - (theight + 2*labelmargin);
	}
	else
	{
		top = y + labelmargin;
		bottom = y + (theight + 2*labelmargin);
	}
	int right = m_width;
	int left = m_width - (twidth + 2*labelmargin);
	cr->set_source_rgba(0, 0, 0, 0.75);
	cr->move_to(left, bottom);
	cr->line_to(right, bottom);
	cr->line_to(right, top);
	cr->line_to(left, top);
	cr->fill();

	//Draw text
	cr->set_source_rgb(color.get_red_p(), color.get_green_p(), color.get_blue_p());
	cr->save();
		cr->move_to(labelmargin + left, top + labelmargin);
		tlayout->update_from_cairo_context(cr);
		tlayout->show_in_cairo_context(cr);
	cr->restore();
}

/**
	@brief Gets the value of our channel at the specified timestamp (absolute, not waveform ticks)
 */
float WaveformArea::GetValueAtTime(int64_t time_fs)
{
	AnalogWaveform* waveform = dynamic_cast<AnalogWaveform*>(m_channel.GetData());
	if(!waveform)
		return 0;
	double ticks = 1.0f * (time_fs - waveform->m_triggerPhase)  / waveform->m_timescale;

	//Find the approximate index of the sample of interest and interpolate the cursor position
	size_t end = waveform->m_offsets.size() - 1;
	size_t index = BinarySearchForGequal(
		(int64_t*)&waveform->m_offsets[0],
		waveform->m_offsets.size(),
		(int64_t)ceil(ticks));
	if(index <= 0)
		return waveform->m_samples[0];
	if(index >= end)
		return waveform->m_samples[end];

	return Filter::InterpolateValue(waveform, index-1, ticks - waveform->m_offsets[index-1]);
}

void WaveformArea::RenderCursors(Cairo::RefPtr< Cairo::Context > cr)
{
	int ytop = 0;
	int ybot = m_height;

	Gdk::Color cursor1 = m_parent->GetPreferences().GetColor("Appearance.Cursors.cursor_1_color");
	Gdk::Color cursor2 = m_parent->GetPreferences().GetColor("Appearance.Cursors.cursor_2_color");
	Gdk::Color cursor_fill = m_parent->GetPreferences().GetColor("Appearance.Cursors.cursor_fill_color");

	switch(m_group->m_cursorConfig)
	{
		//Draw the second vertical cursor
		case WaveformGroup::CURSOR_X_DUAL:

			//Draw filled area between them
			{
				float x = min(XAxisUnitsToXPosition(m_group->m_xCursorPos[0]), m_plotRight);
				float x2 = min(XAxisUnitsToXPosition(m_group->m_xCursorPos[1]), m_plotRight);

				cr->set_source_rgba(cursor_fill.get_red_p(), cursor_fill.get_green_p(), cursor_fill.get_blue_p(), 0.2);
				cr->move_to(x, ytop);
				cr->line_to(x2, ytop);
				cr->line_to(x2, ybot);
				cr->line_to(x, ybot);
				cr->fill();
			}

			//If it's a FFT trace, render in-band power
			if(m_channel.m_channel->GetXAxisUnits() == Unit::UNIT_HZ)
				RenderInBandPower(cr);

			//Render the second cursor
			RenderVerticalCursor(cr, m_group->m_xCursorPos[1], cursor2, false);

			//fall through

		//Draw first vertical cursor
		case WaveformGroup::CURSOR_X_SINGLE:
			RenderVerticalCursor(cr, m_group->m_xCursorPos[0], cursor1, true);
			break;

		//Draw second horizontal cursor
		case WaveformGroup::CURSOR_Y_DUAL:

			if(IsDigital())
				break;

			//Render the second cursor
			RenderHorizontalCursor(cr, m_group->m_yCursorPos[1], cursor2, false, true);

			{
				//Draw filled area between them
				double y = YAxisUnitsToYPosition(m_group->m_yCursorPos[0]);
				double y2 = YAxisUnitsToYPosition(m_group->m_yCursorPos[1]);

				//Skip if it's bigger than our entire FOV
				if( (y <= 0) || (y2 >= m_height) )
				{
				}

				else
				{
					cr->set_source_rgba(
						cursor_fill.get_red_p(),
						cursor_fill.get_green_p(),
						cursor_fill.get_blue_p(), 0.2);
					cr->move_to(0,			y);
					cr->line_to(m_width,	y);
					cr->line_to(m_width,	y2);
					cr->line_to(0, 			y2);
					cr->fill();
				}
			}

			//fall through

		//Draw first horizontal cursor
		case WaveformGroup::CURSOR_Y_SINGLE:
			if(!IsDigital())
				RenderHorizontalCursor(cr, m_group->m_yCursorPos[0], cursor1, true, false);
			break;

		default:
			break;
	}
}

/**
	@brief Displays in-band power between two cursors on a frequency domain waveform
 */
void WaveformArea::RenderInBandPower(Cairo::RefPtr< Cairo::Context > cr)
{
	//If no data, we obviously can't do anything
	auto data = dynamic_cast<AnalogWaveform*>(m_channel.GetData());
	if(!data)
		return;

	//Bounds check cursors
	double vfirst = round((m_group->m_xCursorPos[0] - data->m_triggerPhase)* 1.0 / data->m_timescale);
	vfirst = max(vfirst, (double)0);
	vfirst = min(vfirst, (double)data->m_samples.size()-1);

	double vsecond = round((m_group->m_xCursorPos[1] - data->m_triggerPhase)* 1.0 / data->m_timescale);
	vsecond = max(vsecond, (double)0);
	vsecond = min(vsecond, (double)data->m_samples.size()-1);

	size_t ifirst = vfirst;
	size_t isecond = vsecond;

	//This gets a bit more complicated because we can't just sum dB!
	string text;
	auto yunit = m_channel.GetYAxisUnits();
	if(yunit == Unit(Unit::UNIT_DBM))
	{
		float total_watts = 0;
		for(size_t i=ifirst; i<=isecond; i++)
			total_watts += pow(10, (data->m_samples[i] - 30) / 10);
		float total_dbm = 10 * log10(total_watts) + 30;
		text = string("Band: ") + yunit.PrettyPrint(total_dbm);
	}

	//But if we're using linear display it's easy
	else
	{
		float total = 0;
		for(size_t i=ifirst; i<=isecond; i++)
			total += data->m_samples[i];
		text = string("Band: ") + yunit.PrettyPrint(total);
	}

	//Calculate text size
	int twidth;
	int theight;
	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create(get_pango_context());
	tlayout->set_font_description(m_cursorLabelFont);
	tlayout->set_text(text);
	tlayout->get_pixel_size(twidth, theight);

	//Add some margin
	const int margin = 2;
	int totalwidth = twidth + 2*margin;
	int totalheight = theight + 2*margin;

	//Calculate left/right cursor positions.
	//Hide text if it's too fat to fit between the cursors.
	float left_cursor = XAxisUnitsToXPosition(m_group->m_xCursorPos[0]);
	float right_cursor = XAxisUnitsToXPosition(m_group->m_xCursorPos[1]);
	float aperture = right_cursor - left_cursor;
	if(aperture < totalwidth)
		return;

	//Draw the dark background
	float midpoint = left_cursor + aperture/2;
	float left = midpoint - totalwidth * 0.5f;
	float right = midpoint + totalwidth * 0.5f;
	float bottom = m_height;
	float top = m_height - totalheight;
	cr->set_source_rgba(0, 0, 0, 0.75);
	cr->move_to(left, bottom);
	cr->line_to(right, bottom);
	cr->line_to(right, top);
	cr->line_to(left, top);
	cr->fill();

	//Draw the text
	Gdk::Color cursor_fill_text = m_parent->GetPreferences().GetColor("Appearance.Cursors.cursor_fill_text_color");
	cr->set_source_rgb(cursor_fill_text.get_red_p(), cursor_fill_text.get_green_p(), cursor_fill_text.get_blue_p());
	cr->save();
		cr->move_to(left + margin, top + margin);
		tlayout->update_from_cairo_context(cr);
		tlayout->show_in_cairo_context(cr);
	cr->restore();
}

void WaveformArea::RenderInsertionBar(Cairo::RefPtr< Cairo::Context > cr)
{
	int barsize = 5;

	Gdk::Color bar_color = m_parent->GetPreferences().GetColor("Appearance.Windows.insertion_bar_color");
	Gdk::Color bar_split_color = m_parent->GetPreferences().GetColor("Appearance.Windows.insertion_bar_split_color");

	if(m_insertionBarLocation != INSERT_NONE)
	{
		int barpos = 0;
		float alpha = 0.75;
		bool barhorz = true;
		switch(m_insertionBarLocation)
		{
			case INSERT_BOTTOM:
				cr->set_source_rgba(bar_color.get_red_p(), bar_color.get_green_p(), bar_color.get_blue_p(), alpha);
				barpos = m_height - barsize;
				break;

			case INSERT_BOTTOM_SPLIT:
				cr->set_source_rgba(
					bar_split_color.get_red_p(),
					bar_split_color.get_green_p(),
					bar_split_color.get_blue_p(),
					alpha);
				barpos = m_height - barsize;
				break;

			case INSERT_TOP:
				cr->set_source_rgba(bar_color.get_red_p(), bar_color.get_green_p(), bar_color.get_blue_p(), alpha);
				barpos = 0;
				break;

			case INSERT_RIGHT_SPLIT:
				cr->set_source_rgba(
					bar_split_color.get_red_p(),
					bar_split_color.get_green_p(),
					bar_split_color.get_blue_p(),
					alpha);
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

	else if(m_dragState == DRAG_OVERLAY)
	{
		cr->set_source_rgba(bar_color.get_red_p(), bar_color.get_green_p(), bar_color.get_blue_p(), 0.75);
		cr->move_to(0, 				m_dragOverlayPosition);
		cr->line_to(m_plotRight,	m_dragOverlayPosition);
		cr->line_to(m_plotRight,	m_dragOverlayPosition + barsize);
		cr->line_to(0,				m_dragOverlayPosition + barsize);
		cr->fill();
	}
}

void WaveformArea::MakePathSignalBody(
	const Cairo::RefPtr<Cairo::Context>& cr,
	float xstart, float /*xoff*/, float xend, float ybot, float ymid, float ytop)
{
	//Square off edges if really tiny
	float rounding = 5;
	if((xend-xstart) < 2*rounding)
		rounding = 0;

	cr->begin_new_sub_path();
	cr->move_to(xstart, 			ymid);		//left point
	cr->line_to(xstart + rounding,	ytop);		//top left corner
	cr->line_to(xend - rounding, 	ytop);		//top right corner
	cr->line_to(xend,				ymid);		//right point
	cr->line_to(xend - rounding,	ybot);		//bottom right corner
	cr->line_to(xstart + rounding,	ybot);		//bottom left corner
	cr->line_to(xstart, 			ymid);		//left point again
}

void WaveformArea::RenderComplexSignal(
		const Cairo::RefPtr<Cairo::Context>& cr,
		int visleft, int visright,
		float xstart, float xend, float xoff,
		float ybot, float ymid, float ytop,
		string str,
		Gdk::Color color)
{
	//Clamp start point to left side of display
	if(xstart < visleft)
		xstart = visleft;

	//First-order guess of position: center of the value
	float xp = xstart + (xend-xstart)/2;

	//Width within this signal outline
	float available_width = xend - xstart - 2*xoff;

	//Convert all whitespace in text to spaces
	for(size_t i=0; i<str.length(); i++)
	{
		if(isspace(str[i]))
			str[i] = ' ';
	}

	//If the space is tiny, don't even attempt to render it.
	//Figuring out text size is expensive when we have hundreds or thousands of packets on screen, but in this case
	//we *know* it won't fit.
	bool drew_text = false;
	if(available_width > 15)
	{
		int width;
		int sheight;

		Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create(get_pango_context());
		tlayout->set_font_description(m_decodeFont);
		tlayout->set_text(str);
		tlayout->get_pixel_size(width, sheight);

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
			float new_width = available_width;
			int padding = 5;
			if(xp < (visleft + padding))
			{
				xp = visleft + padding;
				new_width = xend - xp - xoff;
			}

			//Off the right end? Push it left
			else if( (xp + width + padding) > visright)
			{
				xp = visright - (width + padding + xoff);
				if(xp < xstart)
					xp = xstart + xoff;

				if(xend < visright)
					new_width = xend - xp - xoff;
				else
					new_width = visright - xp - xoff;
			}

			if(new_width < available_width)
				available_width = new_width;

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
					tlayout->set_text(str_render);
					tlayout->get_pixel_size(twidth, theight);

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

			drew_text = true;
			cr->save();
				cr->move_to(xp, ymid - sheight/2);
				tlayout->update_from_cairo_context(cr);
				tlayout->show_in_cairo_context(cr);
			cr->restore();
		}
	}

	if(xend > visright)
		xend = visright;

	//If no text fit, draw filler instead
	if(!drew_text)
	{
		cr->set_source_rgb(color.get_red_p() * 0.25, color.get_green_p() * 0.25, color.get_blue_p() * 0.25);
		MakePathSignalBody(cr, xstart, xoff, xend, ybot, ymid, ytop);
		cr->fill();
	}

	//Draw the body outline after any filler so it shows up on top
	cr->set_source_rgb(color.get_red_p(), color.get_green_p(), color.get_blue_p());
	MakePathSignalBody(cr, xstart, xoff, xend, ybot, ymid, ytop);
	cr->stroke();
}

/**
	@brief Draw peaks on the FFT
 */
void WaveformArea::RenderFFTPeaks(Cairo::RefPtr< Cairo::Context > cr)
{
	//Grab input and stop if there's nothing for us to do
	auto chan = m_channel.m_channel;
	auto filter = dynamic_cast<PeakDetector*>(chan);
	if(!filter)
		return;
	const vector<Peak>& peaks = filter->GetPeaks();
	auto data = chan->GetData(0);
	if(peaks.empty() || (data == NULL) )
		return;

	int64_t timescale = data->m_timescale;

	//Settings for the text
	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create(get_pango_context());
	tlayout->set_font_description(m_cursorLabelFont);
	int twidth;
	int theight;
	int margin = 2;

	auto xunit = chan->GetXAxisUnits();
	auto yunit = m_channel.GetYAxisUnits();

	//First pass: get nominal locations of each peak label and discard offscreen ones
	float radius = 4;
	vector<string> texts;
	vector<vec2f> centers;
	vector<Rect> rects;
	for(size_t i=0; i<peaks.size(); i++)
	{
		int64_t nx = peaks[i].m_x * timescale + data->m_triggerPhase;

		//Format the text
		string text = xunit.PrettyPrint(nx) + "\n" + yunit.PrettyPrint(peaks[i].m_y);

		//Calculate text size
		tlayout->set_text(text);
		tlayout->get_pixel_size(twidth, theight);

		float x = XAxisUnitsToXPosition(nx);
		float y = YAxisUnitsToYPosition(peaks[i].m_y);

		//Don't show labels for offscreen peaks
		if( (x < 0) || (x > m_plotRight) )
			continue;

		texts.push_back(text);
		rects.push_back(Rect(x, y, twidth + 2*margin, theight + 2*margin));
		centers.push_back(vec2f(x, y));
	}

	//Move the labels around to remove overlaps
	RemoveOverlaps(rects, centers);

	//Second pass: Lines from rectangle location to peak location
	Gdk::Color outline_color = m_parent->GetPreferences().GetColor("Appearance.Peaks.peak_outline_color");
	Gdk::Color text_color = m_parent->GetPreferences().GetColor("Appearance.Peaks.peak_text_color");
	Gdk::Color background_color = m_parent->GetPreferences().GetColor("Appearance.Peaks.peak_background_color");

	cr->set_source_rgba(outline_color.get_red_p(), outline_color.get_green_p(), outline_color.get_blue_p(), 1);
	for(size_t i=0; i<rects.size(); i++)
	{
		//Draw a line from the rectangle's closest point to the peak location
		auto center = centers[i];
		vec2f closest = rects[i].ClosestPoint(center);
		cr->move_to(closest.x, closest.y);
		cr->line_to(center.x, center.y);
		cr->stroke();
	}

	//Third pass: Background and text
	for(size_t i=0; i<rects.size(); i++)
	{
		tlayout->set_text(texts[i]);

		//Draw the background
		int rounding = 4;
		auto rect = rects[i];
		cr->set_source_rgba(
			background_color.get_red_p(),
			background_color.get_green_p(),
			background_color.get_blue_p(),
			0.75);
		MakePathRoundedRect(cr, rect, rounding);
		cr->fill_preserve();

		//Draw the outline
		cr->set_source_rgba(outline_color.get_red_p(), outline_color.get_green_p(), outline_color.get_blue_p(), 1);
		cr->stroke();

		//Draw the text
		cr->set_source_rgba(text_color.get_red_p(), text_color.get_green_p(), text_color.get_blue_p(), 1);
		cr->save();
			cr->move_to(rect.get_left() + margin, rect.get_top() + margin);
			tlayout->update_from_cairo_context(cr);
			tlayout->show_in_cairo_context(cr);
		cr->restore();

		//Draw the actual peak marker
		auto center = centers[i];
		cr->begin_new_path();
		cr->arc(center.x, center.y, radius, 0, 2*M_PI);
		cr->stroke();
	}
}

/**
	@brief Performs point-feature label placement given a list of nominal label positions

	Simple energy minimization using linear springs.
 */
void WaveformArea::RemoveOverlaps(vector<Rect>& rects, vector<vec2f>& peaks)
{
	//Centroids of each rectangle
	vector<vec2f> centers;
	for(auto r : rects)
		centers.push_back(r.center());
	for(auto c : peaks)
		centers.push_back(c);

	int margin = 3;

	for(int i=0; i<100; i++)
	{
		//Things we can collide with
		int clearance = 8;
		vector<Rect> targets = rects;
		for(auto c : peaks)
			targets.push_back(Rect(c.x - clearance, c.y - clearance, 2*clearance, 2*clearance));

		bool done = true;

		for(size_t j=0; j<rects.size(); j++)
		{
			vec2f force(0, 0);

			//Repulsive force pushing us away from the centroid of all other rectangles.
			for(size_t k=0; k<targets.size(); k++)
			{
				//no self-intersection
				if(j == k)
					continue;

				Rect rj = rects[j];
				Rect rk = targets[k];
				rj.expand(margin, margin);
				rk.expand(margin, margin);

				//if no overlap, ignore
				if(!rj.intersects(rk))
					continue;

				done = false;

				//Scale force by amount of overlap.
				rj.intersect(rk);
				float scale = rj.get_width() * rj.get_height();

				auto delta = (centers[k] - centers[j]);
				force -= delta.norm() * scale * 0.01;
			}

			//Move it
			centers[j] += force;

			//Move rectangle (rounding coordinates to nearest integer)
			rects[j].recenter(centers[j]);

			//If it moved off screen, clamp it
			if(rects[j].get_y() < 0)
			{
				rects[j].set_y(0);
				centers[j].y = rects[j].center().y;
			}

			if(rects[j].get_right() > m_plotRight)
			{
				rects[j].set_x(m_plotRight - rects[j].get_width());
				centers[j].x = rects[j].center().x;
			}
		}

		if(done)
			break;
	}
}
