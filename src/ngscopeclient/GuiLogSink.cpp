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
	@brief Implementation of GuiLogSink
 */
#include "ngscopeclient.h"
#include "GuiLogSink.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

GuiLogSink::GuiLogSink(Severity min_severity)
	: LogSink(min_severity)
{

}

GuiLogSink::~GuiLogSink()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Logging

void GuiLogSink::Clear()
{
	m_lines.clear();
}

void GuiLogSink::Log(Severity severity, const string &msg)
{
	if(severity > m_min_severity)
		return;

	//Blank lines get special handling
	if(msg == "\n")
	{
		m_lines.push_back("");
		return;
	}

	auto indent = GetIndentString();

	//No newline? Append to existing buffer
	if(msg.find('\n') == string::npos)
	{
		if(m_unbufferedLine.empty())
			m_unbufferedLine += indent;
		m_unbufferedLine += msg;
		return;
	}

	//One or more newlines? Split and process it
	auto vec = explode(msg, '\n');
	auto len = vec.size();
	for(size_t i=0; i<len; i++)
	{
		//Blank line at end of buffer? Special handling
		if( (i+1 == len) && vec[i].empty())
			break;

		//If unbuffered line is present, append to it
		if(!m_unbufferedLine.empty())
		{
			m_lines.push_back(m_unbufferedLine + vec[i]);
			m_unbufferedLine = "";
		}

		//Otherwise append it
		else
			m_lines.push_back(indent + vec[i]);
	}
}

void GuiLogSink::Log(Severity severity, const char *format, va_list va)
{
	if(severity > m_min_severity)
		return;

	Log(severity, vstrprintf(format, va));
}
