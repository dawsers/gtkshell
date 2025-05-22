#ifndef __GTKSHELL_HYPRLAND__
#define __GTKSHELL_HYPRLAND__

#include <giomm.h>
#include <glibmm.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/button.h>
#include <gtkmm/eventcontrollerscroll.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/popover.h>
#include <gtkmm/adjustment.h>

#include <any>

class Hyprland : public Glib::Object {
public:
    static Hyprland &get_instance();

    // Avoid copy creation
    Hyprland(const Hyprland &) = delete;
    void operator=(const Hyprland &) = delete;

    void dispatch(const Glib::ustring &dispatcher, const Glib::ustring &args);
    void sync_workspaces(bool notify = true);
    void sync_client(bool notify = true);

    // Scroller Properties
    Glib::Property<std::vector<std::string>> scroller_mode;
    Glib::Property<bool> scroller_overview;
    Glib::Property<std::pair<int, int>> scroller_trail;
    Glib::Property<bool> scroller_trailmark;
    Glib::Property<std::pair<bool, Glib::ustring>> scroller_mark;

    // Hyprland Properties
    Glib::Property<std::pair<int, Glib::ustring>> workspacev2;
    Glib::Property<Glib::ustring> activespecial;
    Glib::Property<Glib::ustring> activewindow;
    Glib::Property<Glib::ustring> activelayout;
    Glib::Property<Glib::ustring> submap;

    sigc::signal<void()> signal_workspaces;
    std::map<int, Glib::ustring> workspaces;

private:
    Hyprland();
    virtual ~Hyprland();

    std::string sock(const std::string &pre, const std::string &HIS, const std::string &socket) const;
    void prepare();
    void watch_socket();
    void event_decode(const std::string &event);

    bool write_socket(const Glib::ustring &message,
                      Glib::RefPtr<Gio::SocketConnection> &connection,
                      Glib::RefPtr<Gio::DataInputStream> &stream);
    Glib::ustring message(const Glib::ustring &message);
    void message_async(const Glib::ustring &message, std::any data, void (*cb)(std::any data, const Glib::ustring &msg_result));
    //Glib::ustring message_async(const Glib::ustring &message);

    void sync_layout(bool notify = true);

    Glib::RefPtr<Gio::SocketClient> client_events, client_requests;
    Glib::RefPtr<Gio::UnixSocketAddress> address_events, address_requests;
    Glib::RefPtr<Gio::SocketConnection> connection;
    Glib::RefPtr<Gio::DataInputStream> listener;

    Glib::RefPtr<Gio::SocketConnection> connection_requests;
    Glib::RefPtr<Gio::DataInputStream> stream_requests;
};

class Workspaces : public Gtk::Box {
public:
    Workspaces();
    ~Workspaces() {
        for (auto workspace : workspaces)
            delete workspace.second;
    }

private:
    std::vector<std::pair<int, Gtk::Button *>> workspaces;
    Glib::RefPtr<Gtk::EventControllerScroll> scroll;
};

class Submap : public Gtk::Label {
public:
    Submap();
    ~Submap() {}
};

class ClientTitle : public Gtk::Label {
public:
    ClientTitle();
    ~ClientTitle() {}
};

class KeyboardLayout : public Gtk::Label {
public:
    KeyboardLayout();
    ~KeyboardLayout() {}
};

class Scroller : public Gtk::Box {
public:
    Scroller();
    ~Scroller() {}

private:
    Glib::RefPtr<Gtk::GestureClick> click;
    Glib::RefPtr<Gtk::Adjustment> auto_entry;
    Gtk::Popover popover;
    Gtk::Label mode_label;
    Gtk::Label overview_label;
    Gtk::Label mark_label;
    Gtk::Label trailmark_label;
    Gtk::Label trail_label;
    sigc::connection a_auto_connection;
    sigc::connection a_entry_connection;
};

#endif // __GTKSHELL_HYPRLAND__
