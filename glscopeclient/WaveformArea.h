/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2019 Andrew D. Zonenberg                                                                          *
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
	WaveformArea(Oscilloscope* scope, OscilloscopeChannel* channel, OscilloscopeWindow* parent, Gdk::Color color);
	virtual ~WaveformArea();

	void OnWaveformDataReady();

	OscilloscopeChannel* GetChannel()
	{ return m_channel; }

protected:
	virtual void on_realize();
	virtual void on_resize (int width, int height);
	virtual bool on_render(const Glib::RefPtr<Gdk::GLContext>& context);
	virtual bool on_button_press_event(GdkEventButton* event);
	virtual bool on_scroll_event (GdkEventScroll* ev);

	//Context menu
	Gtk::Menu m_contextMenu;
		Gtk::CheckMenuItem m_persistenceItem;
		Gtk::MenuItem m_decodeItem;
			Gtk::Menu m_decodeMenu;
		Gtk::MenuItem m_triggerItem;
			Gtk::Menu m_triggerMenu;
		Gtk::MenuItem m_couplingItem;
			Gtk::Menu m_couplingMenu;
				Gtk::RadioMenuItem::Group m_couplingGroup;
				Gtk::RadioMenuItem m_dc50CouplingItem;
				Gtk::RadioMenuItem m_dc1MCouplingItem;
				Gtk::RadioMenuItem m_ac1MCouplingItem;
				Gtk::RadioMenuItem m_gndCouplingItem;
		Gtk::MenuItem m_attenItem;
			Gtk::Menu m_attenMenu;
				Gtk::RadioMenuItem::Group m_attenGroup;
					Gtk::RadioMenuItem m_atten1xItem;
					Gtk::RadioMenuItem m_atten10xItem;
					Gtk::RadioMenuItem m_atten20xItem;
		Gtk::MenuItem m_bwItem;
			Gtk::Menu m_bwMenu;
				Gtk::RadioMenuItem::Group m_bwGroup;
					Gtk::RadioMenuItem m_bwFullItem;
					Gtk::RadioMenuItem m_bw200Item;
					Gtk::RadioMenuItem m_bw20Item;
	void UpdateContextMenu();
	bool m_updatingContextMenu;
	void OnHide();
	void OnTogglePersistence();
	void OnProtocolDecode(std::string name);
	void OnBandwidthLimit(int mhz, Gtk::RadioMenuItem* item);

	int m_width;
	int m_height;

	//Display options
	bool m_persistence;

	//GL stuff (TODO organize)
	Program m_waveformProgram;
	Framebuffer m_windowFramebuffer;

	//Trace rendering
	bool PrepareGeometry();
	void RenderTrace();
	void InitializeWaveformPass();
	std::vector<VertexBuffer*> m_traceVBOs;
	std::vector<VertexArray*> m_traceVAOs;
	glm::mat4 m_projection;
	size_t m_waveformLength;

	//Color correction
	void RenderTraceColorCorrection();
	void InitializeColormapPass();
	VertexArray m_colormapVAO;
	VertexBuffer m_colormapVBO;
	Program m_colormapProgram;
	Gdk::Color m_color;
	Framebuffer m_waveformFramebuffer;
	Texture m_waveformTexture;

	//Persistence
	void RenderPersistence();
	void InitializePersistencePass();
	Program m_persistProgram;
	VertexArray m_persistVAO;
	VertexBuffer m_persistVBO;
	Framebuffer m_persistbuffer;
	Texture m_persistTexture;

	//Cairo overlay rendering for text and protocol decode overlays
	void RenderCairoUnderlays();
	void RenderCairoOverlays();
	void InitializeCairoPass();
	Texture m_cairoTexture;
	VertexArray m_cairoVAO;
	VertexBuffer m_cairoVBO;
	Program m_cairoProgram;

	Oscilloscope* m_scope;
	OscilloscopeChannel* m_channel;
	OscilloscopeChannel* m_selectedChannel;
	OscilloscopeWindow* m_parent;

	double m_frameTime;
	long m_frameCount;
};

#endif
