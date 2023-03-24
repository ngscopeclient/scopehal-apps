/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg                                                                          *
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
#include "../../lib/scopeprotocols/EyePattern.h"
#include "../../lib/scopeprotocols/SpectrogramFilter.h"
#include "../../lib/scopeprotocols/Waterfall.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WaveformRenderData

WaveformRenderData::WaveformRenderData(StreamDescriptor channel, WaveformArea* area)
: m_area(area)
, m_shaderDense()
, m_shaderSparse()
, m_vkCmdPool()
, m_vkCmdBuf()
, m_channel(channel)
, m_geometryOK(false)
, m_count(0)
, m_persistence(false)
{
	if(IsAnalog() || IsDigital())
	{
		std::string shaderfn = "shaders/waveform-compute.";

		if(IsHistogram())
			shaderfn += "histogram";
		else if(IsAnalog())
			shaderfn += "analog";
		else if(IsDigital())
			shaderfn += "digital";

		int durationsSSBOs = 0;

		if(ZeroHoldFlagSet() && !IsHistogram()) // Workaround for previous assumption that histogram implies STREAM_DO_NOT_INTERPOLATE
		{
			// TODO: Need to be able to dispatch this at runtime once we grow a UI setting for interpolation behavior
			shaderfn += ".zerohold";
			durationsSSBOs = 1;
		}

		if (GLEW_ARB_gpu_shader_int64 && !g_noglint64)
			shaderfn += ".int64";
		
		std::string denseShaderFn = shaderfn + ".dense.spv";
		std::string sparseShaderFn = shaderfn + ".spv";
		LogDebug("Channel %s loading shader %s\n", channel.GetName().c_str(), sparseShaderFn.c_str());
		m_shaderDense = std::make_shared<ComputePipeline>(denseShaderFn, 2, sizeof(ConfigPushConstants));
		m_shaderSparse = std::make_shared<ComputePipeline>(sparseShaderFn, durationsSSBOs + 4, sizeof(ConfigPushConstants));

		vk::CommandPoolCreateInfo poolInfo(
			vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			area->GetParent()->m_vkQueue->m_family );
		m_vkCmdPool = std::make_unique<vk::raii::CommandPool>(*g_vkComputeDevice, poolInfo);

		vk::CommandBufferAllocateInfo bufinfo(**m_vkCmdPool, vk::CommandBufferLevel::ePrimary, 1);
		m_vkCmdBuf = std::make_unique<vk::raii::CommandBuffer>(std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));
	}
}

void WaveformRenderData::UpdateCount()
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
			m_count = pdat->size();
			m_count = max((size_t)1, m_count);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

void WaveformArea::UpdateCachedScales()
{
	//Pull vertical size from the scope early on no matter how we're rendering
	m_pixelsPerYAxisUnit = m_height / m_channel.GetVoltageRange();
}

void WaveformArea::PrepareGeometry(WaveformRenderData* wdata, bool update_waveform, float alpha, float persistDecay)
{
	//We need analog or digital data to render
	auto area = wdata->m_area;
	if( (wdata->m_channel.GetType() != Stream::STREAM_TYPE_DIGITAL) &&
		(wdata->m_channel.GetType() != Stream::STREAM_TYPE_ANALOG))
	{
		wdata->m_geometryOK = false;
		return;
	}
	auto pdat = wdata->m_channel.GetData();
	if( (pdat == NULL) || pdat->empty() || (wdata->m_count == 0) )
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
	auto sandat = dynamic_cast<SparseAnalogWaveform*>(pdat);
	auto uandat = dynamic_cast<UniformAnalogWaveform*>(pdat);
	auto sdigdat = dynamic_cast<SparseDigitalWaveform*>(pdat);
	auto udigdat = dynamic_cast<UniformDigitalWaveform*>(pdat);
	if(!sandat && !uandat && !sdigdat && !udigdat)
	{
		wdata->m_geometryOK = false;
		return;
	}

	//Figure out zero voltage level and scaling
	auto height = area->m_height;
	float ybase = height/2;
	float yscale = area->m_pixelsPerYAxisUnit;
	if(sdigdat || udigdat)
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

	//Ensure GPU has actual waveform timestamps and voltages
	//FIXME: This can be optimized by batching all calls to a cmdbuf, but can't
	// use wdata->m_vkCmdBuf for this because RenderTrace uses that.
	if(update_waveform)
	{
		if(sandat)
			sandat->m_samples.PrepareForGpuAccess(false);
		else if(uandat)
			uandat->m_samples.PrepareForGpuAccess(false);
		else if(sdigdat)
			sdigdat->m_samples.PrepareForGpuAccess(false);
		else if(udigdat)
			udigdat->m_samples.PrepareForGpuAccess(false);

		//Copy the X axis timestamps, no conversion needed.
		//But if dense packed, we can skip this
		if(sandat)
		{
			sandat->m_offsets.PrepareForGpuAccess(false);
			sandat->m_durations.PrepareForGpuAccess(false);
		}
		else if(sdigdat)
		{
			sdigdat->m_offsets.PrepareForGpuAccess(false);
			sdigdat->m_durations.PrepareForGpuAccess(false);
		}
	}

	//Calculate indexes for rendering of sparse waveforms
	//TODO: can we parallelize this? move to a compute shader?
	auto group = wdata->m_area->m_group;
	int64_t offset_samples = (group->m_xAxisOffset - pdat->m_triggerPhase) / pdat->m_timescale;
	double xscale = (pdat->m_timescale * group->m_pixelsPerXUnit);
	if(sandat || sdigdat)
	{
		int64_t* offsets;
		if(sandat)
		{
			sandat->m_offsets.PrepareForCpuAccess();
			offsets = sandat->m_offsets.GetCpuPointer();
		}
		else
		{
			sdigdat->m_offsets.PrepareForCpuAccess();
			offsets = sdigdat->m_offsets.GetCpuPointer();
		}

		wdata->m_indexBuffer.resize(wdata->m_area->m_width);
		for(int j=0; j<wdata->m_area->m_width; j++)
		{
			int64_t target = floor(j / xscale) + offset_samples;
			wdata->m_indexBuffer[j] = BinarySearchForGequal(
				offsets,
				wdata->m_count,
				target-2);
		}
		wdata->m_indexBuffer.MarkModifiedFromCpu();
	}

	//Scale alpha by zoom.
	//As we zoom out more, reduce alpha to get proper intensity grading
	int64_t lastOff;
	auto end = pdat->size() - 1;
	if(sandat || uandat)
		lastOff = GetOffsetScaled(sandat, uandat, end);
	else
		lastOff = GetOffsetScaled(sdigdat, udigdat, end);
	float capture_len = lastOff;
	float avg_sample_len = capture_len / pdat->size();
	float samplesPerPixel = 1.0 / (group->m_pixelsPerXUnit * avg_sample_len);
	float alpha_scaled = alpha / sqrt(samplesPerPixel);
	alpha_scaled = min(1.0f, alpha_scaled) * 2;

	//Config stuff
	int64_t innerxoff = group->m_xAxisOffset / pdat->m_timescale;
	int64_t fractional_offset    = group->m_xAxisOffset % pdat->m_timescale;
	wdata->m_config.innerXoff    = -innerxoff;
	wdata->m_config.windowHeight = height;
	wdata->m_config.windowWidth  = wdata->m_area->m_width;
	wdata->m_config.memDepth     = wdata->m_count;
	wdata->m_config.offset_samples = offset_samples - 2;
	wdata->m_config.alpha        = alpha_scaled;
	wdata->m_config.xoff         = (pdat->m_triggerPhase - fractional_offset) * group->m_pixelsPerXUnit;
	wdata->m_config.xscale       = xscale;
	wdata->m_config.ybase        = ybase;
	wdata->m_config.yscale       = yscale;
	wdata->m_config.yoff         = wdata->m_channel.GetOffset();
	if(!wdata->m_persistence)
		wdata->m_config.persistScale = 0;
	else
		wdata->m_config.persistScale = persistDecay;

	//Allocate output buffer
	wdata->m_renderedWaveform.resize(wdata->m_config.windowWidth * wdata->m_config.windowHeight);

	//Done
	wdata->m_geometryOK = true;
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
		if(overlay.GetType() != Stream::STREAM_TYPE_DIGITAL)
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

void WaveformArea::UpdateCounts()
{
	//TODO: Can probably eliminate this entirely and just put the
	// WaveformRenderData::m_count update in PrepareGeometry?

	//Main waveform
	if(IsAnalog() || IsDigital())
		m_waveformRenderData->UpdateCount();

	for(auto overlay : m_overlays)
	{
		if(overlay.GetType() != Stream::STREAM_TYPE_DIGITAL)
			continue;

		if(m_overlayRenderData.find(overlay) != m_overlayRenderData.end())
			m_overlayRenderData[overlay]->UpdateCount();
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

		UpdateCachedScales();

		//Update geometry if needed
		if(m_geometryDirty || m_positionDirty)
		{
			double alpha = m_parent->GetTraceAlpha();

			//Need to get render data first, since this creates buffers we might need in MapBuffers
			vector<WaveformRenderData*> data;
			GetAllRenderData(data);

			//Do the actual update
			UpdateCounts();
			for(auto d : data)
				PrepareGeometry(d, m_geometryDirty, alpha, persistDecay);

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
		if(IsAnalog() || IsDigital())
			RenderTrace(m_waveformRenderData);

		//Launch software rendering passes and push the resulting data to the GPU
		ComputeAndDownloadCairoOverlays();

		//Do compute shader rendering for digital waveforms
		for(auto overlay : m_overlays)
		{
			if(overlay.GetType() != Stream::STREAM_TYPE_DIGITAL)
				continue;

			auto wdat = m_overlayRenderData[overlay];
			wdat->m_renderedWaveform.resize(m_width * m_height);

			RenderTrace(wdat);
		}
	}

	//Underlays don't care about the mutex
	ComputeAndDownloadCairoUnderlays();

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
	if(m_channel.GetType() == Stream::STREAM_TYPE_DIGITAL)
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
	if(m_channel.GetType() != Stream::STREAM_TYPE_EYE)
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
#ifdef _APPLE_SILICON //spectrogram broken on apple silicon due to missing ffts, FIXME
	return;
#else
	if(m_channel.GetType() != Stream::STREAM_TYPE_SPECTROGRAM)
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
#endif
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

void WaveformArea::RenderTrace(WaveformRenderData* data)
{
	if(!data->m_geometryOK)
		return;

	//Round thread block size up to next multiple of the local size (must be power of two)
	//localSize must match COLS_PER_BLOCK in waveform-compute.glsl
	int localSize = 1;
	int numCols = m_plotRight;
	if(0 != (numCols % localSize) )
	{
		numCols |= (localSize-1);
		numCols ++;
	}
	int numGroups = numCols / localSize;

	std::shared_ptr<ComputePipeline> comp;
	if(data->IsDensePacked())
		comp = data->m_shaderDense;
	else
		comp = data->m_shaderSparse;
	data->m_vkCmdBuf->begin({});
	//Bind the output buffer
	//TODO: Can we use outputOnly=true if no persistence?
	comp->BindBufferNonblocking(0, data->m_renderedWaveform, *data->m_vkCmdBuf);
	auto pdat = data->m_channel.GetData();
	auto sandat = dynamic_cast<SparseAnalogWaveform*>(pdat);
	auto uandat = dynamic_cast<UniformAnalogWaveform*>(pdat);
	auto sdigdat = dynamic_cast<SparseDigitalWaveform*>(pdat);
	auto udigdat = dynamic_cast<UniformDigitalWaveform*>(pdat);
	if(sandat)
	{
		comp->BindBufferNonblocking(1, sandat->m_samples, *data->m_vkCmdBuf);
		comp->BindBufferNonblocking(2, sandat->m_offsets, *data->m_vkCmdBuf);
		comp->BindBufferNonblocking(3, data->m_indexBuffer, *data->m_vkCmdBuf);

		if (data->ShouldMapDurations())
			comp->BindBufferNonblocking(4, sandat->m_durations, *data->m_vkCmdBuf);
	}
	else if(uandat)
	{
		comp->BindBufferNonblocking(1, uandat->m_samples, *data->m_vkCmdBuf);
	}
	else if(sdigdat)
	{
		comp->BindBufferNonblocking(1, sdigdat->m_samples, *data->m_vkCmdBuf);
		comp->BindBufferNonblocking(2, sdigdat->m_offsets, *data->m_vkCmdBuf);
		comp->BindBufferNonblocking(3, data->m_indexBuffer, *data->m_vkCmdBuf);

		if (data->ShouldMapDurations())
			comp->BindBufferNonblocking(4, sdigdat->m_durations, *data->m_vkCmdBuf);
	}
	else if(udigdat)
	{
		comp->BindBufferNonblocking(1, udigdat->m_samples, *data->m_vkCmdBuf);
	}
	comp->Dispatch(*data->m_vkCmdBuf, data->m_config, numGroups, 1, 1);
	comp->AddComputeMemoryBarrier(*data->m_vkCmdBuf);
	data->m_vkCmdBuf->end();
	m_parent->m_vkQueue->SubmitAndBlock(*data->m_vkCmdBuf);
	data->m_renderedWaveform.MarkModifiedFromGpu();

	//Copy the rendered waveform data to a GL Texture for compositing
	data->m_renderedWaveform.PrepareForCpuAccess();
	data->m_waveformTexture.Bind();
	ResetTextureFiltering();
	data->m_waveformTexture.SetData(m_width, m_height,
		data->m_renderedWaveform.GetCpuPointer(),
		GL_RED,
		GL_FLOAT,
		GL_RGBA32F);
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
	return pix / m_pixelsPerYAxisUnit;
}

float WaveformArea::YAxisUnitsToPixels(float volt)
{
	return volt * m_pixelsPerYAxisUnit;
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
