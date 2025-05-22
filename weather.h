#ifndef __GTKSHELL_WEATHER__
#define __GTKSHELL_WEATHER__

#include <glibmm/dispatcher.h>
#include <gtkmm/box.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/popover.h>

#include <thread>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

class Weather : public Gtk::Box {
public:
    // polltime in seconds
    Weather(int polltime);
    ~Weather();

private:
    void update();
    void update_task();
    void create_notebook(const json &om_result);

    sigc::connection timer;
    Glib::RefPtr<Gtk::Popover> popover;
    Glib::RefPtr<Gtk::GestureClick> click;
    std::shared_ptr<std::thread> update_thread;
    std::binary_semaphore sem;
    std::atomic<bool> working = false;
    Glib::Dispatcher dispatcher;
    std::mutex mtx;
    typedef struct {
        json om_data;
        json ae_data;
    } Shared;
    Shared shared_data;
};

#endif
