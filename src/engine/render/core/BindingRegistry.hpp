#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace NoMoreDay::render::core {

enum class BindingDomain : uint8_t {
  Global = 0,
  LightCulling = 1,
  ShadowPrepare = 2,
  ShadowBuild = 3,
  ShadowResolve = 4,
  TextIndirectArgs = 5,
};

struct BindingEntry {
  std::string_view symbol;
  uint32_t binding = 0;
  bool isAlias = false;
};

class BindingRegistry final {
public:
  static std::span<const BindingEntry> GetDomainBindings(BindingDomain domain);
  static bool HasDomainConflicts(BindingDomain domain);
  static bool HasAnyConflicts();
  static bool TryResolve(BindingDomain domain, std::string_view symbol,
                         uint32_t &outBinding);

  /// Returns true if the domain is a phase-local compute domain (not the global MDI domain).
  static constexpr bool IsPhaseLocalDomain(BindingDomain domain) {
    return domain != BindingDomain::Global;
  }

  /// Returns true if the symbol in the given domain represents a phase-local SSBO binding/alias.
  static bool IsPhaseLocalSSBO(BindingDomain domain, std::string_view symbol);

  /// Returns true if the symbol is registered as an alias in the domain.
  static bool IsAlias(BindingDomain domain, std::string_view symbol);

  // Runtime helper for tests and dynamic validation hooks.
  static bool HasConflicts(std::span<const uint32_t> bindings);
};

} // namespace NoMoreDay::render::core
