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
#include <stdexcept>

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
    auto& appearance = this->m_treeRoot.AddCategory("Appearance");
        auto& cursors = appearance.AddCategory("Cursors");
            cursors.AddPreference(Preference(
                "cursor_1_color",
                "Cursor #1 color",
                "Color for the left or top cursor",
                Gdk::Color("yellow")));
            cursors.AddPreference(Preference(
                "cursor_2_color",
                "Cursor #2 color",
                "Color for the right or bottom cursor",
                Gdk::Color("orange")));
            cursors.AddPreference(Preference(
                "cursor_fill_color",
                "Cursor fill color",
                "Color for the filled area between cursors",
                Gdk::Color("yellow")));
            cursors.AddPreference(Preference(
                "cursor_fill_text_color",
                "Cursor fill text color",
                "Color for in-band power and other text drawn between cursors",
                Gdk::Color("yellow")));
        auto& windows = appearance.AddCategory("Windows");
            windows.AddPreference(Preference(
                "insertion_bar_color",
                "Insertion bar color (insert)",
                "Color for the insertion bar when dragging a waveform within a group",
                Gdk::Color("yellow")));
            windows.AddPreference(Preference(
                "insertion_bar_split_color",
                "Insertion bar color (split)",
                "Color for the insertion bar when splitting a waveform group",
                Gdk::Color("orange")));

    auto& instrument = this->m_treeRoot.AddCategory("Instrument");
        auto& trans = instrument.AddCategory("Transports");
            trans.AddPreference(Preference("test_string", "Test string", "First test value", "string"));
            trans.AddPreference(Preference::New("test_real", "Test real", "Second test value", 42.09).WithUnit(Unit::UNIT_VOLTS).Build());
            trans.AddPreference(Preference("test_bool", "Test boolean", "Third test value", true));
        auto& decode = instrument.AddCategory("Decoders");
            decode.AddPreference(Preference("test_string", "Test string", "First test value", "string"));
            decode.AddPreference(Preference::New("test_real", "Test real", "Second test value", 42.09).WithUnit(Unit::UNIT_AMPS).Build());
            decode.AddPreference(Preference("test_bool", "Test boolean", "Third test value", true));

    auto& debug = this->m_treeRoot.AddCategory("Debug");
        auto& testSettings = debug.AddCategory("Test Settings");
            testSettings.AddPreference(Preference("test_string", "Test string", "First test value", "string"));
            testSettings.AddPreference(Preference("test_real", "Test real", "Second test value", 42.09));
            testSettings.AddPreference(Preference("test_bool", "Test boolean", "Third test value", true));
            testSettings.AddPreference(Preference("test_color", "Test color", "Some test color", Gdk::Color{}));
        auto& miscSettings = debug.AddCategory("Misc");
            miscSettings.AddPreference(Preference("misc_test_1", "Misc test real", "blabla", 13.37));
}

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
    if(NULL == PathCombineW(directory, stem, L"glscopeclient"))
    {
        throw runtime_error("failed to build directory path");
    }

    // Ensure the directory exists
    const auto result = CreateDirectoryW(directory, NULL);

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

    char configNarrow[MAX_PATH];
    const auto len = wcstombs(configNarrow, config, MAX_PATH);

    if(len == static_cast<size_t>(-1))
        throw runtime_error("Failed to convert wide string");

    m_filePath = string(configNarrow);


    CoTaskMemFree(static_cast<void*>(stem));
#else
    // Ensure all directories in path exist
    CreateDirectory("~/.config");
    CreateDirectory("~/.config/glscopeclient");

    m_filePath = ExpandPath("~/.config/glscopeclient/preferences.yml");
#endif
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

Gdk::Color PreferenceManager::GetColor(const std::string& path) const
{
    return GetPreference(path).GetColor();
}

void PreferenceManager::LoadPreferences()
{
    if(!HasPreferenceFile())
        return;

    try
    {
        auto doc = YAML::LoadAllFromFile(m_filePath)[0];
        this->m_treeRoot.FromYAML(doc);
    }
    catch(const exception& ex)
    {
        LogWarning("Warning: Preference file was present, but couldn't be read. Ignoring. (%s)", ex.what());
    }
}

void PreferenceManager::SavePreferences()
{
    YAML::Node node{ };

    this->m_treeRoot.ToYAML(node);

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
