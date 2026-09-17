#include "doctest.h"

#include "game/systems/modifier/ModifierContext.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace {

using NoMoreDay::ModifierDelta;
using NoMoreDay::ModifierEvalContext;
using NoMoreDay::ModifierEvaluator;
using NoMoreDay::ModifierOp;
using NoMoreDay::ModifierOpCode;
using NoMoreDay::ModifierRecord;
using NoMoreDay::NodePointEntry;

// 技能 ID：10 七星斩 / 11 天剑降临 / 12 血海。
constexpr uint32_t kSevenStarSkill = 10u;
constexpr uint32_t kHeavenlySwordSkill = 11u;
constexpr uint32_t kBloodSeaSkill = 12u;

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

// 单条记录绑定单个节点白名单，按指定点数线性缩放；0 点使算子回落单位元。
ModifierDelta EvalNodeRecord(const ModifierOpCode opcode, const uint32_t skillId,
                             const float perPoint, const uint32_t nodeId,
                             const uint16_t points) {
  ModifierRecord record = MakeDeliveryRecord(opcode, skillId, perPoint);
  record.filter.node_id_whitelist = {nodeId};

  ModifierEvalContext ctx;
  ctx.node_points = {NodePointEntry{nodeId, points}};
  ctx.active_node_ids = {nodeId};

  return ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(&record, 1), ctx);
}

// 多条记录同批求值：覆盖同节点多记录（如 1207 双记录）与跨节点复合缩放。
// nodePoints 按 node_id 升序传入，满足求值层 GetPointsForNode 的二分前提。
ModifierDelta EvalRecords(
    const std::vector<ModifierRecord> &records,
    const std::vector<std::pair<uint32_t, uint16_t>> &nodePoints) {
  ModifierEvalContext ctx;
  for (const auto &[nodeId, points] : nodePoints) {
    ctx.node_points.push_back(NodePointEntry{nodeId, points});
    ctx.active_node_ids.push_back(nodeId);
  }
  return ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(records.data(), records.size()), ctx);
}

} // namespace

// ===== UMR-SKILL-BATCH-4：技能 10/11/12 交付算子孤立求值（设计 §6 R-02） =====

TEST_CASE("[Unit] SkillBatch4Delivery - SevenStarSlash1001BonusCrit") {
  constexpr uint32_t kSkill = kSevenStarSkill;

  // canonical 2010010：节点 1001 每点 +0.02 暴击率，加性累加。
  CHECK(EvalSingle(ModifierOpCode::SKILL_BONUS_CRIT, kSkill, 0.02f)
            .GetSkillBonusCrit(kSkill) == doctest::Approx(0.02f));
  // 4 点 -> 0.02 * 4 = 0.08。
  CHECK(EvalNodeRecord(ModifierOpCode::SKILL_BONUS_CRIT, kSkill, 0.02f, 1001u, 4u)
            .GetSkillBonusCrit(kSkill) == doctest::Approx(0.08f));
  // 0 点：算子不贡献，加性回落 0.0。
  CHECK(EvalNodeRecord(ModifierOpCode::SKILL_BONUS_CRIT, kSkill, 0.02f, 1001u, 0u)
            .GetSkillBonusCrit(kSkill) == doctest::Approx(0.0f));
}

TEST_CASE("[Unit] SkillBatch4Delivery - HeavenlySword1101AreaMult") {
  constexpr uint32_t kSkill = kHeavenlySwordSkill;

  // canonical 2011010：节点 1101 每点范围 +8%，乘算系数 = 1 + 0.08 * 点数。
  CHECK(EvalSingle(ModifierOpCode::SKILL_AREA_MULT, kSkill, 0.08f)
            .GetSkillAreaMult(kSkill) == doctest::Approx(1.08f));
  // 4 点 -> 1 + 0.32 = 1.32。
  CHECK(EvalNodeRecord(ModifierOpCode::SKILL_AREA_MULT, kSkill, 0.08f, 1101u, 4u)
            .GetSkillAreaMult(kSkill) == doctest::Approx(1.32f));
  // 0 点：乘性回落单位元 1.0。
  CHECK(EvalNodeRecord(ModifierOpCode::SKILL_AREA_MULT, kSkill, 0.08f, 1101u, 0u)
            .GetSkillAreaMult(kSkill) == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] SkillBatch4Delivery - HeavenlySword1107AreaMultPenalty") {
  constexpr uint32_t kSkill = kHeavenlySwordSkill;

  // canonical 2011070：节点 1107 点亮即 -30% 范围，乘算系数 = 1 - 0.30 = 0.70。
  CHECK(EvalSingle(ModifierOpCode::SKILL_AREA_MULT, kSkill, -0.30f)
            .GetSkillAreaMult(kSkill) == doctest::Approx(0.70f));
  CHECK(EvalNodeRecord(ModifierOpCode::SKILL_AREA_MULT, kSkill, -0.30f, 1107u, 1u)
            .GetSkillAreaMult(kSkill) == doctest::Approx(0.70f));
  // 0 点：未点亮，无范围惩罚。
  CHECK(EvalNodeRecord(ModifierOpCode::SKILL_AREA_MULT, kSkill, -0.30f, 1107u, 0u)
            .GetSkillAreaMult(kSkill) == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] SkillBatch4Delivery - SevenStarSlash1015DurationFlat") {
  constexpr uint32_t kSkill = kSevenStarSkill;

  // canonical 2010150：节点 1015 每点 +0.03s 无敌时长，加性累加。
  CHECK(EvalSingle(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.03f)
            .GetSkillDurationFlat(kSkill) == doctest::Approx(0.03f));
  // 3 点（节点上限）-> 0.03 * 3 = 0.09。
  CHECK(EvalNodeRecord(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.03f, 1015u, 3u)
            .GetSkillDurationFlat(kSkill) == doctest::Approx(0.09f));
  // 0 点：加性回落 0.0。
  CHECK(EvalNodeRecord(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.03f, 1015u, 0u)
            .GetSkillDurationFlat(kSkill) == doctest::Approx(0.0f));
}

TEST_CASE("[Unit] SkillBatch4Delivery - HeavenlySword1119DurationFlat") {
  constexpr uint32_t kSkill = kHeavenlySwordSkill;

  // canonical 2011190：节点 1119 每点 +0.5s 领域时长，加性累加。
  CHECK(EvalSingle(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.50f)
            .GetSkillDurationFlat(kSkill) == doctest::Approx(0.50f));
  // 3 点（节点上限）-> 0.5 * 3 = 1.50。
  CHECK(EvalNodeRecord(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.50f, 1119u, 3u)
            .GetSkillDurationFlat(kSkill) == doctest::Approx(1.50f));
  CHECK(EvalNodeRecord(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.50f, 1119u, 0u)
            .GetSkillDurationFlat(kSkill) == doctest::Approx(0.0f));
}

TEST_CASE("[Unit] SkillBatch4Delivery - BloodSea1219DurationFlat") {
  constexpr uint32_t kSkill = kBloodSeaSkill;

  // canonical 2012190：节点 1219 每点 +0.6s 血海持续，加性累加。
  CHECK(EvalSingle(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.60f)
            .GetSkillDurationFlat(kSkill) == doctest::Approx(0.60f));
  // 3 点（节点上限）-> 0.6 * 3 = 1.80。
  CHECK(EvalNodeRecord(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.60f, 1219u, 3u)
            .GetSkillDurationFlat(kSkill) == doctest::Approx(1.80f));
  CHECK(EvalNodeRecord(ModifierOpCode::SKILL_DURATION_FLAT, kSkill, 0.60f, 1219u, 0u)
            .GetSkillDurationFlat(kSkill) == doctest::Approx(0.0f));
}

TEST_CASE("[Unit] SkillBatch4Delivery - BloodSea1201MoreDamageMult") {
  constexpr uint32_t kSkill = kBloodSeaSkill;

  // canonical 2012010：节点 1201 每点 More +6%，乘算系数 = 1 + 0.06 * 点数。
  CHECK(EvalSingle(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, kSkill, 0.06f)
            .GetSkillMoreDamageMult(kSkill) == doctest::Approx(1.06f));
  // 4 点（节点上限）-> 1 + 0.24 = 1.24。
  CHECK(EvalNodeRecord(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, kSkill, 0.06f, 1201u, 4u)
            .GetSkillMoreDamageMult(kSkill) == doctest::Approx(1.24f));
  // 0 点：乘性回落单位元 1.0。
  CHECK(EvalNodeRecord(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, kSkill, 0.06f, 1201u, 0u)
            .GetSkillMoreDamageMult(kSkill) == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] SkillBatch4Delivery - BloodSea1207DualRecord") {
  constexpr uint32_t kSkill = kBloodSeaSkill;

  // 节点 1207 无间血狱为同节点双记录：2012070 More +10% 与 2012071 范围 -40%。
  ModifierRecord moreRecord =
      MakeDeliveryRecord(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, kSkill, 0.10f);
  moreRecord.filter.node_id_whitelist = {1207u};
  ModifierRecord areaRecord =
      MakeDeliveryRecord(ModifierOpCode::SKILL_AREA_MULT, kSkill, -0.40f);
  areaRecord.filter.node_id_whitelist = {1207u};

  // 点亮（1 点）：两条记录同时生效，More 1.10 / Area 0.60。
  const ModifierDelta lit =
      EvalRecords({moreRecord, areaRecord}, {{1207u, 1u}});
  CHECK(lit.GetSkillMoreDamageMult(kSkill) == doctest::Approx(1.10f));
  CHECK(lit.GetSkillAreaMult(kSkill) == doctest::Approx(0.60f));

  // 未点亮（0 点）：双记录一并跳过，两项均回落单位元。
  const ModifierDelta unlit =
      EvalRecords({moreRecord, areaRecord}, {{1207u, 0u}});
  CHECK(unlit.GetSkillMoreDamageMult(kSkill) == doctest::Approx(1.0f));
  CHECK(unlit.GetSkillAreaMult(kSkill) == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] SkillBatch4Delivery - CompoundCrossNodeScaling") {
  // 技能 11 复合：1101（4 点 +8%）与 1107（1 点 -30%）同批 -> 范围乘算累乘
  // 1.32 * 0.70 = 0.924；1119（3 点 +0.5s）加性 -> 时长 +1.50s。
  ModifierRecord area1101 =
      MakeDeliveryRecord(ModifierOpCode::SKILL_AREA_MULT, kHeavenlySwordSkill, 0.08f);
  area1101.filter.node_id_whitelist = {1101u};
  ModifierRecord area1107 =
      MakeDeliveryRecord(ModifierOpCode::SKILL_AREA_MULT, kHeavenlySwordSkill, -0.30f);
  area1107.filter.node_id_whitelist = {1107u};
  ModifierRecord duration1119 = MakeDeliveryRecord(
      ModifierOpCode::SKILL_DURATION_FLAT, kHeavenlySwordSkill, 0.50f);
  duration1119.filter.node_id_whitelist = {1119u};

  const ModifierDelta heavenly = EvalRecords(
      {area1101, area1107, duration1119},
      {{1101u, 4u}, {1107u, 1u}, {1119u, 3u}});
  CHECK(heavenly.GetSkillAreaMult(kHeavenlySwordSkill) ==
        doctest::Approx(0.924f));
  CHECK(heavenly.GetSkillDurationFlat(kHeavenlySwordSkill) ==
        doctest::Approx(1.50f));

  // 技能 12 复合：1201（4 点 +6%）与 1207（1 点 +10%）More 累乘
  // 1.24 * 1.10 = 1.364，1207 的范围惩罚同步生效 0.60。
  ModifierRecord more1201 =
      MakeDeliveryRecord(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, kBloodSeaSkill, 0.06f);
  more1201.filter.node_id_whitelist = {1201u};
  ModifierRecord more1207 =
      MakeDeliveryRecord(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, kBloodSeaSkill, 0.10f);
  more1207.filter.node_id_whitelist = {1207u};
  ModifierRecord area1207 =
      MakeDeliveryRecord(ModifierOpCode::SKILL_AREA_MULT, kBloodSeaSkill, -0.40f);
  area1207.filter.node_id_whitelist = {1207u};

  const ModifierDelta bloodSea =
      EvalRecords({more1201, more1207, area1207}, {{1201u, 4u}, {1207u, 1u}});
  CHECK(bloodSea.GetSkillMoreDamageMult(kBloodSeaSkill) ==
        doctest::Approx(1.364f));
  CHECK(bloodSea.GetSkillAreaMult(kBloodSeaSkill) == doctest::Approx(0.60f));
}

TEST_CASE("[Unit] SkillBatch4Delivery - ZeroPointIdentity") {
  // 全部 9 条 canonical 记录在白名单节点 0 点时整体跳过：加性回落 0，
  // 乘性回落 1，不得产生任何交付 delta（迁移等价的空档基线）。
  ModifierRecord crit1001 =
      MakeDeliveryRecord(ModifierOpCode::SKILL_BONUS_CRIT, kSevenStarSkill, 0.02f);
  crit1001.filter.node_id_whitelist = {1001u};
  ModifierRecord duration1015 = MakeDeliveryRecord(
      ModifierOpCode::SKILL_DURATION_FLAT, kSevenStarSkill, 0.03f);
  duration1015.filter.node_id_whitelist = {1015u};
  ModifierRecord area1101 =
      MakeDeliveryRecord(ModifierOpCode::SKILL_AREA_MULT, kHeavenlySwordSkill, 0.08f);
  area1101.filter.node_id_whitelist = {1101u};
  ModifierRecord area1107 =
      MakeDeliveryRecord(ModifierOpCode::SKILL_AREA_MULT, kHeavenlySwordSkill, -0.30f);
  area1107.filter.node_id_whitelist = {1107u};
  ModifierRecord duration1119 = MakeDeliveryRecord(
      ModifierOpCode::SKILL_DURATION_FLAT, kHeavenlySwordSkill, 0.50f);
  duration1119.filter.node_id_whitelist = {1119u};
  ModifierRecord more1201 =
      MakeDeliveryRecord(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, kBloodSeaSkill, 0.06f);
  more1201.filter.node_id_whitelist = {1201u};
  ModifierRecord more1207 =
      MakeDeliveryRecord(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, kBloodSeaSkill, 0.10f);
  more1207.filter.node_id_whitelist = {1207u};
  ModifierRecord area1207 =
      MakeDeliveryRecord(ModifierOpCode::SKILL_AREA_MULT, kBloodSeaSkill, -0.40f);
  area1207.filter.node_id_whitelist = {1207u};
  ModifierRecord duration1219 = MakeDeliveryRecord(
      ModifierOpCode::SKILL_DURATION_FLAT, kBloodSeaSkill, 0.60f);
  duration1219.filter.node_id_whitelist = {1219u};

  const ModifierDelta delta = EvalRecords(
      {crit1001, duration1015, area1101, area1107, duration1119, more1201,
       more1207, area1207, duration1219},
      {{1001u, 0u}, {1015u, 0u}, {1101u, 0u}, {1107u, 0u},
       {1119u, 0u}, {1201u, 0u}, {1207u, 0u}, {1219u, 0u}});

  CHECK(delta.GetSkillBonusCrit(kSevenStarSkill) == doctest::Approx(0.0f));
  CHECK(delta.GetSkillDurationFlat(kSevenStarSkill) == doctest::Approx(0.0f));
  CHECK(delta.GetSkillAreaMult(kHeavenlySwordSkill) == doctest::Approx(1.0f));
  CHECK(delta.GetSkillDurationFlat(kHeavenlySwordSkill) == doctest::Approx(0.0f));
  CHECK(delta.GetSkillMoreDamageMult(kBloodSeaSkill) == doctest::Approx(1.0f));
  CHECK(delta.GetSkillAreaMult(kBloodSeaSkill) == doctest::Approx(1.0f));
  CHECK(delta.GetSkillDurationFlat(kBloodSeaSkill) == doctest::Approx(0.0f));
}
