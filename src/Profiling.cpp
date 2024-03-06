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

size_t prof::UnitExecutionTime::measurement_count() {
    return m_measurements->size();
}

prof::Stats prof::UnitExecutionTime::stats() const {
    Stats result {};
    // calculate sum
    auto measurements = m_measurements.synchronize();
    if (measurements->size() == 0) {
        return result;
    }
    result.n = measurements->size();
    result.max = std::numeric_limits<double>::min();
    result.min = std::numeric_limits<double>::max();
    Duration sum {};
    for (const auto& measurement : *measurements) {
        if (measurement.count() > result.max) {
            result.max = measurement.count();
        }
        if (measurement.count() < result.min) {
            result.min = measurement.count();
        }
        sum += measurement;
    }
    // calculate mean
    result.mean = (sum / measurements->size()).count();
    // calculate stddev
    result.stddev = 0;
    for (const auto& measurement : *measurements) {
        // (measurements[i] - mean)^2
        result.stddev += std::pow(measurement.count() - result.mean, 2);
    }
    result.stddev = std::sqrt(result.stddev / double(measurements->size()));
    result.total_calls = m_total_calls;
    return result;
}

void prof::UnitExecutionTime::add_sample(const Duration& dur) {
    m_measurements->push_back(dur);
    ++m_total_calls;
}

prof::UnitExecutionTime::UnitExecutionTime()
    : m_measurements(boost::circular_buffer<Duration>(100)) {
}

std::unordered_map<std::string, prof::Stats> prof::UnitProfileCollection::all_stats() {
    auto map = m_map.synchronize();
    std::unordered_map<std::string, Stats> result {};
    for (const auto& [name, time] : *map) {
        result[name] = time.stats();
    }
    return result;
}
