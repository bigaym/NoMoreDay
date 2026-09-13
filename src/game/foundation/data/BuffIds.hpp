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
  // 技能9 绝影绝剑形态增益（M-B 枚举化，替代旧 phantom_flash_shadow_hide）
  PhantomTranceForm,        // "phantom_trance_form"（移速/闪避/CDR/减伤/攻速合集）
  PhantomTranceDeathSeal,   // "phantom_trance_death_seal"（981 逆脉增伤）
  PhantomTranceLastStand,   // "phantom_trance_last_stand"（982 孤注一掷动态暴伤）
  PhantomTranceEnchant,     // "phantom_trance_enchant"（991 附魔元素穿透）
  PhantomTranceWeaken,      // "phantom_trance_weaken"（913 灵流穿透诅咒）
  PhantomTranceStealth,     // "phantom_trance_stealth"（980 虚灵之躯潜行）
  SwordArraySlow,           // "array_slow"
  SwordArrayArmorShred,     // "array_armor_shred"
  // 技能6 剑阵专属增益/减益（M-B 枚举化：原实现以字符串字面量直连，
  // 名称保持与历史字面量逐字符一致以兼容存档与既有断言）
  SwordArrayWeaken,         // "SwordArrayWeaken"（632 虚弱领域）
  SwordArrayConnectionSlow, // "SwordArrayConnectionSlow"（613 千丝万缕连线减速）
  SwordArrayCore,           // "SwordArrayCore"（650 阵眼全局增伤）
  SwordArrayCDR,            // "SwordArrayCDR"（654 剑神领域 CDR）
  SwordArrayCorrosion,      // "SwordArrayCorrosion"（674 法阵侵蚀降抗）
  HeavenlySwordFieldResist, // "heavenly_sword_field_resist"
  HeavenlySwordMeteorCore,  // "heavenly_sword_meteor_core"
  FlowingThrustElementBody, // "flowing_thrust_element_body"
  SevenStarSwordStepMirage, // "seven_star_sword_step_mirage"
  LingJianHuTi,             // "ling_jian_hu_ti"
  SupportShield,            // "support_shield"
  AssassinBackstabBoost,    // "assassin_backstab_boost"
  // 技能8 御剑·回旋接刃系增益（833/834/835）
  BladeBoomerangCombo,      // "blade_boomerang_combo"（833 连环劲：接刃后短时攻速）
  BladeBoomerangFreeCast,   // "blade_boomerang_free_cast"（834 御剑接踵：下次投掷免蓝）
  BladeBoomerangSwift,      // "blade_boomerang_swift"（835 回旋游步：接刃后移速）
  // 技能3 灵剑蚀甲（374）按元素区分的降抗减益，名称保留历史字面量以兼容
  // 既有存档与断言。
  SpiritCorrosionFire,      // "SpiritCorrosion_Fire"（374 蚀甲·火）
  SpiritCorrosionLightning, // "SpiritCorrosion_Lightning"（374 蚀甲·雷）
  // 护甲击碎（技能1 152 / 技能2 235 / 技能3 334 / 技能8 813）：跨技能共享的
  // 同一减益，历史上以裸字符串 ArmorShred 直连；名称逐字符保留以兼容存档。
  ArmorShred,               // ArmorShred（每层 -10 护甲 Flat，4s）
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
        "phantom_trance_form",       // PhantomTranceForm
        "phantom_trance_death_seal", // PhantomTranceDeathSeal
        "phantom_trance_last_stand", // PhantomTranceLastStand
        "phantom_trance_enchant",    // PhantomTranceEnchant
        "phantom_trance_weaken",     // PhantomTranceWeaken
        "phantom_trance_stealth",    // PhantomTranceStealth
        "array_slow",                // SwordArraySlow
        "array_armor_shred",         // SwordArrayArmorShred
        "SwordArrayWeaken",          // SwordArrayWeaken（632）
        "SwordArrayConnectionSlow",  // SwordArrayConnectionSlow（613）
        "SwordArrayCore",            // SwordArrayCore（650）
        "SwordArrayCDR",             // SwordArrayCDR（654）
        "SwordArrayCorrosion",       // SwordArrayCorrosion（674）
        "heavenly_sword_field_resist", // HeavenlySwordFieldResist
        "heavenly_sword_meteor_core",  // HeavenlySwordMeteorCore
        "flowing_thrust_element_body", // FlowingThrustElementBody
        "seven_star_sword_step_mirage", // SevenStarSwordStepMirage
        "ling_jian_hu_ti",             // LingJianHuTi
        "support_shield",              // SupportShield
        "assassin_backstab_boost",     // AssassinBackstabBoost
        "blade_boomerang_combo",       // BladeBoomerangCombo（833）
        "blade_boomerang_free_cast",   // BladeBoomerangFreeCast（834）
        "blade_boomerang_swift",       // BladeBoomerangSwift（835）
        "SpiritCorrosion_Fire",        // SpiritCorrosionFire（374）
        "SpiritCorrosion_Lightning",   // SpiritCorrosionLightning（374）
        // 保留完整字面量以逐字符兼容既有存档中的 "ArmorShred"。
        "ArmorShred",                  // ArmorShred（护甲击碎，4 技能共享）
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
