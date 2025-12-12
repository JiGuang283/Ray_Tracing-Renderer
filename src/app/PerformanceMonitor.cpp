#include "PerformanceMonitor.h"
#include <sstream>
#include <iomanip>

PerformanceMonitor::PerformanceMonitor(size_t history_size)
    : max_history_size_(history_size) {}

void PerformanceMonitor::update(const Metrics& metrics) {
    history_.push_back(metrics);
    if (history_.size() > max_history_size_) {
        history_.pop_front();
    }
}

PerformanceMonitor::Metrics PerformanceMonitor::get_average() const {
    if (history_.empty()) {
        return Metrics{};
    }

    Metrics avg{};
    for (const auto& m : history_) {
        avg.cpu_ms += m.cpu_ms;
        avg.upload_ms += m.upload_ms;
        avg.render_ms += m.render_ms;
        avg.fps += m.fps;
    }

    double count = static_cast<double>(history_.size());
    avg.cpu_ms /= count;
    avg.upload_ms /= count;
    avg.render_ms /= count;
    avg.fps /= count;

    return avg;
}

std::string PerformanceMonitor::get_report() const {
    auto avg = get_average();
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "CPU: " << avg.cpu_ms << "ms, "
        << "Upload: " << avg.upload_ms << "ms, "
        << "Render: " << avg.render_ms << "ms, "
        << "FPS: " << avg.fps;
    return oss.str();
}

bool PerformanceMonitor::should_report(double current_time, double interval) const {
    return (current_time - last_report_time_) >= interval;
}

void PerformanceMonitor::mark_reported(double current_time) {
    last_report_time_ = current_time;
}
