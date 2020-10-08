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

/**
	@file
	@author Katharina B.
	@brief  PropertyTree implementation
 */

#include "PreferenceTree.h"

#include <stdexcept>
#include <utility>
#include <iterator>
#include <algorithm>
#include <sstream>

namespace internal
{
    PreferencePath::PreferencePath(const std::string& path)
    {
        std::istringstream p{path};
        std::string token{ };

        while(std::getline(p, token, '.'))
        {
            if(!token.empty())
                this->m_segments.push_back(token);
        }
    }

    PreferencePath::PreferencePath(std::vector<std::string> segments)
        : m_segments{ std::move(segments) }
    {

    }

    PreferencePath PreferencePath::NextLevel() const
    {
        std::vector<std::string> newSegments{ this->m_segments.begin(), std::prev(this->m_segments.end()) };
        return PreferencePath{ std::move(newSegments) };
    }
    
    std::size_t PreferencePath::GetLength() const
    {
        return this->m_segments.size();
    }

    const std::string& PreferencePath::GetCurrentSegment() const
    {
        if(this->GetLength() == 0)
            throw std::runtime_error("Empty preference path");

        return this->m_segments[0];
    }

    const std::string& PreferenceTree::GetIdentifier() const
    {
        return this->m_identifier;
    }

    PreferenceHolder::PreferenceHolder(Preference pref)
        : PreferenceTreeNodeBase(pref.GetIdentifier()), m_pref{ std::move(pref) }
    {

    }

    void PreferenceHolder::ToYAML(YAML::Node& node) const
    {
        node[this->m_identifier] = this->m_pref.ToString();
    }

    void PreferenceHolder::FromYAML(const YAML::Node& node)
    {
        if(const auto& n = node[this->m_identifier])
        {
            try
            {
                switch(this->m_pref.GetType())
                {
                    case PreferenceType::Boolean:
                        this->m_pref.SetBool(n.as<bool>());
                        break;
                        
                    case PreferenceType::Real:
                        this->m_pref.SetReal(n.as<double>());
                        break;
                        
                    case PreferenceType::String:
                        this->m_pref.SetString(n.as<string>());
                        break;
                        
                    default:
                        break;
                }
            }
            catch(...)
            {
                LogWarning("Warning: Can't parse preference value %s for preference %s, ignoring",
                    n.as<string>().c_str(), this->m_identifier.c_str());
            }
        }
    }

    Preference& PreferenceHolder::GetLeaf(const PreferencePath& path)
    {
        if(path.GetLength() > 0)
            throw std::runtime_error("Reached tree leaf, but path isnt empty");

        return this->m_pref;
    }
}

const Preference& PreferenceCategory::GetLeaf(const std::string& path)
{
    return this->GetLeaf(internal::PreferencePath{ path });
}

Preference& PreferenceCategory::GetLeaf(const PreferencePath& path)
{
    if(path.GetLength() == 0)
        throw std::runtime_error("Path too short");

    const auto& segment = path.GetCurrentSegment();

    auto iter = this->m_children.find(segment);
    if(iter == this->m_children.end())
        throw std::runtime_error("Couldnt find path segment in preference category");

    return iter->second->GetLeaf(path.NextLevel());
}

void PreferenceCategory::ToYAML(YAML::Node& node) const
{
    YAML::Node child{ };

    for(const auto& entry: this->m_children)
    {
        entry.second->ToYAML(child);
    }

    node[this->m_identifier] = child;
}

void PreferenceCategory::FromYAML(const YAML::Node& node)
{
    if(const auto& n = node[this->m_identifier])
    {
        for(auto& entry: this->m_children)
        {
            entry.second->FromYAML(n);
        }
    }
}

PreferenceCategory::PreferenceCategory(std::string identifier)
    : PreferenceTreeNodeBase(std::move(identifier))
{

}

void PreferenceCategory::AddPreference(Preference pref)
{
    if(this->m_children.count(pref.GetIdentifier()) > 0)
        throw std::runtime_error("Preference category already contains child with given name");

    const auto identifier = pref.GetIdentifier();
    auto ptr = std::make_unique<internal::PreferenceHolder>(std::move(pref));
    this->m_children[identifier] = std::move(ptr);
}

PreferenceCategory& PreferenceCategory::AddCategory(std::string identifier)
{
    if(this->m_children.count(identifier) > 0)
        throw std::runtime_error("Preference category already contains child with given name");

    auto ptr = std::make_unique<PreferenceCategory>(identifier);
    this->m_children[identifier] = std::move(ptr);

    return *this->m_children[identifier];
}
