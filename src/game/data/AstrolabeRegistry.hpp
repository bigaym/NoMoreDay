#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>

namespace NoMoreDay {

// Forward declaration to avoid header circular dependency
struct AstrolabeNode;

class AstrolabeRegistry {
public:
  static AstrolabeRegistry &Get() {
    static AstrolabeRegistry instance;
    return instance;
  }

  bool Load(const std::string &path);
  const AstrolabeNode *GetNode(uint32_t id) const;
  const std::unordered_map<uint32_t, AstrolabeNode> &GetAllNodes() const;
  
  // Implemented in .cpp to allow using the full definition of AstrolabeNode
  void RegisterNode(const AstrolabeNode &node);

private:
  AstrolabeRegistry() = default;
  std::unordered_map<uint32_t, AstrolabeNode> nodes;
};

} // namespace NoMoreDay