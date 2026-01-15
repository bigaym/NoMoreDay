#include "TestCommon.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/skill/SkillSystem.hpp"

namespace NoMoreDay {

TEST_CASE("Infinite Blades talent node index fix verification") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillBehaviorRegistry::Initialize();

    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<CombatStats>(player);
    registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<SwordIntentComponent>(player);

    SUBCASE("Casting Infinite Blades with talent ID 551 should not crash") {
        SkillExecution exec;
        exec.skill_id = 5;
        exec.owner = player;
        exec.target_pos = {100.0f, 100.0f};
        
        // This simulates the behavior of SkillSystem::TryCast setting the bit
        exec.active_nodes.set(551 % 100); 

        auto castFunc = SkillBehaviorRegistry::GetCast(5);
        REQUIRE(castFunc != nullptr);
        
        // Before fix, if we had "exec.active_nodes.test(551)" inside DoCast,
        // it would throw std::out_of_range here.
        CHECK_NOTHROW(castFunc(registry, player, exec));
        
        // Also verify the effect was applied
        CHECK(registry.all_of<ChannelingComponent>(player));
        auto& chan = registry.get<ChannelingComponent>(player);
        CHECK(chan.skill_id == 5);
        // Note: extra_projectiles would only be true if intent >= 10
    }

    SUBCASE("Casting Infinite Blades with talent ID 530 should not crash") {
        SkillExecution exec;
        exec.skill_id = 5;
        exec.owner = player;
        
        exec.active_nodes.set(530 % 100);

        auto castFunc = SkillBehaviorRegistry::GetCast(5);
        REQUIRE(castFunc != nullptr);
        
        CHECK_NOTHROW(castFunc(registry, player, exec));
        
        auto& chan = registry.get<ChannelingComponent>(player);
        CHECK(chan.full_screen_lock == true);
    }
}

} // namespace NoMoreDay
