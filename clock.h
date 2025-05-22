#ifndef __GTKSHELL_CLOCK__
#define __GTKSHELL_CLOCK__

#include <giomm/file.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>

/*
 * Manage calendar data like holidays, birthdays, etc.
 * Data should be located in a directory passed to the constructor
 * in text files (.txt). CalendarData will read every text file, creating a
 * "class_name" equal to the name of the file for the events contained in it
 * The format of the files is very simple:
 *   '#' At the beginning of a line ignores that line
 *   Any other line is read, split by `,`, and the parsed like this:
 *   There MUST be 6 elements per record.
 *     - BEGIN_DATE(YYYY,MM,DD),REPEAT,COMPUTE,DESCRIPTION
 *     - BEGIN_DATE: (YYYY,MM,DD) Date from which the day will be marked
 *     - REPEAT: 0|1. Repeat the date from then on.
 *     - COMPUTE: 0|1. Personalize message with difference TODAY-BEGIN_DATE (birthdays, anniversaries)
 *     - DESCRIPTION: Anything after the last ','. Of couse it cannot contain commas, or anything after one will be ignored 
 */

typedef struct {
    Glib::ustring event_type;
    Glib::ustring event;
} CalendarEntry;

class Calendar;
class CalendarOptions;

struct UstringHash {
    std::size_t operator() (const Glib::ustring &str) const {
        auto h1 = std::hash<std::string>{}(str);
        return h1;
    }
};

class CalendarData {
public:
    CalendarData(Calendar *calendar, const Glib::ustring &directory);
    CalendarData() = default;

    std::vector<CalendarEntry> get_data_for_day(int year, int month, int day) const;

private:
    typedef struct {
        int year;
        int month;
        int day;
        bool repeat;
        bool compute;
        std::string description;
    } CalendarDatum;

    Calendar *calendar;
    Glib::RefPtr<Gio::FileMonitor> monitor;

    // Contains one element per file, so they can be updated by the file monitor
    std::unordered_map<Glib::ustring, std::vector<CalendarDatum>, UstringHash> database;

    void parse(const Glib::ustring &name, const Glib::ustring &contents);
};

class Clock : public Gtk::Button {
public:
    Clock();
    ~Clock();

private:
    Calendar *calendar;
    CalendarOptions *options;
    Glib::RefPtr<Gtk::Label> clock_label;
};

#endif // __GTKSHELL_CLOCK__

