#include "app/Game.hpp"
#include "core/logging/Logger.hpp"
#include "core/logging/CrashHandler.hpp"

// Diagnostic build: Force asset sync for particle fix
#if defined(_WIN32) && defined(__GNUC__)
#include <windows.h>
extern int main(int argc, char* argv[]);
extern "C" int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return main(__argc, __argv);
}
#endif

int main(int argc, char* argv[]) {
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