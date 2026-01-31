#pragma once
#include <chrono>

namespace NoMoreDay::utils {

/**
 * @brief Global time utility to reduce clock system calls.
 * Should be updated once per frame in the main loop.
 */
class Time {
public:
    static void Update() {
        static const auto startTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        s_currentSeconds = std::chrono::duration<float>(now - startTime).count();
    }

    /// Returns the cached time in seconds since start
    static float GetCurrentSeconds() noexcept {
        return s_currentSeconds;
    }

private:
    inline static float s_currentSeconds = 0.0f;
};

} // namespace NoMoreDay::utils
