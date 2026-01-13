#pragma once
#include "game/components/ItemComponent.hpp"
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>


namespace NoMoreDay {

// 碎片类型
enum class FragmentType : uint8_t {
  Terrain, // 地形碎片 - 决定生成器类型
  Affix,   // 词缀碎片 - 增加怪物/掉落属性
  Unique   // 特殊碎片 - Boss房/商人/宝库
};

// JSON 序列化
inline void to_json(nlohmann::json &j, const FragmentType &e) {
  j = static_cast<uint8_t>(e);
}
inline void from_json(const nlohmann::json &j, FragmentType &e) {
  e = static_cast<FragmentType>(j.get<uint8_t>());
}

// 碎片元素属性 (用于共鸣计算)
enum class FragmentElement : uint8_t {
  None = 0,
  Fire,      // 火焰 - 增加火焰敌人/火焰掉落
  Cold,      // 冰霜 - 增加冰霜敌人
  Lightning, // 闪电 - 增加攻速词缀
  Shadow,    // 暗影 - 增加暗影敌人/毒性掉落
  Chaos      // 混沌 - 随机词缀
};

// JSON 序列化
inline void to_json(nlohmann::json &j, const FragmentElement &e) {
  j = static_cast<uint8_t>(e);
}
inline void from_json(const nlohmann::json &j, FragmentElement &e) {
  e = static_cast<FragmentElement>(j.get<uint8_t>());
}

// 获取元素名称 (用于 UI 显示)
inline const char *GetFragmentElementName(FragmentElement elem) {
  switch (elem) {
  case FragmentElement::None:
    return "None";
  case FragmentElement::Fire:
    return "Fire";
  case FragmentElement::Cold:
    return "Cold";
  case FragmentElement::Lightning:
    return "Lightning";
  case FragmentElement::Shadow:
    return "Shadow";
  case FragmentElement::Chaos:
    return "Chaos";
  default:
    return "Unknown";
  }
}

// 获取元素颜色 (用于 UI 渲染)
inline uint32_t GetFragmentElementColor(FragmentElement elem) {
  switch (elem) {
  case FragmentElement::Fire:
    return 0xFF4444FF; // 红色 (ABGR)
  case FragmentElement::Cold:
    return 0xFFFFAA44; // 蓝色
  case FragmentElement::Lightning:
    return 0xFF44FFFF; // 黄色
  case FragmentElement::Shadow:
    return 0xFF880088; // 紫色
  case FragmentElement::Chaos:
    return 0xFFAAAAAA; // 灰色
  default:
    return 0xFFFFFFFF; // 白色
  }
}

/**
 * @brief 地图碎片组件 - 附加在物品实体上使其成为地图碎片
 */
struct MapFragmentComponent {
  // 4字节成员
  float enemyDensityMod = 1.0f; // 敌人密度乘数 (1.0 = 100%)
  float dropRateMod = 1.0f;     // 掉落率乘数
  int monsterLevelMod = 0;      // 怪物等级调整 (+/- 等级)

  // 1字节成员 (枚举和布尔值)
  FragmentType type = FragmentType::Affix;
  FragmentElement element = FragmentElement::None;
  Rarity rarity = Rarity::Common;
  bool hasBoss = false;     // 是否包含 Boss
  bool hasMerchant = false; // 是否包含商人
  bool hasTreasure = false; // 是否包含宝库

  // 视觉标识与覆盖 (字符串放在最后，避免打断 POD 布局)
  std::string biomeOverride; // 覆盖生物群系 (如 "lava", "ice_cave")
  std::string iconId;        // 图标资源ID

  // 获取该碎片的简短描述 (用于 UI Tooltip)
  std::string GetDescription() const {
    std::string desc;
    if (enemyDensityMod != 1.0f) {
      desc += "Density: ";
      desc += std::to_string(static_cast<int>(enemyDensityMod * 100));
      desc += "%\n";
    }
    if (dropRateMod != 1.0f) {
      desc += "Drop Rate: ";
      desc += std::to_string(static_cast<int>(dropRateMod * 100));
      desc += "%\n";
    }
    if (monsterLevelMod != 0) {
      desc += "Monster Level: ";
      if (monsterLevelMod > 0)
        desc += "+";
      desc += std::to_string(monsterLevelMod);
      desc += "\n";
    }
    if (hasBoss)
      desc += "[Boss]\n";
    if (hasMerchant)
      desc += "[Merchant]\n";
    if (hasTreasure)
      desc += "[Treasure]\n";
    return desc;
  }
};

// JSON 序列化 (不包含 GetDescription 方法)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MapFragmentComponent, type, element,
                                   biomeOverride, enemyDensityMod, dropRateMod,
                                   monsterLevelMod, hasBoss, hasMerchant,
                                   hasTreasure, iconId, rarity)

// 标记实体为地图碎片的标签
struct MapFragmentTag {};

} // namespace NoMoreDay
