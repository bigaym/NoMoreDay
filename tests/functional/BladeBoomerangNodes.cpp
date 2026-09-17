// 技能8 御剑·回旋 — 专项功能测试（审查整改回归）
// 覆盖：case8 数值烘焙、830 侧刃/831 扇形角、854 巨剑模式、811 放血、
//       813 护甲击碎延长、852 折返剑意、812 流血暴击增伤、870/872 转质、855 触发规则。
// 说明：仅做可观测行为断言，不改动 src/assets；每个用例独立重置全局单例，避免跨用例污染。

#include "TestCommon.hpp"

#include "game/contracts/CombatEvents.hpp"
#include "game/contracts/DamagePipelineTypes.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/TriggerRuleComponent.hpp"
#include "game/foundation/data/BuffIds.hpp"
#include "game/foundation/data/SkillContract.hpp"
#include "game/foundation/data/TagRegistry.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "raymath.h"

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace NoMoreDay {
namespace {

using doctest::Approx;

constexpr uint32_t kSkillId = 8;
constexpr float kPi = 3.14159265358979f;

// 完整重置技能机制链，保证用例间无残留（含事件派发器与钩子）
void EnsureSkillMechanics() {
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile("assets/data/skill_mechanics.json"));
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  (void)systems::AilmentRegistry::Get().EnsureLoaded();
  // ModifierRuntimeRegistry 为进程级单例，前置用例可能注入合成 blob；依赖真实
  // 生成数据的烘焙断言前强制重载，避免执行顺序造成跨用例污染。
  REQUIRE(ReloadModifierRuntimeFromAsset());
  SkillBehaviorRegistry::Initialize();
  CombatEventDispatcher::Clear();
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();
}

entt::entity CreateTestPlayer(entt::registry &registry,
                              const std::unordered_map<uint32_t, int> &allocated_nodes = {},
                              Vector2 pos = {0.0f, 0.0f}) {
  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, pos.x, pos.y);
  registry.emplace<HealthComponent>(player, 1000.0f, 1000.0f);

  auto &stats = registry.emplace<CombatStats>(player);
  stats.max_health = 1000.0f;
  stats.health = 1000.0f;
  stats.max_mana = 500.0f;
  stats.mana = 500.0f;
  stats.min_weapon_damage = 50.0f;
  stats.max_weapon_damage = 50.0f;
  stats.crit_chance = 10.0f;
  stats.crit_damage = 1.5f;
  stats.effective_intelligence = 100.0f;
  stats.cached_area_level = 1;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = kSkillId;
  active.slots[0].current_charges = 5;
  active.slots[0].cooldown = 0.0f;

  auto &spec = active.specialized_slots[0];
  spec.skill_id = kSkillId;
  spec.allocated_points = allocated_nodes;

  return player;
}

entt::entity CreateTestEnemy(entt::registry &registry, float x, float y, float max_hp = 1000.0f) {
  auto enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, x, y);
  registry.emplace<HealthComponent>(enemy, max_hp, max_hp);
  auto &stats = registry.emplace<CombatStats>(enemy);
  stats.max_health = max_hp;
  stats.health = max_hp;
  stats.cached_area_level = 1;
  return enemy;
}

// 直调行为层 OnCast：与既有 functional 测试一致，避免依赖完整技能状态机
void CastBoomerang(entt::registry &registry, entt::entity player, Vector2 target_pos) {
  SkillExecution exec{};
  exec.skill_id = kSkillId;
  exec.owner = player;
  exec.target_pos = target_pos;
  static uint64_t s_cast_id = 1;
  exec.cast_id = ++s_cast_id;

  auto castFunc = SkillBehaviorRegistry::GetCast(kSkillId);
  REQUIRE(castFunc != nullptr);
  castFunc(registry, player, exec);
}

// 直调行为层 OnHit：模拟悬停切割/折返命中等无投射物上下文场景
void Hit(entt::registry &registry, entt::entity attacker, entt::entity target,
         Tag tags = Tag::Physical) {
  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);
  hitFunc(registry, attacker, target, tags, false);
}

struct BoomerangRef {
  entt::entity entity;
  BoomerangComponent *boomerang;
  Projectile *projectile;
};

std::vector<BoomerangRef> CollectBoomerangs(entt::registry &registry, entt::entity owner) {
  std::vector<BoomerangRef> out;
  for (auto e : registry.view<BoomerangComponent>()) {
    auto &bc = registry.get<BoomerangComponent>(e);
    if (bc.owner != owner) continue;
    out.push_back({e, &bc, registry.try_get<Projectile>(e)});
  }
  return out;
}

BoomerangComponent *FindBoomerang(entt::registry &registry, entt::entity owner) {
  for (auto e : registry.view<BoomerangComponent>()) {
    auto &bc = registry.get<BoomerangComponent>(e);
    if (bc.owner == owner) return &bc;
  }
  return nullptr;
}

const BuffEffect *FindEffect(entt::registry &registry, entt::entity e,
                             const std::string &id) {
  auto *effects = registry.try_get<ActiveEffectsComponent>(e);
  if (!effects) return nullptr;
  for (const auto &b : effects->effects) {
    if (b.id == id) return &b;
  }
  return nullptr;
}

const BuffEffect *FindAilment(entt::registry &registry, entt::entity e, AilmentType type) {
  auto *effects = registry.try_get<ActiveEffectsComponent>(e);
  if (!effects) return nullptr;
  for (const auto &b : effects->effects) {
    if (b.managed_ailment && b.ailment_type == static_cast<uint8_t>(type)) return &b;
  }
  return nullptr;
}

// 眩晕走 BuffType::Stun 直施（无 ailment_contracts 条目），按类型查找
const BuffEffect *FindEffectType(entt::registry &registry, entt::entity e,
                                 BuffType type) {
  auto *effects = registry.try_get<ActiveEffectsComponent>(e);
  if (!effects) return nullptr;
  for (const auto &b : effects->effects) {
    if (b.type == type) return &b;
  }
  return nullptr;
}

// 便捷：用给定节点配置烘焙技能8专精档案
BakedSkillProfile BakeProfile(entt::registry &registry, entt::entity player) {
  auto *active = registry.try_get<ActiveSkillsComponent>(player);
  REQUIRE(active != nullptr);
  auto &spec = active->specialized_slots[0];
  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, kSkillId, &spec, profile);
  return profile;
}

} // namespace

// 用例1：case8 数值烘焙 — 飞行参数唯一事实源为 skills.json，各节点按点数线性叠加
TEST_CASE("[Functional] BladeBoomerang - case8 数值烘焙 (800/801/803/810/830/832/833/834/851/853/854)") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;

  // 无投入：速度/距离/悬停来自 skills.json params(400/300)，duration 由节点 810 提供
  {
    auto player = CreateTestPlayer(registry, {});
    auto profile = BakeProfile(registry, player);
    CHECK(profile.delivery.speed == Approx(400.0f));
    CHECK(profile.delivery.range == Approx(300.0f));
    CHECK(profile.delivery.duration == Approx(0.0f));
    CHECK(profile.delivery.sub_count == 0);
  }
  // 800=4：每点 -1 法力，8 -> 4
  {
    auto player = CreateTestPlayer(registry, {{800, 4}});
    auto profile = BakeProfile(registry, player);
    CHECK(profile.effective_mana_cost == Approx(4.0f));
  }
  // 801=4：速度与最大距离同比例 +60%
  {
    auto player = CreateTestPlayer(registry, {{801, 4}});
    auto profile = BakeProfile(registry, player);
    CHECK(profile.delivery.speed == Approx(640.0f));
    CHECK(profile.delivery.range == Approx(480.0f));
  }
  // 803=4：折返伤害倍率 1.40
  {
    auto player = CreateTestPlayer(registry, {{803, 4}});
    auto profile = BakeProfile(registry, player);
    CHECK(profile.delivery.return_damage_mult == Approx(1.40f));
  }
  // 810=1：悬停时长 0.8s
  {
    auto player = CreateTestPlayer(registry, {{810, 1}});
    auto profile = BakeProfile(registry, player);
    CHECK(profile.delivery.duration == Approx(0.8f));
  }
  // 830=1：两侧刃
  {
    auto player = CreateTestPlayer(registry, {{830, 1}});
    auto profile = BakeProfile(registry, player);
    CHECK(profile.delivery.sub_count == 2);
  }
  // 832=3：接刃回蓝 6
  {
    auto player = CreateTestPlayer(registry, {{832, 3}});
    auto profile = BakeProfile(registry, player);
    CHECK(profile.delivery.catch_mana == Approx(6.0f));
  }
  // 833=3：接刃后攻速 +45%
  {
    auto player = CreateTestPlayer(registry, {{833, 3}});
    auto profile = BakeProfile(registry, player);
    CHECK(profile.delivery.combo_attack_speed == Approx(45.0f));
  }
  // 834=3：剑步延长 1.5s
  {
    auto player = CreateTestPlayer(registry, {{834, 3}});
    auto profile = BakeProfile(registry, player);
    CHECK(profile.delivery.step_extend_sec == Approx(1.5f));
  }
  // 851=4：牵引半径 +60%
  {
    auto player = CreateTestPlayer(registry, {{851, 4}});
    auto profile = BakeProfile(registry, player);
    CHECK(profile.delivery.pull_radius_mult == Approx(1.60f));
  }
  // 853=3：每层剑意 +6% 尺度
  {
    auto player = CreateTestPlayer(registry, {{853, 3}});
    auto profile = BakeProfile(registry, player);
    CHECK(profile.delivery.intent_scaling == Approx(0.06f));
  }
  // 854=1：取消侧刃 + 5% 护甲转基础物伤
  {
    auto player = CreateTestPlayer(registry, {{854, 1}});
    auto profile = BakeProfile(registry, player);
    CHECK(profile.delivery.sub_count == 0);
    CHECK(profile.delivery.giant_armor_scale == Approx(0.05f));
  }
  // 870/872：转质后物伤标签被元素标签替换
  {
    auto fire = CreateTestPlayer(registry, {{870, 1}});
    auto fireProfile = BakeProfile(registry, fire);
    CHECK(HasTag(fireProfile.effective_tags, Tag::Fire));
    CHECK_FALSE(HasTag(fireProfile.effective_tags, Tag::Physical));

    auto lightning = CreateTestPlayer(registry, {{872, 1}});
    auto lightningProfile = BakeProfile(registry, lightning);
    CHECK(HasTag(lightningProfile.effective_tags, Tag::Lightning));
    CHECK_FALSE(HasTag(lightningProfile.effective_tags, Tag::Physical));
  }
}

// 用例1b：853 心剑合一消费侧 — 每层剑意按比例放大飞行速度与命中盒
TEST_CASE("[Functional] BladeBoomerang - 853 剑意尺度作用于投掷参数") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{853, 3}});
  auto &intent = registry.emplace<SwordIntentComponent>(player);
  intent.stacks = 2;

  CastBoomerang(registry, player, {300.0f, 0.0f});

  auto blades = CollectBoomerangs(registry, player);
  REQUIRE(blades.size() == 1);
  REQUIRE(blades[0].projectile != nullptr);
  // 基础 400 * (1 + 0.06 * 2) = 448；判定盒 40 * 1.12 = 44.8
  CHECK(blades[0].projectile->speed == Approx(448.0f));
  CHECK(blades[0].projectile->radius == Approx(44.8f));
}

// 用例2：830 生成两把侧刃且每把基础伤害 -30%；831 收窄扇形角
TEST_CASE("[Functional] BladeBoomerang - 830 侧刃与 831 扇形角") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  // 仅 830：主刃 50，侧刃 35(50*0.70)，角度约 14 度
  {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{830, 1}});
    CastBoomerang(registry, player, {300.0f, 0.0f});

    auto blades = CollectBoomerangs(registry, player);
    REQUIRE(blades.size() == 3);

    int mainCount = 0;
    int sideCount = 0;
    for (auto &b : blades) {
      REQUIRE(b.projectile != nullptr);
      REQUIRE(b.projectile->payload_context.has_value());
      const float base = b.projectile->payload_context->base_damage_min;
      if (base > 40.0f) {
        ++mainCount;
        CHECK(base == Approx(50.0f));
      } else {
        ++sideCount;
        CHECK(base == Approx(35.0f));
      }
    }
    CHECK(mainCount == 1);
    CHECK(sideCount == 2);

    for (auto &b : blades) {
      const float base = b.projectile->payload_context->base_damage_min;
      if (base > 40.0f) continue;
      const auto &vel = registry.get<Velocity>(b.entity);
      const float deg = std::fabs(std::atan2(vel.vy, vel.vx) * 180.0f / kPi);
      CHECK(std::fabs(deg - 14.0f) <= 0.5f);
    }
  }
  // 830=1 + 831=3：扇形角收窄到 0.7 倍 => 9.8 度
  {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{830, 1}, {831, 3}});
    CastBoomerang(registry, player, {300.0f, 0.0f});

    auto blades = CollectBoomerangs(registry, player);
    REQUIRE(blades.size() == 3);
    for (auto &b : blades) {
      const float base = b.projectile->payload_context->base_damage_min;
      if (base > 40.0f) continue;
      const auto &vel = registry.get<Velocity>(b.entity);
      const float deg = std::fabs(std::atan2(vel.vy, vel.vx) * 180.0f / kPi);
      CHECK(std::fabs(deg - 9.8f) <= 0.5f);
    }
    // 侧刃基础伤害仍保持 -30%
    for (auto &b : blades) {
      const float base = b.projectile->payload_context->base_damage_min;
      CHECK((base == Approx(50.0f) || base == Approx(35.0f)));
    }
  }
}

// 用例3：854 巨剑模式 — 无侧刃、护甲按比例转基础物伤、命中击晕
TEST_CASE("[Functional] BladeBoomerang - 854 巨剑模式") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{854, 1}});
  registry.get<CombatStats>(player).armor = 200.0f;

  CastBoomerang(registry, player, {300.0f, 0.0f});
  auto blades = CollectBoomerangs(registry, player);
  REQUIRE(blades.size() == 1);
  REQUIRE(blades[0].projectile != nullptr);
  REQUIRE(blades[0].projectile->payload_context.has_value());
  // 50 + 200 * 0.05 = 60
  CHECK(blades[0].projectile->payload_context->base_damage_min == Approx(60.0f));

  auto target = CreateTestEnemy(registry, 100.0f, 0.0f);
  Hit(registry, player, target);
  CHECK(FindEffectType(registry, target, BuffType::Stun) != nullptr);
}

// 用例4：811 命中概率施放流血，并将该层折算伤害累计进剑刃流血池
TEST_CASE("[Functional] BladeBoomerang - 811 放血与流血池") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  // 811=4：概率 100%，每层 magnitude 10 * duration 4 = 40
  {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{811, 4}});
    CastBoomerang(registry, player, {300.0f, 0.0f});
    auto target = CreateTestEnemy(registry, 100.0f, 0.0f);

    Hit(registry, player, target);
    CHECK(FindAilment(registry, target, AilmentType::Bleed) != nullptr);

    auto *bc = FindBoomerang(registry, player);
    REQUIRE(bc != nullptr);
    CHECK(bc->bleed_damage_pool == Approx(40.0f));
  }
  // 未投入：概率为 0，不应施放流血
  {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {});
    CastBoomerang(registry, player, {300.0f, 0.0f});
    auto target = CreateTestEnemy(registry, 100.0f, 0.0f);

    Hit(registry, player, target);
    CHECK(FindAilment(registry, target, AilmentType::Bleed) == nullptr);

    auto *bc = FindBoomerang(registry, player);
    REQUIRE(bc != nullptr);
    CHECK(bc->bleed_damage_pool == Approx(0.0f));
  }
}

// 用例5：813 悬停切割概率施加护甲击碎，并对已有击碎延长 +1s
TEST_CASE("[Functional] BladeBoomerang - 813 护甲击碎施加与延长") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  // 813=3：90% 概率，多次命中后应存在且时长超过基础值
  {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{813, 3}});
    CastBoomerang(registry, player, {300.0f, 0.0f});
    auto target = CreateTestEnemy(registry, 100.0f, 0.0f);

    // 813 仅在悬停切割期间生效，构造顶点悬停并把目标放入切割半径
    auto *bc = FindBoomerang(registry, player);
    REQUIRE(bc != nullptr);
    bc->phase = BoomerangPhase::HoverApex;
    bc->apex_position = {100.0f, 0.0f};

    for (int i = 0; i < 40; ++i) {
      Hit(registry, player, target);
    }
    auto *shred = FindEffect(registry, target, "ArmorShred");
    REQUIRE(shred != nullptr);
    CHECK(shred->duration > 4.0f);
  }
  // 未投入：不应施加
  {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {});
    CastBoomerang(registry, player, {300.0f, 0.0f});
    auto target = CreateTestEnemy(registry, 100.0f, 0.0f);

    auto *bc = FindBoomerang(registry, player);
    REQUIRE(bc != nullptr);
    bc->phase = BoomerangPhase::HoverApex;
    bc->apex_position = {100.0f, 0.0f};

    for (int i = 0; i < 40; ++i) {
      Hit(registry, player, target);
    }
    CHECK(FindEffect(registry, target, "ArmorShred") == nullptr);
  }
}

// 用例6：852 仅在折返阶段按概率获取剑意
TEST_CASE("[Functional] BladeBoomerang - 852 折返剑意获取") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  // 852=3：45% 概率，折返阶段多次命中后应有剑意
  {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{852, 3}});
    registry.emplace<SwordIntentComponent>(player);
    CastBoomerang(registry, player, {300.0f, 0.0f});

    auto *bc = FindBoomerang(registry, player);
    REQUIRE(bc != nullptr);
    bc->phase = BoomerangPhase::Returning;

    auto target = CreateTestEnemy(registry, 100.0f, 0.0f);
    for (int i = 0; i < 40; ++i) {
      Hit(registry, player, target);
    }
    auto *intent = registry.try_get<SwordIntentComponent>(player);
    REQUIRE(intent != nullptr);
    CHECK(intent->stacks > 0);
  }
  // 未投入：不应获得剑意
  {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {});
    registry.emplace<SwordIntentComponent>(player);
    CastBoomerang(registry, player, {300.0f, 0.0f});

    auto *bc = FindBoomerang(registry, player);
    REQUIRE(bc != nullptr);
    bc->phase = BoomerangPhase::Returning;

    auto target = CreateTestEnemy(registry, 100.0f, 0.0f);
    for (int i = 0; i < 40; ++i) {
      Hit(registry, player, target);
    }
    auto *intent = registry.try_get<SwordIntentComponent>(player);
    REQUIRE(intent != nullptr);
    CHECK(intent->stacks == 0);
  }
}

// 用例7：812 只对带流血的目标提供额外暴击伤害
TEST_CASE("[Functional] BladeBoomerang - 812 仅对流血目标增伤") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{812, 4}});
  SkillSystem::RebakeSkillProfiles(registry, player);

  auto normal = CreateTestEnemy(registry, 10.0f, 0.0f);
  auto bleeding = CreateTestEnemy(registry, 20.0f, 0.0f);

  systems::AilmentApplyRequest bleed{};
  bleed.ailment = AilmentType::Bleed;
  bleed.source = player;
  bleed.magnitude = 10.0f;
  bleed.duration = 4.0f;
  bleed.stacks = 1;
  REQUIRE(systems::AilmentApplier::Apply(registry, bleeding, bleed));

  DamageRequest request{};
  request.attacker = player;
  request.skill_id = kSkillId;
  request.base_pool.Add(Tag::Physical, 100.0f);
  request.additional_tags = Tag::Critical | Tag::Hit;
  request.dispatch_damage_events = false;
  request.is_simulation = true;

  request.defender = normal;
  DamageResult normalResult = DamagePipeline::Calculate(registry, request);
  request.defender = bleeding;
  DamageResult bleedingResult = DamagePipeline::Calculate(registry, request);

  CHECK(normalResult.total_damage > 0.0f);
  CHECK(normalResult.is_crit);
  CHECK(bleedingResult.total_damage > normalResult.total_damage);
}

// 用例8：855 触发规则生成 — 需 330 巨剑降临(技能3)，暴击命中触发
TEST_CASE("[Functional] BladeBoomerang - 855 巨剑共鸣触发规则") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  const auto *contract = SkillRegistry::Get().GetNodeContract(kSkillId, 855);
  REQUIRE(contract != nullptr);
  CHECK(contract->role == SpecNodeRole::Trigger);
  CHECK(contract->trigger.trigger_skill_id == 3u);
  CHECK(contract->trigger.requires_crit);

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{855, 1}});
  auto *active = registry.try_get<ActiveSkillsComponent>(player);
  REQUIRE(active != nullptr);
  auto &spec = active->specialized_slots[0];
  SkillSpecializationBaker::SyncTriggerRules(registry, player, kSkillId, &spec);

  auto *triggers = registry.try_get<TriggerRuleComponent>(player);
  REQUIRE(triggers != nullptr);

  const TriggerRule *rule = nullptr;
  for (uint8_t i = 0; i < triggers->rule_count; ++i) {
    if (triggers->rules[i].rule_id == 855) {
      rule = &triggers->rules[i];
      break;
    }
  }
  REQUIRE(rule != nullptr);
  CHECK(rule->listen_event == CombatEventType::OnSkillHit);
  CHECK(rule->requires_crit);
  CHECK(rule->cast_skill_id == 3u);
  CHECK(rule->required_source_node_id == 330u);
  CHECK(rule->required_source_skill_id == 3u);
  // 触发来源必须是技能8本体的暴击命中，避免其它技能暴击误触发
  CHECK(rule->required_skill_id == 8u);

  // 870/872 均为转质节点且互斥分组一致
  const auto *fire = SkillRegistry::Get().GetNodeContract(kSkillId, 870);
  const auto *lightning = SkillRegistry::Get().GetNodeContract(kSkillId, 872);
  REQUIRE(fire != nullptr);
  REQUIRE(lightning != nullptr);
  CHECK(fire->role == SpecNodeRole::Transmuter);
  CHECK(lightning->role == SpecNodeRole::Transmuter);
  CHECK(fire->keystone_exclusion_group != 0);
  CHECK(fire->keystone_exclusion_group == lightning->keystone_exclusion_group);
}

} // namespace NoMoreDay
