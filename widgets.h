#ifndef __GTKSHELL_WIDGETS__
#define __GTKSHELL_WIDGETS__

#include <gtkmm/button.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/label.h>
#include <gtkmm/box.h>
#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/popovermenu.h>
#include <glibmm.h>
#include "mpris.h"

#include <thread>

class PackageUpdates : public Gtk::Button {
public:
    PackageUpdates();
    ~PackageUpdates();

private:
    void update();
    void signal_update();

    Glib::Dispatcher dispatcher;
    sigc::connection timer;
    Gtk::Label updates_label;
    std::shared_ptr<std::thread> update_thread;
    std::binary_semaphore sem;
    std::atomic<bool> working;
#if 0
    Glib::RefPtr<Gtk::GestureClick> click;
#endif
    Glib::RefPtr<Gtk::EventControllerMotion> hover;
    // Shared data
    std::mutex mtx;
    typedef struct {
        Glib::ustring updates;
        int nupdates;
    } Shared;
    Shared shared_data;
};

class IdleInhibitor : public Gtk::ToggleButton {
public:
    IdleInhibitor();

private:
    Gtk::Label label;
};

class SysTray : public Gtk::Box {
public:
    SysTray();
};

class NotificationsPopup;
class NotificationsWindow;

class Notifications: public Gtk::Label {
public:
    Notifications(const std::string &monitor);
    ~Notifications();

private:
    NotificationsPopup *popup;
    NotificationsWindow *window;
    Glib::RefPtr<Gtk::GestureClick> click;
};

class ScreenShot : public Gtk::Button {
public:
    ScreenShot();

private:
    Glib::RefPtr<Gtk::GestureClick> click;
};

class MPlayer;

class MediaPlayerBox : public Gtk::Box {
public:
    MediaPlayerBox();
    ~MediaPlayerBox();

private:
    void add_player(Player *player);
    void remove_player(Player *player);
    void update_menu_items();
    
    Glib::RefPtr<Gtk::GestureClick> click;
    Glib::RefPtr<Gtk::PopoverMenu> menu_pop;
    Glib::RefPtr<Gio::SimpleActionGroup> action_group;
    std::map<std::string, MPlayer *> mplayers;
    MPlayer *visible = nullptr;
};

#endif
