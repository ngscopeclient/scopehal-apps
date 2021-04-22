/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2021 Andrew D. Zonenberg                                                                          *
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
	@author Andrew D. Zonenberg
	@brief  Implementation of ProtocolTreeModel
 */
#include "glscopeclient.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ProtocolTreeModel

ProtocolTreeModel::ProtocolTreeModel(const Gtk::TreeModelColumnRecord& columns)
	 : Glib::ObjectBase(typeid(ProtocolTreeModel))
	 , Gtk::TreeModel()
	 , m_columns(columns)
{
	m_nheaders = columns.size() - 9;
}

Glib::RefPtr<ProtocolTreeModel> ProtocolTreeModel::create(const Gtk::TreeModelColumnRecord& columns)
{
	return Glib::RefPtr<ProtocolTreeModel>(new ProtocolTreeModel(columns));
}

Gtk::TreeModelFlags ProtocolTreeModel::get_flags_vfunc() const
{
	return static_cast<Gtk::TreeModelFlags>(GTK_TREE_MODEL_ITERS_PERSIST);
}

int ProtocolTreeModel::get_n_columns_vfunc() const
{
	return m_columns.size();
}

GType ProtocolTreeModel::get_column_type_vfunc(int index) const
{
	return m_columns.types()[index];
}

bool ProtocolTreeModel::iter_next_vfunc(const iterator& iter, iterator& iter_next) const
{
	iter_next = iter;
	auto g = iter_next.gobj();

	int irow = GPOINTER_TO_INT(g->user_data);
	int ichild = GPOINTER_TO_INT(g->user_data2);

	//Child? Bump the second index if possible.
	if(ichild >= 0)
	{
		ichild ++;
		if(ichild >= (int)m_rows[irow].m_children.size())
		{
			g->user_data = GINT_TO_POINTER(-1);
			g->user_data2 = GINT_TO_POINTER(-1);
			return false;
		}
	}

	//Move on to the next node at the same level
	else
		irow ++;

	//Save the final iterator
	if(irow >= (int)m_rows.size())
	{
		g->user_data = GINT_TO_POINTER(-1);
		g->user_data2 = GINT_TO_POINTER(-1);
		return false;
	}
	else
	{
		g->user_data = GINT_TO_POINTER(irow);
		g->user_data2 = GINT_TO_POINTER(ichild);
	}
	return true;
}

bool ProtocolTreeModel::iter_children_vfunc(const iterator& parent, iterator& iter) const
{
	//Look up the parent node
	auto g = parent.gobj();
	int irow = GPOINTER_TO_INT(g->user_data);
	int ichild = GPOINTER_TO_INT(g->user_data2);

	iter = iterator();
	auto h = iter.gobj();

	//If we're a child node, or have no children, nothing to do
	if( (ichild >= 0) || m_rows[irow].m_children.empty() )
	{
		h->user_data = GINT_TO_POINTER(-1);
		h->user_data2 = GINT_TO_POINTER(-1);
		return false;
	}

	//Return iterator to the first child
	else
	{
		h->user_data = GINT_TO_POINTER(irow);
		h->user_data2 = GINT_TO_POINTER(0);
		return true;
	}
}

bool ProtocolTreeModel::iter_has_child_vfunc(const iterator& iter) const
{
	auto g = iter.gobj();
	int irow = GPOINTER_TO_INT(g->user_data);
	int ichild = GPOINTER_TO_INT(g->user_data2);

	//Children can't have children
	if(ichild >= 0)
		return false;

	else
		return !m_rows[irow].m_children.empty();
}

int ProtocolTreeModel::iter_n_children_vfunc(const iterator& iter) const
{
	auto g = iter.gobj();
	int irow = GPOINTER_TO_INT(g->user_data);
	int ichild = GPOINTER_TO_INT(g->user_data2);

	//Children can't have children
	if(ichild >= 0)
		return 0;

	else
		return m_rows[irow].m_children.size();
}

int ProtocolTreeModel::iter_n_root_children_vfunc() const
{
	return m_rows.size();
}

bool ProtocolTreeModel::iter_nth_child_vfunc(const iterator& parent, int n, iterator& iter) const
{
	//Look up the parent node
	auto g = parent.gobj();
	int irow = GPOINTER_TO_INT(g->user_data);
	int ichild = GPOINTER_TO_INT(g->user_data2);

	iter = iterator();
	auto h = iter.gobj();

	//If we're a child node, or have insufficient children, nothing to do
	if( (ichild >= 0) || ((int)m_rows[irow].m_children.size() <= n ) )
	{
		h->user_data = GINT_TO_POINTER(-1);
		h->user_data2 = GINT_TO_POINTER(-1);
		return false;
	}

	//Return iterator to the Nth child
	else
	{
		h->user_data = GINT_TO_POINTER(irow);
		h->user_data2 = GINT_TO_POINTER(n);
		return true;
	}
}

bool ProtocolTreeModel::iter_nth_root_child_vfunc(int n, iterator& iter) const
{
	iter = iterator();
	auto g = iter.gobj();
	g->user_data = GINT_TO_POINTER(n);
	g->user_data2 = GINT_TO_POINTER(-1);

	return n <= (int)m_rows.size();
}

bool ProtocolTreeModel::iter_parent_vfunc(const iterator& /*child*/, iterator& /*iter*/) const
{
	LogError("ProtocolTreeModel::iter_parent_vfunc unimplemented\n");
	return false;
}

Gtk::TreePath ProtocolTreeModel::get_path_vfunc(const iterator& iter) const
{
	auto g = iter.gobj();

	Gtk::TreePath path;
	path.push_back(reinterpret_cast<ssize_t>(g->user_data));
	ssize_t second = reinterpret_cast<ssize_t>(g->user_data2);
	if(second != -1)
		path.push_back(second);

	return path;
}

bool ProtocolTreeModel::get_iter_vfunc(const Gtk::TreePath& path, iterator& iter) const
{
	iter = iterator();

	//we have children
	auto g = iter.gobj();
	if(path.size() > 1)
	{
		g->user_data = GINT_TO_POINTER(path[0]);
		g->user_data2 = GINT_TO_POINTER(path[1]);
	}
	else
	{
		g->user_data = GINT_TO_POINTER(path[0]);
		g->user_data2 = GINT_TO_POINTER(-1);
	}

	return true;
}

/**
	@brief Converts an iterator to the underlying row object
 */
const ProtocolTreeRow* ProtocolTreeModel::GetRow(const iterator& iter) const
{
	auto g = iter.gobj();
	auto prow = &m_rows[GPOINTER_TO_INT(g->user_data)];
	auto second = GPOINTER_TO_INT(g->user_data2);
	if(second == -1)
		return prow;

	else
		return &prow->m_children[second];
}

ProtocolTreeRow* ProtocolTreeModel::GetRow(const iterator& iter)
{
	auto g = iter.gobj();
	auto prow = &m_rows[GPOINTER_TO_INT(g->user_data)];
	auto second = GPOINTER_TO_INT(g->user_data2);
	if(second == -1)
		return prow;

	else
		return &prow->m_children[second];
}

void ProtocolTreeModel::set_value_impl(const iterator& row, int column, const Glib::ValueBase& value)
{
	auto p = GetRow(row);

	switch(column)
	{
		case 0:
			p->m_visible = reinterpret_cast<const Gtk::TreeModelColumn<bool>::ValueType&>(value).get();
			break;

		case 1:
			p->m_bgcolor = reinterpret_cast<const Gtk::TreeModelColumn<Gdk::Color>::ValueType&>(value).get();
			break;

		case 2:
			p->m_fgcolor = reinterpret_cast<const Gtk::TreeModelColumn<Gdk::Color>::ValueType&>(value).get();
			break;

		case 3:
			p->m_height = reinterpret_cast<const Gtk::TreeModelColumn<int>::ValueType&>(value).get();
			break;

		case 4:
			p->m_timestamp = reinterpret_cast<const Gtk::TreeModelColumn<std::string>::ValueType&>(value).get();
			break;

		case 5:
			p->m_capturekey = reinterpret_cast<const Gtk::TreeModelColumn<TimePoint>::ValueType&>(value).get();
			break;

		case 6:
			p->m_offset = reinterpret_cast<const Gtk::TreeModelColumn<int64_t>::ValueType&>(value).get();
			break;

		//header, image, or data
		default:
			{
				int ihead = column - 7;
				if(ihead < m_nheaders)
				{
					p->m_headers.resize(m_nheaders);
					p->m_headers[ihead] =
						reinterpret_cast<const Gtk::TreeModelColumn<std::string>::ValueType&>(value).get();
				}
				else if(ihead == m_nheaders)
					LogDebug("set_value_impl for col %d (image) not implemented yet\n", column);
				else
					p->m_data = reinterpret_cast<const Gtk::TreeModelColumn<std::string>::ValueType&>(value).get();
			}
			break;
	}

	row_changed(get_path_vfunc(row), row);
}

void ProtocolTreeModel::get_value_vfunc(const TreeModel::iterator& iter, int column, Glib::ValueBase& value) const
{
	auto p = GetRow(iter);
	value.init(m_columns.types()[column]);

	switch(column)
	{
		case 0:
			reinterpret_cast<Gtk::TreeModelColumn<bool>::ValueType&>(value).set(p->m_visible);
			break;

		case 1:
			reinterpret_cast<Gtk::TreeModelColumn<Gdk::Color>::ValueType&>(value).set(p->m_bgcolor);
			break;

		case 2:
			reinterpret_cast<Gtk::TreeModelColumn<Gdk::Color>::ValueType&>(value).set(p->m_fgcolor);
			break;

		case 3:
			reinterpret_cast<Gtk::TreeModelColumn<int>::ValueType&>(value).set(p->m_height);
			break;

		case 4:
			reinterpret_cast<Gtk::TreeModelColumn<std::string>::ValueType&>(value).set(p->m_timestamp);
			break;

		case 5:
			reinterpret_cast<Gtk::TreeModelColumn<TimePoint>::ValueType&>(value).set(p->m_capturekey);
			break;

		case 6:
			reinterpret_cast<Gtk::TreeModelColumn<int64_t>::ValueType&>(value).set(p->m_offset);
			break;

		//header, image, or data
		default:
			{
				int ihead = column - 7;
				if(ihead < m_nheaders)
					reinterpret_cast<Gtk::TreeModelColumn<std::string>::ValueType&>(value).set(p->m_headers[ihead]);
				else if(ihead == m_nheaders)
					LogDebug("get_value_vfunc for col %d (image) not implemented yet\n", column);
				else
					reinterpret_cast<Gtk::TreeModelColumn<std::string>::ValueType&>(value).set(p->m_data);
			}
			break;
	}
}

/**
	@brief Add a new node at the root of the tree
 */
Gtk::TreeModel::iterator ProtocolTreeModel::append()
{
	Gtk::TreePath path;
	auto len = m_rows.size();
	path.push_back(len);
	m_rows.resize(len+1);
	auto it = get_iter(path);

	//Update the view
	row_inserted(path, it);
	return it;
}

Gtk::TreeModel::iterator ProtocolTreeModel::erase(const iterator& iter)
{
	LogError("erase unimplemented\n");
}

Gtk::TreeModel::iterator ProtocolTreeModel::append(const Gtk::TreeNodeChildren& node)
{
	auto g = node.gobj();
	auto nrow = GPOINTER_TO_INT(g->user_data);
	auto second = GPOINTER_TO_INT(g->user_data2);
	if(second >= 0)
		LogError("tried to append a node that has children already (%d/%d)\n", nrow, second);

	Gtk::TreePath path;
	path.push_back(nrow);
	auto len = m_rows[nrow].m_children.size();
	path.push_back(len);
	m_rows[nrow].m_children.resize(len+1);
	auto it = get_iter(path);

	//Update the view
	row_inserted(path, it);
	return it;
}
