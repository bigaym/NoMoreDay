#include "game/systems/skill/ElementPathSystem.hpp"

#include "engine/render/GPUData.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/contracts/DamagePipelineTypes.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include <raymath.h>

namespace NoMoreDay::element_path {

namespace {

// 御剑·回旋技能 ID
constexpr uint32_t kSkillId = 8u;

// 路径段合并阈值：与上一段端点相接、方向近似共线且未超过段长上限时延长该段，
// 既避免每帧 Spawn 造成段数爆量，又保留转弯处的几何拐点。
constexpr float kMergeGap = 12.0f;
constexpr float kMergeMinCos = 0.985f; // 约 10°
constexpr float kMaxSegmentLength = 150.0f;

// 电弧节流上限，防御异常参数导致单帧死循环。
constexpr int kMaxArcsPerFrame = 16;

float PointSegmentDistance(Vector2 p, Vector2 a, Vector2 b) {
  const float dx = b.x - a.x;
  const float dy = b.y - a.y;
  const float len_sq = dx * dx + dy * dy;
  if (len_sq <= 1e-6f) {
    return Vector2Distance(p, a);
  }
  float t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / len_sq;
  t = std::clamp(t, 0.0f, 1.0f);
  const Vector2 proj{a.x + t * dx, a.y + t * dy};
  return Vector2Distance(p, proj);
}

Vector2 SegmentMidpoint(const PathSegment &seg) {
  return {(seg.start.x + seg.end.x) * 0.5f, (seg.start.y + seg.end.y) * 0.5f};
}

bool TryGetEntityPosition(entt::registry &registry, entt::entity entity,
                          Vector2 &out) {
  if (!registry.valid(entity)) {
    return false;
  }
  const auto *pos = registry.try_get<Position>(entity);
  if (pos == nullptr) {
    return false;
  }
  out = {pos->x, pos->y};
  return true;
}

// 876 元素护体的减伤比例：以烘焙产物为唯一数值权威（未投入 876 时 Baker 写 0），
// 避免运行期重复读取点数与机制表造成双权威。
float ResolveElementShieldPct(entt::registry &registry, entt::entity owner) {
  const auto *profile = SkillSystem::GetBakedSkillProfile(registry, owner, kSkillId);
  if (profile == nullptr) {
    return 0.0f;
  }
  return std::clamp(profile->delivery.element_shield_pct, 0.0f, 1.0f);
}

// 统一的元素命中入口：走伤害结算钩子，标记 SecondaryHit 防止触发链二次放大。
void ApplyElementHit(entt::registry &registry, entt::entity owner,
                     entt::entity target, Tag element_tag, float base_damage) {
  if (!registry.valid(target) || base_damage <= 0.0f) {
    return;
  }
  if (registry.any_of<KilledTag>(target)) {
    return;
  }

  DamagePayloadContext ctx{};
  ctx.base_damage_min = base_damage;
  ctx.base_damage_max = base_damage;
  ctx.crit_chance = 0.0f;
  ctx.crit_multiplier = 1.0f;
  ctx.increased_damage = 0.0f;
  ctx.more_damage = 1.0f;
  ctx.effective_tags = element_tag;
  ctx.source_skill_id = kSkillId;

  DamageRequest req{};
  req.attacker = owner;
  req.defender = target;
  req.skill_id = kSkillId;
  req.source_entity = owner;
  req.additional_tags = element_tag | Tag::Hit | Tag::SecondaryHit;
  req.payload_context = ctx;

  (void)ResolveDamage(registry, req, target);
}

// 873 电磁爆发附带的短眩晕（仅雷元素）。
void ApplyBurstStun(entt::registry &registry, entt::entity owner,
                    entt::entity target, float duration) {
  if (duration <= 0.0f || !registry.valid(target)) {
    return;
  }
  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(target);
  BuffEffect stun{};
  stun.id = "ElementPathBurstStun";
  stun.name = "Stun";
  stun.type = BuffType::Stun;
  stun.duration = duration;
  stun.remaining = duration;
  stun.stacks = 1;
  stun.max_stacks = 1;
  stun.is_debuff = true;
  stun.source = owner;
  stun.source_skill_id = static_cast<int>(kSkillId);
  effects.AddOrRefresh(stun);
}

void EmitPathVfx(const SpawnParams &params) {
  const Vector2 mid{(params.start.x + params.end.x) * 0.5f,
                    (params.start.y + params.end.y) * 0.5f};
  const Color color = (params.element_tag == Tag::Lightning)
                          ? Color{150, 120, 255, 170}
                          : Color{255, 140, 60, 165};
  components::GPUParticle particle;
  particle.position = mid;
  particle.velocity = {0.0f, 0.0f};
  particle.color = color;
  particle.lifetime = 0.35f;
  particle.maxLifetime = 0.35f;
  particle.scale = params.half_width;
  particle.flags = 2; // Glow
  particle.growthRate = 8.0f;
  systems::GPUParticleSystem::Get().Emit(particle);
}

void EmitArcVfx(Vector2 from, Vector2 to) {
  components::GPUParticle particle;
  particle.position = from;
  particle.velocity = {to.x - from.x, to.y - from.y};
  particle.color = Color{190, 170, 255, 220};
  particle.lifetime = 0.12f;
  particle.maxLifetime = 0.12f;
  particle.scale = 8.0f;
  particle.flags = 2; // Glow
  systems::GPUParticleSystem::Get().Emit(particle);
}

// 沿路径段寻找半径内最近的敌人（用于 872 电弧索敌）。
entt::entity FindNearestEnemyOnSegment(entt::registry &registry,
                                       entt::entity owner,
                                       const PathSegment &seg, float radius) {
  entt::entity best = entt::null;
  float best_dist = radius;
  auto view = registry.view<EnemyTag, Position>();
  for (auto enemy : view) {
    if (enemy == owner || registry.any_of<KilledTag>(enemy)) {
      continue;
    }
    const auto &pos = view.get<Position>(enemy);
    const float dist =
        PointSegmentDistance({pos.x, pos.y}, seg.start, seg.end);
    if (dist <= radius && dist < best_dist) {
      best_dist = dist;
      best = enemy;
    }
  }
  return best;
}

void UpdateLightningArc(entt::registry &registry, entt::entity owner,
                        PathSegment &seg, float dt) {
  const float freq = (seg.arc_freq_mult > 0.01f) ? seg.arc_freq_mult : 1.0f;
  float interval = data::SkillMechanicsRegistry::Get().GetFloat(
                        kSkillId, 872, "arc_interval", 0.5f) /
                    freq;
  if (interval < 0.05f) {
    interval = 0.05f;
  }
  const float arc_radius = data::SkillMechanicsRegistry::Get().GetFloat(
      kSkillId, 872, "arc_radius", 180.0f);
  const float arc_damage = data::SkillMechanicsRegistry::Get().GetFloat(
      kSkillId, 872, "arc_damage", 30.0f);

  seg.arc_timer += dt;
  int guard = 0;
  while (seg.arc_timer >= interval && guard < kMaxArcsPerFrame) {
    seg.arc_timer -= interval;
    ++guard;
    const entt::entity target =
        FindNearestEnemyOnSegment(registry, owner, seg, arc_radius);
    if (target != entt::null) {
      ApplyElementHit(registry, owner, target, Tag::Lightning, arc_damage);
      const auto *tp = registry.try_get<Position>(target);
      if (tp != nullptr) {
        EmitArcVfx(SegmentMidpoint(seg), {tp->x, tp->y});
      }
    }
  }
}

// 以 position 为中心的元素范围爆发（873 接刃电磁爆发复用）。
void ApplyBurst(entt::registry &registry, entt::entity owner, Tag element_tag,
                Vector2 position, float radius) {
  if (radius <= 0.0f || element_tag == Tag::None) {
    return;
  }
  const float burst_damage = data::SkillMechanicsRegistry::Get().GetFloat(
      kSkillId, 873, "burst_damage", 60.0f);
  const float stun_duration =
      (element_tag == Tag::Lightning)
          ? data::SkillMechanicsRegistry::Get().GetFloat(
                kSkillId, 873, "burst_stun_duration", 0.5f)
          : 0.0f;

  auto view = registry.view<EnemyTag, Position>();
  for (auto enemy : view) {
    if (enemy == owner || registry.any_of<KilledTag>(enemy)) {
      continue;
    }
    const auto &pos = view.get<Position>(enemy);
    if (Vector2Distance({pos.x, pos.y}, position) > radius) {
      continue;
    }
    ApplyElementHit(registry, owner, enemy, element_tag, burst_damage);
    if (stun_duration > 0.0f && !registry.any_of<KilledTag>(enemy)) {
      ApplyBurstStun(registry, owner, enemy, stun_duration);
    }
  }
}

} // namespace

void Spawn(entt::registry &registry, const SpawnParams &params) {
  if (!registry.valid(params.owner) || params.element_tag == Tag::None ||
      params.duration <= 0.0f) {
    return;
  }

  auto &comp = registry.get_or_emplace<ElementPathComponent>(params.owner);
  // 870/872 元素互斥：切换元素时清空旧路径，避免元素语义混淆。
  if (comp.element_tag != params.element_tag) {
    comp.element_tag = params.element_tag;
    comp.segments = {};
    comp.count = 0;
  }

  // 尝试延长最近的活动段（同施法、端点相接、方向近似共线、未超长）。
  if (comp.count > 0) {
    PathSegment &last = comp.segments[comp.count - 1];
    if (last.active && last.skill_id == params.skill_id &&
        last.cast_id == params.cast_id) {
      const float gap = Vector2Distance(last.end, params.start);
      const Vector2 last_dir{last.end.x - last.start.x,
                             last.end.y - last.start.y};
      const Vector2 new_dir{params.end.x - last.start.x,
                            params.end.y - last.start.y};
      const float last_len = Vector2Length(last_dir);
      const float new_len = Vector2Length(new_dir);
      float cos_angle = 1.0f;
      if (last_len > 1e-3f && new_len > 1e-3f) {
        cos_angle = (last_dir.x * new_dir.x + last_dir.y * new_dir.y) /
                    (last_len * new_len);
      }
      if (gap <= kMergeGap && cos_angle >= kMergeMinCos &&
          new_len <= kMaxSegmentLength) {
        last.end = params.end;
        last.half_width = params.half_width;
        last.duration = params.duration;
        last.remaining = params.duration;
        last.amp = std::max(last.amp, params.amp);
        last.penetration = std::max(last.penetration, params.penetration);
        last.arc_freq_mult = params.arc_freq_mult;
        EmitPathVfx(params);
        return;
      }
    }
  }

  // 追加新段；达到上限时淘汰最旧段（整体前移）。
  if (comp.count >= kMaxSegmentsPerOwner) {
    for (int i = 0; i < comp.count - 1; ++i) {
      comp.segments[i] = comp.segments[i + 1];
    }
    comp.count = kMaxSegmentsPerOwner - 1;
  }

  PathSegment &seg = comp.segments[comp.count++];
  seg = PathSegment{};
  seg.start = params.start;
  seg.end = params.end;
  seg.half_width = params.half_width;
  seg.duration = params.duration;
  seg.remaining = params.duration;
  seg.amp = params.amp;
  seg.penetration = params.penetration;
  seg.arc_freq_mult = (params.arc_freq_mult > 0.01f) ? params.arc_freq_mult : 1.0f;
  seg.cast_id = params.cast_id;
  seg.skill_id = params.skill_id;
  seg.active = true;
  EmitPathVfx(params);
}

void Update(entt::registry &registry, float dt) {
  if (dt <= 0.0f) {
    return;
  }
  auto view = registry.view<ElementPathComponent>();
  for (auto owner : view) {
    auto &comp = view.get<ElementPathComponent>(owner);
    if (comp.element_tag == Tag::None) {
      continue;
    }
    for (int i = 0; i < comp.count; ++i) {
      PathSegment &seg = comp.segments[i];
      if (!seg.active) {
        continue;
      }
      seg.remaining -= dt;
      if (seg.remaining <= 0.0f) {
        seg.active = false;
        continue;
      }
      if (comp.element_tag == Tag::Lightning) {
        UpdateLightningArc(registry, owner, seg, dt);
      }
    }
  }
}

bool IsInside(entt::registry &registry, entt::entity owner, Tag element_tag,
              Vector2 position) {
  if (!registry.valid(owner) || element_tag == Tag::None) {
    return false;
  }
  const auto *comp = registry.try_get<ElementPathComponent>(owner);
  if (comp == nullptr || comp->element_tag != element_tag) {
    return false;
  }
  for (int i = 0; i < comp->count; ++i) {
    const PathSegment &seg = comp->segments[i];
    if (!seg.active) {
      continue;
    }
    if (PointSegmentDistance(position, seg.start, seg.end) <= seg.half_width) {
      return true;
    }
  }
  return false;
}

float AmpAgainst(entt::registry &registry, entt::entity owner, Tag element_tag) {
  if (!registry.valid(owner) || element_tag == Tag::None) {
    return 0.0f;
  }
  const auto *comp = registry.try_get<ElementPathComponent>(owner);
  if (comp == nullptr || comp->element_tag != element_tag) {
    return 0.0f;
  }
  float amp = 0.0f;
  for (int i = 0; i < comp->count; ++i) {
    const PathSegment &seg = comp->segments[i];
    if (seg.active) {
      amp = std::max(amp, seg.amp);
    }
  }
  return amp;
}

float PenetrationFor(entt::registry &registry, entt::entity owner,
                     Tag element_tag) {
  if (!registry.valid(owner) || element_tag == Tag::None) {
    return 0.0f;
  }
  const auto *comp = registry.try_get<ElementPathComponent>(owner);
  if (comp == nullptr || comp->element_tag != element_tag) {
    return 0.0f;
  }
  float pen = 0.0f;
  for (int i = 0; i < comp->count; ++i) {
    const PathSegment &seg = comp->segments[i];
    if (seg.active) {
      pen = std::max(pen, seg.penetration);
    }
  }
  return pen;
}

float ShieldReductionFor(entt::registry &registry, entt::entity owner,
                         Tag element_tag) {
  if (!registry.valid(owner) || element_tag == Tag::None) {
    return 0.0f;
  }
  const float pct = ResolveElementShieldPct(registry, owner);
  if (pct <= 0.0f) {
    return 0.0f;
  }
  // 876 的「站在自己路径上」以 owner 自身位置判定。
  Vector2 self_pos{0.0f, 0.0f};
  if (!TryGetEntityPosition(registry, owner, self_pos)) {
    return 0.0f;
  }
  if (!IsInside(registry, owner, element_tag, self_pos)) {
    return 0.0f;
  }
  return pct;
}

void Detonate(entt::registry &registry, entt::entity owner, Tag element_tag,
              Vector2 position, float radius) {
  if (!registry.valid(owner)) {
    return;
  }
  ApplyBurst(registry, owner, element_tag, position, radius);
}

void ClearForTests(entt::registry &registry) {
  registry.clear<ElementPathComponent>();
}

} // namespace NoMoreDay::element_path
