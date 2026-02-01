#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>
#include "game/data/TalentData.hpp"

namespace NoMoreDay {

class AstrolabeRegistry {
public:
  static AstrolabeRegistry &Get() {
    static AstrolabeRegistry instance;
    return instance;
  }

  bool Load(const std::string &path);
  const StarNode *GetNode(uint32_t id) const;
  const std::unordered_map<uint32_t, StarNode> &GetAllNodes() const;
  
  void RegisterNode(const StarNode &node);
  
  // Update from current map
  void SetMap(const AstrolabeMap& map);

private:
  AstrolabeRegistry() = default;
  std::unordered_map<uint32_t, StarNode> nodes;
};

} // namespace NoMoreDay
