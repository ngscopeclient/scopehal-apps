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
	@brief  OpenGL rendering code for WaveformArea
 */

#include "glscopeclient.h"
#include "WaveformArea.h"
#include "OscilloscopeWindow.h"
#include <random>
#include <map>
#include <immintrin.h>
#include "ProfileBlock.h"
#include "../../lib/scopeprotocols/EyePattern.h"
#include "../../lib/scopeprotocols/Waterfall.h"

using namespace std;

template size_t WaveformArea::BinarySearchForGequal<float>(float* buf, size_t len, float value);
template size_t WaveformArea::BinarySearchForGequal<int64_t>(int64_t* buf, size_t len, int64_t value);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WaveformRenderData

void WaveformRenderData::MapBuffers(size_t width)
{
	//Calculate the number of points we'll need to draw. Default to 1 if no data
	if( (m_channel.m_channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL) &&
		(m_channel.m_channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_ANALOG))
	{
		m_count = 1;
	}
	else
	{
		auto pdat = m_channel.GetData();
		if( (pdat == NULL) || ((m_count = pdat->m_offsets.size()) == 0) )
			m_count = 1;
	}

	m_mappedXBuffer = (int64_t*)m_waveformXBuffer.Map(m_count*sizeof(int64_t), GL_READ_WRITE);
	if(IsDigital())
	{
		//round up to next multiple of 4 since buffer is actually made of int32's
		m_mappedDigitalYBuffer = (bool*)m_waveformYBuffer.Map((m_count*sizeof(bool) | 3) + 1);
		m_mappedYBuffer = NULL;
	}
	else
	{
		m_mappedYBuffer = (float*)m_waveformYBuffer.Map(m_count*sizeof(float));
		m_mappedDigitalYBuffer = NULL;
	}
	m_mappedIndexBuffer = (uint32_t*)m_waveformIndexBuffer.Map(width*sizeof(uint32_t));
	m_mappedConfigBuffer = (uint32_t*)m_waveformConfigBuffer.Map(sizeof(float)*11);
	m_mappedFloatConfigBuffer = (float*)m_mappedConfigBuffer;
	m_mappedConfigBuffer64 = (int64_t*)m_mappedConfigBuffer;
}

void WaveformRenderData::UnmapBuffers()
{
	m_waveformXBuffer.Unmap();
	m_waveformYBuffer.Unmap();
	m_waveformIndexBuffer.Unmap();
	m_waveformConfigBuffer.Unmap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

void WaveformArea::PrepareGeometry(WaveformRenderData* wdata)
{
	double start = GetTime();

	//We need analog or digital data to render
	auto channel = wdata->m_channel.m_channel;
	if( (channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL) &&
		(channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_ANALOG))
	{
		wdata->m_geometryOK = false;
		return;
	}
	auto pdat = wdata->m_channel.GetData();
	if( (pdat == NULL) || pdat->m_offsets.empty() )
	{
		wdata->m_geometryOK = false;
		return;
	}

	//Make sure capture is the right type
	auto andat = dynamic_cast<AnalogWaveform*>(pdat);
	auto digdat = dynamic_cast<DigitalWaveform*>(pdat);
	if(!andat && !digdat)
	{
		wdata->m_geometryOK = false;
		return;
	}

	//Zero voltage level
	//TODO: properly calculate overlay positions once RenderDecodeOverlays() isn't doing that anymore
	float ybase = m_height/2;
	if(digdat)
	{
		//Main channel
		if(wdata->m_channel == m_channel)
			ybase = 2;

		//Overlay
		else
			ybase = m_height - (m_overlayPositions[wdata->m_channel] + 10);
	}

	//Y axis scaling in shader
	float yscale = 1;
	if(digdat)
	{
		float digheight;
		if(wdata->m_channel == m_channel)
			digheight = m_height - 5;
		else
			digheight = 20;

		yscale = digheight;
		memcpy(wdata->m_mappedDigitalYBuffer, &digdat->m_samples[0], wdata->m_count*sizeof(bool));
	}
	else
	{
		yscale = m_pixelsPerVolt;

		//Copy the waveform
		memcpy(wdata->m_mappedYBuffer, &andat->m_samples[0], wdata->m_count*sizeof(float));
	}

	//Copy the X axis timestamps, no conversion needed
	memcpy(wdata->m_mappedXBuffer, &pdat->m_offsets[0], wdata->m_count*sizeof(int64_t));

	double dt = GetTime() - start;
	m_prepareTime += dt;
	start = GetTime();

	//Calculate indexes for rendering.
	//This is necessary since samples may be sparse and have arbitrary spacing between them, so we can't
	//trivially map sample indexes to X pixel coordinates.
	//TODO: can we parallelize this? move to a compute shader?
	for(int j=0; j<m_width; j++)
	{
		wdata->m_mappedIndexBuffer[j] = BinarySearchForGequal(
			wdata->m_mappedXBuffer,
			wdata->m_count,
			(j + m_group->m_xAxisOffset) / pdat->m_timescale);
	}

	dt = GetTime() - start;
	m_indexTime += dt;

	//Scale alpha by zoom.
	//As we zoom out more, reduce alpha to get proper intensity grading
	float samplesPerPixel = 1.0f / (m_group->m_pixelsPerXUnit * pdat->m_timescale);
	float alpha_scaled = m_parent->GetTraceAlpha() * 2 / samplesPerPixel;

	//Config stuff
	//TODO: we should be able to only update this stuff if we pan/zoom, without redoing the waveform data itself
	wdata->m_mappedConfigBuffer64[0] = -m_group->m_xAxisOffset / pdat->m_timescale;			//innerXoff
	wdata->m_mappedConfigBuffer[2] = m_height;												//windowHeight
	wdata->m_mappedConfigBuffer[3] = m_plotRight;											//windowWidth
	wdata->m_mappedConfigBuffer[4] = wdata->m_count;										//depth
	wdata->m_mappedFloatConfigBuffer[5] = alpha_scaled;										//alpha
	wdata->m_mappedFloatConfigBuffer[6] = pdat->m_triggerPhase * m_group->m_pixelsPerXUnit;	//xoff
	wdata->m_mappedFloatConfigBuffer[7] = pdat->m_timescale * m_group->m_pixelsPerXUnit;	//xscale
	wdata->m_mappedFloatConfigBuffer[8] = ybase;											//ybase
	wdata->m_mappedFloatConfigBuffer[9] = yscale;											//yscale
	wdata->m_mappedFloatConfigBuffer[10] = channel->GetOffset();							//yoff

	//Done
	wdata->m_geometryOK = true;
}

/**
	@brief Look for a value greater than or equal to "value" in buf and return the index
 */
template<class T>
size_t WaveformArea::BinarySearchForGequal(T* buf, size_t len, T value)
{
	size_t pos = len/2;
	size_t last_lo = 0;
	size_t last_hi = len-1;

	//Clip if out of range
	if(buf[0] >= value)
		return 0;
	if(buf[last_hi] < value)
		return len;

	while(true)
	{
		LogIndenter li;

		//Stop if we've bracketed the target
		if( (last_hi - last_lo) <= 1)
			break;

		//Move down
		if(buf[pos] > value)
		{
			size_t delta = pos - last_lo;
			last_hi = pos;
			pos = last_lo + delta/2;
		}

		//Move up
		else
		{
			size_t delta = last_hi - pos;
			last_lo = pos;
			pos = last_hi - delta/2;
		}
	}

	return last_lo;
}

void WaveformArea::ResetTextureFiltering()
{
	//No texture filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void WaveformArea::PrepareAllGeometry()
{
	m_geometryDirty = false;

	//Main waveform
	if(IsAnalog() || IsDigital())
		PrepareGeometry(m_waveformRenderData);

	for(auto overlay : m_overlays)
	{
		if(overlay.m_channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
			continue;

		if(m_overlayRenderData.find(overlay) != m_overlayRenderData.end())
			PrepareGeometry(m_overlayRenderData[overlay]);
	}
}

void WaveformArea::MapAllBuffers()
{
	make_current();

	//Main waveform
	if(IsAnalog() || IsDigital())
		m_waveformRenderData->MapBuffers(m_width);

	for(auto overlay : m_overlays)
	{
		if(overlay.m_channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
			continue;

		if(m_overlayRenderData.find(overlay) != m_overlayRenderData.end())
			m_overlayRenderData[overlay]->MapBuffers(m_width);
	}
}

void WaveformArea::UnmapAllBuffers()
{
	make_current();

	//Main waveform
	if(IsAnalog() || IsDigital())
		m_waveformRenderData->UnmapBuffers();

	for(auto overlay : m_overlays)
	{
		if(overlay.m_channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
			continue;

		if(m_overlayRenderData.find(overlay) != m_overlayRenderData.end())
			m_overlayRenderData[overlay]->UnmapBuffers();
	}
}

bool WaveformArea::on_render(const Glib::RefPtr<Gdk::GLContext>& /*context*/)
{
	//If a file load is in progress don't waste time on expensive render calls.
	//Many render events get dispatched as various parts of the UI config and waveform data load,
	//and we only want to actually draw on the very last one.
	if(m_parent->IsLoadInProgress())
		return true;

	LogIndenter li;

	double start = GetTime();
	double dt = start - m_lastFrameStart;
	if(m_lastFrameStart > 0)
	{
		//LogDebug("Inter-frame time: %.3f ms (%.2f FPS)\n", dt*1000, 1/dt);
		m_frameTime += dt;
		m_frameCount ++;
	}
	m_lastFrameStart = start;

	//Update geometry if needed
	if(m_geometryDirty)
	{
		MapAllBuffers();
		PrepareAllGeometry();
		UnmapAllBuffers();
	}

	//Everything we draw is 2D painter's algorithm.
	//Turn off some stuff we don't need, but leave blending on.
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	//On the first frame, figure out what the actual screen surface FBO is.
	if(m_firstFrame)
	{
		m_windowFramebuffer.InitializeFromCurrentFramebuffer();
		m_firstFrame = false;
	}

	//Pull vertical size from the scope early on no matter how we're rendering
	m_pixelsPerVolt = m_height / m_channel.m_channel->GetVoltageRange();

	//TODO: Do persistence processing

	/*
	if(!m_persistence || m_persistenceClear)
	{
		m_persistenceClear = false;
	}
	else
		RenderPersistenceOverlay();
	*/

	//Draw the main waveform
	if(IsAnalog() || IsDigital())
		RenderTrace(m_waveformRenderData);

	//Launch software rendering passes and push the resulting data to the GPU
	ComputeAndDownloadCairoUnderlays();
	ComputeAndDownloadCairoOverlays();

	//Do compute shader rendering for digital waveforms
	for(auto overlay : m_overlays)
	{
		if(overlay.m_channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
			continue;

		//Create render data if needed
		//(can't do this when m_waveformRenderData is created because decoders are added later on)
		if(m_overlayRenderData.find(overlay) == m_overlayRenderData.end())
			m_overlayRenderData[overlay] = new WaveformRenderData(overlay);

		//Create the texture
		auto wdat = m_overlayRenderData[overlay];
		wdat->m_waveformTexture.Bind();
		wdat->m_waveformTexture.SetData(m_width, m_height, NULL, GL_RGBA, GL_UNSIGNED_BYTE, GL_RGBA32F);
		ResetTextureFiltering();

		RenderTrace(wdat);
	}

	//Make sure all compute shaders are done before we composite
	if(IsDigital())
		m_digitalWaveformComputeProgram.MemoryBarrier();
	else
		m_analogWaveformComputeProgram.MemoryBarrier();

	//Final compositing of data being drawn to the screen
	m_windowFramebuffer.Bind(GL_FRAMEBUFFER);
	RenderCairoUnderlays();
	RenderMainTrace();
	RenderOverlayTraces();
	RenderCairoOverlays();

	//Sanity check
	GLint err = glGetError();
	if(err != 0)
		LogNotice("Render: err = %x\n", err);

	dt = GetTime() - start;
	m_renderTime += dt;

	//If our channel is digital, set us to minimal size
	if(m_channel.m_channel->GetType() == OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
	{
		//Base height
		int height = m_infoBoxRect.get_bottom() - m_infoBoxRect.get_top() + 5;

		//Add in overlays (TODO: don't hard code overlay pitch)
		height += 30*m_overlays.size();

		int rw, rh;
		get_size_request(rw, rh);
		if(height != rh)
			set_size_request(30, height);
	}

	return true;
}

void WaveformArea::RenderMainTrace()
{
	glEnable(GL_SCISSOR_TEST);
	glScissor(0, 0, m_plotRight, m_height);
	if(IsEye())
		RenderEye();
	else if(IsWaterfall())
		RenderWaterfall();
	else
		RenderTraceColorCorrection(m_waveformRenderData);
	glDisable(GL_SCISSOR_TEST);
}

void WaveformArea::RenderOverlayTraces()
{
	glEnable(GL_SCISSOR_TEST);
	glScissor(0, 0, m_plotRight, m_height);

	for(auto it : m_overlayRenderData)
		RenderTraceColorCorrection(it.second);

	glDisable(GL_SCISSOR_TEST);
}

void WaveformArea::RenderEye()
{
	if(m_channel.m_channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_EYE)
		return;
	auto pcap = dynamic_cast<EyeWaveform*>(m_channel.GetData());
	if(pcap == NULL)
		return;

	//It's an eye pattern! Just copy it directly into the waveform texture.
	m_eyeTexture.Bind();
	ResetTextureFiltering();
	m_eyeTexture.SetData(
		pcap->GetWidth(),
		pcap->GetHeight(),
		pcap->GetData(),
		GL_RED,
		GL_FLOAT,
		GL_RGBA32F);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);

	m_eyeProgram.Bind();
	m_eyeVAO.Bind();
	m_eyeProgram.SetUniform(m_eyeTexture, "fbtex", 0);
	m_eyeProgram.SetUniform(m_eyeColorRamp[m_parent->GetEyeColor()], "ramp", 1);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void WaveformArea::RenderWaterfall()
{
	auto pfall = dynamic_cast<Waterfall*>(m_channel.m_channel);
	auto pcap = dynamic_cast<WaterfallWaveform*>(m_channel.GetData());
	if(pfall == NULL)
		return;
	if(pcap == NULL)
		return;

	//Make sure timebase is correct
	pfall->SetTimeScale(m_group->m_pixelsPerXUnit);
	pfall->SetTimeOffset(m_group->m_xAxisOffset);

	//Just copy it directly into the waveform texture.
	m_eyeTexture.Bind();
	ResetTextureFiltering();
	m_eyeTexture.SetData(
		pfall->GetWidth(),
		pfall->GetHeight(),
		pcap->GetData(),
		GL_RED,
		GL_FLOAT,
		GL_RGBA32F);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);

	m_eyeProgram.Bind();
	m_eyeVAO.Bind();
	m_eyeProgram.SetUniform(m_eyeTexture, "fbtex", 0);
	m_eyeProgram.SetUniform(m_eyeColorRamp[m_parent->GetEyeColor()], "ramp", 1);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void WaveformArea::RenderPersistenceOverlay()
{
	/*
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
	*/
}

void WaveformArea::RenderTrace(WaveformRenderData* data)
{
	if(!data->m_geometryOK)
		return;

	//Round thread block size up to next multiple of the local size (must be power of two)
	int localSize = 2;
	int numCols = m_plotRight;
	if(0 != (numCols % localSize) )
	{
		numCols |= (localSize-1);
		numCols ++;
	}
	int numGroups = numCols / localSize;

	if(data->IsDigital())
	{
		m_digitalWaveformComputeProgram.Bind();
		m_digitalWaveformComputeProgram.SetImageUniform(data->m_waveformTexture, "outputTex");
	}
	else
	{
		m_analogWaveformComputeProgram.Bind();
		m_analogWaveformComputeProgram.SetImageUniform(data->m_waveformTexture, "outputTex");
	}
	data->m_waveformXBuffer.BindBase(1);
	data->m_waveformYBuffer.BindBase(4);
	data->m_waveformConfigBuffer.BindBase(2);
	data->m_waveformIndexBuffer.BindBase(3);
	if(data->IsDigital())
		m_digitalWaveformComputeProgram.DispatchCompute(numGroups, 1, 1);
	else
		m_analogWaveformComputeProgram.DispatchCompute(numGroups, 1, 1);
}

void WaveformArea::RenderTraceColorCorrection(WaveformRenderData* data)
{
	if(!data->m_geometryOK)
		return;

	//Prepare to render
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
	m_colormapProgram.Bind();
	m_colormapVAO.Bind();

	//Draw the offscreen buffer to the onscreen buffer
	//as a textured quad. Apply color correction as we do this.
	Gdk::Color color(data->m_channel.m_channel->m_displaycolor);
	m_colormapProgram.SetUniform(data->m_waveformTexture, "fbtex");
	m_colormapProgram.SetUniform(color.get_red_p(), "r");
	m_colormapProgram.SetUniform(color.get_green_p(), "g");
	m_colormapProgram.SetUniform(color.get_blue_p(), "b");

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void WaveformArea::ComputeAndDownloadCairoUnderlays()
{
	double tstart = GetTime();

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

	//Software rendering
	DoRenderCairoUnderlays(cr);

	m_cairoTime += (GetTime() - tstart);
	tstart = GetTime();

	//Update the texture
	//Tell GL it's RGBA even though it's BGRA, faster to invert in the shader than when downloading
	m_cairoTexture.Bind();
	ResetTextureFiltering();
	m_cairoTexture.SetData(
		m_width,
		m_height,
		surface->get_data());

	m_texDownloadTime += (GetTime() - tstart);
}

void WaveformArea::RenderCairoUnderlays()
{
	double tstart = GetTime();

	glDisable(GL_BLEND);

	//Draw the actual image
	m_cairoProgram.Bind();
	m_cairoVAO.Bind();
	m_cairoProgram.SetUniform(m_cairoTexture, "fbtex");
	m_cairoTexture.Bind();
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	m_compositeTime += (GetTime() - tstart);
}

void WaveformArea::ComputeAndDownloadCairoOverlays()
{
	double tstart = GetTime();

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

	m_cairoTime += GetTime() - tstart;
	tstart = GetTime();

	//Get the image data and make a texture from it
	//Tell GL it's RGBA even though it's BGRA, faster to invert in the shader than when downloading
	m_cairoTextureOver.Bind();
	ResetTextureFiltering();
	m_cairoTextureOver.SetData(
		m_width,
		m_height,
		surface->get_data());

	m_texDownloadTime += GetTime() - tstart;
}

void WaveformArea::RenderCairoOverlays()
{
	double tstart = GetTime();

	//Configure blending for Cairo's premultiplied alpha
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);

	//Draw the actual image
	m_windowFramebuffer.Bind(GL_FRAMEBUFFER);
	m_cairoTextureOver.Bind();
	m_cairoProgram.Bind();
	m_cairoVAO.Bind();
	m_cairoProgram.SetUniform(m_cairoTextureOver, "fbtex");
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	m_compositeTime += GetTime() - tstart;
}

int64_t WaveformArea::XPositionToXAxisUnits(float pix)
{
	return m_group->m_xAxisOffset + PixelsToXAxisUnits(pix);
}

int64_t WaveformArea::PixelsToXAxisUnits(float pix)
{
	return pix / m_group->m_pixelsPerXUnit;
}

float WaveformArea::XAxisUnitsToPixels(int64_t t)
{
	return t * m_group->m_pixelsPerXUnit;
}

float WaveformArea::XAxisUnitsToXPosition(int64_t t)
{
	return XAxisUnitsToPixels(t - m_group->m_xAxisOffset);
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
	return m_height/2 - VoltsToPixels(volt + m_channel.m_channel->GetOffset());
}

float WaveformArea::YPositionToVolts(float y)
{
	return PixelsToVolts(-1 * (y - m_height/2) ) - m_channel.m_channel->GetOffset();
}

float WaveformArea::PickStepSize(float volts_per_half_span, int min_steps, int max_steps)
{
	static const float steps[3] = {1, 2, 5};

	for(int exp = -4; exp < 12; exp ++)
	{
		for(int i=0; i<3; i++)
		{
			float step = pow(10, exp) * steps[i];

			float steps_per_half_span = volts_per_half_span / step;
			if(steps_per_half_span > max_steps)
				continue;
			if(steps_per_half_span < min_steps)
				continue;
			return step;
		}
	}

	//if no hits
	return FLT_MAX;
}
