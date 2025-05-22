#ifndef __GTKSHELL_SCROLL__
#define __GTKSHELL_SCROLL__

#include <giomm.h>
#include <glibmm.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/button.h>
#include <gtkmm/eventcontrollerscroll.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/popover.h>
#include <gtkmm/adjustment.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <cstdint>
#include <thread>

#define event_mask(ev) (1u << (ev & 0x7F))

enum ipc_command_type : uint32_t {
  // i3 command types - see i3's I3_REPLY_TYPE constants
  IPC_COMMAND = 0,
  IPC_GET_WORKSPACES = 1,
  IPC_SUBSCRIBE = 2,
  IPC_GET_OUTPUTS = 3,
  IPC_GET_TREE = 4,
  IPC_GET_MARKS = 5,
  IPC_GET_BAR_CONFIG = 6,
  IPC_GET_VERSION = 7,
  IPC_GET_BINDING_MODES = 8,
  IPC_GET_CONFIG = 9,
  IPC_SEND_TICK = 10,

  // sway-specific command types
  IPC_GET_INPUTS = 100,
  IPC_GET_SEATS = 101,

  // scroll-specific command types
  IPC_GET_SCROLLER = 120,
  IPC_GET_TRAILS = 121,

  // Events sent from sway to clients. Events have the highest bits set.
  IPC_EVENT_WORKSPACE = ((1U << 31) | 0),
  IPC_EVENT_OUTPUT = ((1U << 31) | 1),
  IPC_EVENT_MODE = ((1U << 31) | 2),
  IPC_EVENT_WINDOW = ((1U << 31) | 3),
  IPC_EVENT_BARCONFIG_UPDATE = ((1U << 31) | 4),
  IPC_EVENT_BINDING = ((1U << 31) | 5),
  IPC_EVENT_SHUTDOWN = ((1U << 31) | 6),
  IPC_EVENT_TICK = ((1U << 31) | 7),

  // sway-specific event types
  IPC_EVENT_BAR_STATE_UPDATE = ((1U << 31) | 20),
  IPC_EVENT_INPUT = ((1U << 31) | 21),

  // scroll-specific event types
  IPC_EVENT_SCROLLER = ((1U << 31) | 30),
  IPC_EVENT_TRAILS = ((1U << 31) | 31),
};

typedef struct {
    uint32_t size;
    uint32_t type;
    std::string payload;
} ScrollIpcResponse;

class ScrollIpc {
public:
    ScrollIpc();
    ~ScrollIpc();

    sigc::signal<void(const ScrollIpcResponse &)> signal_event;
    sigc::signal<void(const ScrollIpcResponse &)> signal_cmd;

    void send_cmd(uint32_t type, const std::string &payload = "");
    void subscribe(const std::string &payload);

private:
    void working_thread();

    static inline const std::string ipc_magic = "i3-ipc";
    static inline const size_t ipc_header_size = ipc_magic.size() + 8;

    const std::string get_socket_path() const;
    int open(const std::string &) const;
    ScrollIpcResponse send(int fd, uint32_t type, const std::string &payload = "");
    ScrollIpcResponse recv(int fd);

    int fd;
    int fd_event;

    Glib::Dispatcher dispatcher;
    std::shared_ptr<std::thread> worker_thread;
    std::atomic<bool> working;
    std::binary_semaphore sem;
    std::mutex mtx;
    ScrollIpcResponse shared_data;
};

class ScrollSubmap : public Gtk::Label {
public:
    ScrollSubmap();
    ~ScrollSubmap() {}

    void on_event(const ScrollIpcResponse &data);

private:
    ScrollIpc ipc;
};

class ScrollWorkspaces : public Gtk::Box {
public:
    ScrollWorkspaces(const std::string &output);
    ~ScrollWorkspaces() {
        for (auto workspace : workspaces)
            delete workspace.second;
    }

    void on_event(const ScrollIpcResponse &data);
    void on_cmd(const ScrollIpcResponse &data);

private:
    void workspaces_update();

    typedef struct {
        int id;
        bool urgent;
    } Workspace;

    const std::string &output;
    int focused;
    ScrollIpc ipc;
    std::vector<std::pair<Workspace, Gtk::Button *>> workspaces;
};

class ScrollClientTitle : public Gtk::Label {
public:
    ScrollClientTitle();
    ~ScrollClientTitle() {}

    void on_event(const ScrollIpcResponse &data);

private:
    Glib::ustring generate_title(const json &container);

    ScrollIpc ipc;
    int focused;
};

class ScrollKeyboardLayout : public Gtk::Label {
public:
    ScrollKeyboardLayout();
    ~ScrollKeyboardLayout() {}

    void on_event(const ScrollIpcResponse &data);
    void on_cmd(const ScrollIpcResponse &data);

private:
    ScrollIpc ipc;
};

class ScrollTrails : public Gtk::Label {
public:
    ScrollTrails();
    ~ScrollTrails() {}

    void on_event(const ScrollIpcResponse &data);
    void on_cmd(const ScrollIpcResponse &data);

private:
    ScrollIpc ipc;
};

class ScrollScroller : public Gtk::Box {
public:
    ScrollScroller();
    ~ScrollScroller() {}

private:
    ScrollIpc ipc;
    Glib::RefPtr<Gtk::GestureClick> click;
    Glib::RefPtr<Gtk::Adjustment> auto_entry;
    Gtk::Popover popover;
    Gtk::Label mode_label;
    Gtk::Label overview_label;
    Gtk::Label scale_label;
};

#endif // __GTKSHELL_SCROLL__
