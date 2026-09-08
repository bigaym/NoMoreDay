#pragma once

#include <entt/entt.hpp>
#include <raylib.h>
#include <type_traits>
#include <cstdint>
#include "game/foundation/components/CompactEntitySet.hpp"
#include "game/foundation/components/SkillDefs.hpp"

namespace NoMoreDay {

// 12 大通用交付原型枚举
enum class DeliveryArchetype : uint8_t {
  None = 0,
  DirectStrike = 1,        // 近战定向挥砍/震击
  BallisticProjectile = 2, // 直线与散射弹道
  BoomerangProjectile = 3, // 折返与悬停飞刃
  ChainBranching = 4,      // 连锁传导与弹跳
  AreaField = 5,           // 地表领域脉冲
  SkyfallImpact = 6,       // 天降打击与轰炸
  BeamChannel = 7,         // 持续引导光束/弹幕
  Mobility = 8,            // 位移突进/后撤
  OrbitingSentinel = 9,    // 环绕与浮游守护
  PhantasmClone = 10,      // 伴随残影镜像
  ReactiveWard = 11,       // 响应式反制护盾
  StickyDetonation = 12    // 附着印记延迟殉爆
};

// ============================================================================
// 原型 1: 近战定向挥砍与震击 (DirectStrikeDelivery)
// ============================================================================
enum class StrikeShape : uint8_t {
  Sector = 0,
  Box = 1,
  Radial = 2
};

struct DirectStrikeComponent {
  entt::entity owner = entt::null;
  uint64_t cast_id = 0;
  uint32_t skill_id = 0;
  StrikeShape shape = StrikeShape::Sector;
  float radius = 40.0f;
  float sector_angle = 120.0f;
  Vector2 box_extents{20.0f, 60.0f};
  float lifetime = 0.1f;
  float timer = 0.0f;
  bool hit_once = true;
  CompactEntitySet<8> hit_entities{};
  // 伤害有效载荷（more 倍率 / 元素 tags / 暴击修正），由生成方填充并交由
  // ProjectileSystem 的 DirectStrike 处理直接消费，避免遗留未读取的战斗快照。
  bool has_payload = false;
  DamagePayloadContext payload_context{};
};
static_assert(std::is_standard_layout_v<DirectStrikeComponent>);
static_assert(std::is_trivially_destructible_v<DirectStrikeComponent>);

// ============================================================================
// 原型 3: 折返与悬停回旋物 (BoomerangProjectileDelivery)
// ============================================================================
enum class BoomerangPhase : uint8_t {
  Outward = 0,
  HoverApex = 1,
  Returning = 2
};

struct BoomerangComponent {
  entt::entity owner = entt::null;
  uint64_t cast_id = 0;
  uint32_t skill_id = 0;
  BoomerangPhase phase = BoomerangPhase::Outward;
  float max_distance = 300.0f;
  float hover_duration = 0.3f;
  float hover_timer = 0.0f;
  float return_speed_accel = 800.0f;
  float pull_radius = 0.0f;    // 顶点黑洞牵引半径
  float pull_strength = 0.0f;  // 顶点牵引力度
  bool catch_by_owner = true;  // 接剑回调触发
  Vector2 apex_position{0.0f, 0.0f};

  // 飞行与回程参数
  float returnTimer = 0.5f;
  float returnSpeed = 0.0f;
  entt::entity returnTarget = entt::null;
  float returning_damage_mult = 1.0f; // 折返阶段伤害系数 (Node 230: 0.70f, Node 231: More 增伤)
  bool stun_on_apex_end = false;      // 顶点黑洞消散时击晕 (Node 233)
};
static_assert(std::is_standard_layout_v<BoomerangComponent>);
static_assert(std::is_trivially_destructible_v<BoomerangComponent>);

// ============================================================================
// 原型 4: 连锁与传导跳跃 (ChainBranchingDelivery)
// ============================================================================
struct ChainBranchComponent {
  entt::entity owner = entt::null;
  uint64_t cast_id = 0;
  uint32_t skill_id = 0;
  uint8_t remaining_chains = 3;
  float search_radius = 200.0f;
  float damage_falloff = 0.1f;
  entt::entity current_target = entt::null;
  CompactEntitySet<8> visited_targets{};
};
static_assert(std::is_standard_layout_v<ChainBranchComponent>);
static_assert(std::is_trivially_destructible_v<ChainBranchComponent>);

// ============================================================================
// 原型 6: 天降打击与流星轰击 (SkyfallImpactDelivery)
// ============================================================================
struct SkyfallImpactComponent {
  entt::entity owner = entt::null;
  uint64_t cast_id = 0;
  uint32_t skill_id = 0;
  Vector2 target_center{0.0f, 0.0f};
  float delay_before_impact = 0.4f;
  float impact_radius = 80.0f;
  uint8_t wave_count = 1;
  float wave_interval = 0.1f;
  float spread_radius = 0.0f;
  uint8_t leave_field_skill_id = 0;
  float timer = 0.0f;
  uint8_t waves_spawned = 0;
};
static_assert(std::is_standard_layout_v<SkyfallImpactComponent>);
static_assert(std::is_trivially_destructible_v<SkyfallImpactComponent>);

// ============================================================================
// 原型 7: 持续引导光束与弹幕流 (BeamChannelDelivery)
// ============================================================================
enum class BeamChannelMode : uint8_t {
  ContinuousLaser = 0,
  BarrageEmitter = 1
};

struct BeamChannelComponent {
  entt::entity owner = entt::null;
  uint64_t cast_id = 0;
  uint32_t skill_id = 0;
  BeamChannelMode mode = BeamChannelMode::ContinuousLaser;
  float max_channel_time = 5.0f;
  float current_channel_time = 0.0f;
  float tick_interval = 0.2f;
  float tick_timer = 0.0f;
  float turn_rate = 360.0f;               // 转向角速度限制
  bool aim_assist = false;                // 索敌吸附锁定
  uint32_t finisher_trigger_skill_id = 0; // 引导结束衍生技 (如天剑降世)
  Vector2 target_pos{0.0f, 0.0f};
  bool is_empowered = false;
  float bonus_damage_mult = 1.0f;
};
static_assert(std::is_standard_layout_v<BeamChannelComponent>);
static_assert(std::is_trivially_destructible_v<BeamChannelComponent>);

// ============================================================================
// 原型 8: 位移冲刺与机动闪避 (MobilityDelivery)
// ============================================================================
enum class MobilityType : uint8_t {
  Dash = 0,
  Blink = 1,
  Leap = 2
};

struct MobilityComponent {
  entt::entity target_entity = entt::null;
  MobilityType type = MobilityType::Dash;
  Vector2 direction{0.0f, 0.0f};
  float speed = 500.0f;
  float duration = 0.3f;
  float timer = 0.0f;
  bool has_invulnerability = false;
  bool leaves_motion_trail = true;
  uint32_t spawn_clone_node = 0;
  entt::entity owner = entt::null;
  uint32_t skill_id = 0;
  uint64_t cast_id = 0;
};
static_assert(std::is_standard_layout_v<MobilityComponent>);
static_assert(std::is_trivially_destructible_v<MobilityComponent>);

// ============================================================================
// 原型 9: 环绕守护与浮游伴身物 (OrbitingSentinelDelivery)
// ============================================================================
struct OrbitingSentinelComponent {
  entt::entity anchor_entity = entt::null;
  uint32_t skill_id = 0;
  uint8_t count = 3;
  float orbit_radius = 60.0f;
  float current_angle = 0.0f;
  float angular_velocity = 180.0f;
  float interception_chance = 0.0f;
  float attack_scan_radius = 0.0f;
  float attack_interval = 1.0f;
  float attack_timer = 0.0f;
  uint64_t cast_id = 0;
  float damage_mult = 1.0f;
};
static_assert(std::is_standard_layout_v<OrbitingSentinelComponent>);
static_assert(std::is_trivially_destructible_v<OrbitingSentinelComponent>);

// ============================================================================
// 原型 10: 伴随残影与幻象动作镜像 (PhantasmCloneDelivery)
// ============================================================================
struct PhantasmCloneComponent {
  entt::entity creator = entt::null;
  uint32_t skill_id = 0;
  float delay_before_cast = 0.2f;
  float lifetime = 1.5f;
  float damage_scale = 0.5f;
  float timer = 0.0f;
  bool has_cast = false;
  SkillSnapshot snapshot{};
};
static_assert(std::is_standard_layout_v<PhantasmCloneComponent>);
static_assert(std::is_trivially_destructible_v<PhantasmCloneComponent>);

// ============================================================================
// 原型 11: 响应式护盾与反制屏障 (ReactiveWardDelivery)
// ============================================================================
struct ReactiveWardComponent {
  entt::entity owner = entt::null;
  float ward_duration = 3.0f;
  float counter_window = 0.5f;
  float timer = 0.0f;
  float damage_absorb_pool = 0.0f;
  uint32_t counter_skill_id = 0;
  bool triggered = false;
};
static_assert(std::is_standard_layout_v<ReactiveWardComponent>);
static_assert(std::is_trivially_destructible_v<ReactiveWardComponent>);

// ============================================================================
// 原型 12: 附着印记与延迟殉爆 (StickyDetonationDelivery)
// ============================================================================
struct StickyDetonationComponent {
  entt::entity attacker = entt::null;
  uint32_t source_skill_id = 0;
  uint8_t current_stacks = 1;
  uint8_t max_stacks = 5;
  float timer = 4.0f;
  float explode_radius = 80.0f;
  bool explode_on_death = true;
  bool explode_on_max_stacks = false;
};
static_assert(std::is_standard_layout_v<StickyDetonationComponent>);
static_assert(std::is_trivially_destructible_v<StickyDetonationComponent>);

// ============================================================================
// 消耗剑意施放标记 (技能2 节点251/255 联动)
// ============================================================================
// 标识投射物源自「消耗剑意的裂空斩」施放 (251 剑意爆发达到门槛并实际消耗时打标)。
// DoHit 命中处理仅对带此标记的投射物执行 255 意念回流 roll；
// 普通施放 (未消耗剑意) 与 254 回响斩衍生波不带标记，避免凭空回剑意。
// 注：253 湮灭波的无视抗性标志已收敛为 Projectile::ignore_resist 布尔字段，
// 旧的 ProjectileIgnoreResist 独立组件随之移除，避免双份事实来源。
struct IntentConsumedCastTag {};
static_assert(std::is_standard_layout_v<IntentConsumedCastTag>);
static_assert(std::is_trivially_destructible_v<IntentConsumedCastTag>);

} // namespace NoMoreDay
