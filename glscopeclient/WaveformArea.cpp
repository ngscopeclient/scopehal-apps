/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2018 Andrew D. Zonenberg                                                                          *
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
	@brief  Implementation of WaveformArea
 */
#include "glscopeclient.h"
#include "WaveformArea.h"
#include <random>

/*
dvec2 nvec;

//First point in the line? Our normal is perpendicular to the vector from us to the next point
if(j == 0)
{
	dvec2 delta = points[1] - points[0];
	nvec = normalize(dvec2(delta.y, -delta.x));
}

//Last point? Same deal
else if(j == (BLOCK_SIZE-1))
{
	dvec2 delta = points[j] - points[j-1];
	nvec = normalize(dvec2(delta.y, -delta.x));
}

//Nope, we're a midpoint.
//Use the midpoint of the two normals
else
{
	dvec2 delta1 = normalize(points[j] - points[j-1]);
	dvec2 delta2 = normalize(points[j+1] - points[j]);
	dvec2 tangent = normalize(delta1 + delta2);			//tangent to the ideal lines
	nvec = dvec2(-tangent.y, tangent.x);
}

//Offset start/end positions by the normal
dvec2 p1 = points[j] + (nvec * hwidth);
dvec2 p2 = points[j] - (nvec * hwidth);
*/

#define BLOCK_SIZE 1024

using namespace std;
using namespace glm;

WaveformArea::WaveformArea()
{
	set_has_alpha();
}

WaveformArea::~WaveformArea()
{
	for(auto a : m_traceVAOs)
		delete a;
	for(auto b : m_traceVBOs)
		delete b;
	m_traceVAOs.clear();
	m_traceVBOs.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialization

void WaveformArea::on_realize()
{
	//Call base class to create the GL context, then select it
	Gtk::GLArea::on_realize();
	make_current();

	//Do global initialization (independent of camera settings etc)
	glClearColor(0, 0, 0, 1.0);

	double start = GetTime();

	//Create shader objects
	VertexShader vs;
	FragmentShader fs;
	if(!vs.Load("default-vertex.glsl") || !fs.Load("default-fragment.glsl"))
	{
		LogError("failed to load default shaders, aborting");
		exit(1);
	}

	//Create the program
	m_defaultProgram.Add(vs);
	m_defaultProgram.Add(fs);
	if(!m_defaultProgram.Link())
	{
		LogError("failed to link shader program, aborting");
		exit(1);
	}

	double dt = GetTime() - start;
	LogDebug("Shader load: %.3f ms\n", dt * 1000);

	//Load the ADC data
	start = GetTime();
	FILE* fp = fopen("/tmp/adc-dump.bin", "rb");
	fseek(fp, 0, SEEK_END);
	long len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	vector<uint8_t> rawdata;
	rawdata.resize(len);
	fread(&rawdata[0], 1, len, fp);
	fclose(fp);
	dt = GetTime() - start;
	start = GetTime();
	LogDebug("File read: %.3f ms\n", dt * 1000);

	//Crunch the ADC data
	vector<float> waveform;
	waveform.resize(len/2);
	size_t nsamples = len/2;
	int n = 0;
	#pragma omp parallel for
	for(size_t i=0; i<rawdata.size(); i += 2)
	{
		float code_raw = ((rawdata[i] << 8) | rawdata[i+1]);
		waveform[i/2] = code_raw / 65535.0f;
	}
	int nblocks = nsamples / BLOCK_SIZE;
	dt = GetTime() - start;
	start = GetTime();
	LogDebug("Floating point conversion: %.3f ms\n", dt * 1000);

	float* verts = new float[BLOCK_SIZE*2*2];

	//Create VAOs/VBOs
	//TODO: this preprocessing should be optimized and/or moved to a shader
	size_t ptr = 0;
	for(int i=0; i<nblocks; i++)
	{
		VertexArray* va = new VertexArray;
		va->Bind();
		VertexBuffer* vb = new VertexBuffer;
		vb->Bind();

		//Skip samples until we hit a rising-edge trigger point
		//Wait until we go below the trigger, then back up
		float thresh = 0.0007;
		for(; ptr < nsamples && waveform[ptr] > thresh; ptr ++)
		{}
		for(; ptr < nsamples && waveform[ptr] < thresh; ptr ++)
		{}
		if((ptr + BLOCK_SIZE) >= nsamples)
			break;

		//Create geometry
		double lheight = 1.0f / (65535 * 2);	//one ADC code
		size_t base = ptr;
		size_t voff = 0;
		for(int j=0; j<BLOCK_SIZE; j++)
		{
			float y = waveform[base + j];

			//Rather than using a generalized line drawing algorithm, we can cheat!
			//Add some height to the samples
			verts[voff + 0] = j;
			verts[voff + 1] = y + lheight;

			verts[voff + 2] = j;
			verts[voff + 3] = y - lheight;

			voff += 4;
		}
		ptr += BLOCK_SIZE;

		//Set the pointers
		glBufferData(GL_ARRAY_BUFFER, sizeof(float)*2*2*BLOCK_SIZE, verts, GL_STATIC_DRAW);
		m_defaultProgram.EnableVertexArray("vert");
		m_defaultProgram.SetVertexAttribPointer("vert", 2);

		//Save buffers
		m_traceVAOs.push_back(va);
		m_traceVBOs.push_back(vb);
	}
	dt = GetTime() - start;
	start = GetTime();
	size_t nbufs = m_traceVAOs.size();
	size_t nsamps = nbufs * BLOCK_SIZE;
	LogDebug("Created %d buffers (%d samples) in %.3f ms (%.2f kWFM/s, %.2f MSps)\n",
		nbufs,
		nsamps,
		dt * 1000,
		nbufs / (1e3 * dt),
		nsamps / (1e6 * dt)
		);

	delete[] verts;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

void WaveformArea::on_resize(int width, int height)
{
	double start = GetTime();

	m_width = width;
	m_height = height;

	//Reset camera configuration
	glViewport(0, 0, width, height);

	//transformation matrix from screen to pixel coordinates
	m_projection = translate(
		scale(mat4(1.0f), vec3(2.0f / width, 2.0f / height, 1)),	//scale to window size
		vec3(-width/2, -height/2, 0)											//put origin at bottom left
		);

	//Initialize the color buffer
	//TODO: make MSAA config not hard coded
	const bool multisample = false;//true;
	const int numSamples = 4;
	m_framebuffer.Bind(GL_FRAMEBUFFER);
	if(multisample)
	{
		m_fboTexture.Bind(GL_TEXTURE_2D_MULTISAMPLE);
		m_fboTexture.AllocateMultisample(width, height, numSamples, GL_RGBA32F);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, m_framebuffer, 0);
	}
	else
	{
		m_fboTexture.Bind();
		m_fboTexture.SetData(width, height, NULL, GL_RGBA, GL_UNSIGNED_BYTE, GL_RGBA32F);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_framebuffer, 0);
	}

	if(!m_framebuffer.IsComplete())
		LogError("FBO is incomplete: %x\n", glCheckFramebufferStatus(GL_FRAMEBUFFER));

	int err = glGetError();
	if(err != 0)
		LogNotice("resize, err = %x\n", err);

	double dt = GetTime() - start;
	LogDebug("Resize time: %.3f ms\n", dt*1000);
}

bool WaveformArea::on_render(const Glib::RefPtr<Gdk::GLContext>& context)
{
	static double last = -1;

	double start = GetTime();
	double dt = start - last;
	if(last > 0)
		LogDebug("Frame time: %.3f ms (%.2f FPS)\n", dt*1000, 1/dt);
	last = start;

	//Something funky is going on. Why do we get correct results blitting to framebuffer ONE,
	//and nothing showing up when we blit to framebuffer ZERO?
	//According to the GL spec, FBO 0 should be the default.
	const int windowFramebuffer = 1;

	//Draw to the offscreen floating-point framebuffer.
	m_framebuffer.Bind(GL_FRAMEBUFFER);

	//Start with a blank window
	glClear(GL_COLOR_BUFFER_BIT);

	//Set up blending
	glEnable(GL_BLEND);
	//glEnable(GL_MULTISAMPLE);
	glDisable(GL_MULTISAMPLE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_FRAMEBUFFER_SRGB);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_MAX);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//Configure our shader and projection matrix
	m_defaultProgram.Bind();
	m_defaultProgram.SetUniform(m_projection, "projection");
	m_defaultProgram.SetUniform(50.0f, "xoff");
	m_defaultProgram.SetUniform(1.0f, "xscale");
	m_defaultProgram.SetUniform(-50.0f, "yoff");
	m_defaultProgram.SetUniform(1000000.0f, "yscale");

	//Set the color decay value (constant for now)
	m_defaultProgram.SetUniform(0.002f, "alpha");

	//Actually draw the waveform
	for(int i=0; i<m_traceVAOs.size(); i++)
	{
		m_traceVAOs[i]->Bind();
		m_traceVBOs[i]->Bind();

		//Draw it
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 2*BLOCK_SIZE);
	}

	//Once the rendering proper is complete, blit the floating-point buffer into our onscreen buffer
	m_framebuffer.Bind(GL_READ_FRAMEBUFFER);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, windowFramebuffer);
	glBlitFramebuffer(
		0, 0, m_width, m_height,
		0, 0, m_width, m_height,
		GL_COLOR_BUFFER_BIT,
		GL_NEAREST);

	//Sanity check
	int err = glGetError();
	if(err != 0)
		LogNotice("err = %x\n", err);

	queue_draw();
	return true;
}
