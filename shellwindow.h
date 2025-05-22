#ifndef __GTKSHELL_SHELLWINDOW__
#define __GTKSHELL_SHELLWINDOW__

#include <gtkmm/window.h>


enum GtkShellAnchor {
    ANCHOR_NONE = 0,
    ANCHOR_TOP = (1 << 0),
    ANCHOR_BOTTOM = (1 << 1),
    ANCHOR_LEFT = (1 << 2),
    ANCHOR_RIGHT = (1 << 3)
};

inline GtkShellAnchor operator|(GtkShellAnchor a, GtkShellAnchor b) {
    return static_cast<GtkShellAnchor>(static_cast<int>(a) | static_cast<int>(b));
}

enum GtkShellStretch {
    STRETCH_NONE = 0,
    STRETCH_HORIZONTAL = (1 << 0),
    STRETCH_VERTICAL = (1 << 1)
};

inline GtkShellStretch operator|(GtkShellStretch a, GtkShellStretch b) {
    return static_cast<GtkShellStretch>(static_cast<int>(a) | static_cast<int>(b));
}

class GtkShellWindow : public Gtk::Window {
public:
    GtkShellWindow(const char *name, GtkShellAnchor anchor, const char *monitor_conn, bool exclusive_zone = true, GtkShellStretch stretch = GtkShellStretch::STRETCH_HORIZONTAL) {
        add_to_layer(name, anchor, monitor_conn, exclusive_zone, stretch);
    }
    virtual ~GtkShellWindow() {}

private:
    void add_to_layer(const char *name, GtkShellAnchor anchor, const char *monitor_conn, bool exclusive_zone, GtkShellStretch stretch);
};

#endif // __GTKSHELL_SHELLWINDOW__
