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
	@brief  Declaration of ShaderStorageBuffer
 */
#ifndef ShaderStorageBuffer_h
#define ShaderStorageBuffer_h

/**
	@brief An OpenGL SSBO.

	No virtual functions allowed, must be a POD type.
 */
class ShaderStorageBuffer
{
public:
	ShaderStorageBuffer()
	: m_handle(0)
	{}

	~ShaderStorageBuffer()
	{ Destroy(); }

	void Destroy()
	{
		if(m_handle != 0)
			glDeleteBuffers(1, &m_handle);
		m_handle = 0;
	}

	operator GLuint() const
	{ return m_handle; }

	void Bind()
	{
		LazyInit();
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_handle);
	}

	void BindBase(GLuint i)
	{ glBindBufferBase(GL_SHADER_STORAGE_BUFFER, i, m_handle); }

	static void BulkInit(std::vector<ShaderStorageBuffer*>& arr);

protected:

	/**
		@brief Lazily creates the VAO
	 */
	void LazyInit()
	{
		if(!m_handle)
			glGenBuffers(1, &m_handle);
	}

	GLuint	m_handle;
};

#endif
