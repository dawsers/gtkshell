#ifndef __GTKSHELL_MPRIS__
#define __GTKSHELL_MPRIS__

#include <giomm.h>
#include <glibmm/object.h>
#include <glibmm/property.h>

#include <unordered_map>
#include <any>

class PlaybackStatus {
public:
    enum Enum {
        PLAYING,
        PAUSED,
        STOPPED
    };

    constexpr PlaybackStatus() = default;
    /* implicit */ constexpr PlaybackStatus(Enum e) : e(e) {}
    constexpr PlaybackStatus(const Glib::ustring &str) { e = from_string(str); }

    // Allows comparisons with Enum constants.
    constexpr operator Enum() const { return e; }

    // Needed to prevent if(c)
    explicit operator bool() const = delete;

    std::string_view to_string() {
        switch (e) {
            case PLAYING: return "Playing";
            case PAUSED: return "Paused";
            case STOPPED: return "Stopped";
        }
    }
    PlaybackStatus from_string(const Glib::ustring &str) {
        if (str == "Playing")
            return PlaybackStatus::PLAYING;
        else if (str == "Paused")
            return PlaybackStatus::PAUSED;
        else
            return PlaybackStatus::STOPPED;
    }

private:
    Enum e;
};

class Loop {
public:
    enum Enum {
        UNSUPPORTED,
        NONE,
        TRACK,
        PLAYLIST
    };

    constexpr Loop() = default;
    /* implicit */ constexpr Loop(Enum e) : e(e) {}
    constexpr Loop(const Glib::ustring &str) { e = from_string(str); }

    // Allows comparisons with Enum constants.
    constexpr operator Enum() const { return e; }

    // Needed to prevent if(c)
    explicit operator bool() const = delete;

    std::string_view to_string() {
        switch (e) {
            case NONE: return "None";
            case TRACK: return "Track";
            case PLAYLIST: return "Playlist";
            default: return "Unsupported";
        }
    }
    Loop from_string(const Glib::ustring &str) {
        if (str == "None")
            return Loop::NONE;
        else if (str == "Track")
            return Loop::TRACK;
        else if (str == "Playlist")
            return Loop::PLAYLIST;
        else
            return Loop::UNSUPPORTED;
    }

private:
    Enum e;
};

class Shuffle {
public:
    enum Enum {
        UNSUPPORTED,
        ON,
        OFF
    };

    constexpr Shuffle() = default;
    /* implicit */ constexpr Shuffle(Enum e) : e(e) {}
    constexpr Shuffle(bool on) : e(on ? ON : OFF) {}

    // Allows comparisons with Enum constants.
    constexpr operator Enum() const { return e; }

    // Needed to prevent if(c)
    explicit operator bool() const = delete;

    std::string_view to_string() {
        switch (e) {
            case OFF: return "Off";
            case ON: return "On";
            default: return "Unsupported";
        }
    }

private:
    Enum e;
};


class Player : public Glib::Object {
public:
    Player(const Glib::ustring &busname);

    Glib::ustring bus_name;
    Glib::ustring name;
    bool available;

    sigc::signal<void ()> appeared_signal;
    sigc::signal<void ()> closed_signal; 
    sigc::signal<void ()> changed_signal; 

    void raise() {
        if (can_raise)
            mpris_proxy->call_sync("Raise");
    }
    void quit() {
        if (can_quit)
            mpris_proxy->call_sync("Quit");
    }

    bool can_raise;
    bool can_quit;
    bool has_track_list;
    Glib::ustring identity;

    void next() {
        if (can_go_next)
            player_proxy->call_sync("Next");
    }
    void previous() {
        if (can_go_previous)
            player_proxy->call_sync("Previous");
    }
    void pause() {
        if (can_pause)
            player_proxy->call_sync("Pause");
    }
    void play_pause() {
        if (can_control)
            player_proxy->call_sync("PlayPause");
    }
    void stop() {
        if (can_control)
            player_proxy->call_sync("Stop");
    }
    void play() {
        if (can_control)
            player_proxy->call_sync("Play");
    }
    void loop() {
        if (Loop::UNSUPPORTED)
            return;
        switch (loop_status) {
        case Loop::NONE:
            loop_status = Loop::TRACK;
            break;
        case Loop::TRACK:
            loop_status = Loop::PLAYLIST;
            break;
        case Loop::PLAYLIST:
            loop_status = Loop::NONE;
            break;
        default:
            break;
        }
    }
    void shuffle() {
        if(Shuffle::UNSUPPORTED)
            return;
        shuffle_status = shuffle_status == Shuffle::ON ? Shuffle::OFF : Shuffle::ON;
    }

    Loop get_loop_status() const {
        return loop_status;
    }
    void set_loop_status(Loop status) {
        loop_status = status;
    }

    Shuffle get_shuffle_status() const {
        return shuffle_status;
    }

    void set_shuffle_status(Shuffle status) {
        shuffle_status = status;
    }

    double get_rate() const { return rate; }
    void set_rate(double rate) {
        this->rate = rate;
    }
    double get_volume() const { return volume; }
    void set_volume(double vol) {
        volume = vol;
    }

    double get_position() {
        return static_cast<double>(get_proxy_property<int64_t>(player_proxy, "Position")) / 1000000;
    }
    void set_position(double pos) {
        auto params = Glib::Variant<std::tuple<Glib::ustring, int64_t>>::create({ trackid, static_cast<int64_t>(pos * 1000000) });
        player_proxy->call_sync("SetPosition", params);
    }

    PlaybackStatus playback_status;
    double minimum_rate;
    double maximum_rate;
    bool can_go_next;
    bool can_go_previous;
    bool can_play;
    bool can_pause;
    bool can_seek;
    bool can_control;
    std::map<std::string, Glib::VariantBase> metadata;
    Glib::ustring trackid;
    double length;
    Glib::ustring art_url;  // url of the cover art. Use cover_art if available
    Glib::ustring album;
    Glib::ustring album_artist;  // artist of the current album
    Glib::ustring artist;  // artist of the current track
    Glib::ustring lyrics;
    Glib::ustring title;
    Glib::ustring composer;
    Glib::ustring comments;
    Glib::ustring cover_art;  // path of the cached art_url

    std::any get_meta(const Glib::ustring &key) {
        auto iter = metadata.find(key);
        if (iter != metadata.end())
            return iter->second;
        return nullptr;
    }

private:
    template<typename T>
    T get_proxy_property(Glib::RefPtr<Gio::DBus::Proxy> &proxy, const Glib::ustring &name) {
        Glib::Variant<T> prop;
        proxy->get_cached_property(prop, name);
        return prop.gobj() ? prop.get() : T();
    }
    Glib::ustring get_str(const Glib::ustring &key) {
        auto iter = metadata.find(key);
        if (iter == metadata.end())
            return "";

        return iter->second.get_dynamic<Glib::ustring>();
    }
    Glib::ustring join_strv(const Glib::ustring &key, const Glib::ustring &sep) {
        auto iter = metadata.find(key);
        if (iter == metadata.end())
            return "";

        Glib::ustring string;
        auto vector = iter->second.get_dynamic<std::vector<Glib::ustring>>();
        for (ssize_t i = 0; i < vector.size() - 1; ++i) {
            string += vector[i] + sep;
        }
        if (vector.size() > 0) {
            string += vector[vector.size() - 1];
        }
        return string;
    }

    Loop loop_status = Loop::UNSUPPORTED;
    Shuffle shuffle_status = Shuffle::UNSUPPORTED;
    double rate;
    double volume = -1;

    Glib::RefPtr<Gio::DBus::Proxy> mpris_proxy;
    Glib::RefPtr<Gio::DBus::Proxy> player_proxy;
    void sync_property(const Glib::ustring &name, const Glib::VariantBase &value);
    void sync(const Gio::DBus::Proxy::MapChangedProperties &changed, const std::vector<Glib::ustring> &invalidated);
    void sync_all();
};

class Mpris : public Glib::Object {
public:
    static Mpris &get_instance();

    // Avoid copy creation
    Mpris(const Mpris &) = delete;
    void operator=(const Mpris &) = delete;

    sigc::signal<void (Player *)> player_added_signal;
    sigc::signal<void (Player *)> player_closed_signal;
    sigc::signal<void (const Glib::ustring &bus)> player_changed_signal;

    std::unordered_map<std::string, Player *> players;

private:
    Mpris();
    virtual ~Mpris();

    void add_player(const Glib::ustring &busname);
    void delete_player(const Glib::ustring &busname);

    const std::string PREFIX = "org.mpris.MediaPlayer2.";
    Glib::RefPtr<Gio::DBus::Proxy> proxy;
};


#endif // __GTKSHELL_MPRIS__ 

