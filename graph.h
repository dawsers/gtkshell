#ifndef __GTKSHELL_GRAPH__
#define __GTKSHELL_GRAPH__

#include <glibmm.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/button.h>
#include <gtkmm/eventcontrollermotion.h>

#include <thread>

typedef struct Color {
    Color(double r, double g, double b) : r(r), g(g), b(b) {}
    double r, g, b;
} Color;

class Graph;

class CpuGraph : public Gtk::Box {
public:
    CpuGraph(int seconds, int hist, const Color &col);

private:
    void working_thread();

    Glib::RefPtr<Graph> drawing;
    size_t history;
    Color color;
    Gtk::Label label;
    Gtk::Button button;
    Glib::Dispatcher dispatcher;
    std::shared_ptr<std::thread> worker_thread;
    std::atomic<bool> working;
    std::binary_semaphore sem;
    std::mutex mtx;
    typedef struct {
        Glib::ustring result;
    } Shared;
    Shared shared_data;
    Glib::RefPtr<Gtk::EventControllerMotion> hover;
};

class MemGraph : public Gtk::Box {
public:
    MemGraph(int seconds, int hist, const Color &col);

private:
    Glib::RefPtr<Graph> drawing;
    size_t history;
    Color color;
    Gtk::Label label;
    Gtk::Button button;
    Glib::RefPtr<Gtk::EventControllerMotion> hover;
};

class GpuGraph : public Gtk::Box {
public:
    GpuGraph(int seconds, int hist, const Color &col_gpu, const Color &col_mem);

private:
    Glib::RefPtr<Graph> drawing;
    size_t history;
    Color color_gpu, color_mem;
    Gtk::Label label;
    Gtk::Button button;
    Glib::RefPtr<Gtk::EventControllerMotion> hover;
};

#endif // __GTKSHELL_GRAPH__
