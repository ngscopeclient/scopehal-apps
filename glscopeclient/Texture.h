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
	@brief  Declaration of Texture
 */
#ifndef Texture_h
#define Texture_h

/**
	@brief A texture object
 */
class Texture
{
public:
	Texture();
	virtual ~Texture();

	operator GLuint()
	{ return m_handle; }

	void Bind(GLenum target = GL_TEXTURE_2D)
	{
		LazyInit();
		glBindTexture(target, m_handle);
	}

	//we must be bound to use these functions
	void SetData(
		size_t width,
		size_t height,
		void* data = NULL,
		GLenum format = GL_RGBA,
		GLenum type = GL_UNSIGNED_BYTE,
		GLint internalformat = GL_RGBA8,
		GLenum target = GL_TEXTURE_2D,
		int mipmap = 0
		)
	{
		glTexImage2D(target, mipmap, internalformat, width, height, 0, format, type, data);
	}
	void AllocateMultisample(
		size_t width,
		size_t height,
		int samples = 4,
		GLint internalformat = GL_RGBA32F,
		GLenum target = GL_TEXTURE_2D_MULTISAMPLE,
		GLboolean fixed_sample = GL_FALSE
		)
	{
		glTexImage2DMultisample(target, samples, internalformat, width, height, fixed_sample);
	}

protected:

	/**
		@brief Lazily creates the FBO
	 */
	void LazyInit()
	{
		if(!m_handle)
			glGenTextures(1, &m_handle);
	}

	GLuint	m_handle;
};

#endif
