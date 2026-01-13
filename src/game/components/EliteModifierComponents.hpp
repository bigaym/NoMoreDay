#pragma once

#include <entt/entt.hpp>
#include <vector>


namespace NoMoreDay {

/**
 * @brief 灵魂链接组件 - 让怪物与周围友军共享血量池
 *
 * 当 Link 组内任一成员受伤时，伤害平均分配给组内所有成员。
 * 这使得 AOE 技能的效率降低，需要玩家优先击杀 Link 领袖或使用单体技能。
 */
struct SoulLinkComponent {
  std::vector<entt::entity> linkedEntities; // 链接的实体列表
  float linkRange = 150.0f;                 // 链接范围 (像素)
  int maxLinks = 5;                         // 最大链接数量
  bool isLinkLeader = false;                // 是否是链接组的领袖

  // 链接组的共享血量池 (仅领袖维护)
  float totalSharedHealth = 0.0f;
  float maxSharedHealth = 0.0f;

  // 视觉效果
  bool showLinkLines = true; // 是否显示链接线

  SoulLinkComponent() = default;
  SoulLinkComponent(float range, int maxCount = 5)
      : linkRange(range), maxLinks(maxCount) {}
};

/**
 * @brief 复仇者组件 - 周围友军死亡时获得强化
 *
 * 每当范围内友军被击杀，复仇者获得一层堆叠：
 * - 每层伤害 +10%
 * - 每层体型 +10%
 * - 最多 10 层
 */
struct AvengerComponent {
  int avengerStacks = 0;        // 当前复仇层数
  float damagePerStack = 0.10f; // 每层伤害加成系数 (10%)
  float sizePerStack = 0.10f;   // 每层体型增加系数 (10%)
  float stackRadius = 200.0f;   // 触发范围 (像素)
  int maxStacks = 10;           // 最大层数

  // 视觉效果
  float glowIntensity = 0.0f; // 发光强度 (随层数增加)

  AvengerComponent() = default;
  AvengerComponent(float radius, int max = 10)
      : stackRadius(radius), maxStacks(max) {}

  // 获取当前总伤害加成
  float GetDamageMultiplier() const {
    return 1.0f + (avengerStacks * damagePerStack);
  }

  // 获取当前总体型加成
  float GetSizeMultiplier() const {
    return 1.0f + (avengerStacks * sizePerStack);
  }

  // 添加层数
  void AddStack(int count = 1) {
    avengerStacks = std::min(avengerStacks + count, maxStacks);
    glowIntensity = static_cast<float>(avengerStacks) / maxStacks;
  }
};

// 快速查询标签
struct SoulLinkTag {};
struct AvengerTag {};

/**
 * @brief 刺客隐身标记组件
 *
 * 用于渲染系统识别隐身状态并调整 Alpha
 */
struct StealthedTag {
  float alpha = 0.3f; // 隐身时的透明度 (30%)
};

/**
 * @brief 坦克阻挡标记组件
 *
 * 用于物理系统识别高质量实体
 */
struct TankBlockingTag {
  float knockbackResistance = 0.8f; // 击退抗性 (80%)
};

} // namespace NoMoreDay
