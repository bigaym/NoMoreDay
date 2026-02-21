#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>

namespace NoMoreDay {

constexpr uint32_t kSkillContractSchemaVersion = 1;
constexpr uint32_t kSkillContractRuntimeVersion = 1;

enum class SpecNodeRole : uint8_t {
  Passive = 0,
  Keystone = 1,
  Trigger = 2,
  Synergy = 3,
  Transmuter = 4,
};

enum class ResistModel : uint8_t {
  None = 0,
  TypeA_Penetration = 1,
  TypeB_Shred = 2,
  TypeC_Exposure = 3,
  TypeD_StatToPenetration = 4,
  TypeE_CapSuppression = 5,
};

enum class ScopePolicy : uint8_t {
  SkillOnly = 0,
  GlobalWhileBuffActive = 1,
  GlobalAlways = 2,
};

struct TriggerContract {
  uint32_t trigger_skill_id = 0;
  float effectiveness = 0.0f;
  float internal_cooldown = 0.0f;
  bool consumes_mana = false;
};

struct NodeContractData {
  uint32_t node_id = 0;
  SpecNodeRole role = SpecNodeRole::Passive;
  ResistModel resist_model = ResistModel::None;
  ScopePolicy scope_policy = ScopePolicy::SkillOnly;
  bool affects_sword_intent = false;
  bool affects_sword_step = false;
  TriggerContract trigger{};
};

struct SkillContract {
  uint32_t skill_id = 0;
  uint8_t min_nodes = 24;
  uint8_t max_nodes = 26;
  uint8_t max_transmuters = 2;
  uint8_t max_triggers = 1;
  bool has_sword_intent_node = true;
  bool has_synergy_node = true;
  std::array<uint32_t, 2> transmuter_node_ids{};
};

struct SkillContractDefinition {
  SkillContract contract{};
  std::unordered_map<uint32_t, NodeContractData> nodes;
};

static_assert(sizeof(NodeContractData) <= 40,
              "NodeContractData must stay cache-friendly");
static_assert(sizeof(SkillContract) <= 32,
              "SkillContract must stay compact");

} // namespace NoMoreDay
