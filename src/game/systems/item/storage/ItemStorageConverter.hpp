#pragma once

#include "game/foundation/components/ItemComponent.hpp"
#include "game/systems/item/storage/ItemStorageTypes.hpp"

namespace NoMoreDay {

/**
 * @brief Converts runtime ItemComponent into fixed-size POD ItemInstance and optional side-table data.
 * @param comp Source ItemComponent.
 * @param outInst Target POD ItemInstance to populate.
 * @param outSide Optional pointer to ItemSideTableData to populate sparse conversions / damage modifiers.
 * @return true if conversion succeeded.
 */
bool ItemComponentToInstance(const ItemComponent &comp, ItemInstance &outInst,
                             ItemSideTableData *outSide = nullptr);

/**
 * @brief Value-returning overload of ItemComponentToInstance.
 */
ItemInstance ItemComponentToInstance(const ItemComponent &comp,
                                     ItemSideTableData *outSide = nullptr);

/**
 * @brief Converts POD ItemInstance (with optional side-table data) back to full ItemComponent.
 * Static attributes (name, description, slot, type, maxStack, etc.) are resolved via ItemTemplateRegistry.
 * @param inst Source POD ItemInstance.
 * @param outComp Target ItemComponent to populate.
 * @param side Optional sparse side-table data.
 * @return true if conversion succeeded.
 */
bool InstanceToItemComponent(const ItemInstance &inst, ItemComponent &outComp,
                             const ItemSideTableData *side = nullptr);

/**
 * @brief Value-returning overload of InstanceToItemComponent.
 */
ItemComponent InstanceToItemComponent(const ItemInstance &inst,
                                      const ItemSideTableData *side = nullptr);

} // namespace NoMoreDay
