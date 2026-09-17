#include "game/systems/skill/SkillProfileResolve.hpp"

#include "game/foundation/components/SkillDefs.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"

namespace NoMoreDay::skills {

const BakedSkillProfile *ResolveBakedProfile(entt::registry &registry,
                                             entt::entity owner,
                                             uint32_t skillId,
                                             BakedSkillProfile &scratch,
                                             const SpecializedSkill *fallbackSpec) {
  // 1) 缓存命中：仅接受 is_baked 的档案。哨兵档案（skillData 缺失时写入）虽然
  // skill_id 匹配，但只有默认值不可消费，必须继续走回退路径。
  if (const auto *profile =
          SkillSystem::GetBakedSkillProfile(registry, owner, skillId)) {
    if (profile->is_baked) {
      return profile;
    }
  }

  // 2) 技能专属合成专精：优先于槽位扫描，保证合成回退与生产烘焙同一路径。
  if (fallbackSpec != nullptr) {
    SkillSpecializationBaker::Bake(registry, owner, skillId, fallbackSpec, scratch,
                                   nullptr);
    return scratch.is_baked ? &scratch : nullptr;
  }

  // 3) 槽位回退：取首个同 ID 专精，走与生产一致的烘焙路径。
  if (const auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
    for (const auto &spec : active->specialized_slots) {
      if (spec.skill_id == skillId) {
        SkillSpecializationBaker::Bake(registry, owner, skillId, &spec, scratch,
                                       nullptr);
        return scratch.is_baked ? &scratch : nullptr;
      }
    }
  }

  return nullptr;
}

} // namespace NoMoreDay::skills
