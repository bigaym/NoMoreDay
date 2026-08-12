#pragma once
#include <entt/entt.hpp>
#include <memory>

namespace NoMoreDay::systems {

class SIMDSpatialGrid;

// 战利品空间网格生命周期与脏标记（U8 收尾：从 UiShared 迁入 item 域）。
// 网格本体归本系统持有：item 域（InventorySystem/DropSystem/FragmentDropSystem）
// 写脏后经 MarkDirty 标记，本系统的 update 重建，render 适配器经 GetGrid 读查询。
class LootGridSystem {
public:
    static void update(entt::registry& registry);

    // 网格分配/释放（原 UiShared::Init/Shutdown，GameplayRenderAdapter 生命周期委托）。
    static void Init();
    static void Shutdown();
    // item 域写点调用（原 UiShared::s_itemGridDirty = true）。
    static void MarkDirty() noexcept { s_dirty = true; }
    // render 域读查询（null 表示尚未初始化，调用方自行判空）。
    static SIMDSpatialGrid* GetGrid() noexcept { return s_grid.get(); }

private:
    static std::unique_ptr<SIMDSpatialGrid> s_grid;
    static bool s_dirty;
};

} // namespace NoMoreDay::systems
