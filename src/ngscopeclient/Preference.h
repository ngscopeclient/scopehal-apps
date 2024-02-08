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
	@author Katharina B.
	@brief  Basic preference class and auxilliary types
 */

#ifndef Preference_h
#define Preference_h

#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>
#include <type_traits>
#include <cstdint>
#include <set>
#include <vector>

#include <imgui.h>

#include "Unit.h"
#include "FontManager.h"

enum class PreferenceType
{
	Boolean,
	String,
	Real,
	Color,
	Enum,
	Font,
	Int,
	None // Only for moved-from values
};

namespace impl
{
	class PreferenceBuilder;

	struct Color
	{
		Color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
			: m_r{r}, m_g{g}, m_b{b}, m_a{a}
		{

		}

		std::uint8_t m_r, m_g, m_b, m_a;
	};
}

class EnumMapping
{
	using base_type = std::int64_t;

	public:
		EnumMapping()
		{

		}

	public:
		void AddEnumMember(const std::string& name, base_type value);
		const std::string& GetName(base_type value) const;
		base_type GetValue(const std::string& name) const;
		const std::vector<std::string>& GetNames() const;
		bool HasNameFor(base_type value) const;
		bool HasValueFor(const std::string& name) const;

	public:
		template<
			typename E/*,
			typename = typename std::enable_if<
				std::is_convertible<E, base_type>::value &&
				not std::is_same<E, base_type>::value
			>::type*/
		>
		void AddEnumMember(const std::string& name, E value)
		{
			const auto val = static_cast<base_type>(value);
			this->AddEnumMember(name, val);
		}

	protected:
		std::map<std::string, base_type> m_forwardMap;
		std::map<base_type, std::string> m_backwardMap;
		std::vector<std::string> m_names;
};

class Preference
{
	friend class impl::PreferenceBuilder;

private:
	using PreferenceValue =
	typename std::aligned_union<
		std::max(
		{
			sizeof(std::int64_t),
			sizeof(impl::Color),
			sizeof(bool),
			sizeof(double),
			sizeof(std::string),
			sizeof(FontDescription)
		}),
		bool,
		std::string,
		double,
		impl::Color,
		std::int64_t,
		FontDescription
	>::type;

private:
	std::string m_identifier;
	std::string m_label;
	std::string m_description;
	PreferenceType m_type;
	PreferenceValue m_value;
	bool m_isVisible{true};
	Unit m_unit{Unit::UNIT_COUNTS};
	bool m_hasValue{false};
	EnumMapping m_mapping;

public:
	Preference(PreferenceType type, std::string identifier)
		: m_identifier{std::move(identifier)}, m_type{type}
	{

	}

	~Preference()
	{
		CleanUp();
	}

public:
	Preference(const Preference&) = delete;
	Preference(Preference&& other)
	{
		MoveFrom(other);
	}

	Preference& operator=(const Preference&) = delete;
	Preference& operator=(Preference&& other)
	{
		CleanUp();
		MoveFrom(other);
		return *this;
	}

public:
	const std::string& GetIdentifier() const;
	const std::string& GetLabel() const;
	const std::string& GetDescription() const;
	void SetFont(const FontDescription& font);
	FontDescription GetFont() const;
	std::int64_t GetEnumRaw() const;
	void SetEnumRaw(std::int64_t value);
	PreferenceType GetType() const;
	bool GetBool() const;
	double GetReal() const;
	std::int64_t GetInt() const;
	const std::string& GetString() const;
	std::string ToString() const;
	bool GetIsVisible() const;
	ImU32 GetColor() const;
	const impl::Color& GetColorRaw() const;
	void SetBool(bool value);
	void SetReal(double value);
	void SetInt(std::int64_t value);
	void SetString(std::string value);
	void SetColor(const ImU32& value);
	void SetColorRaw(const impl::Color& value);
	void SetLabel(std::string label);
	void SetDescription(std::string description);
	bool HasUnit();
	Unit& GetUnit();
	const EnumMapping& GetMapping() const;

	template< typename E >
	E GetEnum() const
	{
		return static_cast<E>(this->GetEnumRaw());
	}

	template< typename E >
	void SetEnum(E value)
	{
		this->SetEnumRaw(static_cast<std::int64_t>(value));
	}

public:
	static impl::PreferenceBuilder Int(std::string identifier, int64_t defaultValue);
	static impl::PreferenceBuilder Real(std::string identifier, double defaultValue);
	static impl::PreferenceBuilder Bool(std::string identifier, bool defaultValue);
	static impl::PreferenceBuilder String(std::string identifier, std::string defaultValue);
	static impl::PreferenceBuilder Color(std::string identifier, const ImU32& defaultValue);
	static impl::PreferenceBuilder EnumRaw(std::string identifier, std::int64_t defaultValue);
	static impl::PreferenceBuilder Font(std::string identifier, FontDescription defaultValue);

	template< typename E >
	static impl::PreferenceBuilder Enum(std::string identifier, E defaultValue);


private:
	void SetMapping(EnumMapping mapping);

	template<typename T>
	const T& GetValueRaw() const
	{
		return *reinterpret_cast<const T*>(&m_value);
	}

	template<typename T>
	T& GetValueRaw()
	{
		return *reinterpret_cast<T*>(&m_value);
	}

	void CleanUp();

	template<typename T>
	void Construct(T value)
	{
		new (&m_value) T(std::move(value));
		m_hasValue = true;
	}

	void MoveFrom(Preference& other);
};

namespace impl
{
	class PreferenceBuilder
	{
		protected:
			Preference m_pref;

		public:
			PreferenceBuilder(Preference&& pref);

		public:
			PreferenceBuilder Invisible() &&;
			PreferenceBuilder Label(std::string label) &&;
			PreferenceBuilder Description(std::string description) &&;
			PreferenceBuilder Unit(Unit::UnitType type) &&;
			Preference Build() &&;

			template< typename E >
			PreferenceBuilder EnumValue(const std::string& name, E value) &&
			{
				this->m_pref.m_mapping.AddEnumMember<E>(name, value);
				return std::move(*this);
			}
	};
}

template< typename E >
impl::PreferenceBuilder Preference::Enum(std::string identifier, E defaultValue)
{
	return EnumRaw(std::move(identifier), static_cast<std::int64_t>(defaultValue));
}

#endif // Preference_h
