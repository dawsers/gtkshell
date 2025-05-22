#include "dbus.h"
#include "notifications.h"
#include "utils.h"

#include <glib.h>
#include <gdkmm.h>


const std::string NOTIFICATIONS_CACHE_PATH = Utils::XDG_CACHE_HOME + "/gtkshell/notifications";
const std::string CACHE_FILE = NOTIFICATIONS_CACHE_PATH + "/notifications.json";

// https://docs.gtk.org/glib/gvariant-format-strings.html
// https://specifications.freedesktop.org/notification-spec/latest/hints.html

// (iiibiiay)
// width, height, rowstride, has alpha, bits per sample, channels and image data
typedef struct {
    int32_t w, h;
    int32_t rs;
    bool alpha;
    int32_t bps;
    int32_t channels;
} ImageHeader;

Notification::Notification(const Glib::ustring &app_name, uint32_t id, const Glib::ustring &app_icon,
                           const Glib::ustring &summary, const Glib::ustring &body,
                           const std::vector<Glib::ustring> &actions,
                           const std::map<Glib::ustring, Glib::VariantBase> &hints,
                           int32_t expire_timeout)
    : app_name(app_name), id(id), app_icon(app_icon), summary(summary),
      body(body), expire_timeout(expire_timeout), popup(true)
{
    time = Glib::DateTime::create_now_local().to_unix();
    for (size_t i = 0; i < actions.size(); i += 2) {
        this->actions.push_back({actions[i * 2], actions[i * 2 + 1]});
    }

    // Hints we want to keep
    for (auto &[key, val] : hints) {
        if (key == "action-icons") {
            action_icons = val.get_dynamic<bool>();
        } else if (key == "category") {
            category = val.get_dynamic<Glib::ustring>();
        } else if (key == "desktop-entry") {
            desktop_entry = val.get_dynamic<Glib::ustring>();
        } else if (key == "resident") {
            resident = val.get_dynamic<bool>();
        } else if (key == "sound-file") {
            sound_file = val.get_dynamic<Glib::ustring>();
        } else if (key == "sound-name") {
            sound_name = val.get_dynamic<Glib::ustring>();
        } else if (key == "suppress-sound") {
            suppress_sound = val.get_dynamic<bool>();
        } else if (key == "transient") {
            transient = val.get_dynamic<bool>();
        } else if (key == "x") {
            x = val.get_dynamic<int32_t>();
        } else if (key == "y") {
            y = val.get_dynamic<int32_t>();
        } else if (key == "urgency") {
            urgency = Urgency(val.get_dynamic<uint8_t>());
        }
    }
    // Priority for images and icons
    // https://specifications.freedesktop.org/notification-spec/latest/icons-and-images.html
    /*
      An implementation which only displays one image or icon must choose which
      one to display using the following order: "image-data" "image-path"
        app_icon parameter
        for compatibility reason, "icon_data"

      An implementation which can display both the image and icon must show the
      icon from the "app_icon" parameter and choose which image to display using
      the following order:

        "image-data"
        "image-path"
        for compatibility reason, "icon_data"
    */
    // The application should be smart enough not to send several of these
    // in the message, but just in case...
    Glib::ustring file;
    auto iter = hints.find("image-data");
    if (iter != hints.end()) {
        file = parse_image_data(iter->second);
    } else {
        iter = hints.find("image-path");
        if (iter != hints.end()) {
            file = iter->second.get_dynamic<Glib::ustring>();
        } else {
            iter = hints.find("icon_data");
            if (iter != hints.end()) {
                file = parse_image_data(iter->second);
            }
        }
    }
    image_path = file;
}

Notification *Notification::from_json(const json &j)
{
    auto n = new Notification();
    for (auto &[key, value] : j.items()) {
        if (key == "app_name") {
            n->app_name = Glib::ustring(value.get<std::string>());
        } else if (key == "id") {
            n->id = value.get<uint32_t>();
        } else if (key == "time") {
            n->time = value.get<uint64_t>();
        } else if (key == "app_icon") {
            n->app_icon = Glib::ustring(value.get<std::string>());
        } else if (key == "summary") {
            n->summary = Glib::ustring(value.get<std::string>());
        } else if (key == "body") {
            n->body = Glib::ustring(value.get<std::string>());
        } else if (key == "actions") {
            std::vector<json::object_t> actions = value.get<std::vector<json::object_t>>();
            for (auto action : actions) {
                n->actions.push_back({ action["id"].get<std::string>(), action["label"].get<std::string>() });
            }
        } else if (key == "expire_timeout") {
            n->expire_timeout = value.get<int32_t>();
        } else if (key == "action_icons") {
            n->action_icons = value.get<bool>();
        } else if (key == "category") {
            n->category = Glib::ustring(value.get<std::string>());
        } else if (key == "desktop_entry") {
            n->desktop_entry = Glib::ustring(value.get<std::string>());
        } else if (key == "image_path") {
            n->image_path = Glib::ustring(value.get<std::string>());
        } else if (key == "resident") {
            n->resident = value.get<bool>();
        } else if (key == "sound_file") {
            n->sound_file = Glib::ustring(value.get<std::string>());
        } else if (key == "sound_name") {
            n->sound_name = Glib::ustring(value.get<std::string>());
        } else if (key == "suppress_sound") {
            n->suppress_sound = value.get<bool>();
        } else if (key == "transient") {
            n->transient = value.get<bool>();
        } else if (key == "x") {
            n->x = value.get<int32_t>();
        } else if (key == "y") {
            n->y = value.get<int32_t>();
        } else if (key == "urgency") {
            n->urgency = Urgency(value.get<std::string>());
        }
    }
    return n;
}

json Notification::to_json() const
{
    json j;

    j["app_name"] = app_name;
    j["id"] = id;
    j["time"] = time;
    if (app_icon != "")
        j["app_icon"] = app_icon;
    j["summary"] = summary;
    j["body"] = body;
    auto array = json::array();
    for (size_t i = 0; i < actions.size(); ++i) {
        array[i] = { { "id", actions[i].id  }, { "label", actions[i].label } };
    }
    j["actions"] = array;
    j["expire_timeout"] = expire_timeout;
    j["action_icons"] = action_icons;
    if (category != "")
        j["category"] = category;
    if (desktop_entry != "")
        j["desktop_entry"] = desktop_entry;
    if (image_path != "")
        j["image_path"] = image_path;
    j["resident"] = resident;
    if (sound_file != "")
        j["sound_file"] = sound_file;
    if (sound_name != "")
        j["sound_name"] = sound_name;
    j["suppress_sound"] = suppress_sound;
    j["transient"] = transient;
    if (x != 0 || y != 0) {
        j["x"] = x;
        j["y"] = y;
    }
    j["urgency"] = urgency.to_string();

    return j;
}

#if 1
// Remove from sight
void Notification::dismiss()
{
    auto &server = NotificationsServer::get_instance();
    server.dismiss_notification(id);
}

// Delete notification from storage
void Notification::close()
{
    auto &server = NotificationsServer::get_instance();
    server.close_notification(id);
}

void Notification::invoke(const Glib::ustring &action_id)
{
    auto &server = NotificationsServer::get_instance();
    server.invoke_notification(id, action_id);
}
#endif

Notification::Notification() {}

Glib::ustring Notification::parse_image_data(const Glib::VariantBase &var)
{
    auto image = var.get_dynamic<std::tuple<int32_t, int32_t, int32_t, bool, int32_t, int32_t, std::vector<uint8_t>>>();
    ImageHeader header = {
        std::get<0>(image),
        std::get<1>(image),
        std::get<2>(image),
        std::get<3>(image),
        std::get<4>(image),
        std::get<5>(image),
    };
    auto data = std::get<6>(image);

    if (data.size() == 0)
        return "";

    if (header.bps != 8) {
        Utils::log(Utils::LogSeverity::WARNING, "Notifications: parse_image_data(): only 8-bit RGB images are supported");
        return "";
    }

    if (Utils::ensure_directory(NOTIFICATIONS_CACHE_PATH)) {
        auto pix_buf = Gdk::Pixbuf::create_from_data(data.data(), Gdk::Colorspace::RGB, header.alpha, 
                                                     header.bps, header.w, header.h,
                                                     header.rs);
        if (pix_buf == nullptr) {
            Utils::log(Utils::LogSeverity::WARNING, "Notifications: parse_image_data(): error creating Pixbuf");
            return "";
        }
        const std::string file_name = std::format("{}/{}", NOTIFICATIONS_CACHE_PATH, id);
        pix_buf->save(file_name, "png");
        return file_name;
    }
    return "";
}


NotificationsServer &NotificationsServer::get_instance()
{
    // After C++11 this is thread safe. No two threads are allowed to enter a
    // variable declaration's initialization concurrently.
    static NotificationsServer instance;

    return instance;
}

void NotificationsServer::notify_method(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::VariantContainerBase &parameters, const Glib::RefPtr<Gio::DBus::MethodInvocation> &invocation)
{
    /*
        UINT32 org.freedesktop.Notifications.Notify(
            STRING app_name,
            UINT32 replaces_id,
            STRING app_icon,
            STRING summary,
            STRING body,
            as actions,
            a{sv} hints,
            INT32 expire_timeout);

         If replaces_id is 0, the return value is a UINT32 that represent
         the notification. It is unique, and will not be reused unless a MAXINT
         number of notifications have been generated. An acceptable
         implementation may just use an incrementing counter for the ID.
         The returned ID is always greater than zero. Servers must make sure
         not to return zero as an ID.

         If replaces_id is not 0, the returned value is the same value as
         replaces_id. 
    */
    Glib::ustring app_name = parameters.get_child(0).get_dynamic<Glib::ustring>();
    uint32_t replaces_id = parameters.get_child(1).get_dynamic<uint32_t>();
    Glib::ustring app_icon = parameters.get_child(2).get_dynamic<Glib::ustring>();
    Glib::ustring summary = parameters.get_child(3).get_dynamic<Glib::ustring>();
    Glib::ustring body = parameters.get_child(4).get_dynamic<Glib::ustring>();
    std::vector<Glib::ustring> actions = parameters.get_child(5).get_dynamic<std::vector<Glib::ustring>>();
    std::map<Glib::ustring, Glib::VariantBase> hints = parameters.get_child(6).get_dynamic<std::map<Glib::ustring, Glib::VariantBase>>();
    int32_t expire_timeout = parameters.get_child(7).get_dynamic<int32_t>();

    uint32_t id = replaces_id != 0 ? replaces_id : ++id_count;

    Notification *n = new Notification(app_name, id, app_icon, summary, body, actions, hints, expire_timeout);
    notifications[id] = n;

    const auto response = Glib::Variant<uint32_t>::create(id);
    invocation->return_value(Glib::VariantContainerBase::create_tuple(response));
    connection->flush_sync();
    notifications_changed.notify();
    notifications_popup.notify();

    // Deal with timeout
    if (expire_timeout > 0) {
        expiration_timers[id] = Glib::signal_timeout().connect([this, id]() {
            close_notification(id, CLOSE_EXPIRED);
            expiration_timers.erase(id);
            return false;
        }, expire_timeout);
    }

    save_notifications();
}

void NotificationsServer::disconnect_expiration_timer(uint32_t id)
{
    auto iter = expiration_timers.find(id);
    if (iter != expiration_timers.end()) {
        iter->second.disconnect();
        expiration_timers.erase(iter);
    }
}

void NotificationsServer::close_notification_method(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::VariantContainerBase &parameters, const Glib::RefPtr<Gio::DBus::MethodInvocation> &invocation)
{
    /*
         void org.freedesktop.Notifications.CloseNotification(UINT32 id);

         Causes a notification to be forcefully closed and removed from the
         user's view. It can be used, for example, in the event that what
         the notification pertains to is no longer relevant, or to cancel
         a notification with no expiration time.

        The NotificationClosed signal is emitted by this method.

        If the notification no longer exists, an empty D-BUS Error message is sent back. 
     */
    uint32_t id = parameters.get_child(0).get_dynamic<uint32_t>();
    auto iter = notifications.find(id);
    if (iter == notifications.end()) {
        invocation->return_value({});
        connection->flush_sync();
    } else {
        disconnect_expiration_timer(id);
        const auto response = Glib::VariantContainerBase::create_tuple({ Glib::Variant<uint32_t>::create(id), Glib::Variant<uint32_t>::create(CLOSE_CLOSED) });
        /*
            The reason the notification was closed.
            1 - The notification expired.
            2 - The notification was dismissed by the user.
            3 - The notification was closed by a call to CloseNotification.
            4 - Undefined/reserved reasons.
         */
        connection->emit_signal("/org/freedesktop/Notifications", "org.freedesktop.Notifications", "NotificationClosed", {}, response);
    }
}

void NotificationsServer::on_server_method_call(const Glib::RefPtr<Gio::DBus::Connection> &connection,
  const Glib::ustring& /* sender */, const Glib::ustring& /* object_path */,
  const Glib::ustring& /* interface_name */, const Glib::ustring& method_name,
  const Glib::VariantContainerBase& parameters,
  const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation)
{
    if (method_name == "Notify") {
        notify_method(connection, parameters, invocation);
    } else if (method_name == "CloseNotification") {
        close_notification_method(connection, parameters, invocation);
    } else if (method_name == "GetCapabilities") {
        const auto capabilities = Glib::Variant<std::vector<Glib::ustring>>::create(
            { "action-icons", "actions", "body", "body-images", "body-hyperlinks",
              "body-markup", "icon-static", "persistence", "sound" }
        );
        const auto response = Glib::VariantContainerBase::create_tuple(capabilities);
        invocation->return_value(response);
        connection->flush_sync();
    } else if (method_name == "GetServerInformation") {
        const std::vector<Glib::VariantBase> server = {
            Glib::Variant<Glib::ustring>::create("GTKShell Notifications Server"),
            Glib::Variant<Glib::ustring>::create("dawsers"),
            Glib::Variant<Glib::ustring>::create("1.0"),
            Glib::Variant<Glib::ustring>::create("1.2")
        };
        const auto response = Glib::VariantContainerBase::create_tuple(server);
        invocation->return_value(response);
        connection->flush_sync();
    } else {
        Utils::log(Utils::LogSeverity::WARNING, std::format("notifications: DBus: unknown method called: {}", method_name.c_str()));
    }
}

NotificationsServer::NotificationsServer() :
    Glib::ObjectBase(typeid(NotificationsServer)),
    server_available(*this, "running", false),
    notifications_changed(*this, "notifications", false),
    notifications_popup(*this, "popup", false), id_count(0)
{
    // Read cached notifications from file
    try {
        auto txt_notifications = Utils::read_file(Utils::XDG_CACHE_HOME + "/gtkshell/notifications/notifications.json");
        if (txt_notifications != "") {
            auto json_notifications = json::parse(txt_notifications);
            notifications.clear();
            if (json_notifications.is_array()) {
                for (auto notification : json_notifications) {
                    auto n = Notification::from_json(notification);
                    if (n->id > id_count) id_count = n->id;
                    notifications[n->id] = n;
                }
            }
            // This notification doesn't connect, the widget using get_instance()
            // hasn't set the connection up yet. Maybe use a property and revisit other
            // widgets using this.
            //notifications_changed_signal();
            // Use property
            notifications_changed.notify();
        }
    } catch (Gio::Error &err){
    }

    try {
        dbus_name_id = Gio::DBus::own_name(Gio::DBus::BusType::SESSION, "org.freedesktop.Notifications",
                            // Bus acquired
                            [this] (const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &name) {
                                try {
                                    auto introspection_data = Gio::DBus::NodeInfo::create_for_xml(DBUS_FREEDESKTOP_NOTIFICATIONS);
                                    dbus_registered_id = connection->register_object("/org/freedesktop/Notifications", introspection_data->lookup_interface(),
                                                                                     sigc::mem_fun(*this, &NotificationsServer::on_server_method_call));
                                    // Our server is ready to receive notifications
                                    server_available.set_value(true);
                                } catch (const Glib::Error &error) {
                                    Utils::log(Utils::LogSeverity::ERROR, std::format("notifications: DBus: cannot create service: {}", error.what()));     
                                }
                            },
                            // Name acquired
                            [] (const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &name) {

                            },
                            // Name lost
                            [this] (const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &name) {
                                try {
                                    auto conn = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SESSION);
                                    auto proxy = Gio::DBus::Proxy::create_sync(conn, "org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications");

                                    // The proxy's call method returns a tuple of the value(s) that the method
                                    // call produces so just get the tuple as a VariantContainerBase.
                                    const auto call_result = proxy->call_sync("GetServerInformation");
                                    const auto server_name = call_result.get_child(0).get_dynamic<Glib::ustring>();
                                    Utils::log(Utils::LogSeverity::ERROR, std::format("Notifications: DBus: Another notification server is running: {}", server_name.c_str()));
                                } catch (Glib::Error &error) {
                                    // GetServerInformation not implemented or connection error
                                    Utils::log(Utils::LogSeverity::ERROR, std::format("Notifications: DBus: Another notification server is running: {}", error.what()));
                                }
                                // Another server is available to receive our notifications
                                server_available.set_value(true);
                                connection->unregister_object(dbus_registered_id);
                            });

    } catch (const Gio::Error &error) {
        Utils::log(Utils::LogSeverity::ERROR, std::format("notificationss: DBus: error initializing service: {}", error.what()));     
    }
}

/*
    The reason the notification was closed.
    1 - The notification expired.
    2 - The notification was dismissed by the user.
    3 - The notification was closed by a call to CloseNotification.
    4 - Undefined/reserved reasons.
 */
// Send the closed notification signal to the client
void NotificationsServer::send_closed_notification_signal(uint32_t id, NotificationCloseReason reason)
{
    disconnect_expiration_timer(id);

    auto connection = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SESSION);

    const auto response = Glib::VariantContainerBase::create_tuple({ Glib::Variant<uint32_t>::create(id), Glib::Variant<uint32_t>::create(reason) });
    connection->emit_signal("/org/freedesktop/Notifications", "org.freedesktop.Notifications", "NotificationClosed", {}, response);
    connection->flush_sync();
}

NotificationsServer::~NotificationsServer()
{
    Gio::DBus::unown_name(dbus_name_id);

    for (auto notification : notifications) {
        auto n = notification.second;
        send_closed_notification_signal(n->id, CLOSE_DISMISSED);
        delete n;
    }
}

void NotificationsServer::save_notifications() const
{
    auto j = json::array();
    for (auto notification : notifications) {
        auto n = notification.second;
        if (n->resident && !n->transient) {
            j.push_back(n->to_json());
        }
    }
    std::string text = j.dump(4);
    if (!Utils::write_file(Utils::XDG_CACHE_HOME + "/gtkshell/notifications/notifications.json", text)) {
        Utils::log(Utils::LogSeverity::ERROR, "Notifications: Could not save the notifications cache");
    }
}

void NotificationsServer::close_notification(uint32_t id, NotificationCloseReason reason)
{
    auto iter = notifications.find(id);
    if (iter != notifications.end()) {
        auto n = iter->second;
        send_closed_notification_signal(n->id, reason);
        delete n;
        notifications.erase(iter);
        notifications_changed.notify();
        notifications_popup.notify();
        save_notifications();
    }
}
 
void NotificationsServer::dismiss_notification(uint32_t id)
{
    auto iter = notifications.find(id);
    if (iter != notifications.end()) {
        auto n = iter->second;
        send_closed_notification_signal(n->id, CLOSE_DISMISSED);
        n->popup = false;
        notifications_popup.notify();
        notifications_changed.notify();
    }
}

void NotificationsServer::invoke_notification(uint32_t id, const Glib::ustring &action_id)
{
    auto iter = notifications.find(id);
    if (iter != notifications.end()) {
        auto connection = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SESSION);
        const auto response = Glib::VariantContainerBase::create_tuple({ Glib::Variant<uint32_t>::create(id), Glib::Variant<Glib::ustring>::create(action_id) });
        connection->emit_signal("/org/freedesktop/Notifications", "org.freedesktop.Notifications", "ActionInvoked", {}, response);
        connection->flush_sync();
        if (!iter->second->resident) {
            // Close if not resident
            notifications.erase(iter);
            if (iter->second->popup)
                notifications_popup.notify();
            notifications_changed.notify();
        }
    }
}

void NotificationsServer::clear_notifications()
{
    for (auto notification : notifications) {
        auto n = notification.second;
        send_closed_notification_signal(n->id, CLOSE_DISMISSED);
        delete n;
    }
    notifications.clear();
    notifications_changed.notify();
    save_notifications();
}
    
#if 0
void NotificationsServer::add_notification(const Glib::ustring &app_name, const Glib::ustring &app_icon,
                                           const Glib::ustring &summary, const Glib::ustring &body)
{
    int32_t id = ++id_count;

    Notification *n = new Notification(app_name, id, app_icon, summary, body, {}, {}, -1);
    notifications[id] = std::pair<const Glib::RefPtr<Gio::DBus::Connection>, Notification*>(nullptr, n);
    notifications_popup.notify();
    notifications_changed.notify();
    save_notifications();
}
#endif

NotificationsClient &NotificationsClient::get_instance()
{
    // After C++11 this is thread safe. No two threads are allowed to enter a
    // variable declaration's initialization concurrently.
    static NotificationsClient instance;

    return instance;
}

NotificationsClient::NotificationsClient() : sem_pro(0), sem_con(1)
{
    // Start send_notifications thread
    worker = std::make_shared<std::thread>(&NotificationsClient::send_notifications, this);
    worker->detach();
    // The notifications client should "never" fail. If our NotificationsServer
    // is running, it will take the notification, otherwise, the one running, should.
    NotificationsServer &server = NotificationsServer::get_instance();
    if (server.server_available.get_value() == false) {
        server.connect_property_changed("running",
            [&server, this]() {
                if (server.server_available.get_value() == true) {
                    try {
                        auto connection = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SESSION);
                        proxy = Gio::DBus::Proxy::create_sync(connection, "org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications");
                    } catch (const Glib::Error &error) {
                        Utils::log(Utils::LogSeverity::ERROR, std::format("notify: DBus: error creating notifications client: {}", error.what()));
                    }
                }
            }
        );
    }
}

NotificationsClient::~NotificationsClient()
{
}

void NotificationsClient::send_notifications()
{
    while(true) {
        sem_pro.acquire();
        try {
            // The proxy's call method returns a tuple of the value(s) that the method
            // call produces so just get the tuple as a VariantContainerBase.
            std::vector<Glib::ustring> actions;
            std::map<Glib::ustring, Glib::VariantBase> hints;
            const auto params = Glib::VariantContainerBase::create_tuple({
                Glib::Variant<Glib::ustring>::create(notification.app_name),
                Glib::Variant<uint32_t>::create(0),
                Glib::Variant<Glib::ustring>::create(notification.app_icon),
                Glib::Variant<Glib::ustring>::create(notification.summary),
                Glib::Variant<Glib::ustring>::create(notification.body),
                Glib::Variant<std::vector<Glib::ustring>>::create(actions),
                Glib::Variant<std::map<Glib::ustring, Glib::VariantBase>>::create(hints),
                Glib::Variant<int32_t>::create(-1)
            });
            const auto call_result = proxy->call_sync("Notify", params);
        } catch (const Glib::Error &error) {
            Utils::log(Utils::LogSeverity::ERROR, std::format("notify: DBus: error sending notification to server: {}", error.what()));
        }
        sem_con.release();
    }
}

void NotificationsClient::dispatch_notification(const Glib::ustring &app_name, const Glib::ustring &app_icon,
                                           const Glib::ustring &summary, const Glib::ustring &body)
{
    sem_con.acquire();
    notification = { app_name, app_icon, summary, body };
    sem_pro.release();
}

void NotificationsClient::add_notification(const Glib::ustring &app_name, const Glib::ustring &app_icon,
                                           const Glib::ustring &summary, const Glib::ustring &body)
{
    NotificationsServer &server = NotificationsServer::get_instance();
    if (server.server_available.get_value() == false) {
        server.connect_property_changed("running",
                              [&server, this, app_name, app_icon, summary, body]() {
                                  if (server.server_available.get_value() == true) {
                                    dispatch_notification(app_name, app_icon, summary, body);
                                  } else {
                                    add_notification(app_name, app_icon, summary, body);
                                  }
                              }
        );
    } else {
        dispatch_notification(app_name, app_icon, summary, body);
    }
}

