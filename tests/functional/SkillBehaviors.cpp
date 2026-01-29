#pragma once

#include "TestCommon.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/skill/SummonSystem.hpp"
#include "game/systems/skill/behaviors/MindBlade.hpp"

namespace NoMoreDay {

TEST_CASE("[Functional] Skill - Blade Boomerang Specializations") {
    TestSetupScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillBehaviorRegistry::Initialize();

    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    auto& stats = registry.emplace<CombatStats>(player);
    stats.min_weapon_damage = 100.0f;
    stats.max_weapon_damage = 100.0f;
    for (auto& m : stats.damage_multipliers) m = 1.0f;
    
    auto& active = registry.emplace<ActiveSkillsComponent>(player);

    SUBCASE("812 Po Kong - Speed Scaling") {
        auto& spec = active.specialized_slots[0];
        spec.skill_id = 8;
        spec.allocated_points[812] = 3; 

        SkillExecution exec;
        exec.skill_id = 8;
        exec.owner = player;
        exec.target_pos = {100.0f, 0.0f};

        auto castFunc = SkillBehaviorRegistry::GetCast(8);
        REQUIRE(castFunc != nullptr);
        castFunc(registry, player, exec);

        auto view = registry.view<Projectile>();
        bool found = false;
        for (auto entity : view) {
            auto& proj = view.get<Projectile>(entity);
            CHECK(proj.snapshot.damage_multipliers[0] >= 1.0f);
            found = true;
            break;
        }
        CHECK(found);
    }
}

TEST_CASE("[Functional] Skill - Blade Formation Specializations") {
    TestSetupScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillBehaviorRegistry::Initialize();

    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<CombatStats>(player);
    auto& active = registry.emplace<ActiveSkillsComponent>(player);

    SUBCASE("330 Giant Sword") {
        auto& spec = active.specialized_slots[0];
        spec.skill_id = 3;
        spec.allocated_points[330] = 3; 

        SkillExecution exec;
        exec.skill_id = 3;
        exec.owner = player;
        exec.active_nodes.set(330 % 100); // Manually set talent node bit

        auto castFunc = SkillBehaviorRegistry::GetCast(3);
        REQUIRE(castFunc != nullptr);
        castFunc(registry, player, exec);

        auto view = registry.view<BladeFormationComponent>();
        bool found = false;
        for (auto entity : view) {
            auto& bf = view.get<BladeFormationComponent>(entity);
            CHECK(bf.has_giant_sword == true);
            found = true;
            break;
        }
        CHECK(found);
    }
}

TEST_CASE("[Functional] Skill - Blade Ward") {
    TestSetupScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillBehaviorRegistry::Initialize();

    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<CombatStats>(player);
    registry.emplace<SwordIntentComponent>(player).stacks = 0;

    SkillExecution exec;
    exec.skill_id = 4;
    exec.owner = player;

    auto castFunc = SkillBehaviorRegistry::GetCast(4);
    REQUIRE(castFunc != nullptr);
    castFunc(registry, player, exec);

    auto view = registry.view<BladeWardComponent>();
    CHECK(!view.empty());
}

// TEST_CASE("Skill Verification: Eternal Nightmare") {
//     TestSetupScope scope;
//     entt::registry registry;
//     SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
//     SkillBehaviorRegistry::Initialize();

//     auto player = registry.create();
//     registry.emplace<Position>(player, 0.0f, 0.0f);
//     registry.emplace<CombatStats>(player);
//     registry.emplace<ActiveSkillsComponent>(player);

//     SkillExecution exec;
//     exec.skill_id = 12;
//     exec.owner = player;
//     exec.target_pos = {100.0f, 0.0f};

//     auto castFunc = SkillBehaviorRegistry::GetCast(12);
//     if (castFunc) {
//         castFunc(registry, player, exec);
//         auto view = registry.view<ShadowComponent>();
//         CHECK(!view.empty());
//     }
// }

TEST_CASE("[Functional] Skill - Infinite Blades") {
    TestSetupScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillBehaviorRegistry::Initialize();

    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<CombatStats>(player);
    registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<SwordIntentComponent>(player).stacks = 0;

    SkillExecution exec;
    exec.skill_id = 5; 
    exec.owner = player;
    exec.target_pos = {100.0f, 0.0f};

    auto castFunc = SkillBehaviorRegistry::GetCast(5);
    REQUIRE_MESSAGE(castFunc != nullptr, "Infinite Blades (ID 5) not registered");
    CHECK_NOTHROW(castFunc(registry, player, exec));
}

} // namespace NoMoreDay
