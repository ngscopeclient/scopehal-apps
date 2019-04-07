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

using namespace std;
using namespace glm;

/*
static const RGBQUAD g_eyeColorScale[256] =
{
	{   0,   0,   0, 0   },     {   4,   2,  20, 255 },     {   7,   4,  35, 255 },     {   9,   5,  45, 255 },
    {  10,   6,  53, 255 },     {  11,   7,  60, 255 },     {  13,   7,  66, 255 },     {  14,   8,  71, 255 },
    {  14,   8,  75, 255 },     {  16,  10,  80, 255 },     {  16,  10,  85, 255 },     {  17,  10,  88, 255 },
    {  18,  11,  92, 255 },     {  19,  11,  95, 255 },     {  19,  12,  98, 255 },     {  20,  12, 102, 255 },
    {  20,  13, 104, 255 },     {  20,  13, 107, 255 },     {  21,  13, 110, 255 },     {  21,  13, 112, 255 },
    {  23,  14, 114, 255 },     {  23,  14, 117, 255 },     {  23,  14, 118, 255 },     {  23,  14, 121, 255 },
    {  23,  15, 122, 255 },     {  24,  15, 124, 255 },     {  24,  15, 126, 255 },     {  24,  14, 127, 255 },
    {  25,  15, 129, 255 },     {  25,  15, 130, 255 },     {  25,  16, 131, 255 },     {  26,  16, 132, 255 },
    {  26,  15, 134, 255 },     {  27,  16, 136, 255 },     {  26,  16, 136, 255 },     {  26,  16, 137, 255 },
    {  27,  16, 138, 255 },     {  26,  16, 138, 255 },     {  26,  16, 140, 255 },     {  27,  16, 141, 255 },
    {  27,  16, 141, 255 },     {  28,  17, 142, 255 },     {  27,  17, 142, 255 },     {  27,  16, 143, 255 },
    {  28,  17, 144, 255 },     {  28,  17, 144, 255 },     {  28,  17, 144, 255 },     {  28,  17, 144, 255 },
    {  28,  17, 144, 255 },     {  28,  17, 145, 255 },     {  28,  17, 145, 255 },     {  28,  17, 145, 255 },
    {  28,  17, 145, 255 },     {  30,  17, 144, 255 },     {  32,  17, 143, 255 },     {  34,  17, 142, 255 },
    {  35,  16, 140, 255 },     {  37,  17, 139, 255 },     {  38,  16, 138, 255 },     {  40,  17, 136, 255 },
    {  42,  16, 136, 255 },     {  44,  16, 134, 255 },     {  46,  17, 133, 255 },     {  47,  16, 133, 255 },
    {  49,  16, 131, 255 },     {  51,  16, 130, 255 },     {  53,  17, 129, 255 },     {  54,  16, 128, 255 },
    {  56,  16, 127, 255 },     {  58,  16, 126, 255 },     {  60,  16, 125, 255 },     {  62,  16, 123, 255 },
    {  63,  16, 122, 255 },     {  65,  16, 121, 255 },     {  67,  16, 120, 255 },     {  69,  16, 119, 255 },
    {  70,  16, 117, 255 },     {  72,  16, 116, 255 },     {  74,  16, 115, 255 },     {  75,  15, 114, 255 },
    {  78,  16, 113, 255 },     {  79,  16, 112, 255 },     {  81,  16, 110, 255 },     {  83,  15, 110, 255 },
    {  84,  15, 108, 255 },     {  86,  16, 108, 255 },     {  88,  15, 106, 255 },     {  90,  15, 105, 255 },
    {  91,  16, 103, 255 },     {  93,  15, 103, 255 },     {  95,  15, 102, 255 },     {  96,  15, 100, 255 },
    {  98,  15, 100, 255 },     { 100,  15,  98, 255 },     { 101,  15,  97, 255 },     { 104,  15,  96, 255 },
    { 106,  15,  95, 255 },     { 107,  15,  94, 255 },     { 109,  14,  92, 255 },     { 111,  14,  92, 255 },
    { 112,  15,  90, 255 },     { 114,  14,  89, 255 },     { 116,  15,  87, 255 },     { 118,  14,  87, 255 },
    { 119,  14,  86, 255 },     { 121,  14,  85, 255 },     { 123,  14,  83, 255 },     { 124,  14,  83, 255 },
    { 126,  15,  81, 255 },     { 128,  14,  80, 255 },     { 130,  14,  78, 255 },     { 132,  14,  77, 255 },
    { 134,  14,  76, 255 },     { 137,  14,  74, 255 },     { 139,  14,  73, 255 },     { 141,  14,  71, 255 },
    { 143,  13,  70, 255 },     { 146,  13,  68, 255 },     { 148,  14,  67, 255 },     { 150,  13,  65, 255 },
    { 153,  14,  64, 255 },     { 155,  14,  62, 255 },     { 157,  13,  61, 255 },     { 159,  13,  60, 255 },
    { 162,  13,  58, 255 },     { 165,  13,  56, 255 },     { 166,  13,  55, 255 },     { 169,  13,  54, 255 },
    { 171,  13,  52, 255 },     { 173,  13,  50, 255 },     { 176,  13,  48, 255 },     { 179,  12,  47, 255 },
    { 181,  12,  45, 255 },     { 183,  12,  45, 255 },     { 185,  12,  43, 255 },     { 188,  13,  41, 255 },
    { 190,  12,  40, 255 },     { 192,  12,  38, 255 },     { 194,  13,  37, 255 },     { 197,  12,  35, 255 },
    { 199,  12,  33, 255 },     { 201,  12,  32, 255 },     { 204,  12,  30, 255 },     { 206,  12,  29, 255 },
    { 209,  12,  28, 255 },     { 211,  12,  26, 255 },     { 213,  12,  25, 255 },     { 216,  12,  23, 255 },
    { 218,  11,  22, 255 },     { 221,  12,  20, 255 },     { 223,  11,  18, 255 },     { 224,  11,  17, 255 },
    { 227,  11,  16, 255 },     { 230,  11,  14, 255 },     { 231,  11,  12, 255 },     { 234,  12,  11, 255 },
    { 235,  13,  10, 255 },     { 235,  15,  11, 255 },     { 235,  17,  11, 255 },     { 235,  19,  11, 255 },
    { 236,  21,  10, 255 },     { 236,  23,  10, 255 },     { 237,  24,  10, 255 },     { 237,  26,  10, 255 },
    { 236,  28,   9, 255 },     { 237,  30,  10, 255 },     { 237,  32,   9, 255 },     { 238,  34,   9, 255 },
    { 238,  35,   9, 255 },     { 238,  38,   8, 255 },     { 239,  39,   9, 255 },     { 239,  42,   8, 255 },
    { 240,  44,   9, 255 },     { 240,  45,   8, 255 },     { 240,  47,   8, 255 },     { 240,  49,   8, 255 },
    { 241,  51,   7, 255 },     { 241,  53,   8, 255 },     { 241,  55,   7, 255 },     { 241,  57,   7, 255 },
    { 242,  58,   7, 255 },     { 242,  60,   7, 255 },     { 242,  62,   6, 255 },     { 243,  64,   6, 255 },
    { 244,  66,   6, 255 },     { 243,  68,   5, 255 },     { 244,  69,   6, 255 },     { 244,  71,   6, 255 },
    { 245,  74,   6, 255 },     { 245,  76,   5, 255 },     { 245,  79,   5, 255 },     { 246,  82,   5, 255 },
    { 246,  85,   5, 255 },     { 247,  87,   4, 255 },     { 247,  90,   4, 255 },     { 248,  93,   3, 255 },
    { 249,  96,   4, 255 },     { 248,  99,   3, 255 },     { 249, 102,   3, 255 },     { 250, 105,   3, 255 },
    { 250, 107,   2, 255 },     { 250, 110,   2, 255 },     { 251, 113,   2, 255 },     { 252, 115,   1, 255 },
    { 252, 118,   2, 255 },     { 253, 121,   1, 255 },     { 253, 124,   1, 255 },     { 253, 126,   1, 255 },
    { 254, 129,   0, 255 },     { 255, 132,   0, 255 },     { 255, 135,   0, 255 },     { 255, 138,   1, 255 },
    { 254, 142,   3, 255 },     { 253, 145,   4, 255 },     { 253, 148,   6, 255 },     { 252, 151,   9, 255 },
    { 252, 155,  11, 255 },     { 251, 158,  12, 255 },     { 251, 161,  14, 255 },     { 250, 163,  15, 255 },
    { 251, 165,  16, 255 },     { 250, 167,  17, 255 },     { 250, 169,  18, 255 },     { 250, 170,  19, 255 },
    { 250, 172,  20, 255 },     { 249, 174,  21, 255 },     { 249, 177,  22, 255 },     { 248, 178,  23, 255 },
    { 248, 180,  24, 255 },     { 247, 182,  25, 255 },     { 247, 184,  26, 255 },     { 247, 185,  27, 255 },
    { 247, 188,  27, 255 },     { 247, 191,  26, 255 },     { 248, 194,  25, 255 },     { 249, 197,  24, 255 },
    { 248, 200,  22, 255 },     { 249, 203,  21, 255 },     { 249, 205,  20, 255 },     { 250, 209,  18, 255 },
    { 250, 212,  18, 255 },     { 250, 214,  16, 255 },     { 251, 217,  15, 255 },     { 251, 221,  14, 255 },
    { 251, 223,  13, 255 },     { 251, 226,  12, 255 },     { 252, 229,  11, 255 },     { 253, 231,   9, 255 },
    { 253, 234,   9, 255 },     { 253, 237,   7, 255 },     { 253, 240,   6, 255 },     { 253, 243,   5, 255 },
    { 254, 246,   4, 255 },     { 254, 248,   3, 255 },     { 255, 251,   1, 255 },     { 255, 254,   1, 255 }
};
*/

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

//TODO: only do this if the waveform is dirty!
//TODO: Tesselate in a geometry shader, rather than on the CPU!
bool WaveformArea::PrepareGeometry()
{
	//LogDebug("Processing capture\n");
	LogIndenter li;

	auto dat = m_channel->GetData();
	if(!dat)
		return false;

	AnalogCapture& data = *dynamic_cast<AnalogCapture*>(dat);
	size_t count = data.size();

	//Pull vertical size from the scope
	m_pixelsPerVolt = m_height / m_channel->GetVoltageRange();

	//Scaling factor from samples to pixels
	float xscale = m_group->m_pixelsPerPicosecond * dat->m_timescale;
	float xoff = m_group->m_pixelsPerPicosecond * dat->m_triggerPhase;

	//Create the geometry
	size_t waveform_size = count * 12;	//3 points * 2 triangles * 2 coordinates
	double lheight = 1.0;	//pixels
	float* verts = new float[waveform_size];
	double offset = m_channel->GetOffset();
	//#pragma omp parallel for
	for(size_t j=0; j<(count-1); j++)
	{
		//Actual X start/end point of the data
		float xleft = data.GetSampleStart(j) * xscale + xoff;
		float xright = data.GetSampleStart(j+1) * xscale + xoff;

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
		float yleft = (data[j] + offset) * m_pixelsPerVolt;
		float yright = (data[j+1] + offset) * m_pixelsPerVolt;

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

bool WaveformArea::on_render(const Glib::RefPtr<Gdk::GLContext>& /*context*/)
{
	double start = GetTime();
	double dt = start - m_lastFrameStart;
	if(m_lastFrameStart > 0)
	{
		//LogDebug("Frame time: %.3f ms (%.2f FPS)\n", dt*1000, 1/dt);
		m_frameTime += dt;
		m_frameCount ++;
	}
	m_lastFrameStart = start;

	//Everything we draw is 2D painter's algorithm.
	//Turn off some stuff we don't need, but leave blending on.
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_MULTISAMPLE);
	glDisable(GL_FRAMEBUFFER_SRGB);
	glDisable(GL_CULL_FACE);

	//No texture filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	//Render the Cairo layers with the GL waveform sandwiched in between
	RenderPersistenceOverlay();
	if(PrepareGeometry())
		RenderTrace();
	RenderCairoUnderlays();
	RenderTraceColorCorrection();
	RenderCairoOverlays();

	//Sanity check
	int err = glGetError();
	if(err != 0)
		LogNotice("err = %x\n", err);

	return true;
}

void WaveformArea::RenderPersistenceOverlay()
{
	//Draw to the offscreen floating-point framebuffer.
	m_waveformFramebuffer.Bind(GL_FRAMEBUFFER);

	//If not persisting, just wipe out whatever was there before
	if(!m_persistence || m_persistenceClear)
	{
		m_persistenceClear = false;
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
		return;
	}

	//Configure blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
	glBlendColor(0, 0, 0, 0.1);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);

	//Draw a black overlay with a little bit of alpha (to make old traces decay)
	m_persistProgram.Bind();
	m_persistVAO.Bind();
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void WaveformArea::RenderTrace()
{
	//Configure our shader and projection matrix
	m_waveformProgram.Bind();
	m_waveformProgram.SetUniform(m_projection, "projection");
	m_waveformProgram.SetUniform(0.0f, "xoff");
	m_waveformProgram.SetUniform(1.0, "xscale");
	m_waveformProgram.SetUniform(m_height / 2, "yoff");
	m_waveformProgram.SetUniform(1, "yscale");

	glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
	glBlendColor(0, 0, 0, 0.2);
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
	//No blending since we're the first thing to hit the window framebuffer
	m_windowFramebuffer.Bind(GL_FRAMEBUFFER);
	glDisable(GL_BLEND);

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

	//Get the image data and make a texture from it
	m_cairoTexture.Bind();
	m_cairoTexture.SetData(
		m_width,
		m_height,
		surface->get_data(),
		GL_BGRA);

	//Draw the actual image
	m_cairoProgram.Bind();
	m_cairoVAO.Bind();
	m_cairoProgram.SetUniform(m_cairoTexture, "fbtex");
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
	float ymid = halfheight + ybot;

	//Volts from the center line of our graph to the top. May not be the max value in the signal.
	float volts_per_half_span = PixelsToVolts(halfheight);

	//Decide what voltage step to use. Pick from a list (in volts)
	float selected_step = AnalogRenderer::PickStepSize(volts_per_half_span);

	//Calculate grid positions
	std::map<float, float> gridmap;
	gridmap.clear();
	gridmap[0] = 0;
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
	cr->move_to(0, ymid);
	cr->line_to(m_plotRight, ymid);
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
	cr->fill();

	DoRenderCairoOverlays(cr);

	//Get the image data and make a texture from it
	m_cairoTexture.Bind();
	m_cairoTexture.SetData(
		m_width,
		m_height,
		surface->get_data(),
		GL_BGRA);

	//Configure blending for Cairo's premultiplied alpha
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	//Draw the actual image
	m_windowFramebuffer.Bind(GL_FRAMEBUFFER);
	m_cairoProgram.Bind();
	m_cairoVAO.Bind();
	m_cairoProgram.SetUniform(m_cairoTexture, "fbtex");
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void WaveformArea::DoRenderCairoOverlays(Cairo::RefPtr< Cairo::Context > cr)
{
	RenderChannelLabel(cr);
	RenderCursors(cr);
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
		double x = m_group->m_xCursorPos[0] * m_group->m_pixelsPerPicosecond;
		cr->move_to(x, ytop);
		cr->line_to(x, ybot);
		cr->set_source_rgb(yellow.get_red_p(), yellow.get_green_p(), yellow.get_blue_p());
		cr->stroke();

		//Dual cursors
		if(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL)
		{
			//Draw second vertical cursor
			double x2 = m_group->m_xCursorPos[1] * m_group->m_pixelsPerPicosecond;
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
