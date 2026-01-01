#include "AstrolabeRegistry.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include "../tools/Logger.hpp"

namespace NoMoreDay {

bool AstrolabeRegistry::Load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("AstrolabeRegistry: Failed to open data file: {}", path);
        return false;
    }

    try {
        nlohmann::json data;
        file >> data;

        nodes.clear();
        if (data.contains("nodes") && data["nodes"].is_array()) {
            for (const auto& node_json : data["nodes"]) {
                AstrolabeNode node = node_json.get<AstrolabeNode>();
                nodes[node.id] = std::move(node);
            }
        }

        LOG_INFO("AstrolabeRegistry: Loaded {} nodes from {}", nodes.size(), path);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("AstrolabeRegistry: Exception while loading data: {}", e.what());
        return false;
    }
}

const AstrolabeNode* AstrolabeRegistry::GetNode(uint32_t id) const {
    auto it = nodes.find(id);
    if (it != nodes.end()) {
        return &it->second;
    }
    return nullptr;
}

const std::unordered_map<uint32_t, AstrolabeNode>& AstrolabeRegistry::GetAllNodes() const {
    return nodes;
}

}
