#include "Profiling.h"

prof::Duration prof::duration(const TimePoint& start, const TimePoint& end) {
    return end - start;
}
prof::TimePoint prof::now() {
    return std::chrono::high_resolution_clock::now();
}
prof::Duration prof::UnitProfileCollection::average_duration(const std::string& unit) {
    return m_map->operator[](unit).average_duration();
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

prof::Duration prof::UnitExecutionTime::average_duration() const {
    auto measurements = m_measurements.synchronize();
    Duration sum {};
    for (const auto& measurement : *measurements) {
        sum += measurement;
    }
    return sum / measurements->size();
}

void prof::UnitExecutionTime::add_sample(const Duration& dur) {
    m_measurements->push_back(dur);
}

prof::UnitExecutionTime::UnitExecutionTime()
    : m_measurements(boost::circular_buffer<Duration>(16)) {
}

