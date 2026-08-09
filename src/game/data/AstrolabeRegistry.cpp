#include "game/data/AstrolabeRegistry.hpp"
#include "game/data/TalentLoader.hpp"
#include "game/data/AstrolabeConstants.hpp"
#include "game/components/Common.hpp"
#include "core/logging/Logger.hpp"

namespace NoMoreDay {

bool AstrolabeRegistry::Load() {
    return Load(Constants::Astrolabe::TALENT_DATA_PATH);
}

bool AstrolabeRegistry::Load(const std::string& path) {
    TalentGraph graph;
    if (TalentLoader::LoadProfessionTalents(path, graph)) {
        SetGraph(graph);
        return true;
    }
    return false;
}

void AstrolabeRegistry::SetGraph(const TalentGraph& g) {
    graph = g;
    LOG_INFO("AstrolabeRegistry: Synced with graph ({} nodes)", graph.nodes.size());
}

const AstrolabeTalentNode* AstrolabeRegistry::GetNode(uint32_t id) const {
    auto it = graph.nodes.find(id);
    if (it != graph.nodes.end()) {
        return &it->second;
    }
    return nullptr;
}

const std::unordered_map<uint32_t, AstrolabeTalentNode>& AstrolabeRegistry::GetAllNodes() const {
    return graph.nodes;
}

} // namespace NoMoreDay