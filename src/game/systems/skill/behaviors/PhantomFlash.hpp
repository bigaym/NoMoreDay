#include "game/components/SkillDefs.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorBase.hpp"
#include <entt/entt.hpp>

namespace NoMoreDay::skills {

struct PhantomFlash : SkillBehaviorBase<PhantomFlash> {
  static constexpr uint32_t kSkillId = 9;

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec);

  /**
   * @brief Update logic for Phantom Flash counter state.
   * @return true if the counter state should be removed (timed out or triggered).
   */
  static bool Update(entt::registry &registry, entt::entity entity,
                     PhantomFlashComponent &pf, float dt);
};

} // namespace NoMoreDay::skills
