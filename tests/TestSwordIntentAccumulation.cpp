/**
 * @file TestSwordIntentAccumulation.cpp
 * @brief Tests for Sword Intent accumulation logic, rate limiting, and cast_id usage.
 */

#include "TestCommon.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/combat/CombatEvents.hpp"
#include "core/logging/Logger.hpp"

using namespace NoMoreDay;

TEST_CASE("SwordIntentAccumulation: InstantSkill_GainOneStackPerCast_MultipleHits") {
    LoggerScope scope;
    entt::registry registry;
    CombatEventDispatcher::Init();
    SkillSystem::InitHooks();

    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    auto& intent = registry.emplace<SwordIntentComponent>(player);
    intent.max_stacks = 10;
    intent.gain_rate = 1.0f; 
    intent.stacks = 0;

    // Simulate "Instant Skill Cast" (ID 1)
    uint64_t castId1 = 100;
    
    // Hit 1 from Cast 1
    CombatEvent evt1 = CombatEventFactory::CreateSkillHit(player, entt::null, 1, Tag::Hit | Tag::Melee, false, castId1);
    CombatEventDispatcher::Dispatch(registry, evt1);

    // Should gain 1 stack
    CHECK(intent.stacks == 1);
    CHECK(intent.hit_tracking[castId1].stacks_gained == 1);

    // Hit 2 from Cast 1 (Rapid hit, e.g. multi-hit or piercing)
    // Assume same frame or very close
    CombatEvent evt2 = CombatEventFactory::CreateSkillHit(player, entt::null, 1, Tag::Hit | Tag::Melee, false, castId1);
    CombatEventDispatcher::Dispatch(registry, evt2);

    // Should NOT gain another stack (1 per cast)
    CHECK(intent.stacks == 1);
    CHECK(intent.hit_tracking[castId1].stacks_gained == 1);

    // Simulate "Instant Skill Cast 2" (ID 1, New Cast)
    uint64_t castId2 = 101;
    CombatEvent evt3 = CombatEventFactory::CreateSkillHit(player, entt::null, 1, Tag::Hit | Tag::Melee, false, castId2);
    CombatEventDispatcher::Dispatch(registry, evt3);

    // Should gain another stack
    CHECK(intent.stacks == 2);
    CHECK(intent.hit_tracking[castId2].stacks_gained == 1);
}

TEST_CASE("SwordIntentAccumulation: ContinuousSkill_RateLimited_OneStackPerSecond") {
    LoggerScope scope;
    entt::registry registry;
    CombatEventDispatcher::Init();
    SkillSystem::InitHooks();

    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    auto& intent = registry.emplace<SwordIntentComponent>(player);
    intent.max_stacks = 10;
    intent.stacks = 0;

    // Simulate "Continuous Skill" (ID 7, Mind Blade, Channeled)
    uint64_t castIdChan = 200;
    Tag chanTags = Tag::Hit | Tag::Channeled | Tag::SwordSkill;

    // T=0.0s: Hit 1 (Start of channel)
    CombatEvent evt1 = CombatEventFactory::CreateSkillHit(player, entt::null, 7, chanTags, false, castIdChan);
    CombatEventDispatcher::Dispatch(registry, evt1);
    
    CHECK(intent.stacks == 1);
    
    // T=0.1s: Hit 2 (Tick)
    // Should NOT gain stack (rate limited)
    CombatEvent evt2 = CombatEventFactory::CreateSkillHit(player, entt::null, 7, chanTags, false, castIdChan);
    CombatEventDispatcher::Dispatch(registry, evt2);
    
    CHECK(intent.stacks == 1); 

    // Simulate T=1.1s: Hit 3
    // Manually manipulate the tracking time to simulate time passed
    // hit_tracking[castIdChan].last_gain_time stores the output of GetTime() when it was set.
    // We modify it to be 2.0 seconds in the past.
    intent.hit_tracking[castIdChan].last_gain_time -= 2.0f;
    
    CombatEvent evt3 = CombatEventFactory::CreateSkillHit(player, entt::null, 7, chanTags, false, castIdChan);
    CombatEventDispatcher::Dispatch(registry, evt3);

    CHECK(intent.stacks == 2); 
}

TEST_CASE("SwordIntentAccumulation: NewCastId_RequiredForFreshStacks") {
    LoggerScope scope;
    entt::registry registry;
    CombatEventDispatcher::Init();
    SkillSystem::InitHooks();

    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    auto& intent = registry.emplace<SwordIntentComponent>(player);
    intent.max_stacks = 5;
    intent.stacks = 0;

    uint64_t castId = 300;
    
    // Hit 1
    CombatEventDispatcher::Dispatch(registry, CombatEventFactory::CreateSkillHit(player, entt::null, 1, Tag::Hit, false, castId));
    CHECK(intent.stacks == 1);

    // Hit 2 (Same cast)
    CombatEventDispatcher::Dispatch(registry, CombatEventFactory::CreateSkillHit(player, entt::null, 1, Tag::Hit, false, castId));
    CHECK(intent.stacks == 1);

    // New Cast
    uint64_t castIdNew = 301;
    CombatEventDispatcher::Dispatch(registry, CombatEventFactory::CreateSkillHit(player, entt::null, 1, Tag::Hit, false, castIdNew));
    CHECK(intent.stacks == 2);
}
