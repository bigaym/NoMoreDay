/**
 * @file MindBlade.cpp
 * @brief 心剑 (ID 7) - 模块化持续引导激光交付实现
 */
#include "MindBlade.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include <algorithm>

namespace NoMoreDay::skills {

namespace MindBladeNodes {
constexpr uint32_t HeavenMan = 713;
constexpr uint32_t MindLock = 730;
constexpr uint32_t MindUnity = 750;
constexpr uint32_t OneLaw = 752;
constexpr uint32_t RayFocus = 770;
} // namespace MindBladeNodes

void MindBlade::OnCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
  auto &chan = registry.emplace_or_replace<ChannelingComponent>(owner);
  chan.skill_id = kSkillId; chan.channel_timer = 0.25f; chan.tick_interval = 0.3f; chan.tick_timer = -0.01f;
  chan.target_pos = exec.target_pos; chan.is_empowered = exec.is_empowered; chan.total_duration = 0.0f;
  chan.cast_id = exec.cast_id; chan.conversion_tag = Tag::None; chan.bonus_damage_mult = 1.0f;
  chan.bonus_crit_chance = 0.0f; chan.synergy_lock = false;

  const auto *profile = SkillSystem::GetBakedSkillProfile(registry, owner, kSkillId);
  BakedSkillProfile localProfile;
  if (!profile && registry.all_of<ActiveSkillsComponent>(owner)) {
    for (const auto &spec : registry.get<ActiveSkillsComponent>(owner).specialized_slots) {
      if (spec.skill_id == kSkillId) { SkillSpecializationBaker::Bake(registry, owner, kSkillId, &spec, localProfile, nullptr); profile = &localProfile; break; }
    }
  }

  if (profile ? ((profile->delivery.feature_flags & 8) != 0) : exec.active_nodes.test(MindBladeNodes::OneLaw % 100)) {
    (void)SkillSystem::ConsumeSwordIntent(registry, owner, SkillConstants::DEFAULT_MAX_SWORD_INTENT, kSkillId);
  }
  if (profile ? ((profile->delivery.feature_flags & 4) != 0) : exec.active_nodes.test(MindBladeNodes::MindUnity % 100)) {
    chan.conversion_tag = Tag::Void;
  }
  if (profile ? ((profile->delivery.feature_flags & 1) != 0) : exec.active_nodes.test(MindBladeNodes::HeavenMan % 100)) chan.bonus_crit_chance += 20.0f;
  if (profile ? ((profile->delivery.feature_flags & 16) != 0) : exec.active_nodes.test(MindBladeNodes::RayFocus % 100)) chan.conversion_tag = Tag::Lightning;
  if (profile ? ((profile->delivery.feature_flags & 2) != 0) : exec.active_nodes.test(MindBladeNodes::MindLock % 100)) chan.synergy_lock = true;
  if (chan.conversion_tag == Tag::None) chan.conversion_tag = systems::BladeResourceService::GetHeavenlyAttunementElementTag(registry, owner);
  chan.bonus_damage_mult *= (profile ? profile->more_damage_mult : 1.0f);

  auto &beam = registry.emplace_or_replace<BeamChannelComponent>(owner);
  beam.owner = owner; beam.cast_id = exec.cast_id; beam.skill_id = kSkillId;
  beam.mode = BeamChannelMode::ContinuousLaser; beam.max_channel_time = chan.channel_timer;
  beam.tick_interval = chan.tick_interval; beam.target_pos = exec.target_pos;
  beam.is_empowered = exec.is_empowered; beam.bonus_damage_mult = chan.bonus_damage_mult;
}

bool MindBlade::Update(entt::registry &registry, entt::entity entity,
                       MindBladeAI &ai, MindBladeComponent &comp, float dt,
                       systems::SpatialHashGrid &grid) {
  (void)registry; (void)entity; (void)ai; (void)comp; (void)dt; (void)grid;
  return true;
}

REGISTER_SKILL_BEHAVIOR(MindBlade)
void RegisterMindBlade() {}
} // namespace NoMoreDay::skills
