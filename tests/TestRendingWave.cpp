#include "TestCommon.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/data/SkillRegistry.hpp"

namespace NoMoreDay {

TEST_CASE("Skill Verification: Rending Wave Specializations") {
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

    SUBCASE("210 Fen Hai - Projectile Count") {
        auto& spec = active.specialized_slots[0];
        spec.skill_id = 2;
        spec.allocated_points[210] = 3; // +3 projectiles

        SkillExecution exec;
        exec.skill_id = 2;
        exec.owner = player;
        exec.target_pos = {100.0f, 0.0f};

        auto castFunc = SkillBehaviorRegistry::GetCast(2);
        REQUIRE(castFunc != nullptr);
        castFunc(registry, player, exec);

        auto view = registry.view<Projectile>();
        int count = 0;
        for (auto entity : view) { (void)entity; ++count; }
        CHECK(count == 4);
        
        for (auto entity : view) {
            auto& proj = view.get<Projectile>(entity);
            // Penalty for 3 pts is 1.0 - 0.05*3 = 0.85
            CHECK(proj.snapshot.damage_multipliers[0] == doctest::Approx(0.85f));
        }
    }

    SUBCASE("230 Hui Xuan Jin - Boomerang") {
        auto& spec = active.specialized_slots[0];
        spec.skill_id = 2;
        spec.allocated_points[230] = 1;

        SkillExecution exec;
        exec.skill_id = 2;
        exec.owner = player;
        exec.target_pos = {100.0f, 0.0f};
        registry.get_or_emplace<SwordIntentComponent>(player).stacks = 0;

        auto castFunc = SkillBehaviorRegistry::GetCast(2);
        REQUIRE(castFunc != nullptr);
        castFunc(registry, player, exec);

        auto view = registry.view<BoomerangComponent, Projectile>();
        bool found = false;
        for (auto entity : view) {
            auto& bc = view.get<BoomerangComponent>(entity);
            auto& proj = view.get<Projectile>(entity);
            CHECK(bc.phase == BoomerangComponent::Outward);
            CHECK(proj.lifeTime > 1.0f);
            found = true;
            break;
        }
        CHECK(found);
    }

    SUBCASE("250 Ling Li Zhuan Hua - Void Conversion") {
        auto& spec = active.specialized_slots[0];
        spec.skill_id = 2;
        spec.allocated_points[250] = 1;

        SkillExecution exec;
        exec.skill_id = 2;
        exec.owner = player;
        exec.target_pos = {100.0f, 0.0f};

        auto castFunc = SkillBehaviorRegistry::GetCast(2);
        REQUIRE(castFunc != nullptr);
        castFunc(registry, player, exec);

        auto view = registry.view<SkillModifierComponent, Projectile>();
        bool foundVoid = false;
        for (auto entity : view) {
            auto& mods = view.get<SkillModifierComponent>(entity);
            for (auto& mod : mods.damage_modifiers) {
                if (mod.target_tag == Tag::Void && mod.type == ModifierType::Convert) {
                    foundVoid = true;
                    break;
                }
            }
            if (foundVoid) break;
        }
        CHECK(foundVoid == true);
    }
}

} // namespace NoMoreDay
