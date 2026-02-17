#include "engine/render/core/BindingRegistry.hpp"

#include "engine/render/RenderConstants.hpp"

#include <array>
#include <cstddef>

namespace NoMoreDay::render::core {
namespace {

using NoMoreDay::RenderConstants::Binding;

constexpr std::array<BindingEntry, 11> kGlobalBindings = {{
    {"SSBO_ENTITY_DATA", static_cast<uint32_t>(Binding::SSBO_ENTITY_DATA)},
    {"SSBO_VISIBLE_ID", static_cast<uint32_t>(Binding::SSBO_VISIBLE_ID)},
    {"SSBO_COMMAND", static_cast<uint32_t>(Binding::SSBO_COMMAND)},
    {"SSBO_VISUAL_STATS", static_cast<uint32_t>(Binding::SSBO_VISUAL_STATS)},
    {"SSBO_LABEL_INSTANCE", static_cast<uint32_t>(Binding::SSBO_LABEL_INSTANCE)},
    {"SSBO_BEAM_INSTANCE", static_cast<uint32_t>(Binding::SSBO_BEAM_INSTANCE)},
    {"SSBO_SKILL_EFFECTS", static_cast<uint32_t>(Binding::SSBO_SKILL_EFFECTS)},
    {"SSBO_POPUP_DATA", static_cast<uint32_t>(Binding::SSBO_POPUP_DATA)},
    {"SSBO_GLYPH_INSTANCE", static_cast<uint32_t>(Binding::SSBO_GLYPH_INSTANCE)},
    {"SSBO_LIGHT_DATA", static_cast<uint32_t>(Binding::SSBO_LIGHT_DATA)},
    {"SSBO_HOLOBLADE_INSTANCE",
     static_cast<uint32_t>(Binding::SSBO_HOLOBLADE_INSTANCE)},
}};

// Pass-local domains are isolated; values may overlap across different domains.
constexpr std::array<BindingEntry, 3> kLightCullingBindings = {{
    {"LIGHT_LIST_IN", 0u},
    {"CLUSTER_HEADER_OUT", 1u},
    {"CLUSTER_INDEX_OUT", 2u},
}};

constexpr std::array<BindingEntry, 2> kShadowPrepareBindings = {{
    {"SHADOW_CASTER_IN", 0u},
    {"SHADOW_LIGHT_IN", 1u},
}};

constexpr std::array<BindingEntry, 3> kShadowBuildBindings = {{
    {"SHADOW_CASTER_IN", 0u},
    {"SHADOW_SDF_OUT", 1u},
    {"SHADOW_ATLAS_META", 2u},
}};

constexpr std::array<BindingEntry, 2> kShadowResolveBindings = {{
    {"SHADOW_SDF_IN", 0u},
    {"SHADOW_MASK_OUT", 1u},
}};

constexpr bool HasEntryConflicts(std::span<const BindingEntry> entries) {
  for (size_t i = 0; i < entries.size(); ++i) {
    for (size_t j = i + 1; j < entries.size(); ++j) {
      if (entries[i].binding == entries[j].binding) {
        return true;
      }
      if (entries[i].symbol == entries[j].symbol) {
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
         HasDomainConflicts(BindingDomain::ShadowResolve);
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

} // namespace NoMoreDay::render::core
