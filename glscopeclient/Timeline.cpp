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

#include "glscopeclient.h"
#include "WaveformGroup.h"
#include "Timeline.h"
#include "WaveformArea.h"
#include "OscilloscopeWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

Timeline::Timeline(OscilloscopeWindow* parent, WaveformGroup* group)
	: m_group(group)
	, m_parent(parent)
{
	m_dragState = DRAG_NONE;
	m_dragStartX = 0;
	m_originalTimeOffset = 0;

	set_size_request(1, 40);

	add_events(
		Gdk::POINTER_MOTION_MASK |
		Gdk::BUTTON_PRESS_MASK |
		Gdk::SCROLL_MASK |
		Gdk::BUTTON_RELEASE_MASK);
}

Timeline::~Timeline()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI events

bool Timeline::on_button_press_event(GdkEventButton* event)
{
	//for now, only handle left click
	if(event->button == 1)
	{
		m_dragState = DRAG_TIMELINE;
		m_dragStartX = event->x;
		m_originalTimeOffset = m_group->m_xAxisOffset;
	}

	return true;
}

bool Timeline::on_button_release_event(GdkEventButton* event)
{
	if(event->button == 1)
	{
		m_dragState = DRAG_NONE;
	}
	return true;
}

bool Timeline::on_motion_notify_event(GdkEventMotion* event)
{
	switch(m_dragState)
	{
		//Dragging the horizontal offset
		case DRAG_TIMELINE:
			{
				double dx = event->x - m_dragStartX;
				double ps = dx / m_group->m_pixelsPerXUnit;

				//Update offset, but don't allow scrolling before the start of the capture
				m_group->m_xAxisOffset = m_originalTimeOffset - ps;
				if(m_group->m_xAxisOffset < 0)
					m_group->m_xAxisOffset = 0;

				//Clear persistence and redraw the group (fixes #46)
				m_group->GetParent()->ClearPersistence(m_group, false);
			}
			break;

		default:
			break;
	}

	return true;
}

bool Timeline::on_scroll_event (GdkEventScroll* ev)
{
	switch(ev->direction)
	{
		case GDK_SCROLL_LEFT:
			m_parent->OnZoomInHorizontal(m_group);
			break;
		case GDK_SCROLL_RIGHT:
			m_parent->OnZoomOutHorizontal(m_group);
			break;

		case GDK_SCROLL_SMOOTH:
			if(ev->delta_y < 0)
				m_parent->OnZoomInHorizontal(m_group);
			else
				m_parent->OnZoomOutHorizontal(m_group);
			break;

		default:
			break;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool Timeline::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
	cr->save();

	//Cache some coordinates
	size_t w = get_width();
	size_t h = get_height();
	double ytop = 2;

	//Draw the background
	Gdk::Color black("black");
	cr->set_source_rgb(black.get_red_p(), black.get_green_p(), black.get_blue_p());
	cr->rectangle(0, 0, w, h);
	cr->fill();

	//Set the color
	Gdk::Color color("white");
	cr->set_source_rgb(color.get_red_p(), color.get_green_p(), color.get_blue_p());

	//Draw top line
	cr->move_to(0, ytop);
	cr->line_to(w, ytop);
	cr->stroke();

	//Figure out the units to use for the axis
	auto children = m_group->m_waveformBox.get_children();
	Unit unit(Unit::UNIT_PS);
	if(!children.empty())
	{
		auto view = dynamic_cast<WaveformArea*>(children[0]);
		if(view != NULL)
			unit = view->GetChannel()->GetXAxisUnits();
	}

	//And actually draw the rest
	Render(cr, unit);

	cr->restore();
	return true;
}

void Timeline::Render(const Cairo::RefPtr<Cairo::Context>& cr, Unit xAxisUnit)
{
	size_t w = get_width();
	size_t h = get_height();
	double ytop = 2;
	double ybot = h - 10;
	double ymid = (h-10) / 2;

	//Figure out rounding granularity, based on our time scales
	int64_t width_ps = w / m_group->m_pixelsPerXUnit;
	int64_t round_divisor = 1;
	if(width_ps < 1E4)
	{
		//ps, leave default
		if(width_ps < 100)
			round_divisor = 10;
		else if(width_ps < 500)
			round_divisor = 50;
		else if(width_ps < 1000)
			round_divisor = 100;
		else if(width_ps < 2500)
			round_divisor = 250;
		else if(width_ps < 5000)
			round_divisor = 500;
		else
			round_divisor = 1000;
	}
	else if(width_ps < 1E6)
		round_divisor = 1E3;
	else if(width_ps < 1E9)
	{
		if(width_ps < 1e8)
			round_divisor = 1e5;
		else
			round_divisor = 1E6;
	}
	else if(width_ps < 1E11)
		round_divisor = 1E9;
	else
		round_divisor = 1E12;

	//Figure out about how much time per graduation to use
	const double min_label_grad_width = 100;		//Minimum distance between text labels, in pixels
	double grad_ps_nominal = min_label_grad_width / m_group->m_pixelsPerXUnit;

	//Round so the division sizes are sane
	double units_per_grad = grad_ps_nominal * 1.0 / round_divisor;
	double base = 5;
	double log_units = log(units_per_grad) / log(base);
	double log_units_rounded = ceil(log_units);
	double units_rounded = pow(base, log_units_rounded);
	int64_t grad_ps_rounded = units_rounded * round_divisor;

	//avoid divide-by-zero in weird cases with no waveform etc
	if(grad_ps_rounded == 0)
		return;

	//Calculate number of ticks within a division
	double nsubticks = 5;
	double subtick = grad_ps_rounded / nsubticks;

	//Find the start time (rounded down as needed)
	double tstart = floor(m_group->m_xAxisOffset / grad_ps_rounded) * grad_ps_rounded;

	//Print tick marks and labels
	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create (cr);
	Pango::FontDescription font("sans normal 10");
	font.set_weight(Pango::WEIGHT_NORMAL);
	tlayout->set_font_description(font);
	int swidth;
	int sheight;
	for(double t = tstart; t < (tstart + width_ps + grad_ps_rounded); t += grad_ps_rounded)
	{
		double x = (t - m_group->m_xAxisOffset) * m_group->m_pixelsPerXUnit;

		//Draw fine ticks first (even if the labeled graduation doesn't fit)
		for(int tick=1; tick < nsubticks; tick++)
		{
			double subx = (t - m_group->m_xAxisOffset + tick*subtick) * m_group->m_pixelsPerXUnit;

			if(subx < 0)
				continue;
			if(subx > w)
				break;

			cr->move_to(subx, ytop);
			cr->line_to(subx, ytop + 10);
		}
		cr->stroke();

		if(x < 0)
			continue;
		if(x > w)
			break;

		//Tick mark
		cr->move_to(x, ytop);
		cr->line_to(x, ybot);
		cr->stroke();

		//Render it
		tlayout->set_text(xAxisUnit.PrettyPrint(t));
		tlayout->get_pixel_size(swidth, sheight);
		cr->move_to(x+2, ymid + sheight/2);
		tlayout->update_from_cairo_context(cr);
		tlayout->show_in_cairo_context(cr);
	}


	//Draw cursor positions if requested
	Gdk::Color yellow("yellow");
	Gdk::Color orange("orange");

	if( (m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL) ||
		(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_SINGLE) )
	{
		//Dual cursors
		if(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL)
		{
			//Draw filled area between them
			double x = (m_group->m_xCursorPos[0] - m_group->m_xAxisOffset) * m_group->m_pixelsPerXUnit;
			double x2 = (m_group->m_xCursorPos[1] - m_group->m_xAxisOffset) * m_group->m_pixelsPerXUnit;
			cr->set_source_rgba(yellow.get_red_p(), yellow.get_green_p(), yellow.get_blue_p(), 0.2);
			cr->move_to(x, 0);
			cr->line_to(x2, 0);
			cr->line_to(x2, h);
			cr->line_to(x, h);
			cr->fill();

			//Second cursor
			DrawCursor(
				cr,
				m_group->m_xCursorPos[1],
				"X2",
				orange,
				false,
				true,
				xAxisUnit);
		}

		//First cursor
		DrawCursor(
			cr,
			m_group->m_xCursorPos[0],
			"X1",
			yellow,
			true,
			false,
			xAxisUnit);
	}
}

void Timeline::DrawCursor(
	const Cairo::RefPtr<Cairo::Context>& cr,
	int64_t ps,
	const char* name,
	Gdk::Color color,
	bool draw_left,
	bool show_delta,
	Unit xAxisUnit)
{
	int h = get_height();

	Gdk::Color black("black");

	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create (cr);
	Pango::FontDescription font("sans normal 10");
	font.set_weight(Pango::WEIGHT_NORMAL);
	tlayout->set_font_description(font);
	int swidth;
	int sheight;

	//Format label for cursor
	char label[256];
	if(!show_delta)
	{
		snprintf(
			label,
			sizeof(label),
			"%s: %s",
			name,
			xAxisUnit.PrettyPrint(ps).c_str()
			);
	}
	else
	{
		string format("%s: %s\nΔX = %s");

		//Special case for time domain traces
		//Also show the frequency dual
		if(xAxisUnit.GetType() == Unit::UNIT_PS)
			format += " (%.3f MHz)\n";

		int64_t dt = m_group->m_xCursorPos[1] - m_group->m_xCursorPos[0];

		snprintf(
			label,
			sizeof(label),
			format.c_str(),
			name,
			xAxisUnit.PrettyPrint(ps).c_str(),
			xAxisUnit.PrettyPrint(dt).c_str(),
			1.0e6 / dt);
	}
	tlayout->set_text(label);
	tlayout->get_pixel_size(swidth, sheight);

	//Decide which side of the line to draw on
	double x = (ps - m_group->m_xAxisOffset) * m_group->m_pixelsPerXUnit;
	double right = x-5;
	double left = right - swidth - 5;
	if(!draw_left)
	{
		left = x + 5;
		right = left + swidth + 5;
	}

	//Draw filled background for label
	cr->set_source_rgba(black.get_red_p(), black.get_green_p(), black.get_blue_p(), 0.75);
	double bot = 10;
	double top = bot + sheight;
	cr->move_to(left, top);
	cr->line_to(left, bot);
	cr->line_to(right, bot);
	cr->line_to(right, top);
	cr->fill();

	//Label text
	cr->set_source_rgb(color.get_red_p(), color.get_green_p(), color.get_blue_p());
	cr->move_to(left+5, bot);
	tlayout->update_from_cairo_context(cr);
	tlayout->show_in_cairo_context(cr);

	//Cursor line
	cr->move_to(x, 0);
	cr->line_to(x, h);
	cr->set_source_rgb(color.get_red_p(), color.get_green_p(), color.get_blue_p());
	cr->stroke();
}
