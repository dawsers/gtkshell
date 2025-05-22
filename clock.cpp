#include "clock.h"
#include "utils.h"
#include "bind.h"

#include <giomm/file.h>
#include <glibmm.h>
#include <gtkmm/window.h>
#include <gtkmm/box.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/popover.h>

#include <unordered_map>


class CalendarOptions {
public:
    CalendarOptions() {
        Glib::DateTime now = Glib::DateTime::create_now_local();
        start_date = Glib::Date(now.get_day_of_month(), (Glib::Date::Month)now.get_month(), now.get_year());
        number_months_displayed = 12;
        language = "en";
        display_week_number = false;
        week_start = Glib::Date::Weekday::MONDAY;
        calendar_data_dir =  Utils::XDG_CONFIG_HOME + "/gtkshell/calendar";
    }

    Glib::Date start_date;
    int start_year;
    int number_months_displayed;
    Glib::Date min_date;
    Glib::Date max_date;
    std::string language;
    bool display_week_number;
    Glib::Date::Weekday week_start;
    Glib::ustring calendar_data_dir;
};

class Calendar : public Gtk::Popover {
public:
    Calendar(CalendarOptions *opts) {
        if (!opts) {
            options = new CalendarOptions();
        } else {
            options = opts;
        }
        data = new CalendarData(this, options->calendar_data_dir);
		set_start_date(options->start_date);
    }
    ~Calendar() {
    }

    void toggle_visibility() {
        if (get_visible())
            popdown();
        else
            popup();
    }

    void update_ui() {
        create_ui();
    }

private:
	bool is_full_year_mode() const {
		return start_date.get_month() == Glib::Date::Month::JANUARY && options->number_months_displayed == 12;
	}

	/**
     * Gets the week number for a specified date.
     *
     * @param date The specified date.
     *
     * Return 0 if date is before the first Monday of the year
     */
	int get_week_number(const Glib::Date &date) const {
        return date.get_monday_week_of_year();
	}

	/**
     * Gets the period displayed on the calendar.
     */
    typedef struct {
        Glib::Date start_date;
        Glib::Date end_date;
    } Period;

	Period get_current_period() const {
        Period period;
		period.start_date = start_date;
		period.end_date = start_date;
		period.end_date.add_months(options->number_months_displayed);
        return period;
	}

	/**
     * Gets the year displayed on the calendar.
	 * If the calendar is not used in a full year configuration, this will return the year of the first date displayed in the calendar.
     */
	int get_year() const {
        return start_date.get_year();
	}

	/**
     * Sets the year displayed on the calendar.
	 * If the calendar is not used in a full year configuration, this will set the start date to January 1st of the given year.
     *
     * @param year The year to displayed on the calendar.
     */
	void set_year(int year) {
        set_start_date(Glib::Date(1, Glib::Date::Month::JANUARY, year));
	}

	/**
     * Gets the first date displayed on the calendar.
     */
    Glib::Date get_start_date() const {
		return start_date;
	}

	/**
     * Sets the first date that should be displayed on the calendar.
     *
     * @param startDate The first date that should be displayed on the calendar.
     */
	void set_start_date(const Glib::Date &start_date) {
        options->start_date = start_date;
        this->start_date.set_dmy(1, start_date.get_month(), start_date.get_year());
        create_ui();
	}

	/**
     * Gets the number of months displayed by the calendar.
     */
	int get_number_months_displayed() const {
		return options->number_months_displayed;
	}

	/**
     * Sets the number of months displayed that should be displayed by the calendar.
	 * 
	 * This method causes a refresh of the calendar.
     *
     * @param numberMonthsDisplayed Number of months that should be displayed by the calendar.
	 * @param preventRedering Indicates whether the rendering should be prevented after the property update.
     */
	void setNumberMonthsDisplayed(int number_months_displayed) {
		if (number_months_displayed > 0 && number_months_displayed <= 12) {
			options->number_months_displayed = number_months_displayed;
            create_ui();
		}
	}

	/**
     * Gets the minimum date of the calendar.
     */
    Glib::Date get_min_date() const {
		return options->min_date;
	}

	/**
     * Sets the minimum date of the calendar.
	 * 
	 * This method causes a refresh of the calendar.
     *
     * @param minDate The minimum date to set.
	 * @param preventRedering Indicates whether the rendering should be prevented after the property update.
     */
    void set_min_date(const Glib::Date &date) {
        options->min_date = date;
        create_ui();
	}

	/**
     * Gets the maximum date of the calendar.
     */
    Glib::Date get_max_date() const {
		return options->max_date;
	}

	/**
     * Sets the maximum date of the calendar. 
	 * 
	 * This method causes a refresh of the calendar.
     *
     * @param maxDate The maximum date to set.
	 * @param preventRedering Indicates whether the rendering should be prevented after the property update.
     */
    void set_max_date(const Glib::Date &date) {
        options->max_date = date;
        create_ui();
	}

	/**
     * Gets a value indicating whether the weeks number are displayed.
     */
	bool get_display_week_number() const {
		return options->display_week_number;
	}

	/**
     * Sets a value indicating whether the weeks number are displayed.
	 * 
	 * This method causes a refresh of the calendar.
     *
     * @param  displayWeekNumber Indicates whether the weeks number are displayed.
	 * @param preventRedering Indicates whether the rendering should be prevented after the property update.
     */
    void set_display_week_number(bool display_week_number) {
		options->display_week_number = display_week_number;
        create_ui();
	}

	/**
     * Gets the language used for calendar rendering.
     */
    std::string get_language() {
		return options->language;
	}

	/**
     * Sets the language used for calendar rendering.
	 * 
	 * This method causes a refresh of the calendar.
     *
     * @param language The language to use for calendar redering.
	 * @param preventRedering Indicates whether the rendering should be prevented after the property update.
     */
	void set_language(const std::string &language) {
        if (locales.find(language.c_str()) != locales.end()) {
            options->language = language;
            create_ui();
		}
	}

	/**
     * Gets the starting day of the week.
     */
    Glib::Date::Weekday get_week_start() const {
		return options->week_start;
	}

	/**
     * Sets the starting day of the week.
	 * 
	 * This method causes a refresh of the calendar.
     *
     * @param weekStart The starting day of the week. This option overrides the parameter define in the language file.
     * @param preventRedering Indicates whether the rendering should be prevented after the property update.
     */
    void set_week_start(Glib::Date::Weekday week_start) {
		options->week_start = week_start;
        create_ui();
	}
	/**
     * Renders the calendar.
     */
	void create_ui() {
#if 0
        // Window is invisible by default
        bool visible = false;
        if (window) {
            // but inherits visibility in case one existed previously
            // and we are just re-creating the UI
            visible = window->get_visible();
            window->destroy();
            delete window;
        }
        window = new GtkShellWindow("gtkshell-calendar", GtkShellAnchor::ANCHOR_TOP, monitor.c_str(), false, GtkShellStretch::STRETCH_NONE);
        window->set_name("Calendar");
#endif
        auto calendar_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
        calendar_box->add_css_class("calendar");
        calendar_box->append(*render_header());
        calendar_box->append(*render_body());

        set_has_arrow(false);
        set_child(*calendar_box);
	}

    Gtk::Box *render_header() {
        auto iter = locales.find(options->language);
        if (iter == locales.end()) {
            Utils::log(Utils::LogSeverity::ERROR, std::format("Calendar: cannot find locale data for {}", options->language.c_str()));
            return nullptr;
        }
        const CalendarLocale &locale = iter->second; 

        auto header_table = Gtk::make_managed<Gtk::Box>();
        header_table->set_homogeneous(true);
        header_table->add_css_class("calendar-header");
		
		auto period = get_current_period();
		
		// Left arrow
        auto prev_div = Gtk::make_managed<Gtk::Button>();
        prev_div->add_css_class("prev");
        auto prev_div_label = Gtk::make_managed<Gtk::Label>("<");
        prev_div->signal_clicked().connect([this] () { set_year(get_year() - 1); });
        prev_div->set_child(*prev_div_label);
        
		if (options->min_date.valid() && options->min_date >= period.start_date) {
			prev_div->set_visible(false); // ('disabled');
		}
		
		header_table->append(*prev_div);
		
		if (is_full_year_mode()) {
			// Year N-2
            auto prev2year_div = Gtk::make_managed<Gtk::Button>();
            prev2year_div->add_css_class("year-title");
            auto prev2year_div_label = Gtk::make_managed<Gtk::Label>(std::format("{}", start_date.get_year() - 2));
            prev2year_div->signal_clicked().connect([this] () { set_year(start_date.get_year() - 2); });
            prev2year_div->set_child(*prev2year_div_label);
            
            if (options->min_date.valid() && options->min_date >= Glib::Date(31, (Glib::Date::Month)12, start_date.get_year() - 2)) {
                prev2year_div->set_visible(false); // ('disabled');
            }
			header_table->append(*prev2year_div);
			
			// Year N-1
            auto prevyear_div = Gtk::make_managed<Gtk::Button>();
            prevyear_div->add_css_class("year-title");
            auto prevyear_div_label = Gtk::make_managed<Gtk::Label>(std::format("{}", start_date.get_year() - 1));
            prevyear_div->signal_clicked().connect([this] () { set_year(start_date.get_year() - 1); });
            prevyear_div->set_child(*prevyear_div_label);
            
            if (options->min_date.valid() && options->min_date >= Glib::Date(31, (Glib::Date::Month)12, start_date.get_year() - 1)) {
                prevyear_div->set_visible(false); // ('disabled');
            }
			header_table->append(*prevyear_div);
		}
		
		// Current year
        auto year_div = Gtk::make_managed<Gtk::Label>();
        year_div->add_css_class("year-title-current");

		if (is_full_year_mode()) {
			year_div->set_text(std::format("{}", start_date.get_year()));
		} else if (options->number_months_displayed == 12) {
			year_div->set_text(std::format("{} - {}", period.start_date.get_year(), period.end_date.get_year()));
		} else if (options->number_months_displayed > 1) {
            year_div->set_text(std::format("{} {} - {} {}", locale.months[period.start_date.get_month_as_int()], period.start_date.get_year(), locale.months[period.end_date.get_month_as_int()], period.end_date.get_year()));
		} else {
            year_div->set_text(std::format("{} {}", locale.months[period.start_date.get_month_as_int()], period.start_date.get_year()));
		}
		
		header_table->append(*year_div);

		if (is_full_year_mode()) {
			// Year N+1
            auto nextyear_div = Gtk::make_managed<Gtk::Button>();
            nextyear_div->add_css_class("year-title");
            auto nextyear_div_label = Gtk::make_managed<Gtk::Label>(std::format("{}", start_date.get_year() + 1));
            nextyear_div->signal_clicked().connect([this] () { set_year(start_date.get_year() + 1); });
            nextyear_div->set_child(*nextyear_div_label);
            
            if (options->min_date.valid() && options->min_date >= Glib::Date(31, (Glib::Date::Month)12, start_date.get_year() + 1)) {
                nextyear_div->set_visible(false); // ('disabled');
            }
			header_table->append(*nextyear_div);

			// Year N+2
            auto next2year_div = Gtk::make_managed<Gtk::Button>();
            next2year_div->add_css_class("year-title");
            auto next2year_div_label = Gtk::make_managed<Gtk::Label>(std::format("{}", start_date.get_year() + 2));
            next2year_div->signal_clicked().connect([this] () { set_year(start_date.get_year() + 2); });
            next2year_div->set_child(*next2year_div_label);
            
            if (options->min_date.valid() && options->min_date >= Glib::Date(31, (Glib::Date::Month)12, start_date.get_year() + 2)) {
                next2year_div->set_visible(false); // ('disabled');
            }
			header_table->append(*next2year_div);
		}
 
		// Right arrow
        auto next_div = Gtk::make_managed<Gtk::Button>();
        next_div->add_css_class("next");
        auto next_div_label = Gtk::make_managed<Gtk::Label>(">");
        next_div->signal_clicked().connect([this] () { set_year(get_year() + 1); });
        next_div->set_child(*next_div_label);
        
		if (options->max_date.valid() && options->max_date <= period.end_date) {
			next_div->set_visible(false); // ('disabled');
		}
		header_table->append(*next_div);

        return header_table;
	}

    Gtk::FlowBox *render_body() {
        auto iter = locales.find(options->language.c_str());
        if (iter == locales.end()) {
            Utils::log(Utils::LogSeverity::ERROR, std::format("Calendar: cannot find locale data for {}", options->language.c_str()));
            return nullptr;
        }
        const CalendarLocale &locale = iter->second; 

        Glib::DateTime now = Glib::DateTime::create_now_local();
        auto today_d = now.get_day_of_month();
        auto today_m = now.get_month();
        auto today_y = now.get_year();
        Glib::Date today(today_d, (Glib::Date::Month)today_m, today_y);

        auto months_div = Gtk::make_managed<Gtk::FlowBox>();
        months_div->add_css_class("months-container");
        months_div->set_selection_mode(Gtk::SelectionMode::NONE);
        months_div->set_column_spacing(8);
        months_div->set_row_spacing(8);
        months_div->set_min_children_per_line(3);
        months_div->set_max_children_per_line(3);

		auto month_start_date = start_date;
		
		for (auto m = 0; m < options->number_months_displayed; ++m) {
			/* Container */
            auto month_div = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
            month_div->add_css_class("month-container");

			/* Month header */
            auto thead = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
			
            auto title_row = Gtk::make_managed<Gtk::Label>();
            title_row->add_css_class("month-title");
            title_row->set_text(locale.months[month_start_date.get_month_as_int() - 1]);
			
			thead->append(*title_row);
			
            auto header_row = Gtk::make_managed<Gtk::Box>();
            header_row->add_css_class("day-header");
            header_row->set_spacing(2);
            header_row->set_homogeneous(true);
			
			if (options->display_week_number) {
                auto week_number_cell = Gtk::make_managed<Gtk::Label>();
                week_number_cell->add_css_class("week-number");
                week_number_cell->set_text(locale.week_short);
				header_row->append(*week_number_cell);
			}
			
			auto week_start = get_week_start();
            for (int d = 0; d < 7; ++d) {
                int idx = (d + (int)week_start - 1) % 7;
                auto header_cell = Gtk::make_managed<Gtk::Label>();
                header_cell->add_css_class("day-names");
                header_cell->set_text(locale.days_short[idx]);
				
				header_row->append(*header_cell);
            }
			
			thead->append(*header_row);
			month_div->append(*thead);
			
			/* Days */
			auto current_date = month_start_date;
            auto last_date = month_start_date;
            last_date.add_months(1);
            last_date.set_day(1);
			
			while (current_date.get_weekday() != week_start) {
				current_date.subtract_days(1);
			}
			
			while (current_date < last_date) {
                auto row = Gtk::make_managed<Gtk::Box>();
                row->set_spacing(2);
                row->set_homogeneous(true);
				
				if (options->display_week_number) {
                    auto week_number_cell = Gtk::make_managed<Gtk::Label>();
                    week_number_cell->add_css_class("week-number");
                    week_number_cell->set_text(std::format("{}", current_date.get_monday_week_of_year()));
                    row->append(*week_number_cell);
				}
			
				do {
                    auto cell = Gtk::make_managed<Gtk::Label>();
					if (current_date < month_start_date) {
						cell->add_css_class("day-old");
					} else if (current_date >= last_date) {
						cell->add_css_class("day-new");
					} else {
                        // Get calendar data
                        auto day_y = current_date.get_year();
                        auto day_m = current_date.get_month();
                        auto day_d = current_date.get_day();
                        auto day_data = data->get_data_for_day(day_y, (int)day_m, day_d);
                        std::vector<Glib::ustring> class_names;
                        if (day_data.size() > 0) {
                            Glib::ustring tooltip = "";
                            for (auto event : day_data) {
                                class_names.push_back("day-" + event.event_type);
                                tooltip += event.event + "\n";
                            }
                            cell->set_tooltip_text(Utils::trim(tooltip));
                        }

                        // Today is special!
                        if (current_date == today) {
                            class_names.push_back("day-today");
                            if (notification_sent == false) {
                                notification_sent = true;
                                if (day_data.size() > 0) {
                                    Utils::notify("Calendar", "calendar", "Today's Events", cell->get_tooltip_text());
                                }
                            }
                        } else {
                            auto dow = current_date.get_weekday();
                            if (dow == Glib::Date::Weekday::SUNDAY) {
                                class_names.push_back("day-sunday");
                            } else if (dow == Glib::Date::Weekday::SATURDAY) {
                                class_names.push_back("day-saturday");
                            } else {
                                class_names.push_back("day-weekday");
                            }
                        }
                        cell->set_css_classes(class_names);
						cell->set_text(std::to_string(current_date.get_day()));
					}
					
					row->append(*cell);
					
					current_date.add_days(1);
				} while (current_date.get_weekday() != week_start);
				
			    month_div->append(*row);
			}
			
			months_div->append(*month_div);

			month_start_date.add_months(1);
		}
		
		return months_div;
	}

    CalendarOptions *options;

    Glib::Date start_date;
    CalendarData *data;

    static bool notification_sent;

    typedef struct {
        const std::vector<const char *> days;
        const std::vector<const char *> days_short;
        const std::vector<const char *> days_min;
        const std::vector<const char *> months;
        const std::vector<const char *> months_short;
        const char *week_short;
        Glib::Date::Weekday week_start;
    } CalendarLocale;

    const std::unordered_map<std::string, CalendarLocale> locales = {
        { "en" , {
			        { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" },
                    { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" },
                    { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" },
                    { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" },
                    { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" },
                    "W",
                    Glib::Date::Weekday::MONDAY
                 }
        }
    };
	
    const std::vector<Glib::ustring> colors = { "#2C8FC9", "#9CB703", "#F5BB00", "#FF4A32", "#B56CE2", "#45A597" };
};

CalendarData::CalendarData(Calendar *calendar, const Glib::ustring &directory) : calendar(calendar)
{
    auto cwd = Gio::File::create_for_path(directory);

    auto files = cwd->enumerate_children("standard::*", Gio::FileQueryInfoFlags::NOFOLLOW_SYMLINKS);

    for (auto fileinfo = files->next_file(); fileinfo != nullptr; fileinfo = files->next_file()) {
        auto file = files->get_child(fileinfo);
        parse(file->get_basename(), Utils::read_file(file));
    }
    
    monitor = Utils::monitor_directory(directory);

    monitor->signal_changed().connect([this] (const Glib::RefPtr<Gio::File> &file, const Glib::RefPtr<Gio::File> &other, Gio::FileMonitor::Event event) {
                                     parse(file->get_basename(), Utils::read_file(file));
                                     this->calendar->update_ui();
                                     });
}

std::vector<CalendarEntry> CalendarData::get_data_for_day(int year, int month, int day) const
{
    std::vector<CalendarEntry> results;
    for (const auto &entries : database) {
        for (const auto &event : entries.second) {
            if (event.repeat == false) {
                if (event.year != year || event.month != month || event.day != day)
                    continue;
            } else {
                if (event.month != month || event.day != day)
                    continue;
            }
            auto result = entries.first + ": ";
            if (event.compute && event.year >= year) {
                result += std::format("{} ", year - event.year);
            }
            result += event.description;
            results.push_back({ entries.first, result });
        }
    }
    return results;
}


void CalendarData::parse(const Glib::ustring &name, const Glib::ustring &contents)
{
    std::vector<CalendarDatum> records;
    auto file = Utils::split(contents, "\n");
    for (auto line : file) {
        if (line.starts_with('#'))
            continue;
        auto record = Utils::split(line, ",");
        if (record.size() != 6)
            // Invalid record
            continue;
        records.push_back({ std::stoi(record[0]), std::stoi(record[1]), std::stoi(record[2]),
                            std::stoi(record[3]) == 0 ? false : true,
                            std::stoi(record[4]) == 0 ? false : true, record[5]});
    }
    database[Utils::split(name, ".")[0]] = records;
}

bool Calendar::notification_sent = false;

class Date : public Glib::Object {
public:
    Date(int interval) :
        Glib::ObjectBase(typeid(Date)),
        date(*this, "date", get_date())
    {
        Glib::signal_timeout().connect_seconds([this] () {
            date.set_value(get_date());
            return true;
        }, interval);
    }
    ~Date() {
        connection.disconnect();
    }

    Glib::Property<Glib::ustring> date;

private:
#if 0
    Glib::ustring get_date() const {
        return Utils::exec("date +'%a %b %e   %H:%M'");
    }
#else
    Glib::ustring get_date() const {
        auto now = Glib::DateTime::create_now_local();
        return now.format("%a %b %e   %H:%M");
    }
#endif
    sigc::connection connection;
};

static Date *date = nullptr;

Clock::Clock()
{
    options = new CalendarOptions();
    //options->calendar_data_dir = Utils::XDG_DOCUMENTS_HOME + "/calendar";
    calendar = new Calendar(options);
    calendar->set_parent(*this);

    add_css_class("clock");
    if (date == nullptr) {
        date = new Date(15);
    }
    clock_label = Glib::make_refptr_for_instance(Gtk::make_managed<Gtk::Label>());
    set_child(*clock_label);

    bind_property_changed(date, "date",
         sigc::bind([] (Clock *clock) {
                    clock->clock_label->set_text(date->date.get_value());
                    }, this));
    signal_clicked().connect([this] () {
                             calendar->toggle_visibility();
                             });
}

Clock::~Clock()
{
    delete calendar;
    delete options;

    if (date != nullptr) {
        delete date;
        date = nullptr;
    }
}

#if 0
// https://github.com/HowardHinnant/date/wiki/Examples-and-Recipes#print-out-a-compact-calendar-for-the-year
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string>

std::chrono::year current_year()
{
    using namespace std::chrono;
    year_month_day ymd = floor<days>(system_clock::now());
    return ymd.year();
}

// The number of weeks in a calendar month layout plus 2 more for the calendar titles
unsigned number_of_lines_calendar(std::chrono::year_month const ym, std::chrono::weekday const firstdow)
{
    using namespace std::chrono;
    return static_cast<unsigned>(
        ceil<weeks>((weekday{ym/1} - firstdow) + ((ym/last).day() - day{0})).count()) + 2;
}

// Print one line of a calendar month
void print_line_of_calendar_month(std::ostream& os, std::chrono::year_month const ym,
                                  unsigned const line, std::chrono::weekday const firstdow)
{
    using namespace std;
    using namespace std::chrono;
    switch (line)
    {
    case 0:
        // Output month and year title
        os << left << setw(21) << format(os.getloc(), " %B %Y", ym) << right;
        break;
    case 1:
        {
        // Output weekday names title
        auto wd = firstdow;
        do
        {
            auto d = format(os.getloc(), "%a", wd);
            d.resize(2);
            os << ' ' << d;
        } while (++wd != firstdow);
        break;
        }
    case 2:
        {
        // Output first week prefixed with spaces if necessary
        auto wd = weekday{ym/1};
        os << string(static_cast<unsigned>((wd-firstdow).count())*3, ' ');
        auto d = std::chrono::day(1);
        do
        {
            os << format(" %e", d);
            ++d;
        } while (++wd != firstdow);
        break;
        }
    default:
        {
        // Output a non-first week:
        // First find first day of week
        unsigned index = line - 2;
        auto sd = sys_days{ym/1};
        if (weekday{sd} == firstdow)
            ++index;
        auto ymdw = ym/firstdow[index];
        if (ymdw.ok()) // If this is a valid week, print it out
        {
            auto d = year_month_day{ymdw}.day();
            auto const e = (ym/last).day();
            auto wd = firstdow;
            do
            {
                os << format(" %e", d);
            } while (++wd != firstdow && ++d <= e);
            // Append row with spaces if the week did not complete
            os << string(static_cast<unsigned>((firstdow-wd).count())*3, ' ');
        }
        else  // Otherwise not a valid week, output a blank row
            os << string(21, ' ');
        break;
        }
    }
}

void print_calendar_year(std::ostream& os, unsigned const cols = 3,
                         std::chrono::year const y = current_year(),
                         std::chrono::weekday const firstdow = std::chrono::Sunday)
{
    using namespace std::chrono;
    if (cols == 0 || 12 % cols != 0)
        throw std::runtime_error("The number of columns " + std::to_string(cols)
                                 + " must be one of [1, 2, 3, 4, 6, 12]");
    // Compute number of lines needed for each calendar month
    unsigned ml[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    for (auto& m : ml)
        m = number_of_lines_calendar(y/month{m}, firstdow);
    for (auto r = 0u; r < 12/cols; ++r) // for each row
    {
        const auto lines = *std::max_element(std::begin(ml) + (r*cols),
                                             std::begin(ml) + ((r+1)*cols));
        for (auto l = 0u; l < lines; ++l) // for each line
        {
            for (auto c = 0u; c < cols; ++c) // for each column
            {
                if (c != 0)
                    os << "   ";
                print_line_of_calendar_month(os, y/month{r*cols + c+1}, l, firstdow);
            }
            os << '\n';
        }
        os << '\n';
    }
}

int main()
{
    print_calendar_year(std::cout);
}
#endif

