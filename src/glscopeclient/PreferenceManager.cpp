/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
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

#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#else
#include <sys/stat.h>
#include <wordexp.h>
#endif

#include "glscopeclient.h"
#include "PreferenceManager.h"

using namespace std;

#ifndef _WIN32
// POSIX-specific filesystem helpers. These will be moved to xptools in a generalized form later.

// Expand things like ~ in path
static std::string ExpandPath(const string& in)
{
    wordexp_t result;
    wordexp(in.c_str(), &result, 0);
    auto expanded = result.we_wordv[0];
    string out{ expanded };
    wordfree(&result);
    return out;
}

static void CreateDirectory(const string& path)
{
    const auto expanded = ExpandPath(path);

    struct stat fst{ };
    
    // Check if it exists
    if(stat(expanded.c_str(), &fst) != 0)
    {
        // If not, create it
        if(mkdir(expanded.c_str(), 0755) != 0 && errno != EEXIST)
        {
            perror("");
            throw runtime_error("failed to create preferences directory");
        }    
    }
    else if(!S_ISDIR(fst.st_mode))
    {
        // Exists, but is not a directory
        throw runtime_error("preferences directory exists but is not a directory");
    }
}
#endif


void PreferenceManager::InitializeDefaults()
{
    AddPreference("test_string", "Test string", "First test value", "string");
    AddPreference("test_real", "Test real", "Second test value", 42.09);
    AddPreference("test_bool", "Test boolean", "Third test value", true);
}

map<string, Preference>& PreferenceManager::AllPreferences()
{
    return m_preferences;
}

bool PreferenceManager::HasPreferenceFile() const
{
#ifdef _WIN32
    const auto fattr = GetFileAttributes(m_filePath.c_str());
    return (fattr != INVALID_FILE_ATTRIBUTE) && !(fattr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat fs{ };
    const auto result = stat(m_filePath.c_str(), &fs);
    
    return (result == 0) && (fs.st_mode & S_IFREG);
#endif
}

const Preference& PreferenceManager::GetPreference(const string& identifier) const
{
    const auto it = m_preferences.find(identifier);
    
    if(it == m_preferences.end())
    {
        throw runtime_error("tried to access non-existant preference");
    }
    else
    {
        return it->second;
    }
}

void PreferenceManager::DeterminePath()
{
#ifdef _WIN32
    TCHAR stem[MAX_PATH];
    if(S_OK != SHGetKnownFolderPath(
        FOLDERID_RoamingAppData,
        KF_FLAG_CREATE,
        NULL,
        stem))
    {
        throw std::runtime_error("failed to resolve %appdata%");
    }
    
    TCHAR directory[MAX_PATH];
    if(NULL == PathCombine(directory, stem, "glscopeclient"))
    {
        throw runtime_error("failed to build directory path");
    }
    
    // Ensure the directory exists
    const auto result = CreateDirectory(directory, NULL);
    
    if(!result && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        throw runtime_error("failed to create preferences directory");
    }
    
    // Build final path
    TCHAR config[MAX_PATH];
    if(NULL == PathCombine(config, directory, "preferences.yml"))
    {
        throw runtime_error("failed to build directory path");
    }
    
    m_filePath = string(config);
#else
    // Ensure all directories in path exist
    CreateDirectory("~/.config");
    CreateDirectory("~/.config/glscopeclient");

    m_filePath = ExpandPath("~/.config/glscopeclient/preferences.yml");
#endif
}

const std::string& PreferenceManager::GetString(const string& identifier) const
{
    return GetPreference(identifier).GetString();
}

double PreferenceManager::GetReal(const string& identifier) const
{
    return GetPreference(identifier).GetReal();
}

bool PreferenceManager::GetBool(const string& identifier) const
{
    return GetPreference(identifier).GetBool();
}

void PreferenceManager::LoadPreferences()
{
    if(!HasPreferenceFile())
        return;

    try
    {
        auto doc = YAML::LoadAllFromFile(m_filePath)[0];
    
        for(auto& entry: m_preferences)
        {
            // Check if the preferences file contains an entry with that matches the
            // current preference identifier. If so, we overwrite the stored default value.
            if(const auto& node = doc[entry.first])
            {
                auto& preference = entry.second;
            
                try
                {
                    switch(preference.GetType())
                    {
                        case PreferenceType::Boolean:
                            preference.SetBool(node.as<bool>());
                            break;
                            
                        case PreferenceType::Real:
                            preference.SetReal(node.as<double>());
                            break;
                            
                        case PreferenceType::String:
                            preference.SetString(node.as<string>());
                            break;
                            
                        default:
                            break;
                    }
                }
                catch(...)
                {
                    LogWarning("Warning: Can't parse preference value %s for preference %s, ignoring",
                        node.as<string>().c_str(), preference.GetIdentifier().c_str());
                }
            }
        }
    }
    catch(const exception& ex)
    {
        LogWarning("Warning: Preference file was present, but couldn't be read. Ignoring. (%s)", ex.what());
    }
}

void PreferenceManager::SavePreferences()
{
    YAML::Node node{ };
    
    for(const auto& entry: m_preferences)
    {
        node[entry.first] = entry.second.ToString();
    }
    
    ofstream outfs{ m_filePath };
    
    if(!outfs)
    {
        LogError("couldn't open preferences file for writing");
        return;
    }
    
    outfs << node;
    outfs.close();
    
    if(!outfs)
    {
        LogError("couldn't write preferences file to disk");
    }
}
