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
	@brief  Event handling code for WaveformArea
 */

#include "glscopeclient.h"
#include "WaveformArea.h"
#include "OscilloscopeWindow.h"
#include <random>
#include "ProfileBlock.h"
#include "ChannelPropertiesDialog.h"
#include "../../lib/scopeprotocols/EyeDecoder2.h"
#include "../../lib/scopeprotocols/WaterfallDecoder.h"

using namespace std;
using namespace glm;

extern int g_numDecodes;

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

	err = glGetError();
	if(err != 0)
		LogNotice("resize 2, err = %x\n", err);

	//Reallocate waveform texture
	m_waveformRenderData->m_waveformTexture.Bind();
	m_waveformRenderData->m_waveformTexture.SetData(width, height, NULL, GL_RGBA, GL_UNSIGNED_BYTE, GL_RGBA32F);
	ResetTextureFiltering();

	//Reallocate textures for overlays
	for(auto it : m_overlayRenderData)
	{
		it.second->m_waveformTexture.Bind();
		it.second->m_waveformTexture.SetData(width, height, NULL, GL_RGBA, GL_UNSIGNED_BYTE, GL_RGBA32F);
		ResetTextureFiltering();
	}

	SetGeometryDirty();

	err = glGetError();
	if(err != 0)
		LogNotice("resize 3, err = %x\n", err);

	//double dt = GetTime() - start;
	//LogDebug("Resize time: %.3f ms\n", dt*1000);

	//If it's an eye pattern or waterfall, resize it
	if(IsEye())
	{
		auto eye = dynamic_cast<EyeDecoder2*>(m_channel);
		eye->SetWidth(m_width/4);
		eye->SetHeight(m_height);
		eye->Refresh();
	}
	else if(IsWaterfall())
	{
		auto waterfall = dynamic_cast<WaterfallDecoder*>(m_channel);
		waterfall->SetWidth(m_width);
		waterfall->SetHeight(m_height);
	}
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
					if(!IsEyeOrBathtub())
						m_parent->OnZoomInHorizontal(m_group);
					break;
				case GDK_SCROLL_DOWN:
					if(!IsEyeOrBathtub())
						m_parent->OnZoomOutHorizontal(m_group);
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
						SetGeometryDirty();
						queue_draw();
						break;
					case GDK_SCROLL_DOWN:
						m_channel->SetVoltageRange(vrange / 0.9);
						SetGeometryDirty();
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

	for(auto it : m_overlayPositions)
	{
		int top = it.second - 10;
		int bot = it.second + 10;
		if( (event->y >= top) && (event->y <= bot) )
			m_selectedChannel = it.first;
	}

	//Look up the time of our click (if in the plot area)
	int64_t timestamp = XPositionToXAxisUnits(event->x);

	if(event->type == GDK_BUTTON_PRESS)
		OnSingleClick(event, timestamp);
	else if(event->type == GDK_2BUTTON_PRESS)
		OnDoubleClick(event, timestamp);

	return true;
}

void WaveformArea::OnSingleClick(GdkEventButton* event, int64_t timestamp)
{
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
					//Left
					case 1:

						//For now, can only change offset on voltage channels
						if(m_channel->GetYAxisUnits() != Unit::UNIT_VOLTS)
							return;

						m_dragState = DRAG_OFFSET;
						m_dragStartVoltage = YPositionToVolts(event->y);
						break;

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

		//Drag channel name
		case LOC_CHAN_NAME:
			{
				switch(event->button)
				{
					//Left
					case 1:

						if(m_selectedChannel == m_channel)
							m_dragState = DRAG_WAVEFORM_AREA;
						else
						{
						//	LogDebug("Start dragging overlay (not yet implemented)\n");
						}
						break;

					default:
						break;

				}
			}
			break;

		default:
			break;
	}
}

void WaveformArea::OnDoubleClick(GdkEventButton* /*event*/, int64_t /*timestamp*/)
{
	switch(m_clickLocation)
	{
		//Double click on channel name to pop up the config dialog
		case LOC_CHAN_NAME:
			{
				//See if it's a physical channel
				if(m_selectedChannel->IsPhysicalChannel())
				{
					ChannelPropertiesDialog dialog(m_parent, m_selectedChannel);
					if(dialog.run() == Gtk::RESPONSE_OK)
					{
						dialog.ConfigureChannel();
						queue_draw();
					}
				}

				//No, it's a decode
				else
				{
					auto decode = dynamic_cast<ProtocolDecoder*>(m_selectedChannel);
					if(decode)
					{
						m_decodeDialog = new ProtocolDecoderDialog(m_parent, decode, NULL);
						m_decodeDialog->show();
						m_decodeDialog->signal_response().connect(
							sigc::mem_fun(*this, &WaveformArea::OnDecodeReconfigureDialogResponse));
					}
					else
					{
						LogError("Channel \"%s\" is neither a protocol decode nor a physical channel\n",
							m_selectedChannel->m_displayname.c_str());
					}
				}

			}
			break;

		default:
			break;
	}
}

bool WaveformArea::on_button_release_event(GdkEventButton* event)
{
	int64_t timestamp = XPositionToXAxisUnits(event->x);

	switch(m_dragState)
	{
		//Update scope trigger configuration if left mouse is released
		case DRAG_TRIGGER:
			if(event->button == 1)
			{
				m_channel->GetScope()->SetTriggerVoltage(YPositionToVolts(event->y));
				m_parent->ClearAllPersistence();
				queue_draw();
			}
			break;

		case DRAG_CURSOR:
			if(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL)
				m_group->m_xCursorPos[1] = timestamp;
			break;

		case DRAG_WAVEFORM_AREA:
			if(m_dropTarget != NULL)
			{
				//Move us to a new group if needed
				if(m_dropTarget->m_group != m_group)
					m_parent->OnMoveToExistingGroup(this, m_dropTarget->m_group);

				//Reorder within the group
				int target_position = 0;
				auto children = m_group->m_waveformBox.get_children();
				for(int i=0; i<(int)children.size(); i++)
				{
					if(children[i] == m_dropTarget)
					{
						if(m_dropTarget->m_insertionBarLocation == INSERT_TOP)
							target_position = i;
						else
							target_position = i+1;
					}
				}
				m_group->m_waveformBox.reorder_child(*this, target_position);

				//Not dragging anymore
				m_dropTarget->m_insertionBarLocation = INSERT_NONE;
			}
			break;

		default:
			break;
	}

	//Stop dragging things
	if(m_dragState != DRAG_NONE)
	{
		m_dropTarget = NULL;
		m_dragState = DRAG_NONE;
		queue_draw();
	}

	return true;
}

bool WaveformArea::on_motion_notify_event(GdkEventMotion* event)
{
	m_cursorX = event->x;
	m_cursorY = event->y;

	int64_t timestamp = XPositionToXAxisUnits(event->x);

	switch(m_dragState)
	{
		//Trigger drag - update level and refresh
		case DRAG_TRIGGER:
			m_channel->GetScope()->SetTriggerVoltage(YPositionToVolts(event->y));
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

		//Offset drag - update level and refresh
		case DRAG_OFFSET:
			{
				double dv = YPositionToVolts(event->y) - m_dragStartVoltage;
				double old_offset = m_channel->GetOffset();
				m_channel->SetOffset(old_offset + dv);
				queue_draw();
			}
			break;

		//Move this waveform area to a new place
		case DRAG_WAVEFORM_AREA:
			{
				//Window coordinates of our cursor
				int window_x;
				int window_y;
				get_window()->get_origin(window_x, window_y);
				auto alloc = get_allocation();
				int real_x = event->x + alloc.get_x() + window_x;
				int real_y = event->y + alloc.get_y() + window_y;
				Gdk::Rectangle rect(real_x, real_y, 1, 1);

				//Check all waveform areas to see which one we hit
				WaveformArea* target = NULL;
				for(auto w : m_parent->m_waveformAreas)
				{
					int wx;
					int wy;
					w->get_window()->get_origin(wx, wy);
					auto trect = w->get_allocation();
					trect.set_x(trect.get_x() + wx);
					trect.set_y(trect.get_y() + wy);

					if(trect.intersects(rect))
						target = w;

					//If dragging outside another area, clear the insertion mark
					else if(w->m_insertionBarLocation != INSERT_NONE)
					{
						w->m_insertionBarLocation = INSERT_NONE;
						w->queue_draw();
					}
				}

				//Outside the view area, nothing to do
				if(target == NULL)
					m_dropTarget = NULL;

				else
				{
					alloc = target->get_allocation();
					m_dropTarget = target;

					int wx;
					int wy;
					target->get_window()->get_origin(wx, wy);
					alloc.set_x(alloc.get_x() + wx);
					alloc.set_y(alloc.get_y() + wy);

					//int target_x = real_x - alloc.get_x();
					int target_y = real_y - alloc.get_y();

					if(target_y > target->m_height/2)
						target->m_insertionBarLocation = INSERT_BOTTOM;
					else
						target->m_insertionBarLocation = INSERT_TOP;
					target->queue_draw();
				}
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

void WaveformArea::OnCopyNewRight()
{
	m_parent->OnCopyNewRight(this);
}

void WaveformArea::OnCopyNewBelow()
{
	m_parent->OnCopyNewBelow(this);
}

void WaveformArea::OnCopyToExistingGroup(WaveformGroup* group)
{
	m_parent->OnCopyToExistingGroup(this, group);
}

void WaveformArea::OnHide()
{
	//Delete the entire waveform area
	if(m_selectedChannel == m_channel)
		m_parent->OnRemoveChannel(this);

	//Deleting an overlay
	else
	{
		//LogDebug("Deleting overlay %s\n", m_selectedChannel->m_displayname.c_str());

		//Remove the overlay from the list
		for(size_t i=0; i<m_overlays.size(); i++)
		{
			if(m_overlays[i] == m_selectedChannel)
			{
				OnRemoveOverlay(m_overlays[i]);
				m_overlays.erase(m_overlays.begin() + i);
				break;
			}
		}

		queue_draw();
	}
}

void WaveformArea::OnTogglePersistence()
{
	m_persistence = !m_persistence;
	queue_draw();
}

void WaveformArea::OnProtocolDecode(string name)
{
	//Create a new decoder for the incoming signal
	string color = GetDefaultChannelColor(g_numDecodes);
	if(m_pendingDecode)
		delete m_pendingDecode;
	m_pendingDecode = ProtocolDecoder::CreateDecoder(name, color);

	//Only one input with no config required? Do default configuration
	if( (m_pendingDecode->GetInputCount() == 1) && !m_pendingDecode->NeedsConfig())
	{
		m_pendingDecode->SetInput(0, m_selectedChannel);
		m_pendingDecode->SetDefaultName();
		OnDecodeSetupComplete();
	}

	//Multiple inputs or config needed? Show the dialog
	else
	{
		if(m_decodeDialog)
			delete m_decodeDialog;
		m_decodeDialog = new ProtocolDecoderDialog(m_parent, m_pendingDecode, m_selectedChannel);
		m_decodeDialog->show();
		m_decodeDialog->signal_response().connect(sigc::mem_fun(*this, &WaveformArea::OnDecodeDialogResponse));
	}
}

void WaveformArea::OnDecodeDialogResponse(int response)
{
	//Clean up decoder if canceled
	if(response != Gtk::RESPONSE_OK)
	{
		delete m_pendingDecode;
		m_pendingDecode = NULL;
	}

	//All good, set it up
	else
	{
		m_decodeDialog->ConfigureDecoder();
		OnDecodeSetupComplete();
	}

	//Clean up the dialog
	delete m_decodeDialog;
	m_decodeDialog = NULL;
}

void WaveformArea::OnDecodeReconfigureDialogResponse(int response)
{
	//Apply the changes
	if(response == Gtk::RESPONSE_OK)
	{
		m_decodeDialog->ConfigureDecoder();
		queue_draw();
	}

	//Clean up the dialog
	delete m_decodeDialog;
	m_decodeDialog = NULL;
}

void WaveformArea::OnDecodeSetupComplete()
{
	//Increment the color chooser only after we've decided to add the decode.
	//If the dialog is canceled, don't do anything.
	g_numDecodes ++;

	//If it's an eye pattern or waterfall, set the initial size
	auto eye = dynamic_cast<EyeDecoder2*>(m_pendingDecode);
	if(eye != NULL)
	{
		eye->SetWidth(m_width / 4);
		eye->SetHeight(m_height);
	}
	auto fall = dynamic_cast<WaterfallDecoder*>(m_pendingDecode);
	if(fall != NULL)
	{
		fall->SetWidth(m_width);
		fall->SetHeight(m_height);
		fall->SetTimeScale(m_group->m_pixelsPerXUnit);
	}

	//Run the decoder for the first time, so we get valid output even if there's not a trigger pending.
	m_pendingDecode->Refresh();

	//Create a new waveform view for the generated signal
	if(!m_pendingDecode->IsOverlay())
		m_parent->DoAddChannel(m_pendingDecode, m_group, this);

	//It's an overlay. Reference it and add to our overlay list
	else
	{
		m_pendingDecode->AddRef();
		m_overlays.push_back(m_pendingDecode);
		queue_draw();
	}

	//If the decoder is a packet-oriented protocol, pop up a protocol analyzer
	//TODO: UI for re-opening the analyzer if we close it?
	//TODO: allow protocol decoder dialogs to reconfigure decoder in the future
	auto pdecode = dynamic_cast<PacketDecoder*>(m_pendingDecode);
	if(pdecode != NULL)
	{
		char title[256];
		snprintf(title, sizeof(title), "Protocol Analyzer: %s", m_pendingDecode->m_displayname.c_str());

		auto analyzer = new ProtocolAnalyzerWindow(title, m_parent, pdecode, this);
		m_parent->m_analyzers.emplace(analyzer);

		analyzer->OnWaveformDataReady();
		analyzer->show();
	}

	//This decode is no longer pending
	m_pendingDecode = NULL;
}

void WaveformArea::OnMeasure(string name)
{
	m_group->AddColumn(name, m_selectedChannel, m_selectedChannel->m_displaycolor);
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

	m_channel->GetScope()->SetTriggerChannelIndex(m_channel->GetIndex());
	m_channel->GetScope()->SetTriggerType(type);
	m_parent->ClearAllPersistence();
}

void WaveformArea::OnWaveformDataReady()
{
	//If we're a fixed width curve, refresh the parent's time scale
	if(IsEyeOrBathtub())
	{
		auto eye = dynamic_cast<EyeDecoder2*>(m_channel);
		if(eye == NULL)
			eye = dynamic_cast<EyeDecoder2*>(dynamic_cast<ProtocolDecoder*>(m_channel)->GetInput(0));
		int64_t width = eye->GetUIWidth();

		//eye is two UIs wide
		int64_t eye_width_ps = 2 * width;

		//If decode fails for some reason, don't have an invalid timeline
		if(eye_width_ps == 0)
			eye_width_ps = 5;

		m_group->m_pixelsPerXUnit = m_width * 1.0f / eye_width_ps;
		m_group->m_xAxisOffset = -width;
	}

	//Update our measurements and redraw the waveform
	SetGeometryDirty();
	queue_draw();
	m_group->m_timeline.queue_draw();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers

/**
	@brief Update the location of the mouse
 */
WaveformArea::ClickLocation WaveformArea::HitTest(double x, double y)
{
	//On the main channel name button?
	if(m_infoBoxRect.HitTest(x, y))
	{
		m_selectedChannel = m_channel;
		return LOC_CHAN_NAME;
	}

	//On an overlay info box?
	for(auto it : m_overlayBoxRects)
	{
		if(it.second.HitTest(x, y))
		{
			m_selectedChannel = it.first;
			return LOC_CHAN_NAME;
		}
	}

	if(x > m_plotRight)
	{
		//On the trigger button?
		if((m_channel->GetScope() != NULL) && (m_channel->GetIndex() == m_channel->GetScope()->GetTriggerChannelIndex()) )
		{
			float vy = VoltsToYPosition(m_channel->GetScope()->GetTriggerVoltage());
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

void WaveformArea::UpdateMeasureContextMenu(std::vector<Widget*> children)
{
	for(auto item : children)
	{
		Gtk::MenuItem* menu = dynamic_cast<Gtk::MenuItem*>(item);
		if(menu == NULL)
			continue;

		auto m = Measurement::CreateMeasurement(menu->get_label());
		menu->set_sensitive(m->ValidateChannel(0, m_selectedChannel));
		delete m;
	}
}

/**
	@brief Enable/disable or show/hide context menu items for the current selection
 */
void WaveformArea::UpdateContextMenu()
{
	//Let signal handlers know to ignore any events that happen as we pull state from the scope
	m_updatingContextMenu = true;

	//Clean out old group stuff
	for(auto m : m_moveExistingGroupItems)
	{
		m_moveMenu.remove(*m);
		delete m;
	}
	m_moveExistingGroupItems.clear();
	for(auto m : m_copyExistingGroupItems)
	{
		m_copyMenu.remove(*m);
		delete m;
	}
	m_copyExistingGroupItems.clear();

	//Add new entries
	for(auto g : m_parent->m_waveformGroups)
	{
		//Move
		auto item = new Gtk::MenuItem;
		item->set_label(g->m_frame.get_label());
		m_moveMenu.append(*item);
		m_moveExistingGroupItems.emplace(item);
		if(get_parent() == &g->m_waveformBox)
			item->set_sensitive(false);
		item->signal_activate().connect(sigc::bind<WaveformGroup*>(
			sigc::mem_fun(*this, &WaveformArea::OnMoveToExistingGroup), g));

		//Copy
		item = new Gtk::MenuItem;
		item->set_label(g->m_frame.get_label());
		m_copyMenu.append(*item);
		m_copyExistingGroupItems.emplace(item);
		//don't disable if in this group, it's OK to copy to ourself
		item->signal_activate().connect(sigc::bind<WaveformGroup*>(
			sigc::mem_fun(*this, &WaveformArea::OnCopyToExistingGroup), g));
	}
	m_moveMenu.show_all();
	m_copyMenu.show_all();

	//Gray out decoders that don't make sense for the type of channel we've selected
	vector<Gtk::Menu*> childMenus;
	childMenus.push_back(&m_decodeAlphabeticalMenu);
	childMenus.push_back(&m_decodeBusMenu);
	childMenus.push_back(&m_decodeSignalIntegrityMenu);
	childMenus.push_back(&m_decodeClockMenu);
	childMenus.push_back(&m_decodeMathMenu);
	childMenus.push_back(&m_decodeMeasurementMenu);
	childMenus.push_back(&m_decodeMemoryMenu);
	childMenus.push_back(&m_decodeMiscMenu);
	childMenus.push_back(&m_decodeSerialMenu);

	for(auto submenu : childMenus)
	{
		auto subchildren = submenu->get_children();
		for(auto item : subchildren)
		{
			Gtk::MenuItem* menu = dynamic_cast<Gtk::MenuItem*>(item);
			if(menu == NULL)
				continue;

			auto decoder = ProtocolDecoder::CreateDecoder(
				menu->get_label(),
				"");
			menu->set_sensitive(decoder->ValidateChannel(0, m_selectedChannel));
			delete decoder;
		}
	}

	//Gray out measurements that don't make sense for the type of channel we've selected
	auto children = m_measureHorzMenu.get_children();
	UpdateMeasureContextMenu(children);
	children = m_measureVertMenu.get_children();
	UpdateMeasureContextMenu(children);

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

		if(m_channel->GetScope()->GetTriggerChannelIndex() != m_channel->GetIndex())
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

			switch(m_channel->GetScope()->GetTriggerType())
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

	//Set stats checkbox
	m_statisticsItem.set_active(m_group->IsShowingStats(m_selectedChannel));

	m_updatingContextMenu = false;
}

void WaveformArea::OnStatistics()
{
	if(m_updatingContextMenu)
		return;

	if(m_statisticsItem.get_active())
		m_group->ToggleOn(m_selectedChannel);
	else
		m_group->ToggleOff(m_selectedChannel);
}
