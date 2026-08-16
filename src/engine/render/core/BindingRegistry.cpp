#include "engine/render/core/BindingRegistry.hpp"

#include "engine/render/RenderConstants.hpp"

#include <array>
#include <cstddef>

namespace NoMoreDay::render::core {
namespace {

using NoMoreDay::RenderConstants::Binding;

constexpr std::array<BindingEntry, 16> kGlobalBindings = {{
    {"SSBO_ENTITY_DATA", static_cast<uint32_t>(Binding::SSBO_ENTITY_DATA), false},
    {"SSBO_VISIBLE_ID", static_cast<uint32_t>(Binding::SSBO_VISIBLE_ID), false},
    {"SSBO_COMMAND", static_cast<uint32_t>(Binding::SSBO_COMMAND), false},
    {"SSBO_VISUAL_STATS", static_cast<uint32_t>(Binding::SSBO_VISUAL_STATS), false},
    {"SSBO_LABEL_INSTANCE", static_cast<uint32_t>(Binding::SSBO_LABEL_INSTANCE), false},
    {"SSBO_BEAM_INSTANCE", static_cast<uint32_t>(Binding::SSBO_BEAM_INSTANCE), false},
    {"SSBO_SKILL_EFFECTS", static_cast<uint32_t>(Binding::SSBO_SKILL_EFFECTS), false},
    {"SSBO_POPUP_DATA", static_cast<uint32_t>(Binding::SSBO_POPUP_DATA), false},
    {"SSBO_GLYPH_INSTANCE", static_cast<uint32_t>(Binding::SSBO_GLYPH_INSTANCE), false},
    {"SSBO_LIGHT_DATA", static_cast<uint32_t>(Binding::SSBO_LIGHT_DATA), false},
    {"SSBO_TRAIL_HEADERS", static_cast<uint32_t>(Binding::SSBO_TRAIL_HEADERS), false},
    {"SSBO_TRAIL_POINTS", static_cast<uint32_t>(Binding::SSBO_TRAIL_POINTS), false},
    {"SSBO_MATERIAL_DATA", static_cast<uint32_t>(Binding::SSBO_MATERIAL_DATA), false},
    {"SSBO_DISTORTION_DATA", static_cast<uint32_t>(Binding::SSBO_DISTORTION_DATA), false},
    {"SSBO_HOLOBLADE_INSTANCE",
     static_cast<uint32_t>(Binding::SSBO_HOLOBLADE_INSTANCE), false},
    {"SSBO_LOOT_INSTANCE", static_cast<uint32_t>(Binding::SSBO_LOOT_INSTANCE), false},
}};

// Pass-local domains are isolated; values may overlap across different domains.
constexpr std::array<BindingEntry, 6> kLightCullingBindings = {{
    {"LIGHT_LIST_IN", 0u, false},
    {"CLUSTER_HEADER_OUT", 1u, false},
    {"CLUSTER_INDEX_OUT", 2u, false},
    {"LIGHT_BOUNDS_IN", 3u, false},
    {"CLUSTER_COUNTER", 4u, false},
    {"CLUSTER_LIGHT_OUT", 5u, false},
}};

constexpr std::array<BindingEntry, 2> kShadowPrepareBindings = {{
    {"SHADOW_CASTER_IN", 0u, false},
    {"SHADOW_LIGHT_IN", 1u, false},
}};

constexpr std::array<BindingEntry, 5> kShadowBuildBindings = {{
    {"SHADOW_CASTER_IN", 0u, false},
    {"phase_local_ssbo", 0u, true},
    {"SHADOW_OCCLUDER_SSBO", 0u, true},
    {"SHADOW_SDF_OUT", 1u, false},
    {"SHADOW_ATLAS_META", 2u, false},
}};

constexpr std::array<BindingEntry, 2> kShadowResolveBindings = {{
    {"SHADOW_SDF_IN", 0u, false},
    {"SHADOW_MASK_OUT", 1u, false},
}};

constexpr std::array<BindingEntry, 3> kTextIndirectArgsBindings = {{
    {"phase_local_ssbo", 0u, true},
    {"COUNTER_IN", 0u, false},
    {"COMMAND_OUT", 1u, false},
}};

constexpr bool HasEntryConflicts(std::span<const BindingEntry> entries) {
  for (size_t i = 0; i < entries.size(); ++i) {
    for (size_t j = i + 1; j < entries.size(); ++j) {
      if (entries[i].symbol == entries[j].symbol) {
        return true;
      }
      // Non-alias entries cannot share the same binding point within a single domain.
      if (!entries[i].isAlias && !entries[j].isAlias &&
          entries[i].binding == entries[j].binding) {
        return true;
      }
    }
  }
  return false;
}

static_assert(!HasEntryConflicts(kGlobalBindings),
              "BindingRegistry global domain has conflicts");
static_assert(!HasEntryConflicts(kLightCullingBindings),
              "BindingRegistry light-culling domain has conflicts");
static_assert(!HasEntryConflicts(kShadowPrepareBindings),
              "BindingRegistry shadow-prepare domain has conflicts");
static_assert(!HasEntryConflicts(kShadowBuildBindings),
              "BindingRegistry shadow-build domain has conflicts");
static_assert(!HasEntryConflicts(kShadowResolveBindings),
              "BindingRegistry shadow-resolve domain has conflicts");
static_assert(!HasEntryConflicts(kTextIndirectArgsBindings),
              "BindingRegistry text-indirect-args domain has conflicts");

} // namespace

std::span<const BindingEntry>
BindingRegistry::GetDomainBindings(BindingDomain domain) {
  switch (domain) {
  case BindingDomain::Global:
    return kGlobalBindings;
  case BindingDomain::LightCulling:
    return kLightCullingBindings;
  case BindingDomain::ShadowPrepare:
    return kShadowPrepareBindings;
  case BindingDomain::ShadowBuild:
    return kShadowBuildBindings;
  case BindingDomain::ShadowResolve:
    return kShadowResolveBindings;
  case BindingDomain::TextIndirectArgs:
    return kTextIndirectArgsBindings;
  }
  return {};
}

bool BindingRegistry::HasConflicts(std::span<const uint32_t> bindings) {
  for (size_t i = 0; i < bindings.size(); ++i) {
    for (size_t j = i + 1; j < bindings.size(); ++j) {
      if (bindings[i] == bindings[j]) {
        return true;
      }
    }
  }
  return false;
}

bool BindingRegistry::HasDomainConflicts(BindingDomain domain) {
  const auto entries = GetDomainBindings(domain);
  return HasEntryConflicts(entries);
}

bool BindingRegistry::HasAnyConflicts() {
  return HasDomainConflicts(BindingDomain::Global) ||
         HasDomainConflicts(BindingDomain::LightCulling) ||
         HasDomainConflicts(BindingDomain::ShadowPrepare) ||
         HasDomainConflicts(BindingDomain::ShadowBuild) ||
         HasDomainConflicts(BindingDomain::ShadowResolve) ||
         HasDomainConflicts(BindingDomain::TextIndirectArgs);
}

bool BindingRegistry::TryResolve(BindingDomain domain, std::string_view symbol,
                                 uint32_t &outBinding) {
  const auto entries = GetDomainBindings(domain);
  for (const BindingEntry &entry : entries) {
    if (entry.symbol == symbol) {
      outBinding = entry.binding;
      return true;
    }
  }
  return false;
}

bool BindingRegistry::IsPhaseLocalSSBO(BindingDomain domain,
                                      std::string_view symbol) {
  if (!IsPhaseLocalDomain(domain)) {
    return false;
  }
  uint32_t ignored = 0;
  return TryResolve(domain, symbol, ignored);
}

bool BindingRegistry::IsAlias(BindingDomain domain, std::string_view symbol) {
  const auto entries = GetDomainBindings(domain);
  for (const BindingEntry &entry : entries) {
    if (entry.symbol == symbol) {
      return entry.isAlias;
    }
  }
  return false;
}

} // namespace NoMoreDay::render::core
