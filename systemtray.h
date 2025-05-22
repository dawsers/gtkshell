#ifndef __GTKSHELL_SYSTEMTRAY__
#define __GTKSHELL_SYSTEMTRAY__

#include <giomm.h>
#include <glibmm.h>
#include <gtkmm/image.h>
#include <gtkmm/popovermenu.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/menubutton.h>

class DBusMenu;

class SystemTrayItem : public Gtk::MenuButton {
public:
    SystemTrayItem(const Glib::ustring &bus_name, const Glib::ustring &path);
    ~SystemTrayItem();

    sigc::signal<void(Glib::ustring)> signal_removed;
    sigc::signal<void()> signal_changed;
    sigc::signal<void()> signal_ready;

private:
    void refresh_properties();
    void sync_icons();
    Glib::RefPtr<const Gdk::Pixbuf> parse_icon(const std::vector<std::tuple<int32_t, int32_t, std::vector<uint8_t>>> &icon_data);

    Glib::ustring bus_name;
    Glib::ustring path;

    Glib::ustring title;
    Glib::ustring tooltip;
    Glib::ustring status;

    Glib::ustring icon_name, overlay_icon_name, attention_icon_name;
    Glib::RefPtr<const Gdk::Pixbuf> icon_pixbuf, overlay_pixbuf, attention_pixbuf;
    Glib::RefPtr<DBusMenu> menu;
    Glib::RefPtr<Gtk::Image> image;

    Glib::RefPtr<Gio::DBus::Proxy> proxy;
};


// https://github.com/mate-desktop/mate-panel/blob/master/applets/notification_area/libstatus-notifier-watcher/org.kde.StatusNotifierWatcher.xml
// https://invent.kde.org/plasma/plasma-workspace/-/blob/master/statusnotifierwatcher/statusnotifierwatcher.cpp?ref_type=heads
// https://www.freedesktop.org/wiki/Specifications/StatusNotifierItem/StatusNotifierWatcher/
class SystemTray : public Glib::Object {
public:
    static SystemTray &get_instance();

    // Avoid copy creation
    SystemTray(const SystemTray &) = delete;
    void operator=(const SystemTray &) = delete;

    std::map<std::string, SystemTrayItem *> registered_items;
    // I don't use signal_added and signal_removed in the Widget for now.
    // It re-creates the full box with signal_changed
    sigc::signal<void(const Glib::ustring &)> signal_added;
    sigc::signal<void(const Glib::ustring &)> signal_removed;
    sigc::signal<void()> signal_changed;

private:
    SystemTray();
    virtual ~SystemTray();

    void on_server_method_call(const Glib::RefPtr<Gio::DBus::Connection> & /* connection */,
                               const Glib::ustring & /* sender */, const Glib::ustring & /* object_path */,
                               const Glib::ustring & /* interface_name */, const Glib::ustring &method_name,
                               const Glib::VariantContainerBase &parameters,
                               const Glib::RefPtr<Gio::DBus::MethodInvocation> &invocation);

    void on_server_get_property(Glib::VariantBase &property,
                                const Glib::RefPtr<Gio::DBus::Connection> &connection,
                                const Glib::ustring &sender, const Glib::ustring &object_path,
                                const Glib::ustring &interface_name, const Glib::ustring &property_name);



    Glib::RefPtr<Gio::DBus::Proxy> proxy;
    Glib::RefPtr<Gio::DBus::ObjectSkeleton> dbus;

    guint dbus_name_id;
    guint dbus_registered_id;
};


#endif // __GTKSHELL_SYSTEMTRAY__
