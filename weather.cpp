#include "weather.h"

#include "utils.h"
#include "datetime.h"

#include <gtkmm/button.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/notebook.h>
#include <gtkmm/levelbar.h>
#include <gtkmm/label.h>
#include <gtkmm/image.h>

#include <algorithm>

#include <nlohmann/json.hpp>
using json = nlohmann::json;


static const Glib::ustring label_format = "${WMO_WIC[weather_code].symbol} ${temperature_2m}° ${WMO_WIC[weather_code].description}";
//const base_url = "https://api.open-meteo.com/v1/forecast?";
static const Glib::ustring om_base_url = "https://api.open-meteo.com/v1/dwd-icon?";

typedef struct {
    std::string latitude;
    std::string longitude;
    std::string timezone;
    int days;  // 1, 3, 7, 14, 16
    std::vector<std::string> vars_current;
    std::vector<std::string> vars_hourly;
    int prediction;  // interval (current + prediction) to choose for prediction label
} OMInput;

static const OMInput om_input = {
    "16.782230", // Timbuktu
    "-3.005061",
    "Europe%2FBerlin",
    7,
    { "temperature_2m", "precipitation", "weather_code" },
    // { "temperature_2m", "relative_humidity_2m", "precipitation_probability", "precipitation", "weather_code", "pressure_msl", "cloud_cover", "wind_speed_10m", "wind_direction_10m" },
    { "temperature_2m", "precipitation_probability", "precipitation", "weather_code", "cloud_cover_low", "wind_speed_10m", "wind_direction_10m" },
    1
};

static std::string degrees_to_direction(double degrees)
{
    const int val = std::floor((degrees + 5.625) / 11.25);
    const std::vector<std::string> arr = {
        "n",
        "nbe",
        "nne",
        "nebn",
        "ne",
        "nebe",
        "ene",
        "ebn",
        "e",
        "ebs",
        "ese",
        "sebe",
        "se",
        "sebs",
        "sse",
        "sbe",
        "s",
        "sbw",
        "ssw",
        "swbs",
        "sw",
        "swbw",
        "wsw",
        "wbs",
        "w",
        "wbn",
        "wnw",
        "nwbw",
        "nw",
        "nwbn",
        "nnw",
        "nbw"
    };
    return arr[(val % 32)];
}

static int str_to_int(const std::string &str) {
    int v = 0;
    try {
        v = std::stoi(str);
    } catch (std::invalid_argument const& ex) {
        Utils::log(Utils::LogSeverity::INFO, std::format("std::invalid_argument::what(): {}", ex.what()));
    } catch (std::out_of_range const& ex) {
        Utils::log(Utils::LogSeverity::INFO, std::format("std::out_of_range::what(): {}", ex.what()));
    }
    return v;
}

static std::string get_wind_direction(double value)
{
    return degrees_to_direction(value);
}

static Glib::RefPtr<Gtk::Box> Precipitation(const std::string &precip)
{
    auto box = Glib::RefPtr<Gtk::Box>(Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL));

    auto level_bar = Gtk::make_managed<Gtk::LevelBar>();
    level_bar->add_css_class("weather-precipitation-bar");
    level_bar->set_orientation(Gtk::Orientation::VERTICAL);
    // height_request: 100,
    level_bar->set_size_request(-1, 100);
    level_bar->set_inverted(true);
    level_bar->set_min_value(0);
    level_bar->set_max_value(10);
    level_bar->set_mode(Gtk::LevelBar::Mode::CONTINUOUS);
    level_bar->add_offset_value(GTK_LEVEL_BAR_OFFSET_LOW, 1.0);
    level_bar->add_offset_value(GTK_LEVEL_BAR_OFFSET_HIGH, 3.0); // 1.0
    level_bar->add_offset_value(GTK_LEVEL_BAR_OFFSET_FULL, 10.0); // 5.0
    double precip_number = precip != "Ip"? std::stod(precip) : 0.0;
    level_bar->set_value(10.0 * std::tanh(precip_number / 3.0));

    auto label = Gtk::make_managed<Gtk::Label>(precip_number > 0.0 ? std::format("{:.1f}", precip_number) : "");

    box->append(*level_bar);
    box->append(*label);

    return box;
}

typedef struct {
    std::string description;
    std::string icon;
    Glib::ustring symbol;
} WMO_WIC_Code;

static const std::map<int32_t, WMO_WIC_Code> WMO_WIC = {
    { 0 , { "clear sky", "clear-day", "☀️" } },
    { 1 , { "mainly clear", "mostly-clear-day", "🌤️" } },
    { 2 , { "partly cloudy", "partly-cloudy-day", "🌥️" } },
    { 3 , { "overcast", "cloudy", "☁️" } },
    { 45 , { "fog", "fog", "🌫" } },
    { 48 , { "rime", "fog", "🌫" } },
    { 51 , { "light drizzle", "drizzle", "🌧️" } },
    { 53 , { "moderate drizzle", "drizzle", "🌧️" } },
    { 55 , { "dense drizzle", "drizzle", "🌧️" } },
    { 56 , { "freezing light drizzle", "freezingdrizzle", "🌧️" } },
    { 57 , { "freezing dense drizzle", "freezingdrizzle", "🌧️" } },
    { 61 , { "light rain", "rain", "⛈" } },
    { 63 , { "moderate rain", "rain", "⛈" } },
    { 65 , { "heavy rain", "rain", "⛈" } },
    { 66 , { "freezing light rain", "freezingrain", "❄️" } },
    { 67 , { "freezing heavy rain", "freezingrain", "❄️" } },
    { 71 , { "light snow", "snow", "❄️" } },
    { 73 , { "moderate snow", "snow", "❄️" } },
    { 75 , { "heavy snow", "snow", "❄️" } },
    { 77 , { "snow grains", "snowflake", "❄️" } },
    { 80 , { "light rain showers", "rain", "⛈" } },
    { 81 , { "moderate rain showers", "rain", "⛈" } },
    { 82 , { "heavy rain showers", "rain", "⛈" } },
    { 85 , { "light snow showers", "snow", "❄️" } },
    { 86 , { "heavy snow showers", "snow", "❄️" } },
    { 95 , { "thunderstorms", "thunderstorm", "🌩" } },
    { 96 , { "light hail thunderstorms", "hail", "🌩" } },
    { 99 , { "heavy hail thunderstorms", "hail", "🌩"  }}
};

static Glib::RefPtr<Gtk::Box> HourOM(int time, const Glib::ustring &icon, const std::string &details, const std::string &temp, const std::string &pop, const std::string &cloud, const std::string &w_dir, const std::string &w_vel, const std::string &precip)
{
    auto box = Glib::RefPtr<Gtk::Box>(Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL));
    box->set_vexpand(true);
    box->add_css_class("weather-hour");

    auto cbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    cbox->set_vexpand(true);
    cbox->set_homogeneous(true);

    auto time_label = Gtk::make_managed<Gtk::Label>(std::format("{}", time));

    auto weather_icon = Gtk::make_managed<Gtk::Image>(Utils::XDG_CONFIG_HOME + "/gtkshell/weathericons/weathericons-filled/" + icon + ".svg");
    weather_icon->add_css_class("weather-icon");
    weather_icon->set_tooltip_text(details);

    auto temp_label = Gtk::make_managed<Gtk::Label>(std::format("{}°", temp));

    auto cloud_int = std::stoi(cloud);
    auto cloud_label = Gtk::make_managed<Gtk::Label>();
    if (cloud_int > 0) {
        cloud_label->set_text(std::format("{}%", cloud));
        cloud_label->set_tooltip_text(std::format("cloud coverage: {}%", cloud));
    }

    Gtk::Image *w_dir_icon;
    if (w_dir != "") {
        w_dir_icon = Gtk::make_managed<Gtk::Image>(Utils::XDG_CONFIG_HOME + "/gtkshell/windrose/" + get_wind_direction(str_to_int(w_dir)) + ".svg");
        w_dir_icon->set_tooltip_text(std::format("wind direction: {}°", w_dir));
    } else {
        w_dir_icon = Gtk::make_managed<Gtk::Image>();
    }
    w_dir_icon->add_css_class("weather-icon");

    auto w_vel_label = Gtk::make_managed<Gtk::Label>();
    w_vel_label->set_text(std::format("{}", w_vel));
    w_vel_label->set_tooltip_text(std::format("wind speed: {} km/h", w_vel));

    auto pop_int = std::stoi(pop);
    auto pop_label = Gtk::make_managed<Gtk::Label>();
    if (pop_int > 0) {
        pop_label->set_text(std::format("{}%", pop));
        pop_label->set_tooltip_text(std::format("pop: {}%", pop));
    }

    cbox->append(*time_label);
    cbox->append(*weather_icon);
    cbox->append(*temp_label);
    cbox->append(*cloud_label);
    cbox->append(*w_dir_icon);
    cbox->append(*w_vel_label);
    cbox->append(*pop_label);

    box->append(*cbox);
    box->append(*Precipitation(precip));

    return box;
}

static Glib::RefPtr<Gtk::Box> Hours(const std::map<int32_t, std::map<std::string, std::string>> &hours, const std::string &provider)
{
    auto box = Glib::RefPtr<Gtk::Box>(Gtk::make_managed<Gtk::Box>());
    box->set_vexpand(true);
    box->set_homogeneous(true);

    for (auto hour : hours) {
        int32_t h = hour.first;
        if (provider == "open-meteo") {
            auto code = hour.second["weather_code"];
            auto iter = WMO_WIC.find(str_to_int(code));
            if (iter != WMO_WIC.end()) {
                const auto wic = iter->second;
                box->append(*HourOM(h, wic.icon, wic.description, hour.second["temperature_2m"], hour.second["precipitation_probability"],
                                    hour.second["cloud_cover_low"], hour.second["wind_direction_10m"], hour.second["wind_speed_10m"],
                                    hour.second["precipitation"]));
            }
        }
    }

    return box;
}

static Glib::RefPtr<Gtk::Box> Day(const std::string &day, const std::map<int32_t, std::map<std::string, std::string>> &hours, const std::string &provider)
{
    auto box = Glib::RefPtr<Gtk::Box>(Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL));
    box->set_vexpand(true);
    box->add_css_class("weather-day");

    auto day_label = Gtk::make_managed<Gtk::Label>(day);
    day_label->add_css_class("weather-day-name");
    day_label->set_xalign(0);
    day_label->set_hexpand(true);
    day_label->set_justify(Gtk::Justification::LEFT);

    box->append(*day_label);
    box->append(*Hours(hours, provider));

    return box;
}

void Weather::create_notebook(const json &om_result)
{
    if (om_result.size() == 0)
        return;

    popover = Glib::RefPtr<Gtk::Popover>(new Gtk::Popover());

    auto om_box = Gtk::make_managed<Gtk::Box>();
    popover->add_css_class("weather");

    const auto current = om_result["current"];

    // Open-Meteo results
    const std::vector<std::string> weekday = { "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday" };
    const auto om_results = om_result["hourly"];
    int nresults = om_results["time"].size();
    const auto now = DateTime::create_now_local();
    const auto pred = now.add_hours(om_input.prediction);
    std::map<std::string, std::string> prediction;
    for (auto i = 0; i < om_input.days; ++i) {
        const auto day = DateTime::parse("%FT%H:%M", om_results["time"][i * 24]);
        auto dow = weekday[day.get_day_of_week()];
        std::map<int32_t, std::map<std::string, std::string>> hours;
        for (auto h = 0; h < 24; ++h) {
            const auto idx = i * 24 + h;
            if (idx >= nresults)
                goto skip;
            const auto date = DateTime::parse("%FT%H:%M", om_results["time"][idx]);
            if (now > date)
                continue;
            hours[h] = {};
            for (auto variable : om_input.vars_hourly) {
                if (om_results[variable].size() > idx) {
                    auto j = om_results[variable].at(idx);
                    if (!j.is_null()) {
                        hours[h][variable] = std::format("{}", j.get<double>());
                    } else {
                        goto skip;
                    }
                }
            }
            // Check for prediction
            auto date_1 = date.add_hours(1);
            if (pred >= date && pred < date_1) {
                prediction = hours[h];
            }
        }
        if (hours.size() > 0) {
            om_box->append(*Day(dow, hours, "open-meteo"));
        }
    }
skip:
    // Weather indicators for bar
    auto current_label = Gtk::make_managed<Gtk::Label>();
    int code = current["weather_code"];
    auto iter = WMO_WIC.find(code);
    if (iter != WMO_WIC.end()) {
        const auto &code = iter->second;
        current_label->set_text(std::format("{}  {}° {}", code.symbol.c_str(), current["temperature_2m"].get<double>(), code.description));
        current_label->add_css_class("weather-current");
        current_label->set_visible(true);
    }

    auto forecast_label = Gtk::make_managed<Gtk::Label>();
    code = str_to_int(prediction["weather_code"]);
    iter = WMO_WIC.find(code);
    if (iter != WMO_WIC.end()) {
        const auto &code = iter->second;
        forecast_label->set_text(std::format("{}  {}° {}", code.symbol.c_str(), current["temperature_2m"].get<double>(), code.description));
        forecast_label->add_css_class("weather-forecast");
        forecast_label->set_visible(true);
    }

    auto children = get_children();
    for (auto child : children)
        remove(*child);
    append(*current_label);
    append(*forecast_label);

    click = Gtk::GestureClick::create();
    click->set_button(0); // 0 = all, 1 = left, 2 = center, 3 = right
    click->signal_pressed().connect([this] (int n_press, double x, double y) {
        auto mbutton = this->click->get_current_button();
        if (mbutton == GDK_BUTTON_PRIMARY) {
            auto visible = popover->get_visible();
            if (visible)
                popover->popdown();
            else
                popover->popup();
        } else if (mbutton == GDK_BUTTON_SECONDARY) {
            update();
        }
    }, true);
    add_controller(click);

    om_box->set_visible(true);

    // Build the OM page
    auto om_scrollable = Gtk::make_managed<Gtk::ScrolledWindow>();
    om_scrollable->set_size_request(600, 400);
    om_scrollable->set_visible(true);
    om_scrollable->property_hscrollbar_policy().set_value(Gtk::PolicyType::ALWAYS);
    om_scrollable->property_vscrollbar_policy().set_value(Gtk::PolicyType::NEVER);
    om_scrollable->set_child(*om_box);

    auto nb_om_label = Gtk::make_managed<Gtk::Label>("open-meteo");
    nb_om_label->add_css_class("weather-notebook-tab");

    // Create notebook
    auto notebook = Gtk::make_managed<Gtk::Notebook>();
    notebook->append_page(*om_scrollable, *nb_om_label);

    notebook->set_visible(true);
    popover->set_has_arrow(false);
    popover->set_parent(*this);
    popover->set_child(*notebook);

}

void Weather::update_task()
{
    while (true) {
        sem.acquire();
        working = true;

        // Get Open-Meteo data
        std::string url = om_base_url + std::format("latitude={}&longitude={}&current=", om_input.latitude, om_input.longitude);
        for (auto var : om_input.vars_current) {
            url += var + ",";
        }
        url.pop_back();
        url += "&hourly=";
        for (auto var : om_input.vars_hourly) {
            url += var + ",";
        }
        url.pop_back();
        url += std::format("&timezone={}&forecast_days={}", om_input.timezone, om_input.days);

        auto om_data = Utils::fetch(url);
        json om_result;
        if (om_data.size() > 0) {
            om_result = json::parse(om_data);
        }

        mtx.lock();
        shared_data.om_data = om_result;
        mtx.unlock();
        dispatcher.emit();

        working = false;
    }
}

void Weather::update()
{
    if (!working)
        sem.release();
}
Weather::Weather(int polltime) : sem(0), dispatcher()
{
    update_thread = std::make_shared<std::thread>(&Weather::update_task, this);
    update_thread->detach();

    dispatcher.connect(
        [this]() {
            mtx.lock();
            auto om_data = shared_data.om_data;
            mtx.unlock();

            create_notebook(om_data);
        });

    update();
    timer = Glib::signal_timeout().connect_seconds(
        [this]() {
            update();
            return true;
        },
        polltime);
}

Weather::~Weather() {
}
