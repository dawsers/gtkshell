#include "utils.h"
#include "notifications.h"

#include <gdkmm/monitor.h>
#include <giomm.h>
#include <glib.h>
#include <glibmm.h>
#include <gtkmm/icontheme.h>

#include <algorithm>
#include <iostream>

#include <curl/curl.h>
#include <sys/auxv.h>

namespace Utils {

std::string GTKSHELL_INSTANCE;

// Expands a directory name.
// Accepts ~ for $HOME, paths relative to the application executable
// and absolute paths
std::string expand_directory(const char *directory)
{
    std::string result;
    std::string dir(directory);
    switch (dir[0]) {
        case '~':
            result = HOME + dir.substr(1);
            break;
        case '/':
            result = dir;
            break;
        default:
            result = app_path() + "/" + dir;
            break;
    }
    return result;
}

std::string app_path()
{
    std::string exe_path = std::string((const char *)getauxval(AT_EXECFN));
    return exe_path.substr(0, exe_path.find_last_of('/'));
}

std::vector<std::string> split(const std::string &str, const std::string &delim)
{
    std::vector<std::string> tokens;
    size_t size = delim.size();
    size_t start = 0;
    size_t end = str.find(delim);
 
    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + size;
        end = str.find(delim, start);
    }
    tokens.push_back(str.substr(start));
    return tokens;
}
#if 0
void trim(std::string &str)
{
    str.erase(str.begin(),
              std::find_if(str.begin(), str.end(),
                           [](unsigned char c) { return !std::isspace(c); }));
    str.erase(std::find_if(str.rbegin(), str.rend(),
                           [](unsigned char c) { return !std::isspace(c); }).base(),
              str.end());
}
#endif
std::string trim(const std::string &string)
{
    std::string str = string;
    str.erase(str.begin(),
              std::find_if(str.begin(), str.end(),
                           [](unsigned char c) { return !std::isspace(c); }));
    str.erase(std::find_if(str.rbegin(), str.rend(),
                           [](unsigned char c) { return !std::isspace(c); }).base(),
              str.end());
    return str;
}

std::string trim_end(const std::string &string)
{
    std::string str = string;
    str.erase(std::find_if(str.rbegin(), str.rend(),
                           [](unsigned char c) { return !std::isspace(c); }).base(),
              str.end());
    return str;
}
#if 0
int interval(size_t interval, void (*callback)())
{
    callback();
    auto source = g_timeout_source_new(interval);
    g_source_set_priority(source, G_PRIORITY_DEFAULT);
    g_source_set_callback(source, callback, nullptr, nullptr);
        Glib::TimeoutSource::create(interval);
    source->set_priority(Glib::PRIORITY_DEFAULT);
    source->connect([callback] () {
        callback();
        return true;
    });
    source->attach(Glib::MainContext::get_default());
    source->set_ready_time(0);
    const id = source.attach(null);
    if (bind)
        bind.connect('destroy', () => GLib.source_remove(id));

    return source;
}

export function reset_interval(source) {
    source.set_ready_time(0);
}
#endif
// Read a file and return its contents
Glib::ustring read_file(const Glib::ustring &name)
{
    auto file = Gio::File::create_for_path(name);
    if (file) {
        return read_file(file);
    } else {
        log(LogSeverity::ERROR, std::format("Utils::read_file: Error reading file {}", name.data()));
        return "";
    }
}

Glib::ustring read_file(const Glib::RefPtr<Gio::File> &file)
{
    Glib::ustring data;
    if (file) {
        char *contents = nullptr;
        gsize length = 0;
        if (file->load_contents(contents, length) && length > 0) {
            data = contents;
            g_free(contents);
        } else {
            log(LogSeverity::ERROR, std::format("Utils::read_file: Error reading file"));
        }
    }
    return data;
}

std::vector<uint8_t> read_binary_file(const Glib::ustring &name)
{
    auto file = Gio::File::create_for_path(name);
    if (file) {
        return read_binary_file(file);
    } else {
        log(LogSeverity::ERROR, std::format("Utils::read_binary_file: Error reading file {}", name.data()));
        return {};
    }
}

std::vector<uint8_t> read_binary_file(const Glib::RefPtr<Gio::File> &file)
{
    std::vector<uint8_t> data;
    if (file) {
        char *contents = nullptr;
        gsize length = 0;
        if (file->load_contents(contents, length) && length > 0) {
            data.resize(length);
            std::copy(contents, contents + length, data.begin());
            g_free(contents);
        } else {
            log(LogSeverity::ERROR, std::format("Utils::read_binary_file: Error reading file"));
        }
    }
    return data;
}

bool write_file(const Glib::ustring &name, const std::string &contents)
{
    auto file = Gio::File::create_for_path(name);
    if (file) {
        try {
            auto stream = file->replace();
            stream->write(contents);
            stream->close();
        } catch (Gio::Error &error) {
            log(LogSeverity::ERROR, std::format("Utils::write_file: Cannot write file {}, Error: {}", name.c_str(), error.what()));
            return false;
        }
    } else {
        log(LogSeverity::ERROR, "Utils::write_file: Error");
        return false;
    }
    return true;
}

bool write_binary_file(const Glib::ustring &name, const std::vector<uint8_t> &contents)
{
    auto file = Gio::File::create_for_path(name);
    if (file) {
        try {
            auto stream = file->replace();
            stream->write(contents.data(), contents.size());
            stream->close();
        } catch (Gio::Error &error) {
            log(LogSeverity::ERROR, std::format("Utils::write_binary_file: Cannot write file {}, Error: {}", name.c_str(), error.what()));
            return false;
        }
    } else {
        log(LogSeverity::ERROR, "Utils::write_binary_file: Error");
        return false;
    }
    return true;
}


Glib::RefPtr<Gio::FileMonitor> monitor_file(const Glib::ustring &name)
{
    try {
        auto file = Gio::File::create_for_path(name);
        auto monitor = file->monitor();
        return monitor;
    } catch (Gio::Error &err) {
        log(LogSeverity::ERROR, std::format("Utils::monitor_file: {}", err.what()));
        return nullptr;
    }
}

Glib::RefPtr<Gio::FileMonitor> monitor_directory(const Glib::ustring &name)
{
    try {
        auto file = Gio::File::create_for_path(name);
        auto monitor = file->monitor_directory();
        return monitor;
    } catch (Gio::Error &err) {
        log(LogSeverity::ERROR, std::format("Utils::monitor_directory: {}", err.what()));
        return nullptr;
    }
}

bool ensure_directory(const std::string &directory)
{
    if (!Glib::file_test(directory, Glib::FileTest::EXISTS)) {
        return Gio::File::create_for_path(directory)->make_directory_with_parents();
    }
    return true;
}

Glib::ustring iso_8859_1_to_utf8(std::vector<uint8_t> &bytes)
{
    std::string str;
    for (auto it = bytes.begin(); it != bytes.end(); ++it) {
        if (*it < 0x80) {
            str.push_back(*it);
        } else {
            str.push_back(0xc0 | *it >> 6);
            str.push_back(0x80 | (*it & 0x3f));
        }
    }
    return str;
}

static int writer_cb(void *buffer, size_t size, size_t nmemb, void *userp)
{
    if (buffer != NULL && nmemb > 0) {
        // Append the data to the buffer
        std::vector<uint8_t> *downloaded = static_cast<std::vector<uint8_t> *>(userp);
        size_t len = size * nmemb;
        downloaded->insert(downloaded->end(), (uint8_t *)buffer, (uint8_t *)buffer + len);
        return len;
    }
    return 0;
}

std::vector<uint8_t> fetch(const std::string &url)
{ 
    CURL *curl = curl_easy_init();
    std::vector<uint8_t> result;

    if (curl != NULL) {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "cache-control: no-cache");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
        // Don't verify certificate
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);

        CURLcode ret = curl_easy_perform(curl);

        if (ret != CURLE_OK) {
            log(LogSeverity::ERROR, std::format("Utils::fetch: Error fetching data from {}", url));
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    return result;
}

// Synchronous execution
std::string exec(const std::string &cmd)
{
    // Runs cmd with SpawnFlags::SEARCH_PATH
    std::string out, err;
    int status;
    Glib::spawn_command_line_sync(cmd, &out, &err, &status);
    if (!WIFEXITED(status)) {
        return "Error running process: " + err;
    }
    return out;
}

std::string exec(const std::vector<std::string> &argv, const std::string &cwd)
{
    std::string out, err;
    int status;
    Glib::spawn_sync(cwd, argv, Glib::SpawnFlags::DEFAULT, {}, &out, &err, &status);
    if (!WIFEXITED(status)) {
        return "Error running process: " + err;
    }
    return out;
}

std::string exec(const std::vector<std::string> &argv, const std::vector<std::string> &envp, const std::string &cwd)
{
    std::string out, err;
    int status;
    Glib::spawn_sync(cwd, argv, envp, Glib::SpawnFlags::DEFAULT, {}, &out, &err, &status);
    if (!WIFEXITED(status)) {
        return "Error running process: " + err;
    }
    return out;
}

Glib::RefPtr<Glib::Bytes> exec_bin(const std::string &cmd, const Glib::RefPtr<Glib::Bytes> &stdin)
{
    auto argv = Glib::shell_parse_argv(cmd);
    auto p = Gio::Subprocess::create(argv, (stdin == nullptr ? Gio::Subprocess::Flags::NONE : Gio::Subprocess::Flags::STDIN_PIPE) | Gio::Subprocess::Flags::STDOUT_PIPE | Gio::Subprocess::Flags::STDERR_PIPE);
    auto std = p->communicate(stdin);
    if (p->get_successful()) {
        return std.first;
    } else {
        return nullptr;
    }
}

Glib::RefPtr<Glib::Bytes> pipe(const std::string &cmd, const std::string &pipe)
{
    try {
        Gio::Subprocess::Flags cmd_flags = Gio::Subprocess::Flags::STDOUT_PIPE | Gio::Subprocess::Flags::STDERR_PIPE;
        auto cmd_argv = Glib::shell_parse_argv(cmd);
        auto cmd_proc = Gio::Subprocess::create(cmd_argv, cmd_flags);
        Gio::Subprocess::Flags pipe_flags = Gio::Subprocess::Flags::STDIN_PIPE | Gio::Subprocess::Flags::STDOUT_PIPE | Gio::Subprocess::Flags::STDERR_PIPE;
        auto pipe_argv = Glib::shell_parse_argv(pipe);
        auto pipe_proc = Gio::Subprocess::create(pipe_argv, pipe_flags);
        auto size = pipe_proc->get_stdin_pipe()->splice(cmd_proc->get_stdout_pipe(), Gio::OutputStream::SpliceFlags::CLOSE_SOURCE | Gio::OutputStream::SpliceFlags::CLOSE_TARGET);
        if (cmd_proc->get_successful() && pipe_proc->get_successful()) {
            return pipe_proc->get_stdout_pipe()->read_bytes(size, nullptr);
        } else {
            return nullptr;
        }
    } catch (Gio::Error &err) {
        Utils::log(LogSeverity::ERROR, std::format("Utils::pipe(): Error {}", err.what()));
        return nullptr;
    }
}


// Asynchronous execution
void spawn(const std::string &cmd)
{
    // Runs cmd with SpawnFlags::SEARCH_PATH
    Glib::spawn_command_line_async(cmd);
}

void spawn(const std::vector<std::string> &argv, const std::string &cwd)
{
    Glib::spawn_async(cwd, argv);
}

void spawn(const std::vector<std::string> &argv, const std::vector<std::string> &envp, const std::string &cwd)
{
    Glib::spawn_async(cwd, argv, envp);
}

void log(LogSeverity severity, const Glib::ustring &msg)
{
    Glib::ustring error;
    switch (severity) {
    case Utils::LogSeverity::INFO:
        error = "INFO";
        break;
    case Utils::LogSeverity::WARNING:
        error = "WARNING";
        break;
    case Utils::LogSeverity::ERROR:
    default:
        error = "ERROR";
        break;
    }
    std::cerr << error << ": " << msg << std::endl; 
}

static Glib::RefPtr<Gtk::IconTheme> icon_theme = nullptr;

Glib::RefPtr<Gtk::IconPaintable> lookup_icon(const Glib::ustring &name, int size)
{
    if (icon_theme == nullptr) {
        icon_theme = Gtk::IconTheme::get_for_display(Gdk::Display::get_default());
    }

    return icon_theme->lookup_icon(name, size);
}

void notify(const Glib::ustring &app_name, const Glib::ustring &app_icon, const Glib::ustring &summary, const Glib::ustring &body)
{
#if 1
    NotificationsClient &client = NotificationsClient::get_instance();
    client.add_notification(app_name, app_icon, summary, body);
#else
    NotificationsServer &server = NotificationsServer::get_instance();
    server.add_notification(app_name, app_icon, summary, body);
#endif
}

Glib::ustring get_default_monitor_id()
{
    auto display = Gdk::Display::get_default();
    auto monitors = display->get_monitors();
    if (monitors->get_n_items() > 0) {
        auto monitor = monitors->get_typed_object<Gdk::Monitor>(0);
        return monitor->get_connector();
    }
    return "";
}

} // end namespace Utils
