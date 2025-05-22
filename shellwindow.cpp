#include "gtk4-layer-shell.h"
#include <gtkmm.h>

#include "shellwindow.h"

void GtkShellWindow::add_to_layer(const char *name, GtkShellAnchor anchor, const char *monitor_conn, bool exclusive_zone, GtkShellStretch stretch)
{
    GtkWindow *win = this->gobj();
    // Before the window is first realized, set it up to be a layer surface
    gtk_layer_init_for_window (win);
    gtk_layer_set_namespace(win, name);

    // Try to find the monitor connection, otherwise, use default
    auto display = Gdk::Display::get_default();
    auto monitors = display->get_monitors();
    for (uint i = 0; i < monitors->get_n_items(); ++i) {
        auto monitor = monitors->get_typed_object<Gdk::Monitor>(i);
        if (monitor->get_connector() == monitor_conn) {
            gtk_layer_set_monitor(win, monitor->gobj());
        }
    }

    // Order below normal windows
    gtk_layer_set_layer (win, GTK_LAYER_SHELL_LAYER_TOP);

    // Push other windows out of the way
    if (exclusive_zone)
        gtk_layer_auto_exclusive_zone_enable (win);

    // We don't need to get keyboard input
    // gtk_layer_set_keyboard_mode (gtk_window, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE); // NONE is default

    // The margins are the gaps around the window's edges
    // Margins and anchors can be set like this...
    // gtk_layer_set_margin (win, GTK_LAYER_SHELL_EDGE_LEFT, 40);
    // gtk_layer_set_margin (win, GTK_LAYER_SHELL_EDGE_RIGHT, 40);
    // gtk_layer_set_margin (win, GTK_LAYER_SHELL_EDGE_TOP, 20);
    // gtk_layer_set_margin (win, GTK_LAYER_SHELL_EDGE_BOTTOM, 0); // 0 is default

    // Anchors are if the window is pinned to each edge of the output
    // { LEFT, RIGHT, TOP, BOTTOM }
    // If two perpendicular edges are anchored, the surface will be anchored to that corner
    // If two opposite edges are anchored, the window will be stretched across the screen in that direction
    enum { LEFT, RIGHT, TOP, BOTTOM };
    gboolean anchors[4] = { FALSE, FALSE, FALSE, FALSE};
    if (stretch & GtkShellStretch::STRETCH_HORIZONTAL) {
        anchors[LEFT] = TRUE; anchors[RIGHT] = TRUE;
    }
    if (stretch & GtkShellStretch::STRETCH_VERTICAL) {
        anchors[TOP] = TRUE; anchors[BOTTOM] = TRUE;
    }
    if (anchor & GtkShellAnchor::ANCHOR_LEFT)
        anchors[LEFT] = TRUE;
    if (anchor & GtkShellAnchor::ANCHOR_RIGHT)
        anchors[RIGHT] = TRUE;
    if (anchor & GtkShellAnchor::ANCHOR_TOP)
        anchors[TOP] = TRUE;
    if (anchor & GtkShellAnchor::ANCHOR_BOTTOM)
        anchors[BOTTOM] = TRUE;

    for (int i = 0; i < GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER; ++i) {
        gtk_layer_set_anchor(win, (GtkLayerShellEdge) i, anchors[i]);
    }
}

