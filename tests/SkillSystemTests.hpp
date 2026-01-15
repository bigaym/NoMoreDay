#pragma once
#include "TestCommon.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/ProjectileSystem.hpp"
#include "game/systems/skill/SummonSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "engine/render/UIRenderer.hpp"
#include "engine/physics/PhysicsSystem.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/Stats.hpp"
#include "game/components/Common.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/EnemyComponent.hpp"
#include <entt/entt.hpp>
#include <unordered_set>

namespace NoMoreDay {

TEST_CASE("SkillSystem: Registry Loading") {
    LoggerScope scope;
    auto& registry = SkillRegistry::Get();
    registry.LoadFromJson("assets/data/skills.json");

    SUBCASE("Load Flowing Thrust") {
        const auto* skill = registry.GetSkill(1);
        REQUIRE(skill != nullptr);
        CHECK(skill->name_key == "流云刺");
        CHECK(skill->mana_cost == 5.0f);
        CHECK(HasTag(skill->tags, Tag::Physical));
    }

    SUBCASE("Load Rending Wave") {
        const auto* skill = registry.GetSkill(2);
        REQUIRE(skill != nullptr);
        CHECK(HasTag(skill->tags, Tag::Projectile));
        CHECK(skill->max_charges == 3);
    }
}

TEST_CASE("SkillSystem: Execution Logic") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillBehaviorRegistry::Initialize(); 
    CombatEventDispatcher::Init();
    systems::SpatialHashGrid grid(100, 100, 50);

    auto player = registry.create();
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    auto& stats = registry.emplace<CombatStats>(player);
    stats.mana = 100.0f;
    active.slots[0].id = 1; 

    SUBCASE("Charges and Cooldown") {
        active.slots[0].current_charges = 1;
        CHECK(SkillSystem::TryCast(registry, player, 0));
        CHECK(active.slots[0].current_charges == 0);
        
        SkillSystem::Update(registry, grid, 0.5f);
        CHECK_FALSE(SkillSystem::TryCast(registry, player, 0));
    }

    SUBCASE("State Machine & Callback") {
        auto oldCast = SkillBehaviorRegistry::GetCast(1);
        SkillBehaviorRegistry::RegisterCast(1, nullptr);

        bool effect_triggered = false;
        SkillSystem::RegisterEffect(1, [&](entt::registry&, entt::entity, SkillExecution&) {
            effect_triggered = true;
        });

        active.slots[0].current_charges = 1;
        SkillSystem::TryCast(registry, player, 0);
        SkillSystem::Update(registry, grid, 0.11f);
        CHECK(effect_triggered);

        SkillBehaviorRegistry::RegisterCast(1, oldCast);
    }
}

TEST_CASE("SkillSystem: Sword Intent") {
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    CombatEventDispatcher::Init();
    SkillSystem::InitHooks();
    systems::SpatialHashGrid grid(100, 100, 50);

    auto player = registry.create();
    auto& intent = registry.emplace<SwordIntentComponent>(player);
    intent.decay_interval = 1.0f;
    intent.grace_period = 0.1f;

    SUBCASE("Accumulation") {
        CombatEventDispatcher::Dispatch(registry, CombatEventFactory::CreateSkillHit(player, entt::null, 1, Tag::Melee | Tag::Physical | Tag::Hit, false));
        CHECK(intent.stacks == 1);
    }

    SUBCASE("Decay Logic") {
        intent.stacks = 5;
        intent.grace_period = 1.0f;
        intent.time_since_last_gain = 0.0f;
        
        // Update roughly 0.5s - shouldn't decay
        // Need to pass executor pointer as nullptr since it's now required in Update signature?
        // Let's check SkillSystem::Update signature. 
        // void Update(entt::registry &registry, systems::SpatialHashGrid &grid, float dt, tf::Executor *executor = nullptr);
        // It seems it usually takes executor.
        SkillSystem::Update(registry, grid, 0.5f);
        CHECK(intent.stacks == 5);
        
        // Update another 0.6s -> total 1.1s > 1.0s grace -> clear all
        SkillSystem::Update(registry, grid, 0.6f);
        CHECK(intent.stacks == 0);
    }

    SUBCASE("No Passive Gain") {
        intent.stacks = 0;
        intent.gain_rate = 1.0f;
        
        SkillSystem::Update(registry, grid, 2.0f); // 2 seconds
        CHECK(intent.stacks == 0);
    }

    SUBCASE("Empowered Cast") {
        auto& active = registry.emplace<ActiveSkillsComponent>(player);
        registry.emplace<CombatStats>(player);
        active.slots[0].id = 1;
        active.slots[0].current_charges = 1;
        intent.stacks = 10;

        SkillSystem::TryCast(registry, player, 0);
        SkillSystem::Update(registry, grid, 0.11f);
        
        auto exec_view = registry.view<SkillExecution>();
        REQUIRE(!exec_view.empty());
        CHECK(exec_view.get<SkillExecution>(*exec_view.begin()).is_empowered == true);
        CHECK(intent.stacks == 0);
    }
}

TEST_CASE("SkillSpecialization: Logic") {
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    auto player = registry.create();
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    active.available_talent_points = 10;
    active.specialized_slots[0].skill_id = 1;

    SUBCASE("Point Allocation and Reset") {
        CHECK(SkillSystem::AddTalentPoint(registry, player, 1, 100));
        CHECK(active.available_talent_points == 9);
        CHECK(SkillSystem::ResetTalents(registry, player, 1));
        CHECK(active.available_talent_points == 10);
    }
}

TEST_CASE("Skill Logic: Specialized Behaviors") {
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillSystem::InitHooks();
    systems::SpatialHashGrid grid(1000, 1000, 50);
    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<CombatStats>(player);

    SUBCASE("Boomerang") {
        auto proj_ent = registry.create();
        registry.emplace<Position>(proj_ent, 100.0f, 0.0f); 
        registry.emplace<Velocity>(proj_ent, 100.0f, 0.0f); 
        registry.emplace<Projectile>(proj_ent).owner = player;
        auto& bc = registry.emplace<BoomerangComponent>(proj_ent);
        bc.phase = BoomerangComponent::Outward;
        bc.returnTimer = 0.5f;

        ProjectileSystem::Update(registry, grid, 0.6f);
        CHECK(bc.phase == BoomerangComponent::Returning);
    }

    SUBCASE("Channeling - Infinite Blades") {
        auto& chan = registry.emplace<ChannelingComponent>(player);
        chan.skill_id = 5; 
        chan.channel_timer = 1.0f;
        chan.tick_interval = 0.1f;
        chan.tick_timer = 0.1f;

        SkillSystem::Update(registry, grid, 0.15f);
        CHECK(!registry.view<SkillExecution>().empty());
    }

    SUBCASE("Blade Formation - Talent 321") {
        auto& active = registry.emplace<ActiveSkillsComponent>(player);
        active.specialized_slots[0].skill_id = 3;
        active.specialized_slots[0].allocated_points[321] = 1;
        
        auto& formation = registry.emplace<BladeFormationComponent>(player);
        formation.mana_on_hit = true; // Manually set flag normally set in DoCast
        
        registry.get<CombatStats>(player).mana = 10.0f;
        CombatEventDispatcher::Init();
        SkillSystem::InitHooks(); // Re-register behavior dispatcher
        CombatEventDispatcher::Dispatch(registry, CombatEventFactory::CreateSkillHit(player, entt::null, 3, Tag::Hit, false));
        CHECK(registry.get<CombatStats>(player).mana == 12.0f);
    }
}

TEST_CASE("Projectile Snapshotting") {
    entt::registry registry;
    auto attacker = registry.create();
    auto defender = registry.create();
    auto proj_ent = registry.create();

    auto& a_stats = registry.emplace<CombatStats>(attacker);
    a_stats.damage_multipliers[0] = 1.5f;

    auto& proj = registry.emplace<Projectile>(proj_ent);
    proj.owner = attacker;
    proj.snapshot = a_stats;
    registry.emplace<CombatStats>(proj_ent, proj.snapshot);
    
    a_stats.damage_multipliers[0] = 0.1f; // Attacker weakened
    
    CHECK(registry.get<CombatStats>(proj_ent).damage_multipliers[0] == doctest::Approx(1.5f));
}

} // namespace NoMoreDay