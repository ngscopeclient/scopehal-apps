/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2023 Andrew D. Zonenberg                                                                          *
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
	@brief Implementation of ScopeDeskewWizard
 */

#include "ngscopeclient.h"
#include "ScopeDeskewWizard.h"
#include "MainWindow.h"

#include <cinttypes>

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Argument objects

UniformCrossCorrelateArgs::UniformCrossCorrelateArgs(
	UniformAnalogWaveform* ppri, UniformAnalogWaveform* psec, int64_t delta)
	: priTimescale(ppri->m_timescale)
	, secTimescale(psec->m_timescale)
	, trigPhaseDelta(ppri->m_triggerPhase - psec->m_triggerPhase)
	, startingDelta(-delta)
	, numDeltas(delta*2)
	, priLen(ppri->size())
	, secLen(psec->size())
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ScopeDeskewWizard::ScopeDeskewWizard(
	shared_ptr<TriggerGroup> group,
	Oscilloscope* secondary,
	MainWindow* parent,
	Session& session)
	: Dialog(
		string("Deskew Oscilloscope: ") + secondary->m_nickname,
		"Deskew" + secondary->m_nickname,
		ImVec2(700, 400))
	, m_state(STATE_WELCOME_1)
	, m_group(group)
	, m_secondary(secondary)
	, m_parent(parent)
	, m_session(session)
	, m_useExtRefPrimary(true)
	, m_useExtRefSecondary(true)
	, m_measureCycle(0)
	, m_lastTriggerTimestamp(0)
	, m_lastTriggerFs(0)
	, m_bestCorrelation(0)
	, m_bestCorrelationOffset(0)
	, m_maxSkewSamples(30000)
	, m_medianSkew(0)
	, m_queue(g_vkQueueManager->GetComputeQueue("ScopeDeskewWizard.queue"))
	, m_pool(*g_vkComputeDevice,
		vk::CommandPoolCreateInfo(
			vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			m_queue->m_family ))
	, m_cmdBuf(std::move(vk::raii::CommandBuffers(*g_vkComputeDevice,
		vk::CommandBufferAllocateInfo(*m_pool, vk::CommandBufferLevel::ePrimary, 1)).front()))
	, m_corrOut("corrOut")
{
	m_uniformUnequalRatePipeline = make_shared<ComputePipeline>(
		"shaders/ScopeDeskewUniformUnequalRate.spv", 3, sizeof(UniformCrossCorrelateArgs));

	m_uniformEqualRatePipeline = make_shared<ComputePipeline>(
		"shaders/ScopeDeskewUniformEqualRate.spv", 3, sizeof(UniformCrossCorrelateArgs));

	m_uniform4xRatePipeline = make_shared<ComputePipeline>(
		"shaders/ScopeDeskewUniform4xRate.spv", 3, sizeof(UniformCrossCorrelateArgs));

	if(g_hasDebugUtils)
	{
		g_vkComputeDevice->setDebugUtilsObjectNameEXT(
			vk::DebugUtilsObjectNameInfoEXT(
				vk::ObjectType::eCommandPool,
				reinterpret_cast<uint64_t>(static_cast<VkCommandPool>(*m_pool)),
				"ScopeDeskewWizard.pool"));

		g_vkComputeDevice->setDebugUtilsObjectNameEXT(
			vk::DebugUtilsObjectNameInfoEXT(
				vk::ObjectType::eCommandBuffer,
				reinterpret_cast<int64_t>(static_cast<VkCommandBuffer>(*m_cmdBuf)),
				"ScopeDeskewWizard.cmdbuf"));
	}

	m_corrOut.SetCpuAccessHint(AcceleratorBuffer<float>::HINT_LIKELY);
	m_corrOut.SetGpuAccessHint(AcceleratorBuffer<float>::HINT_UNLIKELY);
	m_corrOut.resize(2*m_maxSkewSamples);

	m_gpuCorrelationAvailable = g_hasShaderInt64;

	//Clear out any existing skew calibration
	m_session.SetDeskew(m_secondary, 0);
}

ScopeDeskewWizard::~ScopeDeskewWizard()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool ScopeDeskewWizard::DoRender()
{
	if(m_state == STATE_CLOSE)
		return false;

	switch(m_state)
	{
		case STATE_WELCOME_1:
			ImGui::PushFont(m_parent->GetFontPref("Appearance.General.title_font"));
			ImGui::TextUnformatted("Welcome");
			ImGui::PopFont();
			ImGui::Separator();

			ImGui::TextWrapped(
				"This wizard measures the trigger-path propagation delay between the primary instrument (%s) "
				"and the secondary instrument (%s), and calibrates out the delay so waveforms from both "
				"instruments appear correctly aligned in the ngscopeclient timeline.",
				m_group->m_primary->m_nickname.c_str(),
				m_secondary->m_nickname.c_str() );

			if(ImGui::Button("Continue"))
				m_state = STATE_WELCOME_2;

			break;

		case STATE_WELCOME_2:
			ImGui::PushFont(m_parent->GetFontPref("Appearance.General.title_font"));
			ImGui::TextUnformatted("Cross-Trigger Cabling");
			ImGui::PopFont();
			ImGui::Separator();

			ImGui::Bullet();
			ImGui::TextWrapped(
				"Connect the trigger output of %s to any channel of %s which may be used as a trigger.",
				m_group->m_primary->m_nickname.c_str(),
				m_secondary->m_nickname.c_str());

			ImGui::Bullet();
			ImGui::TextWrapped(
				"It is suggested to use the external trigger input if one is available, in order "
				"to leave signal inputs free.");

			ImGui::Bullet();
			ImGui::TextWrapped(
				"If %s does not have a trigger output, it cannot be used as the primary of the trigger group.",
				m_group->m_primary->m_nickname.c_str());

			if(ImGui::Button("Continue"))
				m_state = STATE_WELCOME_3;
			break;

		case STATE_WELCOME_3:

			ImGui::PushFont(m_parent->GetFontPref("Appearance.General.title_font"));
			ImGui::TextUnformatted("Cross-Trigger Setup");
			ImGui::PopFont();
			ImGui::Separator();

			ImGui::Bullet();
			ImGui::TextWrapped(
				"Configure %s to trigger on the channel connected to the cross-trigger signal and adjust "
				"the trigger level appropriately.",
				m_secondary->m_nickname.c_str());

			ImGui::Bullet();
			ImGui::TextWrapped(
				"Set the trigger position for both instruments to roughly the midpoint of the acquisition.");

			ImGui::Bullet();
			ImGui::TextWrapped("To test if the cabling and trigger level are correct, "
				"press the \"trigger arm\" button on the toolbar and verify both instruments trigger.");

			if(ImGui::Button("Continue"))
				m_state = STATE_WELCOME_4;
			break;

		case STATE_WELCOME_4:

			ImGui::PushFont(m_parent->GetFontPref("Appearance.General.title_font"));
			ImGui::TextUnformatted("Calibration Signal Setup");
			ImGui::PopFont();
			ImGui::Separator();

			ImGui::Bullet();
			ImGui::TextWrapped(
				"Connect a signal with minimal autocorrelation to one channel of %s and one channel of %s.",
				m_group->m_primary->m_nickname.c_str(),
				m_secondary->m_nickname.c_str() );

			ImGui::Bullet();
			ImGui::TextWrapped(
				"You may use an RF splitter and coaxial cabling, or simply touch a probe from each instrument to a "
				"common point. Note that the delays of this cabling or probes will be included in the calibration."
				);

			ImGui::Bullet();
			ImGui::TextWrapped(
				"Scrambled serial data signals and long-period PRBS patterns are good choices for the calibration signal.");

			ImGui::Bullet();
			ImGui::TextWrapped(
				"Avoid clocks, 8B/10B coded serial data signals, and short PRBS patterns (PRBS7, PRBS9) as these contain "
				"repeating patterns which can lead to false alignments.");

			ImGui::Bullet();
			ImGui::TextWrapped(
				"Configure both channels with appropriate coupling, gain, offset, etc. for the calibration signal.");

			ChannelSelector("Primary", m_group->m_primary, m_primaryStream);
			ChannelSelector("Secondary", m_secondary, m_secondaryStream);

			if(ImGui::Button("Continue"))
				m_state = STATE_WELCOME_5;
			break;

		case STATE_WELCOME_5:

			ImGui::PushFont(m_parent->GetFontPref("Appearance.General.title_font"));
			ImGui::TextUnformatted("Reference Clock Setup");
			ImGui::PopFont();
			ImGui::Separator();

			ImGui::Bullet();
			ImGui::TextWrapped(
				"Connecting a common reference clock to both instruments is strongly recommended.\n"
				"It is possible to operate multi-instrument setups without a shared reference clock,\n"
				"however timebase drift will result in increasingly worse alignment between the waveforms\n"
				"at samples further away from the trigger point.");

			ImGui::Checkbox("Use external reference on primary", &m_useExtRefPrimary);
			ImGui::Checkbox("Use external reference on secondary", &m_useExtRefSecondary);

			if(ImGui::Button("Start"))
			{
				LogTrace("Starting\n");
				m_state = STATE_ACQUIRE;

				//Enable external ref on each if requested
				m_group->m_primary->SetUseExternalRefclk(m_useExtRefPrimary);
				m_secondary->SetUseExternalRefclk(m_useExtRefSecondary);

				//Record the current waveform timestamp on each channel (if any)
				//so we can check if new data has shown up
				{
					shared_lock<shared_mutex> lock(m_session.GetWaveformDataMutex());
					auto data = m_primaryStream.GetData();
					if(data)
					{
						m_lastTriggerTimestamp = data->m_startTimestamp;
						m_lastTriggerFs = data->m_startFemtoseconds;
					}
				}

				//Acquire the first test waveform
				m_group->Arm(TriggerGroup::TRIGGER_TYPE_SINGLE);
			}

			break;

		default:
			DoMainProcessingFlow();
			break;
	}

	return true;
}

void ScopeDeskewWizard::ChannelSelector(const char* name, Oscilloscope* scope, StreamDescriptor& stream)
{
	vector<StreamDescriptor> streams;
	vector<string> names;
	int sel = 0;

	for(size_t i=0; i<scope->GetChannelCount(); i++)
	{
		auto chan = scope->GetChannel(i);

		//Skip it if not enabled (we need to be able to grab data off it)
		if(!scope->CanEnableChannel(i))
			continue;
		if(!scope->IsChannelEnabled(i))
			continue;

		for(size_t j=0; j<chan->GetStreamCount(); j++)
		{
			//only allow compatible channels that make sense to use as trigger sources
			auto stype = chan->GetType(j);
			switch(stype)
			{
				case Stream::STREAM_TYPE_ANALOG:
				case Stream::STREAM_TYPE_DIGITAL:
				case Stream::STREAM_TYPE_TRIGGER:
					break;

				//not usable as a trigger
				default:
					continue;
			}

			StreamDescriptor s(chan, j);

			if(stream == s)
				sel = streams.size();

			streams.push_back(s);
			names.push_back(s.GetName());
		}
	}

	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 15);
	if(Combo(name, names, sel))
		stream = streams[sel];

	//If our stream is null, select the first input
	if(!stream)
		stream = streams[0];
}

void ScopeDeskewWizard::DoMainProcessingFlow()
{
	const int nWaveforms = 10;

	ImGui::PushFont(m_parent->GetFontPref("Appearance.General.title_font"));
	ImGui::TextUnformatted("Calibration Measurements");
	ImGui::PopFont();
	ImGui::Separator();

	//Draw progress table
	ImGuiTableFlags flags =
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_BordersOuter |
		ImGuiTableFlags_BordersV |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingFixedFit |
		ImGuiTableFlags_NoKeepColumnsVisible;

	if(ImGui::BeginTable("groups", 4, flags))
	{
		float width = ImGui::GetFontSize();
		ImGui::TableSetupScrollFreeze(0, 1); //Header row does not scroll
		ImGui::TableSetupColumn("Acquire", ImGuiTableColumnFlags_WidthFixed, 6*width);
		ImGui::TableSetupColumn("Correlate", ImGuiTableColumnFlags_WidthFixed, 6*width);
		ImGui::TableSetupColumn("Skew", ImGuiTableColumnFlags_WidthFixed, 6*width);
		ImGui::TableSetupColumn("Correlation", ImGuiTableColumnFlags_WidthFixed, 8*width);
		ImGui::TableHeadersRow();

		Unit fs(Unit::UNIT_FS);

		//Past measurements
		for(int i=0; i<m_measureCycle; i++)
		{
			ImGui::PushID(i);
			ImGui::TableNextRow(ImGuiTableRowFlags_None);

			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted("Done");

			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted("Done");

			ImGui::TableSetColumnIndex(2);
			ImGui::TextUnformatted(fs.PrettyPrint(m_skews[i]).c_str());

			ImGui::TableSetColumnIndex(3);
			ImGui::TextUnformatted(to_string_sci(m_correlations[i]).c_str());

			ImGui::PopID();
		}

		//Current measurement
		if(m_state != STATE_DONE)
		{
			ImGui::PushID(m_measureCycle);
			ImGui::TableNextRow(ImGuiTableRowFlags_None);

			ImGui::TableSetColumnIndex(0);
			if(m_state == STATE_ACQUIRE)
				ImGui::TextUnformatted("Acquiring");
			else
				ImGui::TextUnformatted("Done");

			ImGui::TableSetColumnIndex(1);
			if(m_state == STATE_CORRELATE)
				ImGui::TextUnformatted("Calculating");
			else
				ImGui::TextUnformatted("Pending");

			ImGui::TableSetColumnIndex(2);
			ImGui::TextUnformatted("--");

			ImGui::TableSetColumnIndex(3);
			ImGui::TextUnformatted("--");

			ImGui::PopID();
		}

		//Future measurements
		for(int i=m_measureCycle+1; i<nWaveforms; i++)
		{
			ImGui::PushID(i);
			ImGui::TableNextRow(ImGuiTableRowFlags_None);

			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted("Pending");

			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted("Pending");

			ImGui::TableSetColumnIndex(2);
			ImGui::TextUnformatted("--");

			ImGui::TableSetColumnIndex(3);
			ImGui::TextUnformatted("--");

			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	switch(m_state)
	{
		case STATE_ACQUIRE:
			{
				shared_lock<shared_mutex> lock(m_session.GetWaveformDataMutex());

				//Make sure we have a waveform
				auto data = m_primaryStream.GetData();
				if(!data)
					return;

				//If it's the same timestamp we're looking at stale data, nothing to do
				if( (m_lastTriggerTimestamp == data->m_startTimestamp) &&
					(m_lastTriggerFs == data->m_startFemtoseconds) )
				{
					return;
				}

				//New measurement! Record the timestamp
				m_lastTriggerTimestamp = data->m_startTimestamp;
				m_lastTriggerFs = data->m_startFemtoseconds;

				//We're now ready to do the correlation
				LogTrace("Acquired waveform %d, starting correlation\n", m_measureCycle);
				StartCorrelation();
				m_state = STATE_CORRELATE;
			}
			break;

		case STATE_CORRELATE:
			{
				//TODO: check if the correlation is done

				m_measureCycle ++;

				//Done with acquisition?
				if(m_measureCycle >= nWaveforms)
				{
					//Calculate median skew
					//(this assumes 10 total acquisitions)
					sort(m_skews.begin(), m_skews.end());
					m_medianSkew = (m_skews[4] + m_skews[5]) / 2;

					m_state = STATE_DONE;
					return;
				}

				//Ready to grab next waveform
				LogTrace("Acquiring next waveform\n");
				m_group->Arm(TriggerGroup::TRIGGER_TYPE_SINGLE);
				m_state = STATE_ACQUIRE;
			}
			break;

		case STATE_DONE:
			{
				Unit fs(Unit::UNIT_FS);
				ImGui::TextWrapped("Calculated skew: %s", fs.PrettyPrint(m_medianSkew).c_str());

				if(ImGui::Button("Apply"))
				{
					m_session.SetDeskew(m_secondary, m_medianSkew);
					m_state = STATE_CLOSE;
				}
			}
			break;

		default:
			break;
	}
}

void ScopeDeskewWizard::StartCorrelation()
{
	auto pri = m_primaryStream.GetData();
	auto sec = m_secondaryStream.GetData();

	auto upri = dynamic_cast<UniformAnalogWaveform*>(pri);
	auto usec = dynamic_cast<UniformAnalogWaveform*>(sec);

	auto spri = dynamic_cast<SparseAnalogWaveform*>(pri);
	auto ssec = dynamic_cast<SparseAnalogWaveform*>(sec);

	//Optimized path (if both waveforms are dense packed)
	if(upri && usec)
	{
		//Fall back to software implementation
		if(!m_gpuCorrelationAvailable)
			DoProcessWaveformUniformUnequalRate(upri, usec);

		//If sample rates are equal we can simplify things a lot
		else if(upri->m_timescale == usec->m_timescale)
			DoProcessWaveformUniformEqualRateVulkan(upri, usec);
		/*
		//Also special-case 2:1 sample rate ratio (primary 2x speed of secondary)
		else if((m_primaryWaveform->m_timescale * 2) == m_secondaryWaveform->m_timescale)
			DoProcessWaveformDensePackedDoubleRateGeneric();
		*/

		//Primary 4x rate of secondary?
		//FIXME: this optimized shader seems to be giving erroneous results ~1ns offset from the true peak
		//unsure if implementation or algorithm bug, but disable it for now
		/*else if((upri->m_timescale * 4) == usec->m_timescale)
			DoProcessWaveformUniform4xRateVulkan(upri, usec);*/

		//Unequal sample rates, more math needed
		else
			DoProcessWaveformUniformUnequalRateVulkan(upri, usec);
	}

	//Fallback path (if at least one waveform is not dense packed)
	else if(spri && ssec)
		DoProcessWaveformSparse(spri, ssec);

	else
	{
		LogError("Mixed sparse and uniform waveforms not implemented\n");
		return;
	}

	//Collect the skew from this round
	int64_t skew = m_bestCorrelationOffset * pri->m_timescale;
	Unit fs(Unit::UNIT_FS);
	LogTrace("Bxest correlation = %f (delta = %" PRId64 " / %s)\n",
		m_bestCorrelation, m_bestCorrelationOffset, fs.PrettyPrint(skew).c_str());

	//If we got a correlation of zero (TODO: why would this be?) then retry
	if(m_bestCorrelation < 1e-8)
		m_measureCycle --;
	else
	{
		m_correlations.push_back(m_bestCorrelation);
		m_skews.push_back(skew);
	}
}

void ScopeDeskewWizard::DoProcessWaveformSparse(SparseAnalogWaveform* ppri, SparseAnalogWaveform* psec)
{
	shared_lock<shared_mutex> lock(m_session.GetWaveformDataMutex());

	//Calculate cross-correlation between the primary and secondary waveforms at up to +/- half the waveform length
	int64_t len = ppri->size();
	size_t slen = psec->size();

	std::mutex cmutex;

	#pragma omp parallel for
	for(int64_t d = -m_maxSkewSamples; d < m_maxSkewSamples; d ++)
	{
		//Convert delta from samples of the primary waveform to femtoseconds
		int64_t deltaFs = ppri->m_timescale * d;

		//Loop over samples in the primary waveform
		//TODO: Can we AVX this?
		ssize_t samplesProcessed = 0;
		size_t isecondary = 0;
		double correlation = 0;
		for(size_t i=0; i<(size_t)len; i++)
		{
			//Timestamp of this sample, in fs
			int64_t start = ppri->m_offsets[i] * ppri->m_timescale + ppri->m_triggerPhase;

			//Target timestamp in the secondary waveform
			int64_t target = start + deltaFs;

			//If off the start of the waveform, skip it
			if(target < 0)
				continue;

			//Skip secondary samples if the current secondary sample ends before the primary sample starts
			bool done = false;
			while( (((psec->m_offsets[isecondary] + psec->m_durations[isecondary]) *
						psec->m_timescale) + psec->m_triggerPhase) < target)
			{
				isecondary ++;

				//If off the end of the waveform, stop
				if(isecondary >= slen)
				{
					done = true;
					break;
				}
			}
			if(done)
				break;

			//Do the actual cross-correlation
			correlation += ppri->m_samples[i] * psec->m_samples[isecondary];
			samplesProcessed ++;
		}

		double normalizedCorrelation = correlation / samplesProcessed;

		//Update correlation
		lock_guard<mutex> lock2(cmutex);
		if(normalizedCorrelation > m_bestCorrelation)
		{
			m_bestCorrelation = normalizedCorrelation;
			m_bestCorrelationOffset = d;
		}
	}
}

/*
void ScopeSyncWizard::DoProcessWaveformDensePackedDoubleRateGeneric()
{
	size_t len = m_primaryWaveform->size();
	size_t slen = m_secondaryWaveform->size();

	std::mutex cmutex;

	int64_t phaseshift = (m_primaryWaveform->m_triggerPhase - m_secondaryWaveform->m_triggerPhase)
		/ m_primaryWaveform->m_timescale;

	float* ppri = dynamic_cast<UniformAnalogWaveform*>(m_primaryWaveform)->m_samples.GetCpuPointer();
	float* psec = dynamic_cast<UniformAnalogWaveform*>(m_secondaryWaveform)->m_samples.GetCpuPointer();

	#pragma omp parallel for
	for(int64_t d = -m_maxSkewSamples; d < m_maxSkewSamples; d ++)
	{
		//Shift by relative trigger phase
		int64_t delta = d + phaseshift;

		size_t end = 2*(slen - delta);
		end = min(end, len);

		//Loop over samples in the primary waveform
		ssize_t samplesProcessed = 0;
		double correlation = 0;
		for(size_t i=0; i<end; i++)
		{
			//If off the start of the waveform, skip it
			if(((int64_t)i + delta) < 0)
				continue;

			uint64_t utarget = ((i  + delta) / 2);

			//Do the actual cross-correlation
			correlation += ppri[i] * psec[utarget];
			samplesProcessed ++;
		}

		double normalizedCorrelation = correlation / samplesProcessed;

		//Update correlation
		lock_guard<mutex> lock(cmutex);
		if(normalizedCorrelation > m_bestCorrelation)
		{
			m_bestCorrelation = normalizedCorrelation;
			m_bestCorrelationOffset = d;
		}
	}
}

void ScopeSyncWizard::DoProcessWaveformDensePackedEqualRateGeneric()
{
	int64_t len = m_primaryWaveform->size();
	size_t slen = m_secondaryWaveform->size();

	std::mutex cmutex;

	int64_t phaseshift =
		(m_primaryWaveform->m_triggerPhase - m_secondaryWaveform->m_triggerPhase) /
		m_primaryWaveform->m_timescale;

	float* ppri = dynamic_cast<UniformAnalogWaveform*>(m_primaryWaveform)->m_samples.GetCpuPointer();
	float* psec = dynamic_cast<UniformAnalogWaveform*>(m_secondaryWaveform)->m_samples.GetCpuPointer();

	#pragma omp parallel for
	for(int64_t d = -m_maxSkewSamples; d < m_maxSkewSamples; d ++)
	{
		//Shift by relative trigger phase
		int64_t delta = d + phaseshift;

		//Loop over samples in the primary waveform
		ssize_t samplesProcessed = 0;
		double correlation = 0;
		for(size_t i=0; i<(size_t)len; i++)
		{
			//Target timestamp in the secondary waveform
			int64_t target = i + delta;

			//If off the start of the waveform, skip it
			if(target < 0)
				continue;

			//If off the end of the waveform, stop
			uint64_t utarget = target;
			if(utarget >= slen)
				break;

			//Do the actual cross-correlation
			correlation += ppri[i] * psec[utarget];
			samplesProcessed ++;
		}

		double normalizedCorrelation = correlation / samplesProcessed;

		//Update correlation
		lock_guard<mutex> lock(cmutex);
		if(normalizedCorrelation > m_bestCorrelation)
		{
			m_bestCorrelation = normalizedCorrelation;
			m_bestCorrelationOffset = d;
		}
	}
}
*/
void ScopeDeskewWizard::DoProcessWaveformUniformUnequalRate(UniformAnalogWaveform* ppri, UniformAnalogWaveform* psec)
{
	shared_lock<shared_mutex> lock(m_session.GetWaveformDataMutex());

	double start = GetTime();

	int64_t len = ppri->size();
	size_t slen = psec->size();

	std::mutex cmutex;

	#pragma omp parallel for
	for(int64_t d = -m_maxSkewSamples; d < m_maxSkewSamples; d ++)
	{
		//Convert delta from samples of the primary waveform to femtoseconds
		int64_t deltaFs = ppri->m_timescale * d;

		//Shift by relative trigger phase
		deltaFs += (ppri->m_triggerPhase - psec->m_triggerPhase);

		//Loop over samples in the primary waveform
		ssize_t samplesProcessed = 0;
		size_t isecondary = 0;
		double correlation = 0;
		for(size_t i=0; i<(size_t)len; i++)
		{
			//Target timestamp in the secondary waveform
			int64_t target = i * ppri->m_timescale + deltaFs;

			//If off the start of the waveform, skip it
			if(target < 0)
				continue;

			uint64_t utarget = target;

			//Skip secondary samples if the current secondary sample ends before the primary sample starts
			bool done = false;
			while( static_cast<uint64_t>((isecondary + 1) *	psec->m_timescale) < utarget)
			{
				isecondary ++;

				//If off the end of the waveform, stop
				if(isecondary >= slen)
				{
					done = true;
					break;
				}
			}
			if(done)
				break;

			//Do the actual cross-correlation
			correlation += ppri->m_samples[i] * psec->m_samples[isecondary];
			samplesProcessed ++;
		}

		double normalizedCorrelation = correlation / samplesProcessed;

		//Update correlation
		lock_guard<mutex> lock2(cmutex);
		if(normalizedCorrelation > m_bestCorrelation)
		{
			m_bestCorrelation = normalizedCorrelation;
			m_bestCorrelationOffset = d;
		}
	}

	double dt = GetTime() - start;
	LogTrace("Correlation evaluated in %.3f sec\n", dt);
}

void ScopeDeskewWizard::DoProcessWaveformUniform4xRateVulkan(
	UniformAnalogWaveform* ppri, UniformAnalogWaveform* psec)
{
	auto start = GetTime();

	m_cmdBuf.reset();
	m_cmdBuf.begin({});

	ppri->m_samples.PrepareForGpuAccessNonblocking(false, m_cmdBuf);
	psec->m_samples.PrepareForGpuAccessNonblocking(false, m_cmdBuf);
	m_corrOut.PrepareForGpuAccessNonblocking(true, m_cmdBuf);

	//sync in case transfer happened in another thread
	AcceleratorBuffer<float>::HostToDeviceTransferMemoryBarrier(m_cmdBuf);

	UniformCrossCorrelateArgs args(ppri, psec, m_maxSkewSamples);
	m_uniform4xRatePipeline->BindBufferNonblocking(0, m_corrOut, m_cmdBuf, true);
	m_uniform4xRatePipeline->BindBufferNonblocking(1, ppri->m_samples, m_cmdBuf);
	m_uniform4xRatePipeline->BindBufferNonblocking(2, psec->m_samples, m_cmdBuf);
	m_uniform4xRatePipeline->Dispatch(m_cmdBuf, args, GetComputeBlockCount(2*m_maxSkewSamples, 64));

	m_cmdBuf.end();
	m_queue->SubmitAndBlock(m_cmdBuf);

	PostprocessVulkanCorrelation();

	auto dt = GetTime() - start;
	LogTrace("GPU correlation evaluated in %.3f sec\n", dt);
}

void ScopeDeskewWizard::DoProcessWaveformUniformUnequalRateVulkan(
	UniformAnalogWaveform* ppri, UniformAnalogWaveform* psec)
{
	auto start = GetTime();

	m_cmdBuf.reset();
	m_cmdBuf.begin({});

	ppri->m_samples.PrepareForGpuAccessNonblocking(false, m_cmdBuf);
	psec->m_samples.PrepareForGpuAccessNonblocking(false, m_cmdBuf);
	m_corrOut.PrepareForGpuAccessNonblocking(true, m_cmdBuf);

	//sync in case transfer happened in another thread
	AcceleratorBuffer<float>::HostToDeviceTransferMemoryBarrier(m_cmdBuf);

	UniformCrossCorrelateArgs args(ppri, psec, m_maxSkewSamples);
	m_uniformUnequalRatePipeline->BindBufferNonblocking(0, m_corrOut, m_cmdBuf, true);
	m_uniformUnequalRatePipeline->BindBufferNonblocking(1, ppri->m_samples, m_cmdBuf);
	m_uniformUnequalRatePipeline->BindBufferNonblocking(2, psec->m_samples, m_cmdBuf);
	m_uniformUnequalRatePipeline->Dispatch(m_cmdBuf, args, GetComputeBlockCount(2*m_maxSkewSamples, 64));

	m_cmdBuf.end();
	m_queue->SubmitAndBlock(m_cmdBuf);

	PostprocessVulkanCorrelation();

	auto dt = GetTime() - start;
	LogTrace("GPU correlation evaluated in %.3f sec\n", dt);
}

void ScopeDeskewWizard::DoProcessWaveformUniformEqualRateVulkan(
	UniformAnalogWaveform* ppri, UniformAnalogWaveform* psec)
{
	auto start = GetTime();

	m_cmdBuf.reset();
	m_cmdBuf.begin({});

	ppri->m_samples.PrepareForGpuAccessNonblocking(false, m_cmdBuf);
	psec->m_samples.PrepareForGpuAccessNonblocking(false, m_cmdBuf);
	m_corrOut.PrepareForGpuAccessNonblocking(true, m_cmdBuf);

	//sync in case transfer happened in another thread
	AcceleratorBuffer<float>::HostToDeviceTransferMemoryBarrier(m_cmdBuf);

	UniformCrossCorrelateArgs args(ppri, psec, m_maxSkewSamples);
	m_uniformEqualRatePipeline->BindBufferNonblocking(0, m_corrOut, m_cmdBuf, true);
	m_uniformEqualRatePipeline->BindBufferNonblocking(1, ppri->m_samples, m_cmdBuf);
	m_uniformEqualRatePipeline->BindBufferNonblocking(2, psec->m_samples, m_cmdBuf);
	m_uniformEqualRatePipeline->Dispatch(m_cmdBuf, args, GetComputeBlockCount(2*m_maxSkewSamples, 64));

	m_cmdBuf.end();
	m_queue->SubmitAndBlock(m_cmdBuf);

	PostprocessVulkanCorrelation();

	auto dt = GetTime() - start;
	LogTrace("GPU correlation evaluated in %.3f sec\n", dt);
}

void ScopeDeskewWizard::PostprocessVulkanCorrelation()
{
	m_corrOut.PrepareForCpuAccess();	//todo make this part of the same queue

	//Crunch results
	int64_t bestOffset = 0;
	double bestCorr = 0;
	for(int64_t i=0; i<2*m_maxSkewSamples; i++)
	{
		auto f = m_corrOut[i];
		if(f > bestCorr)
		{
			bestCorr = f;
			bestOffset = i - m_maxSkewSamples;
		}
	}

	m_bestCorrelation = bestCorr;
	m_bestCorrelationOffset = bestOffset;
}
