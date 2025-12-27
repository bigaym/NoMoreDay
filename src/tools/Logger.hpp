#pragma once

#include <memory>
#include <chrono>

// Start: Spdlog includes
// Define SPDLOG_ACTIVE_LEVEL to control compile-time log stripping if needed
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h> 
// End: Spdlog includes

namespace tools {

class Logger {
public:
    static void Init();
    static void Shutdown();
    static std::shared_ptr<spdlog::logger>& GetCoreLogger();

private:
    static std::shared_ptr<spdlog::logger> s_CoreLogger;
};

} // namespace tools

// --- Core Log Macros ---
// Usage: LOG_INFO("Message with param: {}", 42);

#define LOG_TRACE(...)    ::tools::Logger::GetCoreLogger()->trace(__VA_ARGS__)
#define LOG_INFO(...)     ::tools::Logger::GetCoreLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)     ::tools::Logger::GetCoreLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    ::tools::Logger::GetCoreLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::tools::Logger::GetCoreLogger()->critical(__VA_ARGS__)

// --- Rate Limited Log Macros ---
// Usage: LOG_LIMITED_WARN(5.0f, "This warns at most once every 5 seconds");
// interval_seconds: float/double representing seconds

#define LOG_LIMITED_WARN(interval_seconds, ...) \
    do { \
        static auto last_log_time_##__LINE__ = std::chrono::steady_clock::time_point::min(); \
        auto now_##__LINE__ = std::chrono::steady_clock::now(); \
        std::chrono::duration<float> diff_##__LINE__ = now_##__LINE__ - last_log_time_##__LINE__; \
        if (diff_##__LINE__.count() >= interval_seconds) { \
            LOG_WARN(__VA_ARGS__); \
            last_log_time_##__LINE__ = now_##__LINE__; \
        } \
    } while(0)

#define LOG_LIMITED_ERROR(interval_seconds, ...) \
    do { \
        static auto last_log_time_##__LINE__ = std::chrono::steady_clock::time_point::min(); \
        auto now_##__LINE__ = std::chrono::steady_clock::now(); \
        std::chrono::duration<float> diff_##__LINE__ = now_##__LINE__ - last_log_time_##__LINE__; \
        if (diff_##__LINE__.count() >= interval_seconds) { \
            LOG_ERROR(__VA_ARGS__); \
            last_log_time_##__LINE__ = now_##__LINE__; \
        } \
    } while(0)
