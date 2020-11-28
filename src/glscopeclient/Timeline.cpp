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
	, m_xAxisUnit(Unit::UNIT_FS)
{
	m_dragState = DRAG_NONE;
	m_dragStartX = 0;
	m_originalTimeOffset = 0;

	set_size_request(1, 45 * get_pango_context()->get_resolution() / 96);

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
	auto scale = get_window()->get_scale_factor();
	event->x *= scale;
	event->y *= scale;

	if(event->type == GDK_BUTTON_PRESS)
	{
		//for now, only handle left click
		if(event->button == 1)
		{
			m_dragState = DRAG_TIMELINE;
			m_dragStartX = event->x;
			m_originalTimeOffset = m_group->m_xAxisOffset;

			get_window()->set_cursor(Gdk::Cursor::create(get_display(), "grabbing"));
		}
	}

	//Double click
	else if(event->type == GDK_2BUTTON_PRESS)
	{
		if(event->button == 1)
			m_parent->OnTimebaseSettings();

		m_dragState = DRAG_NONE;
	}

	return true;
}

bool Timeline::on_button_release_event(GdkEventButton* event)
{
	if(event->button == 1)
	{
		m_dragState = DRAG_NONE;
		get_window()->set_cursor(Gdk::Cursor::create(get_display(), "grab"));
	}
	return true;
}

bool Timeline::on_motion_notify_event(GdkEventMotion* event)
{
	auto scale = get_window()->get_scale_factor();
	event->x *= scale;
	event->y *= scale;

	switch(m_dragState)
	{
		//Dragging the horizontal offset
		case DRAG_TIMELINE:
			{
				double dx = event->x - m_dragStartX;
				double fs = dx / m_group->m_pixelsPerXUnit;

				//Update offset
				m_group->m_xAxisOffset = m_originalTimeOffset - fs;

				//Clear persistence and redraw the group (fixes #46)
				m_group->GetParent()->ClearPersistence(m_group, false, true);
			}
			break;

		default:
			break;
	}

	return true;
}

bool Timeline::on_scroll_event (GdkEventScroll* ev)
{
	auto scale = get_window()->get_scale_factor();
	ev->x *= scale;
	ev->y *= scale;

	int64_t timestamp = (ev->x / m_group->m_pixelsPerXUnit) + m_group->m_xAxisOffset;

	switch(ev->direction)
	{
		case GDK_SCROLL_LEFT:
			m_parent->OnZoomInHorizontal(m_group, timestamp);
			break;
		case GDK_SCROLL_RIGHT:
			m_parent->OnZoomOutHorizontal(m_group, timestamp);
			break;

		case GDK_SCROLL_SMOOTH:
			if(ev->delta_y < 0)
				m_parent->OnZoomInHorizontal(m_group, timestamp);
			else
				m_parent->OnZoomOutHorizontal(m_group, timestamp);
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
	OscilloscopeChannel* chan = NULL;
	if(!children.empty())
	{
		auto view = dynamic_cast<WaveformArea*>(children[0]);
		if(view != NULL)
		{
			chan = view->GetChannel().m_channel;
			m_xAxisUnit = chan->GetXAxisUnits();
		}
	}

	//And actually draw the rest
	Render(cr, chan);

	cr->restore();
	return true;
}

void Timeline::Render(const Cairo::RefPtr<Cairo::Context>& cr, OscilloscopeChannel* chan)
{
	float xscale = m_group->m_pixelsPerXUnit / get_window()->get_scale_factor();

	size_t w = get_width();
	size_t h = get_height();
	double ytop = 2;
	double ybot = h - 10;
	double ymid = (h-10) / 2;

	//Figure out rounding granularity, based on our time scales
	int64_t width_fs = w / xscale;
	int64_t round_divisor = 1;
	if(width_fs < 1E7)
	{
		//fs, leave default
		if(width_fs < 1e2)
			round_divisor = 1e1;
		else if(width_fs < 1e5)
			round_divisor = 1e4;
		else if(width_fs < 5e5)
			round_divisor = 5e4;
		else if(width_fs < 1e6)
			round_divisor = 1e5;
		else if(width_fs < 2.5e6)
			round_divisor = 2.5e5;
		else if(width_fs < 5e6)
			round_divisor = 5e5;
		else
			round_divisor = 1e6;
	}
	else if(width_fs < 1e9)
		round_divisor = 1e6;
	else if(width_fs < 1e12)
	{
		if(width_fs < 1e11)
			round_divisor = 1e8;
		else
			round_divisor = 1e9;
	}
	else if(width_fs < 1E14)
		round_divisor = 1E12;
	else
		round_divisor = 1E15;

	//Figure out about how much time per graduation to use
	const double min_label_grad_width = 75 * GetDPIScale();	//Minimum distance between text labels, in pixels
	double grad_fs_nominal = min_label_grad_width / xscale;

	//Round so the division sizes are sane
	double units_per_grad = grad_fs_nominal * 1.0 / round_divisor;
	double base = 5;
	double log_units = log(units_per_grad) / log(base);
	double log_units_rounded = ceil(log_units);
	double units_rounded = pow(base, log_units_rounded);
	int64_t grad_fs_rounded = round(units_rounded * round_divisor);

	//avoid divide-by-zero in weird cases with no waveform etc
	if(grad_fs_rounded == 0)
		return;

	//Calculate number of ticks within a division
	double nsubticks = 5;
	double subtick = grad_fs_rounded / nsubticks;

	//Find the start time (rounded as needed)
	double tstart = round(m_group->m_xAxisOffset / grad_fs_rounded) * grad_fs_rounded;

	//Print tick marks and labels
	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create(get_pango_context());
	Pango::FontDescription font = m_parent->GetPreferences().GetFont("Appearance.Timeline.tick_label_font");
	font.set_weight(Pango::WEIGHT_NORMAL);
	tlayout->set_font_description(font);
	int swidth;
	int sheight;
	for(double t = tstart; t < (tstart + width_fs + grad_fs_rounded); t += grad_fs_rounded)
	{
		double x = (t - m_group->m_xAxisOffset) * xscale;

		//Draw fine ticks first (even if the labeled graduation doesn't fit)
		for(int tick=1; tick < nsubticks; tick++)
		{
			double subx = (t - m_group->m_xAxisOffset + tick*subtick) * xscale;

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
		tlayout->set_text(m_xAxisUnit.PrettyPrint(t));
		tlayout->get_pixel_size(swidth, sheight);
		cr->move_to(x+2, ymid);
		tlayout->update_from_cairo_context(cr);
		tlayout->show_in_cairo_context(cr);
	}


	//Draw cursor positions if requested
	if( (m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL) ||
		(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_SINGLE) )
	{
		//Dual cursors
		if(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL)
		{
			//Draw filled area between them
			double x = (m_group->m_xCursorPos[0] - m_group->m_xAxisOffset) * m_group->m_pixelsPerXUnit;
			double x2 = (m_group->m_xCursorPos[1] - m_group->m_xAxisOffset) * m_group->m_pixelsPerXUnit;
			Gdk::Color cursor_fill = m_parent->GetPreferences().GetColor("Appearance.Cursors.cursor_fill_color");
			cr->set_source_rgba(cursor_fill.get_red_p(), cursor_fill.get_green_p(), cursor_fill.get_blue_p(), 0.2);
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
				m_parent->GetPreferences().GetColor("Appearance.Cursors.cursor_2_color"),
				false,
				true);
		}

		//First cursor
		DrawCursor(
			cr,
			m_group->m_xCursorPos[0],
			"X1",
			m_parent->GetPreferences().GetColor("Appearance.Cursors.cursor_1_color"),
			true,
			false);
	}

	//Draw trigger position for the first scope in the plot
	//TODO: handle more than one scope
	if(chan)
	{
		auto scope = chan->GetScope();
		if(scope == NULL)
			return;
		int64_t timestamp = scope->GetTriggerOffset();
		double x = (timestamp - m_group->m_xAxisOffset) * xscale;

		auto trig = scope->GetTrigger();
		if(trig)
		{
			auto c = trig->GetInput(0).m_channel;
			if(c)
			{
				Gdk::Color color(c->m_displaycolor);
				cr->set_source_rgba(color.get_red_p(), color.get_green_p(), color.get_blue_p(), 1.0);
			}

			int size = 5 * GetDPIScale();
			cr->move_to(x-size, h-size);
			cr->line_to(x,		h);
			cr->line_to(x+size, h-size);
			cr->fill();
		}

	}
}

void Timeline::DrawCursor(
	const Cairo::RefPtr<Cairo::Context>& cr,
	int64_t fs,
	const char* name,
	Gdk::Color color,
	bool draw_left,
	bool show_delta)
{
	float xscale = m_group->m_pixelsPerXUnit / get_window()->get_scale_factor();

	int h = get_height();

	Gdk::Color black("black");

	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create(get_pango_context());
	Pango::FontDescription font = m_parent->GetPreferences().GetFont("Appearance.Cursors.label_font");
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
			m_xAxisUnit.PrettyPrint(fs).c_str()
			);
	}
	else
	{
		string format("%s: %s\nΔX = %s");

		//Special case for time domain traces
		//Also show the frequency dual
		Unit hz(Unit::UNIT_HZ);
		if(m_xAxisUnit.GetType() == Unit::UNIT_FS)
			format += " (%s)\n";

		int64_t dt = m_group->m_xCursorPos[1] - m_group->m_xCursorPos[0];

		snprintf(
			label,
			sizeof(label),
			format.c_str(),
			name,
			m_xAxisUnit.PrettyPrint(fs).c_str(),
			m_xAxisUnit.PrettyPrint(dt).c_str(),
			hz.PrettyPrint(FS_PER_SECOND / dt).c_str());
	}
	tlayout->set_text(label);
	tlayout->get_pixel_size(swidth, sheight);

	//Decide which side of the line to draw on
	double x = (fs - m_group->m_xAxisOffset) * xscale;
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

void Timeline::on_realize()
{
	Gtk::Layout::on_realize();

	get_window()->set_cursor(Gdk::Cursor::create(get_display(), "grab"));
}
