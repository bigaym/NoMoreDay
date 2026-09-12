#pragma once

#include "core/logging/Logger.hpp"
#include "game/contracts/DamagePipelineTypes.hpp"
#include "game/systems/combat/damage/DamageConditions.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace NoMoreDay {

// 固定容量向量：热路径零堆分配，容量溢出时丢弃并告警。
// 从 DamagePipeline.cpp 提取到公共头，供攻守快照与服务共用同一实现。
template <typename T, size_t N> struct FixedVector {
  std::array<T, N> data;
  size_t size = 0;

  void push_back(const T &value) {
    if (size < N) {
      data[size++] = value;
    } else {
      LOG_WARN("FixedVector overflow! Capacity: {}", N);
    }
  }

  T &operator[](size_t index) { return data[index]; }
  const T &operator[](size_t index) const { return data[index]; }

  T *begin() { return data.data(); }
  T *end() { return data.data() + size; }
  const T *begin() const { return data.data(); }
  const T *end() const { return data.data() + size; }

  bool empty() const { return size == 0; }
  void clear() { size = 0; }

  [[nodiscard]] std::span<const T> span() const {
    return {data.data(), size};
  }
};

namespace damage {

// 攻方紧凑快照 (P3-1/P3-2)：静态乘区已烘焙，仅保留少量守方动态条件规则。
// 保持 32 字节对齐与可平凡复制，支持高速值拷贝到栈上与批量内核。
struct alignas(32) AttackerSnapshot {
  std::array<float, 6> base_damage = {0.0f}; // After Inc/More/Conversion
  float crit_chance = 0.0f;
  float crit_damage = 1.5f;
  float armor_pen = 0.0f;
  float accuracy = 1.0f;
  Tag hit_tags = Tag::None;
  // 快照演算时实际参与结算的元素标签（转换后），供批量事件按真实元素重映。
  Tag resolved_element_tags = Tag::None;
  FixedVector<ConditionalDamageOp, 4> conditional_ops;
  float _padding[4] = {0.0f};
};
static_assert(alignof(AttackerSnapshot) == 32,
              "AttackerSnapshot must be 32-byte aligned for SIMD");
static_assert(std::is_trivially_copyable_v<AttackerSnapshot>,
              "AttackerSnapshot must stay trivially copyable (POD value copy)");

// 守方紧凑快照：静态减伤模板 + 实时条件掩码。
struct alignas(32) DefenseSnapshot {
  std::array<float, 6> resistances = {0.0f};
  float armor = 0.0f;
  float dodge_chance = 0.0f;
  float block_chance = 0.0f;
  float block_multiplier = 1.0f;
  float global_dr = 0.0f;
  int cached_area_level = 1;
  uint32_t condition_mask = 0; // TargetCondition 位掩码
};

// 实体快照组件：绑定投射物/DoT 生命周期，命中时直接读取，替代全局 LRU 缓存。
// 纯 POD 值：caster 实体销毁后快照仍可独立结算。
struct DamageSnapshotComponent {
  AttackerSnapshot snapshot;
  DamageOrigin origin = DamageOrigin::DirectSkillCast;
  uint32_t skill_id = 0;
  entt::entity caster = entt::null;
};

static_assert(std::is_trivially_copyable_v<DamageSnapshotComponent>,
              "DamageSnapshotComponent must be POD for entity direct-store");

// DoT 快照载体标记：AilmentEngine 在施加异常时创建一个临时实体承载
// DamageSnapshotComponent，并把句柄写入 BuffEffect::snapshot_source。
// target 用于回收时确认对应异常是否仍然存在，避免实体句柄泄漏。
struct AilmentSnapshotHolderComponent {
  entt::entity target = entt::null;
};

} // namespace damage
} // namespace NoMoreDay
