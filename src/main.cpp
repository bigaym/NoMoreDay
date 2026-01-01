#include "core/Game.hpp"
#include "tools/Logger.hpp"

int main() {
    tools::Logger::Init();
    LOG_INFO("Initializing NoMoreDay Engine...");
    
    // Create Game Instance
    // Using a pointer or stack? Stack is fine for Game class.
    Game game(2560, 1440, "NoMoreDay - High Performance ECS");
    
    // Run Loop
    game.run();

    LOG_INFO("Engine Shutdown.");
    tools::Logger::Shutdown();
    return 0;
}