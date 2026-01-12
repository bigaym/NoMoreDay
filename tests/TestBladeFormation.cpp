#include "TestCommon.hpp"
#include "game/systems/skill/SummonSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"

namespace NoMoreDay {

TEST_CASE("Blade Formation Deep Dive") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillBehaviorRegistry::Initialize();
    systems::SpatialHashGrid grid(100, 100, 50);

    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<CombatStats>(player).damage_multipliers[0] = 1.0f;
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 3;

    SUBCASE("Standard Mode - Multiple Swords") {
        // Unlock Talent 300 (Multi-sword)
        active.specialized_slots[0].allocated_points[300] = 2; // +2 swords

        // Cast
        SkillExecution exec;
        exec.skill_id = 3;
        exec.owner = player;
        
        auto castFunc = SkillBehaviorRegistry::GetCast(3);
        REQUIRE(castFunc != nullptr);
        castFunc(registry, player, exec);

        // Check sword count
        auto& formation = registry.get<BladeFormationComponent>(player);
        CHECK(formation.max_swords == 3); // 1 base + 2
        CHECK(formation.current_swords == 3);
        
        int count = 0;
        auto view = registry.view<SpiritSwordTag>();
        for(auto e : view) count++;
        CHECK(count == 3);
    }

    SUBCASE("Giant Sword Mode - Merging") {
        // Unlock Talent 310 (Giant Sword)
        active.specialized_slots[0].allocated_points[310] = 1;
        active.specialized_slots[0].allocated_points[300] = 5; // Even with extra swords, should stay 1

        // Cast
        SkillExecution exec;
        exec.skill_id = 3;
        exec.owner = player;
        
        auto castFunc = SkillBehaviorRegistry::GetCast(3);
        castFunc(registry, player, exec);

        auto& formation = registry.get<BladeFormationComponent>(player);
        CHECK(formation.has_giant_sword == true);
        CHECK(formation.max_swords == 1); // Should be capped at 1
        
        int count = 0;
        auto view = registry.view<SpiritSwordTag>();
        for(auto e : view) count++;
        CHECK(count == 1);
    }

    SUBCASE("Giant Sword - Damage Scaling") {
        // Setup Giant Sword
        active.specialized_slots[0].allocated_points[310] = 1;
        auto castFunc = SkillBehaviorRegistry::GetCast(3);
        SkillExecution exec; exec.skill_id = 3; exec.owner = player;
        castFunc(registry, player, exec);

        // Find the sword
        auto view = registry.view<SpiritSwordTag, SpiritSwordAI>();
        entt::entity sword = view.front();
        auto& ai = view.get<SpiritSwordAI>(sword);
        
        // Spawn a dummy enemy
        auto enemy = registry.create();
        registry.emplace<EnemyTag>(enemy);
        registry.emplace<Position>(enemy, 10.0f, 0.0f); // Close enough
        
        // Mock SkillSystem Hook to check damage
        float damage_mult = 0.0f;
        SkillSystem::ClearHooks();
        SkillSystem::AddPreCastHook([&](entt::registry& r, entt::entity e, SkillExecution& ex) {
            // Check if this is the proxy cast from Spirit Sword (Skill ID 2)
            if (ex.skill_id == 2) {
                if (auto* stats = r.try_get<CombatStats>(ex.owner)) {
                    damage_mult = stats->damage_multipliers[0];
                }
            }
        });

        // Force Attack
        ai.attack_timer = 0.0f;
        ai.target = enemy; // Force target
        
        systems::SummonSystem::UpdateSpiritSwords(registry, 0.1f, grid);
        SkillSystem::UpdateStates(registry, 0.1f); // Process Preparing -> Casting (Hook triggers here)

        CHECK(damage_mult == doctest::Approx(1.5f)); // Giant sword should be 1.5x
    }
    
    SUBCASE("Standard Sword - Damage Scaling") {
         // Standard
        auto castFunc = SkillBehaviorRegistry::GetCast(3);
        SkillExecution exec; exec.skill_id = 3; exec.owner = player;
        castFunc(registry, player, exec);

        auto view = registry.view<SpiritSwordTag, SpiritSwordAI>();
        entt::entity sword = view.front();
        auto& ai = view.get<SpiritSwordAI>(sword);
        
        auto enemy = registry.create();
        registry.emplace<EnemyTag>(enemy);
        registry.emplace<Position>(enemy, 10.0f, 0.0f);

        float damage_mult = 0.0f;
        SkillSystem::ClearHooks();
        SkillSystem::AddPreCastHook([&](entt::registry& r, entt::entity e, SkillExecution& ex) {
            if (ex.skill_id == 2) {
                if (auto* stats = r.try_get<CombatStats>(ex.owner)) {
                    damage_mult = stats->damage_multipliers[0];
                }
            }
        });

        ai.attack_timer = 0.0f;
        ai.target = enemy;
        systems::SummonSystem::UpdateSpiritSwords(registry, 0.1f, grid);
        SkillSystem::UpdateStates(registry, 0.1f); // Process Preparing -> Casting (Hook triggers here)

        CHECK(damage_mult == doctest::Approx(0.5f)); // Standard sword should be 0.5x
    }
}

}
