#pragma once
#include <cstdint>
#include <type_traits>
#include <algorithm>
#include <bit>

namespace NoMoreDay::utils {

/**
 * @brief Branchless optimization utilities.
 *
 * These functions aim to replace conditional branches (if/else) with
 * bitwise operations and arithmetic to reduce branch misprediction penalties
 * in hot loops.
 */

// Convert bool to full-bit mask
// true  -> 0xFFFFFFFF (-1)
// false -> 0x00000000 (0)
[[nodiscard]] constexpr int32_t BoolToMask(bool condition) noexcept {
    return -static_cast<int32_t>(condition);
}

// Select integer based on condition (Branchless)
// Returns ifTrue when condition is true, ifFalse otherwise
[[nodiscard]] constexpr int32_t Select(bool condition, int32_t ifTrue, int32_t ifFalse) noexcept {
    const int32_t mask = BoolToMask(condition);
    return (ifTrue & mask) | (ifFalse & ~mask);
}

// Select float based on condition (Branchless)
// Returns ifTrue when condition is true, ifFalse otherwise
// Uses arithmetic selection: ifFalse + (ifTrue - ifFalse) * condition
[[nodiscard]] constexpr float SelectF(bool condition, float ifTrue, float ifFalse) noexcept {
    return ifFalse + (ifTrue - ifFalse) * static_cast<float>(condition);
}

// Multiplier factor based on condition (Branchless)
// Returns multiplier when condition is true, 1.0f otherwise
// Useful for: if (isCrit) damage *= critMult; -> damage *= MultFactor(isCrit, critMult);
[[nodiscard]] constexpr float MultFactor(bool condition, float multiplier) noexcept {
    return 1.0f + (multiplier - 1.0f) * static_cast<float>(condition);
}

// Add value based on condition (Branchless)
// Returns value when condition is true, 0.0f otherwise
// Useful for: if (bonus) stat += value; -> stat += AddFactor(bonus, value);
[[nodiscard]] constexpr float AddFactor(bool condition, float value) noexcept {
    return value * static_cast<float>(condition);
}

// Branchless clamp for float
// Returns std::clamp(v, lo, hi) but implemented to minimize branching if needed
// Note: modern compilers often optimize std::clamp well, but this is explicit
[[nodiscard]] constexpr float ClampF(float v, float lo, float hi) noexcept {
    const bool tooLow = v < lo;
    const bool tooHigh = v > hi;
    // v < lo ? lo : (v > hi ? hi : v)
    // = SelectF(tooHigh, hi, SelectF(tooLow, lo, v))
    float result = SelectF(tooLow, lo, v);
    return SelectF(tooHigh, hi, result);
}

} // namespace NoMoreDay::utils
