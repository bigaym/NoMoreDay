#include "game/systems/skill/ProcEngine.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/TriggerRuleComponent.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/CombatSystem.hpp"
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
        if ((!triggerComp || !triggerComp->HasRule(9)) && event.type == CombatEventType::OnTakeDamage) {
            if (auto* pf = reg.try_get<PhantomFlashComponent>(listener)) {
                if (pf->counter_window > 0.0f && !pf->triggered) {
                    auto& trig = reg.get_or_emplace<TriggerRuleComponent>(listener);
                    TriggerRule counterRule;
                    counterRule.rule_id = 9;
                    counterRule.listen_event = CombatEventType::OnTakeDamage;
                    counterRule.target_mode = TriggerTargetPolicy::Attacker;
                    counterRule.cast_skill_id = 9;
                    counterRule.base_chance = 1.0f;
                    counterRule.use_proc_scaling = false;
                    counterRule.internal_cooldown = pf->counter_window;
                    counterRule.effectiveness = pf->synergy_shadow_hide ? 1.2f : 1.0f;
                    if (pf->flow_reset) {
                        counterRule.cooldown_refund_skill_id = 8;
                        counterRule.cooldown_refund_amount = 1.5f;
                    }
                    trig.AddRule(counterRule);
                    triggerComp = &trig;
                }
            }
        }
        if (!triggerComp || triggerComp->rule_count == 0) return;

        for (uint8_t i = 0; i < triggerComp->rule_count; ++i) {
            auto& rule = triggerComp->rules[i];
            if (rule.listen_event != event.type || rule.current_cooldown > 0.0f) continue;
            // 需暴击的规则（如 335 巨剑裂空）仅接受暴击命中事件
            if (rule.requires_crit && !event.isCrit) continue;
            if (rule.event_tag_filter != Tag::None && (event.tags & rule.event_tag_filter) != rule.event_tag_filter) continue;

            // 幻影闪反击规则状态门控：若已触发或反击窗口已关闭则跳过
            if (rule.rule_id == 9) {
                if (auto* pf = reg.try_get<PhantomFlashComponent>(listener)) {
                    if (pf->triggered || pf->counter_window <= 0.0f) {
                        continue;
                    }
                }
            }

            float real_chance = rule.base_chance;
            if (rule.use_proc_scaling && event.parent_skill_cd > 0.0f) {
                // 恐怖黎明自适应加权公式
                real_chance *= (1.0f + event.parent_skill_cd * 0.2f);
            }

            const float roll = (rule.base_chance >= 1.0f && !rule.use_proc_scaling)
                                   ? 0.0f
                                   : utils::ThreadSafeRandom::GetFloat01();
            if (roll <= real_chance) {
                rule.current_cooldown = (rule.rule_id == 9) ? 999.0f : rule.internal_cooldown; // 标记冷却（技能 9 立即闭锁防同帧重复）

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
        if (act.skill_id == 9 && event.type == CombatEventType::OnTakeDamage) {
            auto* pf = reg.try_get<PhantomFlashComponent>(listener);
            if (pf) {
                pf->triggered = true;
            }
            if (auto* trigComp = reg.try_get<TriggerRuleComponent>(listener)) {
                trigComp->RemoveRule(9);
            }
            if (reg.valid(act.target_entity) && reg.any_of<CombatStats>(act.target_entity)) {
                if (auto* victim_stats = reg.try_get<CombatStats>(listener)) {
                    if (victim_stats->damage_multipliers[0] <= 0.0f) {
                        victim_stats->damage_multipliers[0] = 1.0f;
                    }
                    DamagePool counterPool;
                    const float baseCounterDamage =
                        (std::max)(25.0f, victim_stats->damage_multipliers[0] * 100.0f);
                    const Tag counterTag =
                        (pf && pf->enchant_tag != Tag::None) ? pf->enchant_tag : Tag::Physical;
                    const float counterScale = (pf && pf->synergy_shadow_hide) ? 1.2f : 1.0f;
                    counterPool.Add(counterTag, baseCounterDamage * counterScale * act.effectiveness);

                    const auto counterResult = DamagePipeline::Calculate(
                        reg, listener, act.target_entity, 9, counterPool,
                        Tag::Hit | Tag::Melee);
                    if (counterResult.total_damage > 0.0f) {
                        CombatSystem::ApplyDamage(reg, act.target_entity,
                                                  counterResult.total_damage, listener,
                                                  counterResult.is_crit);
                    }
                    LOG_INFO("ProcEngine: Defensive counter resolved: victim={} attacker={} damage={}",
                             (uint32_t)listener, (uint32_t)act.target_entity, counterResult.total_damage);
                }
            }
        } else if (act.skill_id != 0) {
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
