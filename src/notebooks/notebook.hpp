/*
 * gnote
 *
 * Copyright (C) 2010-2014,2017,2019,2023 Aurimas Cernius
 * Copyright (C) 2009 Hubert Figuiere
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */



#ifndef __NOTEBOOKS_NOTEBOOK_HPP_
#define __NOTEBOOKS_NOTEBOOK_HPP_

#include "tag.hpp"
#include "note.hpp"

namespace gnote {
namespace notebooks {


/// <summary>
/// An object that represents a notebook in Tomboy
/// </summary>
class Notebook 
  : public std::enable_shared_from_this<Notebook>
{
public:
  typedef std::shared_ptr<Notebook> Ptr;
  static const char * NOTEBOOK_TAG_PREFIX;
  Notebook(NoteManagerBase &, const Glib::ustring &, bool is_special = false);
  Notebook(NoteManagerBase &, const Tag::Ptr &);
  Glib::ustring get_name() const
    { return m_name; }
  void set_name(const Glib::ustring &);
  virtual Glib::ustring get_normalized_name() const;
  virtual Tag::Ptr    get_tag() const;
  Note::Ref find_template_note() const;
  virtual Note::Ptr   get_template_note() const;
  Note::Ptr create_notebook_note();
  virtual bool contains_note(const Note & note, bool include_system = false);
  virtual bool add_note(Note&);
  static Glib::ustring normalize(const Glib::ustring & s);
////
  virtual ~Notebook()
    {}
protected:
  Tag::Ptr template_tag() const;
  bool is_template_note(const Note&);

  NoteManagerBase & m_note_manager;
private:
  static Tag::Ptr s_template_tag;

  Notebook(const Notebook &);
  Notebook & operator=(const Notebook &);
  Glib::ustring m_name;
  Glib::ustring m_normalized_name;
  Glib::ustring m_default_template_note_title;
  Tag::Ptr    m_tag;
};

}
}

#endif
