#pragma once
#include <entt/entity/registry.hpp>
#include <vector>
#include <queue>
#include "raylib.h"

namespace NoMoreDay {

struct PendingDrop {
    entt::entity killer;
    entt::entity victim_snapshot; // We store data needed for drop instead of the entity itself
    Vector2 pos;
    uint32_t poolId;
    int tableMinRolls;
    int tableMaxRolls;
    float dropChance;
    int areaLevel;
};

class DropSystem {
public:
    /**
     * @brief 处理被击杀的实体并根据其 DropTableComponent 生成掉落物。
     */
    static void update(entt::registry& registry, int areaLevel = 1);

    /**
     * @brief 计算特定实体掉落物的核心逻辑。
     */
    static void GenerateDrops(entt::registry& registry, entt::entity killer, entt::entity victim, int areaLevel = 0);

private:
    static std::queue<PendingDrop> s_pendingDrops;
    static constexpr int MAX_DROPS_PER_FRAME = 8; // Process max 8 drop tables per frame to avoid lag
};

} // namespace NoMoreDay
