#pragma once

// 御剑·回旋（技能8）元素路径系统。
//
// 870 劫灰路径 / 872 电磁回旋让飞剑飞行轨迹在地面留下元素残留：
//   - 火：燃烧路径，为 871 燎原之势提供路径内火伤增幅；
//   - 雷：电弧路径，按 873 高压电弧的触发频率沿路径索敌放电。
// 875 灵根破壁依赖路径判定实现「本次攻击的元素穿透」，876 元素护体依赖
// 「施法者站在自身路径上」判定提供对应元素绝对减伤。三者的抗性修正挂点
// 位于 DamageMitigationService（combat 域），本系统只负责路径事实与电弧伤害。
//
// 存储策略：路径段以定长数组挂在 owner 实体的 ElementPathComponent 上，
// 零堆分配；同 owner 同类元素路径取并集。系统不持有跨 registry 的静态状态。

#include "game/foundation/data/TagRegistry.hpp"

#include <array>
#include <cstdint>

#include <entt/entt.hpp>
#include <raylib.h>

namespace NoMoreDay::element_path {

// 单个路径段（定长 POD，避免运行时分配）。
struct PathSegment {
  Vector2 start{0.0f, 0.0f};
  Vector2 end{0.0f, 0.0f};
  float half_width = 30.0f;   // 路径半宽（874 元素尾迹由调用方放大）
  float duration = 0.0f;      // 初始存续时长（秒）
  float remaining = 0.0f;     // 剩余存续时长（秒）
  float amp = 0.0f;           // 871 路径内对应元素增伤 [0,1]
  float penetration = 0.0f;   // 875 攻击时的对应元素穿透 [0,1]
  float arc_freq_mult = 1.0f; // 873 电弧触发频率倍率
  float arc_timer = 0.0f;     // 电弧累计计时
  float vfx_timer = 0.0f;     // 路径拖尾 VFX 节流计时
  uint64_t cast_id = 0;       // 本次施法标识，用于区分同一次飞剑轨迹
  uint32_t skill_id = 0;      // 来源技能（本系统当前仅服务技能8）
  bool active = false;
};

// 每个 owner 最多保留的路径段数，超出后淘汰最旧段。
inline constexpr int kMaxSegmentsPerOwner = 32;

// 挂载在路径归属实体（施法者）上的路径组件。
struct ElementPathComponent {
  Tag element_tag = Tag::None;         // 同类多段取并集；None 表示无有效路径
  std::array<PathSegment, kMaxSegmentsPerOwner> segments{};
  int count = 0; // 已占用段数（前 count 项有效，可能含 inactive）
};

// 生成一段元素路径。WS-B 在飞剑飞行途中按帧/按距离调用。
struct SpawnParams {
  entt::entity owner = entt::null;
  uint64_t cast_id = 0;
  uint32_t skill_id = 0;
  Tag element_tag = Tag::None; // Tag::Fire / Tag::Lightning
  Vector2 start{0.0f, 0.0f};
  Vector2 end{0.0f, 0.0f};
  float half_width = 30.0f;
  float duration = 3.0f;
  float amp = 0.0f;           // 871 燎原之势: 路径内火伤加成 [0,1]
  float penetration = 0.0f;   // 875 灵根破壁: 元素穿透 [0,1]
  float arc_freq_mult = 1.0f; // 873 高压电弧
};

// 写入一段路径；相邻同元素同施法的共线段会合并，超出上限淘汰最旧。
void Spawn(entt::registry &registry, const SpawnParams &params);

// 推进路径衰减、拖尾 VFX 与雷路径电弧结算。
void Update(entt::registry &registry, float dt);

// 点是否落在 owner 指定元素的任一有效路径段内。
bool IsInside(entt::registry &registry, entt::entity owner, Tag element_tag,
              Vector2 position);

// owner 指定元素路径提供的最强路径内增伤（871），无则 0。
float AmpAgainst(entt::registry &registry, entt::entity owner, Tag element_tag);

// owner 指定元素路径提供的最强元素穿透（875），无则 0。
float PenetrationFor(entt::registry &registry, entt::entity owner, Tag element_tag);

// 876 元素护体：仅当 owner 站在自身指定元素路径上时返回对应元素绝对减伤比例。
float ShieldReductionFor(entt::registry &registry, entt::entity owner,
                         Tag element_tag);

// 873：接刃时以接刃点为中心的对应元素范围爆发（雷元素附带短眩晕）。
void Detonate(entt::registry &registry, entt::entity owner, Tag element_tag,
              Vector2 position, float radius);

// 测试钩子：清除所有实体上的元素路径组件，避免用例间串扰。
void ClearForTests(entt::registry &registry);

} // namespace NoMoreDay::element_path
