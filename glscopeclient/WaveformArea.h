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
	@brief  Declaration of WaveformArea
 */
#ifndef WaveformArea_h
#define WaveformArea_h

float sinc(float x, float width);
float blackman(float x, float width);

class OscilloscopeWindow;

class WaveformArea : public Gtk::GLArea
{
public:
	WaveformArea(Oscilloscope* scope, OscilloscopeChannel* channel, OscilloscopeWindow* parent);
	virtual ~WaveformArea();

	void OnWaveformDataReady();

protected:
	virtual void on_realize();
	virtual void on_resize (int width, int height);
	virtual bool on_button_press_event(GdkEventButton* event);
	bool PrepareGeometry();

	void OnHide();

	//Context menu
	Gtk::Menu m_contextMenu;

	//Rendering
	virtual bool on_render(const Glib::RefPtr<Gdk::GLContext>& context);
	void RenderTrace();
	void RenderTraceColorCorrection();
	void RenderPersistence();

	//GL stuff (TODO organize)
	Program m_defaultProgram;
	Framebuffer m_windowFramebuffer;

	Framebuffer m_framebuffer;
	Texture m_fboTexture;

	Framebuffer m_persistbuffer;
	Texture m_persistTexture;

	std::vector<VertexBuffer*> m_traceVBOs;
	std::vector<VertexArray*> m_traceVAOs;

	glm::mat4 m_projection;

	int m_width;
	int m_height;
	size_t m_waveformLength;

	double m_frameTime;
	long m_frameCount;

	//Final rendering pass:
	void InitializeColormapPass();
	VertexArray m_colormapVAO;
	VertexBuffer m_colormapVBO;
	Program m_colormapProgram;

	void InitializePersistencePass();
	Program m_persistProgram;
	VertexArray m_persistVAO;
	VertexBuffer m_persistVBO;

	Oscilloscope* m_scope;
	OscilloscopeChannel* m_channel;
	OscilloscopeWindow* m_parent;
};

#endif
