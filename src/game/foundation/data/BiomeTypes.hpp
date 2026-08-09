#pragma once
#include <cstdint>

namespace NoMoreDay
{
  // 生物群系ID枚举
  enum class BiomeStyle : uint8_t
  {
    Town = 0,
    Open = 1,
    Maze = 2,
    Special = 3
  };

  enum class BiomeFeature : uint32_t
  {
    None = 0,
    AirWall = 1u << 0,
    LowGravity = 1u << 1,
    Destructible = 1u << 2,
    DynamicSpawner = 1u << 3,
    LimitedVision = 1u << 4,
    SpeedZone = 1u << 5,
    FrictionMod = 1u << 6,
    VisualFilter = 1u << 7
  };

  [[nodiscard]] constexpr uint32_t ToBiomeFeatureMask(BiomeFeature feature) noexcept
  {
    return static_cast<uint32_t>(feature);
  }

  [[nodiscard]] constexpr uint32_t AddBiomeFeature(uint32_t mask, BiomeFeature feature) noexcept
  {
    return mask | ToBiomeFeatureMask(feature);
  }

  [[nodiscard]] constexpr uint32_t RemoveBiomeFeature(uint32_t mask, BiomeFeature feature) noexcept
  {
    return mask & ~ToBiomeFeatureMask(feature);
  }

  [[nodiscard]] constexpr bool HasBiomeFeature(uint32_t mask, BiomeFeature feature) noexcept
  {
    return (mask & ToBiomeFeatureMask(feature)) != 0u;
  }

  enum class BiomeID : uint8_t
  {
    None = 0,

    // Town variants (T01-T06)
    Town = 1,
    Town_SwordImmortal = 2,
    Town_Mage = 3,
    Town_Mech = 4,
    Town_Shadow = 5,
    Town_Beast = 6,
    Town_Radiant = 7,

    // Open biomes (A group, C01-C07)
    Cave = 10,
    SunPrairie = 11,
    IceTundra = 12,
    CrimsonWaste = 13,
    DustSea = 14,
    VoidFlats = 15,
    EmeraldWet = 16,
    AshPlain = 17,

    // Maze biomes (B group, C08-C14)
    GloomSpire = 20,
    MagmaVeins = 21,
    JadeMine = 22,
    DrownedLib = 23,
    ClockCore = 24,
    AncientCrypt = 25,
    CrystalLab = 26,

    // Special biomes (C group, C15-C21)
    FloatingIsle = 30,
    CoralRuin = 31,
    WhisperWood = 32,
    HolyArena = 33,
    HiveNest = 34,
    SkyPalace = 35,
    AbyssalGap = 36,

    COUNT
  };
} // namespace NoMoreDay
