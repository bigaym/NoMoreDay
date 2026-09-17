// ============================================================================
// SkillProfileResolveSentinelTests.cpp
//
// 用途：守护 BakedSkillProfile::is_baked 契约——只有完整成功烘焙的档案才可被
// skills::ResolveBakedProfile 消费，skillData 缺失时写入的哨兵档案必须被拒绝。
//
// 背景：SkillSystem::RebakeSkillProfiles 在技能表查不到该 skill_id 时，会写入一份
// 仅带 skill_id、其余字段全为默认值（area_radius == 1.0f）的哨兵档案。修复前
// ResolveBakedProfile 第一步只看 skill_id 命中便返回该哨兵，调用方会消费 1.0f 等
// 无意义默认值；而 1.0f 是合法范围值，无法用数值判别，故引入显式 is_baked 标记。
//
// 可证伪性：用例 (b) 中「未注册 skill_id -> 哨兵档案 -> ResolveBakedProfile 返回
// nullptr」在引入 is_baked 过滤之前必然失败（旧实现会返回非空哨兵指针），是本契约
// 的回归证据。
//
// 用例命名遵循 doctest 约定：前缀 `[Unit] `；值断言用 CHECK，前置条件用 REQUIRE。
// 注意避免 `CHECK(a && b)`（会触发 MSVC doctest C2338）。
// ============================================================================

#include "TestCommon.hpp"

#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/SkillProfileResolve.hpp"
#include "game/systems/skill/SkillSystem.hpp"

#include <cstdint>

namespace NoMoreDay {
namespace {

// 技能表已注册（裂空斩）
constexpr uint32_t kRegisteredSkillId = 2;
// 技能表未注册，触发哨兵写入
constexpr uint32_t kUnregisteredSkillId = 999999u;

} // namespace

TEST_CASE("[Unit] SkillProfileResolveSentinel - Missing Component Returns Null") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto bare = registry.create(); // 无 ActiveSkillsComponent
  BakedSkillProfile scratch{};

  CHECK(skills::ResolveBakedProfile(registry, bare, kRegisteredSkillId, scratch) ==
        nullptr);
}

TEST_CASE("[Unit] SkillProfileResolveSentinel - Unbaked Sentinel Is Rejected") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto entity = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(entity);
  active.slots[0].id = kUnregisteredSkillId;

  // 真实入口：槽位引用了技能表未注册的 ID，RebakeSkillProfiles 写入哨兵档案。
  SkillSystem::RebakeSkillProfiles(registry, entity);

  const BakedSkillProfile *sentinel =
      SkillSystem::GetBakedSkillProfile(registry, entity, kUnregisteredSkillId);
  REQUIRE(sentinel != nullptr);
  REQUIRE(sentinel->is_baked == false); // 哨兵：非空但不可消费

  BakedSkillProfile scratch{};
  // 可证伪断言：修复前此处会返回非空哨兵指针，现在必须返回 nullptr。
  CHECK(skills::ResolveBakedProfile(registry, entity, kUnregisteredSkillId, scratch) ==
        nullptr);
}

TEST_CASE("[Unit] SkillProfileResolveSentinel - Baked Profile Is Consumable") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto entity = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(entity);
  active.slots[0].id = kRegisteredSkillId;

  SkillSystem::RebakeSkillProfiles(registry, entity);

  const BakedSkillProfile *baked =
      SkillSystem::GetBakedSkillProfile(registry, entity, kRegisteredSkillId);
  REQUIRE(baked != nullptr);
  REQUIRE(baked->is_baked);

  BakedSkillProfile scratch{};
  const BakedSkillProfile *resolved =
      skills::ResolveBakedProfile(registry, entity, kRegisteredSkillId, scratch);
  REQUIRE(resolved != nullptr);
  CHECK(resolved == baked);
  CHECK(resolved->is_baked == true);
}

TEST_CASE("[Unit] SkillProfileResolveSentinel - Rebake Is Idempotent") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto entity = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(entity);
  active.slots[0].id = kRegisteredSkillId;

  SkillSystem::RebakeSkillProfiles(registry, entity);
  const BakedSkillProfile firstSnapshot = active.baked_profiles[0];
  REQUIRE(firstSnapshot.is_baked);

  // 第二次：operator== 已包含 is_baked，产出与缓存逐字段相等，先比较后写回分支
  // 因而跳过写回；此处以内容稳定作为幂等的可观测证据。
  SkillSystem::RebakeSkillProfiles(registry, entity);
  CHECK(active.baked_profiles[0] == firstSnapshot);
  CHECK(active.baked_profiles[0].is_baked == true);

  // 反证：人为篡改缓存后再次 Rebake，内容与权威烘焙结果不同，必须被重写回原值，
  // 说明上面的「稳定」确实来自逐字段相等判定，而非 Rebake 未被执行。
  active.baked_profiles[0].area_radius = -1.0f;
  SkillSystem::RebakeSkillProfiles(registry, entity);
  CHECK(active.baked_profiles[0] == firstSnapshot);
}

TEST_CASE("[Unit] SkillProfileResolveSentinel - Unbaked Synthetic Fallback Returns "
          "Null") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto entity = registry.create();
  registry.emplace<ActiveSkillsComponent>(entity);

  // 第二步出口（fallbackSpec 非空）：未注册 ID 的合成专精仍走 Baker 的
  // skillData==nullptr 哨兵路径，scratch 不可消费，必须返回 nullptr。
  // 修复前该出口恒返回 &scratch（非空哨兵），可证伪。
  SpecializedSkill synthesized;
  synthesized.skill_id = kUnregisteredSkillId;

  BakedSkillProfile scratch{};
  CHECK(skills::ResolveBakedProfile(registry, entity, kUnregisteredSkillId, scratch,
                                    &synthesized) == nullptr);
  CHECK(scratch.is_baked == false);
}

TEST_CASE("[Unit] SkillProfileResolveSentinel - Unbaked Slot-Scan Fallback Returns "
          "Null") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto entity = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(entity);
  // 仅写专精槽、不经 RebakeSkillProfiles：缓存无该 ID，强制走第三步槽位扫描出口。
  active.specialized_slots[0].skill_id = kUnregisteredSkillId;

  // 第三步出口：槽位命中的未注册 ID 烘焙为哨兵，必须返回 nullptr（修复前返回
  // &scratch，可证伪）。
  BakedSkillProfile scratch{};
  CHECK(skills::ResolveBakedProfile(registry, entity, kUnregisteredSkillId, scratch) ==
        nullptr);
  CHECK(scratch.is_baked == false);
}

} // namespace NoMoreDay
