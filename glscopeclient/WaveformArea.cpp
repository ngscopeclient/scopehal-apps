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

using namespace std;

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

	//Load the ADC data
	vector<float> waveform;
	FILE* fp = fopen("/tmp/adc-dump.bin", "rb");
	int n = 0;
	while(!feof(fp))
	{
		int c1 = fgetc(fp);
		int c2 = fgetc(fp);

		waveform.push_back(((c1 << 8) | c2) / 65535.0f);
	}
	const int blocksize = 2048;
	int nblocks = waveform.size() / blocksize;

	float* verts = new float[blocksize*3];

	//Normal distribution
	random_device rd{};
    mt19937 gen{rd()};
    normal_distribution<> norm{0, 1};

	//Create VAOs/VBOs
	size_t ptr = 0;
	for(int i=0; i<nblocks; i++)
	{
		VertexArray* va = new VertexArray;
		va->Bind();
		VertexBuffer* vb = new VertexBuffer;
		vb->Bind();

		//Skip samples until we hit a rising-edge trigger point
		//First, wait until we go below the trigger
		float thresh = 0.1;
		for(; ptr < waveform.size() && waveform[ptr] > thresh; ptr ++)
		{}

		//Then wait until we hit it
		for(; ptr < waveform.size() && waveform[ptr] < thresh; ptr ++)
		{}

		//If at end of buffer, stop
		if((ptr + blocksize) >= waveform.size())
			break;

		//Fake trigger jitter
		float jitter = norm(gen);
		if(i == 0)
			jitter = 0;

		//Create geometry
		for(int j=0; j<blocksize; j++)
		{
			verts[j*3] 		= 50 + j * 0.5f + 10*jitter;
			verts[j*3 + 1]	= 50 + waveform[ptr++] * 1200.0f;
			verts[j*3 + 2]	= 0;
		}

		//Set the pointers
		glBufferData(GL_ARRAY_BUFFER, sizeof(float)*3*blocksize, verts, GL_STATIC_DRAW);
		m_defaultProgram.EnableVertexArray("vert");
		m_defaultProgram.SetVertexAttribPointer("vert");

		//Save buffers
		m_traceVAOs.push_back(va);
		m_traceVBOs.push_back(vb);
	}
	LogDebug("%d total buffers created\n", (int)m_traceVAOs.size());

	delete[] verts;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

void WaveformArea::on_resize(int width, int height)
{
	m_width = width;
	m_height = height;

	//Reset camera configuration
	glViewport(0, 0, width, height);

	//transformation matrix from screen to pixel coordinates
	m_projection = glm::translate(
		glm::scale(glm::mat4(1.0f), glm::vec3(2.0f / width, 2.0f / height, 1)),	//scale to window size
		glm::vec3(-width/2, -height/2, 0)											//put origin at bottom left
		);
}

bool WaveformArea::on_render(const Glib::RefPtr<Gdk::GLContext>& context)
{
	//Start with a blank window
	glClear(GL_COLOR_BUFFER_BIT);

	//Set up blending
	glEnable(GL_BLEND);
	//glEnable(GL_LINE_SMOOTH);
	//glEnable(GL_MULTISAMPLE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_FRAMEBUFFER_SRGB);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_MAX);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//Configure our shader and projection matrix
	m_defaultProgram.Bind();
	m_defaultProgram.SetUniform(m_projection, "projection");

	//Actually draw the waveform
	for(int i=0; i<m_traceVAOs.size(); i++)
	{
		m_traceVAOs[i]->Bind();
		m_traceVBOs[i]->Bind();

		float fage = 1.0f - (i * 1.0f / m_traceVAOs.size());

		//Set the color decay value (quadratic decay with current trace extra bright)
		float alpha = 1;
		if(i > 0)
			alpha = pow(fage, 2) * 0.01f;
		m_defaultProgram.SetUniform(alpha, "alpha");

		//Draw it
		glDrawArrays(GL_LINE_STRIP, 0, 2048);
	}

	//Sanity check
	int err = glGetError();
	if(err != 0)
		LogNotice("err = %x\n", err);

	return true;
}
