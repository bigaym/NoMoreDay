#pragma once

#include <memory>
#include <vector>

#include <entt/entt.hpp>
#include <raylib.h>

#include "engine/render/SIMDSpatialGrid.hpp"
#include "game/foundation/components/ItemComponent.hpp"

namespace NoMoreDay {

/**
 * @brief 跨层共享的 UI/渲染状态（设计 §5.3 环2 破环 sink）。
 *
 * 消除 item→render、render→ui、ui→render 的直接边：
 *  - 战利品空间网格 s_itemGrid / s_itemGridDirty（item 域写、render 域读）；
 *  - 可见战利品标签缓存 VisibleItemCache::visibleItems（render 域写、ui 域读）；
 *  - 被 render 适配器消费的 UI 主题/状态（ui 域写、render 域读）。
 *
 * 本目标只持静态状态与薄存取器，不含任何游戏逻辑；仅位于
 * NoMoreDayCore / NoMoreDayTypes / engine 底层头之上（设计 §5.4 中带）。
 */
class UiShared {
public:
  // --- 战利品空间网格：item 域写 dirty、render 域读查询 / rebuild ---
  static std::unique_ptr<systems::SIMDSpatialGrid> s_itemGrid;
  static bool s_itemGridDirty;

  // --- 可见战利品标签缓存：render 域写、ui 域读 ---
  struct VisibleItemCache {
    struct ItemData {
      entt::entity entity;
      Rectangle worldRect; // World Space Bounds for Label
    };
    static std::vector<ItemData> visibleItems;
    static void Clear() { visibleItems.clear(); }
  };

  // --- 被 render 适配器消费的 UI 主题/状态（ui 域写、render 域读）---
  // 世界悬停物品实体（UISystem/UI* 域写，GameplayRenderAdapter 读）。
  static entt::entity &HoveredItem();
  // 全局 UI 字体镜像：UISystem 持有规范的 State.globalFont，并在每个写点经
  // SetGlobalFont 同步到此处；render 适配器只读此处。
  static const Font &GlobalFont();
  static void SetGlobalFont(Font font);
  // 稀有度 -> 颜色查表（原 UIRenderer::GetRarityColor 实现迁入）。
  static Color GetRarityColor(Rarity rarity);

  // 生命周期：战利品空间网格的分配与释放（原 GameplayRenderAdapter::Init/Shutdown）。
  static void Init();
  static void Shutdown();

private:
  static Font s_globalFont;
  static entt::entity s_hoveredItem;
};

} // namespace NoMoreDay
