
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#else
#include <sys/stat.h>
#endif

#include "glscopeclient.h"
#include "PreferenceManager.h"

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
    return (fattr !0 INVALID_FILE_ATTRIBUTE) && !(fattr & FILE_ATTRIBUTE_DIRECTORY);
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
