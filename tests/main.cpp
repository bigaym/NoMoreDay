#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include "TestCommon.hpp"
#include <raylib.h>

using namespace NoMoreDay;

#include "StatsSystemTest.hpp"
#include "ItemSystemTest.hpp"
#include "ItemStatsTest.hpp"
#include "ItemModificationTest.hpp"
#include "EquipmentSystemTest.hpp"
#include "DropSystemTest.hpp"
#include "LootFilterTest.hpp"
#include "ProgressionSystemTest.hpp"
#include "CombatSystemTest.hpp"
#include "DamagePipelineTest.hpp"
#include "SkillSystemTest.hpp"
#include "SkillLoadVerificationTest.hpp"
#include "SkillHookTest.hpp"
#include "SkillModifierTest.hpp"
#include "SkillSpecializationTest.hpp"
#include "FlowingThrustSpecTest.hpp"
#include "RendingWaveSpecTest.hpp"
#include "ShadowSystemTest.hpp"
#include "SwordIntentTest.hpp"
#include "MovementStanceTest.hpp"
#include "FinalIntegrationTest.hpp"
#include "ShadowPerformanceTest.hpp"
#include "BuffRegistryTest.hpp"
#include "BuffComponentTest.hpp"
#include "AstrolabeRegistryTest.hpp"
#include "BiomeRegistryTest.hpp"
#include "AstrolabeSystemTest.hpp"
#include "AstrolabeUITest.hpp"
#include "UISystemTest.hpp"
#include "RenderSystemTest.hpp"
#include "LevelManagerTest.hpp"
#include "AssetLoadingSystemTest.hpp"
#include "PortalSystemTest.hpp"
#include "PersistenceTest.hpp"
#include "TagSystemTest.hpp"
#include "VisualEffectTest.hpp"
#include "AffixSystemTest.hpp"

int main(int argc, char** argv) {
    // Some Raylib functions require a window context even if not drawing
    InitWindow(100, 100, "Headless Tests");
    SetTargetFPS(60);

    doctest::Context context;
    context.applyCommandLine(argc, argv);
    int res = context.run();

    CloseWindow();
    return res;
}