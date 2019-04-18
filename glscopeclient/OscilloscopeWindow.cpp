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
	@brief Implementation of main application window class
 */

#include "glscopeclient.h"
#include "../scopehal/Instrument.h"
#include "OscilloscopeWindow.h"
//#include "../scopehal/AnalogRenderer.h"
//#include "ProtocolDecoderDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

/**
	@brief Initializes the main window
 */
OscilloscopeWindow::OscilloscopeWindow(vector<Oscilloscope*> scopes)
	: m_historyWindow(this)
	, m_scopes(scopes)
	// m_iconTheme(Gtk::IconTheme::get_default())
{
	//Set title
	string title = "Oscilloscope: ";
	for(size_t i=0; i<scopes.size(); i++)
	{
		auto scope = scopes[i];

		char tt[256];
		snprintf(tt, sizeof(tt), "%s (%s %s, serial %s)",
			scope->m_nickname.c_str(),
			scope->GetVendor().c_str(),
			scope->GetName().c_str(),
			scope->GetSerial().c_str()
			);

		if(i > 0)
			title += ", ";
		title += tt;
	}
	set_title(title);

	//Initial setup
	set_reallocate_redraws(true);
	set_default_size(1280, 800);

	//Add widgets
	CreateWidgets();

	//Set the update timer (50 Hz)
	sigc::slot<bool> slot = sigc::bind(sigc::mem_fun(*this, &OscilloscopeWindow::OnTimer), 1);
	sigc::connection conn = Glib::signal_timeout().connect(slot, 20);

	ArmTrigger(false);
	m_toggleInProgress = false;

	m_tLastFlush = GetTime();

	m_eyeColor = EYE_CRT;
}

/**
	@brief Application cleanup
 */
OscilloscopeWindow::~OscilloscopeWindow()
{
	for(auto a : m_analyzers)
		delete a;
	for(auto s : m_splitters)
		delete s;
	for(auto g : m_waveformGroups)
		delete g;
	for(auto w : m_waveformAreas)
		delete w;

	//decoders should self-delete when the last reference to them is removed
	//for(auto d : m_decoders)
	//	delete d;
}

/**
	@brief Helper function for creating widgets and setting up signal handlers
 */
void OscilloscopeWindow::CreateWidgets()
{
	//Set up window hierarchy
	add(m_vbox);
		m_vbox.pack_start(m_menu, Gtk::PACK_SHRINK);
			m_menu.append(m_fileMenuItem);
				m_fileMenuItem.set_label("File");
				m_fileMenuItem.set_submenu(m_fileMenu);
					auto item = Gtk::manage(new Gtk::MenuItem("Quit", false));
					item->signal_activate().connect(
						sigc::mem_fun(*this, &OscilloscopeWindow::OnQuit));
					m_fileMenu.append(*item);
			m_menu.append(m_viewMenuItem);
				m_viewMenuItem.set_label("View");
				m_viewMenuItem.set_submenu(m_viewMenu);
					m_viewMenu.append(m_viewEyeColorMenuItem);
					m_viewEyeColorMenuItem.set_label("Eye color");
					m_viewEyeColorMenuItem.set_submenu(m_viewEyeColorMenu);
						m_viewEyeColorMenu.append(m_eyeColorCrtItem);
							m_eyeColorCrtItem.set_label("CRT");
							m_eyeColorCrtItem.set_group(m_eyeColorGroup);
							m_eyeColorCrtItem.signal_activate().connect(
								sigc::bind<OscilloscopeWindow::EyeColor, Gtk::RadioMenuItem*>(
									sigc::mem_fun(*this, &OscilloscopeWindow::OnEyeColorChanged),
									OscilloscopeWindow::EYE_CRT,
									&m_eyeColorCrtItem));
						m_viewEyeColorMenu.append(m_eyeColorGrayscaleItem);
							m_eyeColorGrayscaleItem.set_label("Grayscale");
							m_eyeColorGrayscaleItem.set_group(m_eyeColorGroup);
							m_eyeColorGrayscaleItem.signal_activate().connect(
								sigc::bind<OscilloscopeWindow::EyeColor, Gtk::RadioMenuItem*>(
									sigc::mem_fun(*this, &OscilloscopeWindow::OnEyeColorChanged),
									OscilloscopeWindow::EYE_GRAYSCALE,
									&m_eyeColorGrayscaleItem));
						m_viewEyeColorMenu.append(m_eyeColorIronbowItem);
							m_eyeColorIronbowItem.set_label("Ironbow");
							m_eyeColorIronbowItem.set_group(m_eyeColorGroup);
							m_eyeColorIronbowItem.signal_activate().connect(
								sigc::bind<OscilloscopeWindow::EyeColor, Gtk::RadioMenuItem*>(
									sigc::mem_fun(*this, &OscilloscopeWindow::OnEyeColorChanged),
									OscilloscopeWindow::EYE_IRONBOW,
									&m_eyeColorIronbowItem));
						m_viewEyeColorMenu.append(m_eyeColorKRainItem);
							m_eyeColorKRainItem.set_label("KRain");
							m_eyeColorKRainItem.set_group(m_eyeColorGroup);
							m_eyeColorKRainItem.signal_activate().connect(
								sigc::bind<OscilloscopeWindow::EyeColor, Gtk::RadioMenuItem*>(
									sigc::mem_fun(*this, &OscilloscopeWindow::OnEyeColorChanged),
									OscilloscopeWindow::EYE_KRAIN,
									&m_eyeColorKRainItem));
						m_viewEyeColorMenu.append(m_eyeColorRainbowItem);
							m_eyeColorRainbowItem.set_label("Rainbow");
							m_eyeColorRainbowItem.set_group(m_eyeColorGroup);
							m_eyeColorRainbowItem.signal_activate().connect(
								sigc::bind<OscilloscopeWindow::EyeColor, Gtk::RadioMenuItem*>(
									sigc::mem_fun(*this, &OscilloscopeWindow::OnEyeColorChanged),
									OscilloscopeWindow::EYE_RAINBOW,
									&m_eyeColorRainbowItem));
						m_viewEyeColorMenu.append(m_eyeColorViridisItem);
							m_eyeColorViridisItem.set_label("Viridis");
							m_eyeColorViridisItem.set_group(m_eyeColorGroup);
							m_eyeColorViridisItem.signal_activate().connect(
								sigc::bind<OscilloscopeWindow::EyeColor, Gtk::RadioMenuItem*>(
									sigc::mem_fun(*this, &OscilloscopeWindow::OnEyeColorChanged),
									OscilloscopeWindow::EYE_VIRIDIS,
									&m_eyeColorViridisItem));
			m_menu.append(m_channelsMenuItem);
				m_channelsMenuItem.set_label("Add");
				m_channelsMenuItem.set_submenu(m_channelsMenu);
		m_vbox.pack_start(m_toolbar, Gtk::PACK_SHRINK);
			m_toolbar.append(m_btnStart, sigc::mem_fun(*this, &OscilloscopeWindow::OnStart));
				m_btnStart.set_tooltip_text("Start (normal trigger)");
				m_btnStart.set_icon_name("media-playback-start");
				m_btnStart.set_sensitive(false);
			m_toolbar.append(m_btnStartSingle, sigc::mem_fun(*this, &OscilloscopeWindow::OnStartSingle));
				m_btnStartSingle.set_tooltip_text("Start (single trigger)");
				m_btnStartSingle.set_icon_name("media-skip-forward");
				m_btnStartSingle.set_sensitive(false);
			m_toolbar.append(m_btnStop, sigc::mem_fun(*this, &OscilloscopeWindow::OnStop));
				m_btnStop.set_tooltip_text("Stop trigger");
				m_btnStop.set_icon_name("media-playback-stop");
			m_toolbar.append(*Gtk::manage(new Gtk::SeparatorToolItem));
			m_toolbar.append(m_btnHistory, sigc::mem_fun(*this, &OscilloscopeWindow::OnHistory));
				m_btnHistory.set_tooltip_text("History");
				m_btnHistory.set_icon_name("search");

		auto split = new Gtk::HPaned;
			m_vbox.pack_start(*split);
			m_splitters.emplace(split);
			auto group = new WaveformGroup;
			m_waveformGroups.emplace(group);
			split->pack1(group->m_frame);

		m_vbox.pack_start(m_statusbar, Gtk::PACK_SHRINK);
		m_statusbar.pack_end(m_sampleRateLabel, Gtk::PACK_SHRINK);
		m_sampleRateLabel.set_size_request(75, 1);
		m_statusbar.pack_end(m_sampleCountLabel, Gtk::PACK_SHRINK);
		m_sampleCountLabel.set_size_request(75, 1);
		m_statusbar.pack_end(m_triggerConfigLabel, Gtk::PACK_SHRINK);
		m_triggerConfigLabel.set_size_request(75, 1);

	//Process all of the channels
	for(auto scope : m_scopes)
	{
		for(size_t i=0; i<scope->GetChannelCount(); i++)
		{
			auto chan = scope->GetChannel(i);

			//Qualify the channel name by the scope name if we have >1 scope enabled
			if(m_scopes.size() > 1)
			{
				char tmp[128];
				snprintf(tmp, sizeof(tmp), "%s:%s", scope->m_nickname.c_str(), chan->GetHwname().c_str());
				chan->m_displayname = tmp;
			}

			//Add a menu item - but not for the external trigger(s)
			if(chan->GetType() != OscilloscopeChannel::CHANNEL_TYPE_TRIGGER)
			{
				auto item = Gtk::manage(new Gtk::MenuItem(chan->m_displayname, false));
				item->signal_activate().connect(
					sigc::bind<OscilloscopeChannel*>(sigc::mem_fun(*this, &OscilloscopeWindow::OnAddChannel), chan));
				m_channelsMenu.append(*item);
			}

			//See which channels are currently on
			//DEBUG: enable all analog channels to save time when setting up the client
			//if(chan->IsEnabled())
			if(chan->GetType() == OscilloscopeChannel::CHANNEL_TYPE_ANALOG)
			{
				auto w = new WaveformArea(
					scope,
					chan,
					this);
				w->m_group = group;
				m_waveformAreas.emplace(w);
				group->m_waveformBox.pack_start(*w);
			}
		}
	}

	m_channelsMenu.show_all();

	m_historyWindow.hide();

	//Done adding widgets
	show_all();

	//Don't show measurements by default
	group->m_measurementFrame.hide();

	//Initialize the style sheets
	m_css = Gtk::CssProvider::create();
	m_css->load_from_path("styles/glscopeclient.css");
	get_style_context()->add_provider_for_screen(
		Gdk::Screen::get_default(),
		m_css,
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Message handlers

void OscilloscopeWindow::OnEyeColorChanged(EyeColor color, Gtk::RadioMenuItem* item)
{
	if(!item->get_active())
		return;

	m_eyeColor = color;
	for(auto v : m_waveformAreas)
		v->queue_draw();
}

OscilloscopeWindow::EyeColor OscilloscopeWindow::GetEyeColor()
{
	return m_eyeColor;
}

void OscilloscopeWindow::OnHistory()
{
	if(m_btnHistory.get_active())
		m_historyWindow.show();
	else
		m_historyWindow.hide();
}

void OscilloscopeWindow::OnMoveNewRight(WaveformArea* w)
{
	OnMoveNew(w, true);
}

void OscilloscopeWindow::OnMoveNewBelow(WaveformArea* w)
{
	OnMoveNew(w, false);
}

void OscilloscopeWindow::SplitGroup(Gtk::Widget* frame, WaveformGroup* group, bool horizontal)
{
	//Hierarchy is WaveformArea -> WaveformGroup waveform box -> WaveformGroup box -> WaveformGroup frame -> splitter
	auto split = dynamic_cast<Gtk::Paned*>(frame->get_parent());
	if(split == NULL)
	{
		LogError("parent isn't a splitter\n");
		return;
	}

	//See what the widget's current parenting situation is.
	//We might have a free splitter area free already!
	Gtk::Paned* csplit = NULL;
	if(horizontal)
		csplit = dynamic_cast<Gtk::HPaned*>(split);
	else
		csplit = dynamic_cast<Gtk::VPaned*>(split);
	if( (csplit != NULL) && (split->get_child2() == NULL) )
	{
		split->pack2(group->m_frame);
		split->show_all();
	}

	//Split the current parent
	else
	{
		//Create a new splitter
		Gtk::Paned* nsplit;
		if(horizontal)
			nsplit = new Gtk::HPaned;
		else
			nsplit = new Gtk::VPaned;
		m_splitters.emplace(nsplit);

		//Take the current frame out of the parent group so we have room for the splitter
		if(frame == split->get_child1())
		{
			split->remove(*frame);
			split->pack1(*nsplit);
		}
		else
		{
			split->remove(*frame);
			split->pack2(*nsplit);
		}

		nsplit->pack1(*frame);
		nsplit->pack2(group->m_frame);
		split->show_all();
	}
}

void OscilloscopeWindow::OnMoveNew(WaveformArea* w, bool horizontal)
{
	//Make a new group
	auto group = new WaveformGroup;
	group->m_pixelsPerPicosecond = w->m_group->m_pixelsPerPicosecond;
	m_waveformGroups.emplace(group);

	//Split the existing group and add the new group to it
	SplitGroup(w->get_parent()->get_parent()->get_parent(), group, horizontal);

	//Move the waveform into the new group
	OnMoveToExistingGroup(w, group);
}

void OscilloscopeWindow::OnCopyNew(WaveformArea* w, bool horizontal)
{
	//Make a new group
	auto group = new WaveformGroup;
	group->m_pixelsPerPicosecond = w->m_group->m_pixelsPerPicosecond;
	m_waveformGroups.emplace(group);

	//Split the existing group and add the new group to it
	SplitGroup(w->get_parent()->get_parent()->get_parent(), group, horizontal);

	//Make a copy of the current waveform view and add to that group
	OnCopyToExistingGroup(w, group);
}

void OscilloscopeWindow::OnMoveToExistingGroup(WaveformArea* w, WaveformGroup* ngroup)
{
	w->m_group = ngroup;
	w->get_parent()->remove(*w);
	ngroup->m_waveformBox.pack_start(*w);

	//TODO: move any measurements related to this trace to the new group?

	//Remove any groups that no longer have any waveform views in them,'
	//or splitters that only have one child
	GarbageCollectGroups();
}

void OscilloscopeWindow::OnCopyNewRight(WaveformArea* w)
{
	OnCopyNew(w, true);
}

void OscilloscopeWindow::OnCopyNewBelow(WaveformArea* w)
{
	OnCopyNew(w, false);
}

void OscilloscopeWindow::OnCopyToExistingGroup(WaveformArea* w, WaveformGroup* ngroup)
{
	//Create a new waveform area that looks like the existing one (not an exact copy)
	WaveformArea* nw = new WaveformArea(w);
	m_waveformAreas.emplace(nw);

	//Then add it like normal
	nw->m_group = ngroup;
	ngroup->m_waveformBox.pack_start(*nw);
	nw->show();
}

void OscilloscopeWindow::GarbageCollectGroups()
{
	//Remove groups with no waveforms (any attached measurements will be deleted)
	std::set<WaveformGroup*> groupsToRemove;
	for(auto g : m_waveformGroups)
	{
		if(g->m_waveformBox.get_children().empty())
			groupsToRemove.emplace(g);
	}
	for(auto g : groupsToRemove)
	{
		auto parent = g->m_frame.get_parent();
		parent->remove(g->m_frame);
		delete g;
		m_waveformGroups.erase(g);
	}

	//If a splitter only has a group in the second half, move it to the first
	for(auto s : m_splitters)
	{
		auto first = s->get_child1();
		auto second = s->get_child2();
		if( (first == NULL) && (second != NULL) )
		{
			s->remove(*second);
			s->pack1(*second);
		}
	}

	//If a splitter only has a group in the first half, move it to the parent splitter and delete it
	//(if there is one)
	for(auto s : m_splitters)
	{
		auto first = s->get_child1();
		auto second = s->get_child2();
		if( (first != NULL) && (second == NULL) )
		{
			//If this is the top level splitter, we have no higher level to move it to
			if(s->get_parent() == &m_vbox)
			{
			}

			else
			{
				//TODO: move single occupant of empty splitter to parent
			}
		}
	}

	//Hide measurement display if there's no measurements in the group
	for(auto g : m_waveformGroups)
	{
		if(g->m_measurementColumns.empty())
			g->m_measurementFrame.hide();
		else
			g->m_measurementFrame.show_all();
	}
}

void OscilloscopeWindow::OnAutofitHorizontal()
{
	LogDebug("autofit horz\n");

	//
}

void OscilloscopeWindow::OnZoomInHorizontal(WaveformGroup* group)
{
	group->m_pixelsPerPicosecond *= 1.5;
	ClearPersistence(group);
}

void OscilloscopeWindow::OnZoomOutHorizontal(WaveformGroup* group)
{
	group->m_pixelsPerPicosecond /= 1.5;
	ClearPersistence(group);
}

void OscilloscopeWindow::ClearPersistence(WaveformGroup* group)
{
	auto children = group->m_vbox.get_children();
	for(auto w : children)
	{
		//Clear persistence on waveform areas
		auto area = dynamic_cast<WaveformArea*>(w);
		if(area != NULL)
			area->ClearPersistence();

		//Redraw everything (timeline included)
		w->queue_draw();
	}
}

void OscilloscopeWindow::ClearAllPersistence()
{
	for(auto w : m_waveformAreas)
	{
		w->ClearPersistence();
		w->queue_draw();
	}
}

void OscilloscopeWindow::OnQuit()
{
	close();
}

void OscilloscopeWindow::OnAddChannel(OscilloscopeChannel* chan)
{
	//Add to a random group for now
	DoAddChannel(chan, *m_waveformGroups.begin());
}

WaveformArea* OscilloscopeWindow::DoAddChannel(OscilloscopeChannel* chan, WaveformGroup* ngroup, WaveformArea* ref)
{
	auto decode = dynamic_cast<ProtocolDecoder*>(chan);
	if(decode)
		AddDecoder(decode);

	//Create the viewer
	auto w = new WaveformArea(
		chan->GetScope(),
		chan,
		this);
	w->m_group = ngroup;
	m_waveformAreas.emplace(w);

	ngroup->m_waveformBox.pack_start(*w);

	//Move the new trace after the reference trace, if one was provided
	if(ref != NULL)
	{
		auto children = ngroup->m_waveformBox.get_children();
		for(size_t i=0; i<children.size(); i++)
		{
			if(children[i] == ref)
				ngroup->m_waveformBox.reorder_child(*w, i+1);
		}
	}

	w->show();
	return w;
}

void OscilloscopeWindow::OnRemoveChannel(WaveformArea* w)
{
	//If we're about to remove the last viewer for a protocol decoder, forget about it
	auto chan = w->GetChannel();
	auto decode = dynamic_cast<ProtocolDecoder*>(chan);
	if(decode && (chan->GetRefCount() == 1) )
		m_decoders.erase(decode);

	//Get rid of the channel
	w->get_parent()->remove(*w);
	m_waveformAreas.erase(w);
	delete w;

	//Clean up in case it was the last channel in the group
	GarbageCollectGroups();
}

bool OscilloscopeWindow::OnTimer(int /*timer*/)
{
	//Flush the config cache every 2 seconds
	if( (GetTime() - m_tLastFlush) > 2)
	{
		for(auto scope : m_scopes)
			scope->FlushConfigCache();
	}

	//TODO: better sync for multiple instruments (wait for all to trigger THEN download waveforms)
	for(auto scope : m_scopes)
	{
		Oscilloscope::TriggerMode status = scope->PollTriggerFifo();
		if(status > Oscilloscope::TRIGGER_MODE_COUNT)
		{
			//Invalid value, skip it
			continue;
		}

		//If triggered, grab the data
		if(status == Oscilloscope::TRIGGER_MODE_TRIGGERED)
			OnWaveformDataReady(scope);
	}

	//Process pending draw calls before we do another polling cycle
	while(Gtk::Main::events_pending())
		Gtk::Main::iteration();

	//false to stop timer
	return true;
}

void OscilloscopeWindow::OnWaveformDataReady(Oscilloscope* scope)
{
	//make sure we close fully
	if(!is_visible())
		m_historyWindow.close();

	LogTrace("----Data ready----\n");
	LogIndenter li;

	//Make sure we don't free the old waveform data
	LogTrace("Detaching\n");
	for(size_t i=0; i<scope->GetChannelCount(); i++)
		scope->GetChannel(i)->Detach();

	//Download the data
	LogTrace("Acquiring\n");
	scope->AcquireDataFifo(sigc::mem_fun(*this, &OscilloscopeWindow::OnCaptureProgressUpdate));

	//Update the status
	UpdateStatusBar();

	//Re-arm trigger for another pass.
	//Do this before we re-run measurements etc, so triggering runs in parallel with the math
	if(!m_triggerOneShot)
		ArmTrigger(false);

	//We've stopped
	else
	{
		m_btnStart.set_sensitive(true);
		m_btnStartSingle.set_sensitive(true);
		m_btnStop.set_sensitive(false);
	}

	//Update the measurements
	for(auto g : m_waveformGroups)
		g->RefreshMeasurements();

	//Update our protocol decoders
	for(auto d : m_decoders)
		d->SetDirty();
	for(auto d : m_decoders)
		d->RefreshIfDirty();

	//Update the views
	for(auto w : m_waveformAreas)
	{
		if( (w->GetChannel()->GetScope() == scope) || (w->GetChannel()->GetScope() == NULL) )
			w->OnWaveformDataReady();
	}

	//Update protocol analyzers
	for(auto a : m_analyzers)
		a->OnWaveformDataReady();

	//Update the history window
	m_historyWindow.OnWaveformDataReady(scope);
}

void OscilloscopeWindow::UpdateStatusBar()
{
	//TODO: redo this for multiple scopes

	//Find the first enabled channel (assume same config as the rest for now)
	OscilloscopeChannel* chan = NULL;
	auto scope = m_scopes[0];
	for(size_t i=0; i<scope->GetChannelCount(); i++)
	{
		chan = scope->GetChannel(i);
		if(chan->IsEnabled())
			break;
	}
	auto data = chan->GetData();
	if(data == NULL)
		return;	//don't update

	double gsps = 1000 / data->m_timescale;
	char tmp[128];
	snprintf(tmp, sizeof(tmp), "%.1f GS/s", gsps);
	m_sampleRateLabel.set_label(tmp);

	size_t len = chan->GetData()->GetDepth();
	if(len > 1e6)
		snprintf(tmp, sizeof(tmp), "%.0f MS", len * 1e-6f);
	else if(len > 1e3)
		snprintf(tmp, sizeof(tmp), "%.0f kS", len * 1e-3f);
	else
		snprintf(tmp, sizeof(tmp), "%zu S", len);
	m_sampleCountLabel.set_label(tmp);

	string name = scope->GetChannel(scope->GetTriggerChannelIndex())->GetHwname();
	float voltage = scope->GetTriggerVoltage();
	if(voltage < 1)
		snprintf(tmp, sizeof(tmp), "%s %.0f mV", name.c_str(), voltage*1000);
	else
		snprintf(tmp, sizeof(tmp), "%s %.3f V", name.c_str(), voltage);
	m_triggerConfigLabel.set_label(tmp);
}

void OscilloscopeWindow::OnStart()
{
	m_btnStart.set_sensitive(false);
	m_btnStartSingle.set_sensitive(false);
	m_btnStop.set_sensitive(true);

	ArmTrigger(false);
}

void OscilloscopeWindow::OnStartSingle()
{
	m_btnStart.set_sensitive(false);
	m_btnStartSingle.set_sensitive(false);
	m_btnStop.set_sensitive(true);

	ArmTrigger(true);
}

void OscilloscopeWindow::OnStop()
{
	m_btnStart.set_sensitive(true);
	m_btnStartSingle.set_sensitive(true);
	m_btnStop.set_sensitive(false);

	for(auto scope : m_scopes)
		scope->Stop();
	m_triggerOneShot = true;
}

void OscilloscopeWindow::ArmTrigger(bool oneshot)
{
	for(auto scope : m_scopes)
		scope->StartSingleTrigger();
	m_triggerOneShot = oneshot;
	m_tArm = GetTime();
}

int OscilloscopeWindow::OnCaptureProgressUpdate(float /*progress*/)
{
	//Do nothing. We don't want to have draws occur while state is inconsistent.

	//Dispatch pending gtk events (such as draw calls)
	//while(Gtk::Main::events_pending())
	//	Gtk::Main::iteration();

	return 0;
}

/**
	@brief Called when the history view selects an old waveform
 */
void OscilloscopeWindow::OnHistoryUpdated()
{
	//Stop triggering if we select a saved waveform
	OnStop();

	//Update the measurements
	for(auto g : m_waveformGroups)
		g->RefreshMeasurements();

	//Update our protocol decoders
	for(auto d : m_decoders)
		d->SetDirty();
	for(auto d : m_decoders)
		d->RefreshIfDirty();

	//Update the views
	for(auto w : m_waveformAreas)
	{
		w->ClearPersistence();
		w->OnWaveformDataReady();
	}

	//Don't update the protocol analyzers, they should already have this waveform saved
}

void OscilloscopeWindow::RemoveHistory(TimePoint timestamp)
{
	for(auto a : m_analyzers)
		a->RemoveHistory(timestamp);
}

void OscilloscopeWindow::JumpToHistory(TimePoint timestamp)
{
	m_historyWindow.JumpToHistory(timestamp);
}
