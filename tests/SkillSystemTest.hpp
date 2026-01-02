#pragma once
#include "TestCommon.hpp"
#include "../src/core/SkillRegistry.hpp"

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

#include "../src/systems/SkillSystem.hpp"
#include "../src/components/SkillSystem.hpp"
#include "../src/components/Stats.hpp"

TEST_CASE("SkillSystem: Charges Logic") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

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
        for(int i=0; i<5; ++i) SkillSystem::Update(registry, 0.1f);

        // Cast 2
        CHECK(SkillSystem::TryCast(registry, player, 0));
        CHECK(active.slots[0].current_charges == 1);
        
        for(int i=0; i<5; ++i) SkillSystem::Update(registry, 0.1f);

        // Cast 3
        CHECK(SkillSystem::TryCast(registry, player, 0));
        CHECK(active.slots[0].current_charges == 0);

        for(int i=0; i<5; ++i) SkillSystem::Update(registry, 0.1f);

        // Cast 4 (Fail)
        CHECK_FALSE(SkillSystem::TryCast(registry, player, 0));
    }

    SUBCASE("Charge Restoration") {
        active.slots[0].current_charges = 0;
        active.slots[0].cooldown = 2.0f;

        // Update 1s
        SkillSystem::Update(registry, 1.0f);
        CHECK(active.slots[0].current_charges == 0);
        CHECK(active.slots[0].cooldown == doctest::Approx(1.0f));

        // Update 1.1s (Total 2.1s) -> 1 charge restored
        SkillSystem::Update(registry, 1.1f);
        CHECK(active.slots[0].current_charges == 1);
        // Cooldown should restart for the next charge
        CHECK(active.slots[0].cooldown > 1.8f); 

        // Update 2.0s -> 2nd charge restored
        SkillSystem::Update(registry, 2.0f);
        CHECK(active.slots[0].current_charges == 2);
        
        // Update 2.0s -> 3rd charge restored
        SkillSystem::Update(registry, 2.0f);
        CHECK(active.slots[0].current_charges == 3);
        CHECK(active.slots[0].cooldown == 0.0f);
    }
}

TEST_CASE("SkillSystem: Execution Logic") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

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
        SkillSystem::Update(registry, 1.0f);
        CHECK(active.slots[1].cooldown > 0.9f);
        CHECK_FALSE(SkillSystem::TryCast(registry, player, 1, {0,0}));

        SkillSystem::Update(registry, 1.1f);
        CHECK(active.slots[1].cooldown == 0.0f);
        
        // Wait for previous execution to finish settling (0.1s windup + 0.05s cast + 0.1s settle)
        SkillSystem::Update(registry, 0.5f);
        CHECK(SkillSystem::TryCast(registry, player, 1, {0,0}));
    }

    SUBCASE("State Machine & Callback") {
        bool effect_triggered = false;
        SkillSystem::RegisterEffect(1, [&](entt::registry&, entt::entity, uint32_t, Vector2) {
            effect_triggered = true;
        });

        stats.mana = 100.0f;
        active.slots[0].id = 1;
        active.slots[0].current_charges = 1;

        CHECK(SkillSystem::TryCast(registry, player, 0, {0, 0}));
        
        // At T=0, still in Preparing state
        SkillSystem::Update(registry, 0.05f);
        CHECK_FALSE(effect_triggered);

        // At T=0.1, transitions to Casting and triggers effect
        SkillSystem::Update(registry, 0.06f);
        CHECK(effect_triggered);

        // After some time (0.05 cast + 0.1 settle), execution entity should be destroyed
        SkillSystem::Update(registry, 0.1f); // Casting -> Settle
        SkillSystem::Update(registry, 0.2f); // Settle -> End
        
        auto exec_view = registry.view<SkillExecution>();
        CHECK(exec_view.empty());
    }

    SUBCASE("Animation State Sync") {
        auto& anim = registry.emplace<AnimationStateComponent>(player);
        stats.mana = 100.0f;
        active.slots[0].id = 1;
        active.slots[0].current_charges = 1;
        
        CHECK(SkillSystem::TryCast(registry, player, 0));
        SkillSystem::Update(registry, 0.01f);
        CHECK(anim.state == EntityAnimState::SkillWindup);

        SkillSystem::Update(registry, 0.1f); // Windup -> Casting
        CHECK(anim.state == EntityAnimState::SkillCasting);

        SkillSystem::Update(registry, 0.1f); // Casting -> Settle
        CHECK(anim.state == EntityAnimState::SkillRecovery);

        SkillSystem::Update(registry, 0.2f); // Settle -> End
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
    registry.emplace<CombatStats>(attacker);
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
        auto result_melee = DamagePipeline::Calculate(registry, attacker, defender, 1, base, Tag::Hit);
        // Expect (100 base_pool + 10 skill_base) * (1.0 base + 0.5 bonus) = 110 * 1.5 = 165
        CHECK(result_melee.total_damage == doctest::Approx(165.0f));

        // 2. Projectile Hit (Rending Wave ID 2 has Projectile tag, NO Melee tag)
        auto result_proj = DamagePipeline::Calculate(registry, attacker, defender, 2, base, Tag::Hit);
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

        auto result = DamagePipeline::Calculate(registry, attacker, defender, 1, base, Tag::Hit);
        CHECK(result.total_damage == doctest::Approx(220.0f));
        CHECK(result.final_pool.Get(Tag::Fire) == 220.0f);
        CHECK(result.final_pool.Get(Tag::Physical) == 0.0f);
    }
}

TEST_CASE("SkillSystem: Sword Intent") {
    LoggerScope scope;
    entt::registry registry;
    auto player = registry.create();
    auto& intent = registry.emplace<SwordIntentComponent>(player);
    intent.decay_interval = 1.0f;

    SUBCASE("Accumulation") {
        SkillSystem::OnSkillHit(registry, player, entt::null, 1, Tag::Melee | Tag::Physical);
        CHECK(intent.stacks == 1);
        SkillSystem::OnSkillHit(registry, player, entt::null, 1, Tag::Melee | Tag::Physical);
        CHECK(intent.stacks == 2);
    }

    SUBCASE("Decay") {
        intent.stacks = 5;
        SkillSystem::Update(registry, 0.5f);
        CHECK(intent.stacks == 5);
        SkillSystem::Update(registry, 0.6f);
        CHECK(intent.stacks == 5);
        SkillSystem::Update(registry, 0.4f);
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
        auto shadow_view = registry.view<ShadowEntityTag>();
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
        SkillSystem::OnSkillHit(registry, player, entt::null, 1, Tag::Hit | Tag::Melee);
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
    CHECK(bonusValue.find("+500%") != std::string::npos);
}