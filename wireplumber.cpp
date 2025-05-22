#include <gtkmm/icontheme.h>

#include "wireplumber.h"

#include "utils.h"

#include <wp/wp.h>


Wireplumber::Wireplumber() :
    Glib::ObjectBase(typeid(Wireplumber))
{
    wp_init(WP_INIT_ALL);
    core = wp_core_new(nullptr, nullptr, nullptr);
    if (!wp_core_connect(core)) {
        Utils::log(Utils::LogSeverity::ERROR, "Wireplumber: Cannot connect to PipeWire");
        return;
    }
    obj_manager = wp_object_manager_new();
    wp_object_manager_add_interest(obj_manager, WP_TYPE_NODE, WP_CONSTRAINT_TYPE_PW_PROPERTY, "media.class", "=s", "Audio/Sink", nullptr);
    wp_object_manager_add_interest(obj_manager, WP_TYPE_NODE, WP_CONSTRAINT_TYPE_PW_PROPERTY, "media.class", "=s", "Audio/Source", nullptr);
    g_signal_connect_swapped(obj_manager, "installed", G_CALLBACK (obj_mananger_installed), this);

    default_speaker = new AudioDevice(AudioDevice::SPEAKER);
    default_microphone = new AudioDevice(AudioDevice::MICROPHONE);

    pending_plugins = 2;
    wp_core_load_component(core, "libwireplumber-module-default-nodes-api", "module", nullptr,
                           "default-nodes-api", nullptr, (GAsyncReadyCallback)plugin_loaded, this);
    wp_core_load_component(core, "libwireplumber-module-mixer-api", "module", nullptr,
                           "mixer-api", nullptr, (GAsyncReadyCallback)plugin_loaded, this);
}

Wireplumber::~Wireplumber() {}

Wireplumber &Wireplumber::get_instance()
{
    static Wireplumber instance;

    return instance;
}

void Wireplumber::plugin_activated(WpObject *obj, GAsyncResult *result, Wireplumber *self)
{
    GError *error = NULL;
    wp_object_activate_finish(obj, result, &error);
    if (error) {
        Utils::log(Utils::LogSeverity::ERROR, std::format("Wireplumber: failed to activate component {}", error->message));
        return;
    }

    if (--self->pending_plugins == 0) {
        self->defaults = wp_plugin_find(self->core, "default-nodes-api");
        self->mixer = wp_plugin_find(self->core, "mixer-api");
        //g_object_set(self->mixer, "scale", self->scale, NULL);
        wp_core_install_object_manager(self->core, self->obj_manager);
    }
}

void Wireplumber::plugin_loaded(WpObject *obj, GAsyncResult *result, Wireplumber *self) {
    GError *error = NULL;
    wp_core_load_component_finish(self->core, result, &error);
    if (error) {
        Utils::log(Utils::LogSeverity::ERROR, std::format("Wireplumber: failed to load component {}", error->message));
        return;
    }
    wp_object_activate(obj, WP_PLUGIN_FEATURE_ENABLED, nullptr, (GAsyncReadyCallback)plugin_activated, self);
}

// https://docs.pipewire.org/
// https://pipewire.pages.freedesktop.org/wireplumber/library/c_api.html
// https://docs.pipewire.org/page_man_pipewire-props_7.html//
void Wireplumber::obj_mananger_installed(Wireplumber *self)
{
    g_signal_connect_swapped(self->defaults, "changed", G_CALLBACK(default_changed), self);
    default_changed(self);
    g_signal_connect_swapped(self->mixer, "changed", G_CALLBACK(mixer_changed), self);
    mixer_changed(self);
}

void Wireplumber::default_changed(Wireplumber *self)
{
    // Get ids for default speaker and microphone
    uint32_t speaker_id, microphone_id;
    g_signal_emit_by_name(self->defaults, "get-default-node", "Audio/Sink", &speaker_id);
    g_signal_emit_by_name(self->defaults, "get-default-node", "Audio/Source", &microphone_id);
    g_autoptr (WpIterator) iter = wp_object_manager_new_iterator(self->obj_manager);
    GValue value = G_VALUE_INIT;
    while (wp_iterator_next(iter, &value)) {
        auto *object = g_value_get_object(&value);
        if (WP_IS_NODE(object)) {
            WpNode *node = WP_NODE(object);
            WpPipewireObject *pobject = WP_PIPEWIRE_OBJECT(node);
            const gchar *dev = wp_pipewire_object_get_property(WP_PIPEWIRE_OBJECT(node), "object.id");
            if (dev != NULL) {
                // There is a bug in the defaults plugin that returns the same id for the default Audio/Sink and Audio/Source
                // because their device is the same, despite their node being different
                int32_t device_id = std::atoi(dev);
                if (device_id == speaker_id && speaker_id != self->default_speaker->id) {
                    self->default_speaker->id = speaker_id;
                    self->default_speaker->name = wp_pipewire_object_get_property(pobject, "node.description");
                    update_volume(self, self->default_speaker);
                    self->speaker_changed();
                }
                if (device_id == microphone_id && microphone_id != self->default_microphone->id) {
                    self->default_microphone->id = microphone_id;
                    self->default_microphone->name = wp_pipewire_object_get_property(pobject, "node.description");
                    update_volume(self, self->default_microphone);
                    self->microphone_changed();
                }
            }
        }
        g_value_unset(&value);
    }
}

void Wireplumber::mixer_changed(Wireplumber *self)
{
    update_volume(self, self->default_speaker);
    update_volume(self, self->default_microphone);
}

void Wireplumber::update_volume(Wireplumber *self, AudioDevice *device)
{
    GVariant *variant = nullptr;

    g_signal_emit_by_name(self->mixer, "get-volume", device->id, &variant);

    if (variant == nullptr) {
        Utils::log(Utils::LogSeverity::ERROR, std::format("Wireplumber: node {} does not support volume", device->name));
        return;
    }
    double vol;
    g_variant_lookup(variant, "volume", "d", &vol);
    device->set_volume(vol, AudioDevice::Scale::CUBIC);
    g_variant_lookup(variant, "step", "d", &device->min_step);
    g_variant_lookup(variant, "mute", "b", &device->muted);
    GVariantIter *channels = nullptr;
    g_variant_lookup(variant, "channelVolumes", "s{sv}", &channels);
    if (channels != nullptr) {
        const gchar *key;
        gdouble channel_volume;
        GVariant *volume;

        while (g_variant_iter_loop(channels, "{&sv}", &key, &volume)) {
            g_variant_lookup(volume, "volume", "d", &channel_volume);
            if (channel_volume > device->volume)
                device->set_volume(channel_volume, AudioDevice::Scale::CUBIC);
        }
    }
    g_clear_pointer(&variant, g_variant_unref);
    if (device->type == AudioDevice::SPEAKER)
        self->speaker_volume_changed();
    else if (device->type == AudioDevice::MICROPHONE)
        self->microphone_volume_changed();
}

bool Wireplumber::set_volume(AudioDevice *device, double volume, AudioDevice::Scale scale)
{
    if (volume < 0.0) volume = 0.0;

    bool ret = false;
    if (volume != device->volume) {
        GVariantBuilder variant = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
        GVariant *volume_variant = g_variant_new_double(volume);
        g_variant_builder_add(&variant, "{sv}", "volume", volume_variant);
        g_signal_emit_by_name(mixer, "set-volume", device->id, g_variant_builder_end(&variant), &ret);
        update_volume(this, device);
    }
    return ret;
}
    
double Wireplumber::get_volume(AudioDevice *device, AudioDevice::Scale scale) const
{
    return device->get_volume(scale);
}

bool Wireplumber::inc_volume(AudioDevice *device, int steps)
{
    double volume = device->get_volume(AudioDevice::Scale::CUBIC);
    volume += steps * device->min_step;
    return set_volume(device, volume);
}

bool Wireplumber::set_mute(AudioDevice *device, bool mute)
{
    bool ret = false;
    if (mute != device->muted) {
        GVariantBuilder variant = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
        GVariant *mute_variant = g_variant_new_boolean(mute);
        g_variant_builder_add(&variant, "{sv}", "mute", mute_variant);
        g_signal_emit_by_name(mixer, "set-volume", device->id, g_variant_builder_end(&variant), &ret);
        update_volume(this, device);
    }
    return ret;
}

bool Wireplumber::get_mute(AudioDevice *device) const
{
    return device->muted;
}
    
Glib::ustring Wireplumber::get_name(AudioDevice *device) const
{
    return device->name;
}

SpeakerIndicator::SpeakerIndicator()
{
    auto &wireplumber = Wireplumber::get_instance();

    wireplumber.speaker_volume_changed.connect([this, &wireplumber]() {
        Glib::ustring name;
        int volume = wireplumber.get_volume(wireplumber.default_speaker) * 100.0;
        if (wireplumber.get_mute(wireplumber.default_speaker)) {
            name = "muted";
        } else {
            if (volume >= 101)
                name = "overamplified";
            else if (volume >= 67)
                name = "high";
            else if (volume >= 34)
                name = "medium";
            else if (volume >= 1)
                name = "low";
            else
                name = "muted";
        }

        icon.set_from_icon_name("audio-volume-" + name + "-symbolic");
        set_tooltip_text(wireplumber.get_name(wireplumber.default_speaker));
        label.set_text(std::format("{}%", std::round(volume)));
    });
    button.set_child(icon);
    append(button);
    append(label);
    add_css_class("speaker");

    // Scroll for volume up/down
    scroll = Gtk::EventControllerScroll::create();
    scroll->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    scroll->signal_scroll().connect([this, &wireplumber] (double dx, double dy) -> bool {
        wireplumber.inc_volume(wireplumber.default_speaker, -dy * 100); // 100 is just a multiplier, nothing to do with %
        return true;
    }, true);
    add_controller(scroll);

    // Left click to mute, right click for pavucontrol
    click = Gtk::GestureClick::create();
    click->set_button(0); // 0 = all, 1 = left, 2 = center, 3 = right
    click->signal_pressed().connect([this, &wireplumber] (int n_press, double x, double y) {
        auto mbutton = this->click->get_current_button();
        if (mbutton == GDK_BUTTON_SECONDARY) {
            Utils::spawn("pavucontrol");
        } else if (mbutton == GDK_BUTTON_PRIMARY) {
            wireplumber.set_mute(wireplumber.default_speaker, !wireplumber.get_mute(wireplumber.default_speaker));
        }
    }, true);
    add_controller(click);
}

MicrophoneIndicator::MicrophoneIndicator()
{
    auto &wireplumber = Wireplumber::get_instance();

    wireplumber.microphone_volume_changed.connect([this, &wireplumber]() {
        Glib::ustring name;
        int volume = wireplumber.get_volume(wireplumber.default_microphone) * 100.0;
        if (wireplumber.get_mute(wireplumber.default_microphone)) {
            name = "muted";
        } else {
            if (volume >= 101)
                name = "overamplified";
            else if (volume >= 67)
                name = "high";
            else if (volume >= 34)
                name = "medium";
            else if (volume >= 1)
                name = "low";
            else
                name = "muted";
        }

        // auto image = Utils::lookup_icon("audio-volume-" + name + "-symbolic");
        icon.set_from_icon_name("audio-volume-" + name + "-symbolic");
        set_tooltip_text(wireplumber.get_name(wireplumber.default_microphone));
        label.set_text(std::format("{}%", std::round(volume)));
    });
    button.set_child(icon);
    append(button);
    append(label);
    add_css_class("microphone");

    scroll = Gtk::EventControllerScroll::create();
    scroll->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    scroll->signal_scroll().connect([this, &wireplumber] (double dx, double dy) -> bool {
        wireplumber.inc_volume(wireplumber.default_microphone, -dy * 100); // 100 is just a multiplier, nothing to do with %
        return true;
    }, true);
    add_controller(scroll);

    // Left click to mute, right click for pavucontrol
    click = Gtk::GestureClick::create();
    click->set_button(0); // 0 = all, 1 = left, 2 = center, 3 = right
    click->signal_pressed().connect([this, &wireplumber] (int n_press, double x, double y) {
        auto mbutton = this->click->get_current_button();
        if (mbutton == GDK_BUTTON_SECONDARY) {
            Utils::spawn("pavucontrol");
        } else if (mbutton == GDK_BUTTON_PRIMARY) {
            wireplumber.set_mute(wireplumber.default_microphone, !wireplumber.get_mute(wireplumber.default_microphone));
        }
    }, true);
    add_controller(click);
}
