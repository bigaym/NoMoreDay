#include "core/logging/Logger.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>
#include <vector>
#include <filesystem>

namespace tools
{

    std::shared_ptr<spdlog::logger> Logger::s_CoreLogger;

    void Logger::Init()
    {
        if (s_CoreLogger && s_CoreLogger->name() == "NMD")
        {
            return;
        }

        // 0. Ensure logs directory exists
        if (!std::filesystem::exists("logs"))
        {
            std::filesystem::create_directory("logs");
        }

        // 1. Initialize Async Thread Pool
        try
        {
            spdlog::init_thread_pool(8192, 1);
        }
        catch (...)
        {
        }

        // 2. Create Sinks
        std::vector<spdlog::sink_ptr> sinks;

        // Sink 1: Console (Color)
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_level(spdlog::level::info);
        consoleSink->set_pattern("%^[%T] %n: %v%$");
        sinks.push_back(consoleSink);

        // Sink 2: File (Async)
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/NoMoreDay.log", true);
        fileSink->set_level(spdlog::level::debug);
        fileSink->set_pattern("[%Y-%m-%d %T] [%l] %v");
        sinks.push_back(fileSink);

        // 3. Create Logger with Sinks
        s_CoreLogger = std::make_shared<spdlog::async_logger>(
            "NMD",
            sinks.begin(),
            sinks.end(),
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::block);

        try
        {
            spdlog::register_logger(s_CoreLogger);
            spdlog::set_default_logger(s_CoreLogger); // Fix: Ensure spdlog::info/error uses our logger
        }
        catch (...)
        {
        }

        s_CoreLogger->set_level(spdlog::level::trace);
        s_CoreLogger->flush_on(spdlog::level::warn);
    }

    void Logger::Shutdown()
    {
        if (s_CoreLogger)
        {
            s_CoreLogger->flush();
            s_CoreLogger.reset(); // 释放 shared_ptr，配合 GetCoreLogger 的 fallback 机制
        }
        spdlog::shutdown();
    }

    std::shared_ptr<spdlog::logger> &Logger::GetCoreLogger()
    {
        if (!s_CoreLogger)
        {
            // Fallback to a simple console logger if not initialized
            // This prevents crashes in tests or early boot
            s_CoreLogger = spdlog::stdout_color_mt("NMD_FALLBACK");
            s_CoreLogger->set_level(spdlog::level::debug);
        }
        return s_CoreLogger;
    }

    void Logger::SetLogLevel(spdlog::level::level_enum level, uint8_t mode)
    {
        auto &sinks = GetCoreLogger()->sinks();

        try
        {
            switch (mode)
            {
            case 0:
                if (sinks.size() > 0) sinks.at(0)->set_level(level);
                break;
            case 1:
                if (sinks.size() > 1) sinks.at(1)->set_level(level);
                break;
            default:
                if (sinks.size() > 0) sinks.at(0)->set_level(level);
                if (sinks.size() > 1) sinks.at(1)->set_level(level);
                GetCoreLogger()->set_level(level);
                break;
            }
        }
        catch (const std::exception &e)
        {
            // std::cerr << e.what() << '\n';
            GetCoreLogger()->error("Failed to set log level: {}", e.what());
        }

        // for (auto& sink : sinks) {
        // GetCoreLogger()->set_level(level);
    }

} // namespace tools
