#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace NoMoreDay {
struct BakedSkillProfile;
struct SpecializedSkill;
} // namespace NoMoreDay

namespace NoMoreDay::skills {

/**
 * @brief 解析技能烘焙档案：优先返回槽位缓存，未命中时回退烘焙到栈上暂存。
 *
 * 多个技能行为的 DoCast 重复了同一段样板：先取 ActiveSkillsComponent::baked_profiles
 * 缓存；未命中且存在专精槽时，按首个同 ID 槽位烘焙到调用方栈对象。本基元抽取该
 * 样板，命中语义与 SkillSystem::GetBakedSkillProfile 逐字一致。
 *
 * @param registry     ECS 注册表（回退烘焙会写实体组件，故需非 const 引用）
 * @param owner        已解析的施法者实体（召唤物命中回调须先解析到真正的 owner）
 * @param skillId      技能 ID
 * @param scratch      调用方栈上的烘焙暂存；仅未命中且发生回退烘焙时被写入
 * @param fallbackSpec 可选的技能专属合成专精：未命中时优先用它烘焙，而非扫描
 *                     specialized_slots（技能9 形态参数按 active_nodes 合成回退需要）
 * @return 命中的缓存档案指针；回退烘焙成功时返回 &scratch；无档案来源时返回 nullptr
 */
[[nodiscard]] const BakedSkillProfile *
ResolveBakedProfile(entt::registry &registry, entt::entity owner, uint32_t skillId,
                    BakedSkillProfile &scratch,
                    const SpecializedSkill *fallbackSpec = nullptr);

} // namespace NoMoreDay::skills
