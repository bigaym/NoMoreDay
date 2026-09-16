#include "doctest.h"

#include "game/systems/modifier/ModifierContext.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"
#include "game/systems/modifier/ModifierRuntimeTypes.hpp"

#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace {

using NoMoreDay::ModifierDelta;
using NoMoreDay::ModifierEvalContext;
using NoMoreDay::ModifierEvaluator;
using NoMoreDay::ModifierOp;
using NoMoreDay::ModifierOpCategory;
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

template <typename T>
void AppendStructDelivery(std::vector<uint8_t> &out, const T &value) {
  const auto *bytes = reinterpret_cast<const uint8_t *>(&value);
  out.insert(out.end(), bytes, bytes + sizeof(T));
}

// 单记录双算子：一个技能交付算子 + 一个属性算子，用于验证类别掩码过滤。
std::vector<uint8_t> BuildDeliveryCategoryRuntimeBlob() {
  NoMoreDay::ModifierRuntimeHeader header;
  header.record_count = 1;
  header.filter_count = 1;
  header.op_count = 2;
  header.index_count = 0;
  header.records_offset = sizeof(NoMoreDay::ModifierRuntimeHeader);
  header.filters_offset =
      header.records_offset + sizeof(NoMoreDay::ModifierRuntimeRecord);
  header.ops_offset =
      header.filters_offset + sizeof(NoMoreDay::ModifierRuntimeFilter);
  header.index_offset = header.ops_offset + 2u * sizeof(NoMoreDay::ModifierRuntimeOp);
  header.crc32 = 0;

  NoMoreDay::ModifierRuntimeRecord record;
  record.id = 6001u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 2;

  NoMoreDay::ModifierRuntimeFilter filter;

  NoMoreDay::ModifierRuntimeOp skillOp;
  skillOp.opcode = static_cast<uint16_t>(ModifierOpCode::SKILL_MORE_DAMAGE_MULT);
  skillOp.param_u32 = 3u;
  skillOp.param_f32 = 0.25f;

  NoMoreDay::ModifierRuntimeOp statOp;
  statOp.opcode = static_cast<uint16_t>(ModifierOpCode::ADD_STAT_FLAT);
  statOp.param_u32 = 4u;
  statOp.param_f32 = 50.0f;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) +
               2u * sizeof(NoMoreDay::ModifierRuntimeOp));
  AppendStructDelivery(blob, header);
  AppendStructDelivery(blob, record);
  AppendStructDelivery(blob, filter);
  AppendStructDelivery(blob, skillOp);
  AppendStructDelivery(blob, statOp);
  return blob;
}

} // namespace

TEST_CASE("[Unit] SkillSpecDelivery - AllOpcodeSinglePointMath") {
  constexpr uint32_t kSkill = 77u;

  CHECK(EvalSingle(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, kSkill, 0.10f)
            .GetSkillMoreDamageMult(kSkill) == doctest::Approx(1.10f));
  // 负系数：More 伤害可为减益。
  CHECK(EvalSingle(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, kSkill, -0.15f)
            .GetSkillMoreDamageMult(kSkill) == doctest::Approx(0.85f));
  CHECK(EvalSingle(ModifierOpCode::SKILL_COOLDOWN_FLAT, kSkill, -1.0f)
            .GetSkillCooldownFlat(kSkill) == doctest::Approx(-1.0f));
  CHECK(EvalSingle(ModifierOpCode::SKILL_COOLDOWN_MULT, kSkill, 0.15f)
            .GetSkillCooldownMult(kSkill) == doctest::Approx(1.15f));
  CHECK(EvalSingle(ModifierOpCode::SKILL_CHARGES_ADD, kSkill, 1.0f)
            .GetSkillCharges(kSkill) == 1);
  CHECK(EvalSingle(ModifierOpCode::SKILL_BONUS_CRIT, kSkill, 0.02f)
            .GetSkillBonusCrit(kSkill) == doctest::Approx(0.02f));
  CHECK(EvalSingle(ModifierOpCode::SKILL_AREA_MULT, kSkill, 0.20f)
            .GetSkillAreaMult(kSkill) == doctest::Approx(1.20f));
  CHECK(EvalSingle(ModifierOpCode::SKILL_MANA_COST_MULT, kSkill, 0.15f)
            .GetManaCostMultiplier(kSkill) == doctest::Approx(0.85f));

  // 缺省值语义：键缺失时回落单位元。
  const ModifierDelta empty;
  CHECK(empty.GetSkillMoreDamageMult(kSkill) == doctest::Approx(1.0f));
  CHECK(empty.GetSkillCooldownFlat(kSkill) == doctest::Approx(0.0f));
  CHECK(empty.GetSkillCooldownMult(kSkill) == doctest::Approx(1.0f));
  CHECK(empty.GetSkillCharges(kSkill) == 0);
  CHECK(empty.GetSkillBonusCrit(kSkill) == doctest::Approx(0.0f));
  CHECK(empty.GetSkillAreaMult(kSkill) == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] SkillSpecDelivery - MultiRecordAccumulation") {
  constexpr uint32_t kSkill = 88u;
  std::vector<ModifierRecord> records;
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, kSkill, 0.10f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, kSkill, 0.05f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_COOLDOWN_MULT, kSkill, 0.15f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_COOLDOWN_MULT, kSkill, 0.15f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_AREA_MULT, kSkill, 0.20f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_AREA_MULT, kSkill, 0.20f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_MANA_COST_MULT, kSkill, 0.15f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_MANA_COST_MULT, kSkill, 0.15f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_COOLDOWN_FLAT, kSkill, -1.0f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_COOLDOWN_FLAT, kSkill, -0.5f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_BONUS_CRIT, kSkill, 0.02f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_BONUS_CRIT, kSkill, 0.03f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_CHARGES_ADD, kSkill, 1.0f));
  records.push_back(
      MakeDeliveryRecord(ModifierOpCode::SKILL_CHARGES_ADD, kSkill, 2.0f));

  ModifierEvalContext ctx;
  const ModifierDelta delta = ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(records), ctx);

  CHECK(delta.GetSkillMoreDamageMult(kSkill) == doctest::Approx(1.10f * 1.05f));
  CHECK(delta.GetSkillCooldownMult(kSkill) == doctest::Approx(1.15f * 1.15f));
  CHECK(delta.GetSkillAreaMult(kSkill) == doctest::Approx(1.20f * 1.20f));
  CHECK(delta.GetManaCostMultiplier(kSkill) == doctest::Approx(0.85f * 0.85f));
  CHECK(delta.GetSkillCooldownFlat(kSkill) == doctest::Approx(-1.5f));
  CHECK(delta.GetSkillBonusCrit(kSkill) == doctest::Approx(0.05f));
  CHECK(delta.GetSkillCharges(kSkill) == 3);
}

TEST_CASE("[Unit] SkillSpecDelivery - ChargesIntegerTruncation") {
  constexpr uint32_t kSkill = 33u;

  const auto evalCharges = [](const float perPoint, const uint16_t points) {
    ModifierRecord record =
        MakeDeliveryRecord(ModifierOpCode::SKILL_CHARGES_ADD, kSkill, perPoint);
    record.filter.node_id_whitelist = {100u};

    ModifierEvalContext ctx;
    ctx.node_points = {{100u, points}};
    ctx.active_node_ids = {100u};

    const ModifierDelta delta = ModifierEvaluator::Evaluate(
        std::span<const ModifierRecord>(&record, 1), ctx);
    return delta.GetSkillCharges(kSkill);
  };

  CHECK(evalCharges(1.9f, 1u) == 1);   // 1.9 -> 1
  CHECK(evalCharges(1.9f, 2u) == 3);   // 3.8 -> 3
  CHECK(evalCharges(-1.5f, 1u) == -1); // 向零截断
}

TEST_CASE("[Unit] SkillSpecDelivery - ManaCostFloorAtZero") {
  constexpr uint32_t kSkill = 55u;

  // 1 - 2.0 为负：下限钳制到 0，不产生负魔耗系数。
  CHECK(EvalSingle(ModifierOpCode::SKILL_MANA_COST_MULT, kSkill, 2.0f)
            .GetManaCostMultiplier(kSkill) == doctest::Approx(0.0f));
  CHECK(EvalSingle(ModifierOpCode::SKILL_MANA_COST_MULT, kSkill, 0.005f)
            .GetManaCostMultiplier(kSkill) == doctest::Approx(0.995f));
}

TEST_CASE("[Unit] SkillSpecDelivery - CategoryMasking") {
  CHECK(NoMoreDay::HasOpCategory(ModifierOpCategory::All,
                                 ModifierOpCategory::SkillDelivery));
  CHECK(static_cast<uint32_t>(ModifierOpCategory::SkillDelivery) == (1u << 3));
  CHECK(static_cast<uint32_t>(ModifierOpCategory::All) == 0x0Fu);

  // 局部实例：避免污染全局单例，其它依赖真实生成数据的用例不受影响。
  NoMoreDay::ModifierRuntimeRegistry registry;
  REQUIRE(registry.LoadFromBytes(BuildDeliveryCategoryRuntimeBlob()));

  NoMoreDay::ModifierRecordRequest request;
  request.record_id = 6001u;
  ModifierEvalContext ctx;

  const ModifierDelta skillOnly = ModifierEvaluator::Evaluate(
      registry, std::span<const NoMoreDay::ModifierRecordRequest>(&request, 1),
      ctx, ModifierOpCategory::SkillDelivery);
  CHECK(skillOnly.GetSkillMoreDamageMult(3u) == doctest::Approx(1.25f));
  CHECK(skillOnly.flat.empty());

  const ModifierDelta statsOnly = ModifierEvaluator::Evaluate(
      registry, std::span<const NoMoreDay::ModifierRecordRequest>(&request, 1),
      ctx, ModifierOpCategory::Stats);
  CHECK(statsOnly.skill_more_damage_mult.empty());
  CHECK(statsOnly.flat.at(4u) == doctest::Approx(50.0f));

  const ModifierDelta all = ModifierEvaluator::Evaluate(
      registry, std::span<const NoMoreDay::ModifierRecordRequest>(&request, 1),
      ctx, ModifierOpCategory::All);
  CHECK(all.GetSkillMoreDamageMult(3u) == doctest::Approx(1.25f));
  CHECK(all.flat.at(4u) == doctest::Approx(50.0f));
}

TEST_CASE("[Unit] SkillSpecDelivery - MergeFromContainerSemantics") {
  constexpr uint32_t kSkill = 4u;

  ModifierDelta a;
  a.AddFlat(1u, 10.0f);
  a.AddPercentAdd(1u, 0.1f);
  a.AddPercentMult(1u, 0.5f); // 存 1.5
  a.AddSkillLevel(2u, 1.0f);
  a.AddManaCostMultiplier(3u, 0.5f); // 存 0.5
  a.AddSkillMoreDamageMult(kSkill, 1.2f);
  a.AddSkillCooldownFlat(kSkill, -1.0f);
  a.AddSkillCooldownMult(kSkill, 1.1f);
  a.AddSkillCharges(kSkill, 2);
  a.AddSkillBonusCrit(kSkill, 0.02f);
  a.AddSkillAreaMult(kSkill, 1.5f);
  a.AddMonsterEventOnUpdate(100u);
  a.AddMonsterEventOnHit(101u);
  a.AddMonsterEventOnDeath(102u);
  a.AddMonsterBehaviorOnUpdate(8u);
  a.AddMonsterBehaviorOnHit(9u);
  a.AddMonsterBehaviorOnDeath(18u);

  ModifierDelta b;
  b.AddFlat(1u, 5.0f);
  b.AddFlat(7u, 2.0f); // 目标侧新键：走插入路径
  b.AddPercentAdd(1u, 0.2f);
  b.AddPercentMult(1u, 1.0f); // 存 2.0
  b.AddSkillLevel(2u, 3.0f);
  b.AddManaCostMultiplier(3u, 0.5f); // 存 0.5
  b.AddSkillMoreDamageMult(kSkill, 2.0f);
  b.AddSkillCooldownFlat(kSkill, -0.5f);
  b.AddSkillCooldownMult(kSkill, 2.0f);
  b.AddSkillCharges(kSkill, 3);
  b.AddSkillBonusCrit(kSkill, 0.03f);
  b.AddSkillAreaMult(kSkill, 0.5f);
  b.AddMonsterEventOnUpdate(100u); // 重复：并集去重
  b.AddMonsterEventOnUpdate(200u);
  b.AddMonsterEventOnHit(101u);
  b.AddMonsterEventOnDeath(300u);
  b.AddMonsterBehaviorOnUpdate(8u);
  b.AddMonsterBehaviorOnHit(19u);
  b.AddMonsterBehaviorOnDeath(18u);

  a.MergeFrom(b);

  // 加性容器累加。
  CHECK(a.flat.at(1u) == doctest::Approx(15.0f));
  CHECK(a.flat.at(7u) == doctest::Approx(2.0f));
  CHECK(a.percent_add.at(1u) == doctest::Approx(0.3f));
  CHECK(a.skill_levels.at(2u) == doctest::Approx(4.0f));
  CHECK(a.skill_cooldown_flat.at(kSkill) == doctest::Approx(-1.5f));
  CHECK(a.skill_charges_add.at(kSkill) == 5);
  CHECK(a.skill_bonus_crit.at(kSkill) == doctest::Approx(0.05f));

  // 乘性容器累乘。
  CHECK(a.percent_mult.at(1u) == doctest::Approx(3.0f));
  CHECK(a.mana_cost_mult.at(3u) == doctest::Approx(0.25f));
  CHECK(a.GetSkillMoreDamageMult(kSkill) == doctest::Approx(2.4f));
  CHECK(a.skill_cooldown_mult.at(kSkill) == doctest::Approx(2.2f));
  CHECK(a.skill_area_mult.at(kSkill) == doctest::Approx(0.75f));

  // 六个怪物集合取并集。
  CHECK(a.monster_event_on_update_affix_ids.size() == 2u);
  CHECK(a.monster_event_on_update_affix_ids.count(100u) == 1u);
  CHECK(a.monster_event_on_update_affix_ids.count(200u) == 1u);
  CHECK(a.monster_event_on_hit_affix_ids.count(101u) == 1u);
  CHECK(a.monster_event_on_death_affix_ids.count(102u) == 1u);
  CHECK(a.monster_event_on_death_affix_ids.count(300u) == 1u);
  CHECK(a.monster_behavior_on_update_opcodes.count(8u) == 1u);
  CHECK(a.monster_behavior_on_hit_opcodes.count(9u) == 1u);
  CHECK(a.monster_behavior_on_hit_opcodes.count(19u) == 1u);
  CHECK(a.monster_behavior_on_death_opcodes.count(18u) == 1u);
}

TEST_CASE("[Unit] SkillSpecDelivery - MergeFromSelfIsNoOp") {
  ModifierDelta self;
  self.AddPercentMult(1u, 0.5f);   // 存 1.5
  self.AddSkillAreaMult(2u, 2.0f); // 存 2.0

  self.MergeFrom(self);

  // 自合并必须早退，否则乘性容器会被自乘。
  CHECK(self.percent_mult.at(1u) == doctest::Approx(1.5f));
  CHECK(self.skill_area_mult.at(2u) == doctest::Approx(2.0f));
}
