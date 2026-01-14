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
#ifndef ngscopeclient_h
#define ngscopeclient_h

#include "../scopehal/scopehal.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "ImGuiDisabler.h"

#include <atomic>

#include "OscilloscopeState.h"
#include "BERTState.h"
#include "PowerSupplyState.h"
#include "FunctionGeneratorState.h"
#include "MultimeterState.h"
#include "LoadState.h"
#include "GuiLogSink.h"
#include "Event.h"

class Session;

class InstrumentThreadArgs
{
public:
	InstrumentThreadArgs(std::shared_ptr<SCPIInstrument> p, Session* sess)
	: inst(p)
	, session(sess)
	{}

	std::shared_ptr<SCPIInstrument> inst;
	std::atomic<bool>* shuttingDown;
	Session* session;

	//Additional per-instrument-type state we can add
	std::shared_ptr<OscilloscopeState> oscilloscopestate;
	std::shared_ptr<LoadState> loadstate;
	std::shared_ptr<MultimeterState> meterstate;
	std::shared_ptr<BERTState> bertstate;
	std::shared_ptr<PowerSupplyState> psustate;
	std::shared_ptr<FunctionGeneratorState> awgstate;
};

void InstrumentThread(InstrumentThreadArgs args);
void WaveformThread(Session* session, std::atomic<bool>* shuttingDown);

void RightJustifiedText(const std::string& str);

bool RectIntersect(ImVec2 posA, ImVec2 sizeA, ImVec2 posB, ImVec2 sizeB);
bool RectContains(ImVec2 posA, ImVec2 sizeA, ImVec2 posB, ImVec2 sizeB);

#endif
