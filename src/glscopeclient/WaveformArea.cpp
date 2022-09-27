/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg                                                                          *
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
#include <stdlib.h>
#include "../../lib/scopeprotocols/scopeprotocols.h"

using namespace std;

bool WaveformArea::m_isGlewInitialized = false;

WaveformArea::WaveformArea(
	StreamDescriptor channel,
	OscilloscopeWindow* parent
	)
	: m_persistence(false)
	, m_channel(channel)
	, m_parent(parent)
	, m_pixelsPerYAxisUnit(1)
{
	SharedCtorInit();
}

/**
	@brief Semi-copy constructor, used when copying a waveform to a new group

	Note that we only clone UI settings; the GL context, GTK properties, etc are new!
 */
WaveformArea::WaveformArea(const WaveformArea* clone)
	: m_persistence(clone->m_persistence)
	, m_channel(clone->m_channel)
	, m_parent(clone->m_parent)
	, m_pixelsPerYAxisUnit(clone->m_pixelsPerYAxisUnit)
	, m_axisLabelFont(clone->m_axisLabelFont)
	, m_infoBoxFont(clone->m_infoBoxFont)
	, m_cursorLabelFont(clone->m_cursorLabelFont)
	, m_decodeFont(clone->m_decodeFont)
{
	SharedCtorInit();
}

void WaveformArea::SharedCtorInit()
{
	m_updatingContextMenu 		= false;
	m_selectedChannel			= m_channel;
	m_dragState 				= DRAG_NONE;
	m_insertionBarLocation		= INSERT_NONE;
	m_dropTarget				= NULL;
	m_padding 					= 2;
	m_overlaySpacing			= 25 * GetDPIScale();
	m_persistenceClear 			= true;
	m_firstFrame 				= false;
	m_waveformRenderData		= NULL;
	m_dragOverlayPosition		= 0;
	m_geometryDirty				= false;
	m_positionDirty				= false;
	m_mouseElementPosition		= LOC_PLOT;
	m_showPendingDecodeAsStats	= false;
	m_selectedMarker			= nullptr;

	m_plotRight = 1;
	m_width		= 1;
	m_height 	= 1;

	m_decodeDialog 			= NULL;
	m_pendingDecode			= NULL;

	//Configure the OpenGL context we want
	set_has_alpha();
	set_has_depth_buffer(false);
	set_has_stencil_buffer(false);
	set_required_version(4, 2);
	set_use_es(false);

	add_events(
		Gdk::EXPOSURE_MASK |
		Gdk::POINTER_MOTION_MASK |
		Gdk::SCROLL_MASK |
		Gdk::BUTTON_PRESS_MASK |
		Gdk::BUTTON_RELEASE_MASK);

	CreateWidgets();

	m_group = NULL;

	m_channel.m_channel->AddRef();
}

WaveformArea::~WaveformArea()
{
	m_channel.m_channel->Release();

	//Need to reload the menu in case we deleted the last reference to this channel
	m_parent->RefreshChannelsMenu();

	for(auto d : m_overlays)
		OnRemoveOverlay(d);
	m_overlays.clear();

	if(m_decodeDialog)
		delete m_decodeDialog;
	if(m_pendingDecode)
		delete m_pendingDecode;

	for(auto m : m_moveExistingGroupItems)
	{
		m_moveMenu.remove(*m);
		delete m;
	}
	m_moveExistingGroupItems.clear();
}

void WaveformArea::OnRemoveOverlay(StreamDescriptor filter)
{
	//Remove the render data for it
	auto it = m_overlayRenderData.find(filter);
	if(it != m_overlayRenderData.end())
		m_overlayRenderData.erase(it);

	filter.m_channel->Release();

	m_parent->GarbageCollectAnalyzers();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialization

void WaveformArea::CreateWidgets()
{
	//Set up fonts
	SyncFontPreferences();

	//Delete
	auto item = Gtk::manage(new Gtk::MenuItem("Delete", false));
		item->signal_activate().connect(
			sigc::mem_fun(*this, &WaveformArea::OnHide));
		m_contextMenu.append(*item);

	//Move/copy
	m_contextMenu.append(m_moveItem);
		m_moveItem.set_label("Move waveform to");
		m_moveItem.set_submenu(m_moveMenu);
			m_moveMenu.append(m_moveNewGroupBelowItem);
				m_moveNewGroupBelowItem.set_label("Insert new group at bottom");
				m_moveNewGroupBelowItem.signal_activate().connect(
					sigc::mem_fun(*this, &WaveformArea::OnMoveNewBelow));
			m_moveMenu.append(m_moveNewGroupRightItem);
				m_moveNewGroupRightItem.set_label("Insert new group at right");
				m_moveNewGroupRightItem.signal_activate().connect(
					sigc::mem_fun(*this, &WaveformArea::OnMoveNewRight));
			m_moveMenu.append(*Gtk::manage(new Gtk::SeparatorMenuItem));
	m_contextMenu.append(m_copyItem);
		m_copyItem.set_label("Copy waveform to");
		m_copyItem.set_submenu(m_copyMenu);
			m_copyMenu.append(m_copyNewGroupBelowItem);
				m_copyNewGroupBelowItem.set_label("Insert new group at bottom");
				m_copyNewGroupBelowItem.signal_activate().connect(
					sigc::mem_fun(*this, &WaveformArea::OnCopyNewBelow));
			m_copyMenu.append(m_copyNewGroupRightItem);
				m_copyNewGroupRightItem.set_label("Insert new group at right");
				m_copyNewGroupRightItem.signal_activate().connect(
					sigc::mem_fun(*this, &WaveformArea::OnCopyNewRight));

	m_contextMenu.append(*Gtk::manage(new Gtk::SeparatorMenuItem));

	//Persistence
	m_contextMenu.append(m_persistenceItem);
		m_persistenceItem.set_label("Persistence");
		m_persistenceItem.signal_activate().connect(
			sigc::mem_fun(*this, &WaveformArea::OnTogglePersistence));

	m_contextMenu.append(*Gtk::manage(new Gtk::SeparatorMenuItem));

	//Cursor
	m_contextMenu.append(m_cursorItem);
		m_cursorItem.set_label("Cursor");
		m_cursorItem.set_submenu(m_cursorMenu);
			m_cursorMenu.append(m_cursorNoneItem);
				m_cursorNoneItem.set_label("None");
				m_cursorNoneItem.set_group(m_cursorGroup);
				m_cursorNoneItem.signal_activate().connect(
					sigc::bind<WaveformGroup::CursorConfig, Gtk::RadioMenuItem*>(
						sigc::mem_fun(*this, &WaveformArea::OnCursorConfig),
						WaveformGroup::CURSOR_NONE,
						&m_cursorNoneItem));
			m_cursorMenu.append(m_cursorSingleVerticalItem);
				m_cursorSingleVerticalItem.set_label("Vertical (single)");
				m_cursorSingleVerticalItem.set_group(m_cursorGroup);
				m_cursorSingleVerticalItem.signal_activate().connect(
					sigc::bind<WaveformGroup::CursorConfig, Gtk::RadioMenuItem*>(
						sigc::mem_fun(*this, &WaveformArea::OnCursorConfig),
						WaveformGroup::CURSOR_X_SINGLE,
						&m_cursorSingleVerticalItem));
			m_cursorMenu.append(m_cursorDualVerticalItem);
				m_cursorDualVerticalItem.set_label("Vertical (dual)");
				m_cursorDualVerticalItem.set_group(m_cursorGroup);
				m_cursorDualVerticalItem.signal_activate().connect(
					sigc::bind<WaveformGroup::CursorConfig, Gtk::RadioMenuItem*>(
						sigc::mem_fun(*this, &WaveformArea::OnCursorConfig),
						WaveformGroup::CURSOR_X_DUAL,
						&m_cursorDualVerticalItem));
			m_cursorMenu.append(m_cursorSingleHorizontalItem);
				m_cursorSingleHorizontalItem.set_label("Horizontal (single)");
				m_cursorSingleHorizontalItem.set_group(m_cursorGroup);
				m_cursorSingleHorizontalItem.signal_activate().connect(
					sigc::bind<WaveformGroup::CursorConfig, Gtk::RadioMenuItem*>(
						sigc::mem_fun(*this, &WaveformArea::OnCursorConfig),
						WaveformGroup::CURSOR_Y_SINGLE,
						&m_cursorSingleHorizontalItem));
			m_cursorMenu.append(m_cursorDualHorizontalItem);
				m_cursorDualHorizontalItem.set_label("Horizontal (dual)");
				m_cursorDualHorizontalItem.set_group(m_cursorGroup);
				m_cursorDualHorizontalItem.signal_activate().connect(
					sigc::bind<WaveformGroup::CursorConfig, Gtk::RadioMenuItem*>(
						sigc::mem_fun(*this, &WaveformArea::OnCursorConfig),
						WaveformGroup::CURSOR_Y_DUAL,
						&m_cursorDualHorizontalItem));

	//Markers
	m_contextMenu.append(m_markerItem);
		m_markerItem.set_label("Marker");
		m_markerItem.set_submenu(m_markerMenu);
			m_markerMenu.append(m_markerAddItem);
				m_markerAddItem.set_label("Add");
				m_markerAddItem.signal_activate().connect(sigc::mem_fun(*this, &WaveformArea::OnMarkerAdd));

	m_contextMenu.append(*Gtk::manage(new Gtk::SeparatorMenuItem));

	//Coupling
	m_contextMenu.append(m_couplingItem);
		m_couplingItem.set_label("Coupling");
		m_couplingItem.set_submenu(m_couplingMenu);
			m_ac1MCouplingItem.set_label("AC 1M");
				m_ac1MCouplingItem.set_group(m_couplingGroup);
				m_ac1MCouplingItem.signal_activate().connect(
					sigc::bind<OscilloscopeChannel::CouplingType, Gtk::RadioMenuItem*>(
						sigc::mem_fun(*this, &WaveformArea::OnCoupling),
						OscilloscopeChannel::COUPLE_AC_1M, &m_ac1MCouplingItem));
				m_couplingMenu.append(m_ac1MCouplingItem);
			m_dc1MCouplingItem.set_label("DC 1M");
				m_dc1MCouplingItem.set_group(m_couplingGroup);
				m_dc1MCouplingItem.signal_activate().connect(
					sigc::bind<OscilloscopeChannel::CouplingType, Gtk::RadioMenuItem*>(
						sigc::mem_fun(*this, &WaveformArea::OnCoupling),
						OscilloscopeChannel::COUPLE_DC_1M, &m_dc1MCouplingItem));
				m_couplingMenu.append(m_dc1MCouplingItem);
			m_dc50CouplingItem.set_label("DC 50Ω");
				m_dc50CouplingItem.set_group(m_couplingGroup);
				m_dc50CouplingItem.signal_activate().connect(
					sigc::bind<OscilloscopeChannel::CouplingType, Gtk::RadioMenuItem*>(
						sigc::mem_fun(*this, &WaveformArea::OnCoupling),
						OscilloscopeChannel::COUPLE_DC_50, &m_dc50CouplingItem));
				m_couplingMenu.append(m_dc50CouplingItem);
			m_ac50CouplingItem.set_label("AC 50Ω");
				m_ac50CouplingItem.set_group(m_couplingGroup);
				m_ac50CouplingItem.signal_activate().connect(
					sigc::bind<OscilloscopeChannel::CouplingType, Gtk::RadioMenuItem*>(
						sigc::mem_fun(*this, &WaveformArea::OnCoupling),
						OscilloscopeChannel::COUPLE_AC_50, &m_ac50CouplingItem));
				m_couplingMenu.append(m_ac50CouplingItem);
			m_gndCouplingItem.set_label("GND");
				m_gndCouplingItem.set_group(m_couplingGroup);
				m_gndCouplingItem.signal_activate().connect(
					sigc::bind<OscilloscopeChannel::CouplingType, Gtk::RadioMenuItem*>(
						sigc::mem_fun(*this, &WaveformArea::OnCoupling),
						OscilloscopeChannel::COUPLE_GND, &m_gndCouplingItem));
				m_couplingMenu.append(m_gndCouplingItem);

	m_contextMenu.append(*Gtk::manage(new Gtk::SeparatorMenuItem));

	//Decode
	m_contextMenu.append(m_decodeAlphabeticalItem);
		m_decodeAlphabeticalItem.set_label("Alphabetical");
		m_decodeAlphabeticalItem.set_submenu(m_decodeAlphabeticalMenu);
	m_contextMenu.append(m_decodeBusItem);
		m_decodeBusItem.set_label("Buses");
		m_decodeBusItem.set_submenu(m_decodeBusMenu);
	m_contextMenu.append(m_decodeClockItem);
		m_decodeClockItem.set_label("Clocking");
		m_decodeClockItem.set_submenu(m_decodeClockMenu);
	m_contextMenu.append(m_decodeMathItem);
		m_decodeMathItem.set_label("Math");
		m_decodeMathItem.set_submenu(m_decodeMathMenu);
	m_contextMenu.append(m_decodeMeasurementItem);
		m_decodeMeasurementItem.set_label("Measurement");
		m_decodeMeasurementItem.set_submenu(m_decodeMeasurementMenu);
	m_contextMenu.append(m_decodeMemoryItem);
		m_decodeMemoryItem.set_label("Memory");
		m_decodeMemoryItem.set_submenu(m_decodeMemoryMenu);
	m_contextMenu.append(m_decodeMiscItem);
		m_decodeMiscItem.set_label("Misc");
		m_decodeMiscItem.set_submenu(m_decodeMiscMenu);
	m_contextMenu.append(m_decodePowerItem);
		m_decodePowerItem.set_label("Power");
		m_decodePowerItem.set_submenu(m_decodePowerMenu);
	m_contextMenu.append(m_decodeRFItem);
		m_decodeRFItem.set_label("RF");
		m_decodeRFItem.set_submenu(m_decodeRFMenu);
	m_contextMenu.append(m_decodeSerialItem);
		m_decodeSerialItem.set_label("Serial");
		m_decodeSerialItem.set_submenu(m_decodeSerialMenu);
	m_contextMenu.append(m_decodeSignalIntegrityItem);
		m_decodeSignalIntegrityItem.set_label("Signal Integrity");
		m_decodeSignalIntegrityItem.set_submenu(m_decodeSignalIntegrityMenu);
	m_contextMenu.append(m_decodeWaveformGenerationItem);
		m_decodeWaveformGenerationItem.set_label("Waveform Generation");
		m_decodeWaveformGenerationItem.set_submenu(m_decodeWaveformGenerationMenu);


		vector<string> names;
		Filter::EnumProtocols(names);
		for(auto p : names)
		{
			item = Gtk::manage(new Gtk::MenuItem(p, false));
			item->signal_activate().connect(
				sigc::bind<string, bool>(sigc::mem_fun(*this, &WaveformArea::OnProtocolDecode), p, false));

			//Create a test decode and see where it goes
			auto d = Filter::CreateFilter(p, "");
			switch(d->GetCategory())
			{
				case Filter::CAT_ANALYSIS:
					m_decodeSignalIntegrityMenu.append(*item);
					break;

				case Filter::CAT_BUS:
					m_decodeBusMenu.append(*item);
					break;

				case Filter::CAT_CLOCK:
					m_decodeClockMenu.append(*item);
					break;

				case Filter::CAT_POWER:
					m_decodePowerMenu.append(*item);
					break;

				case Filter::CAT_RF:
					m_decodeRFMenu.append(*item);
					break;

				//Measurements need some special processing
				case Filter::CAT_MEASUREMENT:

					//Scalar measurement? Just add this item
					if(d->IsScalarOutput())
						m_decodeMeasurementMenu.append(*item);

					//Vector measurements have two possible displays (graph and statistics)
					else
					{
						auto childmenu = Gtk::manage(new Gtk::Menu);
						childmenu->append(*item);
						item->set_label("Graph");

						//Summarize
						item = Gtk::manage(new Gtk::MenuItem("Stats", false));
						item->signal_activate().connect(
							sigc::bind<string, bool>(sigc::mem_fun(*this, &WaveformArea::OnProtocolDecode), p, true));
						childmenu->append(*item);

						auto m = Gtk::manage(new Gtk::MenuItem(p, false));
						m->set_submenu(*childmenu);
						m_decodeMeasurementMenu.append(*m);
					}

					break;

				case Filter::CAT_MATH:
					m_decodeMathMenu.append(*item);
					break;

				case Filter::CAT_MEMORY:
					m_decodeMemoryMenu.append(*item);
					break;

				case Filter::CAT_SERIAL:
					m_decodeSerialMenu.append(*item);
					break;

				case Filter::CAT_GENERATION:
					m_decodeWaveformGenerationMenu.append(*item);
					break;

				default:
				case Filter::CAT_MISC:
					m_decodeMiscMenu.append(*item);
					break;
			}
			delete d;

			//Make a second menu item and put on the alphabetical list
			item = Gtk::manage(new Gtk::MenuItem(p, false));
			item->signal_activate().connect(
				sigc::bind<string>(sigc::mem_fun(*this, &WaveformArea::OnProtocolDecode), p, false));
			m_decodeAlphabeticalMenu.append(*item);
		}

	m_contextMenu.append(*Gtk::manage(new Gtk::SeparatorMenuItem));

	//Statistics
	m_contextMenu.append(m_statisticsItem);
		m_statisticsItem.set_label("Statistics");
		m_statisticsItem.signal_activate().connect(
			sigc::mem_fun(*this, &WaveformArea::OnStatistics));


	m_contextMenu.show_all();
}

void WaveformArea::on_realize()
{
	//Let the base class create the GL context, then select it
	Gtk::GLArea::on_realize();
	make_current();

	//Apply DPI scaling to Pango fonts since we are not using Cairo scaling
	auto c = get_pango_context();
	c->set_resolution(c->get_resolution() * get_window()->get_scale_factor());

	//Set up GLEW
	if(!m_isGlewInitialized)
	{
		//Check if GL was initialized OK
		if(has_error())
		{
			//doesn't seem to be any way to get this error without throwing it??
			try
			{
				throw_if_error();
			}
			catch(Glib::Error& gerr)
			{
				string err =
					"glscopeclient was unable to initialize OpenGL and cannot continue.\n"
					"This probably indicates a problem with your graphics card drivers.\n"
					"\n"
					"GL error: ";
				err += gerr.what();

				Gtk::MessageDialog dlg(
					err,
					false,
					Gtk::MESSAGE_ERROR,
					Gtk::BUTTONS_OK,
					true
					);

				dlg.run();
				exit(1);
			}
		}

		//Print some debug info
		auto context = get_context();
		if(!context)
			LogFatal("context is null but we don't have an error set in GTK\n");
		int major, minor;
		context->get_version(major, minor);
		string profile = "compatibility";
		if(context->is_legacy())
			profile = "legacy";
		else if(context->get_forward_compatible())
			profile = "core";
		string type = "";
		if(context->get_use_es())
			type = " ES";
		LogDebug("Context: OpenGL%s %d.%d %s profile\n",
			type.c_str(),
			major, minor,
			profile.c_str());
		{
			LogIndenter li;
			LogDebug("GL_VENDOR                   = %s\n", glGetString(GL_VENDOR));
			LogDebug("GL_RENDERER                 = %s\n", glGetString(GL_RENDERER));
			LogDebug("GL_VERSION                  = %s\n", glGetString(GL_VERSION));
			LogDebug("GL_SHADING_LANGUAGE_VERSION = %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
			LogDebug("Initial GL error code       = %d\n", glGetError());
		}

		//Initialize GLEW
		GLenum glewResult = glewInit();

		//The glewInit function doesn't allow runtime detection between GLX and
		//EGL that is used by Wayland. It will default to GLX and return the
		//_NO_GLX_DISPLAY error when running under Wayland. We can ignore this
		//error since we don't need the GLX/EGL entry points and the GLX query for
		//ARB works on EGL as well.
		//See https://github.com/nigels-com/glew/issues/172
		//
		//TODO: Call glewContextInit() instead of glewInit, and remove the
		//Wayland check once we rely on GLEW 2.2.0 (the first release that
		//exposes glewContextInit).
		if(glewResult != GLEW_OK
			&& !( getenv("WAYLAND_DISPLAY") && glewResult == GLEW_ERROR_NO_GLX_DISPLAY) )
		{
			string err =
				"glscopeclient was unable to initialize GLEW and cannot continue.\n"
				"This probably indicates a problem with your graphics card drivers.\n"
				"\n"
				"GLEW error: ";
			err += (const char*)glewGetErrorString(glewResult);

			Gtk::MessageDialog dlg(
				err,
				false,
				Gtk::MESSAGE_ERROR,
				Gtk::BUTTONS_OK,
				true
				);

			dlg.run();
			exit(1);
		}

		if(GLEW_ARB_gpu_shader_int64)
		{
			LogDebug("    GL_ARB_gpu_shader_int64     = supported\n");
			if(g_noglint64)
				LogDebug("    but not being used because --noglint64 argument was passed\n");
		}
		else
			LogDebug("    GL_ARB_gpu_shader_int64     = not supported\n");

		//Check for GL 4.2 (required for glBindImageTexture)
		if(!GLEW_VERSION_4_2)
		{
			string err =
				"Your graphics card or driver does not appear to support OpenGL 4.2.\n"
				"\n"
				"Unfortunately, glscopeclient cannot run on your system.\n";

			Gtk::MessageDialog dlg(
				err,
				false,
				Gtk::MESSAGE_ERROR,
				Gtk::BUTTONS_OK,
				true
				);

			dlg.run();
			exit(1);
		}

		//Make sure we have the required extensions
		if(	!GLEW_EXT_blend_equation_separate ||
			!GLEW_EXT_framebuffer_object ||
			!GLEW_ARB_vertex_array_object ||
			!GLEW_ARB_shader_storage_buffer_object ||
			!GLEW_ARB_arrays_of_arrays ||
			!GLEW_ARB_compute_shader)
		{
			string err =
				"Your graphics card or driver does not appear to support one or more of the following required extensions:\n"
				"* GL_ARB_arrays_of_arrays\n"
				"* GL_ARB_compute_shader\n"
				"* GL_ARB_shader_storage_buffer_object\n"
				"* GL_ARB_vertex_array_object\n"
				"* GL_EXT_blend_equation_separate\n"
				"* GL_EXT_framebuffer_object\n"
				"\n"
				"Unfortunately, glscopeclient cannot run on your system.\n";

			Gtk::MessageDialog dlg(
				err,
				false,
				Gtk::MESSAGE_ERROR,
				Gtk::BUTTONS_OK,
				true
				);

			dlg.run();
			exit(1);
		}

		m_isGlewInitialized = true;
	}

	//We're about to draw the first frame after realization.
	//This means we need to save some configuration (like the current FBO) that GTK doesn't tell us directly
	m_firstFrame = true;

	//Create waveform render data for our main trace
	m_waveformRenderData = new WaveformRenderData(m_channel, this);

	//Set stuff up for each rendering pass
	InitializeWaveformPass();
	InitializeColormapPass();
	InitializeCairoPass();
	InitializeEyePass();
	InitializeSpectrogramPass();
}

void WaveformArea::on_unrealize()
{
	make_current();

	CleanupGLHandles();

	Gtk::GLArea::on_unrealize();
}

void WaveformArea::CleanupGLHandles()
{
	//Clean up old shaders
	m_histogramWaveformComputeProgram.Destroy();
	m_digitalWaveformComputeProgram.Destroy();
	m_analogWaveformComputeProgram.Destroy();
	m_zeroHoldAnalogWaveformComputeProgram.Destroy();
	m_denseAnalogWaveformComputeProgram.Destroy();
	m_colormapProgram.Destroy();
	m_eyeProgram.Destroy();
	m_spectrogramProgram.Destroy();
	m_cairoProgram.Destroy();

	//Clean up old VAOs
	m_colormapVAO.Destroy();
	m_cairoVAO.Destroy();
	m_eyeVAO.Destroy();
	m_spectrogramVAO.Destroy();

	//Clean up old VBOs
	m_colormapVBO.Destroy();
	m_cairoVBO.Destroy();
	m_eyeVBO.Destroy();
	m_spectrogramVBO.Destroy();

	//Clean up old textures
	m_cairoTexture.Destroy();
	m_cairoTextureOver.Destroy();
	for(auto& it : m_eyeColorRamp)
		it.second.Destroy();
	m_eyeColorRamp.clear();

	delete m_waveformRenderData;
	m_waveformRenderData = NULL;
	for(auto it : m_overlayRenderData)
		delete it.second;
	m_overlayRenderData.clear();

	//Detach the FBO so we don't destroy it!!
	//GTK manages this, and it might be used by more than one waveform area within the application.
	m_windowFramebuffer.Detach();
}

void WaveformArea::InitializeWaveformPass()
{
	//Load all of the compute shaders
	ComputeShader hwc;
	ComputeShader dwc;
	ComputeShader awc;
	ComputeShader zawc;
	ComputeShader adwc;
	if(GLEW_ARB_gpu_shader_int64 && !g_noglint64)
	{
		if(!hwc.Load(
			"#version 420",
			"#define DENSE_PACK",
			"shaders/waveform-compute-head.glsl",
			"shaders/waveform-compute-histogram.glsl",
			"shaders/waveform-compute-core.glsl",
			NULL))
			LogFatal("failed to load histogram waveform compute shader, aborting\n");
		if(!dwc.Load(
			"#version 420",
			"shaders/waveform-compute-head.glsl",
			"shaders/waveform-compute-digital.glsl",
			"shaders/waveform-compute-core.glsl",
			NULL))
			LogFatal("failed to load digital waveform compute shader, aborting\n");
		if(!awc.Load(
			"#version 420",
			"shaders/waveform-compute-head.glsl",
			"shaders/waveform-compute-analog.glsl",
			"shaders/waveform-compute-core.glsl",
			NULL))
			LogFatal("failed to load analog waveform compute shader, aborting\n");
		if(!zawc.Load(
			"#version 420",
			"#define NO_INTERPOLATION",
			"shaders/waveform-compute-head.glsl",
			"shaders/waveform-compute-analog.glsl",
			"shaders/waveform-compute-core.glsl",
			NULL))
			LogFatal("failed to load zero-hold analog waveform compute shader, aborting\n");
		if(!adwc.Load(
			"#version 420",
			"#define DENSE_PACK",
			"shaders/waveform-compute-head.glsl",
			"shaders/waveform-compute-analog.glsl",
			"shaders/waveform-compute-core.glsl",
			NULL))
			LogFatal("failed to load dense analog waveform compute shader, aborting\n");
	}
	else
	{
		if(!hwc.Load(
			"#version 420",
			"#define DENSE_PACK",
			"shaders/waveform-compute-head-noint64.glsl",
			"shaders/waveform-compute-histogram.glsl",
			"shaders/waveform-compute-core.glsl",
			NULL))
			LogFatal("failed to load histogram waveform compute shader, aborting\n");
		if(!dwc.Load(
			"#version 420",
			"shaders/waveform-compute-head-noint64.glsl",
			"shaders/waveform-compute-digital.glsl",
			"shaders/waveform-compute-core.glsl",
			NULL))
			LogFatal("failed to load digital waveform compute shader, aborting\n");
		if(!awc.Load(
			"#version 420",
			"shaders/waveform-compute-head-noint64.glsl",
			"shaders/waveform-compute-analog.glsl",
			"shaders/waveform-compute-core.glsl",
			NULL))
			LogFatal("failed to load analog waveform compute shader, aborting\n");
		if(!zawc.Load(
			"#version 420",
			"#define NO_INTERPOLATION",
			"shaders/waveform-compute-head-noint64.glsl",
			"shaders/waveform-compute-analog.glsl",
			"shaders/waveform-compute-core.glsl",
			NULL))
			LogFatal("failed to load zero-hold analog waveform compute shader, aborting\n");
		if(!adwc.Load(
			"#version 420",
			"#define DENSE_PACK",
			"shaders/waveform-compute-head-noint64.glsl",
			"shaders/waveform-compute-analog.glsl",
			"shaders/waveform-compute-core.glsl",
			NULL))
			LogFatal("failed to load dense analog waveform compute shader, aborting\n");
	}

	//Link them
	m_histogramWaveformComputeProgram.Add(hwc);
	if(!m_histogramWaveformComputeProgram.Link())
		LogFatal("failed to link histogram waveform shader program, aborting\n");

	m_digitalWaveformComputeProgram.Add(dwc);
	if(!m_digitalWaveformComputeProgram.Link())
		LogFatal("failed to link digital waveform shader program, aborting\n");

	m_analogWaveformComputeProgram.Add(awc);
	if(!m_analogWaveformComputeProgram.Link())
		LogFatal("failed to link analog waveform shader program, aborting\n");

	m_zeroHoldAnalogWaveformComputeProgram.Add(zawc);
	if(!m_zeroHoldAnalogWaveformComputeProgram.Link())
		LogFatal("failed to link zero-hold analog waveform shader program, aborting\n");

	m_denseAnalogWaveformComputeProgram.Add(adwc);
	if(!m_denseAnalogWaveformComputeProgram.Link())
		LogFatal("failed to link dense analog waveform shader program, aborting\n");
}

void WaveformArea::InitializeColormapPass()
{
	//Set up shaders
	VertexShader cvs;
	FragmentShader cfs;
	if(!cvs.Load("shaders/colormap-vertex.glsl", NULL) || !cfs.Load("shaders/colormap-fragment.glsl", NULL))
		LogFatal("failed to load colormap shaders, aborting\n");

	m_colormapProgram.Add(cvs);
	m_colormapProgram.Add(cfs);
	if(!m_colormapProgram.Link())
		LogFatal("failed to link shader program, aborting\n");

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

void WaveformArea::InitializeEyePass()
{
	//Set up shaders
	VertexShader cvs;
	FragmentShader cfs;
	if(!cvs.Load("shaders/eye-vertex.glsl", NULL) || !cfs.Load("shaders/eye-fragment.glsl", NULL))
		LogFatal("failed to load eye shaders, aborting\n");

	m_eyeProgram.Add(cvs);
	m_eyeProgram.Add(cfs);
	if(!m_eyeProgram.Link())
		LogFatal("failed to link shader program, aborting\n");

	//Create the VAO/VBO for a fullscreen polygon
	float verts[8] =
	{
		-1, -1,
		 1, -1,
		 1,  1,
		-1,  1
	};
	m_eyeVBO.Bind();
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

	m_eyeVAO.Bind();
	m_eyeProgram.EnableVertexArray("vert");
	m_eyeProgram.SetVertexAttribPointer("vert", 2, 0);

	//Load the eye color ramp
	auto names = m_parent->GetEyeColorNames();
	char tmp[1024];
	for(auto name : names)
	{
		auto path = m_parent->GetEyeColorPath(name);
		FILE* fp = fopen(path.c_str(), "r");
		if(!fp)
			LogFatal("fail to open eye gradient \"%s\" for name \"%s\"\n", path.c_str(), name.c_str());
		fread(tmp, 1, 1024, fp);
		fclose(fp);

		m_eyeColorRamp[name].Bind();
		ResetTextureFiltering();
		m_eyeColorRamp[name].SetData(256, 1, tmp, GL_RGBA);
	}
}

void WaveformArea::InitializeSpectrogramPass()
{
	//Set up shaders
	VertexShader cvs;
	FragmentShader cfs;
	if(!cvs.Load("shaders/spectrogram-vertex.glsl", NULL) || !cfs.Load("shaders/spectrogram-fragment.glsl", NULL))
		LogFatal("failed to load spectrogram shaders, aborting\n");

	m_spectrogramProgram.Add(cvs);
	m_spectrogramProgram.Add(cfs);
	if(!m_spectrogramProgram.Link())
		LogFatal("failed to link shader program, aborting\n");

	//Create the VAO/VBO for a fullscreen polygon
	float verts[8] =
	{
		-1, -1,
		 1, -1,
		 1,  1,
		-1,  1
	};
	m_spectrogramVBO.Bind();
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

	m_spectrogramVAO.Bind();
	m_spectrogramProgram.EnableVertexArray("vert");
	m_spectrogramProgram.SetVertexAttribPointer("vert", 2, 0);
}

void WaveformArea::InitializeCairoPass()
{
	//Set up shaders
	VertexShader cvs;
	FragmentShader cfs;
	if(!cvs.Load("shaders/cairo-vertex.glsl", NULL) || !cfs.Load("shaders/cairo-fragment.glsl", NULL))
		LogFatal("failed to load cairo shaders, aborting\n");

	m_cairoProgram.Add(cvs);
	m_cairoProgram.Add(cfs);
	if(!m_cairoProgram.Link())
		LogFatal("failed to link shader program, aborting\n");

	//Create the VAO/VBO for a fullscreen polygon
	float verts[8] =
	{
		-1, -1,
		 1, -1,
		 1,  1,
		-1,  1
	};
	m_cairoVBO.Bind();
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

	m_cairoVAO.Bind();
	m_cairoProgram.EnableVertexArray("vert");
	m_cairoProgram.SetVertexAttribPointer("vert", 2, 0);
}

bool WaveformArea::IsWaterfall()
{
	return (m_channel.GetType() == Stream::STREAM_TYPE_WATERFALL);
}

bool WaveformArea::IsDigital()
{
	return (m_channel.GetType() == Stream::STREAM_TYPE_DIGITAL);
}

bool WaveformArea::IsAnalog()
{
	return (m_channel.GetType() == Stream::STREAM_TYPE_ANALOG);
}

bool WaveformArea::IsEye()
{
	return (m_channel.GetType() == Stream::STREAM_TYPE_EYE);
}

bool WaveformArea::IsSpectrogram()
{
	return (m_channel.GetType() == Stream::STREAM_TYPE_SPECTROGRAM);
}

bool WaveformArea::IsEyeOrBathtub()
{
	//TODO: this should really be "is fixed two UI wide plot"
	auto bath = dynamic_cast<HorizontalBathtub*>(m_channel.m_channel);
	return IsEye() || (bath != NULL);
}

bool WaveformArea::IsTime()
{
	return (m_channel.GetYAxisUnits().GetType() == Unit::UNIT_FS);
}
