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

/**
	@file
	@author Andrew D. Zonenberg
	@brief  Rendering code for WaveformArea
 */

#include "glscopeclient.h"
#include "WaveformArea.h"
#include "OscilloscopeWindow.h"
#include <random>
#include "ProfileBlock.h"
#include "../../lib/scopehal/TextRenderer.h"
#include "../../lib/scopehal/DigitalRenderer.h"
#include "../../lib/scopeprotocols/EyeDecoder2.h"

using namespace std;
using namespace glm;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

//TODO: only do this if the waveform is dirty!
//TODO: Tesselate in a geometry shader, rather than on the CPU!
bool WaveformArea::PrepareGeometry()
{
	//LogDebug("Processing capture\n");
	LogIndenter li;

	auto pdat = dynamic_cast<AnalogCapture*>(m_channel->GetData());
	if(!pdat)
		return false;
	AnalogCapture& data = *pdat;
	size_t count = data.size();
	if(count == 0)
		return false;

	//Create the geometry
	size_t waveform_size = count * 12;	//3 points * 2 triangles * 2 coordinates
	double lheight = 1.0;	//pixels
	float* verts = new float[waveform_size];
	double offset = m_channel->GetOffset();
	//#pragma omp parallel for
	for(size_t j=0; j<(count-1); j++)
	{
		//Actual X start/end point of the data
		float xleft = PicosecondsToXPosition(
			(data.GetSampleStart(j) * pdat->m_timescale) + pdat->m_triggerPhase);
		float xright = PicosecondsToXPosition(
			(data.GetSampleStart(j+1) * pdat->m_timescale) + pdat->m_triggerPhase);

		//TODO: if a triangle is <1 pixel wide, don't stretch. Merge it with the adjacent one(s)

		//If the triangle would be degenerate horizontally (less than one pixel wide), stretch it
		float width = xright-xleft;
		float minwidth = 2;
		if(width < minwidth)
		{
			float xmid = width/2 + xleft;

			xleft = xmid - minwidth/2;
			xright = xmid + minwidth/2;
		}

		//Actual Y start point/end of the data
		float yleft = VoltsToPixels(data[j] + offset);
		float yright = VoltsToPixels(data[j+1] + offset);

		//If the triangle doesn't touch the next one, stretch vertically? this SHOULD not be possible,
		//but rendering shows that something is causing stuff to not touch vertically

		//Rather than using a generalized line drawing algorithm, we can cheat since we know the points are
		//always left to right, sorted, and never vertical. Just add some height to the samples!
		size_t voff = j*12;
		verts[voff++] = xleft;
		verts[voff++] = yleft + lheight;

		verts[voff++] = xleft;
		verts[voff++] = yleft - lheight;

		verts[voff++] = xright;
		verts[voff++] = yright - lheight;

		verts[voff++] = xright;
		verts[voff++] = yright - lheight;

		verts[voff++] = xright;
		verts[voff++] = yright + lheight;

		verts[voff++] = xleft;
		verts[voff++] = yleft + lheight;
	}

	//Download waveform data
	m_traceVBOs[0]->Bind();
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * waveform_size, verts, GL_DYNAMIC_DRAW);

	//Configure vertex array settings
	m_traceVAOs[0]->Bind();
	m_waveformProgram.EnableVertexArray("vert");
	m_waveformProgram.SetVertexAttribPointer("vert", 2, 0);

	m_waveformLength = count;

	//Cleanup time
	delete[] verts;

	return true;
}

void WaveformArea::ResetTextureFiltering()
{
	//No texture filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

bool WaveformArea::on_render(const Glib::RefPtr<Gdk::GLContext>& /*context*/)
{
	double start = GetTime();
	//LogDebug("[%f] Rendering %s\n", start, m_channel->m_displayname.c_str());
	LogIndenter li;
	double dt = start - m_lastFrameStart;
	if(m_lastFrameStart > 0)
	{
		//LogDebug("Frame time: %.3f ms (%.2f FPS)\n", dt*1000, 1/dt);
		m_frameTime += dt;
		m_frameCount ++;
	}
	m_lastFrameStart = start;

	//Pull vertical size from the scope early on no matter how we're rendering
	m_pixelsPerVolt = m_height / m_channel->GetVoltageRange();

	//Everything we draw is 2D painter's algorithm.
	//Turn off some stuff we don't need, but leave blending on.
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	//Do persistence processing
	if(!m_persistence || m_persistenceClear)
	{
		m_waveformFramebuffer.Bind(GL_FRAMEBUFFER);
		m_persistenceClear = false;
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	else
		RenderPersistenceOverlay();

	bool is_eye = dynamic_cast<EyeDecoder2*>(m_channel) != NULL;

	//Render the Cairo layers with the GL waveform sandwiched in between
	RenderCairoUnderlays();
	if(is_eye)
		RenderEye();
	else if(PrepareGeometry())
	{
		RenderTrace();
		RenderTraceColorCorrection();
	}
	RenderCairoOverlays();

	//Sanity check
	int err = glGetError();
	if(err != 0)
		LogNotice("err = %x\n", err);

	return true;
}

void WaveformArea::RenderEye()
{
	auto peye = dynamic_cast<EyeDecoder2*>(m_channel);
	auto pcap = dynamic_cast<EyeCapture2*>(m_channel->GetData());
	if(peye == NULL)
		return;
	if(pcap == NULL)
		return;

	//It's an eye pattern! Just copy it directly into the waveform texture.
	m_eyeTexture.Bind();
	ResetTextureFiltering();
	m_eyeTexture.SetData(
		peye->GetWidth(),
		peye->GetHeight(),
		pcap->GetData(),
		GL_RED,
		GL_FLOAT,
		GL_RGBA32F);

	//Drawing to the window
	m_windowFramebuffer.Bind(GL_FRAMEBUFFER);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);

	m_eyeProgram.Bind();
	m_eyeVAO.Bind();
	m_eyeProgram.SetUniform(m_eyeTexture, "fbtex", 0);
	m_eyeProgram.SetUniform(m_eyeColorRamp[m_parent->GetEyeColor()], "ramp", 1);

	//Only look at stuff inside the plot area
	glEnable(GL_SCISSOR_TEST);
	glScissor(0, 0, m_plotRight, m_height);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glDisable(GL_SCISSOR_TEST);
	glActiveTexture(GL_TEXTURE0);
}

void WaveformArea::RenderPersistenceOverlay()
{
	m_waveformFramebuffer.Bind(GL_FRAMEBUFFER);

	//Configure blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
	glBlendColor(0, 0, 0, 0.01);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);

	//Draw a black overlay with a little bit of alpha (to make old traces decay)
	m_persistProgram.Bind();
	m_persistVAO.Bind();
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void WaveformArea::RenderTrace()
{
	m_waveformFramebuffer.Bind(GL_FRAMEBUFFER);

	//Configure our shader and projection matrix
	m_waveformProgram.Bind();
	m_waveformProgram.SetUniform(m_projection, "projection");
	m_waveformProgram.SetUniform(0.0f, "xoff");
	m_waveformProgram.SetUniform(1.0, "xscale");
	m_waveformProgram.SetUniform(m_height / 2, "yoff");
	m_waveformProgram.SetUniform(1, "yscale");

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);

	//Only look at stuff inside the plot area
	glEnable(GL_SCISSOR_TEST);
	glScissor(0, 0, m_plotRight, m_height);

	//Actually draw the waveform
	m_traceVAOs[0]->Bind();

	/*vector<int> firsts;
	vector<int> counts;
	firsts.push_back(0);
	counts.push_back(2*m_waveformLength);
	glMultiDrawArrays(GL_TRIANGLE_STRIP, &firsts[0], &counts[0], 1);
	*/
	glDrawArrays(GL_TRIANGLES, 0, 12*m_waveformLength);

	glDisable(GL_SCISSOR_TEST);
}

void WaveformArea::RenderCairoUnderlays()
{
	//Create the Cairo surface we're drawing on
	Cairo::RefPtr< Cairo::ImageSurface > surface =
		Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, m_width, m_height);
	Cairo::RefPtr< Cairo::Context > cr = Cairo::Context::create(surface);

	//Set up transformation to match GL's bottom-left origin
	cr->translate(0, m_height);
	cr->scale(1, -1);

	//Clear to a blank background
	cr->set_source_rgba(0, 0, 0, 1);
	cr->rectangle(0, 0, m_width, m_height);
	cr->fill();

	DoRenderCairoUnderlays(cr);

	//No blending since we're the first thing to hit the window framebuffer
	m_windowFramebuffer.Bind(GL_FRAMEBUFFER);
	glDisable(GL_BLEND);

	//Draw the actual image
	m_cairoProgram.Bind();
	m_cairoVAO.Bind();
	m_cairoProgram.SetUniform(m_cairoTexture, "fbtex");
	m_cairoTexture.Bind();
	ResetTextureFiltering();
	m_cairoTexture.SetData(
		m_width,
		m_height,
		surface->get_data(),
		GL_BGRA);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

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

int64_t WaveformArea::XPositionToPicoseconds(float pix)
{
	return m_group->m_timeOffset + PixelsToPicoseconds(pix);
}

int64_t WaveformArea::PixelsToPicoseconds(float pix)
{
	return pix / m_group->m_pixelsPerPicosecond;
}

float WaveformArea::PicosecondsToPixels(int64_t t)
{
	return t * m_group->m_pixelsPerPicosecond;
}

float WaveformArea::PicosecondsToXPosition(int64_t t)
{
	return PicosecondsToPixels(t - m_group->m_timeOffset);
}

float WaveformArea::PixelsToVolts(float pix)
{
	return pix / m_pixelsPerVolt;
}

float WaveformArea::VoltsToPixels(float volt)
{
	return volt * m_pixelsPerVolt;
}

float WaveformArea::VoltsToYPosition(float volt)
{
	return m_height/2 - VoltsToPixels(volt + m_channel->GetOffset());
}

float WaveformArea::YPositionToVolts(float y)
{
	return PixelsToVolts(-1 * (y - m_height/2) ) - m_channel->GetOffset();
}

void WaveformArea::RenderGrid(Cairo::RefPtr< Cairo::Context > cr)
{
	cr->save();

	Gdk::Color color(m_channel->m_displaycolor);

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

	float ytop = m_height - m_padding;
	float ybot = m_padding;
	float plotheight = m_height - 2*m_padding;
	float halfheight = plotheight/2;
	//float ymid = halfheight + ybot;

	//Volts from the center line of our graph to the top. May not be the max value in the signal.
	float volts_per_half_span = PixelsToVolts(halfheight);

	//Decide what voltage step to use. Pick from a list (in volts)
	float selected_step = AnalogRenderer::PickStepSize(volts_per_half_span);

	//Calculate grid positions
	std::map<float, float> gridmap;
	gridmap.clear();
	for(float dv=0; ; dv += selected_step)
	{
		float yt = VoltsToYPosition(dv);
		float yb = VoltsToYPosition(-dv);

		if(yb <= (ytop - theight/2) )
			gridmap[-dv] = yb;
		if(yt >= (ybot + theight/2) )
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

	//Dimmed lines above and below
	/*vector<double> dashes;
	dashes.push_back(4);
	dashes.push_back(4);
	cr->set_dash(dashes, 0);*/
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
		char tmp[32];

		if(fabs(v) < 1)
			snprintf(tmp, sizeof(tmp), "%.0f mV", v*1000);
		else
			snprintf(tmp, sizeof(tmp), "%.3f V", v);

		float y = it.second - theight/2;
		if(y < ybot)
			continue;
		if(y > ytop)
			continue;

		tlayout->set_text(tmp);
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

void WaveformArea::RenderTraceColorCorrection()
{
	//Drawing to the window
	m_windowFramebuffer.Bind(GL_FRAMEBUFFER);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);

	Gdk::Color color(m_channel->m_displaycolor);

	//Draw the offscreen buffer to the onscreen buffer
	//as a textured quad. Apply color correction as we do this.
	m_colormapProgram.Bind();
	m_colormapVAO.Bind();
	m_colormapProgram.SetUniform(m_waveformTexture, "fbtex");
	m_colormapProgram.SetUniform(color.get_red_p(), "r");
	m_colormapProgram.SetUniform(color.get_green_p(), "g");
	m_colormapProgram.SetUniform(color.get_blue_p(), "b");

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void WaveformArea::RenderCairoOverlays()
{
	//Create the Cairo surface we're drawing on
	Cairo::RefPtr< Cairo::ImageSurface > surface =
		Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, m_width, m_height);
	Cairo::RefPtr< Cairo::Context > cr = Cairo::Context::create(surface);

	//Set up transformation to match GL's bottom-left origin
	cr->translate(0, m_height);
	cr->scale(1, -1);

	//Clear to a blank background
	cr->set_source_rgba(0, 0, 0, 0);
	cr->rectangle(0, 0, m_width, m_height);
	cr->set_operator(Cairo::OPERATOR_SOURCE);
	cr->fill();
	cr->set_operator(Cairo::OPERATOR_OVER);

	DoRenderCairoOverlays(cr);

	//Get the image data and make a texture from it
	m_cairoTextureOver.Bind();
	ResetTextureFiltering();
	m_cairoTextureOver.SetData(
		m_width,
		m_height,
		surface->get_data(),
		GL_BGRA);

	//Configure blending for Cairo's premultiplied alpha
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);

	//Draw the actual image
	m_windowFramebuffer.Bind(GL_FRAMEBUFFER);
	m_cairoProgram.Bind();
	m_cairoVAO.Bind();
	m_cairoProgram.SetUniform(m_cairoTextureOver, "fbtex");
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void WaveformArea::DoRenderCairoOverlays(Cairo::RefPtr< Cairo::Context > cr)
{
	RenderChannelLabel(cr);
	RenderDecodeOverlays(cr);
	RenderCursors(cr);
}

void WaveformArea::RenderDecodeOverlays(Cairo::RefPtr< Cairo::Context > cr)
{
	int midline = 15;
	int height = 20;
	int spacing = 30;

	//Find which overlay slots are in use
	int max_overlays = 10;
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

		double ymid = m_overlayPositions[o];
		double ytop = ymid - height/2;
		double ybot = ymid + height/2;

		//Render the grayed-out background
		cr->set_source_rgba(0,0,0, 0.6);
		cr->move_to(0, 				ytop);
		cr->line_to(m_plotRight, 	ytop);
		cr->line_to(m_plotRight,	ybot);
		cr->line_to(0,				ybot);
		cr->fill();

		//Render the channel label
		cr->set_source_rgba(1,1,1,1);
		int twidth;
		int theight;
		Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create (cr);
		Pango::FontDescription font("monospace normal 10");
		font.set_weight(Pango::WEIGHT_NORMAL);
		tlayout->set_font_description(font);
		tlayout->set_text(o->m_displayname);
		tlayout->get_pixel_size(twidth, theight);
		cr->move_to(5, ybot - theight);
		tlayout->update_from_cairo_context(cr);
		tlayout->show_in_cairo_context(cr);

		float left = twidth + 5;

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

				double xs = PicosecondsToXPosition(start);
				double xe = PicosecondsToXPosition(end);

				if( (xe < 0) || (xs > m_plotRight) )
					continue;

				auto text = tr->GetText(i);
				auto color = tr->GetColor(i);

				render->RenderComplexSignal(
					cr,
					left, m_plotRight,
					xs, xe, 5,
					ybot, ymid, ytop,
					text,
					color);
			}
		}

		//Handle digital
		auto dr = dynamic_cast<DigitalRenderer*>(render);
		Gdk::Color color(o->m_displaycolor);
		cr->set_source_rgb(color.get_red_p(), color.get_green_p(), color.get_blue_p());
		bool first = true;
		if(dr != NULL)
		{
			auto ddat = dynamic_cast<DigitalCapture*>(data);
			for(size_t i=0; i<data->GetDepth(); i++)
			{
				double start = (data->GetSampleStart(i) * data->m_timescale) + data->m_triggerPhase;
				double end = start + (data->GetSampleLen(i) * data->m_timescale);

				double xs = PicosecondsToXPosition(start);
				double xe = PicosecondsToXPosition(end);

				if( (xs < left) || (xe > m_plotRight) )
					continue;

				double y = ybot;
				if((*ddat)[i])
					y = ytop;

				//start of sample
				if(first)
				{
					cr->move_to(xs, y);
					first = false;
				}
				else
					cr->line_to(xs, y);

				//end of sample
				cr->line_to(xe, y);
			}
			cr->stroke();
		}

		delete render;
	}
}

void WaveformArea::RenderChannelLabel(Cairo::RefPtr< Cairo::Context > cr)
{
	auto ybot = m_height;

	int twidth;
	int theight;
	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create (cr);
	Pango::FontDescription font("sans normal 10");
	font.set_weight(Pango::WEIGHT_NORMAL);
	tlayout->set_font_description(font);
	tlayout->set_text(m_channel->m_displayname);
	tlayout->get_pixel_size(twidth, theight);

	//Black background
	int labelmargin = 2;
	cr->set_source_rgba(0, 0, 0, 0.75);
	cr->rectangle(0, ybot - theight - labelmargin*2, twidth + labelmargin*2, theight + labelmargin*2);
	cr->fill();

	//White text
	cr->save();
		cr->set_source_rgba(1, 1, 1, 1);
		cr->move_to(labelmargin, ybot - theight - labelmargin);
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
		double x = PicosecondsToXPosition(m_group->m_xCursorPos[0]);
		cr->move_to(x, ytop);
		cr->line_to(x, ybot);
		cr->set_source_rgb(yellow.get_red_p(), yellow.get_green_p(), yellow.get_blue_p());
		cr->stroke();

		//Dual cursors
		if(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL)
		{
			//Draw second vertical cursor
			double x2 = PicosecondsToXPosition(m_group->m_xCursorPos[1]);
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
