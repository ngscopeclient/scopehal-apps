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
	@brief Main project include file
 */
#ifndef glscopeclient_h
#define glscopeclient_h

#ifndef __APPLE__
    #define GL_GLEXT_PROTOTYPES
#endif

#include <GL/glew.h>

#ifdef __APPLE__
    #include <OpenGL/gl.h>
#else
    #include <GL/gl.h>
#endif

#include "../scopehal/scopehal.h"
#include "../scopehal/Instrument.h"
#include "../scopehal/Multimeter.h"
#include "../scopehal/OscilloscopeChannel.h"
#include "../scopehal/Filter.h"

#include <locale.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <vector>
#include "Event.h"

#include <giomm.h>
#include <gtkmm.h>

#ifdef _WIN32
#include <GL/glcorearb.h>
#include <GL/glext.h>
#endif

#include "Framebuffer.h"
#include "Program.h"
#include "Shader.h"
#include "ShaderStorageBuffer.h"
#include "Texture.h"
#include "VertexArray.h"
#include "VertexBuffer.h"

#include "OscilloscopeWindow.h"
#include "ScopeApp.h"

double GetTime();
extern bool g_noglint64;

void WaveformProcessingThread(OscilloscopeWindow* window);

extern Event g_waveformReadyEvent;
extern Event g_waveformProcessedEvent;

extern char* g_defaultNumLocale;

#endif
