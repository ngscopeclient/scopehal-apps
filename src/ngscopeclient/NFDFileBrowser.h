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
	@brief Declaration of NFDFileBrowser
 */
#ifndef NFDFileBrowser_h
#define NFDFileBrowser_h

#include "FileBrowser.h"
#include <nfd.h>
#include <future>

/**
	@brief File browser backed by NativeFileDialog-extended
 */
class NFDFileBrowser : public FileBrowser
{
public:
	NFDFileBrowser(
		const std::string& initialPath,
		const std::string& title,
		const std::string& filterName,
		const std::string& filterMask,
		bool saveDialog
		);
	virtual ~NFDFileBrowser();

	virtual void Render();
	virtual bool IsClosed();
	virtual bool IsClosedOK();
	virtual std::string GetFileName();

protected:
	std::optional<std::string> ThreadProc();

	std::string m_initialPath;
	std::string m_title;
	std::string m_filterName;
	std::string m_filterMask;
	bool m_saveDialog;

	std::future<std::optional<std::string> > m_future;

	std::optional<std::string> GetCachedResult();

	bool m_cachedResultValid;
	std::optional<std::string> m_cachedResult;
};

#endif
