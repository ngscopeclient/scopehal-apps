/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg and contributors                                                         *
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
	@brief Implementation of MemoryLeakerDialog
 */

#include "ngscopeclient.h"
#include "MainWindow.h"
#include "MemoryLeakerDialog.h"

using namespace std;
using namespace std::chrono_literals;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

MemoryLeakerDialog::MemoryLeakerDialog(MainWindow* parent)
	: Dialog(
		"Memory Leaker",
		string("Memory Leaker ") + to_string(reinterpret_cast<uintptr_t>(this)),
		ImVec2(500, 300))
	, m_parent(parent)
	, m_deviceMemoryString("0 kB")
	, m_deviceMemoryUsage(0)
	, m_hostMemoryString("0 kB")
	, m_hostMemoryUsage(0)
{
	m_deviceMemoryBuffer.SetGpuAccessHint(AcceleratorBuffer<uint8_t>::HINT_LIKELY);
	m_deviceMemoryBuffer.SetCpuAccessHint(AcceleratorBuffer<uint8_t>::HINT_NEVER);

	m_hostMemoryBuffer.SetGpuAccessHint(AcceleratorBuffer<uint8_t>::HINT_UNLIKELY);
	m_hostMemoryBuffer.SetCpuAccessHint(AcceleratorBuffer<uint8_t>::HINT_LIKELY);
}

MemoryLeakerDialog::~MemoryLeakerDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool MemoryLeakerDialog::DoRender()
{
	ImGui::TextWrapped(
		"This dialog allocates a configurable amount of host and/or device memory "
		"to allow testing of ngscopeclient under memory pressure\n\n"
		"All allocated memory will be freed when the dialog is closed.\n\n"
		"At most 4GB may be allocated by one dialog instance, but several can be spawned.");

	if(Dialog::UnitInputWithImplicitApply(
		"Device Memory",
		m_deviceMemoryString,
		m_deviceMemoryUsage,
		Unit(Unit::UNIT_BYTES)))
	{
		m_deviceMemoryBuffer.resize(m_deviceMemoryUsage);
	}

	if(Dialog::UnitInputWithImplicitApply(
		"Host Memory",
		m_hostMemoryString,
		m_hostMemoryUsage,
		Unit(Unit::UNIT_BYTES)))
	{
		m_hostMemoryBuffer.resize(m_hostMemoryUsage);
	}

	return true;
}
