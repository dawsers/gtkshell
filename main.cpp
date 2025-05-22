#include <gtk/gtk.h>
#include <gtkmm.h>

#include <gtkmm/box.h>
#include <gtkmm/centerbox.h>
#include <gtkmm/label.h>
#include <gtkmm/window.h>

#include "main.h"
#include "utils.h"
#include "shellwindow.h"
#include "network.h"
#include "widgets.h"
#include "hyprland.h"
#include "clock.h"
#include "graph.h"
#include "wireplumber.h"
#include "weather.h"
#include "scroll.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;


static Glib::ustring CONFIG_FILE;
static Glib::ustring STYLE_FILE;

static std::vector<std::string> ARGS;

static Glib::RefPtr<GtkShell> app;

static void reload_stylesheet(const std::string &style_file)
{
    try {
        std::string css_style = Glib::file_get_contents(style_file);
        auto display = Gdk::Display::get_default();
        auto provider = Gtk::CssProvider::create();
        provider->load_from_string(css_style);
        Gtk::StyleContext::add_provider_for_display(display, provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    } catch (Glib::FileError &error) {
        Utils::log(Utils::LogSeverity::WARNING, std::format("Could not load style file {}, error: {}", style_file.c_str(), error.what()));
    }
}

class Bar {
public:
    Bar(const json &json) : window(nullptr), box(nullptr), left(nullptr), center(nullptr), right(nullptr) {
        if (json.contains("monitor")) {
            monitor = json["monitor"];
        } else {
            monitor = Utils::get_default_monitor_id();
        }

        GtkShellAnchor anchor = GtkShellAnchor::ANCHOR_TOP;
        if (json.contains("anchor")) {
            const std::string anchor_str = json["anchor"];
            if (anchor_str == "bottom")
                anchor = GtkShellAnchor::ANCHOR_BOTTOM;
            else if (anchor_str == "left")
                anchor = GtkShellAnchor::ANCHOR_LEFT;
            else if (anchor_str == "right")
                anchor = GtkShellAnchor::ANCHOR_RIGHT;
        }

        GtkShellStretch stretch;
        if (json.contains("stretch")) {
            const std::string stretch_str = json["stretch"];
            if (stretch_str == "none") {
                stretch = GtkShellStretch::STRETCH_NONE;
            } else if (stretch_str == "horizontal") {
                stretch = GtkShellStretch::STRETCH_HORIZONTAL;
            } else if (stretch_str == "vertical") {
                stretch = GtkShellStretch::STRETCH_VERTICAL;
            }
        } else {
            switch (anchor) {
            case GtkShellAnchor::ANCHOR_TOP:
            case GtkShellAnchor::ANCHOR_BOTTOM:
                stretch = GtkShellStretch::STRETCH_HORIZONTAL;
                break;
            case GtkShellAnchor::ANCHOR_LEFT:
            case GtkShellAnchor::ANCHOR_RIGHT:
                stretch = GtkShellStretch::STRETCH_VERTICAL;
                break;
            case GtkShellAnchor::ANCHOR_NONE:
            default:
                stretch = GtkShellStretch::STRETCH_NONE;
                break;
            }
        }
        bool exclusive = true;
        if (json.contains("exclusive") && json["exclusive"] == false)
            exclusive = false;

        window = new GtkShellWindow(("gtkshell-bar-" + monitor).c_str(), anchor, monitor.c_str(), exclusive, stretch);

        window->add_css_class("bar");
        add_widgets(json);
        window->set_child(*box);

        // Add reload menu
        auto model = Gio::Menu::create();
        action_group = Gio::SimpleActionGroup::create();
        action_group->add_action("style", []() {
            reload_stylesheet(STYLE_FILE);
        });
        action_group->add_action("restart", []() {
            Utils::spawn(ARGS, Utils::CWD);
            app->quit();
        });
        model->append("Reload style sheet", "menu.style");
        model->append("Restart", "menu.restart");
        menu.set_menu_model(model);
        box->insert_action_group("menu", action_group);
        menu.set_parent(*box);

        click = Gtk::GestureClick::create();
        click->set_button(3); // 0 = all, 1 = left, 2 = center, 3 = right
        click->signal_pressed().connect([this] (int n_press, double x, double y) {
            const Gdk::Rectangle rect(x, y, 1, 1);
            menu.set_pointing_to(rect);
            menu.popup();
        }, true);
        box->add_controller(click);
    }
    ~Bar() {
        delete window;
    }

    GtkShellWindow *get_window() {
        return window;
    }

private:
    Gtk::Widget *create_widget(const std::string &name) {
        if (name == "workspaces") {
            return Gtk::make_managed<Workspaces>();
        } else if (name == "scroller") {
            return Gtk::make_managed<Scroller>();
        } else if (name == "submap") {
            return Gtk::make_managed<Submap>();
        } else if (name == "client-title") {
            return Gtk::make_managed<ClientTitle>();
        } else if (name == "mpris") {
            return Gtk::make_managed<MediaPlayerBox>();
        } else if (name == "clock") {
            return Gtk::make_managed<Clock>();
        } else if (name == "package-updates") {
            return Gtk::make_managed<PackageUpdates>();
        } else if (name == "idle-inhibitor") {
            return Gtk::make_managed<IdleInhibitor>();
        } else if (name == "keyboard-layout") {
            return Gtk::make_managed<KeyboardLayout>();
        } else if (name == "weather") {
            return Gtk::make_managed<Weather>(1200);
        } else if (name == "screenshot") {
            return Gtk::make_managed<ScreenShot>();
        } else if (name == "cpu-graph") {
            return Gtk::make_managed<CpuGraph>(2, 8, Color(0.0, 0.57, 0.9));
        } else if (name == "mem-graph") {
            return Gtk::make_managed<MemGraph>(5, 8, Color(0.0, 0.7, 0.36));
        } else if (name == "gpu-graph") {
            return Gtk::make_managed<GpuGraph>(5, 8, Color(0.94, 0.78, 0.44), Color(0.65, 0.26, 0.26));
        } else if (name == "network") {
            return Gtk::make_managed<NetworkIndicator>();
        } else if (name == "speaker") {
            return Gtk::make_managed<SpeakerIndicator>();
        } else if (name == "microphone") {
            return Gtk::make_managed<MicrophoneIndicator>();
        } else if (name == "notifications") {
            return Gtk::make_managed<Notifications>(monitor);
        } else if (name == "systray") {
            return Gtk::make_managed<SysTray>();
        } else if (name == "scroll-submap") {
            return Gtk::make_managed<ScrollSubmap>();
        } else if (name == "scroll-workspaces") {
            return Gtk::make_managed<ScrollWorkspaces>(monitor);
        } else if (name == "scroll-client-title") {
            return Gtk::make_managed<ScrollClientTitle>();
        } else if (name == "scroll-keyboard-layout") {
            return Gtk::make_managed<ScrollKeyboardLayout>();
        } else if (name == "scroll-trails") {
            return Gtk::make_managed<ScrollTrails>();
        } else if (name == "scroll-scroller") {
            return Gtk::make_managed<ScrollScroller>();
        }
        return nullptr;
    }
    void add_box_widgets(const json &json, Gtk::Box *box) {
        box->set_hexpand(true);
        box->set_halign(Gtk::Align::FILL);

        if (json.contains("left")) {
            auto left = Gtk::make_managed<Gtk::Box>();
            for (auto module : json["left"]) {
                std::string name = module;
                auto widget = create_widget(name);
                if (widget != nullptr)
                    left->append(*widget);
            }
            left->set_halign(Gtk::Align::START);
            left->set_hexpand(true);
            box->append(*left);
        }
        if (json.contains("right")) {
            auto right = Gtk::make_managed<Gtk::Box>();
            for (auto module : json["right"]) {
                std::string name = module;
                auto widget = create_widget(name);
                if (widget != nullptr)
                    right->append(*widget);
            }
            right->set_halign(Gtk::Align::END);
            right->set_hexpand(true);
            box->append(*right);
        }
    }

    void add_widgets(const json &json) {
        box = Gtk::make_managed<Gtk::CenterBox>();

        if (json.contains("left-box")) {
            auto left_json = json["left-box"];
            if (left_json.size() > 0) {
                left = Gtk::make_managed<Gtk::Box>();
                left->set_halign(Gtk::Align::START);
                left->set_hexpand(true);
                add_box_widgets(left_json, left);
                box->set_start_widget(*left);
            }
        }
        if (json.contains("center-box")) {
            auto center_json = json["center-box"];
            if (center_json.size() > 0) {
                center = Gtk::make_managed<Gtk::Box>();
                center->set_halign(Gtk::Align::CENTER);
                center->set_hexpand(true);
                add_box_widgets(center_json, center);
                center->set_hexpand(false);
                center->set_halign(Gtk::Align::CENTER);
                box->set_center_widget(*center);
            }
        }
        if (json.contains("right-box")) {
            auto right_json = json["right-box"];
            if (right_json.size() > 0) {
                right = Gtk::make_managed<Gtk::Box>();
                right->set_halign(Gtk::Align::END);
                right->set_hexpand(true);
                add_box_widgets(right_json, right);
                box->set_end_widget(*right);
            }
        }
    }

    std::string monitor;
    GtkShellWindow *window;
    Gtk::CenterBox *box;
    Gtk::Box *left, *center, *right;
    Glib::RefPtr<Gtk::GestureClick> click;
    Gtk::PopoverMenu menu;
    Glib::RefPtr<Gio::SimpleActionGroup> action_group;
};

GtkShell::~GtkShell()
{
    for (auto bar : bars)
        delete bar;
}
     
Glib::RefPtr<GtkShell> GtkShell::create(const Glib::ustring &app_id)
{
    return Glib::make_refptr_for_instance<GtkShell>(new GtkShell(app_id));
}

void GtkShell::on_activate()
{
    // Parse json config and create bars
    auto config = Utils::read_file(CONFIG_FILE);
    json json;
    try {
        json = json::parse(config, /* callback */ nullptr, /* allow exceptions */ true, /* ignore comments */ true);
    } catch (const json::parse_error& e) {
        Utils::log(Utils::LogSeverity::ERROR, std::format("message: {}\nexception id: {}\nbyte position of error: {}", e.what(), e.id, e.byte));
    }
    for (auto json_bar : json) {
        auto bar = new Bar(json_bar);
        GtkShellWindow *win = bar->get_window();
        add_window(*win);
        bars.push_back(bar);
        win->present();
    }
}

GtkShell::GtkShell(const Glib::ustring &app_id) : Gtk::Application(app_id, Gio::Application::Flags::DEFAULT_FLAGS)
{
}

// -i instance (default = 1)
// -c config (default = ~/.config/gtkshell/config.js)
// -s style.css (default = ~/.config/gtkshell/style.css)
int main (int argc, char **argv)
{
    std::string instance = "1";
    CONFIG_FILE = Utils::XDG_CONFIG_HOME + "/gtkshell/config.json";
    STYLE_FILE = Utils::XDG_CONFIG_HOME + "/gtkshell/style.css";
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-i") == 0) {
            instance = argv[++i];
        } else if (strcmp(argv[i], "-c") == 0) {
            CONFIG_FILE = Utils::expand_directory(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0) {
            STYLE_FILE = Utils::expand_directory(argv[++i]);
        }
    }
    // Keep a copy of all arguments for possible restart
    for (int i = 0; i < argc; ++i) {
        ARGS.push_back(argv[i]);
    }

    Utils::GTKSHELL_INSTANCE = instance;
    const Glib::ustring app_id = "com.github.dawsers.gtkshell.id" + Utils::GTKSHELL_INSTANCE;

    app = GtkShell::create(app_id);

    // Apply CSS
    reload_stylesheet(STYLE_FILE);

    return app->run();
}
