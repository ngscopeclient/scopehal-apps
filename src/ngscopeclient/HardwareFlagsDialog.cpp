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
	@brief Implementation of HardwareFlagsDialog
 */

#include "ngscopeclient.h"
#include "HardwareFlagsDialog.h"
#include "Session.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

HardwareFlagsDialog::HardwareFlagsDialog()
	: Dialog("Hardware Flags", "Hardware flags", ImVec2(600, 400))
{

}

HardwareFlagsDialog::~HardwareFlagsDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool HardwareFlagsDialog::DoRender()
{
	ImGui::Text("This dialog allows you to override hardware feature flag detection.");
	ImGui::TextWrapped(
		"It is mostly intended for developers to test fallback versions of accelerated functionality by "
		"disabling a feature that the hardware actually supports. There are no guardrails! "
		"Enabling a feature your CPU or Vulkan device does not support will probably crash ngscopeclient");

	if(ImGui::CollapsingHeader("CPU"))
	{
		ImGui::Checkbox("FMA", &g_hasFMA);
		ImGui::Checkbox("AVX2", &g_hasAvx2);
		ImGui::Checkbox("AVX512F", &g_hasAvx512F);
		ImGui::Checkbox("AVX512VL", &g_hasAvx512VL);
		ImGui::Checkbox("AVX512DQ", &g_hasAvx512DQ);
	}

	if(ImGui::CollapsingHeader("GPU"))
	{
		ImGui::Checkbox("Legacy GPU filter enable", &g_gpuFilterEnabled);
		ImGui::Checkbox("Shader float64", &g_hasShaderFloat64);
		ImGui::Checkbox("Shader int64", &g_hasShaderInt64);
		ImGui::Checkbox("Shader atomic int64", &g_hasShaderAtomicInt64);
		ImGui::Checkbox("Shader int16", &g_hasShaderInt16);
		ImGui::Checkbox("Shader int8", &g_hasShaderInt8);
		ImGui::Checkbox("Shader atomic float", &g_hasShaderAtomicFloat);
		ImGui::Checkbox("Debug utils", &g_hasDebugUtils);
		ImGui::Checkbox("Memory budget", &g_hasMemoryBudget);
		ImGui::Checkbox("Push descriptor", &g_hasPushDescriptor);
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers
