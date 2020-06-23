
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

#ifndef _WIN32
// POSIX-specific filesystem helpers. These will be moved to xptools in a generalized form later.

// Expand things like ~ in path
static std::string ExpandPath(const std::string& in)
{
    wordexp_t result;
    wordexp(in.c_str(), &result, 0);
    auto expanded = result.we_wordv[0];
    std::string out{ expanded };
    wordfree(&result);
    return out;
}

static void CreateDirectory(const std::string& path)
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
            throw std::runtime_error("failed to create preferences directory");
        }    
    }
    else if(!S_ISDIR(fst.st_mode))
    {
        // Exists, but is not a directory
        throw std::runtime_error("preferences directory exists but is not a directory");
    }
}
#endif


void PreferenceManager::InitializeDefaults()
{
    AddPreference("test_string", "First test value", "string");
    AddPreference("test_real", "Second test value", 42.09);
    AddPreference("test_bool", "Third test value", true);
}

std::map<std::string, Preference>& PreferenceManager::AllPreferences()
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

const Preference& PreferenceManager::GetPreference(const std::string& identifier) const
{
    const auto it = m_preferences.find(identifier);
    
    if(it == m_preferences.end())
    {
        throw std::runtime_error("tried to access non-existant preference");
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
        throw std::runtime_error("failed to build directory path");
    }
    
    // Ensure the directory exists
    const auto result = CreateDirectory(directory, NULL);
    
    if(!result && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        throw std::runtime_error("failed to create preferences directory");
    }
    
    // Build final path
    TCHAR config[MAX_PATH];
    if(NULL == PathCombine(config, directory, "preferences.yml"))
    {
        throw std::runtime_error("failed to build directory path");
    }
    
    m_filePath = std::string(config);
#else
    // Ensure all directories in path exist
    CreateDirectory("~/.config");
    CreateDirectory("~/.config/glscopeclient");

    m_filePath = ExpandPath("~/.config/glscopeclient/preferences.yml");
#endif
}

const std::string& PreferenceManager::GetString(const std::string& identifier) const
{
    return GetPreference(identifier).GetString();
}

double PreferenceManager::GetReal(const std::string& identifier) const
{
    return GetPreference(identifier).GetReal();
}

bool PreferenceManager::GetBool(const std::string& identifier) const
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
            
                switch(preference.GetType())
                {
                    case PreferenceType::Boolean:
                        preference.SetBool(node.as<bool>());
                        break;
                        
                    case PreferenceType::Real:
                        preference.SetReal(node.as<double>());
                        break;
                        
                    case PreferenceType::String:
                        preference.SetString(node.as<std::string>());
                        break;
                        
                    default:
                        break;
                }
            }
        }
    }
    catch(const std::exception& ex)
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
    
    std::ofstream outfs{ m_filePath };
    
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
