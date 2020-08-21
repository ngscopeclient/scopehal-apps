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
#include "ProfileBlock.h"
#include "../../lib/scopeprotocols/EyeDecoder2.h"
#include "../../lib/scopeprotocols/WaterfallDecoder.h"

using namespace std;

template size_t WaveformArea::BinarySearchForGequal<float>(float* buf, size_t len, float value);
template size_t WaveformArea::BinarySearchForGequal<int64_t>(int64_t* buf, size_t len, int64_t value);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WaveformRenderData

void WaveformRenderData::MapBuffers(size_t width)
{
	//Calculate the number of points we'll need to draw. Default to 1 if no data
	if( (m_channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL) &&
		(m_channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_ANALOG))
	{
		m_count = 1;
	}
	else
	{
		auto pdat = m_channel->GetData();
		if( (pdat == NULL) || ((m_count = pdat->m_offsets.size()) == 0) )
			m_count = 1;
		if(dynamic_cast<DigitalWaveform*>(pdat) != NULL)
			m_count *= 2;
	}

	m_mappedXBuffer = (float*)m_waveformXBuffer.Map(m_count*sizeof(float), GL_READ_WRITE);
	m_mappedYBuffer = (float*)m_waveformYBuffer.Map(m_count*sizeof(float));
	m_mappedIndexBuffer = (uint32_t*)m_waveformIndexBuffer.Map(width*sizeof(uint32_t));
	m_mappedConfigBuffer = (uint32_t*)m_waveformConfigBuffer.Map(sizeof(float)*10);
	m_mappedFloatConfigBuffer = (float*)m_mappedConfigBuffer;
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
	auto channel = wdata->m_channel;
	if( (channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL) &&
		(channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_ANALOG))
	{
		wdata->m_geometryOK = false;
		return;
	}
	auto pdat = channel->GetData();
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

	float xscale = pdat->m_timescale * m_group->m_pixelsPerXUnit;

	//Zero voltage level
	//TODO: properly calculate decoder positions once RenderDecodeOverlays() isn't doing that anymore
	float ybase = m_height/2;
	if(digdat)
	{
		//Main channel
		if(channel == m_channel)
			ybase = 2;

		//Overlay
		else
			ybase = m_height - (m_overlayPositions[dynamic_cast<ProtocolDecoder*>(channel)] + 10);
	}

	float yoff = channel->GetOffset();

	//Y axis scaling in shader
	float yscale = 1;

	//We need to stretch every sample to two samples, one at the very left and one at the very right,
	//so interpolation works right.
	//TODO: we can probably avoid this by rewriting the compute shader to not interpolate like this
	//TODO: only add extra samples if the left and right values are not the same?
	size_t realcount = wdata->m_count;
	if(digdat)
		realcount /= 2;

	if(digdat)
	{
		float digheight;
		if(channel == m_channel)
			digheight = m_height - 5;
		else
			digheight = 20;

		//TODO: AVX
		yscale = digheight;
		for(size_t j=0; j<realcount; j++)
		{
			int64_t off = digdat->m_offsets[j];
			wdata->m_mappedXBuffer[j*2] 	= off;
			wdata->m_mappedXBuffer[j*2 + 1]	= off + digdat->m_durations[j];

			wdata->m_mappedYBuffer[j*2] 	= digdat->m_samples[j];
			wdata->m_mappedYBuffer[j*2 + 1]	= digdat->m_samples[j];
		}
	}
	else
	{
		//Need AVX512DQ or AVX512VL for VCTVQQ2PS
		//TODO: see if there is any way to speed this up at least a little on AVX2?
		if(g_hasAvx512DQ || g_hasAvx512VL)
			Int64ToFloatAVX512(wdata->m_mappedXBuffer, reinterpret_cast<int64_t*>(&andat->m_offsets[0]), wdata->m_count);
		else
			Int64ToFloat(wdata->m_mappedXBuffer, reinterpret_cast<int64_t*>(&andat->m_offsets[0]), wdata->m_count);

		yscale = m_pixelsPerVolt;

		//Copy the waveform
		memcpy(wdata->m_mappedYBuffer, &andat->m_samples[0], wdata->m_count*sizeof(float));
	}

	double dt = GetTime() - start;
	m_prepareTime += dt;
	start = GetTime();

	//Calculate indexes for rendering.
	//This is necessary since samples may be sparse and have arbitrary spacing between them, so we can't
	//trivially map sample indexes to X pixel coordinates.
	//TODO: can we parallelize this? move to a compute shader?
	float xoff = (pdat->m_triggerPhase - m_group->m_xAxisOffset) * m_group->m_pixelsPerXUnit;
	for(int j=0; j<m_width; j++)
		wdata->m_mappedIndexBuffer[j] = BinarySearchForGequal(wdata->m_mappedXBuffer, wdata->m_count, (j - xoff) / xscale);

	dt = GetTime() - start;
	m_indexTime += dt;

	//Config stuff
	wdata->m_mappedConfigBuffer[0] = m_height;							//windowHeight
	wdata->m_mappedConfigBuffer[1] = m_plotRight;						//windowWidth
	wdata->m_mappedConfigBuffer[2] = wdata->m_count;					//depth
	wdata->m_mappedFloatConfigBuffer[3] = m_parent->GetTraceAlpha();	//alpha
	wdata->m_mappedConfigBuffer[4] = digdat ? 1 : 0;					//digital
	wdata->m_mappedFloatConfigBuffer[5] = xoff;							//xoff
	wdata->m_mappedFloatConfigBuffer[6] = xscale;						//xscale
	wdata->m_mappedFloatConfigBuffer[7] = ybase;						//ybase
	wdata->m_mappedFloatConfigBuffer[8] = yscale;						//yscale
	wdata->m_mappedFloatConfigBuffer[9] = yoff;							//yoff

	//Done
	wdata->m_geometryOK = true;
}

/**
	@brief Convert an array of int64_t's to floats
 */
void WaveformArea::Int64ToFloat(float* dst, int64_t* src, size_t len)
{
	float* pdst = reinterpret_cast<float*>(__builtin_assume_aligned(dst, 32));
	int64_t* psrc = reinterpret_cast<int64_t*>(__builtin_assume_aligned(src, 16));

	//Not possible to push this to a compute shader without GL_ARB_gpu_shader_int64,
	//which isn't well supported on integrated gfx yet :(
	for(size_t j=0; j < len; j++)
		pdst[j] 	= psrc[j];
}

__attribute__((target("avx512dq")))
void WaveformArea::Int64ToFloatAVX512(float* dst, int64_t* src, size_t len)
{
	//Round length down to multiple of 8 so we can SIMD the loop
	size_t len_rounded = len - (len % 8);

	//Main unrolled loop
	for(size_t j=0; j<len_rounded; j+= 8)
	{
		__m512i i64x8 = _mm512_load_epi64(src + j);
		__m256 f32x8 = _mm512_cvt_roundepi64_ps(i64x8, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
		_mm256_store_ps(dst + j, f32x8);
	}

	//Do anything we missed
	for(size_t j=len_rounded; j < len; j++)
		dst[j] 	= src[j];
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
		if(overlay->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
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
		if(overlay->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
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
		if(overlay->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
			continue;

		if(m_overlayRenderData.find(overlay) != m_overlayRenderData.end())
			m_overlayRenderData[overlay]->UnmapBuffers();
	}
}

bool WaveformArea::on_render(const Glib::RefPtr<Gdk::GLContext>& /*context*/)
{
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
	m_pixelsPerVolt = m_height / m_channel->GetVoltageRange();

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
		if(overlay->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
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
	m_waveformComputeProgram.MemoryBarrier();

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
	if(m_channel->GetType() == OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
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
	if(m_channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_EYE)
		return;
	auto pcap = dynamic_cast<EyeWaveform*>(m_channel->GetData());
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
	auto pfall = dynamic_cast<WaterfallDecoder*>(m_channel);
	auto pcap = dynamic_cast<WaterfallWaveform*>(m_channel->GetData());
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

	m_waveformComputeProgram.Bind();
	m_waveformComputeProgram.SetImageUniform(data->m_waveformTexture, "outputTex");
	data->m_waveformXBuffer.BindBase(1);
	data->m_waveformYBuffer.BindBase(4);
	data->m_waveformConfigBuffer.BindBase(2);
	data->m_waveformIndexBuffer.BindBase(3);
	m_waveformComputeProgram.DispatchCompute(numGroups, 1, 1);
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
	Gdk::Color color(data->m_channel->m_displaycolor);
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
	return m_height/2 - VoltsToPixels(volt + m_channel->GetOffset());
}

float WaveformArea::YPositionToVolts(float y)
{
	return PixelsToVolts(-1 * (y - m_height/2) ) - m_channel->GetOffset();
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
