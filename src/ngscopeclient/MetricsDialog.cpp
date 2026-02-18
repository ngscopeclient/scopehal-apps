/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2026 Andrew D. Zonenberg and contributors                                                         *
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
	@brief Implementation of MetricsDialog
 */

#include "ngscopeclient.h"
#include "MetricsDialog.h"
#include "Session.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

MetricsDialog::MetricsDialog(Session* session)
	: Dialog("Performance Metrics", "Metrics", ImVec2(300, 400), session)
{
	m_displayRefreshRate = 0;

	auto mon = glfwGetPrimaryMonitor();
	if(mon)
	{
		auto mode = glfwGetVideoMode(mon);
		if(mode)
			m_displayRefreshRate = mode->refreshRate;
	}
}

MetricsDialog::~MetricsDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool MetricsDialog::DoRender()
{
	Unit counts(Unit::UNIT_COUNTS);
	Unit fs(Unit::UNIT_FS);
	Unit hz(Unit::UNIT_HZ);

	string str;

	float width = ImGui::GetFontSize() * 7;

	if(ImGui::CollapsingHeader("Rendering"))
	{
		ImGui::BeginDisabled();
			str = hz.PrettyPrint(ImGui::GetIO().Framerate);
			ImGui::SetNextItemWidth(width);
			ImGui::InputText("Framerate", &str);
		ImGui::EndDisabled();

		HelpMarker(
			"Rate at which the user interface is being redrawn.\n\n"
			"Capped at display refresh rate by vsync.\n"
			"If it drops significantly lower, rendering is taking too long or the GUI thread is bogging down.");

		ImGui::BeginDisabled();
			str = hz.PrettyPrint(m_displayRefreshRate);
			ImGui::SetNextItemWidth(width);
			ImGui::InputText("Refresh rate", &str);
		ImGui::EndDisabled();

		HelpMarker(
			"Refresh rate for your monitor. Framerate should ideally be very close to this.");

		ImGui::BeginDisabled();
			str = fs.PrettyPrint(m_session->GetLastWaveformRenderTime());
			ImGui::SetNextItemWidth(width);
			ImGui::InputText("Rasterize time", &str);
		ImGui::EndDisabled();

		HelpMarker(
			"Most recent execution time for the waveform rasterizing compute shader (total across all waveforms).\n\n"
			"This shader runs every time a waveform is panned, zoomed, or updated and does not "
			"necessarily execute every frame. It runs asynchronously and is not locked to the display framerate."
			);

		ImGui::BeginDisabled();
			str = fs.PrettyPrint(m_session->GetToneMapTime());
			ImGui::SetNextItemWidth(width);
			ImGui::InputText("Tone map time", &str);
		ImGui::EndDisabled();

		HelpMarker(
			"Most recent execution time for the tone mapping compute shader (total across all waveforms).\n\n"
			"This shader runs every time a waveform is re-rasterized or display color ramp settings are changed, and "
			"does not necessarily execute every frame. When needed, it runs synchronously during frame rendering."
			);


		ImGui::BeginDisabled();
			str = counts.PrettyPrint(ImGui::GetIO().MetricsRenderVertices);
			ImGui::SetNextItemWidth(width);
			ImGui::InputText("Vertices", &str);
		ImGui::EndDisabled();

		HelpMarker(
			"Total number of vertex buffer entries in the last frame\n\n"
			"Waveform samples are drawn by a compute shader and not included in this total");

		ImGui::BeginDisabled();
			str = counts.PrettyPrint(ImGui::GetIO().MetricsRenderIndices);
			ImGui::SetNextItemWidth(width);
			ImGui::InputText("Indices", &str);
		ImGui::EndDisabled();

		HelpMarker(
			"Total number of index buffer entries in the last frame\n\n"
			"Waveform samples are drawn by a compute shader and not included in this total");
	}

	if(ImGui::CollapsingHeader("Filter graph"))
	{
		ImGui::BeginDisabled();
			str = counts.PrettyPrint(m_session->GetFilterCount());
			ImGui::SetNextItemWidth(width);
			ImGui::InputText("Total filters", &str);
		ImGui::EndDisabled();

		HelpMarker("Number of filter blocks currently in existence");

		ImGui::BeginDisabled();
			str = fs.PrettyPrint(m_session->GetFilterGraphExecTime());
			ImGui::SetNextItemWidth(width);
			ImGui::InputText("Exec time", &str);
		ImGui::EndDisabled();

		HelpMarker("Update time for the last evaluation of the filter graph");
	}

	if(ImGui::CollapsingHeader("Acquisition"))
	{
		ImGui::BeginDisabled();
			str = hz.PrettyPrint(m_session->GetWaveformDownloadRate());
			ImGui::SetNextItemWidth(width);
			ImGui::InputText("Waveform rate", &str);
		ImGui::EndDisabled();

		HelpMarker(
			"Rate at which waveforms are being retrieved from the queue and processed.\n\n"
			"This is currently capped at the display framerate.\n"
			"If it drops below the framerate, your instrument, filter graph execution, or waveform rendering "
			"are likely the bottleneck."
			);

		//Category for each scope
		auto scopes = m_session->GetScopes();
		for(auto s : scopes)
		{
			if(ImGui::TreeNode(s->m_nickname.c_str()))
			{
				ImGui::BeginDisabled();
					str = counts.PrettyPrint(s->GetPendingWaveformCount());
					ImGui::SetNextItemWidth(width);
					ImGui::InputText("Pending waveforms", &str);
				ImGui::EndDisabled();

				HelpMarker(
					"Number of waveforms queued for processing.\n\n"
					"This value should normally be 0 or 1, and is capped at 5.\n"
					"If it is consistently at or near 5, waveform processing and/or rendering is unable to keep "
					"up with the instrument."
					);

				ImGui::TreePop();
			}
		}
	}

	if(ImGui::CollapsingHeader("Buffers"))
	{
		int64_t hostDeviceCopiesSkipped = AcceleratorBufferPerformanceCounters::m_hostDeviceCopiesSkipped;
		int64_t hostDeviceCopiesBlocking = AcceleratorBufferPerformanceCounters::m_hostDeviceCopiesBlocking;
		int64_t hostDeviceCopiesNonBlocking = AcceleratorBufferPerformanceCounters::m_hostDeviceCopiesNonBlocking;
		int64_t hostDeviceCopiesRequested =
			hostDeviceCopiesSkipped + hostDeviceCopiesBlocking + hostDeviceCopiesNonBlocking;

		int64_t deviceHostCopiesSkipped = AcceleratorBufferPerformanceCounters::m_deviceHostCopiesSkipped;
		int64_t deviceHostCopiesBlocking = AcceleratorBufferPerformanceCounters::m_deviceHostCopiesBlocking;
		int64_t deviceHostCopiesNonBlocking = AcceleratorBufferPerformanceCounters::m_deviceHostCopiesNonBlocking;
		int64_t deviceHostCopiesRequested =
			deviceHostCopiesSkipped + deviceHostCopiesBlocking + deviceHostCopiesNonBlocking;

		int64_t deviceDeviceCopiesSkipped = AcceleratorBufferPerformanceCounters::m_deviceDeviceCopiesSkipped;
		int64_t deviceDeviceCopiesBlocking = AcceleratorBufferPerformanceCounters::m_deviceDeviceCopiesBlocking;
		int64_t deviceDeviceCopiesNonBlocking = AcceleratorBufferPerformanceCounters::m_deviceDeviceCopiesNonBlocking;
		int64_t deviceDeviceCopiesRequested =
			deviceDeviceCopiesSkipped + deviceDeviceCopiesBlocking + deviceDeviceCopiesNonBlocking;

		if(ImGui::TreeNodeEx("Transfers", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if(ImGui::TreeNodeEx("CPU-GPU", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::BeginDisabled();
					str = counts.PrettyPrint(hostDeviceCopiesRequested);
					ImGui::SetNextItemWidth(width);
					ImGui::InputText("Requested", &str);
					HelpMarker("Total number of times a data buffer was requested by a GPU shader");

					str = FormatValueWithPercentage(hostDeviceCopiesSkipped, hostDeviceCopiesRequested);
					ImGui::SetNextItemWidth(width);
					ImGui::InputText("Avoided", &str);
					HelpMarker("Copies which did not happen because the GPU already had up-to-date data");

					str = FormatValueWithPercentage(hostDeviceCopiesBlocking, hostDeviceCopiesRequested);
					ImGui::SetNextItemWidth(width);
					ImGui::InputText("Blocking", &str);
					HelpMarker("Blocking transfers using the implicit global transfer command buffer");

					str = FormatValueWithPercentage(hostDeviceCopiesNonBlocking, hostDeviceCopiesRequested);
					ImGui::SetNextItemWidth(width);
					ImGui::InputText("Non-blocking", &str);
					HelpMarker("Nonblocking transfers using an explicit command buffer");

				ImGui::EndDisabled();

				ImGui::TreePop();
			}

			if(ImGui::TreeNodeEx("GPU-CPU", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::BeginDisabled();
					str = counts.PrettyPrint(deviceHostCopiesRequested);
					ImGui::SetNextItemWidth(width);
					ImGui::InputText("Requested", &str);
					HelpMarker("Total number of times a data buffer was requested by CPU-side code");

					str = FormatValueWithPercentage(deviceHostCopiesSkipped, deviceHostCopiesRequested);
					ImGui::SetNextItemWidth(width);
					ImGui::InputText("Avoided", &str);
					HelpMarker("Copies which did not happen because the CPU already had up-to-date data");

					str = FormatValueWithPercentage(deviceHostCopiesBlocking, deviceHostCopiesRequested);
					ImGui::SetNextItemWidth(width);
					ImGui::InputText("Blocking", &str);
					HelpMarker("Blocking transfers using the implicit global transfer command buffer");

					str = FormatValueWithPercentage(deviceHostCopiesNonBlocking, deviceHostCopiesRequested);
					ImGui::SetNextItemWidth(width);
					ImGui::InputText("Non-blocking", &str);
					HelpMarker("Nonblocking transfers using an explicit command buffer");

				ImGui::EndDisabled();

				ImGui::TreePop();
			}

			if(ImGui::TreeNodeEx("GPU-GPU", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::BeginDisabled();
					str = counts.PrettyPrint(deviceDeviceCopiesRequested);
					ImGui::SetNextItemWidth(width);
					ImGui::InputText("Requested", &str);
					HelpMarker("Total number of times a data buffer was copied from one GPU location to another");

					str = FormatValueWithPercentage(deviceDeviceCopiesSkipped, deviceDeviceCopiesRequested);
					ImGui::SetNextItemWidth(width);
					ImGui::InputText("Avoided", &str);
					HelpMarker("Copies which did not happen because the old buffer was empty");

					str = FormatValueWithPercentage(deviceDeviceCopiesBlocking, deviceDeviceCopiesRequested);
					ImGui::SetNextItemWidth(width);
					ImGui::InputText("Blocking", &str);
					HelpMarker("Blocking transfers using the implicit global transfer command buffer");

					str = FormatValueWithPercentage(deviceDeviceCopiesNonBlocking, deviceDeviceCopiesRequested);
					ImGui::SetNextItemWidth(width);
					ImGui::InputText("Non-blocking", &str);
					HelpMarker("Nonblocking transfers using an explicit command buffer");

				ImGui::EndDisabled();

				ImGui::TreePop();
			}

			ImGui::TreePop();
		}
	}

	if(ImGui::CollapsingHeader("Memory"))
	{
		Unit bytes(Unit::UNIT_BYTES);
		Unit pct(Unit::UNIT_PERCENT);

		//Only show this section if available
		if(g_hasMemoryBudget)
		{
			//TODO: host OS (un-pinned) memory usage too?
			auto properties = g_vkComputePhysicalDevice->getMemoryProperties2<
				vk::PhysicalDeviceMemoryProperties2,
				vk::PhysicalDeviceMemoryBudgetPropertiesEXT>();
			auto membudget = std::get<1>(properties);

			auto pinnedUsage = membudget.heapUsage[g_vkPinnedMemoryHeap];
			auto pinnedBudget = membudget.heapBudget[g_vkPinnedMemoryHeap];

			std::string pinnedNodeName, pinnedHelpMarkerBudgetText, pinnedHelpMarkerUsageText;
			if(g_vulkanDeviceHasUnifiedMemory)
			{
				pinnedNodeName = "Unified";
				pinnedHelpMarkerBudgetText = "Amount of unified RAM available for use by ngscopeclient.\n\n"
					"This is your total RAM minus memory which is in use by the OS or other applications.";
				pinnedHelpMarkerUsageText = "Amount of unified RAM currently in use by ngscopeclient.";
			}
			else
			{
				pinnedNodeName = "Pinned";
				pinnedHelpMarkerBudgetText = "Amount of pinned CPU-side RAM available for use by ngscopeclient.\n\n"
					"This is your total RAM minus memory which cannot be pinned for PCIe access,\n"
					"or is in use by the OS or other applications.";
				pinnedHelpMarkerUsageText = "Amount of pinned CPU-side RAM currently in use by ngscopeclient.";
			}

			if(ImGui::TreeNodeEx(pinnedNodeName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::BeginDisabled();
					str = bytes.PrettyPrint(pinnedBudget, 4);
					ImGui::SetNextItemWidth(10 * ImGui::GetFontSize());
					ImGui::InputText("Budget", &str);
				ImGui::EndDisabled();

				HelpMarker(pinnedHelpMarkerBudgetText);

				ImGui::BeginDisabled();
					str = bytes.PrettyPrint(pinnedUsage, 4) +
						" (" + pct.PrettyPrint(pinnedUsage * 1.0f / pinnedBudget, 4) + ")";
					ImGui::SetNextItemWidth(10 * ImGui::GetFontSize());
					ImGui::InputText("Usage", &str);
				ImGui::EndDisabled();

				HelpMarker(pinnedHelpMarkerUsageText);

				ImGui::TreePop();
			}

			if(!g_vulkanDeviceHasUnifiedMemory && ImGui::TreeNodeEx("Local", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto localUsage = membudget.heapUsage[g_vkLocalMemoryHeap];
				auto localBudget = membudget.heapBudget[g_vkLocalMemoryHeap];

				ImGui::BeginDisabled();
					str = bytes.PrettyPrint(localBudget, 4);
					ImGui::SetNextItemWidth(10 * ImGui::GetFontSize());
					ImGui::InputText("Budget", &str);
				ImGui::EndDisabled();

				HelpMarker(
					"Amount of GPU-side RAM available for use by ngscopeclient.\n\n"
					"This is your total video RAM minus memory which is in use by the OS or other applications."
					);

				ImGui::BeginDisabled();
					str = bytes.PrettyPrint(localUsage, 4) +
						" (" + pct.PrettyPrint(localUsage * 1.0f / localBudget, 4) + ")";
					ImGui::SetNextItemWidth(10 * ImGui::GetFontSize());
					ImGui::InputText("Usage", &str);
				ImGui::EndDisabled();

				HelpMarker("Amount of GPU-side RAM currently in use by ngscopeclient.");

				ImGui::TreePop();
			}

		}

		if(ImGui::TreeNodeEx("Scratch pool", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::BeginDisabled();
				str = bytes.PrettyPrint(ScratchBufferManager::GetTotalSize(), 4);
				ImGui::SetNextItemWidth(10 * ImGui::GetFontSize());
				ImGui::InputText("Total", &str);
			ImGui::EndDisabled();

			if(ImGui::TreeNodeEx("GPU", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if(ImGui::TreeNodeEx("Waveform sized", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::BeginDisabled();
						str = bytes.PrettyPrint(ScratchBufferManager::GetPoolSize(
							ScratchBufferManager::U8_GPU_WAVEFORM), 4);
						ImGui::SetNextItemWidth(10 * ImGui::GetFontSize());
						ImGui::InputText("uint8", &str);
					ImGui::EndDisabled();

					ImGui::BeginDisabled();
						str = bytes.PrettyPrint(ScratchBufferManager::GetPoolSize(
							ScratchBufferManager::U32_GPU_WAVEFORM), 4);
						ImGui::SetNextItemWidth(10 * ImGui::GetFontSize());
						ImGui::InputText("uint32", &str);
					ImGui::EndDisabled();

					ImGui::BeginDisabled();
						str = bytes.PrettyPrint(ScratchBufferManager::GetPoolSize(
							ScratchBufferManager::F32_GPU_WAVEFORM), 4);
						ImGui::SetNextItemWidth(10 * ImGui::GetFontSize());
						ImGui::InputText("float32", &str);
					ImGui::EndDisabled();

					ImGui::BeginDisabled();
						str = bytes.PrettyPrint(ScratchBufferManager::GetPoolSize(
							ScratchBufferManager::I64_GPU_WAVEFORM), 4);
						ImGui::SetNextItemWidth(10 * ImGui::GetFontSize());
						ImGui::InputText("int64", &str);
					ImGui::EndDisabled();

					ImGui::TreePop();
				}

				if(ImGui::TreeNodeEx("Small", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::BeginDisabled();
						str = bytes.PrettyPrint(ScratchBufferManager::GetPoolSize(
							ScratchBufferManager::I64_GPU_SMALL), 4);
						ImGui::SetNextItemWidth(10 * ImGui::GetFontSize());
						ImGui::InputText("int64", &str);
					ImGui::EndDisabled();

					ImGui::TreePop();
				}

				ImGui::TreePop();
			}

			ImGui::TreePop();
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers
