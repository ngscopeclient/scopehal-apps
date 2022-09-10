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
	@brief Declaration of WaveformArea
 */
#ifndef WaveformArea_h
#define WaveformArea_h

/**
	@brief Placeholder for a single channel being displayed within a WaveformArea
 */
class DisplayedChannel
{
public:
	DisplayedChannel(const std::string& name)
		: m_name(name)
	{}

	const std::string& GetName()
	{ return m_name; }

protected:
	std::string m_name;
};

/**
	@brief A WaveformArea is a plot that displays one or more OscilloscopeChannel's worth of data

	WaveformArea's auto resize, and will collectively fill the entire client area of their parent window.
 */
class WaveformArea
{
public:
	WaveformArea();
	virtual ~WaveformArea();

	void Render(int iArea, int numAreas, ImVec2 clientArea);

protected:
	void DraggableButton(std::shared_ptr<DisplayedChannel> chan);

	void DropArea(const std::string& name, ImVec2 start, ImVec2 size);

	/**
		@brief The channels currently living within this WaveformArea

		TODO: make this a FlowGraphNode and just hook up inputs
	 */
	std::vector<std::shared_ptr<DisplayedChannel>> m_displayedChannels;
};

#endif


