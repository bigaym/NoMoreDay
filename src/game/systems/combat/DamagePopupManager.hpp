#pragma once
#include <vector>
#include <string>
#include <atomic>
#include <cmath>
#include "raylib.h"

// Include OUTSIDE of namespace to avoid nesting issues
#include "engine/render/PopupRenderer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUTextSystem.hpp"
#include "engine/render/core/QualityTierManager.hpp"

namespace NoMoreDay {

class DamagePopupManager {
public:
    static DamagePopupManager& Get() {
        static DamagePopupManager instance;
        return instance;
    }

    void Init() {}

    void Emit(Vector2 pos, float amount, bool isCrit, Color color, bool isStatus = false, const std::string& statusText = "") {
        const auto &qualityManager = render::core::QualityTierManager::Get();
        const bool useGpuText =
            qualityManager.IsInitialized() && qualityManager.GetConfig().gpuTextEnabled;
        const bool useAdvancedAnim =
            qualityManager.IsInitialized() &&
            qualityManager.GetConfig().gpuTextAdvancedAnimation;

        if (!useGpuText) {
            if (isStatus) {
                render::PopupRenderer::Get().EmitStatus(pos, statusText.c_str(), color);
            } else {
                render::PopupRenderer::Get().Emit(pos, (int)amount, isCrit, color);
            }
            return;
        }

        // V4 GPU text path. String table mapping is provisional and will be
        // replaced by full UTF-8 layout path.
        components::GPUTextCommand cmd;
        cmd.worldPosX = pos.x;
        cmd.worldPosY = pos.y;
        if (isStatus) {
            cmd.stringId = render::GPUTextSystem::StringIdStatusGeneric;
        } else if (isCrit) {
            cmd.stringId = render::GPUTextSystem::StringIdCrit;
        } else {
            cmd.stringId = static_cast<uint32_t>(std::abs(static_cast<int>(amount)) % 10);
        }

        const uint32_t style = useAdvancedAnim ? (isCrit ? 4u : (isStatus ? 2u : 0u)) : 0u;
        const uint32_t packedColor =
            static_cast<uint32_t>(color.r) |
            (static_cast<uint32_t>(color.g) << 8u) |
            (static_cast<uint32_t>(color.b) << 16u) |
            (style << 24u);
        cmd.colorAndFlags = packedColor;
        (void)render::GPUTextSystem::Get().EnqueueCommand(cmd);
    }

    void Update(float dt) {
        // Now handled by PopupRenderer
    }

    void Draw(const Font& font) {
        // Now handled by PopupRenderer
    }

private:
    DamagePopupManager() {}
};

} // namespace NoMoreDay
