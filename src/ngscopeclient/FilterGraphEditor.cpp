/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg                                                                          *
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
	@brief Implementation of FilterGraphEditor
 */
#include "ngscopeclient.h"
#include "FilterGraphEditor.h"
#include "MainWindow.h"
#include "BERTInputChannelDialog.h"
#include "BERTOutputChannelDialog.h"
#include "ChannelPropertiesDialog.h"
#include "FilterPropertiesDialog.h"
#include "EmbeddedTriggerPropertiesDialog.h"
#include "MeasurementsDialog.h"

//Pull in a bunch of filters we have special icons for
#include "../scopeprotocols/ACCoupleFilter.h"
#include "../scopeprotocols/ACRMSMeasurement.h"
#include "../scopeprotocols/AddFilter.h"
#include "../scopeprotocols/AreaMeasurement.h"
#include "../scopeprotocols/AverageFilter.h"
#include "../scopeprotocols/BandwidthMeasurement.h"
#include "../scopeprotocols/BaseMeasurement.h"
#include "../scopeprotocols/BurstWidthMeasurement.h"
#include "../scopeprotocols/ClipFilter.h"
#include "../scopeprotocols/ClockRecoveryFilter.h"
#include "../scopeprotocols/CSVExportFilter.h"
#include "../scopeprotocols/CSVImportFilter.h"
#include "../scopeprotocols/DeskewFilter.h"
#include "../scopeprotocols/DivideFilter.h"
#include "../scopeprotocols/DownsampleFilter.h"
#include "../scopeprotocols/DutyCycleMeasurement.h"
#include "../scopeprotocols/EnvelopeFilter.h"
#include "../scopeprotocols/Ethernet64b66bDecoder.h"
#include "../scopeprotocols/EyePattern.h"
#include "../scopeprotocols/FallMeasurement.h"
#include "../scopeprotocols/FFTFilter.h"
#include "../scopeprotocols/FrequencyMeasurement.h"
#include "../scopeprotocols/FullWidthHalfMax.h"
#include "../scopeprotocols/HistogramFilter.h"
#include "../scopeprotocols/IBM8b10bDecoder.h"
#include "../scopeprotocols/MaximumFilter.h"
#include "../scopeprotocols/MemoryFilter.h"
#include "../scopeprotocols/MinimumFilter.h"
#include "../scopeprotocols/MultiplyFilter.h"
#include "../scopeprotocols/OvershootMeasurement.h"
#include "../scopeprotocols/PeriodMeasurement.h"
#include "../scopeprotocols/PulseWidthMeasurement.h"
#include "../scopeprotocols/RiseMeasurement.h"
#include "../scopeprotocols/StepGeneratorFilter.h"
#include "../scopeprotocols/SubtractFilter.h"
#include "../scopeprotocols/ThresholdFilter.h"
#include "../scopeprotocols/ToneGeneratorFilter.h"
#include "../scopeprotocols/TopMeasurement.h"
#include "../scopeprotocols/TrendFilter.h"
#include "../scopeprotocols/UARTDecoder.h"
#include "../scopeprotocols/UndershootMeasurement.h"
#include "../scopeprotocols/UpsampleFilter.h"


using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FilterGraphGroup

FilterGraphGroup::FilterGraphGroup(FilterGraphEditor& ed)
	: m_parent(ed)
{
	m_outputId = ed.AllocateID();
	m_inputId = ed.AllocateID();
}

/**
	@brief Refreshes the list of child nodes within this node
 */
void FilterGraphGroup::RefreshChildren()
{
	//Get all of the node IDs
	int nnodes = ax::NodeEditor::GetNodeCount();
	vector<ax::NodeEditor::NodeId> nodes;
	nodes.resize(nnodes);
	ax::NodeEditor::GetOrderedNodeIds(&nodes[0], nnodes);

	//Check to see which are within us
	auto pos = ax::NodeEditor::GetNodePosition(m_id);
	auto size = ax::NodeEditor::GetNodeSize(m_id);
	m_children.clear();
	for(auto nid : nodes)
	{
		auto posNode = ax::NodeEditor::GetNodePosition(nid);
		auto sizeNode = ax::NodeEditor::GetNodeSize(nid);

		if(RectContains(pos, size, posNode, sizeNode))
			m_children.emplace(nid);
	}
}

/**
	@brief Refreshes the list of links between this group and the outside world
 */
void FilterGraphGroup::RefreshLinks()
{
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Outbound links

	//Make a list of all outlinks that we currently have to the outside world
	set<StreamDescriptor> outlinks;
	for(auto it : m_parent.m_linkMap)
	{
		auto link = it.first;

		//We only care about source pins IN this group, going to sink pins OUTSIDE this group
		auto from = m_parent.CanonicalizePin(link.first);
		auto to = m_parent.CanonicalizePin(link.second);
		if(m_childSourcePins.find(from) == m_childSourcePins.end())
			continue;
		if(m_childSinkPins.find(to) != m_childSinkPins.end())
			continue;

		//Look up the stream for the source node and mark it as used
		auto stream = m_parent.m_streamIDMap[from];
		if(!m_parent.m_streamIDMap.HasEntry(from))
			continue;
		outlinks.emplace(stream);

		//Add to the list of hierarchical output ports if it's not there already
		if(!m_hierOutputMap.HasEntry(stream))
			m_hierOutputMap.emplace(stream, m_parent.AllocateID());
		if(!m_hierOutputInternalMap.HasEntry(stream))
			m_hierOutputInternalMap.emplace(stream, m_parent.AllocateID());
	}

	//Remove any links that are no longer in use
	vector<StreamDescriptor> outgarbage;
	for(auto it : m_hierOutputMap)
	{
		if(outlinks.find(it.first) == outlinks.end())
			outgarbage.push_back(it.first);
	}
	for(auto stream : outgarbage)
	{
		m_hierOutputMap.erase(stream);
		m_hierOutputInternalMap.erase(stream);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Inbound links

	//TODO: why is this treating paths within the group as inlinks?

	//Make a list of all inlinks that we currently have to the outside world
	set< pair<FlowGraphNode*, int> > inlinks;
	for(auto it : m_parent.m_linkMap)
	{
		auto link = it.first;

		//We only care about source pins OUTSIDE this group, going to sink pins IN this group
		auto from = m_parent.CanonicalizePin(link.first);
		auto to = m_parent.CanonicalizePin(link.second);
		if(m_childSourcePins.find(from) != m_childSourcePins.end())
			continue;
		if(m_childSinkPins.find(to) == m_childSinkPins.end())
			continue;

		//Look up the stream for the sink node and mark it as used
		if(!m_parent.m_inputIDMap.HasEntry(to))
			continue;
		auto input = m_parent.m_inputIDMap[to];
		inlinks.emplace(input);

		//Add to the list of hierarchical input ports if it's not there already
		if(!m_hierInputMap.HasEntry(input))
			m_hierInputMap.emplace(input, m_parent.AllocateID());
		if(!m_hierInputInternalMap.HasEntry(input))
			m_hierInputInternalMap.emplace(input, m_parent.AllocateID());
	}

	//Remove any links that are no longer in use
	vector< pair<FlowGraphNode*, int> > ingarbage;
	for(auto it : m_hierInputMap)
	{
		if(inlinks.find(it.first) == inlinks.end())
			ingarbage.push_back(it.first);
	}
	for(auto stream : ingarbage)
	{
		m_hierInputMap.erase(stream);
		m_hierInputInternalMap.erase(stream);
	}
}

/**
	@brief Moves this node and all of its child nodes
 */
void FilterGraphGroup::MoveBy(ImVec2 displacement)
{
	auto pos = ax::NodeEditor::GetNodePosition(m_id);
	ax::NodeEditor::SetNodePosition(m_id, pos + displacement);

	for(auto nid : m_children)
	{
		auto cpos = ax::NodeEditor::GetNodePosition(nid);
		ax::NodeEditor::SetNodePosition(nid, cpos + displacement);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

FilterGraphEditor::FilterGraphEditor(Session& session, MainWindow* parent)
	: Dialog("Filter Graph Editor", "Filter Graph Editor", ImVec2(800, 600))
	, m_session(session)
	, m_parent(parent)
	, m_nextID(1)
{
	m_config.SaveSettings = &FilterGraphEditor::SaveSettingsCallback;
	m_config.LoadSettings = &FilterGraphEditor::LoadSettingsCallback;
	m_config.UserPointer = this;

	m_config.SettingsFile = "";
	m_context = ax::NodeEditor::CreateEditor(&m_config);

	//Load icons for filters
	m_parent->GetTextureManager()->LoadTexture("filter-64b66bdecoder", FindDataFile("icons/filters/filter-64b66bdecoder.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-8b10bdecoder", FindDataFile("icons/filters/filter-8b10bdecoder.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-ac-couple", FindDataFile("icons/filters/filter-ac-couple.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-ac-rms", FindDataFile("icons/filters/filter-ac-rms.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-add", FindDataFile("icons/filters/filter-add.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-area-under-curve", FindDataFile("icons/filters/filter-area-under-curve.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-average", FindDataFile("icons/filters/filter-average.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-base", FindDataFile("icons/filters/filter-base.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-bandwidth", FindDataFile("icons/filters/filter-bandwidth.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-burst-width", FindDataFile("icons/filters/filter-burst-width.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-cdrpll", FindDataFile("icons/filters/filter-cdrpll.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-clip", FindDataFile("icons/filters/filter-clip.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-csv-export", FindDataFile("icons/filters/filter-csv-export.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-csv-import", FindDataFile("icons/filters/filter-csv-import.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-deskew", FindDataFile("icons/filters/filter-deskew.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-downsample", FindDataFile("icons/filters/filter-downsample.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-duty-cycle", FindDataFile("icons/filters/filter-duty-cycle.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-divide", FindDataFile("icons/filters/filter-divide.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-envelope", FindDataFile("icons/filters/filter-envelope.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-eyepattern", FindDataFile("icons/filters/filter-eyepattern.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-fall", FindDataFile("icons/filters/filter-fall.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-fft", FindDataFile("icons/filters/filter-fft.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-frequency", FindDataFile("icons/filters/filter-frequency.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-fwhm", FindDataFile("icons/filters/filter-fwhm.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-histogram", FindDataFile("icons/filters/filter-histogram.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-period", FindDataFile("icons/filters/filter-period.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-pulse-width", FindDataFile("icons/filters/filter-pulse-width.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-max", FindDataFile("icons/filters/filter-max.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-memory", FindDataFile("icons/filters/filter-memory.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-min", FindDataFile("icons/filters/filter-min.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-multiply", FindDataFile("icons/filters/filter-multiply.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-overshoot", FindDataFile("icons/filters/filter-overshoot.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-rise", FindDataFile("icons/filters/filter-rise.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-sine", FindDataFile("icons/filters/filter-sine.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-step", FindDataFile("icons/filters/filter-step.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-subtract", FindDataFile("icons/filters/filter-subtract.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-threshold", FindDataFile("icons/filters/filter-threshold.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-top", FindDataFile("icons/filters/filter-top.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-trend", FindDataFile("icons/filters/filter-trend.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-uart", FindDataFile("icons/filters/filter-uart.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-upsample", FindDataFile("icons/filters/filter-upsample.png"));
	m_parent->GetTextureManager()->LoadTexture("filter-undershoot", FindDataFile("icons/filters/filter-undershoot.png"));
	m_parent->GetTextureManager()->LoadTexture("input-banana-dual", FindDataFile("icons/filters/input-banana-dual.png"));
	m_parent->GetTextureManager()->LoadTexture("input-bnc", FindDataFile("icons/filters/input-bnc.png"));
	m_parent->GetTextureManager()->LoadTexture("input-k-dual", FindDataFile("icons/filters/input-k-dual.png"));
	m_parent->GetTextureManager()->LoadTexture("input-k", FindDataFile("icons/filters/input-k.png"));
	m_parent->GetTextureManager()->LoadTexture("input-sma", FindDataFile("icons/filters/input-sma.png"));

	//Fill out map of filter class types to icon names
	m_filterIconMap[type_index(typeid(ACCoupleFilter))] 		= "filter-ac-couple";
	m_filterIconMap[type_index(typeid(ACRMSMeasurement))] 		= "filter-ac-rms";
	m_filterIconMap[type_index(typeid(AddFilter))] 				= "filter-add";
	m_filterIconMap[type_index(typeid(AreaMeasurement))] 		= "filter-area-under-curve";
	m_filterIconMap[type_index(typeid(AverageFilter))] 			= "filter-average";
	m_filterIconMap[type_index(typeid(BandwidthMeasurement))] 	= "filter-bandwidth";
	m_filterIconMap[type_index(typeid(BaseMeasurement))] 		= "filter-base";
	m_filterIconMap[type_index(typeid(BurstWidthMeasurement))] 	= "filter-burst-width";
	m_filterIconMap[type_index(typeid(ClipFilter))] 			= "filter-clip";
	m_filterIconMap[type_index(typeid(ClockRecoveryFilter))]	= "filter-cdrpll";
	m_filterIconMap[type_index(typeid(CSVExportFilter))] 		= "filter-csv-export";
	m_filterIconMap[type_index(typeid(CSVImportFilter))] 		= "filter-csv-import";
	m_filterIconMap[type_index(typeid(DeskewFilter))] 			= "filter-deskew";
	m_filterIconMap[type_index(typeid(DivideFilter))] 			= "filter-divide";
	m_filterIconMap[type_index(typeid(DownsampleFilter))] 		= "filter-downsample";
	m_filterIconMap[type_index(typeid(DutyCycleMeasurement))] 	= "filter-duty-cycle";
	m_filterIconMap[type_index(typeid(EnvelopeFilter))] 		= "filter-envelope";
	m_filterIconMap[type_index(typeid(Ethernet64b66bDecoder))] 	= "filter-64b66bdecoder";
	m_filterIconMap[type_index(typeid(EyePattern))] 			= "filter-eyepattern";
	m_filterIconMap[type_index(typeid(FallMeasurement))] 		= "filter-fall";
	m_filterIconMap[type_index(typeid(FFTFilter))] 				= "filter-fft";
	m_filterIconMap[type_index(typeid(FrequencyMeasurement))] 	= "filter-frequency";
	m_filterIconMap[type_index(typeid(FullWidthHalfMax))] 		= "filter-fwhm";
	m_filterIconMap[type_index(typeid(HistogramFilter))] 		= "filter-histogram";
	m_filterIconMap[type_index(typeid(IBM8b10bDecoder))] 		= "filter-8b10bdecoder";
	m_filterIconMap[type_index(typeid(MaximumFilter))] 			= "filter-max";
	m_filterIconMap[type_index(typeid(MemoryFilter))] 			= "filter-memory";
	m_filterIconMap[type_index(typeid(MinimumFilter))] 			= "filter-min";
	m_filterIconMap[type_index(typeid(MultiplyFilter))] 		= "filter-multiply";
	m_filterIconMap[type_index(typeid(PeriodMeasurement))] 		= "filter-period";
	m_filterIconMap[type_index(typeid(PulseWidthMeasurement))] 	= "filter-pulse-width";
	m_filterIconMap[type_index(typeid(RiseMeasurement))] 		= "filter-rise";
	m_filterIconMap[type_index(typeid(StepGeneratorFilter))] 	= "filter-step";
	m_filterIconMap[type_index(typeid(SubtractFilter))] 		= "filter-subtract";
	m_filterIconMap[type_index(typeid(ThresholdFilter))] 		= "filter-threshold";
	m_filterIconMap[type_index(typeid(ToneGeneratorFilter))] 	= "filter-sine";
	m_filterIconMap[type_index(typeid(TopMeasurement))] 		= "filter-top";
	m_filterIconMap[type_index(typeid(TrendFilter))] 			= "filter-trend";
	m_filterIconMap[type_index(typeid(TopMeasurement))] 		= "filter-top";
	m_filterIconMap[type_index(typeid(OvershootMeasurement))]	= "filter-overshoot";
	m_filterIconMap[type_index(typeid(UARTDecoder))]	 		= "filter-uart";
	m_filterIconMap[type_index(typeid(UndershootMeasurement))] 	= "filter-undershoot";
	m_filterIconMap[type_index(typeid(UpsampleFilter))] 		= "filter-upsample";

	//Load groups from parent, if we have any
	//Start by reserving group IDs so they don't get reused by anything else
	auto groups = parent->GetGraphEditorGroups();
	for(auto it : groups)
		m_session.m_idtable.ReserveID(it.first);
	for(auto it : groups)
	{
		auto group = make_shared<FilterGraphGroup>(*this);
		group->m_id = it.first;
		group->m_name = it.second;
		m_groups.emplace(group, group->m_id);
	}
}

FilterGraphEditor::~FilterGraphEditor()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool FilterGraphEditor::Render()
{
	//If we have an open properties dialog with a file browser open, run it
	auto dlg = m_propertiesDialogs[m_selectedProperties];
	auto fdlg = dynamic_pointer_cast<FilterPropertiesDialog>(dlg);
	auto bdlg = dynamic_pointer_cast<BERTInputChannelDialog>(dlg);
	if(fdlg)
		fdlg->RunFileDialog();
	else if(bdlg)
		bdlg->RunFileDialog();

	return Dialog::Render();
}

/**
	@brief Get a list of all channels that we are displaying nodes for
 */
map<Instrument*, vector<InstrumentChannel*> > FilterGraphEditor::GetAllChannels()
{
	map<Instrument*, vector<InstrumentChannel*> > ret;

	auto insts = m_session.GetInstruments();
	for(auto inst : insts)
	{
		vector<InstrumentChannel*> chans;

		//Channels
		auto scope = dynamic_cast<Oscilloscope*>(inst);
		auto psu = dynamic_cast<PowerSupply*>(inst);
		for(size_t i=0; i<inst->GetChannelCount(); i++)
		{
			auto chan = inst->GetChannel(i);

			//Exclude scope channels that can't be, or are not, enabled
			//TODO: should CanEnableChannel become an Instrument method?
			if(scope)
			{
				if(inst->GetInstrumentTypesForChannel(i) & Instrument::INST_OSCILLOSCOPE)
				{
					//If it's a trigger channel, allow it even if it's not enabled
					//TODO: only allow if currently selected
					if(chan == scope->GetExternalTrigger())
					{
					}
					else
					{
						if(!scope->CanEnableChannel(i))
							continue;
						if(!scope->IsChannelEnabled(i))
							continue;
					}
				}
			}

			//Exclude power supply channels that lack voltage/current controls
			//TODO: still allow filter graph control of on/off?
			if(psu)
			{
				if(inst->GetInstrumentTypesForChannel(i) & Instrument::INST_PSU)
				{
					if(!psu->SupportsVoltageCurrentControl(i))
						continue;
				}
			}

			chans.push_back(chan);
		}

		ret[inst] = chans;
	}

	return ret;
}

/**
	@brief Get a list of all objects we're displaying nodes for (channels, filters, triggers, etc)
 */
vector<FlowGraphNode*> FilterGraphEditor::GetAllNodes()
{
	vector<FlowGraphNode*> ret;

	//Channels
	auto chans = GetAllChannels();
	for(auto it : chans)
	{
		for(auto node : it.second)
			ret.push_back(node);
	}

	//Triggers
	auto insts = m_session.GetInstruments();
	for(auto inst : insts)
	{
		auto scope = dynamic_cast<Oscilloscope*>(inst);
		if(scope)
		{
			auto trig = scope->GetTrigger();
			if(trig)
				ret.push_back(trig);
		}
	}

	//Filters
	auto filters = Filter::GetAllInstances();
	for(auto f : filters)
		ret.push_back(f);

	return ret;
}

/**
	@brief Gets the source pin we should use for drawing a connection

	Note that this may not be the literal source if we're sourcing from a hierarchical port
 */
ax::NodeEditor::PinId FilterGraphEditor::GetSourcePinForLink(StreamDescriptor source, FlowGraphNode* sink)
{
	//Source not in a group? Just use the actual source
	if(!m_nodeGroupMap.HasEntry(source.m_channel))
		return m_streamIDMap[source];

	//Sink in same group as source? Use the actual source
	auto srcGroup = m_nodeGroupMap[source.m_channel];
	if(m_nodeGroupMap.HasEntry(sink))
	{
		if(srcGroup == m_nodeGroupMap[sink])
			return m_streamIDMap[source];
	}

	//Source is in a group, sink is not in the same group. Use the hierarchical port
	if(srcGroup->m_hierOutputMap.HasEntry(source))
		return srcGroup->m_hierOutputMap[source];

	//If we get here, the hierarchical port might have just been created this frame.
	//Use the original port temporarily
	return m_streamIDMap[source];
}

/**
	@brief Gets the sink pin we should use for drawing a connection

	Note that this may not be the literal sink if we're sinking to a hierarchical port
 */
ax::NodeEditor::PinId FilterGraphEditor::GetSinkPinForLink(StreamDescriptor source, pair<FlowGraphNode*, int> sink)
{
	//Sink not in a group? Use actual source
	if(!m_nodeGroupMap.HasEntry(sink.first))
		return m_inputIDMap[sink];

	//Sink in same group as source? Use actual sink
	auto sinkGroup = m_nodeGroupMap[sink.first];
	if(m_nodeGroupMap.HasEntry(source.m_channel))
	{
		if(sinkGroup == m_nodeGroupMap[source.m_channel])
			return m_inputIDMap[sink];
	}

	//Sink is in a group, source is not in the same group. Use the hierarchical port
	if(sinkGroup->m_hierInputMap.HasEntry(sink))
		return sinkGroup->m_hierInputMap[sink];

	//If we get here, the hierarchical port might have just been created this frame.
	//Use the original port temporarily
	return m_inputIDMap[sink];
}

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool FilterGraphEditor::DoRender()
{
	ax::NodeEditor::SetCurrentEditor(m_context);
	ax::NodeEditor::Begin("Filter Graph", ImVec2(0, 0));

	//Make nodes for all groups
	RefreshGroupPorts();
	for(auto it : m_groups)
		DoNodeForGroup(it.first);

	//Make nodes for all instrument channels
	auto chans = GetAllChannels();
	for(auto it : chans)
	{
		for(auto chan : it.second)
			DoNodeForChannel(chan, it.first);
	}

	//Make nodes for all triggers
	auto insts = m_session.GetInstruments();
	for(auto inst : insts)
	{
		auto scope = dynamic_cast<Oscilloscope*>(inst);

		//Triggers (for now, only scopes have these)
		if(scope)
		{
			auto trig = scope->GetTrigger();
			if(trig)
				DoNodeForTrigger(trig);
		}
	}

	//Filters
	auto filters = Filter::GetAllInstances();
	for(auto f : filters)
		DoNodeForChannel(f, nullptr);
	ClearOldPropertiesDialogs();

	//All nodes
	auto nodes = m_session.GetAllGraphNodes();

	//Add links within groups
	for(auto it : m_groups)
		DoInternalLinksForGroup(it.first);

	//Add links from each input to the stream it's fed by
	set<ax::NodeEditor::LinkId, lessID<ax::NodeEditor::LinkId> > freshLinks;
	for(auto f : nodes)
	{
		for(size_t i=0; i<f->GetInputCount(); i++)
		{
			auto stream = f->GetInput(i);
			if(stream)
			{
				auto srcid = GetSourcePinForLink(stream, f);
				auto dstid = GetSinkPinForLink(stream, pair<FlowGraphNode*, size_t>(f, i));
				auto linkid = GetID(pair<ax::NodeEditor::PinId, ax::NodeEditor::PinId>(srcid, dstid));
				freshLinks.emplace(linkid);
				ax::NodeEditor::Link(linkid, srcid, dstid);
			}
		}
	}

	//Add links from each trigger input to the stream it's fed by
	auto& scopes = m_session.GetScopes();
	for(auto scope : scopes)
	{
		auto trig = scope->GetTrigger();
		if(trig)
		{
			for(size_t i=0; i<trig->GetInputCount(); i++)
			{
				auto stream = trig->GetInput(i);
				if(stream)
				{
					auto srcid = GetSourcePinForLink(stream, trig);
					auto dstid = GetID(pair<FlowGraphNode*, size_t>(trig, i));
					auto linkid = GetID(pair<ax::NodeEditor::PinId, ax::NodeEditor::PinId>(srcid, dstid));
					freshLinks.emplace(linkid);
					ax::NodeEditor::Link(linkid, srcid, dstid);
				}
			}
		}
	}

	//Purge any stale entries in our link map
	set<ax::NodeEditor::LinkId, lessID<ax::NodeEditor::LinkId> > staleLinks;
	for(auto it : m_linkMap)
	{
		if(freshLinks.find(it.second) == freshLinks.end())
			staleLinks.emplace(it.second);
	}
	for(auto lid : staleLinks)
		m_linkMap.erase(lid);

	//Handle other user input
	Filter* fReconfigure = nullptr;
	HandleLinkCreationRequests(fReconfigure);
	HandleLinkDeletionRequests(fReconfigure);
	HandleNodeProperties();
	HandleBackgroundContextMenu();

	ax::NodeEditor::End();

	//Refresh all of our groups to have up-to-date child contents
	for(auto it : m_groups)
		it.first->RefreshChildren();

	//Look for and avoid overlaps
	//Must be after End() call which draws node boundaries, so everything is consistent.
	//If we don't do that, node content and frames can get one frame out of sync
	HandleOverlaps();

	ax::NodeEditor::SetCurrentEditor(nullptr);

	//If any filters were reconfigured, dispatch events accordingly
	if(fReconfigure)
	{
		//Update auto generated name
		if(fReconfigure->IsUsingDefaultName())
			fReconfigure->SetDefaultName();

		m_parent->OnFilterReconfigured(fReconfigure);
	}

	return true;
}

/**
	@brief Gets the ID for an arbitrary node
 */
ax::NodeEditor::NodeId FilterGraphEditor::GetID(FlowGraphNode* node)
{
	auto chan = dynamic_cast<InstrumentChannel*>(node);
	if(chan)
		return GetID(chan);

	auto trig = dynamic_cast<Trigger*>(node);
	if(trig)
		return GetID(trig);

	return m_session.m_idtable.emplace(node);
}

/**
	@brief Figure out which source/sink ports are within each group
 */
void FilterGraphEditor::RefreshGroupPorts()
{
	m_nodeGroupMap.clear();

	for(auto it : m_groups)
	{
		auto group = it.first;

		group->m_childSourcePins.clear();
		group->m_childSinkPins.clear();

		auto nodes = GetAllNodes();
		for(auto node : nodes)
		{
			auto id = GetID(node);
			auto chan = dynamic_cast<InstrumentChannel*>(node);

			//Skip anything outside our group
			if(group->m_children.find(id) == group->m_children.end())
				continue;

			m_nodeGroupMap.emplace(node, group);

			//Only instrument channels can source signals
			if(chan)
			{
				for(size_t i=0; i<chan->GetStreamCount(); i++)
				{
					StreamDescriptor stream(chan, i);
					group->m_childSourcePins.emplace(GetID(stream));
				}
			}

			//All flow graph nodes can sink signals
			for(size_t i=0; i<node->GetInputCount(); i++)
			{
				pair<FlowGraphNode*, int> indesc(node, i);
				group->m_childSinkPins.emplace(GetID(indesc));
			}
		}
	}
}

void FilterGraphEditor::DoNodeForGroup(std::shared_ptr<FilterGraphGroup> group)
{
	auto gid = m_groups[group];

	ImVec2 initialsize(320, 240);

	//Make the node for the group
	ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_NodeBg, ImColor(255, 255, 255, 64));
	ax::NodeEditor::BeginNode(gid);
	ImGui::PushID(gid.AsPointer());
	ImGui::TextUnformatted(group->m_name.c_str());
	ax::NodeEditor::Group(initialsize);
	ImGui::PopID();
	ax::NodeEditor::EndNode();
	ax::NodeEditor::PopStyleColor();

	//Find which of our source pins have edges to other groups
	group->RefreshLinks();

	//Groups cannot directly have ports, so make a dummy child node for the hierarchical ports
	DoNodeForGroupOutputs(group);
	DoNodeForGroupInputs(group);

	//Draw the force vector
	if(ImGui::IsKeyDown(ImGuiKey_Q))
	{
		RenderForceVector(
			ax::NodeEditor::GetNodeBackgroundDrawList(gid),
			ax::NodeEditor::GetNodePosition(gid),
			ax::NodeEditor::GetNodeSize(gid),
			m_nodeForces[gid]);
	}
}

void FilterGraphEditor::DoNodeForGroupInputs(shared_ptr<FilterGraphGroup> group)
{
	//Find parent group
	auto gid = m_groups[group];
	auto gpos = ax::NodeEditor::GetNodePosition(gid);

	//Figure out how big the port text is
	auto textfont = ImGui::GetFont();
	float oportmax = 1;
	float iportmax = textfont->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0, "‣").x;
	vector<string> onames;
	for(auto it : group->m_hierInputMap)
	{
		auto sink = it.first;

		//TODO refactor into function
		string sinkname = "(unimplemented)";
		auto chan = dynamic_cast<InstrumentChannel*>(sink.first);
		auto trig = dynamic_cast<Trigger*>(sink.first);
		if(chan)
			sinkname = chan->GetDisplayName();
		else if(trig)
			sinkname = trig->GetScope()->m_nickname;

		auto name = sinkname + " ‣";
		onames.push_back(name);
		oportmax = max(oportmax,
			textfont->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0, name.c_str()).x +
			ImGui::GetFontSize() * 2);
	}
	float nodewidth = oportmax + iportmax + 1*ImGui::GetStyle().ItemSpacing.x;

	//Set size/position
	auto headerfont = m_parent->GetFontPref("Appearance.Filter Graph.header_font");
	auto headerfontsize = headerfont->FontSize * ImGui::GetIO().FontGlobalScale;
	float headerheight = headerfontsize * 1.5;
	auto gborder = ax::NodeEditor::GetStyle().GroupBorderWidth;
	auto gpad = ax::NodeEditor::GetStyle().NodePadding.x;
	ImVec2 pos(
		gpos.x + gborder + gpad,
		gpos.y + headerheight + gborder);
	ax::NodeEditor::SetNodePosition(group->m_inputId, pos);
	ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_NodeRounding, 0);
	ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_NodeBorderWidth, 0);
	ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_HoveredNodeBorderWidth, 0);
	ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_SelectedNodeBorderWidth, 0);
	ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_NodeBg, ImColor(0, 0, 0, 0));
	ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_HovNodeBorder, ImColor(0, 0, 0, 0));
	ax::NodeEditor::BeginNode(group->m_inputId);
	ImGui::PushID(group->m_inputId.AsPointer());

	//Get node info
	pos = ax::NodeEditor::GetNodePosition(group->m_inputId);

	//Table of input ports
	if(ImGui::BeginTable("Ports", 2, 0, ImVec2(nodewidth, 0 ) ) )
	{
		ImGui::TableSetupColumn("inputs", ImGuiTableColumnFlags_WidthFixed, iportmax + 2);
		ImGui::TableSetupColumn("outputs", ImGuiTableColumnFlags_WidthFixed, oportmax + 2);

		for(auto it : group->m_hierInputMap)
		{
			ImGui::TableNextRow();

			auto sink = it.first;
			auto sid = it.second;

			if(sink.first == nullptr)
			{
				LogWarning("null sink\n");
				continue;
			}

			//Input side (path from external node to hierarchical port)
			ImGui::TableNextColumn();
			ax::NodeEditor::BeginPin(sid, ax::NodeEditor::PinKind::Input);
				ax::NodeEditor::PinPivotAlignment(ImVec2(0, 0.5));
				ImGui::TextUnformatted("‣");
			ax::NodeEditor::EndPin();

			//TODO refactor into function
			string sinkname = "(unimplemented)";
			auto chan = dynamic_cast<InstrumentChannel*>(sink.first);
			auto trig = dynamic_cast<Trigger*>(sink.first);
			if(chan)
				sinkname = chan->GetDisplayName();
			else if(trig)
				sinkname = trig->GetScope()->m_nickname;

			//Output side (path from hierarchical port to internal node)
			ImGui::TableNextColumn();
			ax::NodeEditor::BeginPin(group->m_hierInputInternalMap[sink], ax::NodeEditor::PinKind::Output);
				ax::NodeEditor::PinPivotAlignment(ImVec2(1, 0.5));
				RightJustifiedText(sinkname + "." + sink.first->GetInputName(sink.second) + " ‣");
			ax::NodeEditor::EndPin();
		}
		ImGui::EndTable();

	}

	ImGui::PopID();
	ax::NodeEditor::EndNode();
	ax::NodeEditor::PopStyleColor(2);
	ax::NodeEditor::PopStyleVar(4);
}

void FilterGraphEditor::DoNodeForGroupOutputs(shared_ptr<FilterGraphGroup> group)
{
	//Get dimensions of the parent group node
	auto gid = m_groups[group];
	auto gpos = ax::NodeEditor::GetNodePosition(gid);
	auto gsz = ax::NodeEditor::GetNodeSize(gid);

	//Figure out how big the port text is
	auto textfont = ImGui::GetFont();
	float oportmax = 1;
	float iportmax = textfont->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0, "‣").x;
	vector<string> onames;
	for(auto it : group->m_hierOutputMap)
	{
		auto stream = it.first;

		auto name = stream.GetName() + " ‣";
		onames.push_back(name);
		oportmax = max(oportmax, textfont->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0, name.c_str()).x);
	}
	float nodewidth = oportmax + iportmax + 3*ImGui::GetStyle().ItemSpacing.x;

	//Set size/position
	auto headerfont = m_parent->GetFontPref("Appearance.Filter Graph.header_font");
	auto headerfontsize = headerfont->FontSize * ImGui::GetIO().FontGlobalScale;
	float headerheight = headerfontsize * 1.5;
	auto gborder = ax::NodeEditor::GetStyle().GroupBorderWidth;
	auto gpad = ax::NodeEditor::GetStyle().NodePadding.x;
	ImVec2 pos(
		gpos.x + gsz.x - nodewidth - (gborder + gpad*3),
		gpos.y + headerheight + gborder);
	ax::NodeEditor::SetNodePosition(group->m_outputId, pos);

	ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_NodeRounding, 0);
	ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_NodeBorderWidth, 0);
	ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_HoveredNodeBorderWidth, 0);
	ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_SelectedNodeBorderWidth, 0);
	ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_NodeBg, ImColor(0, 0, 0, 0));
	ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_HovNodeBorder, ImColor(0, 0, 0, 0));
	ax::NodeEditor::BeginNode(group->m_outputId);
	ImGui::PushID(group->m_outputId.AsPointer());

	//Get node info
	pos = ax::NodeEditor::GetNodePosition(group->m_outputId);

	//Table of output ports
	StreamDescriptor hoveredStream(nullptr, 0);

	if(ImGui::BeginTable("Ports", 2, 0, ImVec2(nodewidth, 0 ) ) )
	{
		ImGui::TableSetupColumn("inputs", ImGuiTableColumnFlags_WidthFixed, iportmax + 2);
		ImGui::TableSetupColumn("outputs", ImGuiTableColumnFlags_WidthFixed, oportmax + 2);

		for(auto it : group->m_hierOutputMap)
		{
			ImGui::TableNextRow();

			auto stream = it.first;
			auto sid = it.second;

			//Input side (path from internal node to hierarchical port)
			ImGui::TableNextColumn();
			ax::NodeEditor::BeginPin(group->m_hierOutputInternalMap[stream], ax::NodeEditor::PinKind::Input);
				ax::NodeEditor::PinPivotAlignment(ImVec2(0, 0.5));
				ImGui::TextUnformatted("‣");
			ax::NodeEditor::EndPin();

			//Output side (path from hierarchical port to external node)
			ImGui::TableNextColumn();
			ax::NodeEditor::BeginPin(sid, ax::NodeEditor::PinKind::Output);
				ax::NodeEditor::PinPivotAlignment(ImVec2(1, 0.5));
				RightJustifiedText(stream.GetName() + " ‣");
			ax::NodeEditor::EndPin();

			if(sid == ax::NodeEditor::GetHoveredPin())
				hoveredStream = stream;
		}

		ImGui::EndTable();
	}

	//Tooltip on hovered output port
	if(hoveredStream)
	{
		//Output port
		ax::NodeEditor::Suspend();
			OutputPortTooltip(hoveredStream);
		ax::NodeEditor::Resume();
	}

	ImGui::PopID();
	ax::NodeEditor::EndNode();
	ax::NodeEditor::PopStyleColor(2);
	ax::NodeEditor::PopStyleVar(4);
}

/**
	@brief Handle links between nodes in a group and the hierarchical ports
 */
void FilterGraphEditor::DoInternalLinksForGroup(shared_ptr<FilterGraphGroup> group)
{
	//Add links from node outputs to the hierarchical port node
	for(auto it : group->m_hierOutputInternalMap)
	{
		auto fromstream = it.first;

		auto fromPin = GetID(fromstream);
		auto toPin = it.second;

		ax::NodeEditor::LinkId lid;
		if(group->m_hierOutputLinkMap.HasEntry(fromstream))
			lid = group->m_hierOutputLinkMap[fromstream];
		else
		{
			lid = AllocateID();
			group->m_hierOutputLinkMap.emplace(fromstream, lid);
		}

		ax::NodeEditor::Link(lid, fromPin, toPin);
	}

	//And then again for the inputs
	for(auto it : group->m_hierInputInternalMap)
	{
		auto toport = it.first;

		auto toPin = GetID(toport);
		auto fromPin = it.second;

		ax::NodeEditor::LinkId lid;
		if(group->m_hierInputLinkMap.HasEntry(toport))
			lid = group->m_hierInputLinkMap[toport];
		else
		{
			lid = AllocateID();
			group->m_hierInputLinkMap.emplace(toport, lid);
		}

		ax::NodeEditor::Link(lid, fromPin, toPin);
	}
}

/**
	@brief Delete old properties dialogs for no-longer-extant nodes
 */
void FilterGraphEditor::ClearOldPropertiesDialogs()
{
	//Get all of the node IDs
	int nnodes = ax::NodeEditor::GetNodeCount();
	vector<ax::NodeEditor::NodeId> nodes;
	nodes.resize(nnodes);
	ax::NodeEditor::GetOrderedNodeIds(&nodes[0], nnodes);

	//Make a set we can quickly search
	set<ax::NodeEditor::NodeId, lessID<ax::NodeEditor::NodeId> > nodeset;
	for(auto n : nodes)
		nodeset.emplace(n);

	//Find any node IDs that no longer are in use
	vector<ax::NodeEditor::NodeId> idsToRemove;
	for(auto it : m_propertiesDialogs)
	{
		if(nodeset.find(it.first) == nodeset.end())
			idsToRemove.push_back(it.first);
	}

	//Remove them
	for(auto i : idsToRemove)
		m_propertiesDialogs.erase(i);
}

/**
	@brief Display tooltips when mousing over interesting stuff
 */
void FilterGraphEditor::OutputPortTooltip(StreamDescriptor stream)
{
	ImGui::BeginTooltip();
		switch(stream.GetType())
		{
			case Stream::STREAM_TYPE_ANALOG:
				ImGui::TextUnformatted("Analog channel");
				break;

			case Stream::STREAM_TYPE_DIGITAL:
				ImGui::TextUnformatted("Digital channel");
				break;

			case Stream::STREAM_TYPE_DIGITAL_BUS:
				ImGui::TextUnformatted("Digital bus");
				break;

			case Stream::STREAM_TYPE_EYE:
				ImGui::TextUnformatted("Eye pattern");
				break;

			case Stream::STREAM_TYPE_SPECTROGRAM:
				ImGui::TextUnformatted("Spectrogram");
				break;

			case Stream::STREAM_TYPE_WATERFALL:
				ImGui::TextUnformatted("Waterfall");
				break;

			case Stream::STREAM_TYPE_PROTOCOL:
				ImGui::TextUnformatted("Protocol data");
				break;

			case Stream::STREAM_TYPE_TRIGGER:
				ImGui::TextUnformatted("External trigger");
				break;

			case Stream::STREAM_TYPE_ANALOG_SCALAR:
				{
					ImGui::TextUnformatted("Analog value:");
					string value = stream.GetYAxisUnits().PrettyPrint(stream.GetScalarValue());
					ImGui::TextUnformatted(value.c_str());
				}
				break;

			default:
				ImGui::TextUnformatted("Unknown channel type");
				break;
		}
		ImGui::TextUnformatted("Drag from this port to create a connection.");
	ImGui::EndTooltip();
}

/**
	@brief Calculates the forces applied to each node in the graph based on interaction physics
 */
void FilterGraphEditor::CalculateNodeForces(
	const vector<ax::NodeEditor::NodeId>& nodes,
	const vector<bool>& isgroup,
	const vector<bool>& dragging,
	const vector<bool>& nocollide,
	const vector<ImVec2>& positions,
	const vector<ImVec2>& sizes,
	vector<ImVec2>& forces)
{
	//Loop over all nodes and find potential collisions
	for(size_t i=0; i<nodes.size(); i++)
	{
		if(nocollide[i])
			continue;

		auto nodeA = nodes[i];
		auto posA = positions[i];
		auto sizeA = sizes[i];
		bool groupA = isgroup[i];

		for(size_t j=i+1; j<nodes.size(); j++)
		{
			if(nocollide[j])
				continue;

			auto nodeB = nodes[j];
			auto posB = positions[j];
			auto sizeB = sizes[j];
			bool groupB = isgroup[j];

			//Check for node-group collisions
			//Node-node is normal code path, group-group also repels
			if( (groupA && !groupB) || (!groupA && groupB) )
			{
				auto nid = groupA ? nodeB : nodeA;
				auto gid = groupA ? nodeA : nodeB;

				auto posNode = ax::NodeEditor::GetNodePosition(nid);
				auto sizeNode = ax::NodeEditor::GetNodeSize(nid);

				auto posGroup = ax::NodeEditor::GetNodePosition(gid);
				auto sizeGroup = ax::NodeEditor::GetNodeSize(gid);

				//If node is completely INSIDE the group, don't repel
				if(RectContains(posGroup, sizeGroup, posNode, sizeNode))
					continue;

				//If dragging group, we should push nodes away
				//But if dragging the node, allow it to go into the group
				if( (dragging[i] && !groupA) || (dragging[j] && !groupB) )
					continue;
			}

			//If no overlap, no action required
			if(!RectIntersect(posA, sizeA, posB, sizeB))
				continue;

			//We have an overlap!
			//Find the unit vector between the node positions
			float dx = posB.x - posA.x;
			float dy = posB.y - posA.y;
			float mag = sqrt(dx*dx + dy*dy);
			ImVec2 force;
			if(mag > 1e-2)
				force = ImVec2(dx / mag, dy / mag);

			//If nodes are exactly on top of each other apply a force in a random direction
			else
			{
				float theta = fmodf(rand() * 1e-3f, 2*M_PI);
				force = ImVec2(sin(theta), cos(theta));
			}

			//Add this to the existing force vector
			forces[i] -= force;
			forces[j] += force;
		}
	}
}

/**
	@brief Once forces are calculated, actually move the nodes (unless being dragged)
 */
void FilterGraphEditor::ApplyNodeForces(
	const vector<ax::NodeEditor::NodeId>& nodes,
	const vector<bool>& isgroup,
	const vector<bool>& dragging,
	const vector<ImVec2>& positions,
	vector<ImVec2>& forces)
{
	//Don't actually apply the forces
	if(ImGui::IsKeyDown(ImGuiKey_Q))
		return;

	//needs to be high enough that we can have force components >1 in both axes
	//since imgui-node-editor seems to round to pixel coordinates
	float velocityScale = 5;

	for(size_t i=0; i<nodes.size(); i++)
	{
		//If dragging, node is immovable - ignore the force
		if(dragging[i])
			continue;

		//Otherwise, normalize the summed force vector and scale by fixed velocity.
		//If force is substantially zero then there's nothing overlapping, so don't move
		auto f = forces[i];
		auto mag = sqrt(f.x*f.x + f.y*f.y);
		if(mag < 1e-2)
			continue;

		f = f * velocityScale / mag;

		//If node B is a group, we need to move all nodes inside it by the same amount we moved the group
		if(isgroup[i])
			m_groups[nodes[i]]->MoveBy(f);

		//Otherwise just move the node
		else
			ax::NodeEditor::SetNodePosition(nodes[i], positions[i] + f);
	}

	//Map of node IDs being dragged
	set<ax::NodeEditor::NodeId, lessID<ax::NodeEditor::NodeId>> dragNodes;
	for(size_t i=0; i<nodes.size(); i++)
	{
		if(dragging[i])
			dragNodes.emplace(nodes[i]);
	}

	//Second pass: Find nodes that WERE in a group, but are no longer fully inside it
	//and enlarge the group to encompass them.
	//TODO: also find nodes that got pushed into a group here
	for(auto it : m_nodeGroupMap)
	{
		auto nid = GetID(it.first);
		auto gid = m_groups[it.second];

		//If node is being dragged, stop: we don't want to expand the group if we're intentionally trying to leave
		if(dragNodes.find(nid) != dragNodes.end())
			continue;

		auto posNode = ax::NodeEditor::GetNodePosition(nid);
		auto sizeNode = ax::NodeEditor::GetNodeSize(nid);

		auto posGroup = ax::NodeEditor::GetNodePosition(gid);

		auto gnode = reinterpret_cast<ax::NodeEditor::Detail::EditorContext*>(m_context)->FindNode(gid);
		auto sizeGroup = gnode->m_GroupBounds.Max - gnode->m_GroupBounds.Min;

		//Still in the group? All good
		if(RectContains(posGroup, sizeGroup, posNode, sizeNode))
			continue;

		//If we get here, the node got pushed out of the group.
		//We need to resize the group to encompass it.
		auto brNode = posNode + sizeNode;
		auto brGroup = posGroup + sizeGroup;
		posGroup.x = min(posGroup.x, posNode.x);
		posGroup.y = min(posGroup.y, posNode.y);
		brGroup.x = max(brGroup.x, brNode.x);
		brGroup.y = max(brGroup.y, brNode.y);
		sizeGroup = brGroup - posGroup;

		//TODO: add a bit of padding so nodes can't go all the way to the outer border of the group?

		//Apply the changes
		ax::NodeEditor::SetNodePosition(gid, posGroup);
		ax::NodeEditor::SetGroupSize(gid, sizeGroup);
	}
}

/**
	@brief Find nodes that are intersecting, and apply forces to resolve collisions
 */
void FilterGraphEditor::HandleOverlaps()
{
	//Get all of the node IDs
	int nnodes = ax::NodeEditor::GetNodeCount();
	vector<ax::NodeEditor::NodeId> nodes;
	nodes.resize(nnodes);
	ax::NodeEditor::GetOrderedNodeIds(&nodes[0], nnodes);

	//Default all nodes to having zero force on them
	vector<ImVec2> forces;
	forces.resize(nnodes);

	//Get starting positions of each node and figure out if it's a group or normal node
	vector<ImVec2> positions;
	vector<ImVec2> sizes;
	vector<bool> isgroup;
	positions.resize(nnodes);
	sizes.resize(nnodes);
	isgroup.resize(nnodes);
	for(int i=0; i<nnodes; i++)
	{
		positions[i] = ax::NodeEditor::GetNodePosition(nodes[i]);
		sizes[i] = ax::NodeEditor::GetNodeSize(nodes[i]);
		isgroup[i] = m_groups.HasEntry(nodes[i]);
	}

	//Find nodes which should not have collision detection applied to them
	//(e.g. virtual nodes for group input/output regions)
	vector<bool> nocollide;
	nocollide.resize(nnodes);
	for(int i=0; i<nnodes; i++)
	{
		auto nid = nodes[i];
		for(auto it : m_groups)
		{
			if( (nid == it.first->m_inputId) || (nid == it.first->m_outputId) )
			{
				nocollide[i] = true;
				break;
			}
		}
	}

	//Figure out if each node is being dragged or not
	//Need to use internal APIs since this isn't in the public API (annoying)
	vector<bool> dragging;
	dragging.resize(nnodes);
	auto action = reinterpret_cast<ax::NodeEditor::Detail::EditorContext*>(m_context)->GetCurrentAction();
	if(action)
	{
		auto drag = action->AsDrag();
		if(drag)
		{
			for(int i=0; i<nnodes; i++)
			{
				for(auto o : drag->m_Objects)
				{
					auto n = o->AsNode();
					if(!n)
						continue;

					if(n->m_ID == nodes[i])
					{
						dragging[i] = true;
						break;
					}
				}
			}
		}
	}

	//Calculate forces from interaction physics
	CalculateNodeForces(nodes, isgroup, dragging, nocollide, positions, sizes, forces);

	//DEBUG: save the forces
	m_nodeForces.clear();
	for(int i=0; i<nnodes; i++)
		m_nodeForces[nodes[i]] = forces[i];

	//Apply the forces to move the nodes
	//For now, no persistent velocities
	//(modeling a very sticky/high friction surface where things stop immediately when force is removed)
	ApplyNodeForces(nodes, isgroup, dragging, positions, forces);
}

/**
	@brief Gets the actual source/sink pin given a pin which might be a hierarchical port
 */
ax::NodeEditor::PinId FilterGraphEditor::CanonicalizePin(ax::NodeEditor::PinId port)
{
	for(auto it : m_groups)
	{
		auto group = it.first;

		//Check for hierarchical outputs
		if(group->m_hierOutputMap.HasEntry(port))
			return m_streamIDMap[group->m_hierOutputMap[port]];

		//Check for hierarchical inputs
		if(group->m_hierInputMap.HasEntry(port))
			return m_inputIDMap[group->m_hierInputMap[port]];

		//Check for input-facing side of hierarchical inputs
		if(group->m_hierInputInternalMap.HasEntry(port))
		{
			//Figure out what's driving it
			auto input = group->m_hierInputInternalMap[port];
			auto stream = input.first->GetInput(input.second);
			if(stream)
				return CanonicalizePin(GetID(stream));
		}
	}

	return port;
}

/**
	@brief Handle requests to create a new link
 */
void FilterGraphEditor::HandleLinkCreationRequests(Filter*& fReconfigure)
{
	//for some reason node editor wants colors as vec4 not ImU32
	auto& prefs = m_session.GetPreferences();
	auto validcolor = ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Filter Graph.valid_link_color"));
	auto invalidcolor = ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Filter Graph.invalid_link_color"));

	if(ax::NodeEditor::BeginCreate())
	{
		ax::NodeEditor::PinId startId, endId;
		if(ax::NodeEditor::QueryNewLink(&startId, &endId))
		{
			//If both IDs are valid, consider making the path
			if(startId && endId)
			{
				//If start or end pin ID are hierarchical ports, re-map to the actual source port
				startId = CanonicalizePin(startId);
				endId = CanonicalizePin(endId);

				//Link creation code doesn't know start vs dest
				//If we started from the input, swap the pins
				if(m_inputIDMap.HasEntry(startId))
				{
					ax::NodeEditor::PinId tmp = startId;
					startId = endId;
					endId = tmp;
				}

				//Make sure both paths exist and it's a path from output to input
				if(m_inputIDMap.HasEntry(endId) && m_streamIDMap.HasEntry(startId))
				{
					//Get the stream and port we want to look at
					auto inputPort = m_inputIDMap[endId];
					auto stream = m_streamIDMap[startId];

					//Check for and reject back edges (creates cycles)
					if(IsBackEdge(stream.m_channel, inputPort.first))
					{
						ax::NodeEditor::RejectNewItem(invalidcolor);

						ImGui::BeginTooltip();
							ImGui::TextColored(invalidcolor, "x Cannot create loops in filter graph");
						ImGui::EndTooltip();
					}

					//See if the path is valid
					else if(inputPort.first->ValidateChannel(inputPort.second, stream))
					{
						//Yep, looks good
						ImGui::BeginTooltip();
							ImGui::TextColored(validcolor, "+ Connect Port");
						ImGui::EndTooltip();

						if(ax::NodeEditor::AcceptNewItem(validcolor))
						{
							//Hook it up
							inputPort.first->SetInput(inputPort.second, stream);

							//Update names, if needed
							fReconfigure = dynamic_cast<Filter*>(inputPort.first);

							//Push trigger changes if needed
							auto t = dynamic_cast<Trigger*>(inputPort.first);
							if(t != nullptr)
								t->GetScope()->PushTrigger();
						}
					}

					//Not valid
					else
					{
						ax::NodeEditor::RejectNewItem(invalidcolor);

						ImGui::BeginTooltip();
							ImGui::TextColored(invalidcolor, "x Incompatible stream type for input");
						ImGui::EndTooltip();
					}
				}

				//Complain if both ports are input or both output
				if(m_inputIDMap.HasEntry(endId) && m_inputIDMap.HasEntry(startId))
				{
					ax::NodeEditor::RejectNewItem(invalidcolor);

					ImGui::BeginTooltip();
						ImGui::TextColored(invalidcolor, "x Cannot connect two input ports");
					ImGui::EndTooltip();
				}
				if(m_streamIDMap.HasEntry(endId) && m_streamIDMap.HasEntry(startId))
				{
					ax::NodeEditor::RejectNewItem(invalidcolor);

					ImGui::BeginTooltip();
						ImGui::TextColored(invalidcolor, "x Cannot connect two output ports");
					ImGui::EndTooltip();
				}
			}
		}

		if(ax::NodeEditor::QueryNewNode(&startId) && startId)
		{
			startId = CanonicalizePin(startId);

			//Dragging from node output - create new filter from that
			if(m_streamIDMap.HasEntry(startId))
			{
				//See what the stream is
				m_newFilterSourceStream = m_streamIDMap[startId];

				//Cannot create filters using external trigger as input
				if(m_newFilterSourceStream.GetType() == Stream::STREAM_TYPE_TRIGGER)
				{
					ImGui::BeginTooltip();
						ImGui::TextColored(invalidcolor, "x Cannot use external trigger as input to a filter");
					ImGui::EndTooltip();

					ax::NodeEditor::RejectNewItem(invalidcolor);
				}

				//All good otherwise
				else
				{
					ImGui::BeginTooltip();
						ImGui::TextColored(validcolor, "+ Create Filter");
					ImGui::EndTooltip();

					if(ax::NodeEditor::AcceptNewItem())
					{
						ax::NodeEditor::Suspend();
						m_createMousePos = ImGui::GetMousePos();
						ImGui::OpenPopup("Create Filter");
						ax::NodeEditor::Resume();
					}
				}
			}

			//Dragging from node input - display list of channels
			else if(m_inputIDMap.HasEntry(startId))
			{
				ImGui::BeginTooltip();
					ImGui::TextColored(validcolor, "+ Add Channel");
				ImGui::EndTooltip();

				if(ax::NodeEditor::AcceptNewItem())
				{
					m_createInput = m_inputIDMap[startId];

					ax::NodeEditor::Suspend();
					m_createMousePos = ImGui::GetMousePos();
					ImGui::OpenPopup("Add Input");
					ax::NodeEditor::Resume();
				}
			}
		}
	}
	ax::NodeEditor::EndCreate();

	ax::NodeEditor::Suspend();

		//Create-filter menu
		if(ImGui::BeginPopup("Create Filter"))
		{
			FilterMenu(m_newFilterSourceStream);
			ImGui::EndPopup();
		}

		//Add-input menu
		if(ImGui::BeginPopup("Add Input"))
		{
			CreateChannelMenu();
			ImGui::EndPopup();
		}

	ax::NodeEditor::Resume();
}

/**
	@brief Determine if a proposed edge in the filter graph is a back edge (one whose creation would lead to a cycle)

	@param src	Source node
	@param dst	Destination node

	@return True if dst is equal to src, or if dst is directly or indirectly used as an input by src.
 */
bool FilterGraphEditor::IsBackEdge(FlowGraphNode* src, FlowGraphNode* dst)
{
	if( (src == nullptr) || (dst == nullptr) )
		return false;

	if(src == dst)
		return true;

	//Check each input of src
	for(size_t i=0; i<src->GetInputCount(); i++)
	{
		auto stream = src->GetInput(i);
		if(IsBackEdge(stream.m_channel, dst))
			return true;
	}

	return false;
}

/**
	@brief Runs the "add input" menu
 */
void FilterGraphEditor::CreateChannelMenu()
{
	if(ImGui::BeginMenu("Channels"))
	{
		vector<StreamDescriptor> streams;

		auto& scopes = m_session.GetScopes();
		for(auto scope : scopes)
		{
			//Channels
			for(size_t i=0; i<scope->GetChannelCount(); i++)
			{
				if(!scope->CanEnableChannel(i))
					continue;

				auto chan = scope->GetOscilloscopeChannel(i);
				if(!chan)
					continue;

				for(size_t j=0; j<chan->GetStreamCount(); j++)
					streams.push_back(StreamDescriptor(chan, j));
			}
		}

		//Filters
		auto filters = Filter::GetAllInstances();
		for(auto f : filters)
		{
			for(size_t j=0; j<f->GetStreamCount(); j++)
				streams.push_back(StreamDescriptor(f, j));
		}

		//Run the actual menu
		for(auto s : streams)
		{
			//Skip anything not valid for this sink
			if(!m_createInput.first->ValidateChannel(m_createInput.second, s))
				continue;

			//Don't allow creation of back edges
			if(m_createInput.first == s.m_channel)
				continue;

			//Show menu items
			if(ImGui::MenuItem(s.GetName().c_str()))
			{
				m_createInput.first->SetInput(m_createInput.second, s);

				auto trig = dynamic_cast<Trigger*>(m_createInput.first);
				if(trig)
					trig->GetScope()->PushTrigger();
			}
		}

		ImGui::EndMenu();
	}
	if(ImGui::BeginMenu("Create"))
	{
		auto& refs = m_parent->GetSession().GetReferenceFilters();

		//Find all filters in this category and sort them alphabetically
		vector<string> sortedNames;
		for(auto it : refs)
		{
			if(it.second->GetCategory() == Filter::CAT_GENERATION)
				sortedNames.push_back(it.first);
		}
		std::sort(sortedNames.begin(), sortedNames.end());

		//Do all of the menu items
		for(auto fname : sortedNames)
		{
			auto it = refs.find(fname);

			//For now: don't allow creation of filters that take inputs if going back
			if(it->second->GetInputCount() != 0)
				continue;

			if(ImGui::MenuItem(fname.c_str()))
			{
				//Make the filter but don't spawn a properties dialog for it or add to a waveform area
				auto f = m_parent->CreateFilter(fname, nullptr, StreamDescriptor(nullptr, 0), false, false);

				//Get relative mouse position
				auto mousePos = ax::NodeEditor::ScreenToCanvas(m_createMousePos);

				//Assign initial positions
				ax::NodeEditor::SetNodePosition(GetID(f), mousePos);

				//Once the filter exists, hook it up
				m_createInput.first->SetInput(m_createInput.second, StreamDescriptor(f, 0));

				auto trig = dynamic_cast<Trigger*>(m_createInput.first);
				if(trig)
					trig->GetScope()->PushTrigger();
			}
		}

		ImGui::EndMenu();
	}
}

/**
	@brief Runs the "create filter" menu
 */
void FilterGraphEditor::FilterMenu(StreamDescriptor stream)
{
	//See if the source stream is a scalar, if so offer to add a measurement
	if(stream.GetType() == Stream::STREAM_TYPE_ANALOG_SCALAR)
	{
		//Only offer to measure if not already being measured
		auto dlg = m_parent->GetMeasurementsDialog(false);
		if(!dlg || !dlg->HasStream(stream))
		{
			if(ImGui::MenuItem("Measure"))
				m_parent->GetMeasurementsDialog(true)->AddStream(stream);
			ImGui::Separator();
		}
	}

	FilterSubmenu(stream, "Bus", Filter::CAT_BUS);
	FilterSubmenu(stream, "Clocking", Filter::CAT_CLOCK);
	FilterSubmenu(stream, "Export", Filter::CAT_EXPORT);
	FilterSubmenu(stream, "Generation", Filter::CAT_GENERATION);
	FilterSubmenu(stream, "Math", Filter::CAT_MATH);
	FilterSubmenu(stream, "Measurement", Filter::CAT_MEASUREMENT);
	FilterSubmenu(stream, "Memory", Filter::CAT_MEMORY);
	FilterSubmenu(stream, "Miscellaneous", Filter::CAT_MISC);
	FilterSubmenu(stream, "Optical", Filter::CAT_OPTICAL);
	FilterSubmenu(stream, "Power", Filter::CAT_POWER);
	FilterSubmenu(stream, "RF", Filter::CAT_RF);
	FilterSubmenu(stream, "Serial", Filter::CAT_SERIAL);
	FilterSubmenu(stream, "Signal integrity", Filter::CAT_ANALYSIS);
}

/**
	@brief Run the submenu for a single filter category
 */
void FilterGraphEditor::FilterSubmenu(StreamDescriptor stream, const string& name, Filter::Category cat)
{
	auto& refs = m_parent->GetSession().GetReferenceFilters();

	if(ImGui::BeginMenu(name.c_str()))
	{
		//Find all filters in this category and sort them alphabetically
		vector<string> sortedNames;
		for(auto it : refs)
		{
			if(it.second->GetCategory() == cat)
				sortedNames.push_back(it.first);
		}
		std::sort(sortedNames.begin(), sortedNames.end());

		//Do all of the menu items
		for(auto fname : sortedNames)
		{
			auto it = refs.find(fname);
			bool valid = false;
			if(it->second->GetInputCount() == 0)		//No inputs? Always valid
				valid = true;
			else
				valid = it->second->ValidateChannel(0, stream);

			//Hide import filters to avoid cluttering the UI
			if( (cat == Filter::CAT_GENERATION) && (fname.find("Import") != string::npos))
				continue;

			if(ImGui::MenuItem(fname.c_str(), nullptr, false, valid))
			{
				//Make the filter but don't spawn a properties dialog for it
				//If measurement, don't add trends by default
				bool addToArea = true;
				if(cat == Filter::CAT_MEASUREMENT )
					addToArea = false;
				auto f = m_parent->CreateFilter(fname, nullptr, stream, false, addToArea);

				//Get relative mouse position
				auto mousePos = ax::NodeEditor::ScreenToCanvas(m_createMousePos);

				//Assign initial positions
				ax::NodeEditor::SetNodePosition(GetID(f), mousePos);
			}
		}

		ImGui::EndMenu();
	}
}

/**
	@brief Handle requests to delete a link
 */
void FilterGraphEditor::HandleLinkDeletionRequests(Filter*& fReconfigure)
{
	if(ax::NodeEditor::BeginDelete())
	{
		ax::NodeEditor::LinkId lid;
		while(ax::NodeEditor::QueryDeletedLink(&lid))
		{
			//Handle deletion of special paths
			if(!m_linkMap.HasEntry(lid))
			{
				//It's probably an internal path within a group
				//For now, bruteforce groups: this is infrequent and probably not worth the hassle
				//of maintaining an index for
				for(auto it : m_groups)
				{
					auto group = it.first;

					if(group->m_hierOutputLinkMap.HasEntry(lid))
					{
						//For now, reject it
						//TODO: disconnect every sink we're driving? or allow deletion if single sink?

						ax::NodeEditor::RejectDeletedItem();
					}

					//Handle hierarchical inputs
					else if(group->m_hierInputLinkMap.HasEntry(lid))
					{
						if(ax::NodeEditor::AcceptDeletedItem())
						{
							auto sink = group->m_hierInputLinkMap[lid];
							group->m_hierInputLinkMap.erase(lid);

							sink.first->SetInput(sink.second, StreamDescriptor(nullptr, 0), true);
							fReconfigure = dynamic_cast<Filter*>(sink.first);
							break;
						}
					}
				}
			}

			//All paths are deleteable for now
			else if(ax::NodeEditor::AcceptDeletedItem())
			{
				//All paths are from stream to input port
				//so second ID in the link should be the input, which is now connected to nothing
				auto pins = m_linkMap[lid];
				m_linkMap.erase(pins);
				auto inputPort = m_inputIDMap[CanonicalizePin(pins.second)];
				inputPort.first->SetInput(inputPort.second, StreamDescriptor(nullptr, 0), true);

				fReconfigure = dynamic_cast<Filter*>(inputPort.first);
			}
		}

		ax::NodeEditor::NodeId nid;
		while(ax::NodeEditor::QueryDeletedNode(&nid))
		{
			//See if it's a group
			if(m_groups.HasEntry(nid))
			{
				if(ax::NodeEditor::AcceptDeletedItem())
				{
					auto group = m_groups[nid];

					//Remove other references to the group
					set<FlowGraphNode*> nodesToUngroup;
					for(auto it : m_nodeGroupMap)
					{
						if(it.second == group)
							nodesToUngroup.emplace(it.first);
					}
					for(auto n : nodesToUngroup)
						m_nodeGroupMap.erase(n);

					m_groups.erase(nid);
				}
			}

			else
			{
				//TODO: allow deletion of filters/channels
				ax::NodeEditor::RejectDeletedItem();
			}
		}
	}
	ax::NodeEditor::EndDelete();

}

/**
	@brief Make a node for a trigger
 */
void FilterGraphEditor::DoNodeForTrigger(Trigger* trig)
{
	//TODO: special color for triggers?
	//Or use a preference?
	auto& prefs = m_session.GetPreferences();
	auto tsize = ImGui::GetFontSize();
	auto color = ColorFromString("#808080");
	auto id = GetID(trig);
	auto headercolor = prefs.GetColor("Appearance.Filter Graph.header_text_color");
	auto headerfont = m_parent->GetFontPref("Appearance.Filter Graph.header_font");
	auto headerfontsize = headerfont->FontSize * ImGui::GetIO().FontGlobalScale;
	float headerheight = headerfontsize * 1.5;
	float rounding = ax::NodeEditor::GetStyle().NodeRounding;

	ax::NodeEditor::BeginNode(id);
	ImGui::PushID(id.AsPointer());

	//Get node info
	auto pos = ax::NodeEditor::GetNodePosition(id);
	auto size = ax::NodeEditor::GetNodeSize(id);
	string headerText = trig->GetTriggerDisplayName();
	if(m_session.IsMultiScope())
		headerText = trig->GetScope()->m_nickname + ": " + headerText;

	//Figure out how big the header text is and reserve space for it
	auto headerSize = headerfont->CalcTextSizeA(headerfontsize, FLT_MAX, 0, headerText.c_str());
	float nodewidth = max(15*tsize, headerSize.x);
	ImGui::Dummy(ImVec2(nodewidth, headerheight));

	//Table of ports
	static ImGuiTableFlags flags = 0;
	if(ImGui::BeginTable("Ports", 2, flags, ImVec2(nodewidth, 0 ) ) )
	{
		//Input ports
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		for(size_t i=0; i<trig->GetInputCount(); i++)
		{
			auto sid = GetID(pair<FlowGraphNode*, size_t>(trig, i));

			string portname("‣ ");
			portname += trig->GetInputName(i);
			ax::NodeEditor::BeginPin(sid, ax::NodeEditor::PinKind::Input);
				ax::NodeEditor::PinPivotAlignment(ImVec2(0, 0.5));
				ImGui::TextUnformatted(portname.c_str());
			ax::NodeEditor::EndPin();
		}

		//Output ports: none,  triggers are input only
		ImGui::TableNextColumn();

		ImGui::EndTable();
	}

	//Tooltip on hovered node
	if(ax::NodeEditor::GetHoveredPin())
	{}
	else if(id == ax::NodeEditor::GetHoveredNode())
	{
		ax::NodeEditor::Suspend();
			ImGui::BeginTooltip();
				ImGui::TextUnformatted("Drag node to move.\nRight click to open node properties.");
			ImGui::EndTooltip();
		ax::NodeEditor::Resume();
	}

	//Done with node
	ImGui::PopID();
	ax::NodeEditor::EndNode();

	//Draw header after the node is done
	auto bgList = ax::NodeEditor::GetNodeBackgroundDrawList(id);
	bgList->AddRectFilled(
		ImVec2(pos.x + 1, pos.y + 1),
		ImVec2(pos.x + size.x - 1, pos.y + headerheight - 1),
		color,
		rounding,
		ImDrawFlags_RoundCornersTop);
	bgList->AddText(
		headerfont,
		headerfontsize,
		ImVec2(pos.x + headerfontsize*0.5, pos.y + headerfontsize*0.25),
		headercolor,
		headerText.c_str());
}

/**
	@brief Make a node for a single channel, of any type

	TODO: this seems to fail hard if we do not have at least one input OR output on the node. Why?
 */
void FilterGraphEditor::DoNodeForChannel(InstrumentChannel* channel, Instrument* inst)
{
	//If the channel has no color, make it neutral gray
	//(this is often true for e.g. external trigger)
	string displaycolor = channel->m_displaycolor;
	if(displaycolor.empty())
		displaycolor = "#808080";

	auto ochan = dynamic_cast<OscilloscopeChannel*>(channel);
	auto& prefs = m_session.GetPreferences();

	//Get some configuration / style settings
	auto color = ColorFromString(displaycolor);
	auto headercolor = prefs.GetColor("Appearance.Filter Graph.header_text_color");
	auto headerfont = m_parent->GetFontPref("Appearance.Filter Graph.header_font");
	auto headerfontsize = headerfont->FontSize * ImGui::GetIO().FontGlobalScale;
	auto textfont = m_parent->GetFontPref("Appearance.Filter Graph.icon_caption_font");
	auto textfontsize = textfont->FontSize * ImGui::GetIO().FontGlobalScale;
	float headerheight = headerfontsize * 1.5;
	float rounding = ax::NodeEditor::GetStyle().NodeRounding;

	auto id = GetID(channel);
	ax::NodeEditor::BeginNode(id);
	ImGui::PushID(id.AsPointer());

	//Get node info
	auto pos = ax::NodeEditor::GetNodePosition(id);
	auto size = ax::NodeEditor::GetNodeSize(id);
	string headerText = channel->GetDisplayName();

	//If NOT an oscilloscope channel, or if a multi-scope session: scope by instrument name
	if( (!ochan && inst) || (ochan && ochan->GetScope() && m_session.IsMultiScope()) )
		headerText = inst->m_nickname + ": " + headerText;

	//Figure out how big the header text is
	auto headerSize = headerfont->CalcTextSizeA(headerfontsize, FLT_MAX, 0, headerText.c_str());


	//Format block type early, even though it's not drawn until later
	//so that we know how much space to allocate
	string blocktype;
	auto f = dynamic_cast<Filter*>(channel);
	if(f)
		blocktype = f->GetProtocolDisplayName();
	else
	{
		//see if input or output
		if( (dynamic_cast<PowerSupplyChannel*>(channel)) ||
			(dynamic_cast<FunctionGeneratorChannel*>(channel)) ||
			(dynamic_cast<RFSignalGeneratorChannel*>(channel)) ||
			(dynamic_cast<BERTOutputChannel*>(channel))
			)
		{
			blocktype = "Hardware output";
		}
		else
			blocktype = "Hardware input";
	}
	ImVec2 iconsize(ImGui::GetFontSize() * 6, ImGui::GetFontSize() * 3);
	auto captionsize = textfont->CalcTextSizeA(textfontsize, FLT_MAX, 0, blocktype.c_str());

	//Reserve space for the center icon and node type caption
	float iconwidth = max(iconsize.x, captionsize.x);

	//Figure out how big the port text is
	float iportmax = 1;
	float oportmax = 1;
	vector<string> inames;
	vector<string> onames;
	for(size_t i=0; i<channel->GetInputCount(); i++)
	{
		auto name = string("‣ ") + channel->GetInputName(i);
		inames.push_back(name);
		iportmax = max(iportmax, textfont->CalcTextSizeA(textfontsize, FLT_MAX, 0, name.c_str()).x);
	}
	for(size_t i=0; i<channel->GetStreamCount(); i++)
	{
		auto name = channel->GetStreamName(i) + " ‣";
		onames.push_back(name);
		oportmax = max(oportmax, textfont->CalcTextSizeA(textfontsize, FLT_MAX, 0, name.c_str()).x);
	}
	float colswidth = iportmax + oportmax + iconwidth;
	float nodewidth = max(colswidth, headerSize.x) + 3*ImGui::GetStyle().ItemSpacing.x;

	//For really long node names, stretch icon column
	float iconcolwidth = iconwidth;
	if(headerSize.x > colswidth)
		iconcolwidth = headerSize.x - (iportmax + oportmax);

	//Reserve space for the node header
	auto startpos = ImGui::GetCursorPos();
	ImGui::Dummy(ImVec2(nodewidth, headerheight));

	//Table of inputs at left and outputs at right
	//TODO: this should move up to base class or something?
	static ImGuiTableFlags flags = /*ImGuiTableFlags_Borders*/ 0;
	StreamDescriptor hoveredStream(nullptr, 0);
	auto bodystart = ImGui::GetCursorPos();
	ImVec2 iconpos(1, 1);
	if(ImGui::BeginTable("Ports", 3, flags, ImVec2(nodewidth, 0 ) ) )
	{
		size_t maxports = max(channel->GetInputCount(), channel->GetStreamCount());

		ImGui::TableSetupColumn("inputs", ImGuiTableColumnFlags_WidthFixed, iportmax + 2);
		ImGui::TableSetupColumn("icon", ImGuiTableColumnFlags_WidthFixed, iconcolwidth + 2);
		ImGui::TableSetupColumn("outputs", ImGuiTableColumnFlags_WidthFixed, oportmax + 2);

		for(size_t i=0; i<maxports; i++)
		{
			ImGui::TableNextRow();

			//Input ports
			ImGui::TableNextColumn();
			if(i < channel->GetInputCount())
			{
				auto sid = GetID(pair<InstrumentChannel*, size_t>(channel, i));

				ax::NodeEditor::BeginPin(sid, ax::NodeEditor::PinKind::Input);
					ax::NodeEditor::PinPivotAlignment(ImVec2(0, 0.5));
					ImGui::TextUnformatted(inames[i].c_str());
				ax::NodeEditor::EndPin();
			}

			//Icon
			ImGui::TableNextColumn();
			if(i == 0)
				iconpos = ImGui::GetCursorPos();
			ImGui::Dummy(ImVec2(iconcolwidth, 1));

			//Output ports
			ImGui::TableNextColumn();
			if(i < channel->GetStreamCount())
			{
				StreamDescriptor stream(channel, i);
				auto sid = GetID(stream);

				ax::NodeEditor::BeginPin(sid, ax::NodeEditor::PinKind::Output);
					ax::NodeEditor::PinPivotAlignment(ImVec2(1, 0.5));
					RightJustifiedText(onames[i]);
				ax::NodeEditor::EndPin();

				if(sid == ax::NodeEditor::GetHoveredPin())
					hoveredStream = stream;
			}
		}

		ImGui::EndTable();
	}

	//Reserve space for icon and caption if needed
	float contentHeight = ImGui::GetCursorPos().y - bodystart.y;
	float minHeight = iconsize.y + 3*ImGui::GetStyle().ItemSpacing.y + textfontsize;
	if(contentHeight < minHeight)
		ImGui::Dummy(ImVec2(1, minHeight - contentHeight));

	//Tooltip on hovered output port
	if(hoveredStream)
	{
		//TODO: input port

		//Output port
		ax::NodeEditor::Suspend();
			OutputPortTooltip(hoveredStream);
		ax::NodeEditor::Resume();
	}

	//Tooltip on hovered node
	else if(id == ax::NodeEditor::GetHoveredNode())
	{
		ax::NodeEditor::Suspend();
			ImGui::BeginTooltip();
				ImGui::TextUnformatted("Drag node to move.\nRight click to open node properties.");
			ImGui::EndTooltip();
		ax::NodeEditor::Resume();
	}

	ImGui::PopID();
	ax::NodeEditor::EndNode();

	//Draw header after the node is done
	auto bgList = ax::NodeEditor::GetNodeBackgroundDrawList(id);
	bgList->AddRectFilled(
		ImVec2(pos.x + 1, pos.y + 1),
		ImVec2(pos.x + size.x - 1, pos.y + headerheight - 1),
		color,
		rounding,
		ImDrawFlags_RoundCornersTop);
	bgList->AddText(
		headerfont,
		headerfontsize,
		ImVec2(pos.x + headerfontsize*0.5, pos.y + headerfontsize*0.25),
		headercolor,
		headerText.c_str());

	//Draw the force vector
	if(ImGui::IsKeyDown(ImGuiKey_Q))
		RenderForceVector(bgList, pos, size, m_nodeForces[id]);

	//Draw icon for filter blocks
	float iconshift = (iconcolwidth - iconwidth) / 2;
	ImVec2 icondelta = (iconpos - startpos) + ImVec2(ImGui::GetStyle().ItemSpacing.x + iconshift, 0);
	NodeIcon(channel, pos + icondelta, iconsize, bgList);

	//Draw icon caption
	auto textColor = prefs.GetColor("Appearance.Filter Graph.icon_caption_color");
	ImVec2 textpos =
		pos + icondelta +
		ImVec2(0, iconsize.y + ImGui::GetStyle().ItemSpacing.y*3);
	bgList->AddText(
		textfont,
		textfontsize,
		textpos + ImVec2( (iconwidth - captionsize.x)/2, 0),
		textColor,
		blocktype.c_str());
}

void FilterGraphEditor::RenderForceVector(ImDrawList* list, ImVec2 pos, ImVec2 size, ImVec2 vec)
{
	//uncomment to enable this for debugging
	return;

	float width = 2;

	ImVec2 center = pos + size/2;

	//Origin point
	list->AddCircleFilled(
		center,
		4,
		ColorFromString("#0000ff", 255));

	//Main line
	auto endpos = center+(25*vec);
	list->AddLine(
		center,
		endpos,
		ColorFromString("#00ff00", 255),
		width);
}

/**
	@brief Draws an icon showing the function of a node
 */
void FilterGraphEditor::NodeIcon(InstrumentChannel* chan, ImVec2 pos, ImVec2 iconsize, ImDrawList* list)
{
	pos.y += ImGui::GetStyle().ItemSpacing.y*2;

	//Not a filter? Check connector type
	string iconname = "";
	if(dynamic_cast<Filter*>(chan) == nullptr)
	{
		switch(chan->GetPhysicalConnector())
		{
			case InstrumentChannel::CONNECTOR_BANANA_DUAL:
				iconname = "input-banana-dual";
				break;

			case InstrumentChannel::CONNECTOR_K_DUAL:
				iconname = "input-k-dual";
				break;
			case InstrumentChannel::CONNECTOR_K:
				iconname = "input-k";
				break;
			case InstrumentChannel::CONNECTOR_SMA:
				iconname = "input-sma";
				break;

			//TODO: make icons for these
			case InstrumentChannel::CONNECTOR_BMA:
			case InstrumentChannel::CONNECTOR_N:

			case InstrumentChannel::CONNECTOR_BNC:
			default:
				iconname = "input-bnc";
				break;
		}
	}

	//It's a filter, check if we have an icon for it
	else
	{
		auto it = m_filterIconMap.find(typeid(*chan));
		if(it != m_filterIconMap.end())
			iconname = it->second;
	}

	if(iconname != "")
	{
		list->AddImage(
			m_parent->GetTextureManager()->GetTexture(iconname),
			pos,
			pos + iconsize );
		return;
	}

	//If we get here, no graphical icon.
}

/**
	@brief Open the properties window when a node is right clicked
 */
void FilterGraphEditor::HandleNodeProperties()
{
	//Look for context menu
	ax::NodeEditor::NodeId id;
	if(ax::NodeEditor::ShowNodeContextMenu(&id))
	{
		m_selectedProperties = id;

		//See if we're right clicking on a group or a node
		if(m_groups.HasEntry(id))
		{
			ax::NodeEditor::Suspend();
				ImGui::OpenPopup("Group Properties");
			ax::NodeEditor::Resume();
		}

		else
		{
			auto node = static_cast<FlowGraphNode*>(m_session.m_idtable[(uintptr_t)id]);
			auto trig = dynamic_cast<Trigger*>(node);
			auto o = dynamic_cast<OscilloscopeChannel*>(node);
			auto f = dynamic_cast<Filter*>(o);
			auto bo = dynamic_cast<BERTOutputChannel*>(node);
			auto bi = dynamic_cast<BERTInputChannel*>(node);

			//Make the properties window
			if(m_propertiesDialogs.find(id) == m_propertiesDialogs.end())
			{
				if(trig)
					m_propertiesDialogs[id] = make_shared<EmbeddedTriggerPropertiesDialog>(trig->GetScope());
				else if(f)
					m_propertiesDialogs[id] = make_shared<FilterPropertiesDialog>(f, m_parent, true);
				else if(bo)
					m_propertiesDialogs[id] = make_shared<BERTOutputChannelDialog>(bo, true);
				else if(bi)
					m_propertiesDialogs[id] = make_shared<BERTInputChannelDialog>(bi, m_parent, true);

				//must be last since many other types are derived from OscilloscopeChannel
				else if(o)
					m_propertiesDialogs[id] = make_shared<ChannelPropertiesDialog>(o, true);
				else
					LogWarning("Don't know how to display properties of this node!\n");
			}

			//Create the popup
			ax::NodeEditor::Suspend();
				ImGui::OpenPopup("Node Properties");
			ax::NodeEditor::Resume();
		}
	}

	//Run the properties dialogs
	ax::NodeEditor::Suspend();
	if(ImGui::BeginPopup("Node Properties"))
	{
		auto dlg = m_propertiesDialogs[m_selectedProperties];
		if(dlg)
			dlg->RenderAsChild();
		ImGui::EndPopup();
	}
	if(ImGui::BeginPopup("Group Properties"))
	{
		if(m_groups.HasEntry(m_selectedProperties))
		{
			auto group = m_groups[m_selectedProperties];
			ImGui::InputText("Name", &group->m_name);
		}
		ImGui::EndPopup();
	}
	ax::NodeEditor::Resume();
}

/**
	@brief Show add menu when background is right clicked
 */
void FilterGraphEditor::HandleBackgroundContextMenu()
{
	if(ax::NodeEditor::ShowBackgroundContextMenu())
	{
		ax::NodeEditor::Suspend();
			ImGui::OpenPopup("Add Menu");
		ax::NodeEditor::Resume();
	}

	//Run the popup
	ax::NodeEditor::Suspend();
	m_createMousePos = ImGui::GetMousePos();
	if(ImGui::BeginPopup("Add Menu"))
	{
		DoAddMenu();
		ImGui::EndPopup();
	}

	//If no nodes, show help message
	//(but only if popup isn't already open)
	else
	{
		if(ax::NodeEditor::GetNodeCount() == 0)
		{
			ImGui::BeginTooltip();
				ImGui::TextUnformatted("Right click to create a waveform\nor import data from a file");
			ImGui::EndTooltip();
		}
	}
	ax::NodeEditor::Resume();
}

/**
	@brief Implement the add menu
 */
void FilterGraphEditor::DoAddMenu()
{
	//Get all generation filters, sorted alphabetically
	auto& refs = m_session.GetReferenceFilters();
	vector<string> sortedNames;
	for(auto it : refs)
	{
		if(it.second->GetCategory() == Filter::CAT_GENERATION)
			sortedNames.push_back(it.first);
	}
	std::sort(sortedNames.begin(), sortedNames.end());

	if(ImGui::BeginMenu("Import"))
	{
		//Do all of the menu items
		for(auto fname : sortedNames)
		{
			//Hide everything but import filters
			if(fname.find("Import") == string::npos)
				continue;

			string shortname = fname.substr(0, fname.size() - strlen(" Import"));

			//Unlike normal filter creation, we DO want the properties dialog shown immediately
			//since we need to specify a file name to do anything
			if(ImGui::MenuItem(shortname.c_str()))
				m_parent->CreateFilter(fname, nullptr, StreamDescriptor(nullptr, 0));
		}
		ImGui::EndMenu();
	}

	if(ImGui::BeginMenu("Generate"))
	{
		//Do all of the menu items
		for(auto fname : sortedNames)
		{
			//Hide import filters
			if(fname.find("Import") != string::npos)
				continue;

			//Hide filters that have inputs
			if(refs.find(fname)->second->GetInputCount() != 0)
				continue;

			if(ImGui::MenuItem(fname.c_str()))
				m_parent->CreateFilter(fname, nullptr, StreamDescriptor(nullptr, 0));
		}

		ImGui::EndMenu();
	}

	ImGui::Separator();

	if(ImGui::MenuItem("New Group"))
	{
		auto group = make_shared<FilterGraphGroup>(*this);
		auto id = GetID(group);
		group->m_id = id;
		group->m_name = string("Group ") + to_string((intptr_t)id.AsPointer());

		//Get relative mouse position
		auto mousePos = ax::NodeEditor::ScreenToCanvas(m_createMousePos);

		//Assign initial positions
		ax::NodeEditor::SetNodePosition(id, mousePos);

		m_groups.emplace(group, id);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ID allocation

/**
	@brief Allocate an ID, avoiding collisions with the session IDTable
 */
uintptr_t FilterGraphEditor::AllocateID()
{
	//Get next ID, if it's in use try the next one
	uintptr_t id = m_nextID;
	while(m_session.m_idtable.HasID(id))
		id++;

	//Reserve the ID in the session table so nobody else will try to use it
	m_session.m_idtable.ReserveID(id);

	//We now have an ID that is not in the table, so continue from there
	m_nextID = id + 1;
	return id;
}

ax::NodeEditor::PinId FilterGraphEditor::GetID(StreamDescriptor stream)
{
	//If it's in the table already, just return the ID
	if(m_streamIDMap.HasEntry(stream))
		return m_streamIDMap[stream];

	//Not in the table, allocate an ID
	auto id = AllocateID();
	m_streamIDMap.emplace(stream, id);
	return id;
}

ax::NodeEditor::PinId FilterGraphEditor::GetID(pair<FlowGraphNode*, size_t> input)
{
	//If it's in the table already, just return the ID
	if(m_inputIDMap.HasEntry(input))
		return m_inputIDMap[input];

	//Not in the table, allocate an ID
	auto id = AllocateID();
	m_inputIDMap.emplace(input, id);
	return id;
}

ax::NodeEditor::LinkId FilterGraphEditor::GetID(pair<ax::NodeEditor::PinId, ax::NodeEditor::PinId> link)
{
	//If it's in the table already, just return the ID
	if(m_linkMap.HasEntry(link))
		return m_linkMap[link];

	//Not in the table, allocate an ID
	auto id = AllocateID();
	m_linkMap.emplace(link, id);
	return id;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Save configuration

bool FilterGraphEditor::SaveSettingsCallback(
	const char* data,
	size_t size,
	ax::NodeEditor::SaveReasonFlags /*flags*/,
	void* pThis)
{
	auto ed = reinterpret_cast<FilterGraphEditor*>(pThis);
	ed->m_parent->OnGraphEditorConfigModified(std::string(data, size));
	return true;
}

/**
	@param data		Buffer to write data into
	@param pThis	Pointer to the FilterGraphEditor object

	This function is called twice, once with a null data argument to get the required size, then again
	with a valid pointer to store the data. The size must not change between the two invocations.

	@return Number of bytes required for data
 */
size_t FilterGraphEditor::LoadSettingsCallback(
	char* data,
	void* pThis)
{
	auto ed = reinterpret_cast<FilterGraphEditor*>(pThis);
	const string& blob = ed->m_parent->GetGraphEditorConfigBlob();

	if(data != nullptr)
		memcpy(data, blob.c_str(), blob.length());

	return blob.length();
}

/**
	@brief Return a list of group IDs and names
 */
map<uintptr_t, string> FilterGraphEditor::GetGroupIDs()
{
	map<uintptr_t, string> ret;
	for(auto it : m_groups)
		ret[reinterpret_cast<uintptr_t>(it.second.AsPointer())] = it.first->m_name;
	return ret;
}
