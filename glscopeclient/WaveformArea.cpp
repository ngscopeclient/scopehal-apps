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
	@brief  Implementation of WaveformArea
 */
#include "glscopeclient.h"
#include "WaveformArea.h"
#include "OscilloscopeWindow.h"
#include <random>
#include "ProfileBlock.h"

using namespace std;
using namespace glm;

/*
static const RGBQUAD g_eyeColorScale[256] =
{
	{   0,   0,   0, 0   },     {   4,   2,  20, 255 },     {   7,   4,  35, 255 },     {   9,   5,  45, 255 },
    {  10,   6,  53, 255 },     {  11,   7,  60, 255 },     {  13,   7,  66, 255 },     {  14,   8,  71, 255 },
    {  14,   8,  75, 255 },     {  16,  10,  80, 255 },     {  16,  10,  85, 255 },     {  17,  10,  88, 255 },
    {  18,  11,  92, 255 },     {  19,  11,  95, 255 },     {  19,  12,  98, 255 },     {  20,  12, 102, 255 },
    {  20,  13, 104, 255 },     {  20,  13, 107, 255 },     {  21,  13, 110, 255 },     {  21,  13, 112, 255 },
    {  23,  14, 114, 255 },     {  23,  14, 117, 255 },     {  23,  14, 118, 255 },     {  23,  14, 121, 255 },
    {  23,  15, 122, 255 },     {  24,  15, 124, 255 },     {  24,  15, 126, 255 },     {  24,  14, 127, 255 },
    {  25,  15, 129, 255 },     {  25,  15, 130, 255 },     {  25,  16, 131, 255 },     {  26,  16, 132, 255 },
    {  26,  15, 134, 255 },     {  27,  16, 136, 255 },     {  26,  16, 136, 255 },     {  26,  16, 137, 255 },
    {  27,  16, 138, 255 },     {  26,  16, 138, 255 },     {  26,  16, 140, 255 },     {  27,  16, 141, 255 },
    {  27,  16, 141, 255 },     {  28,  17, 142, 255 },     {  27,  17, 142, 255 },     {  27,  16, 143, 255 },
    {  28,  17, 144, 255 },     {  28,  17, 144, 255 },     {  28,  17, 144, 255 },     {  28,  17, 144, 255 },
    {  28,  17, 144, 255 },     {  28,  17, 145, 255 },     {  28,  17, 145, 255 },     {  28,  17, 145, 255 },
    {  28,  17, 145, 255 },     {  30,  17, 144, 255 },     {  32,  17, 143, 255 },     {  34,  17, 142, 255 },
    {  35,  16, 140, 255 },     {  37,  17, 139, 255 },     {  38,  16, 138, 255 },     {  40,  17, 136, 255 },
    {  42,  16, 136, 255 },     {  44,  16, 134, 255 },     {  46,  17, 133, 255 },     {  47,  16, 133, 255 },
    {  49,  16, 131, 255 },     {  51,  16, 130, 255 },     {  53,  17, 129, 255 },     {  54,  16, 128, 255 },
    {  56,  16, 127, 255 },     {  58,  16, 126, 255 },     {  60,  16, 125, 255 },     {  62,  16, 123, 255 },
    {  63,  16, 122, 255 },     {  65,  16, 121, 255 },     {  67,  16, 120, 255 },     {  69,  16, 119, 255 },
    {  70,  16, 117, 255 },     {  72,  16, 116, 255 },     {  74,  16, 115, 255 },     {  75,  15, 114, 255 },
    {  78,  16, 113, 255 },     {  79,  16, 112, 255 },     {  81,  16, 110, 255 },     {  83,  15, 110, 255 },
    {  84,  15, 108, 255 },     {  86,  16, 108, 255 },     {  88,  15, 106, 255 },     {  90,  15, 105, 255 },
    {  91,  16, 103, 255 },     {  93,  15, 103, 255 },     {  95,  15, 102, 255 },     {  96,  15, 100, 255 },
    {  98,  15, 100, 255 },     { 100,  15,  98, 255 },     { 101,  15,  97, 255 },     { 104,  15,  96, 255 },
    { 106,  15,  95, 255 },     { 107,  15,  94, 255 },     { 109,  14,  92, 255 },     { 111,  14,  92, 255 },
    { 112,  15,  90, 255 },     { 114,  14,  89, 255 },     { 116,  15,  87, 255 },     { 118,  14,  87, 255 },
    { 119,  14,  86, 255 },     { 121,  14,  85, 255 },     { 123,  14,  83, 255 },     { 124,  14,  83, 255 },
    { 126,  15,  81, 255 },     { 128,  14,  80, 255 },     { 130,  14,  78, 255 },     { 132,  14,  77, 255 },
    { 134,  14,  76, 255 },     { 137,  14,  74, 255 },     { 139,  14,  73, 255 },     { 141,  14,  71, 255 },
    { 143,  13,  70, 255 },     { 146,  13,  68, 255 },     { 148,  14,  67, 255 },     { 150,  13,  65, 255 },
    { 153,  14,  64, 255 },     { 155,  14,  62, 255 },     { 157,  13,  61, 255 },     { 159,  13,  60, 255 },
    { 162,  13,  58, 255 },     { 165,  13,  56, 255 },     { 166,  13,  55, 255 },     { 169,  13,  54, 255 },
    { 171,  13,  52, 255 },     { 173,  13,  50, 255 },     { 176,  13,  48, 255 },     { 179,  12,  47, 255 },
    { 181,  12,  45, 255 },     { 183,  12,  45, 255 },     { 185,  12,  43, 255 },     { 188,  13,  41, 255 },
    { 190,  12,  40, 255 },     { 192,  12,  38, 255 },     { 194,  13,  37, 255 },     { 197,  12,  35, 255 },
    { 199,  12,  33, 255 },     { 201,  12,  32, 255 },     { 204,  12,  30, 255 },     { 206,  12,  29, 255 },
    { 209,  12,  28, 255 },     { 211,  12,  26, 255 },     { 213,  12,  25, 255 },     { 216,  12,  23, 255 },
    { 218,  11,  22, 255 },     { 221,  12,  20, 255 },     { 223,  11,  18, 255 },     { 224,  11,  17, 255 },
    { 227,  11,  16, 255 },     { 230,  11,  14, 255 },     { 231,  11,  12, 255 },     { 234,  12,  11, 255 },
    { 235,  13,  10, 255 },     { 235,  15,  11, 255 },     { 235,  17,  11, 255 },     { 235,  19,  11, 255 },
    { 236,  21,  10, 255 },     { 236,  23,  10, 255 },     { 237,  24,  10, 255 },     { 237,  26,  10, 255 },
    { 236,  28,   9, 255 },     { 237,  30,  10, 255 },     { 237,  32,   9, 255 },     { 238,  34,   9, 255 },
    { 238,  35,   9, 255 },     { 238,  38,   8, 255 },     { 239,  39,   9, 255 },     { 239,  42,   8, 255 },
    { 240,  44,   9, 255 },     { 240,  45,   8, 255 },     { 240,  47,   8, 255 },     { 240,  49,   8, 255 },
    { 241,  51,   7, 255 },     { 241,  53,   8, 255 },     { 241,  55,   7, 255 },     { 241,  57,   7, 255 },
    { 242,  58,   7, 255 },     { 242,  60,   7, 255 },     { 242,  62,   6, 255 },     { 243,  64,   6, 255 },
    { 244,  66,   6, 255 },     { 243,  68,   5, 255 },     { 244,  69,   6, 255 },     { 244,  71,   6, 255 },
    { 245,  74,   6, 255 },     { 245,  76,   5, 255 },     { 245,  79,   5, 255 },     { 246,  82,   5, 255 },
    { 246,  85,   5, 255 },     { 247,  87,   4, 255 },     { 247,  90,   4, 255 },     { 248,  93,   3, 255 },
    { 249,  96,   4, 255 },     { 248,  99,   3, 255 },     { 249, 102,   3, 255 },     { 250, 105,   3, 255 },
    { 250, 107,   2, 255 },     { 250, 110,   2, 255 },     { 251, 113,   2, 255 },     { 252, 115,   1, 255 },
    { 252, 118,   2, 255 },     { 253, 121,   1, 255 },     { 253, 124,   1, 255 },     { 253, 126,   1, 255 },
    { 254, 129,   0, 255 },     { 255, 132,   0, 255 },     { 255, 135,   0, 255 },     { 255, 138,   1, 255 },
    { 254, 142,   3, 255 },     { 253, 145,   4, 255 },     { 253, 148,   6, 255 },     { 252, 151,   9, 255 },
    { 252, 155,  11, 255 },     { 251, 158,  12, 255 },     { 251, 161,  14, 255 },     { 250, 163,  15, 255 },
    { 251, 165,  16, 255 },     { 250, 167,  17, 255 },     { 250, 169,  18, 255 },     { 250, 170,  19, 255 },
    { 250, 172,  20, 255 },     { 249, 174,  21, 255 },     { 249, 177,  22, 255 },     { 248, 178,  23, 255 },
    { 248, 180,  24, 255 },     { 247, 182,  25, 255 },     { 247, 184,  26, 255 },     { 247, 185,  27, 255 },
    { 247, 188,  27, 255 },     { 247, 191,  26, 255 },     { 248, 194,  25, 255 },     { 249, 197,  24, 255 },
    { 248, 200,  22, 255 },     { 249, 203,  21, 255 },     { 249, 205,  20, 255 },     { 250, 209,  18, 255 },
    { 250, 212,  18, 255 },     { 250, 214,  16, 255 },     { 251, 217,  15, 255 },     { 251, 221,  14, 255 },
    { 251, 223,  13, 255 },     { 251, 226,  12, 255 },     { 252, 229,  11, 255 },     { 253, 231,   9, 255 },
    { 253, 234,   9, 255 },     { 253, 237,   7, 255 },     { 253, 240,   6, 255 },     { 253, 243,   5, 255 },
    { 254, 246,   4, 255 },     { 254, 248,   3, 255 },     { 255, 251,   1, 255 },     { 255, 254,   1, 255 }
};
*/

WaveformArea::WaveformArea(
	Oscilloscope* scope,
	OscilloscopeChannel* channel,
	OscilloscopeWindow* parent,
	Gdk::Color color
	)
	: m_color(color)
	, m_scope(scope)
	, m_channel(channel)
	, m_parent(parent)
{
	m_frameTime = 0;
	m_frameCount = 0;
	m_persistence = false;
	m_updatingContextMenu = false;
	m_selectedChannel = m_channel;

	set_has_alpha();

	add_events(
		Gdk::EXPOSURE_MASK |
		Gdk::SCROLL_MASK |
		Gdk::BUTTON_PRESS_MASK |
		Gdk::BUTTON_RELEASE_MASK);

	//Create the menu
	auto item = Gtk::manage(new Gtk::MenuItem("Hide channel", false));
		item->signal_activate().connect(
			sigc::mem_fun(*this, &WaveformArea::OnHide));
		m_contextMenu.append(*item);
	m_contextMenu.append(*Gtk::manage(new Gtk::SeparatorMenuItem));
	m_contextMenu.append(m_persistenceItem);
		m_persistenceItem.set_label("Persistence");
		m_persistenceItem.signal_activate().connect(
			sigc::mem_fun(*this, &WaveformArea::OnTogglePersistence));
	m_contextMenu.append(*Gtk::manage(new Gtk::SeparatorMenuItem));
	m_contextMenu.append(m_triggerItem);
		m_triggerItem.set_label("Trigger");
		m_triggerItem.set_submenu(m_triggerMenu);
			item = Gtk::manage(new Gtk::RadioMenuItem("Rising edge"));
			m_triggerMenu.append(*item);
			item = Gtk::manage(new Gtk::RadioMenuItem("Falling edge"));
			m_triggerMenu.append(*item);
			item = Gtk::manage(new Gtk::RadioMenuItem("Any edge"));
			m_triggerMenu.append(*item);
			//TODO: more trigger types
	m_contextMenu.append(m_couplingItem);
		m_couplingItem.set_label("Coupling");
		m_couplingItem.set_submenu(m_couplingMenu);
			m_ac1MCouplingItem.set_label("AC 1M");
				m_ac1MCouplingItem.set_group(m_couplingGroup);
				m_couplingMenu.append(m_ac1MCouplingItem);
			m_dc1MCouplingItem.set_label("DC 1M");
				m_dc1MCouplingItem.set_group(m_couplingGroup);
				m_couplingMenu.append(m_dc1MCouplingItem);
			m_dc50CouplingItem.set_label("DC 50Ω");
				m_dc50CouplingItem.set_group(m_couplingGroup);
				m_couplingMenu.append(m_dc50CouplingItem);
			m_gndCouplingItem.set_label("GND");
				m_gndCouplingItem.set_group(m_couplingGroup);
				m_couplingMenu.append(m_gndCouplingItem);
	m_contextMenu.append(m_attenItem);
		m_attenItem.set_label("Attenuation");
		m_attenItem.set_submenu(m_attenMenu);
			m_atten1xItem.set_label("1x");
				m_atten1xItem.set_group(m_attenGroup);
				m_attenMenu.append(m_atten1xItem);
			m_atten10xItem.set_label("10x");
				m_atten10xItem.set_group(m_attenGroup);
				m_attenMenu.append(m_atten10xItem);
			m_atten20xItem.set_label("20x");
				m_atten20xItem.set_group(m_attenGroup);
				m_attenMenu.append(m_atten20xItem);
	m_contextMenu.append(*Gtk::manage(new Gtk::SeparatorMenuItem));
	m_contextMenu.append(m_decodeItem);
		m_decodeItem.set_label("Protocol decode");
		m_decodeItem.set_submenu(m_decodeMenu);
		vector<string> names;
		ProtocolDecoder::EnumProtocols(names);
		for(auto p : names)
		{
			item = Gtk::manage(new Gtk::MenuItem(p, false));
			item->signal_activate().connect(
				sigc::bind<string>(sigc::mem_fun(*this, &WaveformArea::OnProtocolDecode), p));
			m_decodeMenu.append(*item);
		}

	m_contextMenu.show_all();
}

WaveformArea::~WaveformArea()
{
	double tavg = m_frameTime / m_frameCount;
	LogDebug("Average frame time: %.3f ms (%.2f FPS)\n", tavg*1000, 1/tavg);

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
	{
		//ProfileBlock pb("Load waveform shaders");
		VertexShader dvs;
		FragmentShader dfs;
		if(!dvs.Load("default-vertex.glsl") || !dfs.Load("default-fragment.glsl"))
		{
			LogError("failed to load default shaders, aborting");
			exit(1);
		}

		//Create the programs
		m_defaultProgram.Add(dvs);
		m_defaultProgram.Add(dfs);
		if(!m_defaultProgram.Link())
		{
			LogError("failed to link shader program, aborting");
			exit(1);
		}
	}

	InitializeColormapPass();
	InitializePersistencePass();

	//Create the VAOs and VBOs
	{
		//ProfileBlock pb("VAO/VBO creation");

		m_traceVBOs.push_back(new VertexBuffer);
		m_traceVBOs[0]->Bind();
		m_traceVAOs.push_back(new VertexArray);
		m_traceVAOs[0]->Bind();
	}
}

void WaveformArea::InitializeColormapPass()
{
	//ProfileBlock pb("Load colormap shaders");

	//Set up shaders
	VertexShader cvs;
	FragmentShader cfs;
	if(!cvs.Load("colormap-vertex.glsl") || !cfs.Load("colormap-fragment.glsl"))
	{
		LogError("failed to load colormap shaders, aborting");
		exit(1);
	}

	m_colormapProgram.Add(cvs);
	m_colormapProgram.Add(cfs);
	if(!m_colormapProgram.Link())
	{
		LogError("failed to link shader program, aborting");
		exit(1);
	}

	//Create the VAO/VBO for a fullscreen polygon
	float verts[8] =
	{
		-1, -1,
		 1, -1,
		 1,  1,
		-1,  1
	};
	m_colormapVBO.Bind();
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

	m_colormapVAO.Bind();
	m_colormapProgram.EnableVertexArray("vert");
	m_colormapProgram.SetVertexAttribPointer("vert", 2, 0);
}

void WaveformArea::InitializePersistencePass()
{
	//ProfileBlock pb("Load persistence shaders");

	//Set up shaders
	VertexShader cvs;
	FragmentShader cfs;
	if(!cvs.Load("persist-vertex.glsl") || !cfs.Load("persist-fragment.glsl"))
	{
		LogError("failed to load persist shaders, aborting");
		exit(1);
	}

	m_persistProgram.Add(cvs);
	m_persistProgram.Add(cfs);
	if(!m_persistProgram.Link())
	{
		LogError("failed to link shader program, aborting");
		exit(1);
	}

	//Create the VAO/VBO for a fullscreen polygon
	float verts[8] =
	{
		-1, -1,
		 1, -1,
		 1,  1,
		-1,  1
	};
	m_persistVBO.Bind();
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

	m_persistVAO.Bind();
	m_persistProgram.EnableVertexArray("vert");
	m_persistProgram.SetVertexAttribPointer("vert", 2, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers

bool WaveformArea::on_scroll_event (GdkEventScroll* ev)
{
	switch(ev->direction)
	{
		case GDK_SCROLL_UP:
			LogDebug("zoom in\n");
			break;
		case GDK_SCROLL_DOWN:
			LogDebug("zoom out\n");
			break;
		case GDK_SCROLL_LEFT:
			LogDebug("scroll left\n");
			break;
		case GDK_SCROLL_RIGHT:
			LogDebug("scroll right\n");
			break;

		default:
			break;
	}

	return true;
}

/**
	@brief Enable/disable or show/hide context menu items for the current selection
 */
void WaveformArea::UpdateContextMenu()
{
	//Let signal handlers know to ignore any events that happen as we pull state from the scope
	m_updatingContextMenu = true;

	//Gray out decoders that don't make sense for the type of channel we've selected
	auto children = m_decodeMenu.get_children();
	for(auto item : children)
	{
		Gtk::MenuItem* menu = dynamic_cast<Gtk::MenuItem*>(item);
		if(menu == NULL)
			continue;

		auto decoder = ProtocolDecoder::CreateDecoder(
			menu->get_label(),
			"dummy",
			"");
		menu->set_sensitive(decoder->ValidateChannel(0, m_selectedChannel));
		delete decoder;
	}

	//Figure out the current trigger selection
	auto coupling = m_selectedChannel->GetCoupling();
	m_couplingItem.set_sensitive(true);
	switch(coupling)
	{
		case OscilloscopeChannel::COUPLE_DC_1M:
			m_dc1MCouplingItem.set_active(true);
			break;

		case OscilloscopeChannel::COUPLE_AC_1M:
			m_ac1MCouplingItem.set_active(true);
			break;

		case OscilloscopeChannel::COUPLE_DC_50:
			m_dc50CouplingItem.set_active(true);
			break;

		case OscilloscopeChannel::COUPLE_GND:
			m_gndCouplingItem.set_active(true);
			break;

		default:
			m_couplingItem.set_sensitive(false);
			break;
	}

	//Update the current attenuation
	int atten = static_cast<int>(m_selectedChannel->GetAttenuation());
	switch(atten)
	{
		case 1:
			m_atten1xItem.set_active(true);
			break;

		case 10:
			m_atten10xItem.set_active(true);
			break;

		case 20:
			m_atten20xItem.set_active(true);
			break;

		default:
			//TODO: how to handle this?
			break;
	}

	m_updatingContextMenu = false;
}

bool WaveformArea::on_button_press_event(GdkEventButton* event)
{
	//TODO: See if we right clicked on our main channel or a protocol decoder.
	//If a decoder, filter for that instead
	m_selectedChannel = m_channel;

	switch(event->button)
	{
		//Right
		case 3:
			UpdateContextMenu();
			m_contextMenu.popup(event->button, event->time);
			break;

		default:
			LogDebug("Button %d pressed\n", event->button);
			break;
	}

	return true;
}

void WaveformArea::OnHide()
{
	m_parent->OnToggleChannel(this);
}

void WaveformArea::OnTogglePersistence()
{
	m_persistence = !m_persistence;
	queue_draw();
}

void WaveformArea::OnProtocolDecode(string name)
{
	LogDebug("Protocol decode: %s\n", name.c_str());
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

	//GTK creates a FBO for us, but doesn't tell us what it is!
	//We need to glGet the FBO ID the first time we're resized.
	if(!m_windowFramebuffer.IsInitialized())
		m_windowFramebuffer.InitializeFromCurrentFramebuffer();

	//Initialize the color buffers
	//No antialiasing for now, we just alpha blend everything
	m_persistbuffer.Bind(GL_FRAMEBUFFER);
	m_persistTexture.Bind();
	m_persistTexture.SetData(width, height, NULL, GL_RGBA, GL_UNSIGNED_BYTE, GL_RGBA32F);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_persistTexture, 0);
	if(!m_persistbuffer.IsComplete())
		LogError("Persist FBO is incomplete: %x\n", glCheckFramebufferStatus(GL_FRAMEBUFFER));
	glClearColor(0,0,0,0);
	glClear(GL_COLOR_BUFFER_BIT);

	m_framebuffer.Bind(GL_FRAMEBUFFER);
	m_fboTexture.Bind();
	m_fboTexture.SetData(width, height, NULL, GL_RGBA, GL_UNSIGNED_BYTE, GL_RGBA32F);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fboTexture, 0);
	if(!m_framebuffer.IsComplete())
		LogError("FBO is incomplete: %x\n", glCheckFramebufferStatus(GL_FRAMEBUFFER));

	int err = glGetError();
	if(err != 0)
		LogNotice("resize, err = %x\n", err);

	double dt = GetTime() - start;
	LogDebug("Resize time: %.3f ms\n", dt*1000);
}

bool WaveformArea::PrepareGeometry()
{
	//LogDebug("Processing capture\n");
	LogIndenter li;

	auto dat = m_channel->GetData();
	if(!dat)
		return false;

	AnalogCapture& data = *dynamic_cast<AnalogCapture*>(dat);
	size_t count = data.size();

	//Create the geometry
	const int POINTS_PER_TRI = 2;
	const int TRIS_PER_SAMPLE = 2;
	size_t waveform_size = count * POINTS_PER_TRI * TRIS_PER_SAMPLE;
	double lheight = 0.025f;
	float* verts = new float[waveform_size];
	size_t voff = 0;
	for(size_t j=0; j<count; j++)
	{
		float y = data[j];
		float x = data.GetSampleStart(j);

		//Rather than using a generalized line drawing algorithm, we can cheat since we know the points are
		//always left to right, sorted, and never vertical. Just add some height to the samples!
		verts[voff + 0] = x;
		verts[voff + 1] = y + lheight;

		verts[voff + 2] = x;
		verts[voff + 3] = y - lheight;

		voff += 4;
	}

	//Download waveform data
	m_traceVBOs[0]->Bind();
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * waveform_size, verts, GL_DYNAMIC_DRAW);

	//Configure vertex array settings
	m_traceVAOs[0]->Bind();
	m_defaultProgram.EnableVertexArray("vert");
	m_defaultProgram.SetVertexAttribPointer("vert", 2, 0);

	m_waveformLength = count;

	//Cleanup time
	delete[] verts;

	return true;
}

bool WaveformArea::on_render(const Glib::RefPtr<Gdk::GLContext>& /*context*/)
{
	static double last = -1;

	//Draw to the offscreen floating-point framebuffer.
	m_framebuffer.Bind(GL_FRAMEBUFFER);

	double start = GetTime();
	double dt = start - last;
	if(last > 0)
	{
		//LogDebug("Frame time: %.3f ms (%.2f FPS)\n", dt*1000, 1/dt);
		m_frameTime += dt;
		m_frameCount ++;
	}
	last = start;

	//Everything we draw is 2D painter's algorithm.
	//Turn off some stuff we don't need.
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_MULTISAMPLE);
	glDisable(GL_FRAMEBUFFER_SRGB);
	glDisable(GL_CULL_FACE);

	//Start with a blank window
	glClear(GL_COLOR_BUFFER_BIT);

	//Set up blending
	glEnable(GL_BLEND);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_MAX);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//Create the actual waveform we're drawing, then render it.
	if(PrepareGeometry())
		RenderTrace();

	//Do postprocessing even if we have no waveform data (so we get a blank background)
	RenderTraceColorCorrection();
	RenderPersistence();

	//TODO: HUD overlays etc

	//Sanity check
	int err = glGetError();
	if(err != 0)
		LogNotice("err = %x\n", err);

	return true;
}

void WaveformArea::RenderTrace()
{
	//Configure our shader and projection matrix
	m_defaultProgram.Bind();
	m_defaultProgram.SetUniform(m_projection, "projection");
	m_defaultProgram.SetUniform(0.0f, "xoff");
	m_defaultProgram.SetUniform(0.5f, "xscale");
	m_defaultProgram.SetUniform(100.0f, "yoff");
	m_defaultProgram.SetUniform(100.0f, "yscale");

	//Set the color decay value (constant for now)
	m_defaultProgram.SetUniform(1.0f, "alpha");

	//Actually draw the waveform
	m_traceVAOs[0]->Bind();

	vector<int> firsts;
	vector<int> counts;
	firsts.push_back(0);
	counts.push_back(2*m_waveformLength);
	glMultiDrawArrays(GL_TRIANGLE_STRIP, &firsts[0], &counts[0], 1);
}

void WaveformArea::RenderTraceColorCorrection()
{
	//Once the rendering proper is complete, draw the offscreen buffer to the onscreen buffer
	//as a textured quad. Apply color correction as we do this.
	m_windowFramebuffer.Bind(GL_FRAMEBUFFER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	m_colormapProgram.Bind();
	m_colormapVAO.Bind();

	int loc = m_colormapProgram.GetUniformLocation("fbtex");
	glActiveTexture(GL_TEXTURE0);
	m_fboTexture.Bind();

	m_colormapProgram.SetUniform(m_color.get_red_p(), "r");
	m_colormapProgram.SetUniform(m_color.get_green_p(), "g");
	m_colormapProgram.SetUniform(m_color.get_blue_p(), "b");

	glUniform1i(loc, 0);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void WaveformArea::RenderPersistence()
{
	//Draw the persistence buffer over our waveforms
	if(m_persistence)
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		m_persistProgram.Bind();
		m_persistVAO.Bind();
		glActiveTexture(GL_TEXTURE0);
		m_persistTexture.Bind();
		int loc = m_persistProgram.GetUniformLocation("fbtex");
		glUniform1i(loc, 0);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}

	//Copy the whole on-screen buffer to the persistence buffer so we can do decay.
	//Do this even if we're not actually drawing persistence (to clear out old state)
	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_windowFramebuffer);
	m_persistbuffer.Bind(GL_DRAW_FRAMEBUFFER);
	glBlitFramebuffer(
		0, 0, m_width, m_height,
		0, 0, m_width, m_height,
		GL_COLOR_BUFFER_BIT,
		GL_NEAREST);
}

void WaveformArea::OnWaveformDataReady()
{
	//Get ready to immediately refresh
	queue_draw();
}
