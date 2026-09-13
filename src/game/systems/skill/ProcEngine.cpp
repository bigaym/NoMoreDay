#include "game/systems/skill/ProcEngine.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/TriggerRuleComponent.hpp"
#include "game/foundation/data/TagRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "core/logging/Logger.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include <array>
#include <algorithm>

namespace NoMoreDay {

void ProcEngine::DispatchEvent(entt::registry& reg, entt::entity listener, const CombatEvent& event) {
    if (!reg.valid(listener)) return;
    if (event.trigger_depth >= 2) return; // 递归深度保护 (最大深度 2)

    struct PendingAction {
        uint32_t skill_id = 0;
        TriggerTargetPolicy target_mode = TriggerTargetPolicy::Victim;
        entt::entity target_entity = entt::null;
        Vector2 target_pos{0.0f, 0.0f};
        uint8_t depth = 0;
        float effectiveness = 1.0f;
        uint32_t cooldown_refund_skill_id = 0;
        float cooldown_refund_amount = 0.0f;
    };
    std::array<PendingAction, 4> pending_actions{};
    uint8_t action_count = 0;

    // 阶段一：在受控局部作用域内安全收集触发动作与目标信息
    {
        auto* triggerComp = reg.try_get<TriggerRuleComponent>(listener);
        if (!triggerComp || triggerComp->rule_count == 0) return;

        for (uint8_t i = 0; i < triggerComp->rule_count; ++i) {
            auto& rule = triggerComp->rules[i];
            if (rule.listen_event != event.type || rule.current_cooldown > 0.0f) continue;
            // 需暴击的规则（如 335 巨剑裂空）仅接受暴击命中事件
            if (rule.requires_crit && !event.isCrit) continue;
            // 触发前置节点校验: 施法者必须在来源技能上点出指定节点才允许触发。
            // 来源技能默认取本次事件的技能（event.skill_id），可用
            // required_source_skill_id 显式指定。技能8 855 巨剑共鸣要求施法者
            // 已在技能3(灵剑决)点出 330 巨剑降临。
            if (rule.required_source_node_id != 0) {
                const uint32_t source_skill = (rule.required_source_skill_id != 0)
                                                  ? rule.required_source_skill_id
                                                  : event.skill_id;
                if (!SkillSystem::HasAllocatedNode(reg, listener, source_skill,
                                                   rule.required_source_node_id)) {
                    continue;
                }
            }
            // 击杀来源技能过滤: 0=任意来源 (如 714 寂灭仅接受 skill_id==7 的击杀)
            if (rule.required_skill_id != 0 && event.skill_id != rule.required_skill_id) continue;
            // 仅近战命中事件触发 (如技能9 935 逆命反噬)
            if (rule.requires_melee_hit && !HasTag(event.tags, Tag::Melee)) continue;
            // 触发前置窗口: 施法者需处于对应形态/子窗口 (如技能9 绝影形态/逆脉)
            if (rule.required_window != TriggerWindow::None) {
                const auto* pt = reg.try_get<PhantomTranceComponent>(listener);
                if (!pt || pt->remaining <= 0.0f) continue;
                // DeathSeal 窗口口径统一：与 BloodSea.cpp 的禁疗/窗口判定同源走
                // IsDeathSealActive，不再直接读 params.death_seal。按设计 §11.8
                // 确认的时序，该判定排除 ending 终结帧——终结帧不触发 DeathSeal 规则；
                // 此为预期时序，无行为变更。
                if (rule.required_window == TriggerWindow::DeathSeal &&
                    !IsDeathSealActive(*pt)) {
                    continue;
                }
            }
            if (rule.event_tag_filter != Tag::None && (event.tags & rule.event_tag_filter) != rule.event_tag_filter) continue;

            float real_chance = rule.base_chance;
            if (rule.use_proc_scaling && event.parent_skill_cd > 0.0f) {
                // 恐怖黎明自适应加权公式
                real_chance *= (1.0f + event.parent_skill_cd * 0.2f);
            }

            const float roll = (rule.base_chance >= 1.0f && !rule.use_proc_scaling)
                                   ? 0.0f
                                   : utils::ThreadSafeRandom::GetFloat01();
            if (roll <= real_chance) {
                rule.current_cooldown = rule.internal_cooldown; // 标记冷却

                if (action_count < pending_actions.size()) {
                    const bool isDefensiveEvent =
                        (event.type == CombatEventType::OnTakeDamage ||
                         event.type == CombatEventType::OnDodge ||
                         event.type == CombatEventType::OnBlock ||
                         event.type == CombatEventType::OnReceiveAilment ||
                         event.type == CombatEventType::OnStun);

                    entt::entity attacker = isDefensiveEvent ? event.target : event.source;
                    entt::entity victim = isDefensiveEvent ? event.source : event.target;

                    entt::entity resolved_target = entt::null;
                    if (rule.target_mode == TriggerTargetPolicy::Self) {
                        resolved_target = listener;
                    } else if (rule.target_mode == TriggerTargetPolicy::Attacker) {
                        resolved_target = attacker;
                    } else { // Victim / GroundTarget
                        resolved_target = victim;
                    }

                    Vector2 resolved_pos{0.0f, 0.0f};
                    if (reg.valid(resolved_target)) {
                        if (const auto* pos = reg.try_get<Position>(resolved_target)) {
                            resolved_pos = {pos->x, pos->y};
                        }
                    } else if (const auto* listener_pos = reg.try_get<Position>(listener)) {
                        resolved_pos = {listener_pos->x, listener_pos->y};
                    }

                    pending_actions[action_count++] = {
                        rule.cast_skill_id,
                        rule.target_mode,
                        resolved_target,
                        resolved_pos,
                        static_cast<uint8_t>(event.trigger_depth + 1),
                        rule.effectiveness,
                        rule.cooldown_refund_skill_id,
                        rule.cooldown_refund_amount
                    };
                }
            }
        }
    } // 此时 triggerComp 指针完全脱离生命周期，后续组件池重分配无任何 UAF 风险

    // 阶段二：安全派生施法与冷却返还，执行严格的施法者活性检查
    for (uint8_t i = 0; i < action_count; ++i) {
        if (!reg.valid(listener)) break; // 施法者消亡立即中断
        const auto& act = pending_actions[i];
        if (act.cooldown_refund_skill_id != 0 && act.cooldown_refund_amount > 0.0f) {
            if (auto* active = reg.try_get<ActiveSkillsComponent>(listener)) {
                for (auto& slot : active->slots) {
                    if (slot.id == act.cooldown_refund_skill_id && slot.cooldown > 0.0f) {
                        slot.cooldown = std::max(0.0f, slot.cooldown - act.cooldown_refund_amount);
                    }
                }
            }
        }
        if (act.skill_id != 0) {
            SkillSystem::TriggerCast(reg, listener, act.skill_id, act.target_entity, act.target_pos, act.depth, act.effectiveness);
        }
    }
}

void ProcEngine::UpdateCooldowns(entt::registry& reg, float dt) {
    if (dt <= 0.0f) return;
    auto view = reg.view<TriggerRuleComponent>();
    for (auto entity : view) {
        auto& comp = view.get<TriggerRuleComponent>(entity);
        for (uint8_t i = 0; i < comp.rule_count; ++i) {
            if (comp.rules[i].current_cooldown > 0.0f) {
                comp.rules[i].current_cooldown = std::max(0.0f, comp.rules[i].current_cooldown - dt);
            }
        }
    }
}

} // namespace NoMoreDay
