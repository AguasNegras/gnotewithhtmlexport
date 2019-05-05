/*
 * gnote
 *
 * Copyright (C) 2019 Aurimas Cernius
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


#include <glibmm/i18n.h>
#include <glibmm/miscutils.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>
#include <glibmm/thread.h>

#include "debug.hpp"
#include "gvfssyncserviceaddin.hpp"
#include "preferences.hpp"
#include "sharp/directory.hpp"
#include "sharp/files.hpp"
#include "synchronization/filesystemsyncserver.hpp"


namespace gvfssyncservice {

GvfsSyncServiceModule::GvfsSyncServiceModule()
{
  ADD_INTERFACE_IMPL(GvfsSyncServiceAddin);
}




GvfsSyncServiceAddin::GvfsSyncServiceAddin()
  : m_uri_entry(nullptr)
  , m_initialized(false)
  , m_enabled(false)
{
}

void GvfsSyncServiceAddin::initialize()
{
  m_initialized = true;
  m_enabled = true;
}

void GvfsSyncServiceAddin::shutdown()
{
  m_enabled = false;
}

gnote::sync::SyncServer::Ptr GvfsSyncServiceAddin::create_sync_server()
{
  gnote::sync::SyncServer::Ptr server;

  Glib::ustring sync_uri;
  if(get_config_settings(sync_uri)) {
    m_uri = sync_uri;
    if(sharp::directory_exists(m_uri) == false) {
      sharp::directory_create(m_uri);
    }

    auto path = Gio::File::create_for_uri(m_uri);
    if(!mount(path)) {
      throw sharp::Exception(_("Failed to mount the folder"));
    }
    if(!path->query_exists())
      sharp::directory_create(path);

    server = gnote::sync::FileSystemSyncServer::create(path);
  }
  else {
    throw std::logic_error("GvfsSyncServiceAddin.create_sync_server() called without being configured");
  }

  return server;
}


bool GvfsSyncServiceAddin::mount(const Glib::RefPtr<Gio::File> & path)
{
  try {
    path->find_enclosing_mount();
    return true;
  }
  catch(Gio::Error & e) {
  }

  auto root = path;
  auto parent = root->get_parent();
  while(parent) {
    root = parent;
    parent = root->get_parent();
  }

  Glib::Mutex mutex;
  Glib::Cond cond;
  mutex.lock();
  root->mount_enclosing_volume([this, &root, &mutex, &cond](Glib::RefPtr<Gio::AsyncResult> & result) {
    mutex.lock();
    try {
      if(root->mount_enclosing_volume_finish(result)) {
        m_mount = root->find_enclosing_mount();
      }
    }
    catch(...) {
    }

    cond.signal();
    mutex.unlock();
  });
  cond.wait(mutex);
  mutex.unlock();

  return bool(m_mount);
}


void GvfsSyncServiceAddin::unmount()
{
  if(!m_mount) {
    return;
  }

  Glib::Mutex mutex;
  Glib::Cond cond;
  mutex.lock();
  m_mount->unmount([this, &mutex, &cond](Glib::RefPtr<Gio::AsyncResult> & result) {
    mutex.lock();
    try {
      m_mount->unmount_finish(result);
    }
    catch(...) {
    }

    m_mount.reset();
    cond.signal();
    mutex.unlock();
  });
  cond.wait(mutex);
  mutex.unlock();
}


void GvfsSyncServiceAddin::post_sync_cleanup()
{
  unmount();
}


Gtk::Widget *GvfsSyncServiceAddin::create_preferences_control(EventHandler required_pref_changed)
{
  Gtk::Table *table = manage(new Gtk::Table(1, 1, false));
  table->set_row_spacings(5);
  table->set_col_spacings(10);

  // Read settings out of gconf
  Glib::ustring sync_path;
  if(get_config_settings(sync_path) == false) {
    sync_path = "";
  }

  auto l = manage(new Gtk::Label(_("Folder _URI:"), true));
  l->property_xalign() = 1;
  table->attach(*l, 0, 1, 0, 1, Gtk::FILL);

  m_uri_entry = manage(new Gtk::Entry);
  m_uri_entry->set_text(sync_path);
  m_uri_entry->get_buffer()->signal_inserted_text().connect([required_pref_changed](guint, const gchar*, guint) { required_pref_changed(); });
  m_uri_entry->get_buffer()->signal_deleted_text().connect([required_pref_changed](guint, guint) { required_pref_changed(); });
  l->set_mnemonic_widget(*m_uri_entry);

  table->attach(*m_uri_entry, 1, 2, 0, 1,
                Gtk::EXPAND | Gtk::FILL,
                Gtk::EXPAND | Gtk::FILL,
                0, 0);

  table->set_hexpand(true);
  table->set_vexpand(false);
  table->show_all();
  return table;
}


bool GvfsSyncServiceAddin::save_configuration()
{
  Glib::ustring sync_uri = m_uri_entry->get_text();
  std::exception_ptr save_exception;

  // TODO: this is hacky, need to make save into a proper async operation
  Glib::Thread::create([this, &save_exception, sync_uri]() {
    if(sync_uri == "") {
      ERR_OUT(_("The URI is empty"));
      throw gnote::sync::GnoteSyncException(_("URI field is empty."));
    }

    auto path = Gio::File::create_for_uri(sync_uri);
    if(!mount(path))
      throw gnote::sync::GnoteSyncException(_("Could not mount the path: %s. Please, check your settings"));
    try {
      if(sharp::directory_exists(path) == false) {
        if(!sharp::directory_create(path)) {
          DBG_OUT("Could not create \"%s\"", sync_uri.c_str());
          throw gnote::sync::GnoteSyncException(_("Specified folder path does not exist, and Gnote was unable to create it."));
        }
      }
      else {
        // Test creating/writing/deleting a file
        Glib::ustring test_path_base = Glib::build_filename(sync_uri, "test");
        Glib::RefPtr<Gio::File> test_path = Gio::File::create_for_uri(test_path_base);
        int count = 0;

        // Get unique new file name
        while(test_path->query_exists()) {
          test_path = Gio::File::create_for_uri(test_path_base + TO_STRING(++count));
        }

        // Test ability to create and write
        Glib::ustring test_line = "Testing write capabilities.";
        auto stream = test_path->create_file();
        stream->write(test_line);
        stream->close();

        if(!test_path->query_exists()) {
          throw gnote::sync::GnoteSyncException("Failure writing test file");
        }
        Glib::ustring line = sharp::file_read_all_text(test_path);
        if(line != test_line) {
          throw gnote::sync::GnoteSyncException("Failure when checking test file contents");
        }

        // Test ability to delete
        if(!test_path->remove()) {
          throw gnote::sync::GnoteSyncException("Failure when trying to remove test file");
        }
      }

      unmount();
    }
    catch(...) {
      unmount();
      save_exception = std::current_exception();
    }

    gnote::utils::main_context_invoke([]() { gtk_main_quit(); });
  }, false);

  gtk_main();
  if(save_exception) {
    std::rethrow_exception(save_exception);
  }

  m_uri = sync_uri;
  gnote::Preferences::obj().get_schema_settings(
    gnote::Preferences::SCHEMA_SYNC_GVFS)->set_string(gnote::Preferences::SYNC_GVFS_URI, m_uri);
  return true;
}


void GvfsSyncServiceAddin::reset_configuration()
{
  gnote::Preferences::obj().get_schema_settings(
    gnote::Preferences::SCHEMA_SYNC_GVFS)->set_string(gnote::Preferences::SYNC_GVFS_URI, "");
}


bool GvfsSyncServiceAddin::is_configured()
{
  return gnote::Preferences::obj().get_schema_settings(
    gnote::Preferences::SCHEMA_SYNC_GVFS)->get_string(gnote::Preferences::SYNC_GVFS_URI) != "";
}


Glib::ustring GvfsSyncServiceAddin::name()
{
  char *res = _("Online Folder");
  return res ? res : "";
}


Glib::ustring GvfsSyncServiceAddin::id()
{
  return "gvfs";
}


bool GvfsSyncServiceAddin::is_supported()
{
  return true;
}


bool GvfsSyncServiceAddin::initialized()
{
  return m_initialized && m_enabled;
}


bool GvfsSyncServiceAddin::get_config_settings(Glib::ustring & sync_path)
{
  sync_path = gnote::Preferences::obj().get_schema_settings(
    gnote::Preferences::SCHEMA_SYNC_GVFS)->get_string(gnote::Preferences::SYNC_GVFS_URI);

  return sync_path != "";
}

}
