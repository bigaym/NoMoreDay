#pragma once
#include "game/foundation/data/TalentData.hpp"
#include "game/foundation/components/Common.hpp"

namespace NoMoreDay {

class TalentLayoutService {
public:
    // 计算所有节点的世界坐标 (基于扇区和轨道)
    static void computeNodePositions(TalentGraph& graph);
    
    // 获取职业扇区的中心角度 (度)
    // Based on UI spec layout:
    //      Berserker(5) [90]
    //          |
    // Ranger(4)  Knight(3) [150, 30]
    //          |
    // Priest(2)  Mage(1) [210, 330]
    //      Blade(0) [270]
    static float getSectorCenterAngle(ProfessionID profession);
    
    // 获取指定 Tier 的轨道半径
    static float getOrbitRadius(uint8_t tier);
    
private:
    // 计算扇区内节点的均匀分布角度
    static float computeNodeAngle(
        ProfessionID profession,
        uint8_t tier,
        uint8_t sectorIndex,
        uint8_t totalNodesInTier
    );
};

} // namespace NoMoreDay
