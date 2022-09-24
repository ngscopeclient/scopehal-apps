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
	@brief  Event handling code for WaveformArea
 */

#include "glscopeclient.h"
#include "WaveformArea.h"
#include "OscilloscopeWindow.h"
#include <random>
#include <map>
#include "ChannelPropertiesDialog.h"
#include "../../lib/scopeprotocols/EyePattern.h"
#include "../../lib/scopeprotocols/Waterfall.h"
#include "../../lib/scopehal/TwoLevelTrigger.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Window events

void WaveformArea::on_resize(int width, int height)
{
	//vestigial resize
	if( (m_width == width) && (m_height == height) )
		return;

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
	m_waveformRenderData->m_renderedWaveform.resize(m_plotRight * height);
	//TODO: Only zero m_renderedWaveform if persistence is enabled?
	m_waveformRenderData->m_renderedWaveform.PrepareForCpuAccess();
	memset(m_waveformRenderData->m_renderedWaveform.GetCpuPointer(), 0, m_plotRight * height * sizeof(float));
	m_waveformRenderData->m_renderedWaveform.MarkModifiedFromCpu();
	ResetTextureFiltering(); //<- TODO: Necessary?

	//Reallocate textures for overlays
	for(auto it : m_overlayRenderData)
	{
		it.second->m_renderedWaveform.resize(m_plotRight * height);
		//TODO: Only zero m_renderedWaveform if persistence is enabled?
		it.second->m_renderedWaveform.PrepareForCpuAccess();
		memset(it.second->m_renderedWaveform.GetCpuPointer(), 0, m_plotRight * height * sizeof(float));
		it.second->m_renderedWaveform.MarkModifiedFromCpu();
		ResetTextureFiltering(); //<- TODO: Necessary?
	}

	err = glGetError();
	if(err != 0)
		LogNotice("resize 3, err = %x\n", err);

	//double dt = GetTime() - start;
	//LogDebug("Resize time: %.3f ms\n", dt*1000);

	//If it's an eye pattern or waterfall, resize it
	if(IsEye())
	{
		auto eye = dynamic_cast<EyePattern*>(m_channel.m_channel);
		eye->SetWidth(m_width);
		eye->SetHeight(m_height);
		eye->RecalculateUIWidth();
		auto wave = dynamic_cast<EyeWaveform*>(eye->GetData(0));
		RescaleEye(eye, wave);
		eye->Refresh();
	}
	else if(IsWaterfall())
	{
		auto waterfall = dynamic_cast<Waterfall*>(m_channel.m_channel);
		waterfall->SetWidth(m_width);
		waterfall->SetHeight(m_height);
	}

	SetGeometryDirty();
	SetPositionDirty();
	queue_draw();
}

bool WaveformArea::on_scroll_event (GdkEventScroll* ev)
{
	//Scaling for hi-dpi
	auto scale = get_window()->get_scale_factor();
	ev->x *= scale;
	ev->y *= scale;

	m_clickLocation = HitTest(ev->x, ev->y);

	switch(m_clickLocation)
	{
		//Adjust time/div
		case LOC_PLOT:

			switch(ev->direction)
			{
				case GDK_SCROLL_UP:
					if(!IsEyeOrBathtub())
					{
						if(ev->state & GDK_SHIFT_MASK)
						{
							m_group->m_xAxisOffset -= 50.0 / m_group->m_pixelsPerXUnit;
							m_group->GetParent()->ClearPersistence(m_group, false, true);
						}

						else
							m_parent->OnZoomInHorizontal(m_group, XPositionToXAxisUnits(ev->x));
					}
					break;
				case GDK_SCROLL_DOWN:
					if(!IsEyeOrBathtub())
					{
						if(ev->state & GDK_SHIFT_MASK)
						{
							m_group->m_xAxisOffset += 50.0 / m_group->m_pixelsPerXUnit;
							m_group->GetParent()->ClearPersistence(m_group, false, true);
						}
						else
							m_parent->OnZoomOutHorizontal(m_group, XPositionToXAxisUnits(ev->x));
					}
					break;
				case GDK_SCROLL_LEFT:
					if(!IsEyeOrBathtub())
					{
						m_group->m_xAxisOffset -= 50.0 / m_group->m_pixelsPerXUnit;
						m_group->GetParent()->ClearPersistence(m_group, false, true);
					}
					break;
				case GDK_SCROLL_RIGHT:
					if(!IsEyeOrBathtub())
					{
						m_group->m_xAxisOffset += 50.0 / m_group->m_pixelsPerXUnit;
						m_group->GetParent()->ClearPersistence(m_group, false, true);
					}
					break;

				default:
					break;
			}
			break;

		//Adjust volts/div
		case LOC_VSCALE:
			{
				double vrange = m_channel.GetVoltageRange();
				switch(ev->direction)
				{
					case GDK_SCROLL_UP:
						m_channel.SetVoltageRange(vrange * 0.9);
						ClearPersistence();
						SetGeometryDirty();
						queue_draw();
						break;
					case GDK_SCROLL_DOWN:
						m_channel.SetVoltageRange(vrange / 0.9);
						ClearPersistence();
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
	//Scaling for hi-dpi
	auto scale = get_window()->get_scale_factor();
	event->x *= scale;
	event->y *= scale;

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

	//Look up the time and voltage of our click (if in the plot area)
	int64_t timestamp = SnapX(XPositionToXAxisUnits(event->x), event->x, event->y);
	float voltage = YPositionToYAxisUnits(event->y);

	if(event->type == GDK_BUTTON_PRESS)
		OnSingleClick(event, timestamp, voltage);
	else if(event->type == GDK_2BUTTON_PRESS)
		OnDoubleClick(event, timestamp);

	return true;
}

bool WaveformArea::on_key_press_event(GdkEventKey* event)
{
	switch(event->keyval)
	{
		case GDK_KEY_F25:
			//TODO
			break;

		default:
			break;
	}

	return false;
}

void WaveformArea::OnSingleClick(GdkEventButton* event, int64_t timestamp, float voltage)
{
	//Scaling for hi-dpi
	auto scale = get_window()->get_scale_factor();
	event->x *= scale;
	event->y *= scale;

	//Hotkeys
	switch(event->button)
	{
		case 9:
			m_parent->OnStart();
			break;

		case 8:
			m_parent->OnStop();
			break;
	}

	switch(m_clickLocation)
	{
		//Move cursors if we click on them
		case LOC_XCURSOR_0:
			if(event->button == 1)
			{
				m_dragState = DRAG_CURSOR_0;
				m_group->m_xCursorPos[0] = timestamp;
				OnCursorMoved();
				m_group->m_vbox.queue_draw();
			}
			break;

		case LOC_XCURSOR_1:
			if(event->button == 1)
			{
				m_dragState = DRAG_CURSOR_1;
				m_group->m_xCursorPos[1] = timestamp;
				OnCursorMoved();
				m_group->m_vbox.queue_draw();
			}
			break;

		case LOC_YCURSOR_0:
			if(event->button == 1)
			{
				m_dragState = DRAG_CURSOR_0;
				m_group->m_yCursorPos[0] = timestamp;
				OnCursorMoved();
				m_group->m_vbox.queue_draw();
			}
			break;

		case LOC_YCURSOR_1:
			if(event->button == 1)
			{
				m_dragState = DRAG_CURSOR_1;
				m_group->m_yCursorPos[1] = timestamp;
				OnCursorMoved();
				m_group->m_vbox.queue_draw();
			}
			break;

		case LOC_MARKER:
			if(event->button == 1)
			{
				m_dragState = DRAG_MARKER;
				m_selectedMarker->m_offset = timestamp;
				OnMarkerMoved();
				m_group->m_vbox.queue_draw();
			}
			break;

		//Waveform area
		case LOC_PLOT:
			{
				switch(event->button)
				{
					//Left
					case 1:

						switch(m_group->m_cursorConfig)
						{
							//Place a new pair of cursors
							case WaveformGroup::CURSOR_X_DUAL:
								m_dragState = DRAG_CURSOR_1;
								m_group->m_xCursorPos[0] = timestamp;
								m_group->m_xCursorPos[1] = timestamp;
								break;

							case WaveformGroup::CURSOR_Y_DUAL:
								m_dragState = DRAG_CURSOR_1;
								m_group->m_yCursorPos[0] = voltage;
								m_group->m_yCursorPos[1] = voltage;
								break;

							//Place the first cursor
							case WaveformGroup::CURSOR_X_SINGLE:
								m_dragState = DRAG_CURSOR_0;
								m_group->m_xCursorPos[0] = timestamp;
								break;

							case WaveformGroup::CURSOR_Y_SINGLE:
								m_dragState = DRAG_CURSOR_0;
								m_group->m_yCursorPos[0] = voltage;
								break;

							default:
								break;
						}

						//Redraw if we have any cursor
						if(m_group->m_cursorConfig != WaveformGroup::CURSOR_NONE)
						{
							OnCursorMoved();
							m_group->m_vbox.queue_draw();
						}

						break;

					//Middle
					case 2:
						m_parent->OnAutofitHorizontal(m_group);
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
						m_dragState = DRAG_OFFSET;
						m_dragStartVoltage = YPositionToYAxisUnits(event->y);
						get_window()->set_cursor(Gdk::Cursor::create(get_display(), "grabbing"));
						break;

					//Middle
					case 2:
						{
							auto f = dynamic_cast<Filter*>(m_channel.m_channel);
							if(f)
							{
								f->AutoscaleVertical(m_channel.m_stream);
								ClearPersistence();
								queue_draw();
							}
						}
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

		case LOC_TRIGGER_SECONDARY:
			{
				switch(event->button)
				{
					//Left
					case 1:
						m_dragState = DRAG_TRIGGER_SECONDARY;
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
							m_dragState = DRAG_OVERLAY;
							m_dragOverlayPosition =	m_overlayPositions[m_selectedChannel] - 10;
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
	//Stop any in-progress drag if we double click
	if(m_dragState != DRAG_NONE)
	{
		m_dragState = DRAG_NONE;
		queue_draw();
	}

	switch(m_clickLocation)
	{
		//Double click on channel name to pop up the config dialog
		case LOC_CHAN_NAME:
			{
				//See if it's a physical channel
				if(m_selectedChannel.m_channel->IsPhysicalChannel())
				{
					//TODO: make this modeless
					ChannelPropertiesDialog dialog(m_parent, m_selectedChannel.m_channel);
					if(dialog.run() == Gtk::RESPONSE_OK)
					{
						auto oldname = m_selectedChannel.m_channel->GetDisplayName();
						dialog.ConfigureChannel();
						if(m_selectedChannel.m_channel->GetDisplayName() != oldname)
							m_parent->OnChannelRenamed(m_selectedChannel.m_channel);

						//Clear any waveforms acquired while the dialog was open
						for(size_t i=0; i<m_parent->GetScopeCount(); i++)
							m_parent->GetScope(i)->ClearPendingWaveforms();

						//TODO: only if ADC bit depth changed?
						//(if changed we really want to clear persistence for all channels in the group...)
						ClearPersistence(false);

						m_parent->RefreshChannelsMenu();		//update the menu with the channel's new name
						m_parent->RefreshFilterGraphEditor();
						queue_draw();
					}
				}

				//No, it's a decode
				else
				{
					auto decode = dynamic_cast<Filter*>(m_selectedChannel.m_channel);
					if(decode)
					{
						m_decodeDialog = new FilterDialog(m_parent, decode, StreamDescriptor(NULL, 0));
						m_decodeDialog->show();
						m_decodeDialog->signal_delete_event().connect(
							sigc::mem_fun(*this, &WaveformArea::OnDecodeReconfigureDialogClosed));
					}
					else
					{
						LogError("Channel \"%s\" is neither a protocol decode nor a physical channel\n",
							m_selectedChannel.m_channel->GetDisplayName().c_str());
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
	//Scaling for hi-dpi
	auto scale = get_window()->get_scale_factor();
	event->x *= scale;
	event->y *= scale;

	int64_t timestamp = XPositionToXAxisUnits(event->x);
	auto region = HitTest(event->x, event->y);

	switch(m_dragState)
	{
		//Update scope trigger configuration if left mouse is released
		case DRAG_TRIGGER:
			if(event->button == 1)
			{
				auto scope = m_channel.m_channel->GetScope();
				auto trig = scope->GetTrigger();
				trig->SetLevel(YPositionToYAxisUnits(event->y));
				scope->PushTrigger();
				m_parent->ClearAllPersistence();
				queue_draw();
			}
			break;

		//Release a marker
		case DRAG_MARKER:
			timestamp = SnapX(timestamp, event->x, event->y);
			m_selectedMarker->m_offset = timestamp;
			m_selectedMarker = nullptr;
			break;

		case DRAG_TRIGGER_SECONDARY:
			if(event->button == 1)
			{
				auto scope = m_channel.m_channel->GetScope();
				auto trig = dynamic_cast<TwoLevelTrigger*>(scope->GetTrigger());
				if(trig)
				{
					trig->SetLowerBound(YPositionToYAxisUnits(event->y));
					scope->PushTrigger();
				}
				m_parent->ClearAllPersistence();
				queue_draw();
			}
			break;

		//Move the cursor
		case DRAG_CURSOR_0:
			timestamp = SnapX(timestamp, event->x, event->y);
			m_group->m_xCursorPos[0] = timestamp;
			OnCursorMoved();
			break;

		case DRAG_CURSOR_1:
			if(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL)
			{
				timestamp = SnapX(timestamp, event->x, event->y);
				m_group->m_xCursorPos[1] = timestamp;
				OnCursorMoved();
			}
			break;

		//Drag the entire waveform area to a new location
		case DRAG_WAVEFORM_AREA:
			if(m_dropTarget != NULL)
			{
				//If dragging to the same area, don't register the hit
				if( (m_dropTarget->m_group == m_group) && (region == LOC_CHAN_NAME) )
					break;

				//Move us to a new group if needed
				if(m_dropTarget->m_group != m_group)
					m_parent->OnMoveToExistingGroup(this, m_dropTarget->m_group);

				//Create a new group if we're dragging to the edge of the viewport
				if(m_dropTarget->m_insertionBarLocation == INSERT_BOTTOM_SPLIT)
					m_parent->OnMoveNewBelow(this);
				else if(m_dropTarget->m_insertionBarLocation == INSERT_RIGHT_SPLIT)
					m_parent->OnMoveNewRight(this);

				else
				{
					//Reorder within the group
					int target_position = m_group->GetIndexOfChild(m_dropTarget);
					switch(m_dropTarget->m_insertionBarLocation)
					{
						case INSERT_BOTTOM:
							target_position ++;
							break;

						default:
							break;
					}

					m_group->m_waveformBox.reorder_child(*this, target_position);
				}

				//Not dragging anymore
				m_dropTarget->m_insertionBarLocation = INSERT_NONE;
			}
			break;

		//Move overlay to a new location
		case DRAG_OVERLAY:
			{
				//Sort overlays by position
				std::map<int, StreamDescriptor> revmap;
				vector<int> positions;
				for(auto it : m_overlayPositions)
				{
					revmap[it.second] = it.first;
					positions.push_back(it.second);
				}
				std::sort(positions.begin(), positions.end(), less<int>());

				//Make a new, sorted list of decode positions
				vector<StreamDescriptor> sorted;
				bool inserted = false;
				for(size_t i=0; i<positions.size(); i++)
				{
					int pos = positions[i];
					if( (pos > m_dragOverlayPosition) && !inserted)
					{
						sorted.push_back(m_selectedChannel);
						inserted = true;
					}

					if(revmap[pos] != m_selectedChannel)
						sorted.push_back(revmap[pos]);
				}
				if(!inserted)
					sorted.push_back(m_selectedChannel);

				//Reposition everything
				int pos = m_overlaySpacing / 2;
				for(auto d : sorted)
				{
					m_overlayPositions[d] = pos;
					pos += m_overlaySpacing;
				}
				queue_draw();
			}
			break;

		//Done dragging the offset
		case DRAG_OFFSET:
			get_window()->set_cursor(Gdk::Cursor::create(get_display(), "grab"));
			break;

		default:
			break;
	}

	//Stop dragging things
	if(m_dragState != DRAG_NONE)
	{
		m_dropTarget = NULL;
		m_dragState = DRAG_NONE;
		m_insertionBarLocation = INSERT_NONE;
		queue_draw();
	}

	return true;
}

bool WaveformArea::on_motion_notify_event(GdkEventMotion* event)
{
	//Scaling for hi-dpi
	auto scale = get_window()->get_scale_factor();
	event->x *= scale;
	event->y *= scale;

	m_cursorX = event->x;
	m_cursorY = event->y;

	int64_t timestamp = XPositionToXAxisUnits(event->x);
	float voltage = YPositionToYAxisUnits(event->y);

	//Figure out what UI element (if any) the cursor currently is on top of
	ClickLocation oldLocation = m_mouseElementPosition;
	m_mouseElementPosition = HitTest(event->x, event->y);

	switch(m_dragState)
	{
		//Trigger drag - update level and refresh
		//TODO: what happens if window trigger arrows cross?
		case DRAG_TRIGGER:
			{
				auto scope = m_channel.m_channel->GetScope();
				auto trig = scope->GetTrigger();
				trig->SetLevel(YPositionToYAxisUnits(event->y));
				scope->PushTrigger();
				queue_draw();
			}
			break;

		case DRAG_TRIGGER_SECONDARY:
			{
				auto scope = m_channel.m_channel->GetScope();
				auto trig = dynamic_cast<TwoLevelTrigger*>(scope->GetTrigger());
				if(trig)
				{
					trig->SetLowerBound(YPositionToYAxisUnits(event->y));
					scope->PushTrigger();
				}
				queue_draw();
			}
			break;

		case DRAG_MARKER:

			//Snap to the closest toggle
			timestamp = SnapX(timestamp, event->x, event->y);

			m_selectedMarker->m_offset = timestamp;
			OnMarkerMoved();
			m_group->m_vbox.queue_draw();
			break;


		case DRAG_CURSOR_0:

			//Snap to the closest toggle
			timestamp = SnapX(timestamp, event->x, event->y);

			switch(m_group->m_cursorConfig)
			{
				case WaveformGroup::CURSOR_X_DUAL:
					m_group->m_xCursorPos[0] = timestamp;

					//Cursor 0 should always be left of 1.
					//If they cross, flip them
					if(m_group->m_xCursorPos[0] > m_group->m_xCursorPos[1])
					{
						m_dragState = DRAG_CURSOR_1;
						int64_t tmp = m_group->m_xCursorPos[1];
						m_group->m_xCursorPos[1] = m_group->m_xCursorPos[0];
						m_group->m_xCursorPos[0] = tmp;
					}
					break;

				case WaveformGroup::CURSOR_X_SINGLE:
					m_group->m_xCursorPos[0] = timestamp;
					break;

				case WaveformGroup::CURSOR_Y_DUAL:
					m_group->m_yCursorPos[0] = voltage;

					//Cursor 0 should always be above 1.
					//If they cross, flip them
					if(m_group->m_yCursorPos[0] < m_group->m_yCursorPos[1])
					{
						m_dragState = DRAG_CURSOR_1;
						int64_t tmp = m_group->m_yCursorPos[1];
						m_group->m_yCursorPos[1] = m_group->m_yCursorPos[0];
						m_group->m_yCursorPos[0] = tmp;
					}
					break;

				case WaveformGroup::CURSOR_Y_SINGLE:
					m_group->m_yCursorPos[0] = voltage;
					break;

				default:
					break;
			}

			OnCursorMoved();
			m_group->m_vbox.queue_draw();
			break;

		case DRAG_CURSOR_1:

			//Snap to the closest toggle
			timestamp = SnapX(timestamp, event->x, event->y);

			switch(m_group->m_cursorConfig)
			{
				case WaveformGroup::CURSOR_X_DUAL:
					m_group->m_xCursorPos[1] = timestamp;

					//Cursor 0 should always be left of 1
					//If they cross, flip them
					if(m_group->m_xCursorPos[0] > m_group->m_xCursorPos[1])
					{
						m_dragState = DRAG_CURSOR_0;
						int64_t tmp = m_group->m_xCursorPos[1];
						m_group->m_xCursorPos[1] = m_group->m_xCursorPos[0];
						m_group->m_xCursorPos[0] = tmp;
					}
					break;

				case WaveformGroup::CURSOR_Y_DUAL:

					m_group->m_yCursorPos[1] = voltage;

					//Cursor 0 should always be above 1.
					//If they cross, flip them
					if(m_group->m_yCursorPos[0] < m_group->m_yCursorPos[1])
					{
						m_dragState = DRAG_CURSOR_0;
						int64_t tmp = m_group->m_yCursorPos[1];
						m_group->m_yCursorPos[1] = m_group->m_yCursorPos[0];
						m_group->m_yCursorPos[0] = tmp;
					}

					break;

				default:
					break;
			}

			OnCursorMoved();
			m_group->m_vbox.queue_draw();
			break;

		//Offset drag - update level and refresh
		case DRAG_OFFSET:
			{
				double dv = YPositionToYAxisUnits(event->y) - m_dragStartVoltage;
				double old_offset = m_channel.GetOffset();
				m_channel.SetOffset(old_offset + dv);
				ClearPersistence();
				SetGeometryDirty();
				queue_draw();
			}
			break;

		//Reorder the selected overlay.
		//TODO: allow overlays to be moved between waveform areas?
		case DRAG_OVERLAY:
			{
				//See what decoder we're above/below
				int maxpos = 0;
				StreamDescriptor maxover(NULL, 0);
				for(auto it : m_overlayPositions)
				{
					if(event->y > it.second)
					{
						if(it.second > maxpos)
						{
							maxpos = it.second;
							maxover = it.first;
						}
					}
				}

				if(maxover.m_channel != NULL)
				{
					m_dragOverlayPosition = maxpos + 10;
					queue_draw();
				}
				else
				{
					m_dragOverlayPosition = 0;
					queue_draw();
				}

			}
			break;

		//Move this waveform area to a new place
		case DRAG_WAVEFORM_AREA:
			{
				//If we're still over the infobox, don't register it as a drag
				if(m_mouseElementPosition == WaveformArea::LOC_CHAN_NAME)
				{
					m_insertionBarLocation = INSERT_NONE;
					queue_draw();
					break;
				}

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

					int target_x = real_x - alloc.get_x();
					int target_y = real_y - alloc.get_y();

					//Split if dragging to the right side of the group
					if(target_x > target->m_width * 3/4)
						target->m_insertionBarLocation = INSERT_RIGHT_SPLIT;

					//Split if dragging all the way to the edge of the bottom area in the group
					else if(target->m_group->IsLastChild(target) && (target_y > target->m_height*3/4) )
						target->m_insertionBarLocation = INSERT_BOTTOM_SPLIT;

					//No, just reorder
					else if(target_y > target->m_height/2)
						target->m_insertionBarLocation = INSERT_BOTTOM;
					else
						target->m_insertionBarLocation = INSERT_TOP;

					target->queue_draw();
				}
			}
			break;

		//Not dragging. Update mouse cursor based on where we are
		case DRAG_NONE:
			if(oldLocation != m_mouseElementPosition)
			{
				//New cursor shape!
				string shape = "default";
				switch(m_mouseElementPosition)
				{
					case LOC_XCURSOR_0:
					case LOC_XCURSOR_1:
					case LOC_MARKER:
						shape = "ew-resize";
						break;

					case LOC_VSCALE:
						shape = "grab";
						break;

					case LOC_TRIGGER:
					case LOC_TRIGGER_SECONDARY:
					case LOC_YCURSOR_0:
					case LOC_YCURSOR_1:
						shape = "ns-resize";
						break;

					case LOC_CHAN_NAME:
						shape = "default";
						break;

					case LOC_PLOT:
						shape = "crosshair";
						break;

					default:
						break;
				}

				//Create the cursor
				get_window()->set_cursor(Gdk::Cursor::create(get_display(), shape));
			}
			break;

		//Nothing to do
		default:
			break;
	}

	return false;
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
	//After we call OnRemoveChannel(this) we're going to get deleted and must return immediately,
	//without touching any other member variables.
	if(m_selectedChannel == m_channel)
	{
		m_parent->OnRemoveChannel(this);
		return;
	}

	//Deleting an overlay
	else
	{
		//LogDebug("Deleting overlay %s\n", m_selectedChannel->GetDisplayName().c_str());

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

	CalculateOverlayPositions();
	ClearPersistence(false);
	queue_draw();
}

void WaveformArea::OnProtocolDecode(string name, bool forceStats)
{
	m_showPendingDecodeAsStats = forceStats;

	//Create a new decoder for the incoming signal
	string color = GetDefaultChannelColor(g_numDecodes);
	if(m_pendingDecode)
		delete m_pendingDecode;
	m_pendingDecode = Filter::CreateFilter(name, color);

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

		m_decodeDialog = new FilterDialog(m_parent, m_pendingDecode, m_selectedChannel);
		m_decodeDialog->show();
		m_decodeDialog->signal_delete_event().connect(sigc::mem_fun(*this, &WaveformArea::OnDecodeDialogClosed));

		g_numDecodes ++;
		OnDecodeSetupComplete();
	}
}

bool WaveformArea::OnDecodeDialogClosed(GdkEventAny* /*ignored*/)
{
	m_decodeDialog->ConfigureDecoder();

	//Refresh the channels menu with the new channel name etc
	m_parent->RefreshChannelsMenu();
	m_parent->RefreshFilterGraphEditor();

	//Clean up the dialog
	delete m_decodeDialog;
	m_decodeDialog = NULL;

	return false;
}

bool WaveformArea::OnDecodeReconfigureDialogClosed(GdkEventAny* /*ignored*/)
{
	auto decode = m_decodeDialog->GetFilter();
	auto name = decode->GetDisplayName();

	m_decodeDialog->ConfigureDecoder();

	if(name != decode->GetDisplayName())
		m_parent->OnChannelRenamed(decode);

	m_parent->OnAllWaveformsUpdated(true);

	m_parent->RefreshFilterGraphEditor();

	//Clean up the dialog
	delete m_decodeDialog;
	m_decodeDialog = NULL;

	return false;
}

void WaveformArea::OnDecodeSetupComplete()
{
	//If it's an eye pattern or waterfall, set the initial size
	auto eye = dynamic_cast<EyePattern*>(m_pendingDecode);
	if(eye != NULL)
	{
		eye->SetWidth(m_width);
		eye->SetHeight(m_height);
	}
	auto fall = dynamic_cast<Waterfall*>(m_pendingDecode);
	if(fall != NULL)
	{
		fall->SetWidth(m_width);
		fall->SetHeight(m_height);
		fall->SetTimeScale(m_group->m_pixelsPerXUnit);
	}

	//Run the decoder for the first time, so we get valid output even if there's not a trigger pending.
	m_pendingDecode->Refresh();

	//If the pending filter is a scalar output, add a statistic instead.
	//Also do this if requested from the measurement menu
	if(m_pendingDecode->IsScalarOutput() || m_showPendingDecodeAsStats)
		m_group->EnableStats(StreamDescriptor(m_pendingDecode, 0));

	//Vector output
	else
	{
		for(size_t i=0; i<m_pendingDecode->GetStreamCount(); i++)
		{
			//It's an overlay. Reference it and add to our overlay list
			if(	(m_pendingDecode->GetType(i) == Stream::STREAM_TYPE_DIGITAL) ||
				(m_pendingDecode->GetType(i) == Stream::STREAM_TYPE_PROTOCOL) )
			{
				m_pendingDecode->AddRef();
				m_overlays.push_back(StreamDescriptor(m_pendingDecode, i));
			}

			//Create a new waveform view for the generated analog signal
			else
			{
				auto area = m_parent->DoAddChannel(StreamDescriptor(m_pendingDecode, i), m_group, this);

				//If the decode is incompatible with our timebase, make a new group if needed
				//TODO: better way to determine fixed-width stuff like eye patterns
				if(eye || (m_pendingDecode->GetXAxisUnits() != m_channel.m_channel->GetXAxisUnits()) )
				{
					m_parent->MoveToBestGroup(area);

					//If the new unit is Hz, use a reasonable default for the timebase (1 MHz/pixel)
					if(m_pendingDecode->GetXAxisUnits() == Unit(Unit::UNIT_HZ))
						area->m_group->m_pixelsPerXUnit = 1e-6;
				}
			}
		}
	}

	//If the decoder is a packet-oriented protocol, pop up a protocol analyzer
	auto pdecode = dynamic_cast<PacketDecoder*>(m_pendingDecode);
	if(pdecode != NULL)
	{
		char title[256];
		snprintf(title, sizeof(title), "Protocol Analyzer: %s", m_pendingDecode->GetDisplayName().c_str());

		auto analyzer = new ProtocolAnalyzerWindow(title, m_parent, pdecode, this);
		m_parent->m_analyzers.emplace(analyzer);
		m_parent->RefreshAnalyzerMenu();

		analyzer->OnWaveformDataReady();
		analyzer->show();
	}

	//This decode is no longer pending
	m_pendingDecode = NULL;

	SetGeometryDirty();
	queue_draw();
}

void WaveformArea::OnCoupling(OscilloscopeChannel::CouplingType type, Gtk::RadioMenuItem* item)
{
	//ignore spurious events while loading menu config, or from item being deselected
	if(m_updatingContextMenu || !item->get_active())
		return;

	m_selectedChannel.m_channel->SetCoupling(type);
	ClearPersistence();

	//TODO: we should also clear persistence for any other channels derived from this
}

void WaveformArea::OnWaveformDataReady()
{
	//If we're a fixed width curve, refresh the parent's time scale
	if(IsEyeOrBathtub())
	{
		auto eye = dynamic_cast<EyeWaveform*>(m_channel.m_channel->GetData(0));
		auto f = dynamic_cast<Filter*>(m_channel.m_channel);
		if(eye == NULL)
			eye = dynamic_cast<EyeWaveform*>(f->GetInput(0).m_channel->GetData(0));
		if(eye != NULL)
			RescaleEye(f, eye);
	}
}

void WaveformArea::RescaleEye(Filter* f, EyeWaveform* /*eye*/)
{
	auto d = dynamic_cast<EyePattern*>(f);
	if(!d)
		return;

	m_group->m_pixelsPerXUnit = d->GetXScale();
	m_group->m_xAxisOffset = d->GetXOffset();

	//TODO: only if stuff changed
	//TODO: clear sweeps if this happens?
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

	//Right side of plot area
	if(x > m_plotRight)
	{
		//On the trigger button?
		auto scope = m_channel.m_channel->GetScope();
		if(scope != NULL)
		{
			auto trig = scope->GetTrigger();
			if( (trig != NULL) && (m_channel == trig->GetInput(0)) )
			{
				float vy = YAxisUnitsToYPosition(trig->GetLevel());
				float radius = 20;
				if(x < (m_plotRight + radius) )
				{
					//If on top of the trigger, obviously we're a hit
					if(fabs(y - vy) < radius)
						return LOC_TRIGGER;

					//but also check the edges of the plot if trigger is off scale
					if( (vy > m_height) && (fabs(m_height - y) < radius) )
						return LOC_TRIGGER;
					if( (vy < 0) && (y < radius) )
						return LOC_TRIGGER;
				}

				//Check if it's a two-level trigger (second arrow)
				auto wt = dynamic_cast<TwoLevelTrigger*>(trig);
				if(wt)
				{
					vy = YAxisUnitsToYPosition(wt->GetLowerBound());
					if(x < (m_plotRight + radius) )
					{
						//If on top of the trigger, obviously we're a hit
						if(fabs(y - vy) < radius)
							return LOC_TRIGGER_SECONDARY;

						//but also check the edges of the plot if trigger is off scale
						if( (vy > m_height) && (fabs(m_height - y) < radius) )
							return LOC_TRIGGER_SECONDARY;
						if( (vy < 0) && (y < radius) )
							return LOC_TRIGGER_SECONDARY;
					}
				}
			}
		}

		//Nope, just the scale bar
		return LOC_VSCALE;
	}

	//In the plot area. Check for hits on cursors
	float currentCursorX0Pos = XAxisUnitsToXPosition(m_group->m_xCursorPos[0]);
	float currentCursorX1Pos = XAxisUnitsToXPosition(m_group->m_xCursorPos[1]);
	float currentCursorY0Pos = YAxisUnitsToYPosition(m_group->m_yCursorPos[0]);
	float currentCursorY1Pos = YAxisUnitsToYPosition(m_group->m_yCursorPos[1]);
	switch(m_group->m_cursorConfig)
	{
		//Only check cursor 1 if enabled
		case WaveformGroup::CURSOR_X_DUAL:
			if(fabs(currentCursorX1Pos - x) < 5)
				return LOC_XCURSOR_1;

		//fall through
		//Check cursor 0 regardless
		case WaveformGroup::CURSOR_X_SINGLE:
			if(fabs(currentCursorX0Pos - x) < 5)
				return LOC_XCURSOR_0;
			break;

		//Only check cursor 1 if enabled
		case WaveformGroup::CURSOR_Y_DUAL:
			if(fabs(currentCursorY1Pos - y) < 5)
				return LOC_YCURSOR_1;

		//fall through
		//Check cursor 0 regardless
		case WaveformGroup::CURSOR_Y_SINGLE:
			if(fabs(currentCursorY0Pos - y) < 5)
				return LOC_YCURSOR_0;
			break;

		default:
			break;
	}

	//Check markers if not on a cursor
	auto markers = GetMarkersForActiveWaveform();
	for(auto m : markers)
	{
		float mpos = XAxisUnitsToXPosition(m->m_offset);
		if(fabs(mpos - x) < 5)
		{
			m_selectedMarker = m;
			return LOC_MARKER;
		}
	}

	return LOC_PLOT;
}

vector<Marker*> WaveformArea::GetMarkersForActiveWaveform()
{
	vector<Marker*> empty;

	//If we're a filter, use markers from the first waveform in the group
	WaveformBase* w = m_channel.GetData();
	if((!w) || (dynamic_cast<Filter*>(m_channel.m_channel) != nullptr ) )
		w = m_group->GetFirstChannel().GetData();
	if(!w)
		return empty;

	//Look up the current waveform's timestamp
	TimePoint t(w->m_startTimestamp, w->m_startFemtoseconds);
	if(m_parent->m_markers.find(t) == m_parent->m_markers.end())
		return empty;
	return m_parent->m_markers[t];
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
		item->set_label(g->m_framelabel.get_label());
		m_moveMenu.append(*item);
		m_moveExistingGroupItems.emplace(item);
		if(get_parent() == &g->m_waveformBox)
			item->set_sensitive(false);
		item->signal_activate().connect(sigc::bind<WaveformGroup*>(
			sigc::mem_fun(*this, &WaveformArea::OnMoveToExistingGroup), g));

		//Copy
		item = new Gtk::MenuItem;
		item->set_label(g->m_framelabel.get_label());
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
	childMenus.push_back(&m_decodePowerMenu);
	childMenus.push_back(&m_decodeRFMenu);
	childMenus.push_back(&m_decodeSerialMenu);
	childMenus.push_back(&m_decodeWaveformGenerationMenu);

	for(auto submenu : childMenus)
	{
		auto subchildren = submenu->get_children();
		for(auto item : subchildren)
		{
			Gtk::MenuItem* menu = dynamic_cast<Gtk::MenuItem*>(item);
			if(menu == NULL)
				continue;

			//Need to synchronize with OscilloscopeWindow::RefreshAllFilters().
			//Otherwise there's a chance that this filter will be added to the working set and updated
			//after we've deleted it.
			lock_guard<mutex> lock(m_parent->m_filterUpdatingMutex);
			auto filter = Filter::CreateFilter(
				menu->get_label(),
				"");
			if(filter->GetInputCount() == 0)
				menu->set_sensitive();	//filters with no inputs always are legal
			else
				menu->set_sensitive(filter->ValidateChannel(0, m_selectedChannel));
			delete filter;
		}
	}

	if(m_selectedChannel.m_channel->IsPhysicalChannel())
	{
		//See what couplings are available
		auto couplings = m_selectedChannel.m_channel->GetAvailableCouplings();
		if(couplings.size() > 1)
		{
			m_couplingItem.set_sensitive(true);

			//Show only the legal coupling options
			m_dc1MCouplingItem.set_visible(false);
			m_ac1MCouplingItem.set_visible(false);
			m_dc50CouplingItem.set_visible(false);
			m_ac50CouplingItem.set_visible(false);
			m_gndCouplingItem.set_visible(false);
			for(auto c : couplings)
			{
				switch(c)
				{
					case OscilloscopeChannel::COUPLE_DC_1M:
						m_dc1MCouplingItem.set_visible(true);
						break;

					case OscilloscopeChannel::COUPLE_AC_1M:
						m_ac1MCouplingItem.set_visible(true);
						break;

					case OscilloscopeChannel::COUPLE_DC_50:
						m_dc50CouplingItem.set_visible(true);
						break;

					case OscilloscopeChannel::COUPLE_AC_50:
						m_ac50CouplingItem.set_visible(true);
						break;

					case OscilloscopeChannel::COUPLE_GND:
						m_gndCouplingItem.set_visible(true);
						break;

					//synthetic not selectable
					default:
						break;
				}
			}

			//Update the current coupling setting
			auto coupling = m_selectedChannel.m_channel->GetCoupling();
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

				case OscilloscopeChannel::COUPLE_AC_50:
					m_ac50CouplingItem.set_active(true);
					break;

				case OscilloscopeChannel::COUPLE_GND:
					m_gndCouplingItem.set_active(true);
					break;

				//coupling not possible, it's not an analog channel
				default:
					m_couplingItem.set_sensitive(false);
					break;
			}

		}

		//Only one coupling option, don't bother showing the menu
		else
			m_couplingItem.set_sensitive(false);
	}
	else
		m_couplingItem.set_sensitive(false);

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

		case WaveformGroup::CURSOR_Y_SINGLE:
			m_cursorSingleHorizontalItem.set_active(true);
			break;

		case WaveformGroup::CURSOR_Y_DUAL:
			m_cursorDualHorizontalItem.set_active(true);
			break;

		default:
			break;
	}

	//Hide stats for non-analog channels
	if(m_selectedChannel.GetType() != Stream::STREAM_TYPE_ANALOG)
	{
		m_statisticsItem.set_sensitive(false);
		m_statisticsItem.set_active(false);
	}
	else
	{
		m_statisticsItem.set_sensitive(true);
		m_statisticsItem.set_active(m_group->IsShowingStats(m_selectedChannel.m_channel));
	}

	m_updatingContextMenu = false;
}

void WaveformArea::OnStatistics()
{
	if(m_updatingContextMenu)
		return;

	if(m_statisticsItem.get_active())
		m_group->EnableStats(m_selectedChannel);
	else
		m_group->DisableStats(m_selectedChannel);
}

void WaveformArea::CenterMarker(int64_t time)
{
	//Figure out the width of the view, in time units
	int64_t width = PixelsToXAxisUnits(m_width);

	//Center the view
	m_group->m_xAxisOffset = time - width/2;

	//Redraw everything
	m_parent->ClearPersistence(m_group, false, true);
}

void WaveformArea::CenterPacket(int64_t time, int64_t len)
{
	//Figure out the width of the view, in time units
	int64_t width = PixelsToXAxisUnits(m_width);

	//If the packet is too long to fit on screen at the current zoom, have it start 10% of the way across
	if(len > width)
		m_group->m_xAxisOffset = time - width*0.1;

	//If the entire packet fits, center it
	else
		m_group->m_xAxisOffset = time - width/2 + len/2;

	//Redraw everything
	m_parent->ClearPersistence(m_group, false, true);

	//If we have a single X cursor, move it to this point too
	//TODO: preference to enable/disable this?
	if(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_SINGLE)
		m_group->m_xCursorPos[0] = time;
}

void WaveformArea::SyncFontPreferences()
{
	m_axisLabelFont = m_parent->GetPreferences().GetFont("Appearance.Waveforms.y_axis_font");
	m_infoBoxFont = m_parent->GetPreferences().GetFont("Appearance.Waveforms.infobox_font");
	m_cursorLabelFont = m_parent->GetPreferences().GetFont("Appearance.Cursors.label_font");
	m_decodeFont = m_parent->GetPreferences().GetFont("Appearance.Decodes.protocol_font");
}

/**
	@brief Update stuff when a cursor is dragged
 */
void WaveformArea::OnCursorMoved(bool notifySiblings)
{
	//Single cursor - find the timestamp of the cursor and see if it it hit a packet for a protocol analyzer
	if(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_SINGLE)
	{
		for(auto d : m_overlays)
		{
			auto p = dynamic_cast<PacketDecoder*>(d.m_channel);
			if(!p)
				continue;

			HighlightPacketAtTime(p, m_group->m_xCursorPos[0]);
		}

		//Notify all other waveform groups that the cursor moved
		if(notifySiblings)
		{
			for(auto a : m_parent->m_waveformAreas)
			{
				if(a == this)
					continue;
				if(a->m_group != m_group)
					continue;

				a->OnCursorMoved(false);
			}
		}
	}
}

/**
	@brief Update stuff when a marker is moved
 */
void WaveformArea::OnMarkerMoved(bool notifySiblings)
{
	//Notify all other waveform groups that the cursor moved
	if(notifySiblings)
	{
		for(auto a : m_parent->m_waveformAreas)
		{
			if(a == this)
				continue;
			if(a->m_group != m_group)
				continue;

			a->OnMarkerMoved(false);
		}

		m_parent->OnMarkerMoved(m_selectedMarker);
	}
}

void WaveformArea::HighlightPacketAtTime(PacketDecoder* p, int64_t time)
{
	//Find the protocol analyzer window
	ProtocolAnalyzerWindow* a = NULL;
	for(auto w : m_parent->m_analyzers)
	{
		if(w->GetDecoder() == p)
		{
			a = w;
			break;
		}
	}
	if(!a)
		return;

	//If it's not visible, don't bother
	if(!a->is_visible())
		return;

	bool hit = false;

	TimePoint packetTimestamp;
	int64_t packetOffset = 0;

	//TODO: binary search or something more efficient
	auto packets = p->GetPackets();
	for(size_t i=0; i<packets.size(); i++)
	{
		auto pack = packets[i];
		auto end = pack->m_offset + pack->m_len;

		//Too early?
		if(end < time)
			continue;

		//Too late?
		if(pack->m_offset > time)
			break;

		hit = true;
		auto data = p->GetData(0);
		packetTimestamp = TimePoint(data->m_startTimestamp, data->m_startFemtoseconds);
		packetOffset = pack->m_offset;
	}

	if(!hit)
		return;

	//Hit?
	a->SelectPacket(packetTimestamp, packetOffset);
}

/**
	@brief Find the waveform that a given point is under
 */
StreamDescriptor WaveformArea::GetWaveformAtPoint(int x, int y)
{
	//Window coordinates of our cursor
	int window_x;
	int window_y;
	get_window()->get_origin(window_x, window_y);
	auto alloc = get_allocation();
	int real_x = x + alloc.get_x() + window_x;
	int real_y = y + alloc.get_y() + window_y;
	Gdk::Rectangle rect(real_x, real_y, 1, 1);

	//Check all waveform areas to see which one we hit
	WaveformArea* target = NULL;
	int target_y = y;
	for(auto w : m_parent->m_waveformAreas)
	{
		int wx;
		int wy;
		w->get_window()->get_origin(wx, wy);
		auto trect = w->get_allocation();
		trect.set_x(trect.get_x() + wx);
		trect.set_y(trect.get_y() + wy);

		if(trect.intersects(rect))
		{
			target = w;
			target_y = real_y - trect.get_y();
		}
	}
	if(!target)
		return m_channel;

	//Figure out what channel is under the cursor
	StreamDescriptor targetChan = target->m_channel;
	int height = 20 * GetDPIScale();
	for(auto it : target->m_overlayPositions)
	{
		int dy = target_y - it.second;
		if(abs(dy) < height/2)
		{
			targetChan = it.first;
			break;
		}
	}
	return targetChan;
}

/**
	@brief Snap a timestamp to the closest edge in the digital waveform or decode (if applicable)
	under the cursor.
 */
int64_t WaveformArea::SnapX(int64_t time, int x, int y)
{
	auto stream = GetWaveformAtPoint(x, y);
	auto data = stream.GetData();

	//We need to have non-empty data
	if(!data)
		return time;
	if(data->empty())
		return time;

	//Only snap to digital/protocol data
	if( (stream.GetType() != Stream::STREAM_TYPE_DIGITAL) &&
		(stream.GetType() != Stream::STREAM_TYPE_PROTOCOL) )
		return time;

	auto swfm = dynamic_cast<SparseWaveformBase*>(data);
	auto uwfm = dynamic_cast<UniformWaveformBase*>(data);
	data->PrepareForCpuAccess();

	//Convert timestamp to waveform timebase units
	int64_t wtime = (time - data->m_triggerPhase) / data->m_timescale;

	//Binary search to find the closest start point to the target timestamp.
	size_t ibest;
	if(swfm)
		ibest = BinarySearchForGequal(swfm->m_offsets.GetCpuPointer(), data->size(), wtime);
	else
		ibest = wtime;

	//Start a little bit before there and check both start and end points
	size_t base = ibest;
	if(base > 0)
		base --;
	int64_t wtime_snapped = wtime;
	int64_t best_delta = wtime;
	for(size_t i=0; i<2; i++)
	{
		int64_t start = GetOffset(swfm, uwfm, i+base);
		int64_t end = start + GetDuration(swfm, uwfm, i+base);

		int64_t delta = llabs(start - wtime);
		if(delta < best_delta)
		{
			best_delta = delta;
			wtime_snapped = start;
		}

		delta = llabs(end - wtime);
		if(delta < best_delta)
		{
			best_delta = delta;
			wtime_snapped = end;
		}
	}

	//Convert delta (im timebase units) back to pixels and see how close it is
	int64_t delta_pixels = (best_delta * data->m_timescale) * m_group->m_pixelsPerXUnit;
	const int snap_threshold = 5 * GetDPIScale();
	if(llabs(delta_pixels) < snap_threshold)
		return (wtime_snapped * data->m_timescale) + data->m_triggerPhase;

	//Too far away, don't snap
	else
		return time;
}

void WaveformArea::OnMarkerAdd()
{
	//Figure out exactly where we clicked
	auto stream = GetWaveformAtPoint(m_cursorX, m_cursorY);
	auto data = stream.GetData();
	if(!data)
		return;
	auto tpoint = TimePoint(data->m_startTimestamp, data->m_startFemtoseconds);
	int64_t timestamp = XPositionToXAxisUnits(m_cursorX);

	//Add the marker there
	m_parent->AddMarker(tpoint, timestamp);
}
