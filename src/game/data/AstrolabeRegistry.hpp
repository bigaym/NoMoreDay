#pragma once
#include "game/components/Progression.hpp"
#include <unordered_map>
#include <vector>


namespace NoMoreDay {

class AstrolabeRegistry {
public:
  static AstrolabeRegistry &Get() {
    static AstrolabeRegistry instance;
    return instance;
  }

  bool Load(const std::string &path);
  const AstrolabeNode *GetNode(uint32_t id) const;
  const std::unordered_map<uint32_t, AstrolabeNode> &GetAllNodes() const;
  void RegisterNode(const AstrolabeNode &node) { nodes[node.id] = node; }

private:
  AstrolabeRegistry() = default;
  std::unordered_map<uint32_t, AstrolabeNode> nodes;
};

} // namespace NoMoreDay
