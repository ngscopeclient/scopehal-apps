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
#ifndef ngscopeclient_h
#define ngscopeclient_h

#include "../scopehal/scopehal.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#include <implot.h>
#pragma GCC diagnostic pop

#include <atomic>

#include "RFSignalGeneratorState.h"
#include "PowerSupplyState.h"
#include "MultimeterState.h"
#include "GuiLogSink.h"

class RFSignalGeneratorThreadArgs
{
public:
	RFSignalGeneratorThreadArgs(SCPIRFSignalGenerator* p, std::atomic<bool>* s, std::shared_ptr<RFSignalGeneratorState> st)
	: gen(p)
	, shuttingDown(s)
	, state(st)
	{}

	SCPIRFSignalGenerator* gen;
	std::atomic<bool>* shuttingDown;
	std::shared_ptr<RFSignalGeneratorState> state;
};

class PowerSupplyThreadArgs
{
public:
	PowerSupplyThreadArgs(SCPIPowerSupply* p, std::atomic<bool>* s, std::shared_ptr<PowerSupplyState> st)
	: psu(p)
	, shuttingDown(s)
	, state(st)
	{}

	SCPIPowerSupply* psu;
	std::atomic<bool>* shuttingDown;
	std::shared_ptr<PowerSupplyState> state;
};

class MultimeterThreadArgs
{
public:
	MultimeterThreadArgs(SCPIMultimeter* m, std::atomic<bool>* s, std::shared_ptr<MultimeterState> st)
	: meter(m)
	, shuttingDown(s)
	, state(st)
	{}

	SCPIMultimeter* meter;
	std::atomic<bool>* shuttingDown;
	std::shared_ptr<MultimeterState> state;
};

void ScopeThread(Oscilloscope* scope, std::atomic<bool>* shuttingDown);
void PowerSupplyThread(PowerSupplyThreadArgs args);
void MultimeterThread(MultimeterThreadArgs args);
void RFSignalGeneratorThread(RFSignalGeneratorThreadArgs args);

#endif
