#include "Logger.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>
#include <vector>

namespace tools {

std::shared_ptr<spdlog::logger> Logger::s_CoreLogger;

void Logger::Init() {
    // 1. Initialize Async Thread Pool (Queue size 8192, 1 backing thread)
    spdlog::init_thread_pool(8192, 1);

    // 2. Create Sinks
    std::vector<spdlog::sink_ptr> sinks;

    // Sink 1: Console (Color)
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::trace);
    // Pattern: [Time] [LoggerName] [Level] Message
    consoleSink->set_pattern("%^[%T] %n: %v%$"); 
    sinks.push_back(consoleSink);

    // Sink 2: File (Async)
    // We put logs in a "logs" directory relative to execution
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/NoMoreDay.log", true);
    fileSink->set_level(spdlog::level::trace); // File gets everything
    fileSink->set_pattern("[%Y-%m-%d %T] [%l] %v");
    sinks.push_back(fileSink);

    // 3. Create Logger with Sinks
    s_CoreLogger = std::make_shared<spdlog::async_logger>(
        "NMD", 
        sinks.begin(), 
        sinks.end(), 
        spdlog::thread_pool(), 
        spdlog::async_overflow_policy::block
    );

    spdlog::register_logger(s_CoreLogger);

    // 4. Set Global Level based on requirements
    // For now, file logging level is effectively controlled here or via sink levels.
    // The prompt requested file logging to be OFF for now, or just controlled.
    // We will set the fileSink level to OFF as requested, but keep the console active.
    
    // User request: "file logs set to off level for now, only console logs"
    fileSink->set_level(spdlog::level::off); 
    consoleSink->set_level(spdlog::level::trace);
    s_CoreLogger->set_level(spdlog::level::trace);
    
    s_CoreLogger->flush_on(spdlog::level::trace);
}

void Logger::Shutdown() {
    spdlog::shutdown();
}

std::shared_ptr<spdlog::logger>& Logger::GetCoreLogger() {
    return s_CoreLogger;
}

} // namespace tools
