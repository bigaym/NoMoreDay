#pragma once

#include "game/foundation/components/ItemComponent.hpp"
#include "game/systems/item/storage/ItemStorageTypes.hpp"
#include <unordered_map>

namespace NoMoreDay {

/**
 * @brief 将运行时 ItemComponent 转换为固定大小的 POD ItemInstance 及可选的旁表数据。
 *
 * 插槽所有权契约: ItemComponent.sockets 存储场景实体，而 ItemInstance.sockets 存储池化 ItemHandle；
 * 该纯函数本身无法在两者之间转换。保留镶嵌子项的调用方必须传递 socketMap
 * (场景实体 -> 池化 ItemHandle，例如在迁移遍历父项及镶嵌子项时构建)。
 * 若未提供 map，则所有插槽退化为显式空句柄 (ItemHandle{0,0})，且在存在存活实体时记录警告；
 * 调用方负责后续回填。
 *
 * @param comp 源 ItemComponent。
 * @param outInst 待填充的目标 POD ItemInstance。
 * @param outSide 用于填充稀疏 conversions / damage_modifiers 的可选 ItemSideTableData 指针。
 * @param socketMap 可选的插槽场景实体 -> ItemHandle 解析映射。
 * @return 转换成功则返回 true。
 */
bool ItemComponentToInstance(
    const ItemComponent &comp, ItemInstance &outInst,
    ItemSideTableData *outSide = nullptr,
    const std::unordered_map<entt::entity, ItemHandle> *socketMap = nullptr);

/**
 * @brief ItemComponentToInstance 的返回值重载版本。
 */
ItemInstance ItemComponentToInstance(
    const ItemComponent &comp, ItemSideTableData *outSide = nullptr,
    const std::unordered_map<entt::entity, ItemHandle> *socketMap = nullptr);

/**
 * @brief 将 POD ItemInstance (及可选旁表数据) 转换回完整的 ItemComponent。
 * 静态属性 (名称、描述、槽位、类型、最大堆叠等) 通过 ItemTemplateRegistry 解析。
 *
 * 插槽所有权契约与 ItemComponentToInstance 对称: 槽位根据 socketCount 预分配 entt::null 占位符。
 * 需要活动场景实体的调用方必须传递 socketEntityMap (池化 ItemHandle -> 场景实体)；
 * 未解析或空的句柄保持为 entt::null 以供后续回填。
 *
 * @param inst 源 POD ItemInstance。
 * @param outComp 待填充的目标 ItemComponent。
 * @param side 可选的稀疏旁表数据。
 * @param socketEntityMap 可选的插槽 ItemHandle -> 场景实体解析映射。
 * @return 转换成功则返回 true。
 */
bool InstanceToItemComponent(
    const ItemInstance &inst, ItemComponent &outComp,
    const ItemSideTableData *side = nullptr,
    const std::unordered_map<ItemHandle, entt::entity> *socketEntityMap = nullptr);

/**
 * @brief InstanceToItemComponent 的返回值重载版本。
 */
ItemComponent InstanceToItemComponent(
    const ItemInstance &inst, const ItemSideTableData *side = nullptr,
    const std::unordered_map<ItemHandle, entt::entity> *socketEntityMap = nullptr);

} // namespace NoMoreDay
