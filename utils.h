#ifndef __GTKSHELL_UTILS__
#define __GTKSHELL_UTILS__

#include <giomm/file.h>
#include <glibmm.h>
#include <gtkmm/iconpaintable.h>

#include <vector>
#include <string>

namespace Utils {

std::string app_path();

extern std::string GTKSHELL_INSTANCE;
const std::string USER = Glib::get_user_name();
const std::string HOME = Glib::get_home_dir();
const std::string CWD = Glib::get_current_dir();
const std::string EXE_PATH = app_path();
const std::string XDG_RUNTIME_DIR = Glib::get_user_runtime_dir();
const std::string XDG_DATA_HOME = Glib::get_user_data_dir();
const std::string XDG_CONFIG_HOME = Glib::get_user_config_dir();
const std::string XDG_STATE_HOME = Glib::getenv("XDG_STATE_HOME") != "" ? Glib::getenv("XDG_STATE_HOME") : HOME + "/.local/state";
const std::string XDG_CACHE_HOME = Glib::get_user_cache_dir();
const std::string XDG_DOCUMENTS_HOME = HOME + "/Documents";

std::string expand_directory(const char *directory);

std::vector<std::string> split(const std::string &str, const std::string &delim);
//void trim(std::string &str);
std::string trim(const std::string &string);
std::string trim_end(const std::string &string);

Glib::ustring read_file(const Glib::ustring &name);
Glib::ustring read_file(const Glib::RefPtr<Gio::File> &file);

bool write_file(const Glib::ustring &name, const std::string &contents);
bool write_binary_file(const Glib::ustring &name, const std::vector<uint8_t> &contents);

std::vector<uint8_t> read_binary_file(const Glib::ustring &name);
std::vector<uint8_t> read_binary_file(const Glib::RefPtr<Gio::File> &file);

Glib::RefPtr<Gio::FileMonitor> monitor_file(const Glib::ustring &name);
Glib::RefPtr<Gio::FileMonitor> monitor_directory(const Glib::ustring &name);

// Check if directory exists, and if not, create it. Return true if dir exists
// after calling the function, false if it couldn't be created.
bool ensure_directory(const std::string &directory);

// Fetch data from URL
Glib::ustring iso_8859_1_to_utf8(std::vector<uint8_t> &bytes);
std::vector<uint8_t> fetch(const std::string &url);

// Synchronous execution
std::string exec(const std::string &cmd);
std::string exec(const std::vector<std::string> &argv, const std::string &cwd);
std::string exec(const std::vector<std::string> &argv, const std::vector<std::string> &envp, const std::string &cwd);
// Return Bytes instead of string and allow stdin
Glib::RefPtr<Glib::Bytes> exec_bin(const std::string &cmd, const Glib::RefPtr<Glib::Bytes> &stdin = nullptr);

// Pipe std_out from cmd to std_in of pipe
Glib::RefPtr<Glib::Bytes> pipe(const std::string &cmd, const std::string &pipe);

// Asynchronous execution
void spawn(const std::string &cmd);
void spawn(const std::vector<std::string> &argv, const std::string &cwd);
void spawn(const std::vector<std::string> &argv, const std::vector<std::string> &envp, const std::string &cwd);

typedef enum {
    INFO = 0,
    WARNING,
    ERROR
} LogSeverity;

void log(LogSeverity severity, const Glib::ustring &msg);

Glib::RefPtr<Gtk::IconPaintable> lookup_icon(const Glib::ustring &name, int size = 16);

void notify(const Glib::ustring &app_name, const Glib::ustring &app_icon, const Glib::ustring &summary, const Glib::ustring &body);

Glib::ustring get_default_monitor_id();

} // end namespace Utils


#endif

