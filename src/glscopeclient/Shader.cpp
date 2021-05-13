/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
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
	@brief  Implementation of Shader
 */
#include "glscopeclient.h"
#include "Shader.h"
#include <stdarg.h>

using namespace std;

Shader::Shader(GLenum type)
	: m_handle(glCreateShader(type))
{
	if(m_handle == 0)
		LogError("Failed to create shader (of type %d)\n", type);
}

Shader::~Shader()
{
	glDeleteShader(m_handle);
}

/**
	@brief Loads a shader from disk

	We accept arbitrarily many paths and concatenate their contents
 */
bool Shader::Load(const char* path, ...)
{
	string shaderbuf = ReadDataFile(path);

	va_list list;
	va_start(list, path);
	while(true)
	{
		const char* arg = va_arg(list, const char*);
		if(arg == NULL)
			break;
		else if(arg[0] == '#')
			shaderbuf += string(arg) + "\n";
		else
			shaderbuf += ReadDataFile(arg);
	}
	va_end(list);

	//Compile the shader
	const char* bufs[1] = { shaderbuf.c_str() };
	glShaderSource(m_handle, 1, bufs, NULL);
	glCompileShader(m_handle);

	//Check status
	int status;
	glGetShaderiv(m_handle, GL_COMPILE_STATUS, &status);
	if(status == GL_TRUE)
		return true;

	//Compile failed, return error
	char log[4096];
	int len;
	glGetShaderInfoLog(m_handle, sizeof(log), &len, log);
	LogError("Compile of shader %s failed:\n%s\n", path, log);
	LogNotice("Shader source: %s\n", shaderbuf.c_str());

	return false;
}
