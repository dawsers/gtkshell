#include <giomm.h>
#include <glibmm.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/frame.h>
#include <gtkmm/separator.h>

#include <format>

#include "hyprland.h"
#include "utils.h"
#include "bind.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

Hyprland::Hyprland() :
    Glib::ObjectBase(typeid(Hyprland)),
    scroller_mode(*this, "scroller-mode", {}),
    scroller_overview(*this, "scroller-overview", false),
    scroller_trail(*this, "scroller-trail", { -1, 0 }),
    scroller_trailmark(*this, "scroller-trailmark", false),
    scroller_mark(*this, "scroller-mark", { false, "" }),
    workspacev2(*this, "workspacev2", { 1, "" }),
    activespecial(*this, "activespecial", ""),
    activewindow(*this, "activewindow", ""),
    activelayout(*this, "activelayout", ""),
    submap(*this, "submap", "")
{
    prepare();
    watch_socket();
    sync_workspaces();
    sync_client();
    sync_layout();
}
Hyprland::~Hyprland() {}

Hyprland &Hyprland::get_instance()
{
    static Hyprland instance;
    return instance;
}

std::string Hyprland::sock(const std::string &pre, const std::string &HIS, const std::string &socket) const
{
    return std::format("{}/hypr/{}/.{}.sock", pre, HIS, socket);
}

void Hyprland::prepare()
{
    const auto HIS = Glib::getenv("HYPRLAND_INSTANCE_SIGNATURE");
    const auto XRD = Glib::getenv("XDG_RUNTIME_DIR");
    const std::string XDG_RUNTIME_DIR = XRD != "" ? XRD : "/";

    const auto path_events = Glib::file_test(sock(XDG_RUNTIME_DIR, HIS, "socket2"), Glib::FileTest::EXISTS) ?
        sock(XDG_RUNTIME_DIR, HIS, "socket2") : sock("/tmp", HIS, "socket2");
    const auto path_requests = Glib::file_test(sock(XDG_RUNTIME_DIR, HIS, "socket"), Glib::FileTest::EXISTS) ?
        sock(XDG_RUNTIME_DIR, HIS, "socket") : sock("/tmp", HIS, "socket");

    client_events = Gio::SocketClient::create();
    address_events = Gio::UnixSocketAddress::create(path_events);
    client_requests = Gio::SocketClient::create();
    address_requests = Gio::UnixSocketAddress::create(path_requests);
    connection = client_events->connect(address_events);
    listener = Gio::DataInputStream::create(connection->get_input_stream());
    listener->set_close_base_stream(true);
}

void Hyprland::watch_socket()
{
    listener->read_line_async([this](auto &result) {
                              std::string line;
                              listener->read_line_finish(result, line);
                              event_decode(line);
                              watch_socket();
                              }, nullptr);
}

void Hyprland::sync_workspaces(bool notify)
{
    json json_workspaces = json::parse(message("j/workspaces"));
    workspaces.clear();
    for (auto workspace : json_workspaces) {
        int id = workspace["id"].get<int>();
        auto name = workspace["name"].get<std::string>();
        workspaces[id] = name;
    }
    json json_activeworkspace = json::parse(message("j/activeworkspace"));
    workspacev2.set_value({ json_activeworkspace["id"].get<int>(), json_activeworkspace["name"].get<std::string>() });

    signal_workspaces.emit();
}

void Hyprland::sync_client(bool notify)
{
    json json_activewindow = json::parse(message("j/activewindow"));
    // Check if any window is already active
    if (json_activewindow.contains("title"))
        activewindow.set_value(json_activewindow["title"].get<std::string>());
}

void Hyprland::sync_layout(bool notify)
{
    json json_keyboards = json::parse(message("j/devices"))["keyboards"];
    for (auto kb : json_keyboards) {
        if (kb["main"].get<bool>() == true) {
            activelayout.set_value(kb["active_keymap"].get<std::string>());
            return;
        }
    }
}

void Hyprland::event_decode(const std::string &event)
{
    auto msg = Utils::split(event, ">>");
    if (msg[0] == "scroller") {
        // Scroller events
        auto argv = Utils::split(msg[1], ",");
        if (argv[0] == "mode") {
            for (auto &arg : argv)
                arg = Utils::trim(arg);
            scroller_mode.set_value(argv);
        } else if (argv[0] == "overview") {
            scroller_overview.set_value(Utils::trim(argv[1]) == "1" ? true : false);
        } else if (argv[0] == "trail") {
            scroller_trail.set_value({ std::stoi(Utils::trim(argv[1])), std::stoi(Utils::trim(argv[2])) });
        } else if (argv[0] == "trailmark") {
            scroller_trailmark.set_value(Utils::trim(argv[1]) == "1" ? true : false);
        } else if (argv[0] == "mark") {
            scroller_mark.set_value({ Utils::trim(argv[1]) == "1" ? true : false, Utils::trim(argv[2]) });
        }
    } else {
        // Hyprland events
        if (msg[0] == "workspacev2") {
            auto workspace = Utils::split(Utils::trim(msg[1]), ",");
            workspacev2.set_value({ std::stoi(workspace[0]), workspace[1] });
        } else if (msg[0] == "focusedmonv2") {
            auto workspace = Utils::split(Utils::trim(msg[1]), ",");
            int id = std::stoi(workspace[1]);
            workspacev2.set_value({ id, workspaces[id] });
        } else if (msg[0] == "activewindow") {
            auto name = Utils::split(Utils::trim(msg[1]), ",");
            activewindow.set_value(name[1]);
        } else if (msg[0] == "submap") {
            auto name = Utils::trim(msg[1]);
            submap.set_value(name);
        } else if (msg[0] == "createworkspacev2") {
            sync_workspaces();
        } else if (msg[0] == "destroyworkspacev2") {
            sync_workspaces();
        } else if (msg[0] == "activespecial") {
            auto workspace = Utils::split(Utils::trim(msg[1]), ",");
            activespecial.set_value(workspace[0]);
        } else if (msg[0] == "activelayout") {
            auto keyboard = Utils::trim(msg[1]);
            activelayout.set_value(keyboard.substr(keyboard.find(",") + 1));
        }
    }
}

// Hyprland write to socket functions
bool Hyprland::write_socket(const Glib::ustring &message,
                            Glib::RefPtr<Gio::SocketConnection> &connection,
                            Glib::RefPtr<Gio::DataInputStream> &stream)
{
    connection = client_requests->connect(address_requests);
    if (connection != nullptr) {
        connection->get_output_stream()->write(message, nullptr);
        stream = Gio::DataInputStream::create(connection->get_input_stream());
        return true;
    }
    return false;
}

Glib::ustring Hyprland::message(const Glib::ustring &message)
{
    Glib::RefPtr<Gio::SocketConnection> connection;
    Glib::RefPtr<Gio::DataInputStream> stream;
    std::string result;
    if (write_socket(message, connection, stream)) {
        stream->read_upto(result, "\x04");
        connection->close();
    }
    return result;
}

void Hyprland::message_async(const Glib::ustring &message, std::any data, void (*cb)(std::any data, const Glib::ustring &msg_result))
{
    Glib::RefPtr<Gio::SocketConnection> connection;
    Glib::RefPtr<Gio::DataInputStream> stream;
    if (write_socket(message, connection, stream)) {
        stream->read_upto_async("\x04",
            [data, cb, connection, stream] (Glib::RefPtr<Gio::AsyncResult> &res) {
                std::string result;
                bool ok = stream->read_upto_finish(res, result);
                connection->close();
                if (ok)
                    cb(data, result);
            }, nullptr);
    }
}

void Hyprland::dispatch(const Glib::ustring &dispatcher, const Glib::ustring &args)
{
    message_async("dispatch " + dispatcher + " " + args, nullptr, [] (std::any data, const Glib::ustring &msg_result) {
                  if (msg_result != "ok") {
                    Utils::log(Utils::LogSeverity::ERROR, std::format("Hyprland::dispatch"));
                  }
    });
}

Scroller::Scroller() : auto_entry(Gtk::Adjustment::create(2.0, 1.0, 20.0, 1.0, 5.0, 0.0))
{
    auto &hyprland = Hyprland::get_instance();

    auto r_row = Gtk::make_managed<Gtk::CheckButton>("row");
    r_row->signal_toggled().connect(
        [&hyprland, r_row]() {
            if (r_row->get_active())
                hyprland.dispatch("scroller:setmode", "row");
        });
    auto r_col = Gtk::make_managed<Gtk::CheckButton>("column");
    r_col->signal_toggled().connect(
        [&hyprland, r_col]() {
            if (r_col->get_active())
                hyprland.dispatch("scroller:setmode", "col");
        });
    r_col->set_group(*r_row);
    auto r_frame = Gtk::make_managed<Gtk::Frame>("Mode");
    auto r_frame_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    r_frame_box->append(*r_row);
    r_frame_box->append(*r_col);
    r_frame->set_child(*r_frame_box);
    auto r_separator = Gtk::make_managed<Gtk::Separator>();

    auto p_after = Gtk::make_managed<Gtk::CheckButton>("after");
    p_after->signal_toggled().connect([&hyprland, p_after] () { if (p_after->get_active()) hyprland.dispatch("scroller:setmodemodifier", "after"); });
    auto p_before = Gtk::make_managed<Gtk::CheckButton>("before");
    p_before->signal_toggled().connect([&hyprland, p_before] () { if (p_before->get_active()) hyprland.dispatch("scroller:setmodemodifier", "before"); });
    auto p_beginning = Gtk::make_managed<Gtk::CheckButton>("beginning");
    p_beginning->signal_toggled().connect([&hyprland, p_beginning] () { if (p_beginning->get_active()) hyprland.dispatch("scroller:setmodemodifier", "beginning"); });
    auto p_end = Gtk::make_managed<Gtk::CheckButton>("end");
    p_end->signal_toggled().connect([&hyprland, p_end] () { if (p_end->get_active()) hyprland.dispatch("scroller:setmodemodifier", "end"); });
    p_before->set_group(*p_after);
    p_beginning->set_group(*p_after);
    p_end->set_group(*p_after);
    auto p_frame = Gtk::make_managed<Gtk::Frame>("Position");
    auto p_frame_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    p_frame_box->append(*p_after);
    p_frame_box->append(*p_before);
    p_frame_box->append(*p_beginning);
    p_frame_box->append(*p_end);
    p_frame->set_child(*p_frame_box);
    auto p_separator = Gtk::make_managed<Gtk::Separator>();

    auto f_frame = Gtk::make_managed<Gtk::Frame>("Focus");
    auto f_frame_box = Gtk::make_managed<Gtk::Box>();
    auto f_focus = Gtk::make_managed<Gtk::CheckButton>("focus");
    f_focus->signal_toggled().connect([&hyprland, f_focus]() { if (f_focus->get_active()) hyprland.dispatch("scroller:setmodemodifier", ", focus"); });
    auto f_nofocus = Gtk::make_managed<Gtk::CheckButton>("nofocus");
    f_nofocus->set_group(*f_focus);
    f_nofocus->signal_toggled().connect([&hyprland, f_nofocus]() { if (f_nofocus->get_active()) hyprland.dispatch("scroller:setmodemodifier", ", nofocus"); });
    f_frame_box->append(*f_focus);
    f_frame_box->append(*f_nofocus);
    f_frame->set_child(*f_frame_box);
    auto f_separator = Gtk::make_managed<Gtk::Separator>();

    auto a_frame = Gtk::make_managed<Gtk::Frame>("Automatic");
    auto a_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    auto a_manual = Gtk::make_managed<Gtk::CheckButton>("manual"); 
    a_manual->signal_toggled().connect([&hyprland, a_manual] () { if (a_manual->get_active()) hyprland.dispatch("scroller:setmodemodifier", ", , manual"); });
    auto a_auto_box = Gtk::make_managed<Gtk::Box>();
    auto a_auto = Gtk::make_managed<Gtk::CheckButton>("auto");
    auto a_entry = Gtk::make_managed<Gtk::SpinButton>(auto_entry);
    a_auto_connection = a_auto->signal_toggled().connect(
        [this, &hyprland, a_auto, a_entry]() {
            if (a_auto->get_active()) {
                a_entry_connection.block(true);
                hyprland.dispatch("scroller:setmodemodifier", std::format(", , auto, {}", a_entry->get_value()));
                a_entry_connection.unblock();
            }
        });
    a_entry_connection = a_entry->signal_value_changed().connect(
        [this, &hyprland, a_auto, a_entry]() {
            if (a_auto->get_active()) {
                a_auto_connection.block(true);
                hyprland.dispatch("scroller:setmodemodifier", std::format(", , auto, {}", a_entry->get_value()));
                a_auto_connection.unblock();
            }
        });
    a_auto->set_group(*a_manual);
    a_auto_box->append(*a_auto);
    a_auto_box->append(*a_entry);
    a_box->append(*a_manual);
    a_box->append(*a_auto_box);
    a_frame->set_child(*a_box);
    auto a_separator = Gtk::make_managed<Gtk::Separator>();

    auto c_frame = Gtk::make_managed<Gtk::Frame>("Center");
    auto c_frame_box = Gtk::make_managed<Gtk::Box>();
    auto c_col = Gtk::make_managed<Gtk::CheckButton>("column");
    c_col->signal_toggled().connect([&hyprland, c_col]() {
        if (c_col->get_active())
            hyprland.dispatch("scroller:setmodemodifier", ", center_column");
        else
            hyprland.dispatch("scroller:setmodemodifier", ", nocenter_column");
    });
    auto c_win = Gtk::make_managed<Gtk::CheckButton>("window");
    c_win->signal_toggled().connect([&hyprland, c_win]() {
        if (c_win->get_active())
            hyprland.dispatch("scroller:setmodemodifier", ", center_window");
        else
            hyprland.dispatch("scroller:setmodemodifier", ", nocenter_window");
    });
    c_frame_box->append(*c_col);
    c_frame_box->append(*c_win);
    c_frame->set_child(*c_frame_box);

    mode_label.add_css_class("scroller-mode");
    bind_property_changed(&hyprland, "scroller-mode",
          [this, &hyprland, r_row, r_col, p_after, p_before, p_beginning, p_end, f_focus, f_nofocus, a_manual, a_auto, a_entry] {
              auto scroller_mode = hyprland.scroller_mode.get_value();
              if (scroller_mode.size() == 0)
                  // Initialization, no windows yet, no mode
                  return;
              std::string mode;
              if (scroller_mode[1] == "row") {
                  mode = "-";
                  r_row->set_active(true);
              } else {
                  mode = "|";
                  r_col->set_active(true);
              }
              std::string pos;
              // position
              if (scroller_mode[2] == "after") {
                  pos = "→";
                  p_after->set_active(true);
              } else if (scroller_mode[2] == "before") {
                  pos = "←";
                  p_before->set_active(true);
              } else if (scroller_mode[2] == "end") {
                  pos = "⇥";
                  p_end->set_active(true);
              } else if (scroller_mode[2] == "beginning") {
                  pos = "⇤";
                  p_beginning->set_active(true);
              }
              // focus
              std::string focus;
              if (scroller_mode[3] == "focus") {
                  focus = "";
                  f_focus->set_active(true);
              } else {
                  focus = "";
                  f_nofocus->set_active(true);
              }
              // center column/window
              std::string center_column;
              if (scroller_mode[5] == "center_column") {
                  center_column = "";
              } else {
                  center_column = " ";
              }
              std::string center_window;
              if (scroller_mode[6] == "center_window") {
                  center_window = "󰉠";
              } else {
                  center_window = " ";
              }
              // auto
              bool manual = scroller_mode[4].starts_with("manual:");
              const std::string auto_mode = manual ? "✋" : "🅰";
              if (manual) {
                  a_manual->set_active(true);
                  a_entry->set_editable(false);
                  mode_label.set_text(std::format("{} {} {} {} {} {}  ", mode, pos, focus, center_column, center_window, auto_mode));
              } else {
                  a_auto_connection.block(true);
                  a_entry_connection.block(true);
                  a_entry->set_editable(true);
                  a_auto->set_active(true);
                  // auto:N
                  const std::string auto_param = scroller_mode[4].substr(5);
                  a_entry->set_value(std::stod(auto_param));
                  a_entry_connection.unblock();
                  a_auto_connection.unblock();
                  mode_label.set_text(std::format("{} {} {} {} {} {} {}", mode, pos, focus, center_column, center_window, auto_mode, auto_param));
              }
          });

    overview_label.add_css_class("scroller-overview");
    bind_property_changed(&hyprland, "scroller-overview", [this, &hyprland] {
        auto overview = hyprland.scroller_overview.get_value();
        overview_label.set_text(overview ? "🐦" : "");
    });
    
    mark_label.add_css_class("scroller-mark");
    bind_property_changed(&hyprland, "scroller-mark", [this, &hyprland] {
        auto mark = hyprland.scroller_mark.get_value();
        mark_label.set_text(mark.first ? "🔖 " + mark.second : "");
    });
    
    trailmark_label.add_css_class("scroller-trailmark");
    bind_property_changed(&hyprland, "scroller-trailmark", [this, &hyprland] {
        auto trailmark = hyprland.scroller_trailmark.get_value();
        trailmark_label.set_text(trailmark ? "✅" : "");
    });

    trail_label.add_css_class("scroller-trail");
    bind_property_changed(&hyprland, "scroller-trail", [this, &hyprland] {
        auto trail = hyprland.scroller_trail.get_value();
        trail_label.set_text(trail.first != -1 ? std::format("{} ({})", trail.first, trail.second) : "");
    });

    add_css_class("scroller");

    popover.set_parent(mode_label);

    auto pop_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    pop_box->append(*r_frame);
    pop_box->append(*r_separator);
    pop_box->append(*p_frame);
    pop_box->append(*p_separator);
    pop_box->append(*f_frame);
    pop_box->append(*f_separator);
    pop_box->append(*a_frame);
    pop_box->append(*a_separator);
    pop_box->append(*c_frame);
    pop_box->set_vexpand(true);
    popover.set_child(*pop_box);

    append(mode_label);
    append(overview_label);
    append(mark_label);
    append(trailmark_label);
    append(trail_label);

    click = Gtk::GestureClick::create();
    click->set_button(1); // 0 = all, 1 = left, 2 = center, 3 = right
    click->signal_pressed().connect([this] (int n_press, double x, double y) {
        auto visible = popover.get_visible();
        if (visible)
            popover.popdown();
        else
            popover.popup();
    }, true);
    mode_label.add_controller(click);
}

Submap::Submap()
{
    auto &hyprland = Hyprland::get_instance();
    add_css_class("submap");
    bind_property_changed(&hyprland, "submap", [this, &hyprland] {
        auto submap = hyprland.submap.get_value();
        this->set_text(submap != "" ? "󰌌    " + submap : "");
    });
}

ClientTitle::ClientTitle()
{
    auto &hyprland = Hyprland::get_instance();
    add_css_class("client");
    set_max_width_chars(80);
    set_ellipsize(Pango::EllipsizeMode::END);
    bind_property_changed(&hyprland, "activewindow", [this, &hyprland] {
        auto client = hyprland.activewindow.get_value();
        this->set_text(client);
    });
}

KeyboardLayout::KeyboardLayout()
{
    auto &hyprland = Hyprland::get_instance();
    add_css_class("keyboard");
    bind_property_changed(&hyprland, "activelayout", [this, &hyprland]() {
        auto keyboard = hyprland.activelayout.get_value();
        this->set_text(keyboard);
    });
}

Workspaces::Workspaces()
{
    add_css_class("workspaces");
    auto &hyprland = Hyprland::get_instance();

    auto workspaces_update = [this, &hyprland] () {
        const std::vector<Glib::ustring> normal = { "" };
        const std::vector<Glib::ustring> focused = { "focused" };
        auto active = hyprland.workspacev2.get_value();
        auto activespecial = hyprland.activespecial.get_value();
        for (auto workspace : workspaces) {
            if (activespecial != "") {
                // There is a special workspace active
                auto name = hyprland.workspaces[workspace.first];
                workspace.second->set_css_classes(name == activespecial ? focused : normal);
            } else {
                workspace.second->set_css_classes(workspace.first == active.first ? focused : normal);
            }
            if (workspace.first < 0)
                workspace.second->add_css_class("special");
        }
    };

    hyprland.signal_workspaces.connect([this, &hyprland, workspaces_update] () {
        for (auto workspace : workspaces) {
            remove(*workspace.second);
            delete workspace.second;
        }
        workspaces.clear();
        for (auto workspace : hyprland.workspaces) {
            Gtk::Button *button = new Gtk::Button();
            const int &id = workspace.first;
            const Glib::ustring name = workspace.second;
            if (id < 0)
                button->set_label(std::format("S{}", 99 + id));
            else
                button->set_label(name);
            button->signal_clicked().connect(
                [id, name, &hyprland]() {
                    auto active = hyprland.workspacev2.get_value();
                    auto activespecial = hyprland.activespecial.get_value();
                    auto activespecialname = activespecial == "" ? "" : activespecial.substr(activespecial.find("special:") + std::string("special:").size());
                    if (activespecialname != "")
                        // special
                        hyprland.dispatch("togglespecialworkspace", activespecialname);
                    if (id != active.first)
                        hyprland.dispatch("workspace", name);
                });
            workspaces.push_back({ id, button });
            append(*button);
        }
        workspaces_update();
    });
    hyprland.sync_workspaces();

    workspaces_update();
    hyprland.connect_property_changed("workspacev2", workspaces_update);
    hyprland.connect_property_changed("activespecial", workspaces_update);

    // Setup scrolling on widget
    scroll = Gtk::EventControllerScroll::create();
    scroll->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    scroll->signal_scroll().connect([this, &hyprland] (double dx, double dy) -> bool {
        auto cur = hyprland.workspacev2.get_value().first;
        if (dy > 0.0) {
            if (cur < 10)
                hyprland.dispatch("workspace", "+1");
        } else if (dy < 0.0) {
            if (cur > 1)
                hyprland.dispatch("workspace", "-1");
        }
        return true;
    }, true);
    add_controller(scroll);
}
