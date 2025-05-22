#ifndef __GTKSHELL_DATETIME__
#define __GTKSHELL_DATETIME__


#include <chrono>
#include <format>

// https://github.com/HowardHinnant/date/wiki/Examples-and-Recipes
// g++ -std=c++23 datetime datetime.cpp
class DateTime {
public:
    // Create a DateTime object in time zone
    static DateTime create_now(const std::string_view &timezone) {
        DateTime dt;
        std::chrono::zoned_time zt(timezone, std::chrono::system_clock::now());
        dt.tp = zt;
        return dt;
    }
    // Create a DateTime object in local time zone
    static DateTime create_now_local() {
        DateTime dt;
        static const std::chrono::time_zone *tz = std::chrono::current_zone();
        std::chrono::zoned_time zt(tz, std::chrono::system_clock::now());
        dt.tp = zt;
        return dt;
    }
    // Create a DateTime object in local time zone
    static DateTime create_local(int year, int month, int day, int hour, int min, int sec) {
        DateTime dt;
        static const std::chrono::time_zone *tz = std::chrono::current_zone();
        auto ymd = std::chrono::year_month_day(std::chrono::year(year)/std::chrono::month(month)/std::chrono::day(day));
        auto ld = std::chrono::local_days(ymd) + std::chrono::hours(hour) + std::chrono::minutes(min) + std::chrono::seconds(sec);
        std::chrono::zoned_time zt(tz, ld);
        dt.tp = zt;
        return dt;
    }
    // Create a DateTime object in UTC time zone
    static DateTime create_now_utc() {
        DateTime dt;
        std::chrono::zoned_time zt("UTC", std::chrono::system_clock::now());
        dt.tp = zt;
        return dt;
    }
    // Parse datetime, fmt may contain TZ information or not
    static DateTime parse(const std::string &fmt, const std::string &datetime) {
        DateTime dt;
        std::chrono::time_point<std::chrono::system_clock> day;
        std::istringstream iss(datetime);
        iss >> std::chrono::parse(fmt, day);
        dt.tp = day;
        return dt;
    }
    // Parse datetime, fmt may contain TZ information or not, but it will be
    // stored in timezone format
    static DateTime parse(const std::string &fmt, const std::string &datetime, const std::string &timezone) {
        DateTime dt;
        std::chrono::time_point<std::chrono::system_clock> day;
        std::istringstream iss(datetime);
        iss >> std::chrono::parse(fmt, day);
        std::chrono::zoned_time zt(timezone, day);
        dt.tp = zt;
        return dt;
    }
    // Parse datetime, fmt may contain TZ information or not, but it will be
    // stored in local timezone format
    static DateTime parse_local(const std::string &fmt, const std::string &datetime) {
        DateTime dt;
        static const std::chrono::time_zone *tz = std::chrono::current_zone();
        std::chrono::time_point<std::chrono::system_clock> day;
        std::istringstream iss(datetime);
        iss >> std::chrono::parse(fmt, day);
        std::chrono::zoned_time zt(tz, day);
        dt.tp = zt;
        return dt;
    }
    std::string to_string(const std::string &fmt) const {
        std::string s = std::vformat("{:" + fmt + "}", std::make_format_args(tp));
        return s;
    }
    std::tm to_tm() const {
        auto lt = tp.get_local_time();
        auto ld = std::chrono::floor<std::chrono::days>(lt);
        std::chrono::year_month_day ymd(ld);
        std::chrono::hh_mm_ss tod(lt - ld);
        std::tm t{};
        t.tm_sec = tod.seconds().count();
        t.tm_min = tod.minutes().count();
        t.tm_hour = tod.hours().count();
        t.tm_mday = (ymd.day() - std::chrono::day(0)).count();
        t.tm_mon = (ymd.month() - std::chrono::January).count();
        t.tm_year = (ymd.year() - std::chrono::year(1900)).count();
        t.tm_wday = (std::chrono::weekday(ld) - std::chrono::Sunday).count();
        t.tm_yday = (ld - std::chrono::local_days(ymd.year()/std::chrono::January/1)).count();
        t.tm_isdst = tp.get_info().save != std::chrono::minutes(0);
        return t;
    }
    void get_ymd(int32_t &y, int32_t &m, int32_t &d) const {
        auto dp = std::chrono::floor<std::chrono::days>(tp.get_local_time());
        std::chrono::year_month_day ymd(dp);
        y = static_cast<int32_t>(ymd.year());
        m = static_cast<uint32_t>(ymd.month());
        d = static_cast<uint32_t>(ymd.day());
    }
    DateTime add_hours(int hours) const {
        DateTime dt;
        dt.tp = std::chrono::zoned_time(tp.get_time_zone(), tp.get_sys_time() + std::chrono::hours(hours));
        return dt;
    }
    void get_hms(int32_t &h, int32_t &m, int32_t &s) const {
        auto lt = tp.get_local_time();
        auto ld = std::chrono::floor<std::chrono::days>(lt);
        std::chrono::hh_mm_ss tod(lt - ld);
        h = tod.hours().count();
        m = tod.minutes().count(); 
        s = tod.seconds().count();
    }
    DateTime set_hours(int hours) const {
        auto dp = std::chrono::floor<std::chrono::days>(tp.get_local_time());
        std::chrono::hh_mm_ss time(std::chrono::floor<std::chrono::seconds>(tp.get_local_time() - dp));
        DateTime dt;
        dt.tp = std::chrono::zoned_time(tp.get_time_zone(), dp + std::chrono::hours(hours) + time.minutes() + time.seconds() + time.subseconds());
        return dt;
    }
    // Sunday = 0, Monday = 1,...
    int get_day_of_week() const {
        auto dp = std::chrono::floor<std::chrono::days>(tp.get_local_time());
        return (std::chrono::weekday(dp) - std::chrono::Sunday).count();
    }

private:
    friend constexpr bool operator==(const DateTime &x, const DateTime &y) noexcept;
    friend constexpr bool operator<(const DateTime &x, const DateTime &y) noexcept;
    friend constexpr bool operator<=(const DateTime &x, const DateTime &y) noexcept;
    friend constexpr bool operator>(const DateTime &x, const DateTime &y) noexcept;
    friend constexpr bool operator>=(const DateTime &x, const DateTime &y) noexcept;

    std::chrono::zoned_time<std::chrono::system_clock::duration> tp;
};
    
constexpr bool operator==(const DateTime &x, const DateTime &y ) noexcept {
    return x.tp.get_local_time() == y.tp.get_local_time();
}
constexpr bool operator<(const DateTime &x, const DateTime &y ) noexcept {
    return x.tp.get_local_time() < y.tp.get_local_time();
}
constexpr bool operator<=(const DateTime &x, const DateTime &y ) noexcept {
    return x.tp.get_local_time() <= y.tp.get_local_time();
}
constexpr bool operator>(const DateTime &x, const DateTime &y ) noexcept {
    return x.tp.get_local_time() > y.tp.get_local_time();
}
constexpr bool operator>=(const DateTime &x, const DateTime &y ) noexcept {
    return x.tp.get_local_time() >= y.tp.get_local_time();
}

#endif // __GTKSHELL_DATETIME__
