#ifndef __GTKSHELL_NOTIFICATIONS__
#define __GTKSHELL_NOTIFICATIONS__

#include <giomm.h>
#include <glibmm.h>
#include <unordered_map>
#include <thread>
#include <semaphore>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

typedef struct {
    Glib::ustring id;
    Glib::ustring label;
} Action;

/*
    The reason the notification was closed.
    1 - The notification expired.
    2 - The notification was dismissed by the user.
    3 - The notification was closed by a call to CloseNotification.
    4 - Undefined/reserved reasons.
 */
typedef enum {
    CLOSE_EXPIRED = 1,
    CLOSE_DISMISSED = 2,
    CLOSE_CLOSED = 3,
    CLOSE_UNDEFINED = 4,
} NotificationCloseReason;

class Urgency {
public:
    enum Enum {
        LOW = 0,
        NORMAL,
        CRITICAL
    };

    constexpr Urgency() = default;
    /* implicit */ constexpr Urgency(Enum e) : e(e) {}
    constexpr Urgency(const Glib::ustring &str) { e = from_string(str); }
    constexpr Urgency(uint8_t urgency) { e = from_int(urgency); }

    // Allows comparisons with Enum constants.
    constexpr operator Enum() const { return e; }

    // Needed to prevent if(c)
    explicit operator bool() const = delete;

    std::string_view to_string() const {
        switch (e) {
            case LOW: return "low";
            case CRITICAL: return "critical";
            case NORMAL:
            default:
                return "normal";
        }
    }
    Urgency from_string(const Glib::ustring &str) {
        if (str == "low")
            return Urgency::LOW;
        else if (str == "normal")
            return Urgency::NORMAL;
        else
            return Urgency::CRITICAL;
    }

private:
    Urgency from_int(uint8_t urgency) {
        switch (urgency) {
            case 0:
            default:
                return Urgency::LOW;
            case 1: return Urgency::NORMAL;
            case 2: return Urgency::CRITICAL;
        }
    }
    Enum e;
};

class Notification {
public:
    Notification(const Glib::ustring &app_name, uint32_t id, const Glib::ustring &app_icon,
                 const Glib::ustring &summary, const Glib::ustring &body,
                 const std::vector<Glib::ustring> &actions, const std::map<Glib::ustring, Glib::VariantBase> &hints, int32_t expire_timeout);

    static Notification *from_json(const json &j);
    json to_json() const;

    void dismiss();
    void close();
    void invoke(const Glib::ustring &id);

    Glib::ustring app_name;
    uint32_t id = 0;
    Glib::ustring app_icon;
    Glib::ustring summary;
    Glib::ustring body;
    std::vector<Action> actions;
    int32_t expire_timeout = -1;

    // Coming from hints
    bool action_icons = false;
    Glib::ustring category;
    Glib::ustring desktop_entry;
    Glib::ustring image_path;
    bool resident = false;
    Glib::ustring sound_file;
    Glib::ustring sound_name;
    bool suppress_sound = false;
    bool transient = false;
    int32_t x = 0, y = 0;
    Urgency urgency = Urgency::NORMAL;

    uint64_t time = 0;
    bool popup;

private:
    Notification();
    Glib::ustring app_icon_image();
    Glib::ustring parse_image_data(const Glib::VariantBase &var);
};

// https://specifications.freedesktop.org/notification-spec/latest/
// https://dbus.freedesktop.org/doc/dbus-tutorial.html#concepts
class NotificationsServer : public Glib::Object {
public:
    static NotificationsServer &get_instance();

    // Avoid copy creation
    NotificationsServer(const NotificationsServer &) = delete;
    void operator=(const NotificationsServer &) = delete;

#if 0
    // For external: Utils::notify(). It now uses NotificationsClient
    void add_notification(const Glib::ustring &app_name, const Glib::ustring &app_icon,
                          const Glib::ustring &summary, const Glib::ustring &body);
#endif

    void close_notification(uint32_t id, NotificationCloseReason reason = CLOSE_DISMISSED);
    void dismiss_notification(uint32_t id);
    void invoke_notification(uint32_t id, const Glib::ustring &action_id);
    void clear_notifications();

    // there is a notifications server running, either us or someone else
    Glib::Property<bool> server_available;

    Glib::Property<bool> notifications_changed;
    Glib::Property<bool> notifications_popup;
    std::unordered_map<int, Notification *> notifications;

private:
    NotificationsServer();
    virtual ~NotificationsServer();

    void on_server_method_call(const Glib::RefPtr<Gio::DBus::Connection>& /* connection */,
        const Glib::ustring& /* sender */, const Glib::ustring& /* object_path */,
        const Glib::ustring& /* interface_name */, const Glib::ustring& method_name,
        const Glib::VariantContainerBase& parameters,
        const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation);

    void notify_method(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::VariantContainerBase& parameters, const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation);
    void close_notification_method(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::VariantContainerBase &parameters, const Glib::RefPtr<Gio::DBus::MethodInvocation> &invocation);

    void save_notifications() const;

    void disconnect_expiration_timer(uint32_t id);
    void send_closed_notification_signal(uint32_t id, NotificationCloseReason reason);

    const std::string DBUS_NAME = "org.freedesktop.Notifications";
    Glib::RefPtr<Gio::DBus::Proxy> proxy;
    Glib::RefPtr<Gio::DBus::ObjectSkeleton> dbus;

    guint dbus_name_id;
    guint dbus_registered_id;
    uint32_t id_count;

    std::unordered_map<int, sigc::connection> expiration_timers;
};

class NotificationsClient {
public:
    static NotificationsClient &get_instance();
    // Avoid copy creation
    NotificationsClient(const NotificationsServer &) = delete;
    void operator=(const NotificationsServer &) = delete;

    // For external: Utils::notify()
    void add_notification(const Glib::ustring &app_name, const Glib::ustring &app_icon,
                          const Glib::ustring &summary, const Glib::ustring &body);

private:
    NotificationsClient();
    virtual ~NotificationsClient();

    void send_notifications();
    void dispatch_notification(const Glib::ustring &app_name, const Glib::ustring &app_icon,
                               const Glib::ustring &summary, const Glib::ustring &body);

    Glib::RefPtr<Gio::DBus::Proxy> proxy;
    const std::string DBUS_NAME = "org.freedesktop.Notifications";

    typedef struct {
        Glib::ustring app_name;
        Glib::ustring app_icon;
        Glib::ustring summary;
        Glib::ustring body;
    } SimpleNotification;

    SimpleNotification notification;
    std::binary_semaphore sem_pro, sem_con;
    std::shared_ptr<std::thread> worker;
};

#endif // __GTKSHELL_NOTIFICATIONS__
