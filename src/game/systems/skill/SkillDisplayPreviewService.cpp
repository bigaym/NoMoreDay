#include "game/systems/skill/SkillDisplayPreviewService.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/data/TagRegistry.hpp"
#include "game/components/Stats.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/systems/modifier/SkillSpecModifierAdapter.hpp"

namespace NoMoreDay {

SkillDisplayPreview
SkillDisplayPreviewService::Build(entt::registry& registry,
                                  entt::entity player,
                                  uint32_t skillId)
{
    SkillDisplayPreview preview;

    const SkillData* skillData = SkillRegistry::Get().GetSkill(skillId);
    if (!skillData)
    {
        return preview;
    }

    const auto* combatStats = registry.try_get<CombatStats>(player);
    if (!combatStats)
    {
        return preview;
    }

    // 1. Duration logic
    float baseDuration = skillData->GetParam("field_duration", 0.0f);
    if (baseDuration <= 0.0f) {
        baseDuration = skillData->GetParam("summon_lifetime", 0.0f);
    }
    if (baseDuration <= 0.0f) {
        baseDuration = skillData->GetParam("channel_window", 0.0f);
    }

    if (baseDuration > 0.0f)
    {
        preview.has_duration = true;
        preview.display_duration_seconds = baseDuration * combatStats->duration_scale;

        // Overlay specialization-specific duration modifiers if any
        const auto* activeSkills = registry.try_get<ActiveSkillsComponent>(player);
        if (activeSkills)
        {
            for (const auto& slot : activeSkills->specialized_slots)
            {
                if (slot.skill_id == skillId)
                {
                    const auto* tree = SkillRegistry::Get().GetSkillTree(skillId);
                    if (tree)
                    {
                        for (const auto& [nodeId, points] : slot.allocated_points)
                        {
                            if (points <= 0)
                                continue;
                            auto it = tree->nodes.find(nodeId);
                            if (it != tree->nodes.end())
                            {
                                for (const auto& mod : it->second.stat_modifiers)
                                {
                                    if (mod.type == StatType::DurationScale)
                                    {
                                        if (mod.mode == ModifierMode::PercentAdd)
                                        {
                                            preview.display_duration_seconds += baseDuration * mod.value * points;
                                        }
                                        else if (mod.mode == ModifierMode::PercentMult)
                                        {
                                            preview.display_duration_seconds *= (1.0f + mod.value);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }
    }

    // 2. Estimated Damage logic
    preview.has_estimated_damage = true;
    
    // Calculate total base damage including weapon and flat additions
    float baseDamage = (combatStats->min_weapon_damage + combatStats->max_weapon_damage) / 2.0f;
    for (float flat : combatStats->flat_damage) {
        baseDamage += flat;
    }

    // Apply global character multipliers (Strength/Intelligence/etc.)
    // We average the elemental multipliers as a build-comparison baseline
    float globalMultSum = 0.0f;
    for (float m : combatStats->damage_multipliers) {
        globalMultSum += m;
    }
    float avgGlobalMult = globalMultSum / static_cast<float>(combatStats->damage_multipliers.size());
    
    float totalDamage = baseDamage * avgGlobalMult;

    // Apply specialization-node modifiers
    float specMult = 1.0f;
    const auto* activeSkills = registry.try_get<ActiveSkillsComponent>(player);
    if (activeSkills)
    {
        for (const auto& slot : activeSkills->specialized_slots)
        {
            if (slot.skill_id == skillId)
            {
                const auto nodeIds = SkillSpecModifierAdapter::CollectAllocatedNodeIds(slot);
                specMult = SkillSpecModifierAdapter::EvaluateDamageMultiplier(skillId, skillData->tags, nodeIds);
                break;
            }
        }
    }

    preview.estimated_damage_value = totalDamage * specMult;
    
    // Determine damage mode based on skill tags
    if (HasTag(skillData->tags, Tag::DamageOverTime))
    {
        preview.estimated_damage_mode = SkillDisplayDamageMode::PerSecond;
    }
    else if (HasTag(skillData->tags, Tag::Channeled))
    {
        preview.estimated_damage_mode = SkillDisplayDamageMode::ChannelWindow;
    }
    else
    {
        preview.estimated_damage_mode = SkillDisplayDamageMode::Hit;
    }

    return preview;
}

} // namespace NoMoreDay