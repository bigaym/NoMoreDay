#pragma once

#include "game/systems/item/storage/ItemStorageTypes.hpp"

namespace NoMoreDay {

/**
 * @brief 附加到世界中掉落物品上的轻量级组件，引用 ItemStore。
 * 位于 systems 层（而非 foundation/components），因为它依赖于 ItemStorageTypes；
 * 在首个 gameplay 消费者引入前放置于此。
 */
struct GroundItemComponent {
  ItemHandle handle{0, 0};
  uint32_t baseId = 0;
};

} // namespace NoMoreDay
