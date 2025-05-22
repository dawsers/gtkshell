#include "widgets.h"
#include "notifications.h"
#include "shellwindow.h"
#include "systemtray.h"

#include <glibmm.h>
#include <gtkmm/label.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/image.h>

#include "utils.h"
#include "bind.h"

PackageUpdates::PackageUpdates() : dispatcher(), sem(0), working(false)
{
    update_thread = std::make_shared<std::thread>(&PackageUpdates::update, this);
    update_thread->detach();

    timer = Glib::signal_timeout().connect_seconds([this] () -> bool { signal_update(); return true; }, 300);

    dispatcher.connect(
        [this]() {
            mtx.lock();
            auto updates = shared_data.updates;
            auto nupdates = shared_data.nupdates;
            mtx.unlock();
            updates_label.set_text(std::format("󰻌   {}", nupdates));
            const int threshold_yellow = 25;
            if (nupdates == 0) {
                updates_label.set_css_classes({"updates-green"});
            } else if (nupdates <= threshold_yellow) {
                updates_label.set_css_classes({"updates-yellow"});
            } else {
                updates_label.set_css_classes({"updates-red"});
            }
            set_tooltip_text(updates);
        });
    signal_update();
    add_css_class("updates");
    set_child(updates_label);
    // Left button click
    signal_clicked().connect([] () {
        Utils::spawn("pamac-manager --updates");
    });
#if 0
    // Right button click
    click = Gtk::GestureClick::create();
    click->set_button(3); // 0 = all, 1 = left, 2 = center, 3 = right
    click->signal_pressed().connect([this] (int n_press, double x, double y) {
        update();
    }, true);
    add_controller(click);
#endif
    // Hover
    hover = Gtk::EventControllerMotion::create();
    hover->signal_enter().connect([this](double x, double y) {
        signal_update();
    }, true);
    add_controller(hover);
}

PackageUpdates::~PackageUpdates()
{
    timer.disconnect();
}

void PackageUpdates::signal_update()
{
    if (!working)
        sem.release();
}

void PackageUpdates::update()
{
    while (true) {
        sem.acquire();
        working = true;
        auto arch = Utils::exec("checkupdates");
        auto aur = Utils::exec("paru -Qua");
        auto updates = Utils::trim_end(arch);
        if (aur != "") {
            updates += updates == "" ? aur : "\n" + aur;
            updates = Utils::trim_end(updates);
        }
        // auto nupdates = updates == "" ? 0 : Utils::split(updates, "\r\n|\r|\n").size();
        auto nupdates = updates == "" ? 0 : Utils::split(updates, "\n").size();
        mtx.lock();
        shared_data.updates = updates;
        shared_data.nupdates = nupdates;
        mtx.unlock();
        dispatcher.emit();
        working = false;
    }
}

class MPlayer : public Gtk::Box {
public:
    MPlayer(Player *player) : player(player) {
        play_button.set_child(play_label);
        play_button.signal_clicked().connect([this]() {
            this->player->play_pause();
            update_player();
        });
        prev_label.set_text("");
        prev_button.set_child(prev_label);
        prev_button.signal_clicked().connect([this]() {
            this->player->previous();
            update_player();
        });
        next_label.set_text("");
        next_button.set_child(next_label);
        next_button.signal_clicked().connect([this]() {
            this->player->next();
            update_player();
        });
        info_button.set_child(info_label);
        info_button.signal_clicked().connect(
            [this]() { Utils::spawn("kitty ncmpcpp"); });
        append(info_button);
        append(prev_button);
        append(play_button);
        append(next_button);
        update_player();
    }
    virtual ~MPlayer() {}

    Glib::ustring get_info() const {
        return player->name + ": " + play_label.get_text() + " " + info_label.get_text();
    }

    void update_player() {
        Glib::ustring status_icon;
        Glib::ustring class_name;
        switch (player->playback_status) {
        case PlaybackStatus::PAUSED:
            class_name = "mpris-paused";
            status_icon = "";
            break;
        case PlaybackStatus::PLAYING:
            class_name = "mpris-playing";
            status_icon = "";
            break;
        case PlaybackStatus::STOPPED:
        default:
            class_name = "mpris-stopped";
            status_icon = "";
            // "", "", "", "", ""
        }
        if (!player->can_go_previous) {
            prev_label.set_visible(false);
            prev_button.set_visible(false);
        }
        if (!player->can_go_next) {
            next_label.set_visible(false);
            next_button.set_visible(false);
        }
        play_label.set_text(status_icon);
        info_label.set_text(std::format("{} - {} - {}", player->title.c_str(), player->album.c_str(), player->artist.c_str()));
        set_css_classes({ class_name });
    }
    Player *player;

private:
    Gtk::Label play_label;
    Gtk::Button play_button;
    Gtk::Label prev_label;
    Gtk::Button prev_button;
    Gtk::Label next_label;
    Gtk::Button next_button;
    Gtk::Label info_label;
    Gtk::Button info_button;
};

static Glib::ustring get_player_info(const Player *player)
{
    Glib::ustring status;
    switch (player->playback_status) {
    case PlaybackStatus::PAUSED:
        status = "";
        break;
    case PlaybackStatus::PLAYING:
        status = "";
        break;
    case PlaybackStatus::STOPPED:
    default:
        status = "";
        // "", "", "", "", ""
    }
    return std::format("{}: {} {} - {} - {}", player->name.c_str(), status.c_str(), player->title.c_str(), player->album.c_str(), player->artist.c_str());
}

MediaPlayerBox::MediaPlayerBox()
{
    Mpris &mpris = Mpris::get_instance();
    for (auto iter : mpris.players) {
        add_player(iter.second);
    }
    update_menu_items();
    mpris.player_changed_signal.connect(
        [this, &mpris](const Glib::ustring &bus) {
            auto iter = mplayers.find(bus);
            if (iter != mplayers.end()) {
                iter->second->update_player();
                if (iter->second != visible) {
                    visible->set_visible(false);
                    visible = iter->second;
                    visible->set_visible(true);
                }
            }
        });
    mpris.player_added_signal.connect(
        [this](Player *player) {
            add_player(player);
            update_menu_items();
        });
    mpris.player_closed_signal.connect(
        [this](Player *player) {
            remove_player(player);
            update_menu_items();
        });
    click = Gtk::GestureClick::create();
    //click->set_button(0); // All buttons
    click->set_button(GDK_BUTTON_SECONDARY);
    click->signal_pressed().connect([this] (int n_press, double x, double y) {
//        auto mbutton = this->click->get_current_button();
//        if (mbutton == GDK_BUTTON_SECONDARY) {
            const Gdk::Rectangle rect(x, y, 1, 1);
            menu_pop->set_pointing_to(rect);
            menu_pop->popup();
//        } else if (mbutton == GDK_BUTTON_PRIMARY) {
//            Utils::spawn("kitty ncmpcpp");
//        }
    }, true);
    add_controller(click);
}

MediaPlayerBox::~MediaPlayerBox()
{
    for (auto player : mplayers) {
        delete player.second;
    }
}

void MediaPlayerBox::remove_player(Player *player)
{
    auto iter = mplayers.find(player->bus_name);
    if (iter != mplayers.end()) {
        bool is_visible = false;
        if (iter->second == visible) {
            is_visible = true;
            visible->set_visible(false);
        }
        remove(*iter->second);
        delete iter->second;
        mplayers.erase(iter);
        if (mplayers.size() > 0 && is_visible) {
            visible = mplayers.begin()->second;
            visible->set_visible(true);
        }
    }
}

void MediaPlayerBox::add_player(Player *player)
{
    auto mplayer = new MPlayer(player);
    mplayers[player->bus_name] = mplayer;
    if (visible)
        visible->set_visible(false);
    mplayer->set_visible(true);
    visible = mplayer;
    append(*mplayer);
}

void MediaPlayerBox::update_menu_items()
{
    Mpris &mpris = Mpris::get_instance();
    auto menu = Gio::Menu::create();
    action_group = Gio::SimpleActionGroup::create();
    for (auto player : mpris.players) {
        action_group->add_action(player.first, [player] () {
                                 Mpris::get_instance().player_changed_signal(player.first);
                                 });
        menu->append(player.second->identity, "menu." + player.first);
    }
    insert_action_group("menu", action_group);
    menu_pop = Glib::RefPtr<Gtk::PopoverMenu>(new Gtk::PopoverMenu(menu));
    menu_pop->set_parent(*this);
}

IdleInhibitor::IdleInhibitor()
{
    label.set_text("");
    label.set_css_classes({"disabled"});
    set_child(label);
    set_active(false);
    add_css_class("idle-inhibitor");
    set_tooltip_text("Idle Inhibitor");
    Utils::spawn("matcha --daemon --off");
    signal_toggled().connect([this]() {
        if (get_active()) {
            label.set_text("");
            label.set_css_classes({"enabled"});
        } else {
            label.set_text("");
            label.set_css_classes({"disabled"});
        }
        Utils::spawn("matcha --toggle");
    });
}

ScreenShot::ScreenShot()
{
    add_css_class("screenshot");
    set_label("");
    set_tooltip_text("LEFT_BUTTON: Save to Downloads\nRIGHT_BUTTON: Copy to clipboard");

    click = Gtk::GestureClick::create();
    click->set_button(0); // 0 = all, 1 = left, 2 = center, 3 = right
    click->signal_pressed().connect([this] (int n_press, double x, double y) {
        auto mbutton = this->click->get_current_button();
        if (mbutton == GDK_BUTTON_PRIMARY) {
            // grim  -g "$(slurp -d)" ~/Downloads/screenshot-$(date +"%s.png")
            auto slurp = Utils::trim_end(Utils::exec("slurp -d"));
            auto now = Glib::DateTime::create_now_local().format("%s");
            auto cmd = std::format("grim  -g \"{}\" {}/Downloads/screenshot-{}.png", slurp, Utils::HOME.c_str(), now.c_str());
            Utils::exec(cmd);
        } else if (mbutton == GDK_BUTTON_SECONDARY) {
            // grim -g "$(slurp -d)" - | wl-copy
            auto slurp = Utils::trim_end(Utils::exec("slurp -d"));
            auto cmd = std::format("grim  -g \"{}\" -", slurp);
            Utils::pipe(cmd, "wl-copy");
        }
    }, true);
    add_controller(click);
}

// Event propagation in GTK
// Buttons can cause conflicts with GestureClicks.
// Maybe it is better to avoid buttons altogether and use labels with
// GestureClick instead
// https://www.reddit.com/r/GTK/comments/190vhj6/comment/kgzxf4n/
// https://docs.gtk.org/gtk4/input-handling.html
class NotificationWidget : public Gtk::Box {
public:
    NotificationWidget(Notification *notification, bool keep) {
        if (notification->image_path != "") {
            icon.set(notification->image_path);
        } else if (notification->app_icon != "") {
            icon.set_from_icon_name(notification->app_icon);
        } else if (notification->desktop_entry != "") {
            icon.set_from_icon_name(notification->desktop_entry);
        } else {
            icon.set_from_icon_name("dialog-information-symbolic");
        }
        icon.add_css_class("icon");
        icon.set_valign(Gtk::Align::START);
        icon.set_pixel_size(64);

        auto date = Glib::DateTime::create_now_local(notification->time);
        when.set_text(date.format("%H:%M %A"));
        when.add_css_class("notification-date");
        when.set_xalign(0);
        when.set_justify(Gtk::Justification::LEFT);

        title.add_css_class("notification-title");
        title.set_xalign(0);
        title.set_justify(Gtk::Justification::LEFT);
        title.set_wrap(true);
        title.set_use_markup(true);
        title.set_text(notification->summary);

        body.add_css_class("notification-body");
        body.set_xalign(0);
        body.set_justify(Gtk::Justification::LEFT);
        body.set_wrap(true);
        body.set_use_markup(true);
        body.set_text(notification->body);

        actions.add_css_class("actions");
        for (auto action : notification->actions) {
            auto button = Gtk::make_managed<Gtk::Button>();
            button->add_css_class("action-button");
            button->signal_clicked().connect([notification, action, keep] () {
                // invoke will remove the notification if it is not resident
                notification->invoke(action.id);
                if (keep)
                    notification->dismiss();
                else
                    notification->close();
            });
            button->set_label(action.label);
            actions.append(*button);
        }

        click = Gtk::GestureClick::create();
        click->set_button(GDK_BUTTON_PRIMARY);
        click->signal_pressed().connect(
            [notification, keep](int npress, double x, double y) {
                if (keep)
                    notification->dismiss();
                else
                    notification->close();
            }, true);
        add_css_class(std::format("notification-{}", notification->urgency.to_string()));
        set_orientation(Gtk::Orientation::VERTICAL);
        auto tbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
        tbox->append(when);
        tbox->append(title);
        tbox->append(body);
        auto box = Gtk::make_managed<Gtk::Box>();
        box->append(icon);
        box->append(*tbox);
        box->add_controller(click);
        append(*box);
        append(actions);
    }

private:
    Gtk::Label when;
    Gtk::Image icon;
    Gtk::Label title;
    Gtk::Label body;
    Gtk::Box actions;
    Glib::RefPtr<Gtk::GestureClick> click;
};

class DebugBox : public Gtk::Box {
public:
    DebugBox() : Gtk::Box(Gtk::Orientation::VERTICAL) {}
    virtual ~DebugBox() {
        Utils::log(Utils::LogSeverity::INFO, "Destroying box");
    }
};

class NotificationsWindow : public GtkShellWindow {
public:
    NotificationsWindow(const Glib::ustring &monitor)
        : GtkShellWindow("gtkshell-notifications", GtkShellAnchor::ANCHOR_TOP | GtkShellAnchor::ANCHOR_RIGHT,
                         monitor.c_str(), false, GtkShellStretch::STRETCH_NONE) {
        NotificationsServer &notifications = NotificationsServer::get_instance();
        add_css_class("notifications-window");
        // Using -1 for height crashes!
        set_default_size(350, 1);
        set_visible(false);
    }
    ~NotificationsWindow() {
    }

    void create_widgets() {
        NotificationsServer &notifications = NotificationsServer::get_instance();
        box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
        box->add_css_class("notifications");
        for (auto notification : notifications.notifications) {
            const auto n = notification.second;
            auto nwidget = Gtk::make_managed<NotificationWidget>(notification.second, false);
            box->append(*nwidget);
        }
        // This deletes the old child and its contents
        unset_child();
        if (notifications.notifications.size() > 0) {
            set_child(*box);
        } else {
            delete box;
            set_visible(false);
        }
    }

private:
    Gtk::Box *box;
};

class NotificationsPopup : public GtkShellWindow {
public:
    NotificationsPopup(const Glib::ustring &monitor)
        : GtkShellWindow("gtkshell-notifications-popup", GtkShellAnchor::ANCHOR_TOP | GtkShellAnchor::ANCHOR_RIGHT,
                         monitor.c_str(), false, GtkShellStretch::STRETCH_NONE) {
        NotificationsServer &notifications = NotificationsServer::get_instance();
        bind_property_changed(&notifications, "popup",
            [this]() {
                create_widgets();
            });
        add_css_class("notifications-window-popup");
        // Using -1 for height crashes!
        set_default_size(350, 1);
    }
    ~NotificationsPopup() {
    }

    void create_widgets() {
        NotificationsServer &notifications = NotificationsServer::get_instance();
        box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
        box->add_css_class("notifications-popup");
        size_t visible = 0;
        for (auto notification : notifications.notifications) {
            const auto n = notification.second;
            if (n->popup) {
                auto nwidget = Gtk::make_managed<NotificationWidget>(notification.second, true);
                box->append(*nwidget);
                ++visible;
            }
        }
        // This deletes the old child and its contents
        unset_child();
        if (visible > 0) {
            set_child(*box);
            set_visible(true);
        } else {
            delete box;
            set_visible(false);
        }
    }

private:
    Gtk::Box *box;
};


Notifications::Notifications(const std::string &monitor)
{
    popup = new NotificationsPopup(monitor);
    window = new NotificationsWindow(monitor);
    NotificationsServer &notifications = NotificationsServer::get_instance();
    bind_property_changed(&notifications, "notifications",
        [this, &notifications]() {
            auto len = notifications.notifications.size();
            if (len > 0) {
                set_css_classes({"notifications-exist"});
                set_text(std::format(" {}", len));
                popup->set_visible(true);
            } else {
                set_css_classes({"notifications-empty"});
                set_text(std::format(" ", len));
            }
            window->create_widgets();
        });
    click = Gtk::GestureClick::create();
    click->set_button(0); // 0 = all, 1 = left, 2 = center, 3 = right
    click->signal_pressed().connect([this, &notifications] (int n_press, double x, double y) {
        auto mbutton = this->click->get_current_button();
        if (mbutton == GDK_BUTTON_PRIMARY) {
            auto visible = window->get_visible();
            window->set_visible(!visible);
        } else if (mbutton == GDK_BUTTON_SECONDARY) {
            window->set_visible(false);
            popup->set_visible(false);
            NotificationsServer::get_instance().clear_notifications();
        }
    }, true);
    add_controller(click);
}

Notifications::~Notifications()
{
    delete popup;
    delete window;
}

SysTray::SysTray()
{
    add_css_class("systray");
    SystemTray &tray = SystemTray::get_instance();
    tray.signal_changed.connect(
        [this, &tray]() {
            set_visible(false);
            auto children = get_children();
            for (auto child : children)
                remove(*child);
            for (auto item : tray.registered_items) {
                append(*item.second);
            }
            set_hexpand(true);
            set_visible(true);
        });
}
