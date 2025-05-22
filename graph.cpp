#include <glibmm.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/button.h>
#include <gdkmm.h>

#include <vector>
#include <deque>
#include <numeric>
#include <fstream>

#include "bind.h"
#include "utils.h"
#include "graph.h"

class Graph : public Gtk::DrawingArea {
public:
    Graph(const Glib::Property<double> &variable, size_t hist, const Color &col)
        : value(variable), history(hist), color(col), data(hist, 0.0) {
        value.get_proxy().signal_changed().connect([this]() {
            double d = value.get_value();
            if (!data.empty()) {
                data.pop_front();
            }
            data.push_back(d);
            this->queue_draw();
        });
        set_content_width(36);
        set_content_height(12);
        //set_size_request(36, 12);
        set_draw_func([this](const Cairo::RefPtr<Cairo::Context> &cr,
                                    int width, int height) {
            cr->scale(width, height);
            cr->rectangle(0.0, 0.0, 1.0, 1.0);
            cr->fill();
            int samples = data.size() - 1;
            const double max = 100.0;
            cr->set_source_rgb(color.r, color.g, color.b);
            if (samples > 0) {
                cr->move_to(1.0, 1.0);
                double x = 1.0, y = 1.0 - data[samples] / max;
                cr->line_to(x, y);
                for (auto j = samples - 1; j >= 0; j--) {
                    y = 1.0 - data[j] / max;
                    x = static_cast<double>(j) / samples;
                    cr->line_to(x, y);
                }
                cr->line_to(x, 1.0);
                cr->close_path();
                cr->fill();
            }
        });
    }

private:
    const Glib::Property<double> &value;
    size_t history;
    Color color;
    std::deque<double> data;
};


// CPU

typedef struct {
    int pid;
    std::string filename;
    std::string name;
    std::string state;
    int ppid;
    size_t utime;
    size_t stime;
    size_t cutime;
    size_t cstime;
    size_t time;  // Total CPU time at sample
} ProcessCpuData;

typedef struct {
    std::string name;
    double utilization;
    size_t total_time;
} ProcessUtilization;

class CpuMonitor : public Glib::Object {
public:
    CpuMonitor(int seconds)
        : Glib::ObjectBase(typeid(CpuMonitor)), cpu_load(*this, "cpu-load", 0.0), prev_idle_time(0), prev_total_time(0) {
        timer = Glib::signal_timeout().connect_seconds(
            [this]() {
                /*
                https://docs.kernel.org/filesystems/proc.html
                user: normal processes executing in user mode
                nice: niced processes executing in user mode
                system: processes executing in kernel mode
                idle: twiddling thumbs
                iowait: waiting for I/O to complete
                irq: servicing interrupts
                softirq: servicing softirqs
                */

                std::ifstream proc_stat("/proc/stat");
                proc_stat.ignore(5, ' '); // Skip the 'cpu' prefix.

                std::vector<size_t> times;
                for (size_t time; proc_stat >> time; times.push_back(time))
                    ;

                if (times.size() >= 4) {
                    auto idle_time = times[3];
                    // Accumulate only first four values, and skip IOWait, IRQ and SoftIRQ
                    auto total_time = std::accumulate(times.begin(), times.begin() + 4, 0);
                    const auto idle_time_delta = idle_time - prev_idle_time;
                    prev_idle_time = idle_time;
                    const auto total_time_delta = total_time - prev_total_time;
                    prev_total_time = total_time;
                    const double utilization = 100.0 * (1.0 - static_cast<double>(idle_time_delta) / total_time_delta);
                    cpu_load.set_value(std::round(utilization));
                    return true;
                } else {
                    Utils::log(Utils::LogSeverity::ERROR, "CpuMonitor: /proc/stat doesn't contain all the needed information");
                    return false;
                }
            },
            seconds);
    }
    std::vector<ProcessUtilization> get_processes_cpu_utilization(double seconds, int number = 10) const {
        pid_t pid = getpid();
        std::vector<ProcessUtilization> result;
        std::vector<ProcessCpuData> data0 = get_processes_cpu_data();
        Glib::usleep(seconds * 1000000);
        size_t pid_time = 0;
        for (const auto d0 : data0) {
            ProcessCpuData d1 = get_cpu_data_from_file(d0.filename);
            size_t time = d1.utime + d1.stime + d1.cutime + d1.cstime -
                d0.utime - d0.stime - d0.cutime - d0.cstime;
            if (time > 0) {
                if (d0.pid == pid) {
                    pid_time = time;
                } else {
                    auto total_time = d1.time - d0.time;
                    ProcessUtilization p;
                    p.name = d0.name;
                    p.utilization = 100.0 * time;
                    p.total_time = total_time;
                    result.push_back(p);
                }
            }
        }
        // Correct by total_time with pid substracted
        for (auto &r : result) {
            r.utilization /= r.total_time - pid_time;
        }
        auto iter = result.end();
        auto size = result.size();
        if (result.size() > number) {
            iter = result.begin() + number;
            size = number;
        }
        std::partial_sort(result.begin(), iter, result.end(),
                  [](const ProcessUtilization &a, const ProcessUtilization &b) -> bool {
                      return a.utilization > b.utilization;
                  });
        result.resize(size);

        return result;
    }

    Glib::Property<double> cpu_load;

private:
    ProcessCpuData get_cpu_data_from_file(const std::string &filename) const {
        ProcessCpuData p;
        p.filename = filename;
        std::ifstream proc_stat(filename);
        // https://docs.kernel.org/filesystems/proc.html
        proc_stat >> p.pid;
        // Parse p.name: it may contain spaces, so read everything between ()
        char c;
        bool in_name = true;
        int par = 0;
        std::string name;
        while (in_name && proc_stat.read(&c, 1)) {
            if (c == '(') {
                ++par;
            } else if (c == ')') {
                par--;
                if (par == 0)
                    in_name = false;
            } else {
                name.push_back(c);
            }
        }
        if (!in_name) {
            p.name = name;
            proc_stat >> p.state >> p.ppid;
            for (int i = 0; i < 9; ++i) {
                size_t skip;
                proc_stat >> skip;
            }
            proc_stat >> p.utime >> p.stime >> p.cutime >> p.cstime;
            // Store timestamp of read
            p.time = get_total_time();
        } else {
            Utils::log(Utils::LogSeverity::ERROR, std::format("CpuMonitor: error getting CPU data for process {}", name));
        }
        proc_stat.close();
        return p;
    }
    std::vector<ProcessCpuData> get_processes_cpu_data() const {
        std::vector<ProcessCpuData> result;

        // For every directory with a name that is a number (PID), load
        // /proc/PID/stat
        auto cwd = Gio::File::create_for_path("/proc");

        auto files = cwd->enumerate_children(
            "standard::*", Gio::FileQueryInfoFlags::NOFOLLOW_SYMLINKS);

        for (auto fileinfo = files->next_file(); fileinfo != nullptr; fileinfo = files->next_file()) {
            auto file = files->get_child(fileinfo);
            if (file->query_file_type() == Gio::FileType::DIRECTORY) {
                std::string s = file->get_basename();
                if (!s.empty() && s.find_first_not_of("0123456789") == std::string::npos) {
                    // Found a process directory
                    ProcessCpuData p = get_cpu_data_from_file(file->get_path() + "/stat");
                    result.push_back(p);
                }
            }
        }
        return result;
    }
    size_t get_total_time() const {
        std::ifstream proc_stat("/proc/stat");
        proc_stat.ignore(5, ' '); // Skip the 'cpu' prefix.

        std::vector<size_t> times;
        for (size_t time; proc_stat >> time; times.push_back(time))
            ;
        // Accumulate only first four values, and skip IOWait, IRQ and SoftIRQ
        auto total_time = std::accumulate(times.begin(), times.begin() + 4, 0);

        return total_time;
    }

    size_t prev_idle_time, prev_total_time;
    sigc::connection timer;
};

static CpuMonitor *cpu_monitor = nullptr;

CpuGraph::CpuGraph(int seconds, int hist, const Color &col)
    : history(hist), color(col), dispatcher(), sem(0), working(false)
{
    if (cpu_monitor == nullptr) {
        cpu_monitor = new CpuMonitor(seconds);
    }
    worker_thread = std::make_shared<std::thread>(&CpuGraph::working_thread, this);
    worker_thread->detach();
    dispatcher.connect(
        [this]() {
            mtx.lock();
            auto result = shared_data.result;
            mtx.unlock();
            button.set_tooltip_text(Utils::trim_end(result));
        });

    add_css_class("cpu-monitor");
    drawing = Glib::make_refptr_for_instance(Gtk::make_managed<Graph>(cpu_monitor->cpu_load, history, color));
    button.set_child(*drawing);
    bind_property_changed(cpu_monitor, "cpu-load", [this]() {
        double d = cpu_monitor->cpu_load.get_value();
        label.set_text(std::format("{}%", static_cast<int>(std::round(d))));
    });
    button.signal_clicked().connect([this]() {
        Utils::spawn("kitty btop");
    });
    hover = Gtk::EventControllerMotion::create();
    hover->signal_enter().connect([this](double x, double y) {
        if (!working)
            sem.release();
    }, true);
    add_controller(hover);
    append(label);
    append(button);
}

void CpuGraph::working_thread()
{
    const size_t number = 10;
    const double seconds = 0.3;
    while (true) {
        sem.acquire();
        working = true;
        std::vector<ProcessUtilization> cpu_use = cpu_monitor->get_processes_cpu_utilization(seconds, number);
        Glib::ustring result;
        size_t len = std::min(number, cpu_use.size());
        if (len > 0) {
            for (size_t i = 0; i < len; ++i) {
                result += std::format("{:5.1f}% {}\n", cpu_use[i].utilization,
                                      cpu_use[i].name);
            }
        } else {
            result = "-";
        }
        mtx.lock();
        shared_data.result = result;
        mtx.unlock();
        dispatcher.emit();
        working = false;
    }
}


// Memory

typedef struct {
    std::string name;
    size_t mem_used;
} ProcessMemData;

class MemMonitor : public Glib::Object {
public:
    MemMonitor(int seconds)
        : Glib::ObjectBase(typeid(MemMonitor)),
          mem_load(*this, "memory-load", 0.0), mem_used(*this, "memory-used", 0),
          mem_available(0), mem_total(0) {
        timer = Glib::signal_timeout().connect_seconds(
            [this]() {
                if (read_data()) {
                    mem_load.set_value(std::round(
                        (100.0 * (mem_total - mem_available)) / mem_total));
                    mem_used.set_value(mem_total - mem_available);
                    return true;
                } else {
                    mem_load.set_value(0.0);
                    mem_used.set_value(0);
                    return false;
                }
            },
            seconds);
    }
    Glib::Property<double> mem_load;
    Glib::Property<size_t> mem_used;

    std::vector<ProcessMemData> get_processes_mem_data(int number = 10) {
        std::vector<ProcessMemData> result;

        // For every directory with a name that is a number (PID), load
        // /proc/PID/status
        auto cwd = Gio::File::create_for_path("/proc");

        auto files = cwd->enumerate_children("standard::*", Gio::FileQueryInfoFlags::NOFOLLOW_SYMLINKS);

        for (auto fileinfo = files->next_file(); fileinfo != nullptr; fileinfo = files->next_file()) {
            auto file = files->get_child(fileinfo);
            if (file->query_file_type() == Gio::FileType::DIRECTORY) {
                std::string s = file->get_basename();
                if (!s.empty() && s.find_first_not_of("0123456789") == std::string::npos) {
                    // Found a process directory
                    ProcessMemData p = { "", 0 };
                    std::ifstream proc_status(file->get_path() + "/status");
                    std::string line;
                    while (getline(proc_status, line)) {
                        std::istringstream ss(line);
                        std::string field;
                        ss >> field;
                        if (field == "Name:") {
                            ss >> p.name;
                        } else if (field == "VmRSS:") {
                            ss >> p.mem_used;
                        }
                        if (p.name != "" && p.mem_used > 0) {
                            break;
                        }
                    }
                    proc_status.close();
                    result.push_back(p);
                }
            }
        }
        auto iter = result.end();
        auto size = result.size();
        if (result.size() > number) {
            iter = result.begin() + number;
            size = number;
        }
        std::partial_sort(result.begin(), iter, result.end(),
                  [](const ProcessMemData &a, const ProcessMemData &b) -> bool {
                      return a.mem_used > b.mem_used;
                  });
        result.resize(size);

        return result;
    }

private:
    bool read_data() {
        mem_total = 0;
        mem_available = 0;
        std::ifstream proc_meminfo("/proc/meminfo");
        std::string line;
        while (getline(proc_meminfo, line)) {
            std::istringstream ss(line);
            std::string name;
            ss >> name;
            if (name == "MemTotal:") {
                ss >> mem_total;
            } else if (name == "MemAvailable:") {
                ss >> mem_available;
            }
            if (mem_total > 0 && mem_available > 0)
                return true;
        }
        Utils::log(Utils::LogSeverity::ERROR, "MemMonitor: /proc/meminfo doesn't contain all the needed information");
        return false;
    }

    size_t mem_available, mem_total;
    sigc::connection timer;
};


static MemMonitor *mem_monitor = nullptr;

MemGraph::MemGraph(int seconds, int hist, const Color &col)
    : history(hist), color(col)
{
    if (mem_monitor == nullptr) {
        mem_monitor = new MemMonitor(seconds);
    }
    add_css_class("mem-monitor");
    drawing = Glib::make_refptr_for_instance(Gtk::make_managed<Graph>(mem_monitor->mem_load, history, color));
    button.set_child(*drawing);
    bind_property_changed(mem_monitor, "memory-load", [this]() {
        double d = mem_monitor->mem_load.get_value();
        label.set_text(std::format("{}%", static_cast<int>(std::round(d))));
    });
    button.signal_clicked().connect([this]() {
        Utils::spawn("kitty btop");
    });
    hover = Gtk::EventControllerMotion::create();
    hover->signal_enter().connect([this](double x, double y) {
        std::vector<ProcessMemData> mem_use = mem_monitor->get_processes_mem_data();
        Glib::ustring result = std::format("{:6.1f} GB used\n\n", std::round(mem_monitor->mem_used.get_value() / 1024.0 / 1024.0));
        size_t len = std::min(static_cast<size_t>(10), mem_use.size());
        for (size_t i = 0; i < len; ++i) {
            result += std::format("{:8.0f} MB  {}\n", std::round(mem_use[i].mem_used / 1024.0), mem_use[i].name);
        }
        button.set_tooltip_text(Utils::trim_end(result));
    });
    add_controller(hover);
    append(label);
    append(button);
}


// Nvidia GPU

class GpuMonitor : public Glib::Object {
public:
    GpuMonitor(int seconds)
        : Glib::ObjectBase(typeid(GpuMonitor)),
          gpu_load(*this, "gpu-load", 0), gpu_mem_load(*this, "gpu-memory-load", 0.0), gpu_mem_used(*this, "gpu-memory-used", 0), gpu_mem_total(*this, "gpu-memory-total", 0),
          mem_used(0), mem_total(0) {
        timer = Glib::signal_timeout().connect_seconds(
            [this]() {
                if (read_data()) {
                    gpu_load.set_value(utilization);
                    gpu_mem_load.set_value(std::round(100.0 * mem_used / mem_total));
                    gpu_mem_used.set_value(mem_used);
                    gpu_mem_total.set_value(mem_total);
                    return true;
                } else {
                    gpu_load.set_value(0);
                    gpu_mem_load.set_value(0.0);
                    gpu_mem_used.set_value(0);
                    gpu_mem_total.set_value(0);
                    return false;
                }
            },
            seconds);
    }
    Glib::Property<int> gpu_load;
    Glib::Property<double> gpu_mem_load;
    Glib::Property<int> gpu_mem_used;
    Glib::Property<int> gpu_mem_total;

private:
    bool read_data() {
        const auto stats = Utils::split(Utils::exec("nvidia-smi -i 0 --query-gpu=memory.total,memory.used,utilization.gpu --format=csv,noheader,nounits"), ",");
        if (stats.size() < 3) {
            Utils::log(Utils::LogSeverity::ERROR, "GpuMonitor: nvidia-smi doesn't return all the needed information");
            return false;
        }
        mem_total = std::atoi(stats[0].c_str());
        mem_used = std::atoi(stats[1].c_str());
        utilization = std::atoi(stats[2].c_str());
        return true;
    }

    int utilization, mem_used, mem_total;
    sigc::connection timer;
};

static GpuMonitor *gpu_monitor = nullptr;

GpuGraph::GpuGraph(int seconds, int hist, const Color &col_gpu, const Color &col_mem)
    : history(hist), color_gpu(col_gpu), color_mem(col_mem)
{
    if (gpu_monitor == nullptr) {
        gpu_monitor = new GpuMonitor(seconds);
    }
    add_css_class("gpu-monitor");
    drawing = Glib::make_refptr_for_instance(Gtk::make_managed<Graph>(gpu_monitor->gpu_mem_load, history, color_mem));
    button.set_child(*drawing);
    bind_property_changed(gpu_monitor, "gpu-memory-load", [this]() {
        double d = gpu_monitor->gpu_mem_load.get_value();
        label.set_text(std::format("{}%", static_cast<int>(std::round(d))));
    });
    button.signal_clicked().connect([this]() {
        Utils::spawn("kitty nvtop");
    });
    hover = Gtk::EventControllerMotion::create();
    hover->signal_enter().connect([this](double x, double y) {
        Glib::ustring result = std::format("{}% utilization\n{} MB used of {} MB total", gpu_monitor->gpu_load.get_value(), gpu_monitor->gpu_mem_used.get_value(), gpu_monitor->gpu_mem_total.get_value());
        button.set_tooltip_text(result);
    });
    add_controller(hover);
    label.add_css_class("gpu-monitor-mem");
    append(label);
    append(button);
}

