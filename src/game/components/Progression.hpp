#pragma once
#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <set>
#include <unordered_map>
#include "game/components/Stats.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/data/TalentData.hpp"
#include <nlohmann/json.hpp>

namespace NoMoreDay {

    // Aliases for compatibility
    using AstrolabeNode = StarNode;
    using AstrolabeNodeType = StarNodeType;

    struct AstrolabeComponent {
        // --- 现有字段 (保留) ---
        std::set<uint32_t> activated_nodes;  // 已激活的节点 ID 集合
        int available_points = 0;            // 可分配的星尘点数
        
        // --- 六扇区系统新增字段 (V1.1) ---
        // 职业亲和度 (Profession Affinity)
        // 每个职业投入的点数总和
        std::array<int, 6> professionAffinity = {0, 0, 0, 0, 0, 0};
        
        // 主修职业 (誓约后锁定, -1 表示未选择)
        int mainProfession = -1;
        
        // 节点已投入点数 (支持多点节点 0/5)
        std::unordered_map<uint32_t, uint8_t> nodePoints;
        
        // --- 辅助方法 ---
        
        // 获取某职业的亲和度
        [[nodiscard]] int getAffinity(ProfessionID prof) const {
            return professionAffinity[static_cast<uint8_t>(prof)];
        }
        
        // 是否已誓约
        [[nodiscard]] bool hasVow() const { return mainProfession >= 0; }
        
        // 是否为主修职业
        [[nodiscard]] bool isMainProfession(ProfessionID prof) const {
            return mainProfession == static_cast<int>(prof);
        }
        
        // 获取节点当前点数
        [[nodiscard]] uint8_t getNodePoints(uint32_t nodeId) const {
            auto it = nodePoints.find(nodeId);
            return it != nodePoints.end() ? it->second : 0;
        }
    };

    // Serialize AstrolabeComponent (useful for saving)
    inline void to_json(nlohmann::json& j, const AstrolabeComponent& c) {
        j = nlohmann::json{
            {"activated_nodes", c.activated_nodes},
            {"available_points", c.available_points},
            {"profession_affinity", c.professionAffinity},
            {"main_profession", c.mainProfession},
            {"node_points", c.nodePoints}
        };
    }
    inline void from_json(const nlohmann::json& j, AstrolabeComponent& c) {
        j.at("activated_nodes").get_to(c.activated_nodes);
        j.at("available_points").get_to(c.available_points);
        // 兼容旧存档：新字段可能不存在
        if (j.contains("profession_affinity")) {
            j.at("profession_affinity").get_to(c.professionAffinity);
        }
        if (j.contains("main_profession")) {
            j.at("main_profession").get_to(c.mainProfession);
        }
        if (j.contains("node_points")) {
            j.at("node_points").get_to(c.nodePoints);
        }
    }

    // --- Class Traits ---
    
    // Sword Cultivator Trait
    struct SwordHeartComponent {};
    
    inline void to_json(nlohmann::json& j, const SwordHeartComponent& c) { j = nlohmann::json::object(); }
    inline void from_json(const nlohmann::json& j, SwordHeartComponent& c) { }

} // namespace NoMoreDay