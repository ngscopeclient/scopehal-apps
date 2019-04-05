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
	@brief  Event handling code for WaveformArea
 */

#include "glscopeclient.h"
#include "WaveformArea.h"
#include "OscilloscopeWindow.h"
#include <random>
#include "ProfileBlock.h"

using namespace std;
using namespace glm;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Window events

void WaveformArea::on_resize(int width, int height)
{
	//double start = GetTime();

	m_width = width;
	m_height = height;
	m_plotRight = width;

	int err = glGetError();
	if(err != 0)
		LogNotice("resize 1, err = %x\n", err);

	//Reset camera configuration
	glViewport(0, 0, width, height);

	//transformation matrix from screen to pixel coordinates
	m_projection = translate(
		scale(mat4(1.0f), vec3(2.0f / width, 2.0f / height, 1)),	//scale to window size
		vec3(-width/2, -height/2, 0)											//put origin at bottom left
		);
	err = glGetError();
	if(err != 0)
		LogNotice("resize 2, err = %x\n", err);

	//GTK creates a FBO for us, but doesn't tell us what it is!
	//We need to glGet the FBO ID the first time we're resized.
	if(!m_windowFramebuffer.IsInitialized())
		m_windowFramebuffer.InitializeFromCurrentFramebuffer();

	err = glGetError();
	if(err != 0)
		LogNotice("resize 3, err = %x\n", err);

	//Initialize the color buffers
	//No antialiasing for now, we just alpha blend everything
	m_waveformFramebuffer.Bind(GL_FRAMEBUFFER);
	m_waveformTexture.Bind();
	m_waveformTexture.SetData(width, height, NULL, GL_RGBA, GL_UNSIGNED_BYTE, GL_RGBA32F);
	m_waveformFramebuffer.SetTexture(m_waveformTexture);
	if(!m_waveformFramebuffer.IsComplete())
		LogError("FBO is incomplete: %x\n", glCheckFramebufferStatus(GL_FRAMEBUFFER));

	err = glGetError();
	if(err != 0)
		LogNotice("resize, err = %x\n", err);

	//double dt = GetTime() - start;
	//LogDebug("Resize time: %.3f ms\n", dt*1000);
}

bool WaveformArea::on_scroll_event (GdkEventScroll* ev)
{
	m_clickLocation = HitTest(ev->x, ev->y);

	switch(m_clickLocation)
	{
		//Adjust time/div
		case LOC_PLOT:

			switch(ev->direction)
			{
				case GDK_SCROLL_UP:
					m_parent->OnZoomInHorizontal(m_group);
					ClearPersistence();
					break;
				case GDK_SCROLL_DOWN:
					m_parent->OnZoomOutHorizontal(m_group);
					ClearPersistence();
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
			break;

		//Adjust volts/div
		case LOC_VSCALE:
			{
				double vrange = m_channel->GetVoltageRange();
				switch(ev->direction)
				{
					case GDK_SCROLL_UP:
						m_channel->SetVoltageRange(vrange * 0.9);
						queue_draw();
						break;
					case GDK_SCROLL_DOWN:
						m_channel->SetVoltageRange(vrange / 0.9);
						queue_draw();
						break;

					default:
						break;
				}
			}
			break;

		default:
			break;
	}

	return true;
}

bool WaveformArea::on_button_press_event(GdkEventButton* event)
{
	//TODO: See if we right clicked on our main channel or a protocol decoder.
	//If a decoder, filter for that instead
	m_selectedChannel = m_channel;
	m_clickLocation = HitTest(event->x, event->y);

	//Look up the time of our click (if in the plot area)
	int64_t timestamp = PixelsToPicoseconds(event->x);

	switch(m_clickLocation)
	{
		//Waveform area
		case LOC_PLOT:
			{
				switch(event->button)
				{
					//Left
					case 1:

						//Start dragging the second cursor
						if(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL)
						{
							m_dragState = DRAG_CURSOR;
							m_group->m_xCursorPos[1] = timestamp;
						}

						//Place the first cursor
						if( (m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL) ||
							(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_SINGLE))
						{
							m_group->m_xCursorPos[0] = timestamp;
						}

						//Redraw if we have any cursor
						if(m_group->m_cursorConfig != WaveformGroup::CURSOR_NONE)
							m_group->m_vbox.queue_draw();

						break;

					//Middle
					case 2:
						m_parent->OnAutofitHorizontal();
						break;

					//Right
					case 3:
						UpdateContextMenu();
						m_contextMenu.popup(event->button, event->time);
						break;

					default:
						//LogDebug("Button %d pressed on waveform plot\n", event->button);
						break;
				}

			};
			break;

		//Vertical axis
		case LOC_VSCALE:
			{
				switch(event->button)
				{
					//Right
					case 3:
						break;

					default:
						//LogDebug("Button %d pressed on vertical scale\n", event->button);
						break;
				}
			}
			break;

		//Trigger indicator
		case LOC_TRIGGER:
			{
				switch(event->button)
				{
					//Left
					case 1:
						m_dragState = DRAG_TRIGGER;
						queue_draw();
						break;

					default:
						//LogDebug("Button %d pressed on trigger\n", event->button);
						break;
				}
			}
			break;


	}

	return true;
}

bool WaveformArea::on_button_release_event(GdkEventButton* event)
{
	int64_t timestamp = PixelsToPicoseconds(event->x);

	switch(m_dragState)
	{
		//Update scope trigger configuration if left mouse is released
		case DRAG_TRIGGER:
			if(event->button == 1)
			{
				m_scope->SetTriggerVoltage(YPositionToVolts(event->y));
				m_parent->ClearAllPersistence();
				queue_draw();
			}
			break;

		case DRAG_CURSOR:
			if(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL)
				m_group->m_xCursorPos[1] = timestamp;
			break;

		default:
			break;
	}

	//Stop dragging things
	if(m_dragState != DRAG_NONE)
	{
		m_dragState = DRAG_NONE;
		queue_draw();
	}

	return true;
}

bool WaveformArea::on_motion_notify_event(GdkEventMotion* event)
{
	m_cursorX = event->x;
	m_cursorY = event->y;

	int64_t timestamp = PixelsToPicoseconds(event->x);

	switch(m_dragState)
	{
		//Trigger drag - update level and refresh
		case DRAG_TRIGGER:
			m_scope->SetTriggerVoltage(YPositionToVolts(event->y));
			m_parent->ClearAllPersistence();
			queue_draw();
			break;

		case DRAG_CURSOR:
			if(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL)
			{
				m_group->m_xCursorPos[1] = timestamp;
				m_group->m_vbox.queue_draw();
			}
			break;

		//Nothing to do
		default:
			break;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Menu/toolbar commands

void WaveformArea::OnCursorConfig(WaveformGroup::CursorConfig config, Gtk::RadioMenuItem* item)
{
	if(m_updatingContextMenu || !item->get_active())
		return;

	m_group->m_cursorConfig = config;
	m_group->m_vbox.queue_draw();
}

void WaveformArea::OnMoveNewRight()
{
	m_parent->OnMoveNewRight(this);
}

void WaveformArea::OnMoveNewBelow()
{
	m_parent->OnMoveNewBelow(this);
}

void WaveformArea::OnMoveToExistingGroup(WaveformGroup* group)
{
	m_parent->OnMoveToExistingGroup(this, group);
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

void WaveformArea::OnMeasure(string name)
{
	m_group->AddColumn(name, m_selectedChannel, m_color.to_string());
}

void WaveformArea::OnBandwidthLimit(int mhz, Gtk::RadioMenuItem* item)
{
	//ignore spurious events while loading menu config, or from item being deselected
	if(m_updatingContextMenu || !item->get_active())
		return;

	m_selectedChannel->SetBandwidthLimit(mhz);
	ClearPersistence();
}

void WaveformArea::OnTriggerMode(Oscilloscope::TriggerType type, Gtk::RadioMenuItem* item)
{
	//ignore spurious events while loading menu config, or from item being deselected
	if(m_updatingContextMenu || !item->get_active())
		return;

	m_scope->SetTriggerChannelIndex(m_channel->GetIndex());
	m_scope->SetTriggerType(type);
	m_parent->ClearAllPersistence();
}

void WaveformArea::OnWaveformDataReady()
{
	//Update our measurements and redraw the waveform
	queue_draw();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers

/**
	@brief Update the location of the mouse
 */
WaveformArea::ClickLocation WaveformArea::HitTest(double x, double y)
{
	if(x > m_plotRight)
	{
		//On the trigger button?
		if(m_channel->GetIndex() == m_scope->GetTriggerChannelIndex())
		{
			float vy = VoltsToYPosition(m_scope->GetTriggerVoltage());
			float radius = 20;
			if( (fabs(y - vy) < radius) &&
				(x < (m_plotRight + radius) ) )
			{
				return LOC_TRIGGER;
			}
		}

		//Nope, just the scale bar
		return LOC_VSCALE;
	}

	return LOC_PLOT;
}

/**
	@brief Enable/disable or show/hide context menu items for the current selection
 */
void WaveformArea::UpdateContextMenu()
{
	//Let signal handlers know to ignore any events that happen as we pull state from the scope
	m_updatingContextMenu = true;

	//Update waveform views
	for(auto m : m_moveExistingGroupItems)
	{
		m_moveMenu.remove(*m);
		delete m;
	}
	m_moveExistingGroupItems.clear();
	for(auto g : m_parent->m_waveformGroups)
	{
		auto item = new Gtk::MenuItem;
		item->set_label(g->m_frame.get_label());
		m_moveMenu.append(*item);
		m_moveExistingGroupItems.emplace(item);

		//Gray out the item if it's our current parent
		if(get_parent() == &g->m_vbox)
			item->set_sensitive(false);

		item->signal_activate().connect(sigc::bind<WaveformGroup*>(
			sigc::mem_fun(*this, &WaveformArea::OnMoveToExistingGroup), g));
	}
	m_moveMenu.show_all();

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

	//Gray out measurements that don't make sense for the type of channel we've selected
	children = m_measureMenu.get_children();
	for(auto item : children)
	{
		Gtk::MenuItem* menu = dynamic_cast<Gtk::MenuItem*>(item);
		if(menu == NULL)
			continue;

		auto m = Measurement::CreateMeasurement(menu->get_label());
		menu->set_sensitive(m->ValidateChannel(0, m_selectedChannel));
		delete m;
	}

	if(m_selectedChannel->IsPhysicalChannel())
	{
		m_bwMenu.set_sensitive(true);
		m_attenMenu.set_sensitive(true);
		m_couplingMenu.set_sensitive(true);

		//Update the current coupling setting
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

			//coupling not possible, it's not an analog channel
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

		//Update the bandwidth limit
		int bwl = m_selectedChannel->GetBandwidthLimit();
		switch(bwl)
		{
			case 0:
				m_bwFullItem.set_active(true);
				break;

			case 20:
				m_bw20Item.set_active(true);
				break;

			case 200:
				m_bw200Item.set_active(true);
				break;

			default:
				//TODO: how to handle this?
				break;
		}

		if(m_scope->GetTriggerChannelIndex() != m_channel->GetIndex())
		{
			m_risingTriggerItem.set_inconsistent(true);
			m_fallingTriggerItem.set_inconsistent(true);
			m_bothTriggerItem.set_inconsistent(true);

			m_risingTriggerItem.set_draw_as_radio(false);
			m_fallingTriggerItem.set_draw_as_radio(false);
			m_bothTriggerItem.set_draw_as_radio(false);
		}
		else
		{
			m_risingTriggerItem.set_inconsistent(false);
			m_fallingTriggerItem.set_inconsistent(false);
			m_bothTriggerItem.set_inconsistent(false);

			m_risingTriggerItem.set_draw_as_radio(true);
			m_fallingTriggerItem.set_draw_as_radio(true);
			m_bothTriggerItem.set_draw_as_radio(true);

			switch(m_scope->GetTriggerType())
			{
				case Oscilloscope::TRIGGER_TYPE_RISING:
					m_risingTriggerItem.set_active();
					break;

				case Oscilloscope::TRIGGER_TYPE_FALLING:
					m_fallingTriggerItem.set_active();
					break;

				case Oscilloscope::TRIGGER_TYPE_CHANGE:
					m_bothTriggerItem.set_active();
					break;

				//unsupported trigger
				default:
					break;
			}
		}
	}
	else
	{
		m_bwMenu.set_sensitive(false);
		m_attenMenu.set_sensitive(false);
		m_couplingMenu.set_sensitive(false);
	}

	//Select cursor config
	switch(m_group->m_cursorConfig)
	{
		case WaveformGroup::CURSOR_NONE:
			m_cursorNoneItem.set_active(true);
			break;

		case WaveformGroup::CURSOR_X_SINGLE:
			m_cursorSingleVerticalItem.set_active(true);
			break;

		case WaveformGroup::CURSOR_X_DUAL:
			m_cursorDualVerticalItem.set_active(true);
			break;

		default:
			break;
	}

	m_updatingContextMenu = false;
}
