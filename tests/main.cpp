#define DOCTEST_CONFIG_IMPLEMENT
#include "TestCommon.hpp"
#include "doctest.h"
#include <raylib.h>

using namespace NoMoreDay;

// --- Unit Tests ---
#include "unit/BarrierTests.hpp"
#include "unit/BuffTests.hpp"
#include "unit/HazardSystemTests.hpp"
#include "unit/MonsterAffixTests.hpp"
#include "unit/NemesisEvolutionTests.hpp"
#include "unit/SystemMechanics.hpp"


// --- Integration Tests ---
#include "integration/GameplaySystems.hpp"
#include "integration/SkillSystemTests.hpp"

// --- Functional Tests ---
#include "functional/SkillBehaviors.hpp"

// --- Tech & Engine Tests ---
#include "tech/EngineTechTests.hpp"
#include "tech/GPUFlowFieldTest.hpp"
#include "tech/UITests.hpp"

// --- Performance Benchmarks ---
#include "performance/DropSystemBenchmark.hpp"
#include "performance/StatsBenchmark.hpp"

#if defined(_WIN32) && defined(__GNUC__)
#include <windows.h>
extern int main(int argc, char **argv);
extern "C" int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                              LPSTR lpCmdLine, int nCmdShow) {
  return main(__argc, __argv);
}
#endif

int main(int argc, char **argv) {
  // 设置日志级别为warning
  tools::Logger::Init();
  tools::Logger::SetLogLevel(spdlog::level::warn, 2);

  // Some Raylib functions require a window context even if not drawing
  InitWindow(100, 100, "Headless Tests");
  SetTargetFPS(60);

  doctest::Context context;
  context.applyCommandLine(argc, argv);
  int res = context.run();

  tools::Logger::Shutdown();
  CloseWindow();
  return res;
}