#include <giomm.h>
#include <glibmm.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/frame.h>
#include <gtkmm/separator.h>

#include <format>

#include "scroll.h"
#include "utils.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

ScrollIpc::ScrollIpc() : dispatcher(), sem(0), working(false) {
    const std::string &path = get_socket_path();
    fd = open(path);
    fd_event = open(path);

    worker_thread = std::make_shared<std::thread>(&ScrollIpc::working_thread, this);
    worker_thread->detach();
    dispatcher.connect(
        [this]() {
            mtx.lock();
            auto result = shared_data;
            mtx.unlock();
            signal_event.emit(result);
            sem.release();
        });
}

void ScrollIpc::working_thread() {
    while (true) {
        sem.acquire();
        working = true;
        const auto res = ScrollIpc::recv(fd_event);
        mtx.lock();
        shared_data = res;
        mtx.unlock();
        dispatcher.emit();
        working = false;
    }
}

ScrollIpc::~ScrollIpc() {
    if (fd > 0) {
        // To fail the IPC header
        if (write(fd, "close-sway-ipc", 14) == -1) {
            Utils::log(Utils::LogSeverity::ERROR, "Scroll: Failed to close IPC");
        }
        close(fd);
        fd = -1;
    }
    if (fd_event > 0) {
        if (write(fd_event, "close-sway-ipc", 14) == -1) {
            Utils::log(Utils::LogSeverity::ERROR, "Scroll: Failed to close IPC event handler");
        }
        close(fd_event);
        fd_event = -1;
    }
}

const std::string ScrollIpc::get_socket_path() const {
    const char *env = getenv("SCROLLSOCK");
    if (env != nullptr) {
        return std::string(env);
    }
    throw std::runtime_error("Scroll: SCROLLSOCK variable is empty");
    return "";
}

int ScrollIpc::open(const std::string &socketPath) const {
    int32_t fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        throw std::runtime_error("Scroll: Unable to open Unix socket");
    }
    (void)fcntl(fd, F_SETFD, FD_CLOEXEC);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = 0;
    int l = sizeof(struct sockaddr_un);
    if (::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), l) == -1) {
        throw std::runtime_error("Scroll: Unable to connect");
    }
    return fd;
}

ScrollIpcResponse ScrollIpc::recv(int fd) {
    std::string header;
    header.resize(ipc_header_size);
    auto data32 = reinterpret_cast<uint32_t *>(header.data() + ipc_magic.size());
    size_t total = 0;

    while (total < ipc_header_size) {
        auto res = ::recv(fd, header.data() + total, ipc_header_size - total, 0);
        if (fd_event == -1 || fd == -1) {
            // IPC is closed so just return an empty response
            return {0, 0, ""};
        }
        if (res <= 0) {
            throw std::runtime_error("Scroll: Unable to receive IPC header");
        }
        total += res;
    }
    auto magic = std::string(header.data(), header.data() + ipc_magic.size());
    if (magic != ipc_magic) {
        throw std::runtime_error("Scroll: Invalid IPC magic");
    }

    total = 0;
    std::string payload;
    payload.resize(data32[0]);
    while (total < data32[0]) {
        auto res = ::recv(fd, payload.data() + total, data32[0] - total, 0);
        if (res < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            throw std::runtime_error("Scroll: Unable to receive IPC payload");
        }
        total += res;
    }
    return { data32[0], data32[1], payload };
}

ScrollIpcResponse ScrollIpc::send(int fd, uint32_t type, const std::string &payload) {
    std::string header;
    header.resize(ipc_header_size);
    auto data32 = reinterpret_cast<uint32_t *>(header.data() + ipc_magic.size());
    memcpy(header.data(), ipc_magic.c_str(), ipc_magic.size());
    data32[0] = payload.size();
    data32[1] = type;

    if (::send(fd, header.data(), ipc_header_size, 0) == -1) {
        throw std::runtime_error("Scroll: Unable to send IPC header");
    }
    if (::send(fd, payload.c_str(), payload.size(), 0) == -1) {
        throw std::runtime_error("Scroll: Unable to send IPC payload");
    }
    return ScrollIpc::recv(fd);
}

void ScrollIpc::send_cmd(uint32_t type, const std::string &payload) {
    const auto res = ScrollIpc::send(fd, type, payload);
    signal_cmd.emit(res);
}

void ScrollIpc::subscribe(const std::string &payload) {
    auto res = ScrollIpc::send(fd_event, IPC_SUBSCRIBE, payload);
    if (res.payload != "{\"success\": true}") {
        throw std::runtime_error("Scroll: Unable to subscribe ipc event");
    }
    sem.release();
}


// ScrollSubmap
ScrollSubmap::ScrollSubmap()
{
    add_css_class("submap");
    ipc.signal_event.connect(sigc::mem_fun(*this, &ScrollSubmap::on_event));
    ipc.subscribe(R"(["mode"])");
}

void ScrollSubmap::on_event(const ScrollIpcResponse &res) {
    json json_mode = json::parse(res.payload);
    std::string mode = json_mode["change"];
    this->set_text(mode != "default" ? "󰌌    " + mode : "");
}

// ScrollWorkspaces
ScrollWorkspaces::ScrollWorkspaces(const std::string &output) : output(output) {
    add_css_class("workspaces");

    ipc.signal_event.connect(sigc::mem_fun(*this, &ScrollWorkspaces::on_event));
    ipc.signal_cmd.connect(sigc::mem_fun(*this, &ScrollWorkspaces::on_cmd));
    ipc.subscribe(R"(["workspace"])");
    ipc.send_cmd(IPC_GET_WORKSPACES);
}

void ScrollWorkspaces::workspaces_update() {
    const std::vector<Glib::ustring> normal = { "" };
    const std::vector<Glib::ustring> focused = { "focused" };
    for (auto workspace : workspaces) {
        workspace.second->set_css_classes(workspace.first.id == this->focused ? focused : normal);
        if (workspace.first.urgent)
            workspace.second->add_css_class("urgent");
    }
}

void ScrollWorkspaces::on_event(const ScrollIpcResponse &data) {
    json json_mode = json::parse(data.payload);
    std::string change = json_mode["change"];

    if (change == "init") {
        ipc.send_cmd(IPC_GET_WORKSPACES);
    } else if (change == "empty") {
        ipc.send_cmd(IPC_GET_WORKSPACES);
    } else if (change == "focus") {
        json json_current = json_mode["current"];
        std::string output = json_current["output"].get<std::string>();
        if (output == this->output) {
            int num = json_current["num"].get<int>();
            this->focused = num;
        }
        workspaces_update();
    } else if (change == "move") {
        ipc.send_cmd(IPC_GET_WORKSPACES);
    } else if (change == "rename") {
        ipc.send_cmd(IPC_GET_WORKSPACES);
    } else if (change == "urgent") {
        json json_current = json_mode["current"];
        std::string output = json_current["output"].get<std::string>();
        if (output == this->output) {
            int id = json_current["num"].get<int>();
            bool urgent = json_current["urgent"].get<bool>();
            for (auto &workspace : workspaces) {
                if (workspace.first.id == id) {
                    workspace.first.urgent = urgent;
                    break;
                }
            }
            workspaces_update();
        }
    } else if (change == "reload") {
        ipc.send_cmd(IPC_GET_WORKSPACES);
    }
}

void ScrollWorkspaces::on_cmd(const ScrollIpcResponse &data) {
    if (data.type == IPC_GET_WORKSPACES) {
        for (auto workspace : workspaces) {
            remove(*workspace.second);
            delete workspace.second;
        }
        workspaces.clear();
        json json_workspaces = json::parse(data.payload);
        for (auto workspace : json_workspaces) {
            if (workspace["output"].get<std::string>() == output) {
                Gtk::Button *button = new Gtk::Button();
                int id = workspace["num"].get<int>();
                bool focused = workspace["focused"].get<bool>();
                if (focused) {
                    this->focused = id;
                }
                bool urgent = workspace["urgent"].get<bool>();
                const std::string name = workspace["name"].get<std::string>();
                button->set_label(name);
                button->signal_clicked().connect(
                    [id, name, this]() {
                        ipc.send_cmd(IPC_COMMAND, std::format("workspace {}", name));
                    });
                workspaces.push_back({ { id, urgent }, button });
                append(*button);
            }
        }
        workspaces_update();
    }
}

// Scroll ClientTitle
ScrollClientTitle::ScrollClientTitle() {
    add_css_class("client");
    set_max_width_chars(80);
    set_ellipsize(Pango::EllipsizeMode::END);
    ipc.signal_event.connect(sigc::mem_fun(*this, &ScrollClientTitle::on_event));
    ipc.subscribe(R"(["window"])");
}

Glib::ustring ScrollClientTitle::generate_title(const json &container) {
    json marks = container["marks"];
    json trailmark = container["trailmark"];
    const std::string name = container["name"].is_null() ? "" : container["name"].get<std::string>();
    const std::string mark = marks.is_null() || marks.empty() ? "" : "🔖";
    const std::string trail = trailmark.get<bool>() ? "✅" : "";
    return std::format("{} {}{}", name, mark, trail);
}
void ScrollClientTitle::on_event(const ScrollIpcResponse &data) {
    json json_window = json::parse(data.payload);
    std::string change = json_window["change"];
    json container = json_window["container"];
    if (change == "focus") {
        this->focused = container["id"].get<int>();
        this->set_text(generate_title(container));
    } else if (change == "title") {
        if (json_window["container"]["id"].get<int>() == this->focused) {
            this->set_text(generate_title(container));
        }
    } else if (change == "close") {
        if (json_window["container"]["id"].get<int>() == this->focused) {
            this->set_text("");
        }
    } else if (change == "mark") {
        if (json_window["container"]["id"].get<int>() == this->focused) {
            this->set_text(generate_title(container));
        }
    } else if (change == "trailmark") {
        if (json_window["container"]["id"].get<int>() == this->focused) {
            this->set_text(generate_title(container));
        }
    }
}

// Scroll KeyboardLayout
ScrollKeyboardLayout::ScrollKeyboardLayout() {
    add_css_class("keyboard");
    ipc.signal_event.connect(sigc::mem_fun(*this, &ScrollKeyboardLayout::on_event));
    ipc.signal_cmd.connect(sigc::mem_fun(*this, &ScrollKeyboardLayout::on_cmd));
    ipc.subscribe(R"(["input"])");
    // Get current keyboard
    ipc.send_cmd(IPC_GET_INPUTS);
}

void ScrollKeyboardLayout::on_event(const ScrollIpcResponse &data) {
    json json_input = json::parse(data.payload);
    std::string change = json_input["change"];
    if (change == "xkb_layout") {
        this->set_text(json_input["input"]["xkb_active_layout_name"].get<std::string>());
    }
}

void ScrollKeyboardLayout::on_cmd(const ScrollIpcResponse &data) {
    if (data.type == IPC_GET_INPUTS) {
        json json_inputs = json::parse(data.payload);
        for (auto input : json_inputs) {
            if (input["type"].get<std::string>() == "keyboard") {
                this->set_text(input["xkb_active_layout_name"].get<std::string>());
            }
        }
    }
}

// Scroll Trails
ScrollTrails::ScrollTrails() {
    add_css_class("trails");
    ipc.signal_event.connect(sigc::mem_fun(*this, &ScrollTrails::on_event));
    ipc.signal_cmd.connect(sigc::mem_fun(*this, &ScrollTrails::on_cmd));
    ipc.subscribe(R"(["trails"])");
    ipc.send_cmd(IPC_GET_TRAILS);
}

void ScrollTrails::on_event(const ScrollIpcResponse &data) {
    json json_input = json::parse(data.payload);
    int active = json_input["trails"]["active"].get<int>();
    int length = json_input["trails"]["length"].get<int>();
    int nmarks = json_input["trails"]["trail_length"].get<int>();
    const std::string trails = std::format("{}/{} ({})", active, length, nmarks);
    this->set_text(trails);
}

void ScrollTrails::on_cmd(const ScrollIpcResponse &data) {
    if (data.type == IPC_GET_TRAILS) {
        json json_input = json::parse(data.payload);
        int active = json_input["trails"]["active"].get<int>();
        int length = json_input["trails"]["length"].get<int>();
        int nmarks = json_input["trails"]["trail_length"].get<int>();
        const std::string trails = std::format("{}/{} ({})", active, length, nmarks);
        this->set_text(trails);
    }
}

ScrollScroller::ScrollScroller() : auto_entry(Gtk::Adjustment::create(2.0, 1.0, 20.0, 1.0, 5.0, 0.0)) {
    auto r_hor = Gtk::make_managed<Gtk::CheckButton>("horizontal");
    r_hor->signal_toggled().connect(
        [this, r_hor]() {
            if (r_hor->get_active())
                ipc.send_cmd(IPC_COMMAND, "set_mode h");
        });
    auto r_ver = Gtk::make_managed<Gtk::CheckButton>("vertical");
    r_ver->signal_toggled().connect(
        [this, r_ver]() {
            if (r_ver->get_active())
                ipc.send_cmd(IPC_COMMAND, "set_mode v");
        });
    r_ver->set_group(*r_hor);
    auto r_frame = Gtk::make_managed<Gtk::Frame>("Mode");
    auto r_frame_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    r_frame_box->append(*r_hor);
    r_frame_box->append(*r_ver);
    r_frame->set_child(*r_frame_box);
    auto r_separator = Gtk::make_managed<Gtk::Separator>();

    auto p_after = Gtk::make_managed<Gtk::CheckButton>("after");
    p_after->signal_toggled().connect([this, p_after] () { if (p_after->get_active()) ipc.send_cmd(IPC_COMMAND, "set_mode after"); });
    auto p_before = Gtk::make_managed<Gtk::CheckButton>("before");
    p_before->signal_toggled().connect([this, p_before] () { if (p_before->get_active()) ipc.send_cmd(IPC_COMMAND, "set_mode before"); });
    auto p_beginning = Gtk::make_managed<Gtk::CheckButton>("beginning");
    p_beginning->signal_toggled().connect([this, p_beginning] () { if (p_beginning->get_active()) ipc.send_cmd(IPC_COMMAND, "set_mode beginning"); });
    auto p_end = Gtk::make_managed<Gtk::CheckButton>("end");
    p_end->signal_toggled().connect([this, p_end] () { if (p_end->get_active()) ipc.send_cmd(IPC_COMMAND, "set_mode end"); });
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
    f_focus->signal_toggled().connect([this, f_focus]() { if (f_focus->get_active()) ipc.send_cmd(IPC_COMMAND, "set_mode focus"); });
    auto f_nofocus = Gtk::make_managed<Gtk::CheckButton>("nofocus");
    f_nofocus->set_group(*f_focus);
    f_nofocus->signal_toggled().connect([this, f_nofocus]() { if (f_nofocus->get_active()) ipc.send_cmd(IPC_COMMAND, "set_mode nofocus"); });
    f_frame_box->append(*f_focus);
    f_frame_box->append(*f_nofocus);
    f_frame->set_child(*f_frame_box);
    auto f_separator = Gtk::make_managed<Gtk::Separator>();

    auto a_frame = Gtk::make_managed<Gtk::Frame>("Reorder");
    auto a_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    auto a_manual = Gtk::make_managed<Gtk::CheckButton>("noauto"); 
    a_manual->signal_toggled().connect([this, a_manual] () { if (a_manual->get_active()) ipc.send_cmd(IPC_COMMAND, "set_mode noreorder_auto"); });
    auto a_auto = Gtk::make_managed<Gtk::CheckButton>("auto");
    a_auto->set_group(*a_manual);
    a_auto->signal_toggled().connect([this, a_auto] () { if (a_auto->get_active()) ipc.send_cmd(IPC_COMMAND, "set_mode reorder_auto"); });
    a_box->append(*a_manual);
    a_box->append(*a_auto);
    a_frame->set_child(*a_box);
    auto a_separator = Gtk::make_managed<Gtk::Separator>();

    auto c_frame = Gtk::make_managed<Gtk::Frame>("Center");
    auto c_frame_box = Gtk::make_managed<Gtk::Box>();
    auto c_col = Gtk::make_managed<Gtk::CheckButton>("horizontal");
    c_col->signal_toggled().connect([this, c_col]() {
        if (c_col->get_active())
            ipc.send_cmd(IPC_COMMAND, "set_mode center_horiz");
        else
            ipc.send_cmd(IPC_COMMAND, "set_mode nocenter_horiz");
    });
    auto c_win = Gtk::make_managed<Gtk::CheckButton>("vertical");
    c_win->signal_toggled().connect([this, c_win]() {
        if (c_win->get_active())
            ipc.send_cmd(IPC_COMMAND, "set_mode center_vert");
        else
            ipc.send_cmd(IPC_COMMAND, "set_mode nocenter_vert");
    });
    c_frame_box->append(*c_col);
    c_frame_box->append(*c_win);
    c_frame->set_child(*c_frame_box);

    mode_label.add_css_class("scroller-mode");

    auto update_data = [this, r_hor, r_ver, p_after, p_before, p_beginning, p_end, f_focus, f_nofocus, a_manual, a_auto] (const ScrollIpcResponse &data) {
        if (data.type == IPC_GET_SCROLLER || data.type == IPC_EVENT_SCROLLER) {
            json json_scroller = json::parse(data.payload);
            json scroller = json_scroller["scroller"];
            if (scroller.is_null()) {
                // Workspace doesn't exist yet
                return;
            }
            overview_label.set_text(scroller["overview"].get<bool>() ? "🐦" : "");
            if (scroller["scaled"].get<bool>()) {
                scale_label.set_text(std::format("{:4.2f}", scroller["scale"].get<double>()));
            } else {
                scale_label.set_text("");
            }
            std::string mode;
            if (scroller["mode"].get<std::string>() == "horizontal") {
                mode = "-";
                r_hor->set_active(true);
            } else {
                mode = "|";
                r_ver->set_active(true);
            }
            std::string pos;
            // position
            std::string insert = scroller["insert"].get<std::string>();
            if (insert == "after") {
                pos = "→";
                p_after->set_active(true);
            } else if (insert == "before") {
                pos = "←";
                p_before->set_active(true);
            } else if (insert == "end") {
                pos = "⇥";
                p_end->set_active(true);
            } else if (insert == "beginning") {
                pos = "⇤";
                p_beginning->set_active(true);
            }
            // focus
            std::string focus;
            if (scroller["focus"].get<bool>()) {
                focus = "";
                f_focus->set_active(true);
            } else {
                focus = "";
                f_nofocus->set_active(true);
            }
            // center column/window
            std::string center_column;
            if (scroller["center_horizontal"].get<bool>()) {
                center_column = "";
            } else {
                center_column = " ";
            }
            std::string center_window;
            if (scroller["center_vertical"].get<bool>()) {
                center_window = "󰉠";
            } else {
                center_window = " ";
            }
            // auto
            std::string reorder = scroller["reorder"].get<std::string>();
            std::string auto_mode;
            if (reorder == "auto") {
                auto_mode = "🅰";
                a_auto->set_active(true);
            } else {
                auto_mode = "✋";
                a_manual->set_active(true);
            }
            mode_label.set_text(std::format("{} {} {} {} {} {}  ", mode, pos, focus, center_column, center_window, auto_mode));
        }
    };

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
    append(scale_label);

    ipc.signal_cmd.connect(update_data);
    ipc.signal_event.connect(update_data);
    ipc.subscribe(R"(["scroller"])");
    ipc.send_cmd(IPC_GET_SCROLLER);

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
