// ============================================================================
// SkillProfileResolveTests.cpp
//
// 用途：skills::ResolveBakedProfile 的命中语义守护。该基元抽取了 9 个技能
// DoCast 中重复的「取槽位缓存 → 未命中回退烘焙」样板，本测试守住以下不变量：
//   (a) 缓存命中时返回槽位内档案本体，绝不触碰调用方 scratch；
//   (b) 未命中且存在同 ID 专精槽时，回退烘焙到 scratch 并返回 &scratch，
//       结果与直接调用 SkillSpecializationBaker::Bake 逐字一致；
//   (c) 无档案来源（无组件 / 无同 ID 槽）时返回 nullptr；
//   (d) 显式 fallbackSpec（技能9 合成专精）优先于槽位扫描；
//   (e) 按传入 owner 解析（召唤物命中回调须先解析到真正的 owner）。
//
// 用例命名遵循 doctest 约定：前缀 `[Unit] `；值断言用 CHECK，前置条件用 REQUIRE。
// 注意避免 `CHECK(a && b)`（会触发 MSVC doctest C2338）。
// ============================================================================

#include "TestCommon.hpp"

#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/SkillProfileResolve.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"

#include <cstdint>

namespace NoMoreDay {
namespace {

constexpr uint32_t kSkillId = 2;
constexpr uint32_t kExtraProjectileNode = 210; // +1 投射物/点

// 构造带单个专精槽的活跃技能实体（不含烘焙缓存）。
entt::entity MakeEntityWithSpec(entt::registry &registry, uint32_t skillId,
                                uint32_t node, int points) {
  const auto entity = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(entity);
  active.specialized_slots[0].skill_id = skillId;
  active.specialized_slots[0].allocated_points[node] = points;
  return entity;
}

} // namespace

TEST_CASE("[Unit] SkillProfileResolve - Cache Hit Returns Cached Profile") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto entity = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(entity);
  active.baked_profiles[0].skill_id = kSkillId;
  active.baked_profiles[0].effective_level = 7;

  BakedSkillProfile scratch{};
  scratch.skill_id = 999; // 哨兵：命中时不得被改写

  const auto *resolved = skills::ResolveBakedProfile(registry, entity, kSkillId, scratch);
  REQUIRE(resolved != nullptr);
  CHECK(resolved == &active.baked_profiles[0]);
  CHECK(resolved->effective_level == 7);
  CHECK(scratch.skill_id == 999);
}

TEST_CASE("[Unit] SkillProfileResolve - Miss Falls Back To Specialized Slot") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto entity = MakeEntityWithSpec(registry, kSkillId, kExtraProjectileNode, 3);

  // 先取一份专精副本算出期望值，避免在 Bake 期间持有组件引用。
  SpecializedSkill spec;
  {
    const auto &active = registry.get<ActiveSkillsComponent>(entity);
    spec = active.specialized_slots[0];
  }
  BakedSkillProfile expected{};
  SkillSpecializationBaker::Bake(registry, entity, kSkillId, &spec, expected, nullptr);

  BakedSkillProfile scratch{};
  const auto *resolved = skills::ResolveBakedProfile(registry, entity, kSkillId, scratch);
  REQUIRE(resolved != nullptr);
  CHECK(resolved == &scratch);
  CHECK(resolved->skill_id == kSkillId);

  // 与直接烘焙逐字对比：基元只复用同一烘焙路径，不引入额外数值。
  CHECK(*resolved == expected);
  CHECK(resolved->projectile_count == expected.projectile_count);
}

TEST_CASE("[Unit] SkillProfileResolve - No Source Returns Null") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  BakedSkillProfile scratch{};

  // (1) 完全没有 ActiveSkillsComponent。
  const auto bare = registry.create();
  CHECK(skills::ResolveBakedProfile(registry, bare, kSkillId, scratch) == nullptr);

  // (2) 有组件但没有同 ID 的专精槽与缓存。
  const auto empty = registry.create();
  registry.emplace<ActiveSkillsComponent>(empty);
  CHECK(skills::ResolveBakedProfile(registry, empty, kSkillId, scratch) == nullptr);

  // (3) 只有其他技能的槽位，不误命中。
  const auto other = MakeEntityWithSpec(registry, kSkillId + 1, kExtraProjectileNode, 1);
  CHECK(skills::ResolveBakedProfile(registry, other, kSkillId, scratch) == nullptr);
}

TEST_CASE("[Unit] SkillProfileResolve - Explicit Fallback Spec Takes Precedence") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  // 槽位里有 3 点（应产出 4 枚投射物），但显式合成专精为空（基准 1 枚）。
  const auto entity = MakeEntityWithSpec(registry, kSkillId, kExtraProjectileNode, 3);

  SpecializedSkill synthesized;
  synthesized.skill_id = kSkillId;

  BakedSkillProfile scratch{};
  const auto *resolved = skills::ResolveBakedProfile(registry, entity, kSkillId, scratch,
                                                     &synthesized);
  REQUIRE(resolved != nullptr);
  CHECK(resolved == &scratch);
  CHECK(resolved->projectile_count == 1); // 来自 fallbackSpec，而非槽位

  BakedSkillProfile expected{};
  SkillSpecializationBaker::Bake(registry, entity, kSkillId, &synthesized, expected, nullptr);
  CHECK(*resolved == expected);
}

TEST_CASE("[Unit] SkillProfileResolve - Resolves Passed Owner") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  // 施法者与召唤物各自持有同 ID 档案：基元必须按传入 owner 解析。
  const auto caster = registry.create();
  auto &casterActive = registry.emplace<ActiveSkillsComponent>(caster);
  casterActive.baked_profiles[0].skill_id = kSkillId;
  casterActive.baked_profiles[0].effective_level = 7;

  const auto summon = registry.create();
  auto &summonActive = registry.emplace<ActiveSkillsComponent>(summon);
  summonActive.baked_profiles[0].skill_id = kSkillId;
  summonActive.baked_profiles[0].effective_level = 3;

  BakedSkillProfile scratch{};
  const auto *resolved = skills::ResolveBakedProfile(registry, summon, kSkillId, scratch);
  REQUIRE(resolved != nullptr);
  CHECK(resolved == &summonActive.baked_profiles[0]);
  CHECK(resolved->effective_level == 3);
}

} // namespace NoMoreDay
