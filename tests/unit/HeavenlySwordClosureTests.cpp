#include "TestCommon.hpp"
#include "SkillKeyNodeMatrixTestHelpers.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

namespace NoMoreDay {

TEST_CASE("[Unit] SkillBehaviorGuard - Heavenly Sword element nodes close remaining gaps") {
    entt::registry registry;
    // Isolate from the process-wide ailment-proc budget singleton: AilmentApplier
    // denies procs once the shared token buckets (keyed by entity id) are drained
    // by earlier tests in the same run. ResetForTests() restores a clean state.
    ProcBudgetManager::Get().ResetForTests();
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

        // FrozenDominion applies Freeze with a 15% chance per field damage tick
        // (HeavenlySwordDescent.cpp), and a single field ticks roughly once per
        // 0.5s, so one cast can legitimately miss the roll. Cast repeatedly until
        // the chance resolves so this assertion no longer depends on the shared
        // thread-local RNG stream position, which varies with test order and made
        // CHECK(hasFreeze) intermittently fail on first runs.
        bool hasFreeze = false;
        for (int attempt = 0; attempt < 10 && !hasFreeze; ++attempt) {
            resource.current = 1000; // enough for repeated casts (each consumes <= 5)
            SkillExecution exec;
            exec.skill_id = 11;
            exec.owner = player;
            exec.target_pos = {18.0f, 0.0f};

            auto cast = SkillBehaviorRegistry::GetCast(11);
            REQUIRE(cast != nullptr);
            cast(registry, player, exec);
            grid.rebuild(registry.view<Position>(), registry);

            for (int i = 0; i < 50 && !hasFreeze; ++i) {
                SkillSystem::Update(registry, grid, 0.10f);
                if (registry.all_of<ActiveEffectsComponent>(target)) {
                    const auto &effects = registry.get<ActiveEffectsComponent>(target);
                    for (const auto& effect : effects.effects) {
                        if (effect.type == BuffType::Freeze) {
                            hasFreeze = true;
                            break;
                        }
                    }
                }
            }
        }
        CHECK(hasFreeze);
    }
}

} // namespace NoMoreDay
