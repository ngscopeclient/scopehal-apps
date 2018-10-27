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
#include <GL/gl.h>
#include <GL/glu.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace std;

WaveformArea::WaveformArea()
{
	FILE* fp = fopen("/tmp/adc-dump.bin", "rb");
	int n = 0;
	while(!feof(fp))
	{
		int c1 = fgetc(fp);
		int c2 = fgetc(fp);

		m_waveformData.push_back(((c1 << 8) | c2) / 65535.0f);
	}
}

WaveformArea::~WaveformArea()
{

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
	m_defaultVertexShader = glCreateShader(GL_VERTEX_SHADER);
	if(!LoadShader(m_defaultVertexShader, "default-vertex.glsl"))
	{
		LogError("failed to load default vertex shader, aborting");
		exit(1);
	}

	m_defaultFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	if(!LoadShader(m_defaultFragmentShader, "default-fragment.glsl"))
	{
		LogError("failed to load default fragment shader, aborting");
		exit(1);
	}

	//Create the program
	m_defaultProgram = glCreateProgram();
	if(!LinkProgram(m_defaultProgram, m_defaultFragmentShader, m_defaultVertexShader))
	{
		LogError("failed to link shader program, aborting");
		exit(1);
	}

	//Create vertex array object and vertex buffer object
	glGenVertexArrays(1, &m_defaultArray);
	glBindVertexArray(m_defaultArray);

	glGenBuffers(1, &m_defaultBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, m_defaultBuffer);

	//Create a buffer with a bunch of waveform data in it
	const int NUM_VERTS = 4096;
	float* verts = new float[NUM_VERTS * 3];
	for(int i=0; i<NUM_VERTS; i++)
	{
		verts[i*3] 		= 50 + i * 0.25f;
		verts[i*3 + 1]	= 50 + m_waveformData[i] * 800.0f;
		verts[i*3 + 2]	= 0;
	}

	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*NUM_VERTS*3, verts, GL_STATIC_DRAW);

	delete[] verts;

	int vertIndex = glGetAttribLocation(m_defaultProgram, "vert");
	LogDebug("vert is at index %d\n", vertIndex);

	glEnableVertexAttribArray(vertIndex);
	glVertexAttribPointer(vertIndex, 3, GL_FLOAT, GL_FALSE, 0, NULL);
}

string WaveformArea::GetFileContents(string path)
{
	FILE* fp = fopen(path.c_str(), "rb");
	if(!fp)
	{
		LogWarning("GetFileContents: Could not open file \"%s\"\n", path.c_str());
		return "";
	}
	fseek(fp, 0, SEEK_END);
	size_t fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char* buf = new char[fsize];
	if(fsize != fread(buf, 1, fsize, fp))
	{
		LogWarning("GetFileContents: Could not read file \"%s\"\n", path.c_str());
		delete[] buf;
		fclose(fp);
		return "";
	}
	fclose(fp);
	string tmp(buf, fsize);		//use range constructor since file may contain null bytes
	delete[] buf;
	return tmp;
}

bool WaveformArea::LinkProgram(int program, int fragment, int vertex)
{
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);

	int status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if(status == GL_TRUE)
		return true;

	//Compile failed, return error
	char log[4096];
	int len;
	glGetProgramInfoLog(program, sizeof(log), &len, log);
	LogError("Link of shader progam failed:\n%s\n", log);

	return false;
}

bool WaveformArea::LoadShader(int shader, string path)
{
	//Load and compile the shader
	string ssrc = GetFileContents(path);
	const char* src = ssrc.c_str();
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);

	//Check status
	int status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if(status == GL_TRUE)
		return true;

	//Compile failed, return error
	char log[4096];
	int len;
	glGetShaderInfoLog(shader, sizeof(log), &len, log);
	LogError("Compile of shader %s failed:\n%s\n", path.c_str(), log);

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

void WaveformArea::on_resize(int width, int height)
{
	//Reset camera configuration
	glViewport(0, 0, width, height);
	LogDebug("window is %d x %d\n", width, height);
}

bool WaveformArea::on_render(const Glib::RefPtr<Gdk::GLContext>& context)
{
	int width = get_allocated_width();
	int height = get_allocated_height();

	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(m_defaultProgram);

	//use pixel coordinates
	glm::mat4 projection =
		glm::translate(
			glm::scale(glm::mat4(1.0f), glm::vec3(2.0f / width, 2.0f / height, 1)),	//scale to window size
			glm::vec3(-width/2, -height/2, 0)											//put origin at bottom left
		);

	int projIndex = glGetUniformLocation(m_defaultProgram, "projection");
	LogDebug("projection is at index %d\n", projIndex);
	glUniformMatrix4fv(projIndex, 1, GL_FALSE, glm::value_ptr(projection));

	glDrawArrays(GL_LINE_STRIP, 0, 4096);

	//Initialize projection
	/*glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0, width, 0, height);*/

	int err = glGetError();
	LogNotice("err = %x\n", err);

	return true;
}
