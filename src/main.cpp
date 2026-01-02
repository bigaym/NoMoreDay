#include "core/Game.hpp"
#include "tools/Logger.hpp"
#include "tools/CrashHandler.hpp"

int main() {
    NoMoreDay::CrashHandler::Init(); // 初始化崩溃捕获
    tools::Logger::Init();
    LOG_INFO("Initializing NoMoreDay Engine...");
    
    {
        // 确保 Game 对象在 Logger::Shutdown 之前析构
        // Create Game Instance
        Game game(2560, 1440, "NoMoreDay - High Performance ECS");
        
        // Run Loop
        game.run();
    }

    LOG_INFO("Engine Shutdown.");
    tools::Logger::Shutdown();
    return 0;
}