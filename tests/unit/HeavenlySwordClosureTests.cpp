#include "TestCommon.hpp"
#include "SkillKeyNodeMatrixTestHelpers.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

namespace NoMoreDay {

TEST_CASE("[Unit] SkillBehaviorGuard - Heavenly Sword element nodes close remaining gaps") {
    entt::registry registry;
    CombatEventDispatcher::Init();
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillBehaviorRegistry::Initialize();
    SkillSystem::InitHooks();
    systems::SpatialHashGrid grid(100, 100, 50);

    const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
    auto &mastery = registry.emplace<BladeMasteryComponent>(player);
    mastery.selected = BladeMasteryId::HeavenlySword;

    auto &resource = registry.emplace<BladeResourceComponent>(player);
    resource.kind = BladeResourceKind::SpiritBladeTier;
    resource.current = 5;
    resource.max = 10;

    const auto target = test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});
    if (auto *hp = registry.try_get<HealthComponent>(target)) {
        hp->max = 1000000.0f;
        hp->current = 1000000.0f;
    }

    SUBCASE("SolarIncineration (1123) applies Ignite") {
        mastery.heavenly_attunement = BladeAttunement::Fire;
        test::skill_keynode_matrix::ConfigureSpecialization(registry, player, 11, {{1120, 1}, {1123, 1}});

        SkillExecution exec;
        exec.skill_id = 11;
        exec.owner = player;
        exec.target_pos = {18.0f, 0.0f};

        auto cast = SkillBehaviorRegistry::GetCast(11);
        REQUIRE(cast != nullptr);
        cast(registry, player, exec);

        grid.rebuild(registry.view<Position>(), registry);
        SkillSystem::Update(registry, grid, 0.01f);

        auto &effects = registry.get<ActiveEffectsComponent>(target);
        bool hasIgnite = false;
        for (const auto& effect : effects.effects) {
            if (effect.type == BuffType::Burn) {
                hasIgnite = true;
                break;
            }
        }
        CHECK(hasIgnite);
    }

    SUBCASE("FrozenDominion (1122) has chance to Freeze") {
        mastery.heavenly_attunement = BladeAttunement::Frost;
        test::skill_keynode_matrix::ConfigureSpecialization(registry, player, 11, {{1120, 1}, {1122, 1}});

        SkillExecution exec;
        exec.skill_id = 11;
        exec.owner = player;
        exec.target_pos = {18.0f, 0.0f};

        auto cast = SkillBehaviorRegistry::GetCast(11);
        REQUIRE(cast != nullptr);
        cast(registry, player, exec);

        // We might need multiple ticks to see a freeze if it's chance-based, 
        // but for this test we'll assume it should happen or we'll mock the chance.
        grid.rebuild(registry.view<Position>(), registry);
        bool hasFreeze = false;
        for(int i=0; i<30; ++i) {
            SkillSystem::Update(registry, grid, 0.10f);
            if (registry.all_of<ActiveEffectsComponent>(target)) {
                auto &effects = registry.get<ActiveEffectsComponent>(target);
                for (const auto& effect : effects.effects) {
                    if (effect.type == BuffType::Freeze) {
                        hasFreeze = true;
                        break;
                    }
                }
            }
            if (hasFreeze) break;
        }
        CHECK(hasFreeze);
    }
}

} // namespace NoMoreDay
