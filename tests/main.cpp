#define DOCTEST_CONFIG_IMPLEMENT
#include "TestCommon.hpp"
#include "doctest.h"
#include <raylib.h>

using namespace NoMoreDay;

// Merged Test Groups
#include "AstrolabeTests.hpp"
#include "BuffTests.hpp"
#include "CombatSystemTests.hpp"
#include "ItemEquipmentTests.hpp"
#include "SkillSystemTests.hpp"
#include "UITests.hpp"

// System and World Tests
#include "ResonanceCalculatorTest.hpp"
#include "WorldSystemTests.hpp"

// Engine and Tech Tests
#include "EngineTechTests.hpp"

// Integration and Benchmarks
#include "FinalIntegrationTest.hpp"
#include "GPUFlowFieldTest.hpp"
#include "NemesisSystemTests.hpp"
#include "StatsBenchmark.cpp"
#include "TestBladeBoomerang.cpp"
#include "TestBladeFormation.cpp"
#include "TestBladeWard.cpp"
#include "TestDefenseMechanics.cpp"
#include "TestEternalNightmare.cpp"
#include "TestHeirloomSystem.cpp"
#include "TestLegendaryInfrastructure.cpp"
#include "TestPersistence.cpp"
#include "TestPolishSystems.cpp"
#include "TestPortalSystem.cpp"
#include "TestRendingWave.cpp"
#include "TestShadowSystem.cpp"
#include "TestSkillBehaviors.cpp"
#include "TestSwordIntentAccumulation.cpp"


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