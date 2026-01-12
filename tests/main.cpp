#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include "TestCommon.hpp"
#include <raylib.h>

using namespace NoMoreDay;

// Merged Test Groups
#include "ItemEquipmentTests.hpp"
#include "CombatSystemTests.hpp"
#include "SkillSystemTests.hpp"
#include "BuffTests.hpp"
#include "AstrolabeTests.hpp"
#include "UITests.hpp"

// System and World Tests
#include "WorldSystemTests.hpp"

// Engine and Tech Tests
#include "EngineTechTests.hpp"

// Integration and Benchmarks
#include "FinalIntegrationTest.hpp"
#include "StatsBenchmark.cpp"
#include "TestSwordIntentAccumulation.cpp"
#include "TestShadowSystem.cpp"
#include "GPUFlowFieldTest.hpp"

#if defined(_WIN32) && defined(__GNUC__)
#include <windows.h>
extern int main(int argc, char** argv);
extern "C" int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return main(__argc, __argv);
}
#endif

int main(int argc, char** argv) {
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
