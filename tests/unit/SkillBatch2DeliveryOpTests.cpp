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

TEST_CASE("[Unit] SkillBatch2Delivery - DurationFlatScaling") {
  constexpr uint32_t kSkill = 201u;

  // 42 号算子 41 SKILL_DURATION_FLAT：每点 +0.5s，纯加性。
  CHECK(EvalSingle(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.5f)
            .GetSkillDurationFlat(kSkill) == doctest::Approx(0.5f));
  // 每点 +0.5、分配 3 点 -> 1.5s。
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.5f, 3u)
            .GetSkillDurationFlat(kSkill) == doctest::Approx(1.5f));

  // 多条记录加性累加：0.5 + 0.5 = 1.0，不是乘算。
  std::vector<ModifierRecord> records;
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.5f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.5f));

  ModifierEvalContext ctx;
  const ModifierDelta delta = ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(records), ctx);
  CHECK(delta.GetSkillDurationFlat(kSkill) == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] SkillBatch2Delivery - DurationFlatNegativeNotClampedAtEval") {
  constexpr uint32_t kSkill = 202u;

  // 加性容器本身不做下限钳制：负值即缩短，取 max(0, duration) 由烘焙消费端承担。
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, -0.5f, 3u)
            .GetSkillDurationFlat(kSkill) == doctest::Approx(-1.5f));

  std::vector<ModifierRecord> records;
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, -2.0f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.5f));

  ModifierEvalContext ctx;
  const ModifierDelta delta = ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(records), ctx);
  CHECK(delta.GetSkillDurationFlat(kSkill) == doctest::Approx(-1.5f));
}

TEST_CASE("[Unit] SkillBatch2Delivery - SpeedMultScaling") {
  constexpr uint32_t kSkill = 203u;

  // 42 SKILL_SPEED_MULT：每点 1.0 + 0.25 * N。
  CHECK(EvalSingle(ModifierOpCode::SKILL_SPEED_MULT, kSkill, 0.25f)
            .GetSkillSpeedMult(kSkill) == doctest::Approx(1.25f));
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_SPEED_MULT, kSkill, 0.25f, 3u)
            .GetSkillSpeedMult(kSkill) == doctest::Approx(1.75f));
  // 0 点（无分配节点）回落单位元 1.0。
  CHECK(EvalAtPoints(ModifierOpCode::SKILL_SPEED_MULT, kSkill, 0.25f, 0u)
            .GetSkillSpeedMult(kSkill) == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] SkillBatch2Delivery - SpeedMultMultiplicativeAccumulation") {
  constexpr uint32_t kSkill = 204u;

  // 多条记录乘性累乘：1.25 * 1.25 = 1.5625。
  std::vector<ModifierRecord> records;
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_SPEED_MULT, kSkill, 0.25f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_SPEED_MULT, kSkill, 0.25f));

  ModifierEvalContext ctx;
  const ModifierDelta delta = ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(records), ctx);
  CHECK(delta.GetSkillSpeedMult(kSkill) == doctest::Approx(1.5625f));
}

TEST_CASE("[Unit] SkillBatch2Delivery - DurationAndSpeedDefaultSemantics") {
  constexpr uint32_t kSkill = 205u;

  // 键缺失回落单位元：加性 0.0f，乘性 1.0f；只读查询不得为此分配容器。
  const ModifierDelta empty;
  CHECK(empty.GetSkillDurationFlat(kSkill) == doctest::Approx(0.0f));
  CHECK(empty.GetSkillSpeedMult(kSkill) == doctest::Approx(1.0f));
  CHECK(empty.skill_duration_flat.empty());
  CHECK(empty.skill_speed_mult.empty());
}

TEST_CASE("[Unit] SkillBatch2Delivery - MergeFromContainerSemantics") {
  constexpr uint32_t kSkill = 206u;
  constexpr uint32_t kOtherSkill = 207u;

  ModifierDelta a;
  a.AddSkillDurationFlat(kSkill, 1.0f);
  a.AddSkillSpeedMult(kSkill, 1.25f);

  ModifierDelta b;
  b.AddSkillDurationFlat(kSkill, 0.5f);
  b.AddSkillDurationFlat(kOtherSkill, -0.5f); // 目标侧新键：走插入路径
  b.AddSkillSpeedMult(kSkill, 1.25f);
  b.AddSkillSpeedMult(kOtherSkill, 1.5f); // 目标侧新键：走插入路径

  a.MergeFrom(b);

  // 加性容器累加，乘性容器累乘，且两条容器各自处理新键插入。
  CHECK(a.GetSkillDurationFlat(kSkill) == doctest::Approx(1.5f));
  CHECK(a.GetSkillDurationFlat(kOtherSkill) == doctest::Approx(-0.5f));
  CHECK(a.GetSkillSpeedMult(kSkill) == doctest::Approx(1.5625f));
  CHECK(a.GetSkillSpeedMult(kOtherSkill) == doctest::Approx(1.5f));

  // 与另两条容器语义相互独立：交付容器未被本次合并触碰。
  CHECK(a.GetSkillRangeMult(kSkill) == doctest::Approx(1.0f));
}
