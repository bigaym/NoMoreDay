#include "TestCommon.hpp"

#include "doctest.h"

#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/TagRegistry.hpp"
#include "game/systems/modifier/ModifierContext.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"
#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"
#include "game/systems/modifier/ModifierRuntimeTypes.hpp"
#include "game/systems/modifier/SkillSpecModifierAdapter.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace {

template <typename T>
void AppendStruct(std::vector<uint8_t> &out, const T &value) {
  const auto *bytes = reinterpret_cast<const uint8_t *>(&value);
  out.insert(out.end(), bytes, bytes + sizeof(T));
}

std::vector<uint8_t> BuildSkillSpecRuntimeBlob() {
  NoMoreDay::ModifierRuntimeHeader header;
  header.record_count = 1;
  header.filter_count = 1;
  header.op_count = 1;
  header.index_count = 2;
  header.records_offset = sizeof(NoMoreDay::ModifierRuntimeHeader);
  header.filters_offset =
      header.records_offset + sizeof(NoMoreDay::ModifierRuntimeRecord);
  header.ops_offset =
      header.filters_offset + sizeof(NoMoreDay::ModifierRuntimeFilter);
  header.index_offset = header.ops_offset + sizeof(NoMoreDay::ModifierRuntimeOp);
  header.crc32 = 0;

  NoMoreDay::ModifierRuntimeRecord record;
  record.id = 2099001u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 1;

  NoMoreDay::ModifierRuntimeFilter filter;
  filter.skill_whitelist_offset = 0;
  filter.skill_whitelist_count = 1;
  filter.node_whitelist_offset = 1;
  filter.node_whitelist_count = 1;

  NoMoreDay::ModifierRuntimeOp op;
  op.opcode = static_cast<uint16_t>(NoMoreDay::ModifierOpCode::ADD_STAT_PERCENT_MULT);
  op.param_u32 = static_cast<uint32_t>(NoMoreDay::StatType::PhysicalDamage);
  op.param_f32 = 0.35f;

  constexpr uint32_t kSkillWhitelist = 77u;
  constexpr uint32_t kNodeWhitelist = 999u;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) + sizeof(op) +
               sizeof(kSkillWhitelist) + sizeof(kNodeWhitelist));
  AppendStruct(blob, header);
  AppendStruct(blob, record);
  AppendStruct(blob, filter);
  AppendStruct(blob, op);
  AppendStruct(blob, kSkillWhitelist);
  AppendStruct(blob, kNodeWhitelist);
  return blob;
}

// 单记录双算子 blob：交付算子 36（SKILL_MANA_COST_MULT）与非交付算子 4
// （MANA_COST_MULT，Stats 类别）挂在同一记录上，用于验证交付求值只应用
// SkillDelivery 类别算子。
std::vector<uint8_t> BuildMixedOpSkillSpecRuntimeBlob() {
  NoMoreDay::ModifierRuntimeHeader header;
  header.record_count = 1;
  header.filter_count = 1;
  header.op_count = 2;
  header.index_count = 2;
  header.records_offset = sizeof(NoMoreDay::ModifierRuntimeHeader);
  header.filters_offset =
      header.records_offset + sizeof(NoMoreDay::ModifierRuntimeRecord);
  header.ops_offset =
      header.filters_offset + sizeof(NoMoreDay::ModifierRuntimeFilter);
  header.index_offset =
      header.ops_offset + 2u * sizeof(NoMoreDay::ModifierRuntimeOp);
  header.crc32 = 0;

  NoMoreDay::ModifierRuntimeRecord record;
  record.id = 2099002u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 2;

  NoMoreDay::ModifierRuntimeFilter filter;
  // index 布局: [0] = 技能白名单, [1] = 节点白名单。
  filter.skill_whitelist_offset = 0;
  filter.skill_whitelist_count = 1;
  filter.node_whitelist_offset = 1;
  filter.node_whitelist_count = 1;

  NoMoreDay::ModifierRuntimeOp deliveryOp;
  deliveryOp.opcode =
      static_cast<uint16_t>(NoMoreDay::ModifierOpCode::SKILL_MANA_COST_MULT);
  deliveryOp.param_u32 = 1u;
  deliveryOp.param_f32 = 0.15f;

  NoMoreDay::ModifierRuntimeOp statOp;
  statOp.opcode =
      static_cast<uint16_t>(NoMoreDay::ModifierOpCode::MANA_COST_MULT);
  statOp.param_u32 = 1u;
  statOp.param_f32 = 0.5f;

  constexpr uint32_t kSkillWhitelist = 1u;
  constexpr uint32_t kNodeWhitelist = 777u;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) +
               2u * sizeof(deliveryOp) + 2u * sizeof(uint32_t));
  AppendStruct(blob, header);
  AppendStruct(blob, record);
  AppendStruct(blob, filter);
  AppendStruct(blob, deliveryOp);
  AppendStruct(blob, statOp);
  AppendStruct(blob, kSkillWhitelist);
  AppendStruct(blob, kNodeWhitelist);
  return blob;
}

float RunSpecFixture(const uint32_t skillId, const std::vector<uint32_t> &nodes) {
  constexpr float kBaseline = 100.0f;
  return NoMoreDay::SkillSpecModifierAdapter::ApplyHeavyMomentum(
      kBaseline, skillId, NoMoreDay::Tag::None, nodes);
}

} // namespace

TEST_CASE("[Unit] SkillSpecModifierAdapter - evaluates runtime-record-driven scalar") {
  auto &runtimeRegistry = NoMoreDay::ModifierRuntimeRegistry::Get();
  const auto blob = BuildSkillSpecRuntimeBlob();
  REQUIRE(runtimeRegistry.LoadFromBytes(blob));

  const float baseline = RunSpecFixture(77u, {});
  const float boosted = RunSpecFixture(77u, {999u});
  CHECK(baseline == doctest::Approx(100.0f));
  CHECK(boosted == doctest::Approx(135.0f));
  CHECK(RunSpecFixture(77u, {213u}) == doctest::Approx(100.0f));
  CHECK(RunSpecFixture(99u, {999u}) == doctest::Approx(100.0f));
}

TEST_CASE("[Unit] SkillSpecModifierAdapter - runtime multiplier only affects physical damage") {
  auto &runtimeRegistry = NoMoreDay::ModifierRuntimeRegistry::Get();
  const auto blob = BuildSkillSpecRuntimeBlob();
  REQUIRE(runtimeRegistry.LoadFromBytes(blob));

  std::array<float, 6> multipliers = {1.0f, 1.5f, 0.8f, 1.2f, 0.9f, 1.1f};
  const std::vector<uint32_t> heavyMomentumNodes = {999u};

  NoMoreDay::SkillSpecModifierAdapter::ApplyHeavyMomentumToDamageMultipliers(
      multipliers, 77u, NoMoreDay::Tag::None, heavyMomentumNodes);

  CHECK(multipliers[static_cast<uint8_t>(NoMoreDay::DamageType::Physical)] ==
        doctest::Approx(1.35f));
  CHECK(multipliers[static_cast<uint8_t>(NoMoreDay::DamageType::Fire)] ==
        doctest::Approx(1.5f));
  CHECK(multipliers[static_cast<uint8_t>(NoMoreDay::DamageType::Cold)] ==
        doctest::Approx(0.8f));
  CHECK(multipliers[static_cast<uint8_t>(NoMoreDay::DamageType::Lightning)] ==
        doctest::Approx(1.2f));
  CHECK(multipliers[static_cast<uint8_t>(NoMoreDay::DamageType::Poison)] ==
        doctest::Approx(0.9f));
  CHECK(multipliers[static_cast<uint8_t>(NoMoreDay::DamageType::Shadow)] ==
        doctest::Approx(1.1f));
}

TEST_CASE("[Unit] SkillSpecModifierAdapter - collects skill delivery records for skill 1") {
  // 显式重载生成产物：同进程内其他用例的 LoadFromBytes 合成数据会残留在单例中，
  // EnsureLoaded 视为通配来源不再重载。复用 TestCommon 的单一资产路径来源。
  REQUIRE(ReloadModifierRuntimeFromAsset());

  // 未加点：不采集任何交付记录，返回默认 delta
  {
    NoMoreDay::SpecializedSkill spec;
    spec.skill_id = 1u;
    const auto delta =
        NoMoreDay::SkillSpecModifierAdapter::EvaluateSkillDeliveryDeltas(
            1u, NoMoreDay::Tag::None, spec);
    CHECK(delta.GetManaCostMultiplier(1u) == doctest::Approx(1.0f));
    CHECK(delta.GetSkillMoreDamageMult(1u) == doctest::Approx(1.0f));
    CHECK(delta.GetSkillCooldownFlat(1u) == doctest::Approx(0.0f));
    CHECK(delta.GetSkillCooldownMult(1u) == doctest::Approx(1.0f));
    CHECK(delta.GetSkillCharges(1u) == 0);
    CHECK(delta.GetSkillBonusCrit(1u) == doctest::Approx(0.0f));
    CHECK(delta.GetSkillAreaMult(1u) == doctest::Approx(1.0f));
  }

  // 多节点多点数：9 条记录按分配点数线性缩放
  NoMoreDay::SpecializedSkill spec;
  spec.skill_id = 1u;
  spec.allocated_points[101] = 3; // 法耗 ×(1 - 0.15×3) = ×0.55
  spec.allocated_points[102] = 3; // 暴击 += 0.02×3 = +0.06
  spec.allocated_points[103] = 2; // More ×(1 + 0.10×2) = ×1.20
  spec.allocated_points[110] = 1; // CD 平 -1.0，More ×(1 - 0.15) = ×0.85
  spec.allocated_points[111] = 2; // 充能 +2，CD 乘 ×(1 + 0.15×2) = ×1.30
  spec.allocated_points[134] = 2; // More ×1.50，范围 ×1.40

  const auto delta =
      NoMoreDay::SkillSpecModifierAdapter::EvaluateSkillDeliveryDeltas(
          1u, NoMoreDay::Tag::None, spec);
  CHECK(delta.GetManaCostMultiplier(1u) == doctest::Approx(0.55f));
  CHECK(delta.GetSkillBonusCrit(1u) == doctest::Approx(0.06f));
  CHECK(delta.GetSkillCharges(1u) == 2);
  CHECK(delta.GetSkillCooldownFlat(1u) == doctest::Approx(-1.0f));
  CHECK(delta.GetSkillCooldownMult(1u) == doctest::Approx(1.30f));
  // 103 ×1.20、110 ×0.85、134 ×1.50 连乘（乘法可交换，与记录遍历顺序无关）
  CHECK(delta.GetSkillMoreDamageMult(1u) ==
        doctest::Approx(1.20f * 0.85f * 1.50f));
  CHECK(delta.GetSkillAreaMult(1u) == doctest::Approx(1.40f));

  // 交付采集与 legacy 伤害乘算路径相互独立：213/技能2 记录不被采集
  NoMoreDay::SpecializedSkill other;
  other.skill_id = 2u;
  other.allocated_points[213] = 9;
  const auto otherDelta =
      NoMoreDay::SkillSpecModifierAdapter::EvaluateSkillDeliveryDeltas(
          2u, NoMoreDay::Tag::None, other);
  CHECK(otherDelta.GetSkillMoreDamageMult(2u) == doctest::Approx(1.0f));
  CHECK(otherDelta.GetSkillAreaMult(2u) == doctest::Approx(1.0f));
}

TEST_CASE(
    "[Unit] SkillSpecModifierAdapter - delivery delta excludes non-delivery ops") {
  auto &runtimeRegistry = NoMoreDay::ModifierRuntimeRegistry::Get();
  REQUIRE(runtimeRegistry.LoadFromBytes(BuildMixedOpSkillSpecRuntimeBlob()));

  NoMoreDay::SpecializedSkill spec;
  spec.skill_id = 1u;
  spec.allocated_points[777] = 1;

  const auto delta =
      NoMoreDay::SkillSpecModifierAdapter::EvaluateSkillDeliveryDeltas(
          1u, NoMoreDay::Tag::None, spec);

  // 同记录内 op36 交付算子生效（1 - 0.15 = 0.85）；op4（Stats 类别，每点 -50% 法耗）
  // 必须被排除。若被错误折叠，结果会是 0.85 * 0.5 = 0.425。
  CHECK(delta.GetManaCostMultiplier(1u) == doctest::Approx(0.85f));
}
