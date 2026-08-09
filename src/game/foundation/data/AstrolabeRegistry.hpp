#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>
#include "game/foundation/data/TalentData.hpp"

namespace NoMoreDay {

class AstrolabeRegistry {
public:
  static AstrolabeRegistry &Get() {
    static AstrolabeRegistry instance;
    return instance;
  }

  bool Load(const std::string &path);
  bool Load(); // Uses default path defined in Common.hpp
  
  // Update from new graph
  void SetGraph(const TalentGraph& graph);

  const AstrolabeTalentNode *GetNode(uint32_t id) const;
  const std::unordered_map<uint32_t, AstrolabeTalentNode> &GetAllNodes() const;
  
  const TalentGraph& GetGraph() const { return graph; }

private:
  AstrolabeRegistry() = default;
  TalentGraph graph;
};

} // namespace NoMoreDay
