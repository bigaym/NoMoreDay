#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace NoMoreDay {

/**
 * @brief Enum identifiers for skill-system buffs/debuffs.
 *
 * Centralizes the string literals previously scattered across creation and
 * comparison sites. BuffEffect::id remains a std::string for serialization
 * compatibility; convert via BuffIdToString/BuffIdFromString at the boundary.
 *
 * NOTE: kBuffIdNames must stay character-for-character identical to the
 * original literals (e.g. "array_slow" / "array_armor_shred") so that saved
 * games and existing data keep working.
 */
enum class BuffId : uint8_t {
  SwordStep,                // "flowing_thrust_swift"
  BladeWard,                // "blade_ward"
  BladeBoomerangBleed,      // "blade_boomerang_bleed"
  BladeBoomerangGuardQi,    // "blade_boomerang_guard_qi"
  BloodSeaMiasma,           // "blood_sea_miasma"
  PhantomFlashShadowHide,   // "phantom_flash_shadow_hide"
  SwordArraySlow,           // "array_slow"
  SwordArrayArmorShred,     // "array_armor_shred"
  HeavenlySwordFieldResist, // "heavenly_sword_field_resist"
  HeavenlySwordMeteorCore,  // "heavenly_sword_meteor_core"
  FlowingThrustElementBody, // "flowing_thrust_element_body"
  SevenStarSwordStepMirage, // "seven_star_sword_step_mirage"
  LingJianHuTi,             // "ling_jian_hu_ti"
  SupportShield,            // "support_shield"
  AssassinBackstabBoost,    // "assassin_backstab_boost"
  Count,
};

inline constexpr std::array<std::string_view,
                            static_cast<std::size_t>(BuffId::Count)>
    kBuffIdNames = {
        "flowing_thrust_swift",      // SwordStep
        "blade_ward",                // BladeWard
        "blade_boomerang_bleed",     // BladeBoomerangBleed
        "blade_boomerang_guard_qi",  // BladeBoomerangGuardQi
        "blood_sea_miasma",          // BloodSeaMiasma
        "phantom_flash_shadow_hide", // PhantomFlashShadowHide
        "array_slow",                // SwordArraySlow
        "array_armor_shred",         // SwordArrayArmorShred
        "heavenly_sword_field_resist", // HeavenlySwordFieldResist
        "heavenly_sword_meteor_core",  // HeavenlySwordMeteorCore
        "flowing_thrust_element_body", // FlowingThrustElementBody
        "seven_star_sword_step_mirage", // SevenStarSwordStepMirage
        "ling_jian_hu_ti",             // LingJianHuTi
        "support_shield",              // SupportShield
        "assassin_backstab_boost",     // AssassinBackstabBoost
};

[[nodiscard]] constexpr std::string_view BuffIdToString(BuffId id) noexcept {
  const auto idx = static_cast<std::size_t>(id);
  return idx < kBuffIdNames.size() ? kBuffIdNames[idx] : std::string_view{};
}

[[nodiscard]] constexpr std::optional<BuffId> BuffIdFromString(
    std::string_view name) noexcept {
  for (std::size_t i = 0; i < kBuffIdNames.size(); ++i) {
    if (kBuffIdNames[i] == name) {
      return static_cast<BuffId>(i);
    }
  }
  return std::nullopt;
}

} // namespace NoMoreDay
