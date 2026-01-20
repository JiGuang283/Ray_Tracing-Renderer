#ifndef PERFORMANCEMONITOR_H
#define PERFORMANCEMONITOR_H

#include <deque>
#include <string>

class PerformanceMonitor {
public:
    struct Metrics {
        double cpu_ms = 0.0;
        double upload_ms = 0.0;
        double render_ms = 0.0;
        double fps = 0.0;
    };

    PerformanceMonitor(size_t history_size = 60);

    void update(const Metrics& metrics);
    Metrics get_average() const;

    std::string get_report() const;
    bool should_report(double current_time, double interval = 1.0) const;
    void mark_reported(double current_time);

private:
    std::deque<Metrics> history_;
    size_t max_history_size_;
    double last_report_time_ = 0.0;
};

#endif //PERFORMANCEMONITOR_H
