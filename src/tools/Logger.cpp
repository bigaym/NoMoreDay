#include "Logger.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>
#include <vector>

namespace tools {

std::shared_ptr<spdlog::logger> Logger::s_CoreLogger;

void Logger::Init() {
    // 1. Initialize Async Thread Pool (Queue size 8192, 1 backing thread)
    // 1. 初始化异步线程池（队列大小 8192，1 个后台线程）
    spdlog::init_thread_pool(8192, 1);

    // 2. Create Sinks
    // 2. 创建接收器
    std::vector<spdlog::sink_ptr> sinks;

    // Sink 1: Console (Color)
    // 接收器 1：控制台（彩色）
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::trace);
    // Pattern: [Time] [LoggerName] [Level] Message
    // 模式：[时间] [日志器名称] [级别] 消息
    consoleSink->set_pattern("%^[%T] %n: %v%$"); 
    sinks.push_back(consoleSink);

    // Sink 2: File (Async)
    // We put logs in a "logs" directory relative to execution
    // 接收器 2：文件（异步）
    // 我们将日志放在相对于执行路径的 "logs" 目录中
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/NoMoreDay.log", true);
    fileSink->set_level(spdlog::level::trace); // 文件接收所有日志
    fileSink->set_pattern("[%Y-%m-%d %T] [%l] %v");
    sinks.push_back(fileSink);

    // 3. Create Logger with Sinks
    // 3. 使用接收器创建日志器
    s_CoreLogger = std::make_shared<spdlog::async_logger>(
        "NMD", 
        sinks.begin(), 
        sinks.end(), 
        spdlog::thread_pool(), 
        spdlog::async_overflow_policy::block
    );
    spdlog::register_logger(s_CoreLogger);
    // 4. 根据要求设置全局级别
    // 目前，文件日志级别在此处或通过接收器级别进行有效控制。
    // 提示要求文件日志暂时关闭，或仅受控制。
    // 我们将文件接收器级别设置为 OFF，但保持控制台活动。
    // 用户请求："文件日志暂时设置为关闭级别，只保留控制台日志"
    fileSink->set_level(spdlog::level::off); 
    consoleSink->set_level(spdlog::level::trace);
    s_CoreLogger->set_level(spdlog::level::trace);
    
    s_CoreLogger->flush_on(spdlog::level::trace);
}

void Logger::Shutdown() {
    spdlog::shutdown();
}

std::shared_ptr<spdlog::logger>& Logger::GetCoreLogger() {
    if (!s_CoreLogger) {
        // Fallback to a simple console logger if not initialized
        // This prevents crashes in tests or early boot
        s_CoreLogger = spdlog::stdout_color_mt("NMD_FALLBACK");
        s_CoreLogger->set_level(spdlog::level::trace);
    }
    return s_CoreLogger;
}

} // namespace tools
