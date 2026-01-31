#pragma once
#include <chrono>
#include <string>
#include "core/logging/Logger.hpp"

namespace NoMoreDay::utils {

class ScopedTimer {
public:
    ScopedTimer(const std::string& name, int64_t threshold_us = 10) : m_name(name), m_threshold(threshold_us), m_start(std::chrono::high_resolution_clock::now()) {}
    ~ScopedTimer() {
        // auto end = std::chrono::high_resolution_clock::now();
        // auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_start).count();
        // // Log in microseconds for readability
        // double micros = duration / 1000.0;
        // if (micros > m_threshold) { // Only log if significant (>threshold_us) to avoid spamming too much
        //      LOG_DEBUG("[TIMER] {} took {:.3f} us", m_name, micros);
        // }
    }

private:
    std::string m_name;
    int64_t m_threshold;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
};

} // namespace NoMoreDay::utils
