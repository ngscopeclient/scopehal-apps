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

#include "ngscopeclient.h"
#include "PreferenceManager.h"

#include <fstream>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#else
#include <sys/stat.h>
#include <wordexp.h>
#endif

using namespace std;

PreferenceCategory& PreferenceManager::AllPreferences()
{
	return this->m_treeRoot;
}

bool PreferenceManager::HasPreferenceFile() const
{
#ifdef _WIN32
	const auto fattr = GetFileAttributes(m_filePath.c_str());
	return (fattr != INVALID_FILE_ATTRIBUTES) && !(fattr & FILE_ATTRIBUTE_DIRECTORY);
#else
	struct stat fs{ };
	const auto result = stat(m_filePath.c_str(), &fs);

	return (result == 0) && (fs.st_mode & S_IFREG);
#endif
}

const Preference& PreferenceManager::GetPreference(const string& path) const
{
	return this->m_treeRoot.GetLeaf(path);
}

void PreferenceManager::DeterminePath()
{
#ifdef _WIN32
	wchar_t* stem;
	if(S_OK != SHGetKnownFolderPath(
		FOLDERID_RoamingAppData,
		KF_FLAG_CREATE,
		NULL,
		&stem))
	{
		throw std::runtime_error("failed to resolve %appdata%");
	}

	wchar_t directory[MAX_PATH];
	if(NULL == PathCombineW(directory, stem, L"ngscopeclient"))
	{
		throw runtime_error("failed to build directory path");
	}

	// Ensure the directory exists
	const auto result = CreateDirectoryW(directory, NULL);
	m_configDir = NarrowPath(directory);

	if(!result && GetLastError() != ERROR_ALREADY_EXISTS)
	{
		throw runtime_error("failed to create preferences directory");
	}

	// Build final path
	wchar_t config[MAX_PATH];
	if(NULL == PathCombineW(config, directory, L"preferences.yml"))
	{
		throw runtime_error("failed to build directory path");
	}
	m_filePath = NarrowPath(config);

	CoTaskMemFree(static_cast<void*>(stem));
#else
	// Ensure all directories in path exist
	CreateDirectory("~/.config");
	CreateDirectory("~/.config/ngscopeclient");
	m_configDir = ExpandPath("~/.config/ngscopeclient");

	m_filePath = ExpandPath("~/.config/ngscopeclient/preferences.yml");
#endif
}

int64_t PreferenceManager::GetInt(const string& path) const
{
	return GetPreference(path).GetInt();
}

int64_t PreferenceManager::GetEnumRaw(const string& path) const
{
	return GetPreference(path).GetEnumRaw();
}

const std::string& PreferenceManager::GetString(const string& path) const
{
	return GetPreference(path).GetString();
}

double PreferenceManager::GetReal(const string& path) const
{
	return GetPreference(path).GetReal();
}

bool PreferenceManager::GetBool(const string& path) const
{
	return GetPreference(path).GetBool();
}

ImU32 PreferenceManager::GetColor(const std::string& path) const
{
	return GetPreference(path).GetColor();
}

FontDescription PreferenceManager::GetFont(const std::string& path) const
{
	return GetPreference(path).GetFont();
}

void PreferenceManager::LoadPreferences()
{
	if(!HasPreferenceFile())
	{
		LogTrace("No preference file found\n");
		return;
	}

	try
	{
		LogTrace("Loading preferences from %s\n", m_filePath.c_str());
		auto docs = YAML::LoadAllFromFile(m_filePath);
		if(docs.size())
			this->m_treeRoot.FromYAML(docs[0]);
	}
	catch(const exception& ex)
	{
		LogWarning("Preference file was present, but couldn't be read. Ignoring. (%s)\n", ex.what());
	}
}

void PreferenceManager::SavePreferences()
{
	LogTrace("Saving preferences to %s\n", m_filePath.c_str());

	YAML::Node node{ };

	this->m_treeRoot.ToYAML(node);

	ofstream outfs{ m_filePath };

	if(!outfs)
	{
		LogError("couldn't open preferences file for writing\n");
		return;
	}

	outfs << node;
	outfs.close();

	if(!outfs)
		LogError("couldn't write preferences file to disk\n");
}
