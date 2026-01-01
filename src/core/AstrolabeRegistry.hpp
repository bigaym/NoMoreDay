#pragma once
#include <vector>
#include <unordered_map>
#include "../components/Progression.hpp"

namespace NoMoreDay {

class AstrolabeRegistry {
public:
    static AstrolabeRegistry& Get() {
        static AstrolabeRegistry instance;
        return instance;
    }

    bool Load(const std::string& path);
    const AstrolabeNode* GetNode(uint32_t id) const;
    const std::unordered_map<uint32_t, AstrolabeNode>& GetAllNodes() const;

private:
    AstrolabeRegistry() = default;
    std::unordered_map<uint32_t, AstrolabeNode> nodes;
};

}
