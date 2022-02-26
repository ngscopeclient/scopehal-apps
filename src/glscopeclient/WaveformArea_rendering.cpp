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
	@brief  OpenGL rendering code for WaveformArea
 */

#include "glscopeclient.h"
#include "WaveformArea.h"
#include "OscilloscopeWindow.h"
#include <random>
#include <map>
#include <immintrin.h>
#include "../../lib/scopeprotocols/EyePattern.h"
#include "../../lib/scopeprotocols/SpectrogramFilter.h"
#include "../../lib/scopeprotocols/Waterfall.h"

using namespace std;

template size_t WaveformArea::BinarySearchForGequal<float>(float* buf, size_t len, float value);
template size_t WaveformArea::BinarySearchForGequal<int64_t>(int64_t* buf, size_t len, int64_t value);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WaveformRenderData

void WaveformRenderData::MapBuffers(size_t width, bool update_waveform)
{
	//Calculate the number of points we'll need to draw. Default to 1 if no data
	if(!IsAnalog() && !IsDigital() )
	{
		m_count = 1;
	}
	else
	{
		auto pdat = m_channel.GetData();
		m_count = 1;
		if(pdat != NULL)
		{
			m_count = pdat->m_offsets.size();
			m_count = max((size_t)1, m_count);
		}
	}

	if(update_waveform)
	{
		//Skip mapping X buffer if dense packed analog
		if(IsDensePacked() && IsAnalog() )
			m_mappedXBuffer = NULL;
		else
			m_mappedXBuffer = (int64_t*)m_waveformXBuffer.Map(m_count*sizeof(int64_t));

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
	}

	//Skip mapping index buffer if dense packed analog
	if(IsDensePacked() && IsAnalog())
		m_mappedIndexBuffer = NULL;
	else
		m_mappedIndexBuffer = (uint32_t*)m_waveformIndexBuffer.Map(width*sizeof(uint32_t));

	m_mappedConfigBuffer = (uint32_t*)m_waveformConfigBuffer.Map(sizeof(float)*13);
	//We're writing to different offsets in the buffer, not reinterpreting, so this is safe.
	//A struct is probably the better long term solution...
	//cppcheck-suppress invalidPointerCast
	m_mappedFloatConfigBuffer = (float*)m_mappedConfigBuffer;
	m_mappedConfigBuffer64 = (int64_t*)m_mappedConfigBuffer;
}

void WaveformRenderData::UnmapBuffers(bool update_waveform)
{
	if(update_waveform)
	{
		if(m_mappedXBuffer != NULL)
			m_waveformXBuffer.Unmap();
		m_waveformYBuffer.Unmap();
	}
	if(m_mappedIndexBuffer != NULL)
		m_waveformIndexBuffer.Unmap();
	m_waveformConfigBuffer.Unmap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

void WaveformArea::PrepareGeometry(WaveformRenderData* wdata, bool update_waveform, float alpha, float persistDecay)
{
	//We need analog or digital data to render
	auto area = wdata->m_area;
	auto channel = wdata->m_channel.m_channel;
	if( (channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL) &&
		(channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_ANALOG))
	{
		wdata->m_geometryOK = false;
		return;
	}
	auto pdat = wdata->m_channel.GetData();
	if( (pdat == NULL) || pdat->m_offsets.empty() || (wdata->m_count == 0) )
	{
		wdata->m_geometryOK = false;
		return;
	}

	//Bail if timebase is garbage
	if(pdat->m_timescale == 0)
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

	//Figure out zero voltage level and scaling
	auto height = area->m_height;
	float ybase = height/2;
	float yscale = area->m_pixelsPerVolt;
	if(digdat)
	{
		float digheight;

		//Overlay?
		if(area->m_overlayPositions.find(wdata->m_channel) != area->m_overlayPositions.end())
		{
			ybase = area->m_height - (area->m_overlayPositions[wdata->m_channel] + 10);
			digheight = 20;
		}

		//Main channel
		else
		{
			ybase = 2;
			digheight = height - 5;
		}

		yscale = digheight;
	}

	//Download actual waveform timestamps and voltages
	if(update_waveform)
	{
		if(digdat)
			memcpy(wdata->m_mappedDigitalYBuffer, &digdat->m_samples[0], wdata->m_count*sizeof(bool));
		else
			memcpy(wdata->m_mappedYBuffer, &andat->m_samples[0], wdata->m_count*sizeof(float));

		//Copy the X axis timestamps, no conversion needed.
		//But if dense packed, we can skip this
		//TODO: skip for dense packed digital path too once the shader supports that
		if(!wdata->IsDensePacked() || !andat)
			memcpy(wdata->m_mappedXBuffer, &pdat->m_offsets[0], wdata->m_count*sizeof(int64_t));
	}

	//Calculate indexes for rendering of sparse waveforms
	//TODO: can we parallelize this? move to a compute shader?
	auto group = wdata->m_area->m_group;
	int64_t offset_samples = (group->m_xAxisOffset - pdat->m_triggerPhase) / pdat->m_timescale;
	float xscale = (pdat->m_timescale * group->m_pixelsPerXUnit);
	if(!wdata->IsDensePacked() || !andat)	//TODO: skip for dense packed digital path too once the shader supports that
	{
		for(int j=0; j<wdata->m_area->m_width; j++)
		{
			int64_t target = floor(j / xscale) + offset_samples;
			wdata->m_mappedIndexBuffer[j] = BinarySearchForGequal(
				(int64_t*)&pdat->m_offsets[0],
				wdata->m_count,
				target-2);
		}
	}

	//Scale alpha by zoom.
	//As we zoom out more, reduce alpha to get proper intensity grading
	float capture_len = pdat->m_offsets[pdat->m_offsets.size() - 1] * pdat->m_timescale;
	float avg_sample_len = capture_len / pdat->m_offsets.size();
	float samplesPerPixel = 1.0 / (group->m_pixelsPerXUnit * avg_sample_len);
	float alpha_scaled = alpha / sqrt(samplesPerPixel);
	alpha_scaled = min(1.0f, alpha_scaled) * 2;

	//Config stuff
	int64_t innerxoff = group->m_xAxisOffset / pdat->m_timescale;
	int64_t fractional_offset = group->m_xAxisOffset % pdat->m_timescale;
	wdata->m_mappedConfigBuffer64[0] = -innerxoff;											//innerXoff
	wdata->m_mappedConfigBuffer[2] = height;												//windowHeight
	wdata->m_mappedConfigBuffer[3] = wdata->m_area->m_plotRight;							//windowWidth
	wdata->m_mappedConfigBuffer[4] = wdata->m_count;										//depth
	wdata->m_mappedConfigBuffer[5] = offset_samples - 2;									//offset_samples
	wdata->m_mappedFloatConfigBuffer[6] = alpha_scaled;										//alpha
	wdata->m_mappedFloatConfigBuffer[7] = (pdat->m_triggerPhase - fractional_offset) * group->m_pixelsPerXUnit;	//xoff
	wdata->m_mappedFloatConfigBuffer[8] = xscale;											//xscale
	wdata->m_mappedFloatConfigBuffer[9] = ybase;											//ybase
	wdata->m_mappedFloatConfigBuffer[10] = yscale;											//yscale
	wdata->m_mappedFloatConfigBuffer[11] = wdata->m_channel.GetOffset();					//yoff

	//persistScale
	if(!wdata->m_persistence)
		wdata->m_mappedFloatConfigBuffer[12] = 0;
	else
		wdata->m_mappedFloatConfigBuffer[12] = persistDecay;

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
		return len-1;

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

void WaveformArea::GetAllRenderData(vector<WaveformRenderData*>& data)
{
	bool persist = m_persistence && !m_persistenceClear;
	m_waveformRenderData->m_persistence = persist;

	if(IsAnalog() || IsDigital())
		data.push_back(m_waveformRenderData);

	for(auto overlay : m_overlays)
	{
		//Skip anything not digital
		if(overlay.m_channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
			continue;

		//Create render data if needed.
		//Despite what cppcheck says we do have to check before inserting,
		//since we're dynamically creating
		if(m_overlayRenderData.find(overlay) == m_overlayRenderData.end())
		{
			//cppcheck-suppress stlFindInsert
			m_overlayRenderData[overlay] = new WaveformRenderData(overlay, this);
			m_geometryDirty = true;
		}
		m_overlayRenderData[overlay]->m_persistence = persist;

		data.push_back(m_overlayRenderData[overlay]);
	}
}

void WaveformArea::MapAllBuffers(bool update_y)
{
	make_current();

	//Main waveform
	if(IsAnalog() || IsDigital())
		m_waveformRenderData->MapBuffers(m_width, update_y);

	for(auto overlay : m_overlays)
	{
		if(overlay.m_channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
			continue;

		if(m_overlayRenderData.find(overlay) != m_overlayRenderData.end())
			m_overlayRenderData[overlay]->MapBuffers(m_width, update_y);
	}
}

void WaveformArea::UnmapAllBuffers(bool update_y)
{
	make_current();

	//Main waveform
	if(IsAnalog() || IsDigital())
		m_waveformRenderData->UnmapBuffers(update_y);

	for(auto overlay : m_overlays)
	{
		if(overlay.m_channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
			continue;

		if(m_overlayRenderData.find(overlay) != m_overlayRenderData.end())
			m_overlayRenderData[overlay]->UnmapBuffers(update_y);
	}
}

float WaveformArea::GetPersistenceDecayCoefficient()
{
	float f = m_parent->GetPreferences().GetReal("Appearance.Waveforms.persist_decay_rate");
	f = min(f, 1.0f);
	f = max(f, 0.0f);

	return f;
}

bool WaveformArea::on_render(const Glib::RefPtr<Gdk::GLContext>& /*context*/)
{
	//If a file load is in progress don't waste time on expensive render calls.
	//Many render events get dispatched as various parts of the UI config and waveform data load,
	//and we only want to actually draw on the very last one.
	if(m_parent->IsLoadInProgress())
		return true;

	LogIndenter li;
	float persistDecay = GetPersistenceDecayCoefficient();

	//Overlay positions need to be calculated before geometry download,
	//since scaling data is pushed to the GPU at this time
	CalculateOverlayPositions();

	//This block cares about waveform data.
	{
		lock_guard<recursive_mutex> lock(m_parent->m_waveformDataMutex);

		//Pull vertical size from the scope early on no matter how we're rendering
		m_pixelsPerVolt = m_height / m_channel.GetVoltageRange();

		//Update geometry if needed
		if(m_geometryDirty || m_positionDirty)
		{
			double alpha = m_parent->GetTraceAlpha();

			//Need to get render data first, since this creates buffers we might need in MapBuffers
			vector<WaveformRenderData*> data;
			GetAllRenderData(data);

			//Do the actual update
			MapAllBuffers(m_geometryDirty);
			for(auto d : data)
				PrepareGeometry(d, m_geometryDirty, alpha, persistDecay);
			UnmapAllBuffers(m_geometryDirty);

			m_geometryDirty = false;
			m_positionDirty = false;
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

		//Draw the main waveform
		if(IsAnalog() || IsDigital() )
			RenderTrace(m_waveformRenderData);

		//Launch software rendering passes and push the resulting data to the GPU
		ComputeAndDownloadCairoOverlays();

		//Do compute shader rendering for digital waveforms
		for(auto overlay : m_overlays)
		{
			if(overlay.m_channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
				continue;

			//Create the texture
			auto wdat = m_overlayRenderData[overlay];
			wdat->m_waveformTexture.Bind();
			wdat->m_waveformTexture.SetData(m_width, m_height, NULL, GL_RGBA, GL_UNSIGNED_BYTE, GL_RGBA32F);
			ResetTextureFiltering();

			RenderTrace(wdat);
		}
	}

	//Underlays don't care about the mutex
	ComputeAndDownloadCairoUnderlays();

	//Make sure all compute shaders are done before we composite
	m_digitalWaveformComputeProgram.MemoryBarrier();
	m_histogramWaveformComputeProgram.MemoryBarrier();
	m_denseAnalogWaveformComputeProgram.MemoryBarrier();
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

	//Done, not clearing persistence
	m_persistenceClear = false;

	return true;
}

void WaveformArea::RenderMainTrace()
{
	glEnable(GL_SCISSOR_TEST);
	glScissor(0, 0, m_plotRight, m_height);
	if(IsSpectrogram())
		RenderSpectrogram();
	else if(IsEye())
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

void WaveformArea::RenderSpectrogram()
{
	if(m_channel.m_channel->GetType() != OscilloscopeChannel::CHANNEL_TYPE_SPECTROGRAM)
		return;
	auto pcap = dynamic_cast<SpectrogramWaveform*>(m_channel.GetData());
	if(pcap == NULL)
		return;

	//Reuse the texture from the eye pattern rendering path
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

	//Figure out the X scale and offset factors.
	float xpixscale = m_group->m_pixelsPerXUnit / m_width;
	float xoff = (m_group->m_xAxisOffset + pcap->GetStartTime()) * xpixscale;
	float xscale = pcap->GetDuration() * xpixscale;

	//Figure out Y axis scale and offset
	float range = m_channel.GetVoltageRange();
	float yoff = -m_channel.GetOffset() / range - 0.5;
	float yscale = pcap->GetMaxFrequency() / range;

	m_spectrogramProgram.Bind();
	m_spectrogramVAO.Bind();
	m_spectrogramProgram.SetUniform(xscale, "xscale");
	m_spectrogramProgram.SetUniform(xoff, "xoff");
	m_spectrogramProgram.SetUniform(yscale, "yscale");
	m_spectrogramProgram.SetUniform(yoff, "yoff");
	m_spectrogramProgram.SetUniform(m_eyeTexture, "fbtex", 0);
	m_spectrogramProgram.SetUniform(m_eyeColorRamp[m_parent->GetEyeColor()], "ramp", 1);

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

Program* WaveformArea::GetProgramForWaveform(WaveformRenderData* data)
{
	if(data->IsDigital())
		return &m_digitalWaveformComputeProgram;
	else if(data->IsHistogram())
		return &m_histogramWaveformComputeProgram;
	else if(data->IsDensePacked())
		return &m_denseAnalogWaveformComputeProgram;
	else
		return &m_analogWaveformComputeProgram;
}

void WaveformArea::RenderTrace(WaveformRenderData* data)
{
	if(!data->m_geometryOK)
		return;

	bool good = false;

	switch(GetRenderingBackend())
	{
		#ifdef HAVE_OPENCL

		//OpenCL path
		case ACCEL_OPENCL:
			try
			{
				cl::CommandQueue queue(*g_clContext, g_contextDevices[0], 0);

				//Output buffer
				//TODO: GL-CL zero copy interop
				vector<float> host_outbuf;
				host_outbuf.resize(m_width * m_height*4);
				for(size_t i=0; i<host_outbuf.size(); i++)
					host_outbuf[i] = 0.0;
				cl::Buffer outbuf(queue, host_outbuf.begin(), host_outbuf.end(), false, true, NULL);

				//Input buffer
				//For now, copy everything here
				//TODO: do in PrepareGeometry()
				auto pdat = data->m_channel.GetData();
				auto andat = dynamic_cast<AnalogWaveform*>(pdat);
				auto digdat = dynamic_cast<DigitalWaveform*>(pdat);
				//cl::Buffer xbuf(queue, pdat->m_offsets.begin(), pdat->m_offsets.end(), true, true, NULL);

				//TODO: Figure out indexing

				//Number of threads to use for a single column of pixels
				const int threads_per_column = 64;

				//Grab a few helpful variables
				auto area = data->m_area;
				auto group = area->m_group;
				int64_t offset_samples = (group->m_xAxisOffset - pdat->m_triggerPhase) / pdat->m_timescale;
				int64_t innerxoff = group->m_xAxisOffset / pdat->m_timescale;
				int64_t fractional_offset = group->m_xAxisOffset % pdat->m_timescale;
				float xscale = (pdat->m_timescale * group->m_pixelsPerXUnit);

				//Scale alpha by zoom.
				//As we zoom out more, reduce alpha to get proper intensity grading
				float alpha = 0.9;
				float capture_len = pdat->m_offsets[pdat->m_offsets.size() - 1] * pdat->m_timescale;
				float avg_sample_len = capture_len / pdat->m_offsets.size();
				float samplesPerPixel = 1.0 / (group->m_pixelsPerXUnit * avg_sample_len);
				float alpha_scaled = alpha / sqrt(samplesPerPixel);
				alpha_scaled = min(1.0f, alpha_scaled) * 2;

				//Figure out zero voltage level and scaling
				auto height = area->m_height;
				float ybase = height/2;
				float yscale = area->m_pixelsPerVolt;
				if(digdat)
				{
					float digheight;

					//Overlay?
					auto f = dynamic_cast<Filter*>(data->m_channel.m_channel);
					if(area->m_overlayPositions.find(data->m_channel) != area->m_overlayPositions.end())
					{
						ybase = area->m_height - (area->m_overlayPositions[data->m_channel] + 10);
						digheight = 20;
					}

					//Main channel
					else
					{
						ybase = 2;
						digheight = height - 5;
					}

					yscale = digheight;
				}

				float persistDecay = 0.95f;

				//OpenCL won't let us run a massive work group.
				//Break us up into 256-pixel blocks rendered consecutively.
				//TODO: respect CL_DEVICE_MAX_WORK_GROUP_SIZE and use largest sane value
				const int blocksize = 256;
				for(int i=0; i<m_width; i += blocksize)
				{
					if(andat)
					{
						//Analog Y buffer
						cl::Buffer ybuf(queue, andat->m_samples.begin(), andat->m_samples.end(), true, true, NULL);

						//We only support dense packed waveforms for now
						if(pdat->m_densePacked)
						{
							m_renderDenseAnalogWaveformKernel->setArg(0, (unsigned int)data->m_area->m_plotRight);
							m_renderDenseAnalogWaveformKernel->setArg(1, (unsigned int)data->m_area->m_width);
							m_renderDenseAnalogWaveformKernel->setArg(2, (unsigned int)data->m_area->m_height);
							m_renderDenseAnalogWaveformKernel->setArg(3, (unsigned long)i);
							m_renderDenseAnalogWaveformKernel->setArg(4, pdat->m_offsets.size());
							m_renderDenseAnalogWaveformKernel->setArg(5, -innerxoff);
							m_renderDenseAnalogWaveformKernel->setArg(6, offset_samples - 2);
							m_renderDenseAnalogWaveformKernel->setArg(7, alpha_scaled);
							m_renderDenseAnalogWaveformKernel->setArg(8,
								(unsigned long)((pdat->m_triggerPhase - fractional_offset) * group->m_pixelsPerXUnit));
							m_renderDenseAnalogWaveformKernel->setArg(9, xscale);
							m_renderDenseAnalogWaveformKernel->setArg(10, ybase);
							m_renderDenseAnalogWaveformKernel->setArg(11, yscale);
							m_renderDenseAnalogWaveformKernel->setArg(12, (float)data->m_channel.GetOffset());
							if(!data->m_persistence)
								m_renderDenseAnalogWaveformKernel->setArg(13, 0.0f);
							else
								m_renderDenseAnalogWaveformKernel->setArg(13, persistDecay);
							m_renderDenseAnalogWaveformKernel->setArg(14, ybuf);
							m_renderDenseAnalogWaveformKernel->setArg(15, outbuf);

							queue.enqueueNDRangeKernel(
								*m_renderDenseAnalogWaveformKernel,
								cl::NullRange,
								cl::NDRange(blocksize, threads_per_column),
								cl::NDRange(1, threads_per_column),
								NULL);

							good = true;
						}
					}
					else if(digdat)
					{
						//LogWarning("OpenCL digital rendering unimplemented for now\n");
					}

				}

				//Read results and copy to the output texture
				if(good)
				{
					void* ptr = queue.enqueueMapBuffer(outbuf, true, CL_MAP_READ, 0, host_outbuf.size() * sizeof(float));
					queue.enqueueUnmapMemObject(outbuf, ptr);
					data->m_waveformTexture.Bind();
					data->m_waveformTexture.SetData(
						m_width,
						m_height,
						&host_outbuf[0],
						GL_RED,
						GL_FLOAT,
						GL_RGBA32F);
				}
			}
			catch(const cl::Error& e)
			{
				LogWarning("OpenCL error: %s (%d)\n", e.what(), e.err() );
				good = false;
			}
			if(good)
				break;
			//else fall through

		#endif

		case ACCEL_OPENGL:
		default:
			{
				//Round thread block size up to next multiple of the local size (must be power of two)
				//localSize must match COLS_PER_BLOCK in waveform-compute-core.glsl
				int localSize = 1;
				int numCols = m_plotRight;
				if(0 != (numCols % localSize) )
				{
					numCols |= (localSize-1);
					numCols ++;
				}
				int numGroups = numCols / localSize;

				auto prog = GetProgramForWaveform(data);
				prog->Bind();
				prog->SetImageUniform(data->m_waveformTexture, "outputTex");

				data->m_waveformXBuffer.BindBase(1);
				data->m_waveformYBuffer.BindBase(4);
				data->m_waveformConfigBuffer.BindBase(2);
				data->m_waveformIndexBuffer.BindBase(3);

				prog->DispatchCompute(numGroups, 1, 1);
			}
			break;
	}
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

	//Update the texture
	//Tell GL it's RGBA even though it's BGRA, faster to invert in the shader than when downloading
	m_cairoTexture.Bind();
	ResetTextureFiltering();
	m_cairoTexture.SetData(
		m_width,
		m_height,
		surface->get_data());
}

void WaveformArea::RenderCairoUnderlays()
{
	glDisable(GL_BLEND);

	//Draw the actual image
	m_cairoProgram.Bind();
	m_cairoVAO.Bind();
	m_cairoProgram.SetUniform(m_cairoTexture, "fbtex");
	m_cairoTexture.Bind();
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void WaveformArea::ComputeAndDownloadCairoOverlays()
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
	//Tell GL it's RGBA even though it's BGRA, faster to invert in the shader than when downloading
	m_cairoTextureOver.Bind();
	ResetTextureFiltering();
	m_cairoTextureOver.SetData(
		m_width,
		m_height,
		surface->get_data());
}

void WaveformArea::RenderCairoOverlays()
{
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

float WaveformArea::PixelToYAxisUnits(float pix)
{
	return pix / m_pixelsPerVolt;
}

float WaveformArea::YAxisUnitsToPixels(float volt)
{
	return volt * m_pixelsPerVolt;
}

float WaveformArea::YAxisUnitsToYPosition(float volt)
{
	return m_height/2 - YAxisUnitsToPixels(volt + m_channel.GetOffset());
}

float WaveformArea::YPositionToYAxisUnits(float y)
{
	return PixelToYAxisUnits(-1 * (y - m_height/2) ) - m_channel.GetOffset();
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
