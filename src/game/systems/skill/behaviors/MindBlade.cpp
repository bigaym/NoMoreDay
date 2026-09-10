/**
 * @file MindBlade.cpp
 * @brief 心剑 (ID 7) - 模块化持续引导激光交付实现
 */
#include "MindBlade.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"

namespace NoMoreDay::skills {

namespace MindBladeNodes {
// 分支 D 转质节点：两者互斥由数据层契约保证，行为层只做元素映射。
constexpr uint32_t GlacialShards = 770; // 天外冰晶 → Cold
constexpr uint32_t OrbitalStrike = 772; // 神雷天罡 → Lightning
} // namespace MindBladeNodes

void MindBlade::OnCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
  // ChannelingComponent 仅承担输入保活窗口与交付层所需的引导元数据。
  auto &chan = registry.emplace_or_replace<ChannelingComponent>(owner);
  chan.skill_id = kSkillId;
  chan.channel_timer = 0.25f; // 输入保活窗口：按住时由 SkillSystem::HandleSkillInput 刷新
  chan.tick_interval = 0.3f;
  chan.tick_timer = -0.01f;
  chan.target_pos = exec.target_pos;
  chan.is_empowered = exec.is_empowered;
  chan.total_duration = 0.0f;
  chan.cast_id = exec.cast_id;
  chan.conversion_tag = Tag::None;
  chan.bonus_damage_mult = 1.0f;
  chan.bonus_crit_chance = 0.0f;
  chan.synergy_lock = false;

  const auto *profile = SkillSystem::GetBakedSkillProfile(registry, owner, kSkillId);
  BakedSkillProfile localProfile;
  if (!profile && registry.all_of<ActiveSkillsComponent>(owner)) {
    for (const auto &spec : registry.get<ActiveSkillsComponent>(owner).specialized_slots) {
      if (spec.skill_id == kSkillId) { SkillSpecializationBaker::Bake(registry, owner, kSkillId, &spec, localProfile, nullptr); profile = &localProfile; break; }
    }
  }

  // 转质元素：优先按 770/772 的已投点数映射（数据层保证互斥，映射方向以此为准），
  // 再退化为烘焙元素标签，最后回落到御天诀共鸣。
  Tag elementTag = Tag::None;
  if (const auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
    for (const auto &spec : active->specialized_slots) {
      if (spec.skill_id != kSkillId) continue;
      // 与 GetSkill7Point 口径一致：仅存在记录不算激活，必须已投点数 > 0；
      // 770/772 的互斥 max1 语义仍由数据层契约保证。
      const auto glacialIt = spec.allocated_points.find(MindBladeNodes::GlacialShards);
      const auto orbitalIt = spec.allocated_points.find(MindBladeNodes::OrbitalStrike);
      if (glacialIt != spec.allocated_points.end() && glacialIt->second > 0) {
        elementTag = Tag::Cold;
      } else if (orbitalIt != spec.allocated_points.end() && orbitalIt->second > 0) {
        elementTag = Tag::Lightning;
      }
      break;
    }
  }
  if (elementTag == Tag::None && profile &&
      (profile->effective_tags & (Tag::Cold | Tag::Lightning)) != Tag::None) {
    elementTag = profile->effective_tags & (Tag::Cold | Tag::Lightning);
  }
  if (elementTag == Tag::None) {
    elementTag = systems::BladeResourceService::GetHeavenlyAttunementElementTag(registry, owner);
  }
  chan.conversion_tag = elementTag;

  const float moreDamage = profile ? profile->more_damage_mult : 1.0f;
  chan.bonus_damage_mult *= moreDamage;

  // BeamChannelComponent 为唯一交付权威：max_channel_time 是硬性引导上限，
  // 绝不写入输入保活窗口值；输入保活由 ChannelingComponent::channel_timer 独立承担。
  auto &beam = registry.emplace_or_replace<BeamChannelComponent>(owner);
  beam.owner = owner;
  beam.cast_id = exec.cast_id;
  beam.skill_id = kSkillId;
  beam.mode = BeamChannelMode::ContinuousLaser;
  beam.max_channel_time = data::SkillMechanicsRegistry::Get().GetFloat(kSkillId, 0, "max_channel_time", 5.0f);
  beam.tick_interval = data::SkillMechanicsRegistry::Get().GetFloat(kSkillId, 0, "tick_interval", 0.3f);
  beam.tick_timer = 0.0f;
  beam.target_pos = exec.target_pos;
  beam.is_empowered = exec.is_empowered;
  beam.bonus_damage_mult = moreDamage;
  beam.aim_assist = false;
}

REGISTER_SKILL_BEHAVIOR(MindBlade)
void RegisterMindBlade() {}
} // namespace NoMoreDay::skills
