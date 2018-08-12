/*
 * gnote
 *
 * Copyright (C) 2017,2018 Aurimas Cernius
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


#include <cstdio>
#include <iostream>

#include <glib/gstdio.h>
#include <UnitTest++/UnitTest++.h>

#include "notemanager.hpp"
#include "sharp/files.hpp"
#include "sharp/directory.hpp"
#include "synchronization/silentui.hpp"
#include "test/testnotemanager.hpp"
#include "test/testsyncmanager.hpp"
#include "test/testtagmanager.hpp"

using namespace gnote;


SUITE(SyncManagerTests)
{
  struct Fixture
  {
    Glib::ustring notesdir;
    Glib::ustring notesdir2;
    Glib::ustring syncdir;
    Glib::ustring manifest;
    test::NoteManager *manager1;
    test::NoteManager *manager2;
    test::SyncManager *sync_manager1;
    test::SyncManager *sync_manager2;

    Fixture()
    {
      Glib::ustring notes_dir = make_temp_dir();
      Glib::ustring notes_dir2 = make_temp_dir();
      notesdir = notes_dir + "/notes";
      notesdir2 = notes_dir2 + "/notes";
      syncdir = notes_dir + "/sync";
      REQUIRE CHECK(g_mkdir(syncdir.c_str(), S_IRWXU) == 0);
      manifest = notes_dir + "/manifest.xml";

      test::TagManager::ensure_exists();

      manager1 = new test::NoteManager(notesdir);
      create_note(*manager1, "note1", "content1");
      create_note(*manager1, "note2", "content2");
      create_note(*manager1, "note3", "content3");

      manager2 = new test::NoteManager(notesdir2);

      sync_manager1 = new test::SyncManager(*manager1, syncdir);
      sync_manager2 = new test::SyncManager(*manager2, syncdir);
    }

    ~Fixture()
    {
      delete manager1;
      delete manager2;
    }

    Glib::ustring make_temp_dir()
    {
      char temp_dir_tmpl[] = "/tmp/gnotetestnotesXXXXXX";
      char *temp_dir = g_mkdtemp(temp_dir_tmpl);
      REQUIRE CHECK(temp_dir != NULL);
      return temp_dir;
    }

    void create_note(test::NoteManager & manager, const Glib::ustring & title, const Glib::ustring & body)
    {
      Glib::ustring content = Glib::ustring::compose("<note-content><note-title>%1</note-title>\n\n%2</note-content>",
                                title, body);
      manager.create(title, content)->save();
    }
  };

  TEST_FIXTURE(Fixture, clean_sync)
  {
    test::SyncClient::Ptr sync_client1 = dynamic_pointer_cast<test::SyncClient>(sync_manager1->get_client(manifest));
    gnote::sync::SilentUI::Ptr sync_ui = gnote::sync::SilentUI::create(*manager1);
    sync_manager1->perform_synchronization(sync_ui);

    Glib::ustring syncednotesdir = syncdir + "/0/0";
    REQUIRE CHECK(sharp::directory_exists(syncednotesdir));
    std::list<Glib::ustring> files;
    sharp::directory_get_files_with_ext(syncednotesdir, ".note", files);
    REQUIRE CHECK_EQUAL(3, files.size());
    bool note1found = false, note2found = false, note3found = false;
    for(auto file : files) {
      Glib::ustring content = sharp::file_read_all_text(file);
      if(content.find("<note-title>note1</note-title>") != Glib::ustring::npos) {
        note1found = true;
      }
      else if(content.find("<note-title>note2</note-title>") != Glib::ustring::npos) {
        note2found = true;
      }
      else if(content.find("<note-title>note3</note-title>") != Glib::ustring::npos) {
        note3found = true;
      }
    }

    CHECK(note1found);
    CHECK(note2found);
    CHECK(note3found);
  }
}

