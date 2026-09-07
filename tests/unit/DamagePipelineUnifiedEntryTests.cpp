#include "TestCommon.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/combat/CombatConstants.hpp"
#include "game/contracts/CombatFormula.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include <algorithm>

namespace NoMoreDay {
namespace {

float ComputeLegacyDamageReference(const CombatStats &attacker,
                                   const CombatStats &defender,
                                   const float baseDamage,
                                   const DamageType type) {
  float multiplier = attacker.damage_multipliers[(int)type];
  float effectiveMult = (multiplier > 0.001f) ? multiplier : 1.0f;
  float damage = baseDamage * effectiveMult;

  float mitigation = 0.0f;
  if (type == DamageType::Physical) {
    float effectiveArmor = defender.armor - attacker.armor_pen;
    int areaLevel = defender.cached_area_level;
    if (areaLevel < 1) {
      areaLevel = 1;
    }
    float armorMult =
        CombatFormula::CalculateArmorMultiplier(effectiveArmor, areaLevel);
    mitigation = 1.0f - armorMult;
  } else {
    using namespace NoMoreDay::Constants::Combat;
    mitigation =
        std::min(defender.resistances[(int)type], Cap::RESISTANCE);
  }
  damage *= (1.0f - mitigation);

  using namespace NoMoreDay::Constants::Combat;
  float reduction = std::min(defender.damage_reduction, Cap::DR);
  float effectiveDr = reduction > 0.0f ? reduction : 0.0f;
  damage *= (1.0f - effectiveDr);
  return std::max(0.0f, damage);
}

} // namespace

TEST_CASE("[Unit] DamagePipelineUnifiedEntry - Legacy formula equivalence") {
  TestSetupScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  attackerStats.damage_multipliers[(int)DamageType::Physical] = 1.0f;
  attackerStats.damage_multipliers[(int)DamageType::Fire] = 1.0f;
  attackerStats.damage_multipliers[(int)DamageType::Lightning] = 1.0f;
  attackerStats.armor_pen = 0.0f;
  attackerStats.crit_chance = 0.0f;

  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  defenderStats.armor = 260.0f;
  defenderStats.resistances[(int)DamageType::Fire] = 0.35f;
  defenderStats.resistances[(int)DamageType::Lightning] = 0.2f;
  defenderStats.damage_reduction = 0.1f;
  defenderStats.cached_area_level = 45;

  auto run_equivalence = [&](DamageType type, Tag typeTag, float baseDamage) {
    DamageRequest req;
    req.attacker = attacker;
    req.defender = defender;
    req.skill_id = 987654321u;
    req.base_pool.Add(typeTag, baseDamage);
    req.is_simulation = true;
    req.additional_tags = Tag::None;

    const auto pipelineResult = DamagePipeline::Calculate(registry, req);
    const float legacyDamage = ComputeLegacyDamageReference(
        attackerStats, defenderStats, baseDamage, type);
    CHECK(pipelineResult.total_damage ==
          doctest::Approx(legacyDamage).epsilon(0.0001f));
  };

  run_equivalence(DamageType::Physical, Tag::Physical, 120.0f);
  run_equivalence(DamageType::Fire, Tag::Fire, 95.0f);
  run_equivalence(DamageType::Lightning, Tag::Lightning, 88.0f);
}

TEST_CASE("[Unit] DamagePipelineUnifiedEntry - Thorns damage bypasses crit and mitigation") {
  TestSetupScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  attackerStats.crit_chance = 100.0f;
  attackerStats.crit_damage = 3.0f;
  attackerStats.damage_multipliers[(int)DamageType::Physical] = 2.0f;

  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  defenderStats.armor = 800.0f;
  defenderStats.damage_reduction = 0.6f;
  defenderStats.cached_area_level = 70;

  DamageRequest normalReq;
  normalReq.attacker = attacker;
  normalReq.defender = defender;
  normalReq.skill_id = 987654322u;
  normalReq.base_pool.Add(Tag::Physical, 37.0f);
  normalReq.additional_tags = Tag::Hit;
  normalReq.is_simulation = true;

  DamageRequest thornsReq = normalReq;
  thornsReq.skip_mitigation = true;
  thornsReq.thorns_like_damage = true;

  const auto normalResult = DamagePipeline::Calculate(registry, normalReq);
  const auto thornsResult = DamagePipeline::Calculate(registry, thornsReq);

  CHECK(thornsResult.total_damage == doctest::Approx(37.0f).epsilon(0.0001f));
  CHECK(thornsResult.is_crit == false);
  CHECK(thornsResult.total_damage > normalResult.total_damage);
}

TEST_CASE("[Unit] DamagePipelineUnifiedEntry - Self damage supports skip_mitigation") {
  TestSetupScope scope;
  entt::registry registry;

  const auto self = registry.create();
  auto &stats = registry.emplace<CombatStats>(self);
  stats.armor = 500.0f;
  stats.damage_reduction = 0.4f;
  stats.cached_area_level = 60;
  stats.crit_chance = 0.0f;
  stats.damage_multipliers[(int)DamageType::Physical] = 1.0f;

  DamageRequest mitigatedReq;
  mitigatedReq.attacker = self;
  mitigatedReq.defender = self;
  mitigatedReq.skill_id = 987654323u;
  mitigatedReq.base_pool.Add(Tag::Physical, 50.0f);
  mitigatedReq.additional_tags = Tag::Hit;
  mitigatedReq.is_simulation = true;

  DamageRequest bypassReq = mitigatedReq;
  bypassReq.skip_mitigation = true;

  const auto mitigated = DamagePipeline::Calculate(registry, mitigatedReq);
  const auto bypassed = DamagePipeline::Calculate(registry, bypassReq);

  CHECK(bypassed.total_damage == doctest::Approx(50.0f).epsilon(0.0001f));
  CHECK(bypassed.total_damage > mitigated.total_damage);
}

TEST_CASE("[Unit] DamagePipelineUnifiedEntry - DamagePayloadContext precedence overrides attacker stats") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  entt::registry registry;

  const auto attacker = registry.create();
  auto &stats = registry.emplace<CombatStats>(attacker);
  stats.min_weapon_damage = 10.0f;
  stats.max_weapon_damage = 10.0f;
  stats.crit_chance = 0.0f;
  stats.crit_damage = 1.5f;
  stats.damage_multipliers[(int)DamageType::Physical] = 1.0f;

  const auto defender = registry.create();
  auto &defStats = registry.emplace<CombatStats>(defender);
  defStats.armor = 0.0f;
  defStats.damage_reduction = 0.0f;
  defStats.cached_area_level = 1;

  DamageRequest req;
  req.attacker = attacker;
  req.defender = defender;
  req.skill_id = 1;
  req.additional_tags = Tag::Hit;
  req.skip_mitigation = true;
  req.is_simulation = true;

  DamagePayloadContext ctx{};
  ctx.base_damage_min = 100.0f;
  ctx.base_damage_max = 100.0f;
  ctx.crit_chance = 1.0f;       // 100% crit chance override
  ctx.crit_multiplier = 2.5f;   // 2.5x crit multiplier override
  ctx.increased_damage = 0.5f;  // +50% increased damage
  ctx.more_damage = 2.0f;       // 2.0x more damage
  ctx.effective_tags = Tag::Physical;
  ctx.source_skill_id = 1;
  req.payload_context = ctx;

  const auto result = DamagePipeline::Calculate(registry, req);

  // Expected calculation:
  // Skill 1: base 10 + 100 weapon * 1.2 mult = 130
  // Multiplier: 100% base + 50% increased = 1.5x
  // More: 1.0 * 2.0 = 2.0x
  // Damage before crit: 130 * 1.5 * 2.0 = 390
  // Crit: guaranteed 1.0 chance -> is_crit = true, multiplier = 2.5x
  // Total damage: 390 * 2.5 = 975
  CHECK(result.is_crit == true);
  CHECK(result.total_damage == doctest::Approx(975.0f));
}

TEST_CASE("[Unit] DamagePipelineUnifiedEntry - Baseline Equivalence with Zero Increased Damage (No +100% Regression)") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  entt::registry registry;

  const auto attacker = registry.create();
  auto &stats = registry.emplace<CombatStats>(attacker);
  stats.min_weapon_damage = 100.0f;
  stats.max_weapon_damage = 100.0f;
  stats.crit_chance = 0.0f;
  stats.crit_damage = 1.5f;
  stats.damage_multipliers[(int)DamageType::Physical] = 1.0f;

  const auto defender = registry.create();
  auto &defStats = registry.emplace<CombatStats>(defender);
  defStats.armor = 0.0f;
  defStats.damage_reduction = 0.0f;
  defStats.cached_area_level = 1;

  // 1. Without payload_context
  DamageRequest reqWithout;
  reqWithout.attacker = attacker;
  reqWithout.defender = defender;
  reqWithout.skill_id = 1;
  reqWithout.additional_tags = Tag::Hit;
  reqWithout.skip_mitigation = true;
  reqWithout.is_simulation = true;
  const auto resWithout = DamagePipeline::Calculate(registry, reqWithout);

  // 2. With payload_context (increased_damage = 0.0f baseline, more_damage = 1.0f)
  DamageRequest reqWith;
  reqWith.attacker = attacker;
  reqWith.defender = defender;
  reqWith.skill_id = 1;
  reqWith.additional_tags = Tag::Hit;
  reqWith.skip_mitigation = true;
  reqWith.is_simulation = true;

  DamagePayloadContext ctx{};
  ctx.base_damage_min = 100.0f;
  ctx.base_damage_max = 100.0f;
  ctx.crit_chance = 0.0f;
  ctx.crit_multiplier = 1.5f;
  ctx.increased_damage = 0.0f; // Baseline: 0.0f fractional increment
  ctx.more_damage = 1.0f;
  ctx.effective_tags = Tag::Physical;
  ctx.source_skill_id = 1;
  reqWith.payload_context = ctx;
  const auto resWith = DamagePipeline::Calculate(registry, reqWith);

  // Expected baseline: Skill 1 base 10 + 100 weapon * 1.2 mult = 130
  // Both paths must yield exactly 130.0f, proving ctx.increased_damage = 0 does NOT duplicate base multiplier
  CHECK(resWithout.total_damage == doctest::Approx(130.0f));
  CHECK(resWith.total_damage == doctest::Approx(130.0f));
  CHECK(resWithout.total_damage == resWith.total_damage);
}

TEST_CASE("[Unit] DamagePipelineUnifiedEntry - Normalized Crit Chance Boundaries and Zero-Override") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  entt::registry registry;

  // Attacker has 100% crit chance in base stats
  const auto attacker = registry.create();
  auto &stats = registry.emplace<CombatStats>(attacker);
  stats.min_weapon_damage = 100.0f;
  stats.max_weapon_damage = 100.0f;
  stats.crit_chance = 1.0f; // 100% crit chance on attacker
  stats.crit_damage = 2.0f;
  stats.damage_multipliers[(int)DamageType::Physical] = 1.0f;

  const auto defender = registry.create();
  auto &defStats = registry.emplace<CombatStats>(defender);
  defStats.armor = 0.0f;
  defStats.damage_reduction = 0.0f;
  defStats.cached_area_level = 1;

  // 1. Explicit 0.0f crit_chance in payload_context MUST override attacker's 100% crit
  DamageRequest reqZeroCrit;
  reqZeroCrit.attacker = attacker;
  reqZeroCrit.defender = defender;
  reqZeroCrit.skill_id = 1;
  reqZeroCrit.additional_tags = Tag::Hit;
  reqZeroCrit.skip_mitigation = true;
  reqZeroCrit.is_simulation = true;

  DamagePayloadContext ctxZero{};
  ctxZero.base_damage_min = 100.0f;
  ctxZero.base_damage_max = 100.0f;
  ctxZero.crit_chance = 0.0f; // Explicitly 0% crit chance override
  ctxZero.crit_multiplier = 2.0f;
  ctxZero.increased_damage = 0.0f;
  ctxZero.more_damage = 1.0f;
  ctxZero.effective_tags = Tag::Physical;
  ctxZero.source_skill_id = 1;
  reqZeroCrit.payload_context = ctxZero;

  const auto resZeroCrit = DamagePipeline::Calculate(registry, reqZeroCrit);
  CHECK_FALSE(resZeroCrit.is_crit);
  CHECK(resZeroCrit.total_damage == doctest::Approx(130.0f));

  // 2. 0.01f normalized crit_chance represents exactly 1% crit (expected mult = 1 + 0.01 * (2 - 1) = 1.01)
  DamageRequest reqOnePct;
  reqOnePct.attacker = attacker;
  reqOnePct.defender = defender;
  reqOnePct.skill_id = 1;
  reqOnePct.additional_tags = Tag::Hit;
  reqOnePct.skip_mitigation = true;
  reqOnePct.is_simulation = true;

  DamagePayloadContext ctxOnePct = ctxZero;
  ctxOnePct.crit_chance = 0.01f; // 1% crit chance
  reqOnePct.payload_context = ctxOnePct;

  const auto resOnePct = DamagePipeline::Calculate(registry, reqOnePct);
  CHECK_FALSE(resOnePct.is_crit);
  CHECK(resOnePct.total_damage == doctest::Approx(130.0f * 1.01f));

  // 3. 1.0f normalized crit_chance represents 100% guaranteed crit (mult = 2.0)
  DamageRequest reqFullCrit;
  reqFullCrit.attacker = attacker;
  reqFullCrit.defender = defender;
  reqFullCrit.skill_id = 1;
  reqFullCrit.additional_tags = Tag::Hit;
  reqFullCrit.skip_mitigation = true;
  reqFullCrit.is_simulation = true;

  DamagePayloadContext ctxFullCrit = ctxZero;
  ctxFullCrit.crit_chance = 1.0f; // 100% crit chance
  reqFullCrit.payload_context = ctxFullCrit;

  const auto resFullCrit = DamagePipeline::Calculate(registry, reqFullCrit);
  CHECK(resFullCrit.is_crit);
  CHECK(resFullCrit.total_damage == doctest::Approx(130.0f * 2.0f));
}

TEST_CASE("[Unit] DamagePipeline - BuffEffect source_skill_id filters Flat resist debuff (SkillOnly)") {
  TestSetupScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  attackerStats.damage_multipliers[(int)DamageType::Fire] = 1.0f;
  attackerStats.armor_pen = 0.0f;
  attackerStats.crit_chance = 0.0f;
  attackerStats.crit_damage = 0.0f;

  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  // resistances 为 AttributePipeline 烘焙后的全局基础值：不含 debuff 修饰符
  // (AttributePipeline 不聚合 ActiveEffects)，此处以基础抗性 0.35 为起点，
  // 减抗 debuff 由 DamagePipeline 聚合生效。
  defenderStats.resistances[(int)DamageType::Fire] = 0.35f;
  defenderStats.damage_reduction = 0.0f;
  defenderStats.cached_area_level = 1;

  // 使用不在 skills.json 中的假 skill_id，避免技能自带基础伤害干扰伤害数值
  constexpr uint32_t kSrcSkill = 990001; // 减抗来源技能
  constexpr uint32_t kOtherSkill = 990002; // 其他技能

  // 给 defender 施加带来源技能归属(kSrcSkill)的火焰减抗 debuff: -12% 抗性
  auto &effects = registry.emplace<ActiveEffectsComponent>(defender);
  BuffEffect erosion;
  erosion.id = "ElementalErosionFire";
  erosion.name = "Elemental Erosion";
  erosion.is_debuff = true;
  erosion.source_skill_id = static_cast<int>(kSrcSkill);
  erosion.modifiers.push_back({.value = -12.0f,
                               .type = StatType::ResistFire,
                               .mode = ModifierMode::Flat});
  effects.effects.push_back(erosion);

  auto calc_damage = [&](uint32_t skill_id) {
    DamageRequest req;
    req.attacker = attacker;
    req.defender = defender;
    req.skill_id = skill_id;
    req.base_pool.Add(Tag::Fire, 100.0f);
    req.additional_tags = Tag::None;
    req.is_simulation = true;
    return DamagePipeline::Calculate(registry, req).total_damage;
  };

  // 来源技能：debuff 减抗聚合生效，抗性 0.35 - 0.12 = 0.23
  const float dmgSrc = calc_damage(kSrcSkill);
  CHECK(dmgSrc == doctest::Approx(100.0f * (1.0f - 0.23f)).epsilon(0.0001f));

  // 其他技能：SkillOnly 归属过滤跳过该减抗，抗性保持基础值 0.35
  const float dmgOther = calc_damage(kOtherSkill);
  CHECK(dmgOther == doctest::Approx(100.0f * (1.0f - 0.35f)).epsilon(0.0001f));

  // 无归属(source_skill_id=0)的减抗对全体伤害生效：替换 debuff 后两技能都吃到减抗
  effects.effects[0].source_skill_id = 0;
  CHECK(calc_damage(kSrcSkill) == doctest::Approx(100.0f * (1.0f - 0.23f)).epsilon(0.0001f));
  CHECK(calc_damage(kOtherSkill) == doctest::Approx(100.0f * (1.0f - 0.23f)).epsilon(0.0001f));
}

} // namespace NoMoreDay
