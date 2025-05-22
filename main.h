#ifndef __GTKSHELL_MAIN__
#define __GTKSHELL_MAIN__

#include <gtkmm/application.h>

class Bar;

class GtkShell : public Gtk::Application {
public:
    virtual ~GtkShell();     

    static Glib::RefPtr<GtkShell> create(const Glib::ustring &app_id);

    void on_activate();

private:
    GtkShell(const Glib::ustring &app_id);

    std::vector<Bar *> bars;
};

#endif
