#pragma once

#include "engine/render/graph/RenderPass.hpp"
#include <memory>
#include <string>
#include <vector>

namespace NoMoreDay::render::graph {

struct ResourceAccess {
  enum class Type {
    Read,
    Write,
  };

  std::string resourceName;
  Type type = Type::Read;
};

class RenderGraphBuilder {
public:
  void Read(const std::string &resourceName);
  void Write(const std::string &resourceName);

  const std::vector<ResourceAccess> &GetAccesses() const { return m_accesses; }

private:
  std::vector<ResourceAccess> m_accesses;
};

struct RenderContext;

class RenderGraph {
public:
  void AddPass(std::shared_ptr<RenderPass> pass);
  void Clear();
  void Build();
  void Execute(RenderContext &context);

  size_t GetPassCount() const { return m_nodes.size(); }

private:
  struct Node {
    std::shared_ptr<RenderPass> pass;
    std::vector<ResourceAccess> accesses;
  };

  std::vector<Node> m_nodes;
  bool m_isBuilt = false;
};

} // namespace NoMoreDay::render::graph
