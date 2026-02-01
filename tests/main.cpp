#define DOCTEST_CONFIG_IMPLEMENT
#include "TestCommon.hpp"
#include "doctest.h"
#include "engine/render/GPUUtils.hpp"
#include <raylib.h>

using namespace NoMoreDay;

#if defined(_WIN32) && defined(__GNUC__)
#include <windows.h>
extern int main(int argc, char **argv);
extern "C" int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                              LPSTR lpCmdLine, int nCmdShow) {
  return main(__argc, __argv);
}
#endif

int main(int argc, char **argv) {
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
  CloseWindow();
  return res;
}
