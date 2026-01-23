#define DOCTEST_CONFIG_IMPLEMENT
#include "TestCommon.hpp"
#include "doctest.h"
#include <raylib.h>

using namespace NoMoreDay;

// --- Unit Tests ---
#include "unit/BarrierTests.hpp"
#include "unit/BuffTests.hpp"
#include "unit/CombatFormulaTest.hpp"
#include "unit/GroupLayoutTest.hpp"
#include "unit/HazardSystemTests.hpp"
#include "unit/MonsterAffixTests.hpp"
#include "unit/NemesisEvolutionTests.hpp"
#include "unit/SIMDSpatialGridTest.hpp"
#include "unit/StashSystemTest.hpp" // ADDED
#include "unit/SystemMechanics.hpp"
#include "unit/TalentModifierTest.cpp"

// --- Integration Tests ---
#include "integration/CombatBalanceTest.hpp"
#include "integration/GameplaySystems.hpp"
#include "integration/MDIRenderTest.hpp"
#include "integration/NemesisScalingTest.hpp"
#include "integration/SkillSystemTests.hpp"

// --- Functional Tests ---
#include "functional/BackstabMechanicsTest.cpp"
#include "functional/DamagePipelineConversionTest.cpp"
#include "functional/SkillBehaviors.hpp"

// --- Unit Tests ---
#include "unit/BranchlessTest.cpp"
#include "unit/GPUFlagsTest.cpp"
#include "unit/MonsterScalingTest.cpp"

// --- Integration Tests ---
#include "integration/AIFlowFieldIntegrationTest.cpp"

// --- Tech & Engine Tests ---
#include "performance/BranchlessBenchmark.hpp"
#include "performance/StatsBenchmark.hpp"
#include "tech/EngineTechTests.hpp"
#include "tech/PersistentBufferTest.hpp"
#include "tech/UITests.hpp"

// --- Performance Benchmarks ---
#include "performance/DropSystemBenchmark.hpp"
#include "performance/MDIRenderBenchmark.hpp"
#include "performance/RenderingBenchmark.hpp"
#include "performance/SpatialGridBenchmark.hpp"
#include "performance/StashBenchmark.hpp" // ADDED

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