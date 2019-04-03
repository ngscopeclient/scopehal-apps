/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2019 Andrew D. Zonenberg                                                                          *
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

using namespace std;

Timeline::Timeline()
{
	set_size_request(1, 40);
}

Timeline::~Timeline()
{
}

bool Timeline::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
	cr->save();

	//Cache some coordinates
	size_t w = get_width();
	size_t h = get_height();
	double ytop = 2;
	double ybot = h - 10;
	double ymid = (h-10) / 2;

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

	//Figure out what units to use, based on the width of our window
	//TODO: handle horizontal offsets
	int64_t width_ps = w / m_group->m_pixelsPerPicosecond;
	const char* units = "ps";
	int64_t unit_divisor = 1;
	int64_t round_divisor = 1;
	string sformat = "%.0lf %s";
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

		sformat = "%.0lf %s";
	}
	else if(width_ps < 1E6)
	{
		units = "ns";
		unit_divisor = 1E3;
		round_divisor = 1E3;

		sformat = "%.2lf %s";
	}
	else if(width_ps < 1E9)
	{
		units = "μs";
		unit_divisor = 1E6;

		if(width_ps < 1e8)
			round_divisor = 1e5;
		else
			round_divisor = 1E6;

		sformat = "%.4lf %s";
	}
	else if(width_ps < 1E11)
	{
		units = "ms";
		unit_divisor = 1E9;
		round_divisor = 1E9;

		sformat = "%.6lf %s";
	}
	else
	{
		units = "s";
		unit_divisor = 1E12;
		round_divisor = 1E12;
	}
	//LogDebug("width_ps = %zu, unit_divisor = %zu, round_divisor = %zu\n",
	//	width_ps, unit_divisor, round_divisor);

	//Figure out about how much time per graduation to use
	const int min_label_grad_width = 100;		//Minimum distance between text labels, in pixels
	int64_t grad_ps_nominal = min_label_grad_width / m_group->m_pixelsPerPicosecond;

	//Round so the division sizes are sane
	double units_per_grad = grad_ps_nominal * 1.0 / round_divisor;
	double base = 5;
	double log_units = log(units_per_grad) / log(base);
	double log_units_rounded = ceil(log_units);
	double units_rounded = pow(base, log_units_rounded);
	int64_t grad_ps_rounded = units_rounded * round_divisor;

	//Calculate number of ticks within a division
	double nsubticks = 5;
	double subtick = grad_ps_rounded / nsubticks;

	//Print tick marks and labels
	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create (cr);
	Pango::FontDescription font("sans normal 10");
	font.set_weight(Pango::WEIGHT_NORMAL);
	tlayout->set_font_description(font);
	for(double t = 0; t < width_ps; t += grad_ps_rounded)
	{
		double x = t * m_group->m_pixelsPerPicosecond;

		//Tick mark
		cr->move_to(x, ytop);
		cr->line_to(x, ybot);
		cr->stroke();

		//Format the string
		double scaled_time = t / unit_divisor;
		char namebuf[256];
		snprintf(namebuf, sizeof(namebuf), sformat.c_str(), scaled_time, units);

		//Render it
		int swidth;
		int sheight;
		tlayout->set_text(namebuf);
		tlayout->get_pixel_size(swidth, sheight);
		cr->move_to(x+2, ymid + sheight/2);
		tlayout->update_from_cairo_context(cr);
		tlayout->show_in_cairo_context(cr);

		//Draw fine ticks
		for(int tick=1; tick < nsubticks; tick++)
		{
			double subx = (t + tick*subtick) * m_group->m_pixelsPerPicosecond;

			cr->move_to(subx, ytop);
			cr->line_to(subx, ytop + 10);
			cr->stroke();
		}
	}

	//Draw cursor positions if requested
	Gdk::Color yellow("yellow");
	Gdk::Color orange("orange");

	if( (m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL) ||
		(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_SINGLE) )
	{
		//Draw first vertical cursor
		double x = m_group->m_xCursorPos[0] * m_group->m_pixelsPerPicosecond;
		cr->move_to(x, 0);
		cr->line_to(x, h);
		cr->set_source_rgb(yellow.get_red_p(), yellow.get_green_p(), yellow.get_blue_p());
		cr->stroke();

		//Dual cursors
		if(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL)
		{
			//Draw second vertical cursor
			double x2 = m_group->m_xCursorPos[1] * m_group->m_pixelsPerPicosecond;
			cr->move_to(x2, 0);
			cr->line_to(x2, h);
			cr->set_source_rgb(orange.get_red_p(), orange.get_green_p(), orange.get_blue_p());
			cr->stroke();

			//Draw filled area between them
			cr->set_source_rgba(yellow.get_red_p(), yellow.get_green_p(), yellow.get_blue_p(), 0.2);
			cr->move_to(x, 0);
			cr->line_to(x2, 0);
			cr->line_to(x2, h);
			cr->line_to(x, h);
			cr->fill();
		}
	}

	cr->restore();
	return true;
}
