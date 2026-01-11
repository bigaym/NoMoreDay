#pragma once
#include "TestCommon.hpp"
#include "../src/core/SkillRegistry.hpp"
#include "../src/systems/SkillSystem.hpp"
#include "../src/components/SkillSystem.hpp"
#include "../src/components/Stats.hpp"
#include "../src/components/Common.hpp"
#include "../src/components/PlayerState.hpp"

namespace NoMoreDay {

TEST_CASE("Shadow Kill Array: Duplication Logic") {
    TestSetupScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    systems::SpatialHashGrid grid(100, 100, 50);

    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    auto& stats = registry.emplace<CombatStats>(player);
    auto& playerStats = registry.emplace<PlayerStats>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<Velocity>(player, 0.0f, 0.0f);
    registry.emplace<AnimationStateComponent>(player);

    stats.mana = 100.0f;
    stats.max_mana = 100.0f;
    playerStats.last_shadow_trigger_time = -10.0f;

    // Setup skills
    // Slot 0: Flowing Thrust (ID 1, Movement)
    // Slot 1: Rending Wave (ID 2, Projectile/Attack)
    // Slot 2: Blade Ward (ID 4, Buff)
    // Slot 3: Infinite Blades (ID 5, Channeled)
    active.slots[0].id = 1; active.slots[0].current_charges = 1;
    active.slots[1].id = 2; active.slots[1].current_charges = 3;
    active.slots[2].id = 4; active.slots[2].current_charges = 1;
    active.slots[3].id = 5; active.slots[3].current_charges = 1;

    // Manually activate Shadow Kill Array ready state
    // We'll need a way to mark that the next skill should be duplicated.
    // Let's assume we use a tag component 'ShadowKillArrayReady'.

    SUBCASE("Basic Duplication") {
        registry.emplace<ShadowKillArrayReady>(player);
        
        // Cast Rending Wave (ID 2)
        // Mana cost is 15. Duplication should cost 15 + 7.5 = 22.5.
        CHECK(SkillSystem::TryCast(registry, player, 1, {100, 0}));
        
        // Verify mana consumption (15 base + 7.5 extra = 22.5)
        // Wait, the spec says "consumes 50% of the original skill's Mana cost".
        // Original: 15. Duplicated: 7.5. Total: 22.5.
        CHECK(stats.mana == 77.5f);

        // Verify Shadow entity creation
        // The shadow entity should have ShadowComponent and ShadowCloneComponent
        auto shadow_view = registry.view<ShadowComponent, ShadowCloneComponent>();
        CHECK(std::distance(shadow_view.begin(), shadow_view.end()) == 1);
        
        // Verify 'Ready' tag is removed
        CHECK_FALSE(registry.any_of<ShadowKillArrayReady>(player));
        
        // Verify ICD is set
        // Assuming SkillSystem uses some internal timer or current time.
        // For tests, we might need to mock time or just check if it's > -10.
        CHECK(playerStats.last_shadow_trigger_time >= 0.0f);
    }

    SUBCASE("Tag Exclusion: Movement") {
        registry.emplace<ShadowKillArrayReady>(player);
        
        // Cast Flowing Thrust (ID 1, Movement)
        CHECK(SkillSystem::TryCast(registry, player, 0, {100, 0}));
        
        // Should NOT duplicate
        auto shadow_view = registry.view<ShadowComponent, ShadowCloneComponent>();
        CHECK(shadow_view.begin() == shadow_view.end());
        
        // 'Ready' tag should REMAINS? Or be consumed?
        // Usually, if a skill is ineligible, it shouldn't consume the "ready" state so the player doesn't waste it.
        CHECK(registry.any_of<ShadowKillArrayReady>(player));
    }

    SUBCASE("Tag Exclusion: Buff") {
        registry.emplace<ShadowKillArrayReady>(player);
        
        // Cast Blade Ward (ID 4, Buff)
        CHECK(SkillSystem::TryCast(registry, player, 2));
        
        CHECK(registry.view<ShadowComponent, ShadowCloneComponent>().begin() == registry.view<ShadowComponent, ShadowCloneComponent>().end());
        CHECK(registry.any_of<ShadowKillArrayReady>(player));
    }

    SUBCASE("Tag Exclusion: Channeled") {
        registry.emplace<ShadowKillArrayReady>(player);
        
        // Cast Infinite Blades (ID 5, Channeled)
        CHECK(SkillSystem::TryCast(registry, player, 3));
        
        CHECK(registry.view<ShadowComponent, ShadowCloneComponent>().begin() == registry.view<ShadowComponent, ShadowCloneComponent>().end());
        CHECK(registry.any_of<ShadowKillArrayReady>(player));
    }

    SUBCASE("Internal Cooldown (ICD)") {
        registry.emplace<ShadowKillArrayReady>(player);
        playerStats.last_shadow_trigger_time = 1.0f; // Current time is 0 (or we assume it's small)
        
        // Mock current time if possible, otherwise we need to wait.
        // Let's assume SkillSystem::TryCast uses a time provider.
        // If we can't easily mock time, we might just check the logic in SkillSystem.cpp.
        // For now, let's assume if LastShadowTriggerTime + 3.0 > CurrentTime, it fails.
        
        // Attempt cast within 3s ICD
        CHECK(SkillSystem::TryCast(registry, player, 1, {100, 0}));
        
        // Should NOT duplicate
        CHECK(registry.view<ShadowComponent, ShadowCloneComponent>().begin() == registry.view<ShadowComponent, ShadowCloneComponent>().end());
        CHECK(registry.any_of<ShadowKillArrayReady>(player));
    }

    SUBCASE("Damage Reduction: 50%") {
        registry.emplace<ShadowKillArrayReady>(player);
        
        // Cast Rending Wave (ID 2)
        CHECK(SkillSystem::TryCast(registry, player, 1, {100, 0}));
        
        auto shadow_view = registry.view<ShadowComponent, ShadowCloneComponent>();
        REQUIRE(shadow_view.begin() != shadow_view.end());
        auto shadow_ent = *shadow_view.begin();
        
        // Advance time to trigger the shadow cast (delay = 0.1s)
        SkillSystem::Update(registry, grid, 0.15f);
        
        // The shadow should have triggered SkillExecution
        auto exec_view = registry.view<SkillExecution>();
        entt::entity shadow_exec = entt::null;
        for(auto e : exec_view) {
            if (exec_view.get<SkillExecution>(e).owner == shadow_ent) {
                shadow_exec = e;
                break;
            }
        }
        REQUIRE(static_cast<bool>(shadow_exec != entt::null));
        
        // Move to Casting state
        SkillSystem::Update(registry, grid, 0.15f);
        
        // Damage calculation should happen (usually triggered by skill callback)
        // For testing, we can manually call DamagePipeline::Calculate
        DamagePool base;
        base.Add(Tag::Physical, 100.0f);
        
        // Attacker is player
        auto result_player = DamagePipeline::Calculate(registry, player, player, 2, base, Tag::Hit);
        
        // Attacker is shadow
        // NOTE: We manually add ShadowCloneComponent to trigger the 50% reduction logic in DamagePipeline
        registry.emplace<ShadowCloneComponent>(shadow_ent);
        auto result_shadow = DamagePipeline::Calculate(registry, shadow_ent, player, 2, base, Tag::Hit);
        
        // Shadow damage should be exactly 50% of player damage
        CHECK(result_shadow.total_damage == doctest::Approx(result_player.total_damage * 0.5f));
    }
}

} // namespace NoMoreDay
