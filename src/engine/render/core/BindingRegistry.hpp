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
};

struct BindingEntry {
  std::string_view symbol;
  uint32_t binding = 0;
};

class BindingRegistry final {
public:
  static std::span<const BindingEntry> GetDomainBindings(BindingDomain domain);
  static bool HasDomainConflicts(BindingDomain domain);
  static bool HasAnyConflicts();
  static bool TryResolve(BindingDomain domain, std::string_view symbol,
                         uint32_t &outBinding);

  // Runtime helper for tests and dynamic validation hooks.
  static bool HasConflicts(std::span<const uint32_t> bindings);
};

} // namespace NoMoreDay::render::core
