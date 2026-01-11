#pragma once
#include "TestCommon.hpp"
#include "../src/core/SkillRegistry.hpp"
#include "../src/systems/SummonSystem.hpp"

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
        CHECK(HasTag(skill->tags, Tag::Melee));
        CHECK(HasTag(skill->tags, Tag::Movement));
        CHECK(skill->max_charges == 1);
    }

    SUBCASE("Load Rending Wave") {
        const auto* skill = registry.GetSkill(2);
        REQUIRE(skill != nullptr);
        CHECK(skill->name_key == "裂空斩");
        CHECK(HasTag(skill->tags, Tag::Projectile));
        CHECK(skill->cooldown == 2.0f);
        CHECK(skill->max_charges == 3);
    }
}

TEST_CASE("SkillSystem: Parameter Loading") {
    LoggerScope scope;
    auto& registry = SkillRegistry::Get();
    registry.LoadFromJson("assets/data/skills.json");

    SUBCASE("Load Boomerang Params") {
        const auto* skill = registry.GetSkill(8);
        REQUIRE(skill != nullptr);
        CHECK(skill->GetParam("speed") == doctest::Approx(400.0f));
        CHECK(skill->GetParam("return_timer") == doctest::Approx(0.45f));
        CHECK(skill->GetParam("radius") == doctest::Approx(40.0f));
        CHECK(skill->GetParam("non_existent", 123.0f) == doctest::Approx(123.0f));
    }

    SUBCASE("Load Rending Wave Params") {
        const auto* skill = registry.GetSkill(2);
        REQUIRE(skill != nullptr);
        CHECK(skill->GetParam("speed") == doctest::Approx(300.0f));
    }
}

#include "../src/systems/SkillSystem.hpp"
#include "../src/components/SkillSystem.hpp"
#include "../src/components/Stats.hpp"

TEST_CASE("SkillSystem: Charges Logic") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    systems::SpatialHashGrid grid(100, 100, 50);

    auto player = registry.create();
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<CombatStats>(player);
    
    // Equip "Rending Wave" (ID 2, 2s CD, 3 charges)
    active.slots[0].id = 2;
    active.slots[0].current_charges = 3;
    active.slots[0].cooldown = 0.0f;

    SUBCASE("Multiple Casts") {
        // Cast 1
        CHECK(SkillSystem::TryCast(registry, player, 0));
        CHECK(active.slots[0].current_charges == 2);
        CHECK(active.slots[0].cooldown > 0.0f);
        
        // Fully clear execution state machine (needs 3+ updates)
        for(int i=0; i<5; ++i) SkillSystem::Update(registry, grid, 0.1f);

        // Cast 2
        CHECK(SkillSystem::TryCast(registry, player, 0));
        CHECK(active.slots[0].current_charges == 1);
        
        for(int i=0; i<5; ++i) SkillSystem::Update(registry, grid, 0.1f);

        // Cast 3
        CHECK(SkillSystem::TryCast(registry, player, 0));
        CHECK(active.slots[0].current_charges == 0);

        for(int i=0; i<5; ++i) SkillSystem::Update(registry, grid, 0.1f);

        // Cast 4 (Fail)
        CHECK_FALSE(SkillSystem::TryCast(registry, player, 0));
    }

    SUBCASE("Charge Restoration") {
        active.slots[0].current_charges = 0;
        active.slots[0].cooldown = 2.0f;

        // Update 1s
        SkillSystem::Update(registry, grid, 1.0f);
        CHECK(active.slots[0].current_charges == 0);
        CHECK(active.slots[0].cooldown == doctest::Approx(1.0f));

        // Update 1.1s (Total 2.1s) -> 1 charge restored
        SkillSystem::Update(registry, grid, 1.1f);
        CHECK(active.slots[0].current_charges == 1);
        // Cooldown should restart for the next charge
        CHECK(active.slots[0].cooldown > 1.8f); 

        // Update 2.0s -> 2nd charge restored
        SkillSystem::Update(registry, grid, 2.0f);
        CHECK(active.slots[0].current_charges == 2);
        
        // Update 2.0s -> 3rd charge restored
        SkillSystem::Update(registry, grid, 2.0f);
        CHECK(active.slots[0].current_charges == 3);
        CHECK(active.slots[0].cooldown == 0.0f);
    }
}

TEST_CASE("SkillSystem: Execution Logic") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    systems::SpatialHashGrid grid(100, 100, 50);

    auto player = registry.create();
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    auto& stats = registry.emplace<CombatStats>(player);
    
    // Equip "Flowing Thrust" (ID 1) in slot 0
    active.slots[0].id = 1;
    // Equip "Rending Wave" (ID 2) in slot 1
    active.slots[1].id = 2;

    stats.mana = 100.0f;
    stats.max_mana = 100.0f;

    SUBCASE("Successful Cast") {
        active.slots[0].id = 1; // Ensure ID is 1
        active.slots[0].current_charges = 1;
        CHECK(SkillSystem::TryCast(registry, player, 0));
        CHECK(stats.mana == 95.0f); // 100 - 5
        
        // Slot 0 has 0 cooldown in data
        CHECK(active.slots[0].cooldown == 0.0f);
    }

    SUBCASE("Mana Restriction") {
        active.slots[0].id = 1;
        active.slots[0].current_charges = 1;
        stats.mana = 2.0f;
        CHECK_FALSE(SkillSystem::TryCast(registry, player, 0)); // Needs 5
        CHECK(stats.mana == 2.0f);
    }

    SUBCASE("Cooldown Restriction") {
        // Equip "Rending Wave" (ID 2, 2s CD)
        active.slots[1].id = 2;
        active.slots[1].current_charges = 3;
        
        CHECK(SkillSystem::TryCast(registry, player, 1, {0,0}));
        CHECK(active.slots[1].cooldown > 1.9f);
        
        // Immediate second cast should fail
        CHECK_FALSE(SkillSystem::TryCast(registry, player, 1, {0,0}));
        
        // Advance time
        SkillSystem::Update(registry, grid, 1.0f);
        CHECK(active.slots[1].cooldown > 0.9f);
        CHECK_FALSE(SkillSystem::TryCast(registry, player, 1, {0,0}));

        SkillSystem::Update(registry, grid, 1.1f);
        CHECK(active.slots[1].cooldown == 0.0f);
        
        // Wait for previous execution to finish settling (0.1s windup + 0.05s cast + 0.1s settle)
        SkillSystem::Update(registry, grid, 0.5f);
        CHECK(SkillSystem::TryCast(registry, player, 1, {0,0}));
    }

    SUBCASE("State Machine & Callback") {
        bool effect_triggered = false;
        SkillSystem::RegisterEffect(1, [&](entt::registry&, entt::entity, SkillExecution&) {
            effect_triggered = true;
        });

        stats.mana = 100.0f;
        active.slots[0].id = 1;
        active.slots[0].current_charges = 1;

        CHECK(SkillSystem::TryCast(registry, player, 0, {0, 0}));
        
        // At T=0, still in Preparing state
        SkillSystem::Update(registry, grid, 0.05f);
        CHECK_FALSE(effect_triggered);

        // At T=0.1, transitions to Casting and triggers effect
        SkillSystem::Update(registry, grid, 0.06f);
        CHECK(effect_triggered);

        // After some time (0.05 cast + 0.1 settle), execution entity should be destroyed
        SkillSystem::Update(registry, grid, 0.1f); // Casting -> Settle
        SkillSystem::Update(registry, grid, 0.2f); // Settle -> End
        
        auto exec_view = registry.view<SkillExecution>();
        CHECK(exec_view.empty());
    }

    SUBCASE("Animation State Sync") {
        auto& anim = registry.emplace<AnimationStateComponent>(player);
        stats.mana = 100.0f;
        active.slots[0].id = 1;
        active.slots[0].current_charges = 1;
        
        CHECK(SkillSystem::TryCast(registry, player, 0));
        SkillSystem::Update(registry, grid, 0.01f);
        CHECK(anim.state == EntityAnimState::SkillWindup);

        SkillSystem::Update(registry, grid, 0.1f); // Windup -> Casting
        CHECK(anim.state == EntityAnimState::SkillCasting);

        SkillSystem::Update(registry, grid, 0.1f); // Casting -> Settle
        CHECK(anim.state == EntityAnimState::SkillRecovery);

        SkillSystem::Update(registry, grid, 0.2f); // Settle -> End
        CHECK(anim.state == EntityAnimState::Idle);
    }
}

#include "../src/systems/DamagePipeline.hpp"
#include "../src/core/UIRenderer.hpp"
#include "../src/core/AstrolabeRegistry.hpp"
#include "../src/components/Common.hpp"

TEST_CASE("SkillSystem: Tag Scaling & Conversion") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

    auto attacker = registry.create();
    auto defender = registry.create();
    auto& a_stats = registry.emplace<CombatStats>(attacker);
    a_stats.crit_chance = 0.0f; // Disable crit for deterministic damage
    registry.emplace<CombatStats>(defender);
    
    // Base damage pool
    DamagePool base;
    base.Add(Tag::Physical, 100.0f);

    SUBCASE("Basic Tagged Scaling") {
        // Add +50% Physical Melee Damage
        auto& list = registry.emplace<ModifierList>(attacker);
        list.modifiers.push_back({
            StatType::PhysicalDamage, 
            ModifierMode::PercentAdd, 
            50.0f, 
            Tag::Melee | Tag::Physical
        });

        // 1. Melee Hit (Flowing Thrust ID 1 has Melee tag)
        auto result_melee = DamagePipeline::Calculate(registry, attacker, defender, 1, base, Tag::Hit, entt::null, true);
        // Expect (100 base_pool + 10 skill_base) * 1.5 = 165.0
        CHECK(result_melee.total_damage == doctest::Approx(165.0f));

        // 2. Projectile Hit (Rending Wave ID 2 has Projectile tag, NO Melee tag)
        auto result_proj = DamagePipeline::Calculate(registry, attacker, defender, 2, base, Tag::Hit, entt::null, true);
        // Expect (100 base_pool + 25 skill_base) = 125
        CHECK(result_proj.total_damage == doctest::Approx(125.0f));
    }

    SUBCASE("Tag Conversion") {
        auto& global = registry.emplace<GlobalModifierComponent>(attacker);
        // Convert 100% Physical to Fire
        global.modifiers.push_back({Tag::Physical, Tag::Fire, 1.0f, ModifierType::Convert});
        
        // Add +100% Fire Damage
        auto& list = registry.emplace<ModifierList>(attacker);
        list.modifiers.push_back({StatType::FireDamage, ModifierMode::PercentAdd, 100.0f, Tag::Fire});

        auto result = DamagePipeline::Calculate(registry, attacker, defender, 1, base, Tag::Hit, entt::null, true);
        CHECK(result.total_damage == doctest::Approx(220.0f));
        CHECK(result.final_pool.Get(Tag::Fire) == 220.0f);
        CHECK(result.final_pool.Get(Tag::Physical) == 0.0f);
    }

    SUBCASE("Conversion Inheritance") {
        auto& global = registry.emplace<GlobalModifierComponent>(attacker);
        // Convert 100% Physical to Fire
        global.modifiers.push_back({Tag::Physical, Tag::Fire, 1.0f, ModifierType::Convert});

        // Add +50% Physical Damage (Should apply to the converted Fire damage because it was Physical)
        auto& list = registry.emplace<ModifierList>(attacker);
        list.modifiers.push_back({StatType::PhysicalDamage, ModifierMode::PercentAdd, 50.0f});

        // Skill 1 (Flowing Thrust) has Physical base.
        // Base Pool has 100 Physical.
        // Skill Base 10 Physical.
        // Total Base 110 Physical.
        // Converted to 110 Fire.
        // Multiplier: 1.5.
        // Result: 110 * 1.5 = 165.

        auto result = DamagePipeline::Calculate(registry, attacker, defender, 1, base, Tag::Hit, entt::null, true);
        CHECK(result.total_damage == doctest::Approx(165.0f));
        CHECK(result.final_pool.Get(Tag::Fire) == 165.0f);
    }
}

TEST_CASE("SkillSystem: Sword Intent") {
    LoggerScope scope;
    entt::registry registry;
    systems::SpatialHashGrid grid(100, 100, 50);
    auto player = registry.create();
    auto& intent = registry.emplace<SwordIntentComponent>(player);
    intent.decay_interval = 1.0f;
    intent.grace_period = 0.1f; // Small grace period for testing decay

    SUBCASE("Accumulation") {
        SkillSystem::OnSkillHit(registry, player, entt::null, 1, Tag::Melee | Tag::Physical, false);
        CHECK(intent.stacks == 1);
        SkillSystem::OnSkillHit(registry, player, entt::null, 1, Tag::Melee | Tag::Physical, false);
        CHECK(intent.stacks == 2);
    }

    SUBCASE("Decay") {
        intent.stacks = 5;
        intent.time_since_last_gain = 0.2f; // Past grace period
        SkillSystem::Update(registry, grid, 0.5f);
        CHECK(intent.stacks == 5);
        SkillSystem::Update(registry, grid, 0.6f);
        CHECK(intent.stacks == 4);
        SkillSystem::Update(registry, grid, 0.4f);
        CHECK(intent.stacks == 4);
    }

    SUBCASE("Decay Robustness") {
        // Test decay with large dt (simulation catch-up)
        intent.stacks = 10;
        intent.time_since_last_gain = 0.2f; // Past grace period (0.1f)
        intent.decay_interval = 1.0f;

        // Jump 5.5 seconds.
        // Should decay 5 times (at 1.0, 2.0, 3.0, 4.0, 5.0 accumulated time)
        // time_since_last_gain starts at 0.2.
        // +5.5s -> 5.7s.
        // 5.5s elapsed.
        // decay_tick_timer starts at 0.0.
        // +5.5s -> 5.5s.
        // 5.5 / 1.0 = 5 decays. Remainder 0.5.
        
        SkillSystem::Update(registry, grid, 5.5f);
        CHECK(intent.stacks == 5);
        
        // Another 0.6s -> Total 6.1s. Total decays should be 6.
        SkillSystem::Update(registry, grid, 0.6f);
        CHECK(intent.stacks == 4);
    }
}

TEST_CASE("SkillSystem: Shadow Casting") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    auto player = registry.create();

    SUBCASE("Trigger Shadow Cast") {
        CHECK(SkillSystem::ShadowCast(registry, player, 1, {100, 200}, {300, 400}));
        auto shadow_view = registry.view<ShadowLifetime>();
        CHECK(!shadow_view.empty());
    }
}

TEST_CASE("SkillSystem: Mana on Hit") {
    LoggerScope scope;
    entt::registry registry;
    auto player = registry.create();
    auto& stats = registry.emplace<CombatStats>(player);
    stats.mana = 10.0f;
    stats.max_mana = 100.0f;
    stats.mana_on_hit = 5.0f;

    SUBCASE("Basic Restore") {
        SkillSystem::OnSkillHit(registry, player, entt::null, 1, Tag::Hit | Tag::Melee, false);
        CHECK(stats.mana == 15.0f);
    }
}

TEST_CASE("SkillSystem: Tooltip Integration") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    AstrolabeRegistry::Get().Load("assets/data/astrolabe.json");

    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<CombatStats>(player);
    auto& astro = registry.emplace<AstrolabeComponent>(player);

    // Initial check (No Astrolabe nodes)
    auto lines = UIRenderer::GetSkillTooltipLines(registry, 1);
    bool hasAstroBonus = false;
    for (const auto& line : lines) {
        if (line.text.find("星盘伤害") != std::string::npos) hasAstroBonus = true;
    }
    CHECK_FALSE(hasAstroBonus);

    // Activate node 11 (+5% Physical Damage)
    astro.activated_nodes.insert(11);
    
    lines = UIRenderer::GetSkillTooltipLines(registry, 1);
    hasAstroBonus = false;
    std::string bonusValue = "";
    printf("DEBUG: Tooltip Lines for Skill 1:\n");
    for (const auto& line : lines) {
        printf("  - %s\n", line.text.c_str());
        if (line.text.find("星盘伤害") != std::string::npos) {
            hasAstroBonus = true;
            bonusValue = line.text;
        }
    }
    CHECK(hasAstroBonus);
    CHECK(bonusValue.find("+700%") != std::string::npos);
}

TEST_CASE("SkillSystem: Channeling Skills") {
    LoggerScope scope;
    entt::registry registry;
    tf::Executor executor;
    systems::SpatialHashGrid grid(100, 100, 50);
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillSystem::InitHooks(); // IMPORTANT: Init hooks for effects

    auto entity = registry.create();
    auto& stats = registry.emplace<CombatStats>(entity);
    stats.mana = 500.0f; // Sufficient mana
    stats.max_mana = 500.0f;
    registry.emplace<Position>(entity, 0.0f, 0.0f);
    registry.emplace<AnimationStateComponent>(entity);

    SUBCASE("Channeling Skill (ID 5) - Infinite Blades") {
        // 1. Setup
        auto& active = registry.emplace<ActiveSkillsComponent>(entity);
        active.slots[0].id = 5;
        active.slots[0].current_charges = 1;

        registry.emplace<SwordIntentComponent>(entity); // Ensure intent component exists

        // 2. Cast
        bool cast = SkillSystem::TryCast(registry, entity, 0, {100.0f, 100.0f});
        CHECK(cast);
        CHECK(registry.any_of<SkillExecution>(entity));

        // 3. Update to trigger callback (Preparing -> Casting)
        // Timer is 0.1f initially. Update 0.11f to trigger transition.
        SkillSystem::Update(registry, grid, 0.11f, &executor); 
        // Now Casting. Timer resets to 0.05f.
        
        // Update another 0.06f to finish Casting and call hooks
        SkillSystem::Update(registry, grid, 0.06f, &executor);

        // Verify ChannelingComponent exists
        CHECK(registry.any_of<ChannelingComponent>(entity));
        
        if (registry.any_of<ChannelingComponent>(entity)) {
            const auto& chan = registry.get<ChannelingComponent>(entity);
            CHECK(chan.skill_id == 5);
            CHECK(chan.channel_timer > 0.0f);
        }

        // 4. Update to trigger Tick
        // Tick interval is 0.1f.
        SkillSystem::Update(registry, grid, 0.2f, &executor);
        
        // Should have spawned particles or triggered logic (Shadow Cast Rending Wave)
        // We check if Rending Wave (ID 2) execution was spawned
        bool rendingWaveSpawned = false;
        auto execView = registry.view<SkillExecution>();
        for (auto e : execView) {
            if (execView.get<SkillExecution>(e).skill_id == 2) {
                rendingWaveSpawned = true;
                break;
            }
        }
        CHECK(rendingWaveSpawned);
    }
}

TEST_CASE("SkillSystem: Blade Formation (ID 3)") {
    LoggerScope scope;
    entt::registry registry;
    tf::Executor executor;
    systems::SpatialHashGrid grid(100, 100, 32.0f); // Large grid
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillSystem::InitHooks();

    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<CombatStats>(player, 100.0f, 100.0f); // HP, Mana
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    active.slots[0].id = 3;
    active.slots[0].current_charges = 1;

    SUBCASE("Activation & Striking") {
        // 1. Cast
        CHECK(SkillSystem::TryCast(registry, player, 0));
        
        // 2. Transistion to Casting (Windup 0.1s)
        SkillSystem::Update(registry, grid, 0.11f, &executor);
        // Effects should be called now
        CHECK(registry.any_of<BladeFormationComponent>(player));
        
        const auto& formation = registry.get<BladeFormationComponent>(player);
        CHECK(formation.max_swords == 1);
        CHECK(formation.current_swords == 1);

        // 3. Setup Enemy
        auto enemy = registry.create();
        registry.emplace<Position>(enemy, 50.0f, 50.0f);
        registry.emplace<EnemyTag>(enemy);
        auto view = registry.view<EnemyTag, Position>();
        grid.rebuild(view, registry);

        // 4. Update to trigger strike (Interval is 1.0s, timer starts at 0)
        // Note: RegisterEffect sets attack_timer to 0 (default initialized in struct and not explicitly set in callback)
        // Wait, in RegisterEffect(3), I didn't set attack_timer. So it's 0.0f.
        SkillSystem::Update(registry, grid, 0.01f, &executor);
        NoMoreDay::systems::SummonSystem::Update(registry, 0.01f, grid);

        // 5. Verify Strike (Should be Skill ID 2 execution)
        bool strikeFound = false;
        auto execView = registry.view<SkillExecution>();
        for (auto e : execView) {
            auto& exec = execView.get<SkillExecution>(e);
            if (exec.skill_id == 2) {
                strikeFound = true;
                break;
            }
        }
        CHECK(strikeFound);
    }

    SUBCASE("Talent: Immortality (322)") {
        // Setup specialized skill
        active.specialized_slots[0].skill_id = 3;
        active.specialized_slots[0].allocated_points[322] = 1; // Immortality

        // Cast
        SkillSystem::TryCast(registry, player, 0);
        SkillSystem::Update(registry, grid, 0.2f, &executor); // Complete cast

        CHECK(registry.get<BladeFormationComponent>(player).immortality_ready);

        // Take lethal damage
        auto& hp = registry.get<HealthComponent>(player);
        hp.current = 10.0f;
        
        // Apply 100 damage
        CombatSystem::ApplyDamage(registry, player, 100.0f, entt::null);

        // Should be alive with 30% HP
        CHECK(hp.current == doctest::Approx(30.0f));
        CHECK_FALSE(registry.get<BladeFormationComponent>(player).immortality_ready);
    }

    SUBCASE("Talent: Mana on Hit (321)") {
        active.specialized_slots[0].skill_id = 3;
        active.specialized_slots[0].allocated_points[321] = 1;

        // Cast
        SkillSystem::TryCast(registry, player, 0);
        SkillSystem::Update(registry, grid, 0.2f, &executor);

        auto& stats = registry.get<CombatStats>(player);
        stats.mana = 10.0f;

        // Trigger hit with ID 2 (Spirit Sword Strike uses ID 2)
        SkillSystem::OnSkillHit(registry, player, entt::null, 2, Tag::Hit, false);

        CHECK(stats.mana == 12.0f); // 10 + 2
    }
}