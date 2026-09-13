#pragma once
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/contracts/DamagePipelineTypes.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include <array>
#include <optional>

namespace NoMoreDay {

// Projectile Component
// Represents a flying object that carries damage stats (Snapshot)
struct Projectile {
  // Snapshot of the attacker's stats at the moment of firing.
  // This ensures that if the attacker changes stats (e.g. equipment change,
  // buff expiry) while the projectile is in flight, the projectile's damage
  // remains consistent.
  CombatStats snapshot;

  // Lightweight payload snapshot (Zero heap allocation, 64-byte aligned)
  std::optional<DamagePayloadContext> payload_context;

  // Who fired this? (Entity ID) - Useful for kill credit, friendly fire checks
  entt::entity owner = entt::null;
  uint64_t cast_id =
      0; // NEW: Unique ID for the cast that spawned this projectile

  // Mechanics
  float lifeTime = 5.0f; // Max flight time
  float speed = 500.0f;
  float radius = 5.0f;   // Hitbox size
  float arcWidth = 0.0f; // Visual arc width in degrees (0 = use system default)
  int visualType = 0;    // 0 = Fan, 1 = Circle, 2 = Beam
  bool pierce = false;   // Does it pass through enemies?
  bool visual_only = false; // 仅表现：命中不产生伤害结算
  int pierceCount = 0;   // How many enemies can it hit?
  bool hasRendered =
      false; // Flag to prevent first-frame destruction before visibility
  bool hitLimitReached =
      false; // Persistent flag to ensure destruction after rendering

  // Pull mechanics
  bool hasPull = false;
  float pullStrength = 0.0f;

  // Tracking hits to prevent multi-hit on the same target (True SBO: zero heap allocation for <=32 hits)
  static constexpr uint8_t kMaxInlineHits = 32;
  static constexpr uint8_t kUnlimitedPiercing = 255; // 无限穿透/AoE Hitbox 哨兵值
  std::array<entt::entity, kMaxInlineHits> hit_cache{};
  std::vector<entt::entity> overflow_hits;
  uint16_t hit_count = 0;
  uint8_t max_pierce = 8; // 规则级穿透上限 (255 表示无限穿透/AoE Hitbox)

  bool IsUnlimitedPiercing() const {
    return max_pierce == kUnlimitedPiercing;
  }

  bool HasHit(entt::entity target) const {
    const uint16_t check_count = (std::min<uint16_t>)(hit_count, kMaxInlineHits);
    for (uint16_t i = 0; i < check_count; ++i) {
      if (hit_cache[i] == target) return true;
    }
    for (entt::entity e : overflow_hits) {
      if (e == target) return true;
    }
    return false;
  }

  /**
   * @brief 尝试记录命中目标并返回是否成功，同时给出是否已达穿透上限
   * @param target 命中的目标实体
   * @param outLimitReached 输出参数：若已达到穿透/命中上限则为 true
   * @return bool 若成功记录新命中则返回 true；若先前已命中该目标则返回 false
   */
  bool TryRecordHit(entt::entity target, bool &outLimitReached) {
    if (HasHit(target)) {
      outLimitReached = !IsUnlimitedPiercing() && (hit_count >= max_pierce);
      return false;
    }
    if (hit_count < kMaxInlineHits) {
      hit_cache[hit_count] = target;
    } else {
      // 达到内联容量后采用堆溢出存储，保证大怪群及折返飞行物全量去重，绝不逐出已命中目标
      overflow_hits.push_back(target);
    }
    hit_count++;
    if (IsUnlimitedPiercing()) {
      outLimitReached = false;
    } else {
      outLimitReached = (hit_count >= max_pierce);
    }
    return true;
  }

  /**
   * @brief 兼容接口：返回是否达到穿透上限
   */
  bool RecordHit(entt::entity target) {
    bool limitReached = false;
    TryRecordHit(target, limitReached);
    return limitReached;
  }

  void ClearHits() {
    hit_count = 0;
    overflow_hits.clear();
  }

  // --- NEW: Lifecycle Callbacks (Phase 2) ---
  enum class OnDeathBehavior : uint8_t {
    None = 0,
    Split,   // Split into multiple projectiles in a cone
    Explode, // Explode radially
    Hover    // Stop and deal Area Damage
  };
  OnDeathBehavior on_death = OnDeathBehavior::None;

  // Split Config
  uint8_t split_count = 3;
  float split_damage_mult = 0.5f;
  float split_speed_mult = 0.8f;
  float split_radius_mult = 0.6f;
  float split_spread = 0.6f; // Radians

  // Explode Config
  uint8_t explode_count = 8;
  float explode_damage_mult = 0.4f;

  // Hover Config
  float hover_duration = 1.0f;
  float hover_tick_rate = 0.2f;
  float hover_damage_mult = 0.3f;

  // 技能2 湮灭波 (Node 253) 无视物理护甲/抗性标志：生成方 (RendingWave) 在满层
  // 巨波上设置，消费方 (DamageMitigationService) 读取后将有效护甲减半。
  // 显式布尔取代旧 "snapshot.armor_pen += 500" 哨兵值——穿透数值不得承载布尔语义
  bool ignore_resist = false;
};

struct HomingTag {};

} // namespace NoMoreDay
