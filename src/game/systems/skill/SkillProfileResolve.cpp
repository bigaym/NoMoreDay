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
  // 1) 缓存命中：直接复用槽位内已烘焙档案，调用方 scratch 不被触碰。
  if (const auto *profile =
          SkillSystem::GetBakedSkillProfile(registry, owner, skillId)) {
    return profile;
  }

  // 2) 技能专属合成专精：优先于槽位扫描，保证合成回退与生产烘焙同一路径。
  if (fallbackSpec != nullptr) {
    SkillSpecializationBaker::Bake(registry, owner, skillId, fallbackSpec, scratch,
                                   nullptr);
    return &scratch;
  }

  // 3) 槽位回退：取首个同 ID 专精，走与生产一致的烘焙路径。
  if (const auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
    for (const auto &spec : active->specialized_slots) {
      if (spec.skill_id == skillId) {
        SkillSpecializationBaker::Bake(registry, owner, skillId, &spec, scratch,
                                       nullptr);
        return &scratch;
      }
    }
  }

  return nullptr;
}

} // namespace NoMoreDay::skills
