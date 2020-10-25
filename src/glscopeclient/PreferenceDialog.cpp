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
	@brief Implementation of the preference dialog class.
 */

#include "PreferenceDialog.h"

#include <utility>
#include <limits>
#include <memory>
#include <algorithm>
#include <string>
#include <iostream>

using namespace std;

PreferencePage::PreferencePage(PreferenceCategory& category)
    : m_category{category}
{
    set_row_spacing(5);
    set_column_spacing(150);
    CreateWidgets();
}

void PreferencePage::CreateWidgets()
{
    Gtk::Widget* last = nullptr;

    auto& entries = m_category.GetChildren();

    for(const auto& identifier: m_category.GetOrdering())
    {
        auto& node = entries[identifier];

        if(!node->IsPreference())
            continue;

        auto& preference = node->AsPreference();
        
        switch(preference.GetType())
        {
            case PreferenceType::Boolean:
            {
                auto row = unique_ptr<BooleanRow>{ new BooleanRow() };
                row->m_identifier = preference.GetIdentifier();
                row->m_label.set_label(preference.GetLabel());
                row->m_label.set_halign(Gtk::ALIGN_START);
                row->m_check.set_active(preference.GetBool());
                row->m_check.set_halign(Gtk::ALIGN_CENTER);
                row->m_label.set_tooltip_text(preference.GetDescription());
                row->m_check.set_tooltip_text(preference.GetDescription());
                
                if(!last)
                    attach(row->m_label, 0, 0, 1, 1);
                else
                    attach_next_to(row->m_label, *last, Gtk::POS_BOTTOM, 1, 1);
                    
                attach_next_to(row->m_check, row->m_label, Gtk::POS_RIGHT, 1, 1);
                
                last = &(row->m_label);
                m_booleanRows.push_back(std::move(row));
                
                break;
            }
            
            case PreferenceType::Real:
            case PreferenceType::String:
            {
                auto row = unique_ptr<StringRealRow>{ new StringRealRow() };
                row->m_identifier = preference.GetIdentifier();
                row->m_label.set_label(preference.GetLabel());
                row->m_label.set_halign(Gtk::ALIGN_START);
                row->m_label.set_tooltip_text(preference.GetDescription());
                row->m_value.set_tooltip_text(preference.GetDescription());
                    
                const auto text = (preference.GetType() == PreferenceType::Real) ?
                    to_string(preference.GetReal()) : preference.GetString();
         
                row->m_value.set_text(text);
                
                if(!last)
                    attach(row->m_label, 0, 0, 1, 1);
                else
                    attach_next_to(row->m_label, *last, Gtk::POS_BOTTOM, 1, 1);
                    
                attach_next_to(row->m_value, row->m_label, Gtk::POS_RIGHT, 1, 1);
                
                last = &(row->m_label);
                m_stringRealRows.push_back(std::move(row));
                
                break;
            }   
                
            default:
                break;
        }
    }
}

void PreferencePage::SaveChanges()
{
    for(auto& entry: m_category.GetChildren())
    {
        auto* node = entry.second.get();

        if(!node->IsPreference())
            continue;

        auto& preference = node->AsPreference();
        
        switch(preference.GetType())
        {
            case PreferenceType::Boolean:
            {
                // This will always succeed
                const auto it = find_if(m_booleanRows.begin(), m_booleanRows.end(),
                    [&preference](const unique_ptr<BooleanRow>& x) -> bool 
                    {
                        return x->m_identifier == preference.GetIdentifier(); 
                    });
                    
                preference.SetBool((*it)->m_check.get_active());
                   
                break;
            }
            
            case PreferenceType::Real:
            case PreferenceType::String:
            {
                // This will always succeed
                const auto it = find_if(m_stringRealRows.begin(), m_stringRealRows.end(),
                    [&preference](const unique_ptr<StringRealRow>& x) -> bool 
                    {
                        return x->m_identifier == preference.GetIdentifier(); 
                    });
                    
                const auto text = (*it)->m_value.get_text();
                    
                if(preference.GetType() == PreferenceType::Real)
                {
                    try
                    {
                        preference.SetReal(stod(text));
                    }
                    catch(...)
                    {
                        LogError("Ignoring value %s for preference %s: Wrong number format", text.c_str(), preference.GetIdentifier().c_str());
                    }
                }
                else
                {
                    preference.SetString(text);
                }
                   
                break;
            }
               
            default:
                break;
        }
    }
}

PreferenceDialog::PreferenceDialog(OscilloscopeWindow* parent, PreferenceManager& preferences)
    : Gtk::Dialog("Preferences", *parent, Gtk::DIALOG_MODAL)
    , m_preferences(preferences)
{
    set_position(Gtk::WIN_POS_CENTER);
    CreateWidgets();
    show_all();
}

void PreferenceDialog::SetupTree()
{
    m_treeModel = Gtk::TreeStore::create(m_columns);
    m_tree.set_model(m_treeModel);
    m_tree.append_column("Category", m_columns.m_col_category);

    m_tree.set_headers_visible(false);

    auto& prefTree = m_preferences.AllPreferences();
    this->ProcessRootCategories(prefTree);

    m_tree.get_selection()->signal_changed().connect(
		sigc::mem_fun(*this, &PreferenceDialog::OnSelectionChanged));
}

void PreferenceDialog::OnSelectionChanged()
{
    auto selection = m_tree.get_selection();
	if(!selection->count_selected_rows())
		return;

	auto row = *selection->get_selected();
    void* newPageVp = row[m_columns.m_col_page];
    PreferencePage* newPage = reinterpret_cast<PreferencePage*>(newPageVp);

    ActivatePage(newPage);
}

void PreferenceDialog::ActivatePage(PreferencePage* page)
{
    auto* child = m_root.get_child2();

    if(child)
        m_root.remove(*child);

    m_root.add2(*page);
    show_all();
}

void PreferenceDialog::ProcessCategory(PreferenceCategory& category, Gtk::TreeModel::Row& parent)
{
    auto& children = category.GetChildren();
    for(const auto& identifier: category.GetOrdering())
    {
        auto& node = children[identifier];

        if(node->IsCategory())
        {
            auto& subCategory = node->AsCategory();

            unique_ptr<PreferencePage> pagePtr{ new PreferencePage(subCategory) };

            Gtk::TreeModel::Row childrow = *(m_treeModel->append(parent.children()));
            childrow[m_columns.m_col_category] = identifier.c_str();
            childrow[m_columns.m_col_page] = pagePtr.get();

            ProcessCategory(subCategory, childrow);
            m_pages.push_back(std::move(pagePtr));
        }
    }
}

void PreferenceDialog::ProcessRootCategories(PreferenceCategory& root)
{
    auto& children = root.GetChildren();
    for(const auto& identifier: root.GetOrdering())
    {
        auto& node = children[identifier];

        if(node->IsCategory())
        {
            auto& subCategory = node->AsCategory();

            unique_ptr<PreferencePage> pagePtr{ new PreferencePage(subCategory) };

            Gtk::TreeModel::Row row = *(m_treeModel->append());
            row[m_columns.m_col_category] = identifier.c_str();
            row[m_columns.m_col_page] = pagePtr.get();
            ProcessCategory(subCategory, row);
            m_pages.push_back(std::move(pagePtr));
        }
    }
}

void PreferenceDialog::CreateWidgets()
{
    resize(650, 500);
    add_button("OK", Gtk::RESPONSE_OK);
	add_button("Cancel", Gtk::RESPONSE_CANCEL);
    set_deletable(false);

    SetupTree();

    get_vbox()->pack_start(m_root, Gtk::PACK_EXPAND_WIDGET);
        m_root.add1(m_wnd);
            m_wnd.add(m_tree);
            m_wnd.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
        m_root.set_position(200);
}

void PreferenceDialog::SaveChanges()
{
    for(auto& page: m_pages)
        page->SaveChanges();
    
    m_preferences.SavePreferences();
}
 
 
