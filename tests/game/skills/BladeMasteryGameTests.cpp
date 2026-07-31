#include "TestCommon.hpp"
#include "SkillKeyNodeMatrixTestHelpers.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/systems/skill/BladeMasteryService.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include <iostream>

namespace NoMoreDay {

TEST_CASE("[Game] Skill - Blade Mastery Audit Final Verification") {
    TestSetupScope scope;
    entt::registry registry;
    CombatEventDispatcher::Init();
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillBehaviorRegistry::Initialize();
    SkillSystem::InitHooks();
    systems::SpatialHashGrid grid(100, 100, 50);

    const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
    auto &stats = registry.get<CombatStats>(player);
    stats.min_weapon_damage = 100.0f;
    stats.max_weapon_damage = 100.0f;
    for (auto& m : stats.damage_multipliers) m = 1.0f;
    stats.crit_chance = 0.0f;
    stats.crit_damage = 1.5f;

    auto &mastery = registry.emplace<BladeMasteryComponent>(player);
    
    SUBCASE("Node 1111 Sword Rain Echo damage (10%)") {
        mastery.selected = BladeMasteryId::HeavenlySword;
        mastery.heavenly_attunement = BladeAttunement::Fire;
        
        auto &resource = registry.emplace<BladeResourceComponent>(player);
        resource.kind = BladeResourceKind::SpiritBladeTier;
        resource.current = 1; // 1 tier spent
        resource.max = 10;

        test::skill_keynode_matrix::ConfigureSpecialization(registry, player, 11, {{1111, 1}});

        const auto target = test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});
        auto &targetStats = registry.get<CombatStats>(target);
        targetStats.max_health = 10000.0f;
        targetStats.health = 10000.0f;
        targetStats.armor = 0.0f;
        for (auto& r : targetStats.resistances) r = 0.0f;
        if (auto* hp = registry.try_get<HealthComponent>(target)) {
            hp->max = 10000.0f;
            hp->current = 10000.0f;
        }

        std::vector<float> damageDealt;
        uint32_t handlerId = CombatEventDispatcher::Register(CombatEventType::OnDealDamage, [&](entt::registry&, const CombatEvent& evt) {
            if (evt.skill_id == 11) {
                damageDealt.push_back(evt.reported_damage);
            }
        });

        SkillExecution exec;
        exec.skill_id = 11;
        exec.owner = player;
        exec.target_pos = {18.0f, 0.0f};

        auto cast = SkillBehaviorRegistry::GetCast(11);
        REQUIRE(cast != nullptr);
        
        // Ensure no cooldown for trigger
        auto& runtime = registry.get_or_emplace<SkillContractRuntimeComponent>(player);
        runtime.trigger_cooldowns.erase(1111);

        cast(registry, player, exec);

        CombatEventDispatcher::Unregister(CombatEventType::OnDealDamage, handlerId);

        // Expected damage:
        // 1. Impact damage: base_damage (140) * impact_damage_mult (1.18) * 1.05 (global) = 173.46
        // 2. Echo damage: base_damage (140) * 0.10 * 1 tier * 1.05 (global) = 14.7
        
        const auto* skillData = SkillRegistry::Get().GetSkill(11);
        const auto* nodeContract = SkillRegistry::Get().GetNodeContract(11, 1111);
        REQUIRE(skillData != nullptr);
        REQUIRE(nodeContract != nullptr);

        const float expectedEcho = skillData->base_damage * nodeContract->trigger.effectiveness * 1.05f;

        bool foundEcho = false;
        for (float d : damageDealt) {
            if (doctest::Approx(d) == expectedEcho) {
                foundEcho = true;
                break;
            }
        }
        CHECK(foundEcho);
    }

    SUBCASE("Node 1011 Star Scar Follow range and effectiveness") {
        mastery.selected = BladeMasteryId::SwordSaint;
        
        auto &resource = registry.emplace<BladeResourceComponent>(player);
        resource.kind = BladeResourceKind::SwordFlow;
        resource.current = 10; // Max flow
        resource.max = 10;

        // Node 1011: Star Scar Follow
        test::skill_keynode_matrix::ConfigureSpecialization(registry, player, 10, {{1011, 1}});

        const auto target = test::skill_keynode_matrix::CreateTarget(registry, {24.0f, 0.0f});
        auto &targetStats = registry.get<CombatStats>(target);
        targetStats.max_health = 10000.0f;
        targetStats.health = 10000.0f;
        targetStats.armor = 0.0f;
        for (auto& r : targetStats.resistances) r = 0.0f;
        if (auto* hp = registry.try_get<HealthComponent>(target)) {
            hp->max = 10000.0f;
            hp->current = 10000.0f;
        }
        
        std::vector<float> damageDealt;
        uint32_t handlerId = CombatEventDispatcher::Register(CombatEventType::OnDealDamage, [&](entt::registry&, const CombatEvent& evt) {
            if (evt.skill_id == 10) {
                damageDealt.push_back(evt.reported_damage);
            }
        });

        SkillExecution exec;
        exec.skill_id = 10;
        exec.owner = player;
        exec.target_pos = {24.0f, 0.0f};

        auto cast = SkillBehaviorRegistry::GetCast(10);
        REQUIRE(cast != nullptr);

        // Ensure no cooldown for trigger
        auto& runtime = registry.get_or_emplace<SkillContractRuntimeComponent>(player);
        runtime.trigger_cooldowns.erase(1011);

        cast(registry, player, exec);

        CombatEventDispatcher::Unregister(CombatEventType::OnDealDamage, handlerId);

        CHECK(damageDealt.size() >= 8);
        
        // Data-driven verification
        const auto* skillData10 = SkillRegistry::Get().GetSkill(10);
        const auto* nodeContract10 = SkillRegistry::Get().GetNodeContract(10, 1011);
        REQUIRE(skillData10 != nullptr);
        REQUIRE(nodeContract10 != nullptr);

        // Base slash damage calculation (matching SevenStarSlash.cpp logic):
        // baseSlashDamage = averageWeaponDamage * baseDamageMultiplier * (weapon_damage_mult + resource * flowBonusPerStack)
        // averageWeaponDamage = 100, baseDamageMultiplier = 1.0, resource = 10
        const float flowBonusPerStack = skillData10->GetParam("flow_bonus_per_stack", 0.06f);
        const float baseSlashDamage = 100.0f * 1.0f * (skillData10->weapon_damage_mult + 10.0f * flowBonusPerStack);
        const float expectedExtra = baseSlashDamage * nodeContract10->trigger.effectiveness * 1.05f;
        
        bool foundExtra = false;
        for (float d : damageDealt) {
            if (doctest::Approx(d) == expectedExtra) {
                foundExtra = true;
                break;
            }
        }
        CHECK(foundExtra);
    }
}

} // namespace NoMoreDay
