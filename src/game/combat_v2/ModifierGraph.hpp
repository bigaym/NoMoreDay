#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace NoMoreDay::CombatV2 {

enum class ModifierStage : uint8_t {
    PreHit = 0,
    Hit = 1,
    PostHit = 2,
    DotTick = 3,
};

enum class ModifierOp : uint8_t {
    Flat = 0,
    Increased = 1,
    More = 2,
    Convert = 3,
    GainExtra = 4,
    ClampMin = 5,
    ClampMax = 6,
};

struct ModifierNode {
    uint32_t nodeId{0};
    ModifierStage stage{ModifierStage::PreHit};
    ModifierOp op{ModifierOp::Flat};
    float value{0.0f};
    uint32_t conditionProgramId{0};
    uint16_t priority{0};
    uint32_t sourceId{0};
};

struct CompiledModifierNode {
    ModifierNode node{};
    std::vector<uint32_t> forbiddenFilterIds{};
    std::vector<uint32_t> nodeWhitelist{};
};

struct ModifierGraph {
    static constexpr size_t kStageCount = 4;
    std::array<std::vector<CompiledModifierNode>, kStageCount> stageBuckets{};
};

struct ModifierGraphBuildRequest {
    const std::vector<CompiledModifierNode> *candidates{nullptr};
    const std::vector<uint32_t> *activeForbiddenFilters{nullptr};
};

enum class ModifierGraphCompileStatus : uint8_t {
    Ok = 0,
    InvalidInput = 1,
    NotImplemented = 2,
};

struct ModifierGraphCompileResult {
    ModifierGraphCompileStatus status{ModifierGraphCompileStatus::NotImplemented};
    ModifierGraph graph{};
};

class ModifierGraphCompiler {
  public:
    [[nodiscard]] ModifierGraphCompileResult Compile(const ModifierGraphBuildRequest &request) const;
};

} // namespace NoMoreDay::CombatV2
