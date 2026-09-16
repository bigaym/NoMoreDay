#include "doctest.h"

#include "game/systems/modifier/ModifierContext.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"
#include "game/systems/modifier/ModifierRuntimeTypes.hpp"

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
using NoMoreDay::NodePointEntry;

// 构造单算子记录：param_u32 为技能 ID，param_f32 为每点幅度，
// nodeWhitelist 非空时记录仅在对应专精节点上生效。
ModifierRecord MakeSingleOpRecord(const ModifierOpCode opcode,
                                  const uint32_t skillId, const float perPoint,
                                  std::initializer_list<uint32_t> nodeWhitelist) {
  ModifierRecord record;
  record.filter.node_id_whitelist.assign(nodeWhitelist.begin(),
                                         nodeWhitelist.end());

  ModifierOp op;
  op.opcode = opcode;
  op.param_u32 = skillId;
  op.param_f32 = perPoint;
  record.ops.push_back(op);
  return record;
}

// active_node_ids 由 node_points 派生，保证 filter 的节点白名单能命中。
ModifierEvalContext MakePointsCtx(std::vector<NodePointEntry> points) {
  ModifierEvalContext ctx;
  ctx.node_points = std::move(points);
  for (const NodePointEntry &entry : ctx.node_points) {
    ctx.active_node_ids.push_back(entry.node_id);
  }
  return ctx;
}

ModifierDelta EvalRecord(const ModifierRecord &record,
                         const ModifierEvalContext &ctx) {
  return ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(&record, 1), ctx);
}

template <typename T>
void AppendStructScaling(std::vector<uint8_t> &out, const T &value) {
  const auto *bytes = reinterpret_cast<const uint8_t *>(&value);
  out.insert(out.end(), bytes, bytes + sizeof(T));
}

constexpr uint32_t kScalingSkillId = 42u;

// 单记录单算子 + 节点白名单，走 registry 运行时路径。
std::vector<uint8_t> BuildNodeScalingRuntimeBlob() {
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
  record.id = 4242u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 1;

  NoMoreDay::ModifierRuntimeFilter filter;
  // index 布局: [0] = 节点白名单, [1] = 技能白名单。
  filter.node_whitelist_offset = 0;
  filter.node_whitelist_count = 1;
  filter.skill_whitelist_offset = 1;
  filter.skill_whitelist_count = 1;

  NoMoreDay::ModifierRuntimeOp op;
  op.opcode = static_cast<uint16_t>(ModifierOpCode::SKILL_AREA_MULT);
  op.param_u32 = 11u;
  op.param_f32 = 0.2f;

  const uint32_t nodeId = 500u;
  const uint32_t skillId = 11u;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) + sizeof(op) +
               2u * sizeof(uint32_t));
  AppendStructScaling(blob, header);
  AppendStructScaling(blob, record);
  AppendStructScaling(blob, filter);
  AppendStructScaling(blob, op);
  AppendStructScaling(blob, nodeId);
  AppendStructScaling(blob, skillId);
  return blob;
}

} // namespace

TEST_CASE("[Unit] ModifierPointsScaling - ZeroPointsSkipsRecord") {
  const ModifierRecord record = MakeSingleOpRecord(
      ModifierOpCode::SKILL_MORE_DAMAGE_MULT, 7u, 0.1f, {100u});

  // 白名单命中的节点已分配 0 点 -> 记录整体跳过，不产生任何 delta 条目。
  const ModifierDelta delta = EvalRecord(record, MakePointsCtx({{100u, 0u}}));
  CHECK(delta.skill_more_damage_mult.empty());
  CHECK(delta.GetSkillMoreDamageMult(7u) == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] ModifierPointsScaling - OnePointBaseline") {
  const ModifierRecord record = MakeSingleOpRecord(
      ModifierOpCode::SKILL_MORE_DAMAGE_MULT, 7u, 0.1f, {100u});

  const ModifierDelta delta = EvalRecord(record, MakePointsCtx({{100u, 1u}}));
  CHECK(delta.GetSkillMoreDamageMult(7u) == doctest::Approx(1.1f));
}

TEST_CASE("[Unit] ModifierPointsScaling - LinearScalingAtThreeAndFivePoints") {
  const auto evalAt = [](const ModifierOpCode opcode, const float perPoint,
                         const uint16_t points) {
    const ModifierRecord record =
        MakeSingleOpRecord(opcode, kScalingSkillId, perPoint, {100u});
    return EvalRecord(record, MakePointsCtx({{100u, points}}));
  };

  // 乘性: 1.0 + perPoint * points
  CHECK(evalAt(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, 0.1f, 3u)
            .GetSkillMoreDamageMult(kScalingSkillId) == doctest::Approx(1.3f));
  CHECK(evalAt(ModifierOpCode::SKILL_MORE_DAMAGE_MULT, 0.1f, 5u)
            .GetSkillMoreDamageMult(kScalingSkillId) == doctest::Approx(1.5f));
  CHECK(evalAt(ModifierOpCode::SKILL_AREA_MULT, 0.2f, 3u)
            .GetSkillAreaMult(kScalingSkillId) == doctest::Approx(1.6f));
  CHECK(evalAt(ModifierOpCode::SKILL_AREA_MULT, 0.2f, 5u)
            .GetSkillAreaMult(kScalingSkillId) == doctest::Approx(2.0f));
  CHECK(evalAt(ModifierOpCode::SKILL_COOLDOWN_MULT, 0.15f, 3u)
            .GetSkillCooldownMult(kScalingSkillId) == doctest::Approx(1.45f));
  CHECK(evalAt(ModifierOpCode::SKILL_COOLDOWN_MULT, 0.15f, 5u)
            .GetSkillCooldownMult(kScalingSkillId) == doctest::Approx(1.75f));

  // 加性: perPoint * points
  CHECK(evalAt(ModifierOpCode::SKILL_COOLDOWN_FLAT, -1.0f, 3u)
            .GetSkillCooldownFlat(kScalingSkillId) == doctest::Approx(-3.0f));
  CHECK(evalAt(ModifierOpCode::SKILL_COOLDOWN_FLAT, -1.0f, 5u)
            .GetSkillCooldownFlat(kScalingSkillId) == doctest::Approx(-5.0f));
  CHECK(evalAt(ModifierOpCode::SKILL_BONUS_CRIT, 0.02f, 3u)
            .GetSkillBonusCrit(kScalingSkillId) == doctest::Approx(0.06f));
  CHECK(evalAt(ModifierOpCode::SKILL_BONUS_CRIT, 0.02f, 5u)
            .GetSkillBonusCrit(kScalingSkillId) == doctest::Approx(0.10f));
  CHECK(evalAt(ModifierOpCode::SKILL_CHARGES_ADD, 1.0f, 3u)
            .GetSkillCharges(kScalingSkillId) == 3);
  CHECK(evalAt(ModifierOpCode::SKILL_CHARGES_ADD, 1.0f, 5u)
            .GetSkillCharges(kScalingSkillId) == 5);

  // 法耗折扣: max(0, 1 - perPoint * points)
  CHECK(evalAt(ModifierOpCode::SKILL_MANA_COST_MULT, 0.15f, 3u)
            .GetManaCostMultiplier(kScalingSkillId) == doctest::Approx(0.55f));
  CHECK(evalAt(ModifierOpCode::SKILL_MANA_COST_MULT, 0.15f, 5u)
            .GetManaCostMultiplier(kScalingSkillId) == doctest::Approx(0.25f));
}

TEST_CASE("[Unit] ModifierPointsScaling - LegacyFallbackWithoutNodePoints") {
  // 仅填充 active_node_ids（node_points 留空）：兼容回退为单点生效。
  const ModifierRecord record = MakeSingleOpRecord(
      ModifierOpCode::SKILL_COOLDOWN_FLAT, 9u, -1.0f, {100u});

  ModifierEvalContext ctx;
  ctx.active_node_ids = {100u};
  const ModifierDelta delta = EvalRecord(record, ctx);

  CHECK(delta.skill_cooldown_flat.size() == 1u);
  CHECK(delta.GetSkillCooldownFlat(9u) == doctest::Approx(-1.0f));
}

TEST_CASE("[Unit] ModifierPointsScaling - MultiNodeWhitelistPicksFirstAllocated") {
  const ModifierRecord record = MakeSingleOpRecord(
      ModifierOpCode::SKILL_MORE_DAMAGE_MULT, 5u, 0.1f, {100u, 200u});

  // 100 为 0 点、200 为 4 点 -> 取白名单中第一个已加点 200。
  const ModifierDelta delta =
      EvalRecord(record, MakePointsCtx({{100u, 0u}, {200u, 4u}}));
  CHECK(delta.GetSkillMoreDamageMult(5u) == doctest::Approx(1.4f));
}

TEST_CASE("[Unit] ModifierPointsScaling - GetPointsForNodeLookup") {
  ModifierEvalContext ctx;
  ctx.node_points = {{100u, 5u}, {300u, 2u}};

  CHECK(ctx.GetPointsForNode(100u) == 5u);
  CHECK(ctx.GetPointsForNode(300u) == 2u);
  CHECK(ctx.GetPointsForNode(999u) == 0u);
  CHECK(ctx.GetPointsForNode(150u) == 0u);
}

TEST_CASE("[Unit] ModifierPointsScaling - GetPointsForNodeUnsortedFallback") {
  ModifierEvalContext ctx;
  // 故意乱序：二分不可用，线性扫描兜底仍需正确返回。
  ctx.node_points = {{300u, 2u}, {100u, 5u}};

  CHECK(ctx.GetPointsForNode(100u) == 5u);
  CHECK(ctx.GetPointsForNode(300u) == 2u);
  CHECK(ctx.GetPointsForNode(999u) == 0u);
}

TEST_CASE("[Unit] ModifierPointsScaling - RuntimeRegistryAppliesNodePoints") {
  // 局部实例：避免污染全局单例，其它依赖真实生成数据的用例不受影响。
  NoMoreDay::ModifierRuntimeRegistry registry;
  REQUIRE(registry.LoadFromBytes(BuildNodeScalingRuntimeBlob()));

  ModifierEvalContext ctx;
  ctx.skill_id = 11u;
  ctx.active_node_ids = {500u};
  ctx.node_points = {{500u, 2u}};

  const uint32_t recordIds[] = {4242u};
  const ModifierDelta delta = ModifierEvaluator::Evaluate(
      registry, std::span<const uint32_t>(recordIds, 1), ctx);
  CHECK(delta.GetSkillAreaMult(11u) == doctest::Approx(1.4f));

  // 同一记录在 0 点时被整体跳过。
  ModifierEvalContext zeroCtx = ctx;
  zeroCtx.node_points = {{500u, 0u}};
  const ModifierDelta zeroDelta = ModifierEvaluator::Evaluate(
      registry, std::span<const uint32_t>(recordIds, 1), zeroCtx);
  CHECK(zeroDelta.skill_area_mult.empty());
}

TEST_CASE("[Unit] ModifierPointsScaling - NoTruncationBeyondUsualNodeCap") {
  // 设计 §3.1：求值层刻意不做截断，超过常见节点上限的点数仍按线性外推；
  // 点数合法性由加点/存档校验层负责，故此处锁定"无隐藏截断"行为。
  const ModifierRecord record = MakeSingleOpRecord(
      ModifierOpCode::SKILL_MORE_DAMAGE_MULT, 8u, 0.1f, {100u});

  const ModifierDelta delta = EvalRecord(record, MakePointsCtx({{100u, 20u}}));
  CHECK(delta.GetSkillMoreDamageMult(8u) == doctest::Approx(3.0f));
}

TEST_CASE(
    "[Unit] ModifierPointsScaling - NonEmptyPointsTableMissingWhitelistedNodeSkips") {
  // node_points 非空但不含白名单节点：GetPointsForNode 返回 0，记录整体跳过。
  // 覆盖"部分填充点数表"的语义，避免 node_points.empty() 的 legacy 回退掩盖之。
  const ModifierRecord record = MakeSingleOpRecord(
      ModifierOpCode::SKILL_MORE_DAMAGE_MULT, 12u, 0.1f, {100u});

  ModifierEvalContext ctx;
  ctx.active_node_ids = {100u};   // 过滤层命中
  ctx.node_points = {{200u, 5u}}; // 点数表非空，但不含白名单节点 100
  const ModifierDelta delta = EvalRecord(record, ctx);
  CHECK(delta.skill_more_damage_mult.empty());
}
