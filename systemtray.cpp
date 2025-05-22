#include "dbus.h"
#include "systemtray.h"
#include "utils.h"

#include <glib.h>
#include <gdkmm.h>
#include <gtkmm/menubutton.h>

template<typename T>
T get_proxy_property(Glib::RefPtr<Gio::DBus::Proxy> &proxy, const Glib::ustring &name)
{
    Glib::Variant<T> prop;
    proxy->get_cached_property(prop, name);
    return prop.gobj() ? prop.get() : T();
}

// file:///usr/share/gtk-doc/html/libdbusmenu-glib/index.html
// https://github.com/ubuntu/gnome-shell-extension-appindicator/blob/master/dbusMenu.js
// https://github.com/JetBrains/libdbusmenu
class DBusMenuItem : public Gio::MenuItem {
public:
    DBusMenuItem(const std::map<Glib::ustring, Glib::VariantBase> &properties) : Gio::MenuItem() {
        for (auto property : properties) {
            if (property.first == "type") {
                type = property.second.get_dynamic<Glib::ustring>();
            } else if (property.first == "children-display") {
                children_display = property.second.get_dynamic<Glib::ustring>();
            } else if (property.first == "label") {
                label = property.second.get_dynamic<Glib::ustring>();
                set_label(label);
            } else if (property.first == "enabled") {
                enabled = property.second.get_dynamic<bool>();
            } else if (property.first == "visible") {
                visible = property.second.get_dynamic<bool>();
            } else if (property.first == "icon-name") {
                icon_name = property.second.get_dynamic<Glib::ustring>();
                icon = Gio::ThemedIcon::create(icon_name);
            } else if (property.first == "icon-data") {
                auto data = property.second.get_dynamic<std::vector<uint8_t>>();
                icon_data = Glib::Bytes::create(data.data(), data.size());
                icon = Gio::BytesIcon::create(icon_data);
            } else if (property.first == "toggle-type") {
                toggle_type = property.second.get_dynamic<Glib::ustring>();
            } else if (property.first == "toggle-state") {
                toggle_state = property.second.get_dynamic<int32_t>();
            }
        }
        // Set icon at the end so label doesn't overwrite it. No need.
        if (icon != nullptr)
            set_icon(icon);
    }
    Glib::ustring get_parameter_type() const { return type; }
    Glib::ustring get_parameter_label() const { return label; }
    bool get_parameter_visible() const { return visible; }
    bool get_parameter_enabled() const { return enabled; }
    Glib::ustring get_parameter_icon_name() const { return icon_name; }
    const Glib::RefPtr<Gio::Icon> &get_parameter_icon() const { return icon; }
    Glib::ustring get_parameter_toggle_type() const { return toggle_type; }
    int32_t get_parameter_toggle_state() const { return toggle_state; }
    Glib::ustring get_parameter_children_display() const { return children_display; }

private:
    // Properties
    Glib::ustring type;
    Glib::ustring label;
    bool visible = true;
    bool enabled = true;
    Glib::ustring icon_name;
    Glib::RefPtr<Glib::Bytes> icon_data;   // PNG data of the icon
    Glib::RefPtr<Gio::Icon> icon;
//    std::vector<std::vector<Glib::ustring>> shortcut;
    Glib::ustring toggle_type;
    int32_t toggle_state = -1;  // 0 = off, 1 = on, else = indeterminate
    Glib::ustring children_display;
};

class DBusMenu : public Gtk::PopoverMenu {
public:
    DBusMenu(const Glib::ustring &bus_name, const Glib::ustring &path) {
        add_css_class("systray-item");
        set_flags(Gtk::PopoverMenu::Flags::NESTED);
        set_expand(true);
        Glib::RefPtr<Gio::DBus::InterfaceInfo> interface_info = Gio::DBus::NodeInfo::create_for_xml(DBUS_CANONICAL_DBUSMENU)->lookup_interface("com.canonical.dbusmenu");
        if (path == "/StatusNotifierItem")
            proxy = Gio::DBus::Proxy::create_for_bus_sync(Gio::DBus::BusType::SESSION, bus_name, "/MenuBar", "com.canonical.dbusmenu", interface_info);
        else
            proxy = Gio::DBus::Proxy::create_for_bus_sync(Gio::DBus::BusType::SESSION, bus_name, path + "/Menu", "com.canonical.dbusmenu", interface_info);
        proxy->connect_property_changed("LayoutUpdated", [this] () { update_menu(); });
        proxy->connect_property_changed("ItemsPropertiesUpdated", [this] () { update_menu(); });

        update_menu();
    }

private:
    void update_menu() {
        auto params = Glib::Variant<std::tuple<int32_t, int32_t, std::vector<Glib::ustring>>>::create({ 0, -1, { "type", "children-display", "label", "enabled", "visible", "toggle-type", "toggle-state" } });
        try {
            auto response = proxy->call_sync("GetLayout", params);
            auto rev = response.get_child(0).get_dynamic<uint32_t>();
            if (rev > 0 && rev == revision)
                return;
            revision = rev;
            menu = Gio::Menu::create();
            action_group = Gio::SimpleActionGroup::create();
            recurse_menu_update(menu, response.get_child(1));
        } catch (Glib::Error &error) {
            Utils::log(Utils::LogSeverity::INFO, std::format("Error! {}", error.what()));
        }
        set_menu_model(menu);
        insert_action_group("actions", action_group);
    }

    Glib::ustring get_item_type(const Glib::VariantBase &container) {
        auto data = container.get_dynamic<std::tuple<int32_t, std::map<Glib::ustring, Glib::VariantBase>, std::vector<Glib::VariantBase>>>();
        auto properties = std::get<1>(data);
        auto type = properties.find("type");
        if (type != properties.end())
            return type->second.get_dynamic<Glib::ustring>();

        return "";
    }

    void send_click_event(int id) {
        auto params = Glib::Variant<std::tuple<int32_t, Glib::ustring, Glib::VariantBase, uint32_t>>::create({id, "clicked", Glib::Variant<uint8_t>::create(0), 0});
        try {
            proxy->call_sync("Event", params);
        } catch (Glib::Error &error) {
            Utils::log(Utils::LogSeverity::INFO, std::format("Error! {}", error.what()));
        }
    }

    void recurse_menu_update(Glib::RefPtr<Gio::Menu> &menu, const Glib::VariantBase &container) {
        auto data = container.get_dynamic<std::tuple<int32_t, std::map<Glib::ustring, Glib::VariantBase>, std::vector<Glib::VariantBase>>>();
        auto id = std::get<0>(data);
        auto properties = std::get<1>(data);

        // First, create a menu_item so we parse all the parameters in properties.
        // We may later discard it if it is a separator etc.
        auto menu_item = Glib::RefPtr<DBusMenuItem>(new DBusMenuItem(properties));
        auto type = menu_item->get_parameter_type();

        if (menu_item->get_parameter_children_display() == "submenu") {
            // This id contains a submenu
            bool in_section = false;
            auto submenu = Gio::Menu::create();
            auto children = std::get<2>(data);
            Glib::RefPtr<Gio::Menu> cursection;
            for (auto child : children) {
                if (get_item_type(child) == "separator") {
                    // We don't add separators as menu items, just create a new section
                    if (cursection != nullptr) {
                        submenu->append_section(cursection);
                    }
                    cursection = Gio::Menu::create();
                } else {
                    if (cursection != nullptr)
                        recurse_menu_update(cursection, child);
                    else
                        recurse_menu_update(submenu, child);
                }
            }
            if (cursection != nullptr)
                submenu->append_section(cursection);
            if (id == 0)
                menu = submenu;
            else
                menu->append_submenu(menu_item->get_parameter_label(), submenu);
            return;
        }
        Glib::ustring action_name = std::format("{}", id);
        Glib::RefPtr<Gio::SimpleAction> action;
        if (menu_item->get_parameter_toggle_type() == "checkmark") {
            action = Gio::SimpleAction::create_bool(action_name, menu_item->get_parameter_toggle_state() != 0);
            action->signal_change_state().connect([this, id, action](const Glib::VariantBase &var) {
                action->set_state(var);
                send_click_event(id);
            });
            action_group->add_action(action);
            menu_item->set_action("actions." + action_name);
        } else if (menu_item->get_parameter_toggle_type() == "radio") {
            action = Gio::SimpleAction::create_radio_integer(action_name, menu_item->get_parameter_toggle_state());
            action->signal_change_state().connect([this, id, action](const Glib::VariantBase &var) {
                action->set_state(var);
                send_click_event(id);
            });
            action_group->add_action(action);
            menu_item->set_action("actions." + action_name);
        } else {
            action = Gio::SimpleAction::create(action_name, Glib::VariantType("i"));
            action->signal_activate().connect([this] (const Glib::VariantBase &var) { send_click_event(var.get_dynamic<int32_t>()); });
            action_group->add_action(action);
            menu_item->set_action_and_target("actions." + action_name, Glib::Variant<int32_t>::create(id));
        }
        if (menu_item->get_parameter_enabled() == false) {
            action->set_enabled(false);
        }
        if (menu_item->get_parameter_visible()) {
            menu->append_item(menu_item);
        }
    }

    uint32_t revision = 0;
    Glib::RefPtr<Gio::Menu> menu;
    Glib::RefPtr<Gio::DBus::Proxy> proxy;
    Glib::RefPtr<Gio::SimpleActionGroup> action_group;
};


// https://www.freedesktop.org/wiki/Specifications/StatusNotifierItem/StatusNotifierItem/
SystemTrayItem::SystemTrayItem(const Glib::ustring &bus_name, const Glib::ustring &path) : bus_name(bus_name), path(path)
{
    add_css_class("systray-item");
    Glib::RefPtr<Gio::DBus::InterfaceInfo> item_interface_info = Gio::DBus::NodeInfo::create_for_xml(DBUS_KDE_STATUSNOTIFIERITEM)->lookup_interface("org.kde.StatusNotifierItem");
    Gio::DBus::Proxy::create_for_bus(Gio::DBus::BusType::SESSION, bus_name, path, "org.kde.StatusNotifierItem",
                                     [this, bus_name, path](Glib::RefPtr<Gio::AsyncResult> &result) {
                                         menu = Glib::RefPtr<DBusMenu>(new DBusMenu(bus_name, path));
                                         set_popover(*menu);
                                         proxy = Gio::DBus::Proxy::create_for_bus_finish(result);
                                         refresh_properties();
                                         proxy->connect_property_changed("g-name-owner",
                                                                         [this]() {
                                                                             signal_removed(this->bus_name + this->path);
                                                                         });
                                         proxy->connect_property_changed("g-signal",
                                                                         [this]() {
                                                                             refresh_properties();
                                                                         });
                                         proxy->connect_property_changed("g-properties-changed",
                                                                         [this]() {
                                                                             signal_changed();
                                                                         });
                                         proxy->connect_property_changed("NewIcon", [this] () { sync_icons(); });
                                         proxy->connect_property_changed("NewAttentionIcon", [this] () { sync_icons(); });
                                         proxy->connect_property_changed("NewOverlayIcon", [this] () { sync_icons(); });
                                         signal_ready(); },
                                     item_interface_info);
}

void SystemTrayItem::refresh_properties()
{
    sync_icons();
}

SystemTrayItem::~SystemTrayItem()
{

}

Glib::RefPtr<const Gdk::Pixbuf> SystemTrayItem::parse_icon(const std::vector<std::tuple<int32_t, int32_t, std::vector<uint8_t>>> &icon_data)
{
    // Get first icon
    auto width = std::get<0>(icon_data[0]);
    auto height = std::get<1>(icon_data[0]);
    auto pixbuf = Gdk::Pixbuf::create_from_data(std::get<2>(icon_data[0]).data(), Gdk::Colorspace::RGB, true, 8, width, height, 4 * width);
    return pixbuf;
}

void SystemTrayItem::sync_icons()
{
    icon_name = get_proxy_property<Glib::ustring>(proxy, "IconName");
    if (icon_name == "") {
        std::vector<std::tuple<int32_t, int32_t, std::vector<uint8_t>>> icon_pixmap = get_proxy_property<std::vector<std::tuple<int32_t, int32_t, std::vector<uint8_t>>>>(proxy, "IconPixmap");
        if (icon_pixmap.size() > 0) {
            icon_pixbuf = parse_icon(icon_pixmap);
        }
    }
    overlay_icon_name = get_proxy_property<Glib::ustring>(proxy, "OverlayIconName");
    if (overlay_icon_name == "") {
        std::vector<std::tuple<int32_t, int32_t, std::vector<uint8_t>>> overlay_icon_pixmap = get_proxy_property<std::vector<std::tuple<int32_t, int32_t, std::vector<uint8_t>>>>(proxy, "OverlayIconPixmap");
        if (overlay_icon_pixmap.size() > 0) {
            overlay_pixbuf = parse_icon(overlay_icon_pixmap);
        }
    }
    attention_icon_name = get_proxy_property<Glib::ustring>(proxy, "AttentionIconName");
    if (attention_icon_name == "") {
        std::vector<std::tuple<int32_t, int32_t, std::vector<uint8_t>>> attention_icon_pixmap = get_proxy_property<std::vector<std::tuple<int32_t, int32_t, std::vector<uint8_t>>>>(proxy, "AttentionIconPixmap");
        if (attention_icon_pixmap.size() > 0) {
            attention_pixbuf = parse_icon(attention_icon_pixmap);
        }
    }
    image = Glib::RefPtr<Gtk::Image>(new Gtk::Image());
    if (icon_name != "") {
        if (Glib::file_test(icon_name.c_str(), Glib::FileTest::EXISTS)) {
            image->set(icon_name);
        } else {
            image->set_from_icon_name(icon_name);
        }
    } else if (icon_pixbuf != nullptr) {
        image->set(icon_pixbuf);
    } else {
        image->set_from_icon_name("image-missing");
    }
    set_child(*image);
}


// SystemTray
SystemTray &SystemTray::get_instance()
{
    // After C++11 this is thread safe. No two threads are allowed to enter a
    // variable declaration's initialization concurrently.
    static SystemTray instance;

    return instance;
}
void SystemTray::on_server_method_call(const Glib::RefPtr<Gio::DBus::Connection> &connection,
  const Glib::ustring &sender, const Glib::ustring& /* object_path */,
  const Glib::ustring& /* interface_name */, const Glib::ustring& method_name,
  const Glib::VariantContainerBase& parameters,
  const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation)
{
    if (method_name == "RegisterStatusNotifierItem") {
        // service will be either the bus name (:1.xx), or the path
        // so if the service is a bus name, we create an object path equel to /StatusNotifier
        std::string service = parameters.get_child(0).get_dynamic<std::string>();
        Glib::ustring bus_name, path;
        if (service.starts_with('/')) {
            path = service;
            bus_name = sender;
        } else {
            bus_name = service;
            path = "/StatusNotifierItem";
        }
        invocation->return_value({});

        Glib::ustring item_name = bus_name + path;
        if (registered_items.find(item_name) != registered_items.end())
            return;

        auto item = new SystemTrayItem(bus_name, path);
        item->signal_ready.connect(
            [this, item, item_name, bus_name]() {
                registered_items[item_name] = item;
                signal_added(item_name);
                signal_changed();
                auto response = Glib::VariantContainerBase::create_tuple({Glib::Variant<std::string>::create(item_name)});
                auto connection = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SESSION);
                connection->emit_signal("/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher", "StatusNotifierItemRegistered", bus_name, response);
            });

        item->signal_removed.connect(
            [this, bus_name](const Glib::ustring &item_name) {
                auto iter = registered_items.find(item_name);
                if (iter != registered_items.end()) {
                    delete iter->second;
                    registered_items.erase(iter);
                }
                signal_removed(item_name);
                signal_changed();
                auto response = Glib::VariantContainerBase::create_tuple({Glib::Variant<std::string>::create(item_name)});
                auto connection = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SESSION);
                connection->emit_signal("/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher", "StatusNotifierItemRegistered", bus_name, response);
            });

    } else if (method_name == "RegisterStatusNotifierHost") {
        // By setting IsStatusNotifierHostRegistered to true, there is no need to do anything here.
        Utils::log(Utils::LogSeverity::INFO, std::format("system tray: DBus: {} called RegisterStatusNotifierHost", sender.c_str()));
    } else {
        Utils::log(Utils::LogSeverity::WARNING, std::format("system tray: DBus: unknown method called: {}", method_name.c_str()));
    }
}

void SystemTray::on_server_get_property(Glib::VariantBase& property,
  const Glib::RefPtr<Gio::DBus::Connection>& connection,
  const Glib::ustring& sender, const Glib::ustring& object_path,
  const Glib::ustring& interface_name, const Glib::ustring& property_name)
{
    if (property_name == "RegisteredStatusNotifierItems") {
        std::vector<std::string> items;
        for (auto item : registered_items) {
            items.push_back(item.first);
        }
        property = Glib::Variant<std::vector<std::string>>::create(items);
    } else if (property_name == "IsStatusNotifierHostRegistered") {
        property = Glib::Variant<bool>::create(true);
    } else if (property_name == "ProtocolVersion") {
        property = Glib::Variant<int32_t>::create(0);
    } else {
        Utils::log(Utils::LogSeverity::WARNING, std::format("system tray: DBus: trying to query unknown property: {}", property_name.c_str()));
    }
}

SystemTray::SystemTray() :
    Glib::ObjectBase(typeid(SystemTray))
{
    try {
        dbus_name_id = Gio::DBus::own_name(Gio::DBus::BusType::SESSION, "org.kde.StatusNotifierWatcher",
                            // Bus acquired
                            [this] (const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &name) {
                                try {
                                    auto introspection_data = Gio::DBus::NodeInfo::create_for_xml(DBUS_KDE_STATUSNOTIFIERWATCHER);
                                    dbus_registered_id = connection->register_object("/StatusNotifierWatcher", introspection_data->lookup_interface(),
                                                                                     sigc::mem_fun(*this, &SystemTray::on_server_method_call),
                                                                                     sigc::mem_fun(*this, &SystemTray::on_server_get_property));
                                } catch (const Glib::Error &error) {
                                    Utils::log(Utils::LogSeverity::ERROR, std::format("system tray: DBus: cannot create service: {}", error.what()));     
                                }
                            },
                            // Name acquired
                            [] (const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &name) {

                            },
                            // Name lost
                            [this] (const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &name) {
                                Utils::log(Utils::LogSeverity::ERROR, "system tray: DBus: Another system tray is already running");
                                connection->unregister_object(dbus_registered_id);
                            });

    } catch (const Gio::Error &error) {
        Utils::log(Utils::LogSeverity::ERROR, std::format("system tray: DBus: error initializing service: {}", error.what()));     
    }
}

SystemTray::~SystemTray()
{
    Gio::DBus::unown_name(dbus_name_id);

    for (auto item : registered_items) {
        delete item.second;
    }
}
