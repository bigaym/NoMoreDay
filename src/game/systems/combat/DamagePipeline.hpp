#pragma once
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/contracts/DamagePipelineTypes.hpp"
#include "game/systems/combat/damage/DamageSnapshot.hpp"
#include <entt/entt.hpp>
#include <taskflow/taskflow.hpp>
#include <vector>

namespace NoMoreDay {

class DamagePipeline {
public:
  static DamageResult Calculate(entt::registry &registry,
                                const DamageRequest &request);

  static DamageExecutionResult Execute(entt::registry &registry,
                                       const DamageRequest &request,
                                       entt::entity apply_attacker = entt::null,
                                       bool show_vfx = true);

  /**
   * @brief Executes the unified damage pipeline with defense contract
   * mitigation.
   *
   * @param attacker The entity performing the attack
   * @param defender The entity receiving the damage
   * @param skill_id The ID of the skill being used (to look up base tags/mods)
   * @param base_pool The initial flat damage from the skill/weapon.
   * @param additional_tags Extra tags from the specific hit (e.g., Critical,
   * Hit)
   * @param source_entity The entity representing the skill execution (e.g.,
   * Projectile, Shadow)
   */
  static DamageResult Calculate(entt::registry &registry, entt::entity attacker,
                                entt::entity defender, uint32_t skill_id,
                                const DamagePool &base_pool,
                                Tag additional_tags = Tag::None,
                                entt::entity source_entity = entt::null,
                                bool is_simulation = false);

  /**
   * @brief Optimized batch calculation for many targets.
   *
   * 契约说明（P4 审查跟踪项）：本 void 批量入口当前仅被测试/基准调用，生产
   * 批量路径统一走 ResolveDamageBatch → CalculateBatchResults → 逐目标
   * Calculate。二者语义不完全等价：本入口不复用单目标路径的 invulnerability
   * 拦截、FrostAmp 增伤与 source_entity 快照（快照由 CreateSnapshot 在批内
   * 自行构建）。因此本接口不应用于生产结算；如需生产批量，请使用
   * ResolveDamageBatch。此处保留不改行为，仅明确契约边界。
   */
  // 标记弃用仅为防误用：生产批量结算必须走 ResolveDamageBatch。
  [[deprecated("Use DamageResolutionHooks::ResolveDamageBatch in production; "
               "CalculateBatch is test/benchmark only")]]
  static void CalculateBatch(entt::registry &registry, entt::entity attacker,
                             const std::vector<entt::entity> &defenders,
                             uint32_t skill_id, const DamagePool &base_pool,
                             Tag additional_tags,
                             entt::entity source_entity = entt::null,
                             tf::Executor *executor = nullptr);

  /**
   * @brief Computes the per-target DamageResult for a damage request without
   * applying damage. Used by the calculateBatch resolution hook so batch
   * callers observe real results rather than an empty vector (P1-1).
   */
  static std::vector<DamageResult>
  CalculateBatchResults(entt::registry &registry, const DamageRequest &request);

  // P3-3: 守方减伤分流可观测性。计数器按目标/向量块累计，主要用于测试断言
  // 路径选择，不参与结算逻辑。
  struct BatchKernelStats {
    uint64_t scalar_targets = 0; // 走标量内核的目标数
    uint64_t simd_blocks = 0;    // 走 SIMD 内核的向量块数
    uint64_t simd_targets = 0;   // 走 SIMD 内核的目标数
  };

  [[nodiscard]] static BatchKernelStats GetBatchKernelStats();
  static void ResetBatchKernelStats();

  // P3-3 测试专用：强制守方减伤全部走标量内核，用于与 SIMD 内核做同输入
  // 数值一致性对照。仅应由测试调用，生产默认 false。
  static void SetForceScalarKernelForTests(bool force);

  // 结算期延后动作：反击/受击触发在结算中途入队，最外层入口收尾统一派发，
  // 杜绝结算中重入修改组件池。内部实现细节，公开仅为让门面同 TU 的队列助手引用。
  struct DeferredCombatAction {
    DamageRequest request;
    entt::entity apply_attacker = entt::null;
    bool show_vfx = true;
  };

  // P3-1: 从攻方构建紧凑快照 (静态乘区烘焙 + 动态条件规则)。
  // P3-2: 投射物/DoT 挂载与批量内核复用同一实现。
  static damage::AttackerSnapshot
  CreateSnapshot(entt::registry &registry, entt::entity attacker,
                 uint32_t skill_id, const DamagePool &base_pool, Tag hit_tags,
                 entt::entity source_entity,
                 const DamagePayloadContext *payload = nullptr,
                 DamageOrigin origin = DamageOrigin::DirectSkillCast);

  // P3-2: 为实体挂载 DamageSnapshotComponent。holder 通常为投射物/DoT 载体；
  // attacker 为参与快照演算的攻方实体 (可为 holder 自身)。值拷贝，无裸指针。
  static void AttachSnapshotComponent(
      entt::registry &registry, entt::entity holder, entt::entity attacker,
      uint32_t skill_id, const DamagePool &base_pool, Tag hit_tags,
      entt::entity source_entity, const DamagePayloadContext *payload = nullptr,
      DamageOrigin origin = DamageOrigin::DirectSkillCast);

private:
};

} // namespace NoMoreDay
