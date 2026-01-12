#include "TestCommon.hpp"
#include "game/systems/skill/ShadowSystem.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/data/SkillRegistry.hpp"

namespace NoMoreDay {

TEST_CASE("ShadowSystem Tests") {
    LoggerScope scope;
    entt::registry registry;
    
    // Ensure skills are loaded so ShadowCast can find ID 1
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    
    SUBCASE("Shadow Execution Logic") {
        auto shadow_ent = registry.create();
        registry.emplace<LocalLevelTag>(shadow_ent); // Required for ShadowCast logic usually? No.
        
        auto& sc = registry.emplace<ShadowComponent>(shadow_ent);
        sc.delay = 0.5f;
        sc.lifetime = 2.0f;
        sc.snapshot.skill_id = 1; // Flowing Thrust
        sc.snapshot.position = {100, 100};
        sc.snapshot.target_pos = {200, 200};
        
        // 1. Update before delay -> No trigger
        ShadowSystem::Update(registry, 0.4f);
        CHECK(sc.delay == doctest::Approx(0.1f));
        CHECK(sc.triggered == false);
        
        // 2. Update after delay -> Trigger
        ShadowSystem::Update(registry, 0.2f);
        CHECK(sc.triggered == true);
        
        // 3. Verify SkillExecution was created
        bool executed = false;
        auto view = registry.view<SkillExecution, ShadowCastTag>();
        for(auto entity : view) {
            auto& exec = view.get<SkillExecution>(entity);
            if (exec.skill_id == 1 && exec.owner == shadow_ent) {
                executed = true;
                CHECK(exec.target_pos.x == 200.0f);
                CHECK(exec.target_pos.y == 200.0f);
            }
        }
        CHECK(executed);
    }

    SUBCASE("Shadow Cleanup - Immediate if not casting") {
         auto shadow_ent = registry.create();
         auto& sc = registry.emplace<ShadowComponent>(shadow_ent);
         sc.lifetime = 1.0f;
         sc.triggered = true; 
         
         // Since no SkillExecution exists for this owner, it should expire
         ShadowSystem::Update(registry, 0.1f);
         
         CHECK_FALSE(registry.valid(shadow_ent));
    }

    SUBCASE("Shadow Cleanup - Waits for cast") {
         auto shadow_ent = registry.create();
         auto& sc = registry.emplace<ShadowComponent>(shadow_ent);
         sc.lifetime = 1.0f;
         sc.triggered = true; 
         
         // Manually create a cast execution owned by shadow
         auto cast_ent = registry.create();
         auto& exec = registry.emplace<SkillExecution>(cast_ent);
         exec.owner = shadow_ent;
         exec.skill_id = 1;
         
         // Update - should NOT expire yet
         ShadowSystem::Update(registry, 0.1f);
         CHECK(registry.valid(shadow_ent));
         
         // Finish cast
         registry.destroy(cast_ent);
         
         // Update - should expire now
         ShadowSystem::Update(registry, 0.1f);
         CHECK_FALSE(registry.valid(shadow_ent));
    }
    
    SUBCASE("Shadow Cleanup - Lifetime limit") {
         auto shadow_ent = registry.create();
         auto& sc = registry.emplace<ShadowComponent>(shadow_ent);
         sc.lifetime = 0.1f; // Short life
         sc.triggered = false; 
         
         // Update past lifetime
         ShadowSystem::Update(registry, 0.2f);
         CHECK_FALSE(registry.valid(shadow_ent));
    }
}

}
