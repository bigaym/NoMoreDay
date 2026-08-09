#define DOCTEST_CONFIG_IMPLEMENT
#include "TestCommon.hpp"
#include "doctest.h"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include <raylib.h>

#include <filesystem>
#include <iostream>

using namespace NoMoreDay;

namespace fs = std::filesystem;

void AnchorWorkingDirectory() {
    fs::path current = fs::current_path();
    // Try to find 'assets' in current or parent directories (up to 3 levels)
    for (int i = 0; i < 4; ++i) {
        if (fs::exists(current / "assets")) {
            fs::current_path(current);
            std::cout << "[Test] Working directory anchored to: " << current.string() << std::endl;
            return;
        }
        if (current.has_parent_path()) {
            current = current.parent_path();
        } else {
            break;
        }
    }
    std::cerr << "[Test] Warning: Could not find 'assets' folder in search path!" << std::endl;
}

int main(int argc, char **argv) {
  printf("Test runner starting...\n");
  AnchorWorkingDirectory();
  
  // Set Raylib log level to Warning to suppress INFO logs
  SetTraceLogLevel(LOG_WARNING);

  // 设置日志级别为warning
  tools::Logger::Init();
  tools::Logger::SetLogLevel(spdlog::level::warn, 2);
  
  // Force global spdlog level just in case
  spdlog::set_level(spdlog::level::warn);

  // Some Raylib functions require a window context even if not drawing
  InitWindow(100, 100, "Headless Tests");
  SetTargetFPS(60);

  // Initialize GPU Utilities
  NoMoreDay::utils::GPUUtils::Initialize();

  doctest::Context context;
  context.applyCommandLine(argc, argv);
  int res = context.run();

  tools::Logger::Shutdown();

  // Release GPU-backed singletons while the GL context is alive (mirrors RenderSystem::Shutdown).
  render::lighting::LightManager::Get().Shutdown();

  CloseWindow();
  return res;
}
