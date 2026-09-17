#include "doctest.h"

#include "game/systems/modifier/ModifierContext.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace {

using NoMoreDay::ModifierDelta;
using NoMoreDay::ModifierEvalContext;
using NoMoreDay::ModifierEvaluator;
using NoMoreDay::ModifierOp;
using NoMoreDay::ModifierOpCode;
using NoMoreDay::ModifierRecord;

ModifierRecord MakeDeliveryRecord(const ModifierOpCode opcode,
                                  const uint32_t skillId,
                                  const float perPoint) {
  ModifierRecord record;
  ModifierOp op;
  op.opcode = opcode;
  op.param_u32 = skillId;
  op.param_f32 = perPoint;
  record.ops.push_back(op);
  return record;
}

// 空 filter + 空 node_points：有效点数为 1，孤立检验单点数值。
ModifierDelta EvalSingle(const ModifierOpCode opcode, const uint32_t skillId,
                         const float perPoint) {
  const ModifierRecord record = MakeDeliveryRecord(opcode, skillId, perPoint);
  ModifierEvalContext ctx;
  return ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(&record, 1), ctx);
}

// 指定节点分配点数，检验按点数线性缩放；0 点使记录整体跳过，回落单位元。
ModifierDelta EvalAtPoints(const ModifierOpCode opcode, const uint32_t skillId,
                           const float perPoint, const uint16_t points) {
  ModifierRecord record = MakeDeliveryRecord(opcode, skillId, perPoint);
  record.filter.node_id_whitelist = {100u};

  ModifierEvalContext ctx;
  ctx.node_points = {{100u, points}};
  ctx.active_node_ids = {100u};

  return ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(&record, 1), ctx);
}

} // namespace

// ===== UMR-SKILL-BATCH-3：技能 7/8/9 交付算子孤立求值（设计 §5.3） =====

TEST_CASE("[Unit] SkillBatch3Delivery - MindBlade700ManaCostMultiplier") {
  constexpr uint32_t kSkill = 7u;

  // 36 SKILL_MANA_COST_MULT：系数 = max(0, 1 - 每点幅度 * 点数)；
  // canonical 700 每点 +0.10（正幅度即法耗折扣），1 点 -> 1 - 0.10 = 0.90。
  CHECK(EvalSingle(ModifierOpCode::SKILL_MANA_COST_MULT, kSkill, 0.10f)
            .GetManaCostMultiplier(kSkill) == doctest::Approx(0.90f));
  // 4 点 -> 1 - 0.40 = 0.60。
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_MANA_COST_MULT, kSkill, 0.10f, 4u)
            .GetManaCostMultiplier(kSkill) == doctest::Approx(0.60f));
  // 0 点：记录不生效，键缺失回落乘性单位元 1.0。
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_MANA_COST_MULT, kSkill, 0.10f, 0u)
            .GetManaCostMultiplier(kSkill) == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] SkillBatch3Delivery - MindBlade701MoreDamageMultiplier") {
  constexpr uint32_t kSkill = 7u;

  // 30 SKILL_MORE_DAMAGE_MULT：系数 = 1 + 每点幅度 * 点数；701 每点 +0.10。
  CHECK(EvalSingle(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, kSkill, 0.10f)
            .GetSkillMoreDamageMult(kSkill) == doctest::Approx(1.10f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, kSkill, 0.10f, 4u)
            .GetSkillMoreDamageMult(kSkill) == doctest::Approx(1.40f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, kSkill, 0.10f, 0u)
            .GetSkillMoreDamageMult(kSkill) == doctest::Approx(1.0f));

  // 多条记录乘性累乘：1.10 * 1.10 = 1.21，而非加性叠加。
  std::vector<ModifierRecord> records;
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, kSkill, 0.10f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, kSkill, 0.10f));

  ModifierEvalContext ctx;
  const ModifierDelta delta = ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(records), ctx);
  CHECK(delta.GetSkillMoreDamageMult(kSkill) == doctest::Approx(1.21f));
}

TEST_CASE("[Unit] SkillBatch3Delivery - MindBlade702AreaMultiplier") {
  constexpr uint32_t kSkill = 7u;

  // 35 SKILL_AREA_MULT：系数 = 1 + 每点幅度 * 点数；702 每点 +0.10。
  CHECK(EvalSingle(ModifierOpCode::SKILL_AREA_MULT, kSkill, 0.10f)
            .GetSkillAreaMult(kSkill) == doctest::Approx(1.10f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_AREA_MULT, kSkill, 0.10f, 4u)
            .GetSkillAreaMult(kSkill) == doctest::Approx(1.40f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_AREA_MULT, kSkill, 0.10f, 0u)
            .GetSkillAreaMult(kSkill) == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] SkillBatch3Delivery - MindBlade703RangeMultiplier") {
  constexpr uint32_t kSkill = 7u;

  // 40 SKILL_RANGE_MULT：系数 = 1 + 每点幅度 * 点数；703 每点 +0.10。
  CHECK(EvalSingle(ModifierOpCode::SKILL_RANGE_MULT, kSkill, 0.10f)
            .GetSkillRangeMult(kSkill) == doctest::Approx(1.10f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_RANGE_MULT, kSkill, 0.10f, 4u)
            .GetSkillRangeMult(kSkill) == doctest::Approx(1.40f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_RANGE_MULT, kSkill, 0.10f, 0u)
            .GetSkillRangeMult(kSkill) == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] SkillBatch3Delivery - MindBlade732ManaPenaltyMultiplier") {
  constexpr uint32_t kSkill = 7u;

  // 732 步影随行惩罚复用 36 SKILL_MANA_COST_MULT，但每点幅度为负 -0.50：
  // 系数 = max(0, 1 - (-0.50) * 点数)，即点数越高法耗惩罚越重。
  CHECK(EvalSingle(ModifierOpCode::SKILL_MANA_COST_MULT, kSkill, -0.50f)
            .GetManaCostMultiplier(kSkill) == doctest::Approx(1.50f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_MANA_COST_MULT, kSkill, -0.50f, 2u)
            .GetManaCostMultiplier(kSkill) == doctest::Approx(2.00f));
  // 0 点回落单位元，惩罚不生效。
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_MANA_COST_MULT, kSkill, -0.50f, 0u)
            .GetManaCostMultiplier(kSkill) == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] SkillBatch3Delivery - BladeBoomerang800ManaCostFlat") {
  constexpr uint32_t kSkill = 8u;

  // 38 SKILL_MANA_COST_FLAT：每点绝对法耗增减（800 每点 -1.0），加性累加。
  CHECK(EvalSingle(ModifierOpCode::SKILL_MANA_COST_FLAT, kSkill, -1.0f)
            .GetSkillManaCostFlat(kSkill) == doctest::Approx(-1.0f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_MANA_COST_FLAT, kSkill, -1.0f, 4u)
            .GetSkillManaCostFlat(kSkill) == doctest::Approx(-4.0f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_MANA_COST_FLAT, kSkill, -1.0f, 0u)
            .GetSkillManaCostFlat(kSkill) == doctest::Approx(0.0f));

  // 多条记录线性累减：-1 + -1 = -2（加性容器，非相乘）。
  std::vector<ModifierRecord> records;
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_MANA_COST_FLAT, kSkill, -1.0f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_MANA_COST_FLAT, kSkill, -1.0f));

  ModifierEvalContext ctx;
  const ModifierDelta delta = ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(records), ctx);
  CHECK(delta.GetSkillManaCostFlat(kSkill) == doctest::Approx(-2.0f));

  // 加性容器本身不做下限钳制（负值原样保留）；法耗下限 0 由消费端
  // （Baker 步骤 3 的 max(0, base + flat)）承担。混合正负记录线性累加：
  // -1.0 + 0.5 = -0.5，负的净额原样保留而非被钳到 0。
  std::vector<ModifierRecord> mixedRecords;
  mixedRecords.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_MANA_COST_FLAT, kSkill, -1.0f));
  mixedRecords.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_MANA_COST_FLAT, kSkill, 0.5f));

  const ModifierDelta mixedDelta = ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(mixedRecords), ctx);
  CHECK(mixedDelta.GetSkillManaCostFlat(kSkill) == doctest::Approx(-0.5f));
}

TEST_CASE("[Unit] SkillBatch3Delivery - BladeBoomerang801SpeedMultiplier") {
  constexpr uint32_t kSkill = 8u;

  // 42 SKILL_SPEED_MULT：系数 = 1 + 每点幅度 * 点数；801 每点 +0.15。
  CHECK(EvalSingle(ModifierOpCode::SKILL_SPEED_MULT, kSkill, 0.15f)
            .GetSkillSpeedMult(kSkill) == doctest::Approx(1.15f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_SPEED_MULT, kSkill, 0.15f, 4u)
            .GetSkillSpeedMult(kSkill) == doctest::Approx(1.60f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_SPEED_MULT, kSkill, 0.15f, 0u)
            .GetSkillSpeedMult(kSkill) == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] SkillBatch3Delivery - BladeBoomerang801RangeMultiplier") {
  constexpr uint32_t kSkill = 8u;

  // 40 SKILL_RANGE_MULT：系数 = 1 + 每点幅度 * 点数；801 每点 +0.15。
  CHECK(EvalSingle(ModifierOpCode::SKILL_RANGE_MULT, kSkill, 0.15f)
            .GetSkillRangeMult(kSkill) == doctest::Approx(1.15f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_RANGE_MULT, kSkill, 0.15f, 4u)
            .GetSkillRangeMult(kSkill) == doctest::Approx(1.60f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_RANGE_MULT, kSkill, 0.15f, 0u)
            .GetSkillRangeMult(kSkill) == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] SkillBatch3Delivery - BladeBoomerang810DurationFlat") {
  constexpr uint32_t kSkill = 8u;

  // 41 SKILL_DURATION_FLAT：每点绝对秒数加算；810 每点 +0.80。
  CHECK(EvalSingle(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.80f)
            .GetSkillDurationFlat(kSkill) == doctest::Approx(0.80f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.80f, 4u)
            .GetSkillDurationFlat(kSkill) == doctest::Approx(3.20f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.80f, 0u)
            .GetSkillDurationFlat(kSkill) == doctest::Approx(0.0f));
}

TEST_CASE("[Unit] SkillBatch3Delivery - PhantomTrance975DurationFlat") {
  constexpr uint32_t kSkill = 9u;

  // 41 SKILL_DURATION_FLAT：975 延命每点 +0.25s，加性累加。
  CHECK(EvalSingle(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.25f)
            .GetSkillDurationFlat(kSkill) == doctest::Approx(0.25f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.25f, 4u)
            .GetSkillDurationFlat(kSkill) == doctest::Approx(1.00f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.25f, 0u)
            .GetSkillDurationFlat(kSkill) == doctest::Approx(0.0f));

  // 多条记录加性叠加：0.25 + 0.25 = 0.50，而不是乘算。
  std::vector<ModifierRecord> records;
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.25f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.25f));

  ModifierEvalContext ctx;
  const ModifierDelta delta = ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(records), ctx);
  CHECK(delta.GetSkillDurationFlat(kSkill) == doctest::Approx(0.50f));
}

TEST_CASE("[Unit] SkillBatch3Delivery - PhantomTrance986CooldownFlat") {
  constexpr uint32_t kSkill = 9u;

  // 31 SKILL_COOLDOWN_FLAT：每点绝对秒数（负值 = 减冷却）；986 每点 -1.0。
  CHECK(EvalSingle(ModifierOpCode::SKILL_COOLDOWN_FLAT, kSkill, -1.0f)
            .GetSkillCooldownFlat(kSkill) == doctest::Approx(-1.0f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_COOLDOWN_FLAT, kSkill, -1.0f, 4u)
            .GetSkillCooldownFlat(kSkill) == doctest::Approx(-4.0f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_COOLDOWN_FLAT, kSkill, -1.0f, 0u)
            .GetSkillCooldownFlat(kSkill) == doctest::Approx(0.0f));

  // 多条记录加性累减：-1 + -1 = -2；冷却下限由 Baker 终局覆盖负责。
  std::vector<ModifierRecord> records;
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_COOLDOWN_FLAT, kSkill, -1.0f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_COOLDOWN_FLAT, kSkill, -1.0f));

  ModifierEvalContext ctx;
  const ModifierDelta delta = ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(records), ctx);
  CHECK(delta.GetSkillCooldownFlat(kSkill) == doctest::Approx(-2.0f));
}
