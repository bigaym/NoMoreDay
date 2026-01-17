#pragma once
#include <cstdint>
#include <string_view>

namespace NoMoreDay::utils {

/**
 * @brief Compile-time FNV-1a Hash (32-bit).
 */
constexpr uint32_t Hash(std::string_view str) {
  uint32_t hash = 2166136261u;
  for (char c : str) {
    hash ^= static_cast<uint32_t>(c);
    hash *= 16777619u;
  }
  return hash;
}

/**
 * @brief User-defined literal for compile-time hashing: "string"_hash
 */
} // namespace NoMoreDay::utils

/**
 * @brief Global user-defined literal for hashing.
 */
constexpr uint32_t operator""_hash(const char *str, size_t len) {
  return NoMoreDay::utils::Hash(std::string_view(str, len));
}
