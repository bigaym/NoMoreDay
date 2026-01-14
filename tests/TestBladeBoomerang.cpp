#include "TestCommon.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/data/SkillRegistry.hpp"

namespace NoMoreDay {

TEST_CASE("Skill Verification: Blade Boomerang Specializations") {
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
        spec.allocated_points[812] = 3; // 3 pts speed scaling

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
            // Base speed 400. Bonus = (400/100) * 0.1 * 3 = 1.2. Mult = 1.0 + 1.2 = 2.2.
            CHECK(proj.snapshot.damage_multipliers[0] == doctest::Approx(2.2f));
            found = true;
            break;
        }
        CHECK(found);
    }

    SUBCASE("813 Huan Ying Hui Xuan - Extra Boomerangs") {
        auto& spec = active.specialized_slots[0];
        spec.skill_id = 8;
        spec.allocated_points[813] = 1;

        SkillExecution exec;
        exec.skill_id = 8;
        exec.owner = player;
        exec.target_pos = {100.0f, 0.0f};

        auto castFunc = SkillBehaviorRegistry::GetCast(8);
        REQUIRE(castFunc != nullptr);
        castFunc(registry, player, exec);

        auto view = registry.view<Projectile>();
        int count = 0;
        for (auto entity : view) { (void)entity; ++count; }
        // 1 original + 2 side-kicks = 3.
        CHECK(count == 3);
    }

    SUBCASE("833 Jian Qi Hei Dong - Pull Strength") {
        auto& spec = active.specialized_slots[0];
        spec.skill_id = 8;
        spec.allocated_points[830] = 1; // Required for pull
        spec.allocated_points[833] = 1; // Black hole

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
            if (proj.radius > 50.0f) { // Check if it's the main projectile (radius 40 * 1.5 = 60)
                CHECK(proj.hasPull == true);
                // Base pull 300. 833 doubles strength.
                CHECK(proj.pullStrength == doctest::Approx(600.0f));
                CHECK(proj.radius == doctest::Approx(60.0f));
                found = true;
                break;
            }
        }
        CHECK(found);
    }
}

} // namespace NoMoreDay
