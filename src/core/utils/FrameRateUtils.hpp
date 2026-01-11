#pragma once
#include <random>
#include <cstdlib>

namespace NoMoreDay::utils {

/// Helper utilities for frame-rate independent logic
/// Reference: 60 FPS baseline for all probability calculations
struct FrameRateUtils {
    static constexpr float REFERENCE_FPS = 60.0f;
    static constexpr float REFERENCE_DT = 1.0f / REFERENCE_FPS;
    
    /// Convert per-frame probability to time-adjusted probability
    /// @param baseChancePercent Original chance at 60 FPS (0-100)
    /// @param dt Current frame delta time
    /// @return Adjusted chance (0-100) that maintains same per-second rate
    /// 
    /// At 60 FPS (dt ≈ 0.0167): returns baseChance
    /// At 120 FPS (dt ≈ 0.0083): returns ~half chance
    /// At 180 FPS (dt ≈ 0.0056): returns ~1/3 chance
    [[nodiscard]] static constexpr float AdjustedChance(float baseChancePercent, float dt) noexcept {
        return baseChancePercent * (dt / REFERENCE_DT);
    }
    
    /// Check if random event should trigger (time-corrected)
    /// @param baseChancePercent Original chance at 60 FPS (0-100)
    /// @param dt Current frame delta time
    /// @return true if event should trigger this frame
    [[nodiscard]] static bool ShouldTrigger(float baseChancePercent, float dt) noexcept {
        float adjusted = AdjustedChance(baseChancePercent, dt);
        return static_cast<float>(std::rand() % 100) < adjusted;
    }
    
    /// Calculate time-adjusted interval for throttled updates
    /// @param baseFrameInterval Number of frames to skip at 60 FPS
    /// @return Time interval in seconds
    [[nodiscard]] static constexpr float FramesToSeconds(int baseFrameInterval) noexcept {
        return static_cast<float>(baseFrameInterval) * REFERENCE_DT;
    }
};

} // namespace NoMoreDay::utils
