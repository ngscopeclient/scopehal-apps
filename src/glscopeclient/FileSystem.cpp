/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2020 Andrew D. Zonenberg                                                                          *
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

#include "FileSystem.h"

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#include <fileapi.h>
#include <shellapi.h>
#else
#include <glob.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#endif

using namespace std;

vector<string> Glob(const string& pathPattern, bool onlyDirectories)
{
	vector<string> result{ };

#ifdef _WIN32
	WIN32_FIND_DATA findData{ };
	HANDLE fileSearch{ };

	fileSearch = FindFirstFileEx(
		pathPattern.c_str(),
		FindExInfoStandard,
		&findData,
		onlyDirectories ? FindExSearchLimitToDirectories : FindExSearchNameMatch,
		NULL,
		0
	);

	if(fileSearch != INVALID_HANDLE_VALUE)
	{
		while(FindNextFile(fileSearch, &findData))
		{
			const auto* dir = findData.cFileName;

			if(!strcmp(dir, "..") || !strcmp(dir, "."))
				continue;

			if(!onlyDirectories || (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				result.push_back(string{dir});
			}
		}
	}
#else
	glob_t globResult{ };

// GLOB_ONLYDIR is only a performance flag, it doesn't promise only dirs
#ifdef GLOB_ONLYDIR
	glob(pathPattern.c_str(), onlyDirectories ? GLOB_ONLYDIR : 0, NULL, &globResult);
#else
	glob(pathPattern.c_str(), 0, NULL, &globResult);
#endif

	if(globResult.gl_pathc > 0)
	{
		for(auto ix = 0U; ix < globResult.gl_pathc; ++ix)
		{
			const auto* dir = globResult.gl_pathv[ix];
			result.push_back(string{dir});
		}
	}

	globfree(&globResult);
#endif

	return result;
}

void RemoveDirectory(const string& basePath)
{
#ifdef _WIN32
	SHFILEOPSTRUCT deleteDir = {
		NULL,
		FO_DELETE,
		basePath.c_str(),
		NULL,
		FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION,
		FALSE,
		NULL,
		NULL
	};

	SHFileOperation(&deleteDir);
#else
	const auto deleteTree =
		[](const char* path, const struct stat*, int, struct FTW*) -> int
		{
			::remove(path);
			return 0;
		};

	nftw(basePath.c_str(), deleteTree, 32, FTW_DEPTH);
#endif
}
