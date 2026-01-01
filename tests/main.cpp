#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include "TestCommon.hpp"

using namespace NoMoreDay;

#include "ProgressionSystemTest.hpp"
#include "AffixSystemTest.hpp"
#include "VisualEffectTest.hpp"
#include "UISystemTest.hpp"
#include "CombatSystemTest.hpp"
#include "DropSystemTest.hpp"
#include "EquipmentSystemTest.hpp"
#include "ItemModificationTest.hpp"
#include "ItemStatsTest.hpp"
#include "ItemSystemTest.hpp"
#include "LootFilterTest.hpp"
#include "RenderSystemTest.hpp"
#include "StatsSystemTest.hpp"
#include "AssetLoadingSystemTest.hpp"
#include "TagSystemTest.hpp"
#include "AstrolabeRegistryTest.hpp"
#include "AstrolabeSystemTest.hpp"
#include "SkillSystemTest.hpp"
#include "DamagePipelineTest.hpp"
#include "AstrolabeUITest.hpp"

int main(int argc, char** argv) {
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    return context.run();
}
