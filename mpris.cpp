#include "dbus.h"
#include "mpris.h"
#include "utils.h"

Mpris &Mpris::get_instance()
{
    // After C++11 this is thread safe. No two threads are allowed to enter a
    // variable declaration's initialization concurrently.
    static Mpris instance;

    return instance;
}

Mpris::Mpris()
{
    try {
        auto connection = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SESSION);
        proxy = Gio::DBus::Proxy::create_sync(connection, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus");

        // The proxy's call method returns a tuple of the value(s) that the method
        // call produces so just get the tuple as a VariantContainerBase.
        const auto call_result = proxy->call_sync("ListNames");

        // Now extract the single item in the variant container which is the
        // array of strings (the names).
        Glib::Variant<std::vector<Glib::ustring>> names_variant;
        call_result.get_child(names_variant);

        // Get the vector of strings.
        auto names = names_variant.get();

        for (const auto &busname : names) {
            if (busname.find(PREFIX) == 0) {
                add_player(busname);
            }
        }
        proxy->signal_signal("NameOwnerChanged").connect([this](const Glib::ustring &, const Glib::ustring &, const Glib::VariantContainerBase &params) {
            const Glib::ustring &name = params.get_child(0).get_dynamic<Glib::ustring>();
            const Glib::ustring &old_owner = params.get_child(1).get_dynamic<Glib::ustring>();
            const Glib::ustring &new_owner = params.get_child(2).get_dynamic<Glib::ustring>();
            if (name.find(PREFIX) != 0)
                return;
            if (new_owner != "" && old_owner == "") {
                add_player(name);
            } else if (old_owner != "" && new_owner == "") {
                delete_player(name);
            }
        });
    } catch (const Gio::Error &error) {
        Utils::log(Utils::LogSeverity::ERROR, std::format("mpris: DBus: proxy to user session bus not available: {}", error.what()));     
    }
}

Mpris::~Mpris()
{
    for (auto player : players) {
        delete player.second;
    }
}

void Mpris::add_player(const Glib::ustring &busname)
{
    if (busname == "org.mpris.MediaPlayer2.playerctld")
        return;

    Player *p = new Player(busname);
    players[busname] = p;
    p->closed_signal.connect(
        [this, p, busname]() {
            players.erase(busname);
            player_closed_signal(p);
            delete p;
        });
    p->changed_signal.connect(
        [this, busname]() {
            player_changed_signal(busname);
        });
    player_added_signal(p);
}

void Mpris::delete_player(const Glib::ustring &busname)
{
    auto iter = players.find(busname);
    if (iter != players.end()) {
        iter->second->closed_signal();
    }
}

Player::Player(const Glib::ustring &busname)
{
    bus_name = busname.find("org.mpris.MediaPlayer2.") == 0 ?
        busname : "org.mpris.MediaPlayer2." + busname;

    name = Utils::split(bus_name, ".")[3];
    Glib::RefPtr<Gio::DBus::InterfaceInfo> mpris_interface_info = Gio::DBus::NodeInfo::create_for_xml(DBUS_MPRIS_MEDIAPLAYER2)->lookup_interface("org.mpris.MediaPlayer2");
    mpris_proxy = Gio::DBus::Proxy::create_for_bus_sync(Gio::DBus::BusType::SESSION, bus_name, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2", mpris_interface_info);
    Glib::RefPtr<Gio::DBus::InterfaceInfo> player_interface_info = Gio::DBus::NodeInfo::create_for_xml(DBUS_MPRIS_MEDIAPLAYER2_PLAYER)->lookup_interface("org.mpris.MediaPlayer2.Player");
    player_proxy = Gio::DBus::Proxy::create_for_bus_sync(Gio::DBus::BusType::SESSION, bus_name, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", player_interface_info);
    player_proxy->signal_properties_changed().connect([this] (const Gio::DBus::Proxy::MapChangedProperties &changed, const std::vector<Glib::ustring> &invalidated) { sync(changed, invalidated); });
    appeared_signal();

    sync_all();
}

void Player::sync_property(const Glib::ustring &name, const Glib::VariantBase &value)
{
    if (name == "CanQuit")
        can_quit = value.get_dynamic<bool>();
    else if (name == "CanRaise")
        can_raise = value.get_dynamic<bool>();
    else if (name == "HasTrackList")
        has_track_list = value.get_dynamic<bool>();
    else if (name == "Identity")
        identity = value.get_dynamic<Glib::ustring>();
    else if (name == "LoopStatus")
        loop_status = Loop(value.get_dynamic<Glib::ustring>());
    else if (name == "Shuffle")
        shuffle_status = value.get_dynamic<bool>();
    else if (name == "PlaybackStatus")
        playback_status = PlaybackStatus(value.get_dynamic<Glib::ustring>());
    else if (name == "MinimumRate")
        minimum_rate = value.get_dynamic<double>();
    else if (name == "MaximumRate")
        maximum_rate = value.get_dynamic<double>();
    else if (name == "CanGoNext")
        can_go_next = value.get_dynamic<bool>();
    else if (name == "CanGoPrevious")
        can_go_previous = value.get_dynamic<bool>();
    else if (name == "CanPlay")
        can_play = value.get_dynamic<bool>();
    else if (name == "CanPause")
        can_pause = value.get_dynamic<bool>();
    else if (name == "CanSeek")
        can_seek = value.get_dynamic<bool>();
    else if (name == "CanControl")
        can_control = value.get_dynamic<bool>();
    else if (name == "Metadata") {
        metadata = value.get_dynamic<std::map<std::string, Glib::VariantBase>>();
        if (metadata.size() > 0) {
            auto iter = metadata.find("mpris:length");
            if (iter != metadata.end()) {
                auto t = iter->second.get_type().get_string();
                if (t == "x" || t == "t") {
                    length = static_cast<double>(iter->second.get_dynamic<int64_t>()) / 1000000;
                } else  {
                    length = -1;
                }
            }
            trackid = get_str("mpris:trackid");
            art_url = get_str("mpris:artUrl");
            album = get_str("xesam:album");
            lyrics = get_str("xesam:asText");
            title = get_str("xesam:title");
            album_artist = join_strv("xesam:albumArtist", ", ");
            artist = join_strv("xesam:artist", ", ");
            comments = join_strv("xesam:comments", "\n");
            composer = join_strv("xesam:composer", ", ");
        }
    } else if (name == "Rate")
        rate = value.get_dynamic<double>();
    else if (name == "Volume")
        volume = value.get_dynamic<double>();
}

void Player::sync(const Gio::DBus::Proxy::MapChangedProperties &changed, const std::vector<Glib::ustring> &invalidated)
{
    for (auto property : changed) {
        sync_property(property.first, property.second);
    }
    if (changed.size() > 0) {
        changed_signal();
        return;
    }
}

void Player::sync_all()
{
    can_quit = get_proxy_property<bool>(mpris_proxy, "CanQuit");
    can_raise = get_proxy_property<bool>(mpris_proxy, "CanRaise");
    has_track_list = get_proxy_property<bool>(mpris_proxy, "HasTrackList");
    identity = get_proxy_property<Glib::ustring>(mpris_proxy, "Identity");

    loop_status = Loop(get_proxy_property<Glib::ustring>(player_proxy, "LoopStatus"));
    shuffle_status = get_proxy_property<bool>(player_proxy, "Shuffle");
    playback_status = PlaybackStatus(get_proxy_property<Glib::ustring>(player_proxy, "PlaybackStatus"));
    minimum_rate = get_proxy_property<double>(player_proxy, "MinimumRate");
    maximum_rate = get_proxy_property<double>(player_proxy, "MaximumRate");
    can_go_next = get_proxy_property<bool>(player_proxy, "CanGoNext");
    can_go_previous = get_proxy_property<bool>(player_proxy, "CanGoPrevious");
    can_play = get_proxy_property<bool>(player_proxy, "CanPlay");
    can_pause = get_proxy_property<bool>(player_proxy, "CanPause");
    can_seek = get_proxy_property<bool>(player_proxy, "CanSeek");
    can_control = get_proxy_property<bool>(player_proxy, "CanControl");

    metadata = get_proxy_property<std::map<std::string, Glib::VariantBase>>(player_proxy, "Metadata");
    if (metadata.size() > 0) {
        auto iter = metadata.find("mpris:length");
        if (iter != metadata.end()) {
            auto t = iter->second.get_type().get_string();
            if (t == "x" || t == "t") {
                length = static_cast<double>(iter->second.get_dynamic<int64_t>()) / 1000000;
            } else  {
                length = -1;
            }
        }
        trackid = get_str("mpris:trackid");
        art_url = get_str("mpris:artUrl");
        album = get_str("xesam:album");
        lyrics = get_str("xesam:asText");
        title = get_str("xesam:title");
        album_artist = join_strv("xesam:albumArtist", ", ");
        artist = join_strv("xesam:artist", ", ");
        comments = join_strv("xesam:comments", "\n");
        composer = join_strv("xesam:composer", ", ");
    }
    rate = get_proxy_property<double>(player_proxy, "Rate");
    volume = get_proxy_property<double>(player_proxy, "Volume");
    changed_signal();
}
