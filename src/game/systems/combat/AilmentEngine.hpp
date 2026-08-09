#pragma once

#include "game/components/Buff.hpp"
#include "game/contracts/CombatEvents.hpp"
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NoMoreDay::systems {

struct AilmentTypeHash {
  size_t operator()(AilmentType value) const noexcept {
    return static_cast<size_t>(value);
  }
};

enum class RefreshPolicy : uint8_t {
  Refresh = 0,
  Extend,
  Independent
};

enum class OverwritePolicy : uint8_t {
  Strongest = 0,
  Newest,
  Additive
};

enum class DamagePoolPolicy : uint8_t {
  PerStack = 0,
  Consolidated
};

struct AilmentContract {
  AilmentType ailment = AilmentType::None;
  uint8_t max_stacks = 1;
  RefreshPolicy refresh_policy = RefreshPolicy::Refresh;
  OverwritePolicy overwrite_policy = OverwritePolicy::Strongest;
  float immunity_and_resistance = 1.0f;
  float tick_interval = 1.0f;
  DamagePoolPolicy damage_pool_policy = DamagePoolPolicy::PerStack;
  float base_duration = 1.0f;
  Tag damage_tag = Tag::Poison;
  BuffType legacy_buff_type = BuffType::DamageOverTime;
};

struct AilmentApplyRequest {
  AilmentType ailment = AilmentType::None;
  entt::entity source = entt::null;
  float magnitude = 0.0f;
  float duration = 0.0f;
  uint8_t stacks = 1;
};

class AilmentRegistry {
public:
  static AilmentRegistry &Get();

  [[nodiscard]] const AilmentContract *Find(AilmentType ailment) const;
  [[nodiscard]] bool EnsureLoaded();
  [[nodiscard]] bool
  LoadFromFile(const std::string &path = "assets/data/ailment_contracts.json");

  void ResetForTests();
  [[nodiscard]] size_t Size() const noexcept { return m_contracts.size(); }

private:
  void LoadBuiltins();

  std::unordered_map<AilmentType, AilmentContract, AilmentTypeHash> m_contracts;
  bool m_loaded = false;
};

class AilmentAdapter {
public:
  [[nodiscard]] static std::optional<AilmentType>
  TryMapLegacyBuff(const BuffEffect &effect);
  [[nodiscard]] static BuffType ToLegacyBuffType(AilmentType ailment);
  [[nodiscard]] static Tag ResolveDamageTag(AilmentType ailment,
                                            const BuffEffect &effect);

  [[nodiscard]] static std::string BuildRuntimeId(AilmentType ailment,
                                                  uint64_t instance = 0);
  [[nodiscard]] static bool IsManagedAilmentId(std::string_view id,
                                               AilmentType *parsed = nullptr);
};

class AilmentApplier {
public:
  [[nodiscard]] static bool Apply(entt::registry &registry, entt::entity target,
                                  const AilmentApplyRequest &request);
};

class AilmentTickDriver {
public:
  static void Tick(entt::registry &registry, float dt);
};

} // namespace NoMoreDay::systems
