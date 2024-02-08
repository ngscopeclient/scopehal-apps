/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2023 Andrew D. Zonenberg                                                                          *
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

#include "ngscopeclient.h"
#include "PreferenceTree.h"

#include <stdexcept>
#include <utility>
#include <iterator>
#include <algorithm>
#include <sstream>
#include <utility>
#include <memory>

using namespace std;

namespace internal
{
	PreferencePath::PreferencePath(const string& path)
	{
		istringstream p{path};
		string token{ };

		while(getline(p, token, '.'))
		{
			if(!token.empty())
				this->m_segments.push_back(token);
		}
	}

	PreferencePath::PreferencePath(vector<string> segments)
		: m_segments{ std::move(segments) }
	{

	}

	PreferencePath PreferencePath::NextLevel() const
	{
		vector<string> newSegments{ next(this->m_segments.begin()), this->m_segments.end() };
		return PreferencePath{ std::move(newSegments) };
	}

	size_t PreferencePath::GetLength() const
	{
		return this->m_segments.size();
	}

	const string& PreferencePath::GetCurrentSegment() const
	{
		if(this->GetLength() == 0)
			throw runtime_error("Empty preference path");

		return this->m_segments[0];
	}

	const string& PreferenceTreeNodeBase::GetIdentifier() const
	{
		return this->m_identifier;
	}

	PreferenceTreeNodeType PreferenceTreeNodeBase::GetType() const
	{
		return this->m_type;
	}

	bool PreferenceTreeNodeBase::IsCategory() const
	{
		return this->GetType() == PreferenceTreeNodeType::Category;
	}

	bool PreferenceTreeNodeBase::IsPreference() const
	{
		return this->GetType() == PreferenceTreeNodeType::Preference;
	}

	PreferenceCategory& PreferenceTreeNodeBase::AsCategory()
	{
		if(!this->IsCategory())
			throw runtime_error("Node is not a category");

		return *static_cast<PreferenceCategory*>(this);
	}

	Preference& PreferenceTreeNodeBase::AsPreference()
	{
		if(!this->IsPreference())
			throw runtime_error("Node is not a preference");

		return static_cast<PreferenceHolder*>(this)->Get();
	}

	PreferenceHolder::PreferenceHolder(Preference pref)
		: PreferenceTreeNodeBase(PreferenceTreeNodeType::Preference, pref.GetIdentifier()), m_pref{ std::move(pref) }
	{

	}

	void PreferenceHolder::ToYAML(YAML::Node& node) const
	{
		switch(this->m_pref.GetType())
		{
			case PreferenceType::Color:
			{
				YAML::Node child{ };

				const auto& color = this->m_pref.GetColorRaw();

				//Save as int rather than uint8 because uint8 is often a character type
				child["r"] = (int)color.m_r;
				child["g"] = (int)color.m_g;
				child["b"] = (int)color.m_b;
				child["a"] = (int)color.m_a;

				node[this->m_identifier] = child;
				break;
			}

			case PreferenceType::Font:
			{
				YAML::Node child{ };

				const auto& font = this->m_pref.GetFont();
				child["path"] = font.first;
				child["size"] = font.second;
				node[this->m_identifier] = child;

				break;
			}

			default:
			{
				node[this->m_identifier] = this->m_pref.ToString();
				break;
			}
		}
	}

	bool PreferenceHolder::IsVisible() const
	{
		return this->m_pref.GetIsVisible();
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

					case PreferenceType::Int:
						this->m_pref.SetInt(n.as<std::int64_t>());
						break;

					case PreferenceType::String:
						this->m_pref.SetString(n.as<string>());
						break;

					case PreferenceType::Font:
						this->m_pref.SetFont(FontDescription(n["path"].as<string>(), n["size"].as<float>()));
						break;

					case PreferenceType::Enum:
					{
						const auto value = n.as<string>();
						const auto& mapper = this->m_pref.GetMapping();
						this->m_pref.SetEnumRaw(mapper.GetValue(value));
						break;
					}

					case PreferenceType::Color:
					{
						//Load as int rather than uint8 because uint8 is often a character type
						const auto n_r = n["r"].as<int>();
						const auto n_g = n["g"].as<int>();
						const auto n_b = n["b"].as<int>();
						const auto n_a = n["a"].as<int>();

						this->m_pref.SetColorRaw(impl::Color(n_r, n_g, n_b, n_a));
						break;
					}

					default:
						break;
				}
			}
			catch(...)
			{
				LogWarning("Can't parse preference value %s for preference %s, ignoring\n",
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

	Preference& PreferenceHolder::Get()
	{
		return this->m_pref;
	}

	const Preference& PreferenceHolder::Get() const
	{
		return this->m_pref;
	}
}

Preference& PreferenceCategory::GetLeaf(const string& path)
{
	return this->GetLeaf(internal::PreferencePath{ path });
}

PreferenceCategory::map_type& PreferenceCategory::GetChildren()
{
	return this->m_children;
}

bool PreferenceCategory::IsVisible() const
{
	// Preference category is only visible if theres at least one visible entry in it
	return any_of(m_children.begin(), m_children.end(),
		[](const map_type::value_type& element) -> bool
		{
			return element.second->IsVisible();
		});
}

const PreferenceCategory::seq_type& PreferenceCategory::GetOrdering() const
{
	return this->m_ordering;
}

Preference& PreferenceCategory::GetLeaf(const internal::PreferencePath& path)
{
	if(path.GetLength() == 0)
		throw runtime_error("Path too short");

	const auto& segment = path.GetCurrentSegment();

	auto iter = this->m_children.find(segment);
	if(iter == this->m_children.end())
		throw runtime_error("Couldnt find path segment in preference category");

	return iter->second->GetLeaf(path.NextLevel());
}

void PreferenceCategory::ToYAML(YAML::Node& node) const
{
	YAML::Node child{ };

	for(const auto& entry: this->m_children)
	{
		entry.second->ToYAML(child);
	}

	if(this->m_identifier == "")
		node = child;
	else
		node[this->m_identifier] = child;
}


void PreferenceCategory::FromYAML(const YAML::Node& node)
{
	const auto readChildren = [this](const YAML::Node& n)
	{
		for(auto& entry: this->m_children)
		{
			entry.second->FromYAML(n);
		}
	};

	if(this->m_identifier == "")
	{
		readChildren(node);
	}
	else if(const auto& n = node[this->m_identifier])
	{
		readChildren(n);
	}
}

PreferenceCategory::PreferenceCategory(string identifier)
	: PreferenceTreeNodeBase(PreferenceTreeNodeType::Category, std::move(identifier))
{

}

const Preference& PreferenceCategory::GetLeaf(const string& path) const
{
	auto* thisNonConst = const_cast<PreferenceCategory*>(this);
	return thisNonConst->GetLeaf(path);
}

void PreferenceCategory::AddPreference(Preference pref)
{
	if(this->m_children.count(pref.GetIdentifier()) > 0)
		throw runtime_error("Preference category already contains child with given name");

	const auto identifier = pref.GetIdentifier();
	unique_ptr<internal::PreferenceTreeNodeBase> ptr{ new internal::PreferenceHolder{ std::move(pref) } };
	this->m_children[identifier] = std::move(ptr);
	this->m_ordering.push_back(identifier);
}

void PreferenceCategory::AddPreference(impl::PreferenceBuilder&& pref)
{
	this->AddPreference(std::move(pref).Build());
}

PreferenceCategory& PreferenceCategory::AddCategory(string identifier)
{
	if(this->m_children.count(identifier) > 0)
		throw runtime_error("Preference category already contains child with given name");

	unique_ptr<internal::PreferenceTreeNodeBase> ptr{ new PreferenceCategory{ identifier } };
	this->m_children[identifier] = std::move(ptr);
	this->m_ordering.push_back(identifier);

	return *static_cast<PreferenceCategory*>(this->m_children[identifier].get());
}
