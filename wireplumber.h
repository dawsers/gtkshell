#ifndef __GTKSHELL_WIREPLUMBER__
#define __GTKSHELL_WIREPLUMBER__

#include <glibmm.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/eventcontrollerscroll.h>
#include <gtkmm/label.h>
#include <wp/wp.h>

class AudioDevice {
public:
    typedef enum {
        SPEAKER = 0,
        MICROPHONE
    } Type;

    typedef enum {
        LINEAR,
        CUBIC
    } Scale;

    AudioDevice(AudioDevice::Type device) : type(device), name(nullptr), id(-1),
        muted(false), volume(0.0), min_step(0.0), scale (Scale::LINEAR) {}

private:
    friend class Wireplumber;

    void set_volume(double volume, Scale scale) {
        this->volume = volume;
        this->scale = scale;
    }
    double get_volume(Scale scale) {
        if (this->scale == scale)
            return volume;
        switch (scale) {
        case Scale::LINEAR:
            return std::pow(volume, 1.0 / 3.0);
        case Scale::CUBIC:
            return std::pow(volume, 3.0);
        default:
            return 0.0;
        }
    }

    Type type;
    const gchar *name;
    uint32_t id;
    bool muted;
    double volume; // in linear scale
    double min_step;
    AudioDevice::Scale scale;
};


class Wireplumber : public Glib::Object {
public:
    static Wireplumber &get_instance();

    //Avoid copy creation
    Wireplumber(const Wireplumber &) = delete;
    void operator=(const Wireplumber &) = delete;

    sigc::signal<void()> speaker_changed;
    sigc::signal<void()> microphone_changed;
    sigc::signal<void()> speaker_volume_changed;
    sigc::signal<void()> microphone_volume_changed;

    AudioDevice *default_speaker = nullptr;
    AudioDevice *default_microphone = nullptr;

    Glib::ustring get_name(AudioDevice *device) const;
    // Volume between 0 and 10, pre cube
    bool set_volume(AudioDevice *device, double volume, AudioDevice::Scale scale = AudioDevice::Scale::LINEAR);
    double get_volume(AudioDevice *device, AudioDevice::Scale scale = AudioDevice::Scale::LINEAR) const;
    bool set_mute(AudioDevice *device, bool mute);
    bool get_mute(AudioDevice *device) const;
    bool inc_volume(AudioDevice *device, int steps);

private:
    Wireplumber();
    virtual ~Wireplumber();

    static void obj_mananger_installed(Wireplumber *self);
    static void plugin_loaded(WpObject *obj, GAsyncResult *result, Wireplumber *self);
    static void plugin_activated(WpObject *obj, GAsyncResult *result, Wireplumber *self);
    static void default_changed(Wireplumber *self);
    static void mixer_changed(Wireplumber *self);
    static void update_volume(Wireplumber *self, AudioDevice *device);

    WpCore *core;
    WpObjectManager *obj_manager;

    WpPlugin *defaults;
    WpPlugin *mixer;
    int pending_plugins;
};

class SpeakerIndicator : public Gtk::Box {
public:
    SpeakerIndicator();

private:
    Gtk::Button button;
    Gtk::Image icon;
    Gtk::Label label;
    Glib::RefPtr<Gtk::GestureClick> click;
    Glib::RefPtr<Gtk::EventControllerScroll> scroll;
};

class MicrophoneIndicator : public Gtk::Box {
public:
    MicrophoneIndicator();

private:
    Gtk::Button button;
    Gtk::Image icon;
    Gtk::Label label;
    Glib::RefPtr<Gtk::GestureClick> click;
    Glib::RefPtr<Gtk::EventControllerScroll> scroll;
};


#endif // __GTKSHELL_WIREPLUMBER__
