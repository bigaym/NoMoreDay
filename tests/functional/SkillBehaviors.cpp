#pragma once

#include "TestCommon.hpp"
#include "game/components/AdvancedAffixComponents.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Progression.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/data/BladeMasteryRegistry.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/skill/BladeMasteryService.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
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

TEST_CASE("[Functional] Skill - Seven Star Slash") {
    TestSetupScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
        "assets/data/blade_masteries.json"));
    CombatEventDispatcher::Init();
    SkillBehaviorRegistry::Initialize();

    auto player = registry.create();
    auto target = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<Position>(target, 24.0f, 0.0f);

    auto& playerStats = registry.emplace<PlayerStats>(player);
    playerStats.level = 50;
    auto& playerCombat = registry.emplace<CombatStats>(player);
    playerCombat.min_weapon_damage = 40.0f;
    playerCombat.max_weapon_damage = 40.0f;
    playerCombat.max_mana = 100.0f;
    playerCombat.mana = 100.0f;

    registry.emplace<CombatStats>(target);

    auto& astrolabe = registry.emplace<AstrolabeComponent>(player);
    astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);
    systems::BladeMasteryService::RefreshPlayerState(registry, player);
    REQUIRE(systems::BladeMasteryService::SelectMastery(
        registry, player, BladeMasteryId::SwordSaint));
    REQUIRE(systems::BladeResourceService::Gain(registry, player, 5, 10u));

    SkillExecution exec;
    exec.skill_id = 10;
    exec.owner = player;
    exec.target_pos = {24.0f, 0.0f};

    auto castFunc = SkillBehaviorRegistry::GetCast(10);
    REQUIRE_MESSAGE(castFunc != nullptr, "Seven Star Slash (ID 10) not registered");
    CHECK_NOTHROW(castFunc(registry, player, exec));
    CHECK(registry.all_of<InvulnerableComponent>(player));
    CHECK(registry.get<BladeResourceComponent>(player).current == 0);
}

TEST_CASE("[Functional] Skill - Seven Star Slash Node Effects") {
    auto runSlash = [](const std::unordered_map<uint32_t, int>& nodes,
                       uint32_t activeTransmuter,
                       std::vector<float> targetXs) {
        TestSetupScope scope;
        entt::registry registry;
        SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
        REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
            "assets/data/blade_masteries.json"));
        CombatEventDispatcher::Init();
        SkillBehaviorRegistry::Initialize();

        auto player = registry.create();
        registry.emplace<Position>(player, 0.0f, 0.0f);

        auto& playerStats = registry.emplace<PlayerStats>(player);
        playerStats.level = 50;
        auto& playerCombat = registry.emplace<CombatStats>(player);
        playerCombat.min_weapon_damage = 40.0f;
        playerCombat.max_weapon_damage = 40.0f;
        playerCombat.max_mana = 100.0f;
        playerCombat.mana = 100.0f;

        auto& astrolabe = registry.emplace<AstrolabeComponent>(player);
        astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);
        auto& active = registry.emplace<ActiveSkillsComponent>(player);
        active.specialized_slots[0].skill_id = 10;
        active.specialized_slots[0].allocated_points = nodes;
        auto& runtime = registry.emplace<SkillContractRuntimeComponent>(player);
        if (activeTransmuter != 0u) {
            runtime.active_transmuter_node_by_skill[10] = activeTransmuter;
        }

        systems::BladeMasteryService::RefreshPlayerState(registry, player);
        REQUIRE(systems::BladeMasteryService::SelectMastery(
            registry, player, BladeMasteryId::SwordSaint));
        REQUIRE(systems::BladeResourceService::Gain(registry, player, 5, 10u));

        std::vector<entt::entity> targets;
        for (float x : targetXs) {
            auto target = registry.create();
            registry.emplace<Position>(target, x, 0.0f);
            registry.emplace<CombatStats>(target);
            targets.push_back(target);
        }

        int damageEvents = 0;
        float reportedDamage = 0.0f;
        std::unordered_map<entt::entity, int> hitsByTarget;
        const uint32_t handlerId = CombatEventDispatcher::Register(
            CombatEventType::OnDealDamage,
            [&](entt::registry&, const CombatEvent& evt) {
                if (evt.skill_id != 10) {
                    return;
                }
                ++damageEvents;
                reportedDamage += CombatEventFactory::GetReportedDamage(evt);
                hitsByTarget[evt.target]++;
            });

        SkillExecution exec;
        exec.skill_id = 10;
        exec.owner = player;
        exec.target_pos = {targetXs.front(), 0.0f};

        auto castFunc = SkillBehaviorRegistry::GetCast(10);
        REQUIRE(castFunc != nullptr);
        castFunc(registry, player, exec);
        CombatEventDispatcher::Unregister(CombatEventType::OnDealDamage, handlerId);

        return std::tuple{damageEvents, reportedDamage, hitsByTarget, targets};
    };

    SUBCASE("Follow-through and execute nodes increase burst output") {
        const auto [baseEvents, baseReportedDamage, baseHits, baseTargets] =
            runSlash({}, 0u, {24.0f});
        const auto [nodeEvents, nodeReportedDamage, nodeHits, nodeTargets] =
            runSlash({{1001, 3}, {1002, 1}, {1003, 1}}, 0u, {24.0f});

        CHECK(baseEvents == 7);
        CHECK(nodeEvents == 8);
        CHECK(nodeReportedDamage > baseReportedDamage);
        CHECK(baseHits.at(baseTargets.front()) == 7);
        CHECK(nodeHits.at(nodeTargets.front()) == 8);
    }

    SUBCASE("Transmuters rewrite targeting radius") {
        const auto [focusedEvents, focusedReportedDamage, focusedHits, focusedTargets] =
            runSlash({{1020, 1}}, 1020, {40.0f, 108.0f});
        const auto [huntEvents, huntReportedDamage, huntHits, huntTargets] =
            runSlash({{1021, 1}}, 1021, {132.0f});

        CHECK(focusedEvents > 0);
        CHECK(focusedHits.contains(focusedTargets.front()));
        CHECK_FALSE(focusedHits.contains(focusedTargets.back()));

        CHECK(huntEvents > 0);
        CHECK(huntHits.contains(huntTargets.front()));
    }
}

TEST_CASE("[Functional] Sword Saint - Sword Flow changes cast feel") {
    TestSetupScope scope;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
        "assets/data/blade_masteries.json"));
    SkillBehaviorRegistry::Initialize();

    SUBCASE("Flowing Thrust dash speed scales with Sword Flow") {
        auto buildCaster = [](int flowStacks) {
            entt::registry registry;
            auto player = registry.create();
            registry.emplace<Position>(player, 0.0f, 0.0f);
            registry.emplace<Velocity>(player, 0.0f, 0.0f);
            registry.emplace<DashComponent>(player);
            registry.emplace<CombatStats>(player);

            auto& stats = registry.emplace<PlayerStats>(player);
            stats.level = 50;
            auto& astrolabe = registry.emplace<AstrolabeComponent>(player);
            astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);

            systems::BladeMasteryService::RefreshPlayerState(registry, player);
            REQUIRE(systems::BladeMasteryService::SelectMastery(
                registry, player, BladeMasteryId::SwordSaint));
            if (flowStacks > 0) {
                REQUIRE(systems::BladeResourceService::Gain(
                    registry, player, flowStacks, 1u));
            }

            SkillExecution exec;
            exec.skill_id = 1;
            exec.owner = player;
            exec.target_pos = {120.0f, 0.0f};
            auto castFunc = SkillBehaviorRegistry::GetCast(1);
            REQUIRE(castFunc != nullptr);
            castFunc(registry, player, exec);
            return registry.get<DashComponent>(player).dashSpeed;
        };

        const float baseSpeed = buildCaster(0);
        const float flowSpeed = buildCaster(5);
        CHECK(flowSpeed > baseSpeed);
    }

    SUBCASE("Rending Wave projectile speed and radius scale with Sword Flow") {
        auto buildWave = [](int flowStacks) {
            entt::registry registry;
            auto player = registry.create();
            registry.emplace<Position>(player, 0.0f, 0.0f);
            auto& combat = registry.emplace<CombatStats>(player);
            combat.max_mana = 100.0f;
            combat.mana = 100.0f;

            auto& stats = registry.emplace<PlayerStats>(player);
            stats.level = 50;
            auto& astrolabe = registry.emplace<AstrolabeComponent>(player);
            astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);

            systems::BladeMasteryService::RefreshPlayerState(registry, player);
            REQUIRE(systems::BladeMasteryService::SelectMastery(
                registry, player, BladeMasteryId::SwordSaint));
            if (flowStacks > 0) {
                REQUIRE(systems::BladeResourceService::Gain(
                    registry, player, flowStacks, 2u));
            }

            SkillExecution exec;
            exec.skill_id = 2;
            exec.owner = player;
            exec.target_pos = {160.0f, 0.0f};
            auto castFunc = SkillBehaviorRegistry::GetCast(2);
            REQUIRE(castFunc != nullptr);
            castFunc(registry, player, exec);

            auto view = registry.view<Projectile>();
            REQUIRE_FALSE(view.empty());
            const auto projectile = *view.begin();
            const auto& proj = view.get<Projectile>(projectile);
            return std::pair{proj.speed, proj.radius};
        };

        const auto [baseSpeed, baseRadius] = buildWave(0);
        const auto [flowSpeed, flowRadius] = buildWave(5);
        CHECK(flowSpeed > baseSpeed);
        CHECK(flowRadius > baseRadius);
    }
}

TEST_CASE("[Functional] Sword Saint - High Sword Flow unlocks cast upgrades") {
    TestSetupScope scope;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
        "assets/data/blade_masteries.json"));
    SkillBehaviorRegistry::Initialize();

    SUBCASE("Flowing Thrust gains echo slash at 5 Sword Flow") {
        auto buildCaster = [](int flowStacks) -> std::size_t {
            entt::registry registry;
            auto player = registry.create();
            registry.emplace<Position>(player, 0.0f, 0.0f);
            registry.emplace<Velocity>(player, 0.0f, 0.0f);
            registry.emplace<DashComponent>(player);
            registry.emplace<CombatStats>(player);

            auto& stats = registry.emplace<PlayerStats>(player);
            stats.level = 50;
            auto& astrolabe = registry.emplace<AstrolabeComponent>(player);
            astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);

            systems::BladeMasteryService::RefreshPlayerState(registry, player);
            if (!systems::BladeMasteryService::SelectMastery(
                    registry, player, BladeMasteryId::SwordSaint)) {
                return 0;
            }
            if (flowStacks > 0) {
                if (!systems::BladeResourceService::Gain(
                        registry, player, flowStacks, 1u)) {
                    return 0;
                }
            }

            SkillExecution exec;
            exec.skill_id = 1;
            exec.owner = player;
            exec.target_pos = {120.0f, 0.0f};
            auto castFunc = SkillBehaviorRegistry::GetCast(1);
            if (castFunc == nullptr) {
                return 0;
            }
            castFunc(registry, player, exec);
            std::size_t count = 0;
            for (auto entity : registry.view<ShadowComponent>()) {
                (void)entity;
                ++count;
            }
            return count;
        };

        CHECK(buildCaster(4) == 0);
        CHECK(buildCaster(5) > 0);
    }

    SUBCASE("Flowing Thrust gains a second echo slash at 8 Sword Flow") {
        auto buildCaster = [](int flowStacks) -> std::size_t {
            entt::registry registry;
            auto player = registry.create();
            registry.emplace<Position>(player, 0.0f, 0.0f);
            registry.emplace<Velocity>(player, 0.0f, 0.0f);
            registry.emplace<DashComponent>(player);
            registry.emplace<CombatStats>(player);

            auto& stats = registry.emplace<PlayerStats>(player);
            stats.level = 50;
            auto& astrolabe = registry.emplace<AstrolabeComponent>(player);
            astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);

            systems::BladeMasteryService::RefreshPlayerState(registry, player);
            if (!systems::BladeMasteryService::SelectMastery(
                    registry, player, BladeMasteryId::SwordSaint)) {
                return 0;
            }
            if (flowStacks > 0) {
                if (!systems::BladeResourceService::Gain(
                        registry, player, flowStacks, 1u)) {
                    return 0;
                }
            }

            SkillExecution exec;
            exec.skill_id = 1;
            exec.owner = player;
            exec.target_pos = {120.0f, 0.0f};
            auto castFunc = SkillBehaviorRegistry::GetCast(1);
            if (castFunc == nullptr) {
                return 0;
            }
            castFunc(registry, player, exec);
            std::size_t count = 0;
            for (auto entity : registry.view<ShadowComponent>()) {
                (void)entity;
                ++count;
            }
            return count;
        };

        CHECK(buildCaster(7) == 1);
        CHECK(buildCaster(8) >= 2);
    }

    SUBCASE("Flowing Thrust reaches ultimate echo at 10 Sword Flow") {
        auto buildCaster = [](int flowStacks) -> std::size_t {
            entt::registry registry;
            auto player = registry.create();
            registry.emplace<Position>(player, 0.0f, 0.0f);
            registry.emplace<Velocity>(player, 0.0f, 0.0f);
            registry.emplace<DashComponent>(player);
            registry.emplace<CombatStats>(player);

            auto& stats = registry.emplace<PlayerStats>(player);
            stats.level = 50;
            auto& astrolabe = registry.emplace<AstrolabeComponent>(player);
            astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);

            systems::BladeMasteryService::RefreshPlayerState(registry, player);
            if (!systems::BladeMasteryService::SelectMastery(
                    registry, player, BladeMasteryId::SwordSaint)) {
                return 0;
            }
            if (flowStacks > 0) {
                if (!systems::BladeResourceService::Gain(
                        registry, player, flowStacks, 1u)) {
                    return 0;
                }
            }

            SkillExecution exec;
            exec.skill_id = 1;
            exec.owner = player;
            exec.target_pos = {120.0f, 0.0f};
            auto castFunc = SkillBehaviorRegistry::GetCast(1);
            if (castFunc == nullptr) {
                return 0;
            }
            castFunc(registry, player, exec);
            std::size_t count = 0;
            for (auto entity : registry.view<ShadowComponent>()) {
                (void)entity;
                ++count;
            }
            return count;
        };

        CHECK(buildCaster(9) == 2);
        CHECK(buildCaster(10) >= 3);
    }

    SUBCASE("Rending Wave gains an extra wave at 5 Sword Flow") {
        auto buildWave = [](int flowStacks) -> std::size_t {
            entt::registry registry;
            auto player = registry.create();
            registry.emplace<Position>(player, 0.0f, 0.0f);
            auto& combat = registry.emplace<CombatStats>(player);
            combat.max_mana = 100.0f;
            combat.mana = 100.0f;

            auto& stats = registry.emplace<PlayerStats>(player);
            stats.level = 50;
            auto& astrolabe = registry.emplace<AstrolabeComponent>(player);
            astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);

            systems::BladeMasteryService::RefreshPlayerState(registry, player);
            if (!systems::BladeMasteryService::SelectMastery(
                    registry, player, BladeMasteryId::SwordSaint)) {
                return 0;
            }
            if (flowStacks > 0) {
                if (!systems::BladeResourceService::Gain(
                        registry, player, flowStacks, 2u)) {
                    return 0;
                }
            }

            SkillExecution exec;
            exec.skill_id = 2;
            exec.owner = player;
            exec.target_pos = {160.0f, 0.0f};
            auto castFunc = SkillBehaviorRegistry::GetCast(2);
            if (castFunc == nullptr) {
                return 0;
            }
            castFunc(registry, player, exec);
            std::size_t count = 0;
            for (auto entity : registry.view<Projectile>()) {
                (void)entity;
                ++count;
            }
            return count;
        };

        CHECK(buildWave(4) == 1);
        CHECK(buildWave(5) == 2);
    }

    SUBCASE("Rending Wave gains a second extra wave at 8 Sword Flow") {
        auto buildWave = [](int flowStacks) -> std::size_t {
            entt::registry registry;
            auto player = registry.create();
            registry.emplace<Position>(player, 0.0f, 0.0f);
            auto& combat = registry.emplace<CombatStats>(player);
            combat.max_mana = 100.0f;
            combat.mana = 100.0f;

            auto& stats = registry.emplace<PlayerStats>(player);
            stats.level = 50;
            auto& astrolabe = registry.emplace<AstrolabeComponent>(player);
            astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);

            systems::BladeMasteryService::RefreshPlayerState(registry, player);
            if (!systems::BladeMasteryService::SelectMastery(
                    registry, player, BladeMasteryId::SwordSaint)) {
                return 0;
            }
            if (flowStacks > 0) {
                if (!systems::BladeResourceService::Gain(
                        registry, player, flowStacks, 2u)) {
                    return 0;
                }
            }

            SkillExecution exec;
            exec.skill_id = 2;
            exec.owner = player;
            exec.target_pos = {160.0f, 0.0f};
            auto castFunc = SkillBehaviorRegistry::GetCast(2);
            if (castFunc == nullptr) {
                return 0;
            }
            castFunc(registry, player, exec);
            std::size_t count = 0;
            for (auto entity : registry.view<Projectile>()) {
                (void)entity;
                ++count;
            }
            return count;
        };

        CHECK(buildWave(7) == 2);
        CHECK(buildWave(8) == 3);
    }

    SUBCASE("Rending Wave reaches ultimate wave count at 10 Sword Flow") {
        auto buildWave = [](int flowStacks) -> std::size_t {
            entt::registry registry;
            auto player = registry.create();
            registry.emplace<Position>(player, 0.0f, 0.0f);
            auto& combat = registry.emplace<CombatStats>(player);
            combat.max_mana = 100.0f;
            combat.mana = 100.0f;

            auto& stats = registry.emplace<PlayerStats>(player);
            stats.level = 50;
            auto& astrolabe = registry.emplace<AstrolabeComponent>(player);
            astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);

            systems::BladeMasteryService::RefreshPlayerState(registry, player);
            if (!systems::BladeMasteryService::SelectMastery(
                    registry, player, BladeMasteryId::SwordSaint)) {
                return 0;
            }
            if (flowStacks > 0) {
                if (!systems::BladeResourceService::Gain(
                        registry, player, flowStacks, 2u)) {
                    return 0;
                }
            }

            SkillExecution exec;
            exec.skill_id = 2;
            exec.owner = player;
            exec.target_pos = {160.0f, 0.0f};
            auto castFunc = SkillBehaviorRegistry::GetCast(2);
            if (castFunc == nullptr) {
                return 0;
            }
            castFunc(registry, player, exec);
            std::size_t count = 0;
            for (auto entity : registry.view<Projectile>()) {
                (void)entity;
                ++count;
            }
            return count;
        };

        CHECK(buildWave(9) == 3);
        CHECK(buildWave(10) == 4);
    }
}

} // namespace NoMoreDay
