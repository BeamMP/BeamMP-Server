#include "Profiling.h"
#include <limits>

prof::Duration prof::duration(const TimePoint& start, const TimePoint& end) {
    return end - start;
}
prof::TimePoint prof::now() {
    return std::chrono::high_resolution_clock::now();
}
prof::Stats prof::UnitProfileCollection::stats(const std::string& unit) {
    return m_map->operator[](unit).stats();
}

size_t prof::UnitProfileCollection::measurement_count(const std::string& unit) {
    return m_map->operator[](unit).measurement_count();
}

void prof::UnitProfileCollection::add_sample(const std::string& unit, const Duration& duration) {
    m_map->operator[](unit).add_sample(duration);
}

prof::Stats prof::UnitExecutionTime::stats() const {
    std::unique_lock lock(m_mtx);
    Stats result {};
    // calculate sum
    result.n = m_total_calls;
    result.max = m_min;
    result.min = m_max;
    // calculate mean: mean = sum_x / n
    result.mean = m_sum / double(m_total_calls);
    // calculate stdev: stdev = sqrt((sum_x2 / n) - (mean * mean))
    result.stdev = std::sqrt((m_measurement_sqr_sum / double(result.n)) - (result.mean * result.mean));
    return result;
}

void prof::UnitExecutionTime::add_sample(const Duration& dur) {
    std::unique_lock lock(m_mtx);
    m_sum += dur.count();
    m_measurement_sqr_sum += dur.count() * dur.count();
    m_min = std::min(dur.count(), m_min);
    m_max = std::max(dur.count(), m_max);
    ++m_total_calls;
}

prof::UnitExecutionTime::UnitExecutionTime() {
}

std::unordered_map<std::string, prof::Stats> prof::UnitProfileCollection::all_stats() {
    auto map = m_map.synchronize();
    std::unordered_map<std::string, Stats> result {};
    for (const auto& [name, time] : *map) {
        result[name] = time.stats();
    }
    return result;
}
size_t prof::UnitExecutionTime::measurement_count() const {
    std::unique_lock lock(m_mtx);
    return m_total_calls;
}

