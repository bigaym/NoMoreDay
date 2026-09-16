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

// 指定节点分配点数，检验按点数线性缩放。
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

TEST_CASE("[Unit] SkillBatch1Delivery - ProjectilesScaling") {
  constexpr uint32_t kSkill = 101u;

  // 每点 +1：单点与多点线性。
  CHECK(EvalSingle(ModifierOpCode::SKILL_PROJECTILES_ADD, kSkill, 1.0f)
            .GetSkillProjectiles(kSkill) == 1);
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_PROJECTILES_ADD, kSkill, 1.0f, 3u)
            .GetSkillProjectiles(kSkill) == 3);
  // 每点 +2、分配 2 点 -> 4。
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_PROJECTILES_ADD, kSkill, 2.0f, 2u)
            .GetSkillProjectiles(kSkill) == 4);
}

TEST_CASE("[Unit] SkillBatch1Delivery - ManaCostFlatAccumulation") {
  constexpr uint32_t kSkill = 102u;

  // 负值 = 减免，纯加性累加；下限 0 由消费端（烘焙）负责，本例只断言累加值。
  CHECK(EvalSingle(ModifierOpCode::SKILL_MANA_COST_FLAT, kSkill, -10.0f)
            .GetSkillManaCostFlat(kSkill) == doctest::Approx(-10.0f));

  std::vector<ModifierRecord> records;
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_MANA_COST_FLAT, kSkill, -10.0f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_MANA_COST_FLAT, kSkill, -5.0f));

  ModifierEvalContext ctx;
  const ModifierDelta delta = ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(records), ctx);
  // 减免叠加不在此钳制，消费端再对法耗取 max(0, ...)。
  CHECK(delta.GetSkillManaCostFlat(kSkill) == doctest::Approx(-15.0f));
}

TEST_CASE("[Unit] SkillBatch1Delivery - BonusCritDamageAccumulation") {
  constexpr uint32_t kSkill = 103u;

  CHECK(EvalSingle(ModifierOpCode::SKILL_BONUS_CRIT_DAMAGE, kSkill, 0.15f)
            .GetSkillBonusCritDamage(kSkill) == doctest::Approx(0.15f));

  std::vector<ModifierRecord> records;
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_BONUS_CRIT_DAMAGE, kSkill, 0.15f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_BONUS_CRIT_DAMAGE, kSkill, 0.05f));

  ModifierEvalContext ctx;
  const ModifierDelta delta = ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(records), ctx);
  CHECK(delta.GetSkillBonusCritDamage(kSkill) == doctest::Approx(0.20f));
}

TEST_CASE("[Unit] SkillBatch1Delivery - RangeMultScaling") {
  constexpr uint32_t kSkill = 104u;

  // 每点 +10%：单点为 1.10，点数缩放为 1.0 + 0.1 * N。
  CHECK(EvalSingle(ModifierOpCode::SKILL_RANGE_MULT, kSkill, 0.10f)
            .GetSkillRangeMult(kSkill) == doctest::Approx(1.10f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_RANGE_MULT, kSkill, 0.10f, 3u)
            .GetSkillRangeMult(kSkill) == doctest::Approx(1.30f));

  // 多条记录乘性累乘：1.10 * 1.10 = 1.21。
  std::vector<ModifierRecord> records;
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_RANGE_MULT, kSkill, 0.10f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_RANGE_MULT, kSkill, 0.10f));

  ModifierEvalContext ctx;
  const ModifierDelta delta = ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(records), ctx);
  CHECK(delta.GetSkillRangeMult(kSkill) == doctest::Approx(1.21f));
}

TEST_CASE("[Unit] SkillBatch1Delivery - DefaultSemantics") {
  constexpr uint32_t kSkill = 105u;

  // 键缺失时回落单位元：加性 0 / 0.0f，乘性 1.0f。
  const ModifierDelta empty;
  CHECK(empty.GetSkillProjectiles(kSkill) == 0);
  CHECK(empty.GetSkillManaCostFlat(kSkill) == doctest::Approx(0.0f));
  CHECK(empty.GetSkillBonusCritDamage(kSkill) == doctest::Approx(0.0f));
  CHECK(empty.GetSkillRangeMult(kSkill) == doctest::Approx(1.0f));
  // Get 为只读查询：空容器查询后仍保持为空，不得为此分配。
  CHECK(empty.skill_projectiles_add.empty());
  CHECK(empty.skill_mana_cost_flat.empty());
  CHECK(empty.skill_bonus_crit_damage.empty());
  CHECK(empty.skill_range_mult.empty());
}

TEST_CASE("[Unit] SkillBatch1Delivery - MergeFromContainerSemantics") {
  constexpr uint32_t kSkill = 106u;
  constexpr uint32_t kOtherSkill = 107u;

  ModifierDelta a;
  a.AddSkillProjectiles(kSkill, 2);
  a.AddSkillManaCostFlat(kSkill, -5.0f);
  a.AddSkillBonusCritDamage(kSkill, 0.20f);
  a.AddSkillRangeMult(kSkill, 1.5f);

  ModifierDelta b;
  b.AddSkillProjectiles(kSkill, 3);
  b.AddSkillProjectiles(kOtherSkill, 1); // 目标侧新键：走插入路径
  b.AddSkillManaCostFlat(kSkill, -2.0f);
  b.AddSkillBonusCritDamage(kSkill, 0.10f);
  b.AddSkillRangeMult(kSkill, 0.5f);

  a.MergeFrom(b);

  // 加性容器累加。
  CHECK(a.GetSkillProjectiles(kSkill) == 5);
  CHECK(a.GetSkillProjectiles(kOtherSkill) == 1);
  CHECK(a.GetSkillManaCostFlat(kSkill) == doctest::Approx(-7.0f));
  CHECK(a.GetSkillBonusCritDamage(kSkill) == doctest::Approx(0.30f));

  // 乘性容器累乘。
  CHECK(a.GetSkillRangeMult(kSkill) == doctest::Approx(0.75f));
}
