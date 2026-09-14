#pragma once
#include "game/foundation/components/SkillDefs.hpp"

namespace NoMoreDay {

struct HeavenlySwordFieldComponent {
  // 公共头以本组件原默认值初始化：duration=5.0f / radius=140.0f / tick=0.5f，
  // 保持默认构造下的数值与时序行为不变。
  PersistentFieldHeader header = {
      .duration = 5.0f,
      .radius = 140.0f,
      .tick_interval = 0.5f,
  };
  float linked_cut_cooldown = 0.0f;
  float cycle_refund_timer = 1.0f;
  int spent_tiers = 0;
  int cycle_refunds_granted = 0;
  int echo_strikes_triggered = 0;
  int pending_scar_strikes = 0;
  float impact_damage_mult = 1.0f;
  float field_damage_mult = 1.0f;
  float resist_reduction = 6.0f;
  float extra_resist_reduction = 0.0f;
  float elite_first_second_timer = 0.0f;
  float elite_impact_bonus_mult = 0.0f;
  float elite_field_bonus_mult = 0.0f;
  float scar_delay_timer = 0.0f;
  float scar_interval = 0.0f;
  float scar_damage_mult = 0.0f;
  float spinning_heavens_bonus = 0.0f;
  float return_to_sheath_timer = 0.0f;
  float return_to_sheath_bonus_mult = 0.0f;
  bool return_to_sheath_ready = false;
  float afflicted_pressure_bonus_mult = 0.0f;
  float original_formation_attack_interval = 0.0f;
  float original_channel_tick_interval = 0.0f;
  BladeAttunement attunement = BladeAttunement::None;
  bool has_trigger_echo = false;
  bool has_cycle = false;
  bool has_domain_lock = false;
  bool has_polarization = false;
  bool lightning_tribunal = false;
  bool frozen_dominion = false;
  bool solar_incineration = false;
};
static_assert(std::is_standard_layout_v<HeavenlySwordFieldComponent>);
static_assert(std::is_trivially_destructible_v<HeavenlySwordFieldComponent>);

struct BloodSeaFieldComponent {
  // 公共头以本组件原默认值初始化：duration=5.0f / radius=120.0f / tick=0.25f，
  // 保持默认构造下的数值与时序行为不变。
  PersistentFieldHeader header = {
      .duration = 5.0f,
      .radius = 120.0f,
      .tick_interval = 0.25f,
  };
  float linked_pulse_cooldown = 0.0f;
  int consumed_bloodthirst = 0;
  int pulses_triggered = 0;
  float bonus_damage_mult = 1.0f;
  float leech_ratio = 0.12f;
  float move_follow_speed = 10.0f;
  float resist_shred = 0.0f;
  float pursuit_bonus_mult = 0.0f;
  float aftershock_bonus_mult = 0.0f;
  float return_empower_timer = 0.0f;
  float return_empower_bonus_mult = 0.0f;
  float close_pressure_bonus_mult = 0.0f;
  float linked_pressure_bonus_mult = 0.0f;
  float miasma_duration_bonus = 0.0f;
  float void_damage_bonus_mult = 0.0f;
  // 1217 绝影共噬窗口：施放瞬间若已点 1217 且技能9 逆脉/免死窗口激活，则 DoCast 写入
  // 前 N 秒的增伤 / 增疗倍率；UpdateField 每帧递减 timer，归零后增益自动失效。
  float shared_devour_timer = 0.0f;
  float shared_devour_damage_mult = 0.0f;
  float shared_devour_heal_mult = 0.0f;
  bool has_trigger_burst = false;
  bool has_recovery_keystone = false;
  bool has_void_keystone = false;
  bool torrent_form = false;
  bool ring_form = false;
};
static_assert(std::is_standard_layout_v<BloodSeaFieldComponent>);
static_assert(std::is_trivially_destructible_v<BloodSeaFieldComponent>);

} // namespace NoMoreDay
