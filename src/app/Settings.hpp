#pragma once
#include "engine/render/core/RenderConstants.hpp"
#include <nlohmann/json.hpp>
#include <array>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace NoMoreDay {

    struct GameSettings {
        using RenderQualityTier = render::core::QualityTier;
        static constexpr size_t kRenderQualityTierCount = 4;

        // constexpr lookup table (IIFE), enum value is direct index.
        static constexpr auto kRenderQualityTierTable = []() constexpr {
            return std::array<std::string_view, kRenderQualityTierCount>{
                "Low",
                "Medium",
                "High",
                "Ultra",
            };
        }();

        float cameraZoom = 1.5f;
        float shakeIntensity = 1.0f;
        int targetFPS = 0;  // 0 for Unlimited FPS
        RenderQualityTier renderQualityTier = RenderQualityTier::Medium;

        static constexpr size_t RenderQualityTierToIndex(RenderQualityTier tier) {
            return static_cast<size_t>(tier);
        }

        static constexpr std::string_view RenderQualityTierToStringView(RenderQualityTier tier) {
            const size_t index = RenderQualityTierToIndex(tier);
            if (index < kRenderQualityTierTable.size()) {
                return kRenderQualityTierTable[index];
            }
            return "Medium";
        }

        static RenderQualityTier RenderQualityTierFromString(std::string value) {
            auto equalsIgnoreCase = [](std::string_view lhs,
                                       std::string_view rhs) -> bool {
                if (lhs.size() != rhs.size()) {
                    return false;
                }
                for (size_t i = 0; i < lhs.size(); ++i) {
                    const auto l = static_cast<unsigned char>(lhs[i]);
                    const auto r = static_cast<unsigned char>(rhs[i]);
                    if (std::tolower(l) != std::tolower(r)) {
                        return false;
                    }
                }
                return true;
            };

            const std::string_view source(value);
            for (size_t i = 0; i < kRenderQualityTierTable.size(); ++i) {
                if (equalsIgnoreCase(source, kRenderQualityTierTable[i])) {
                    return static_cast<RenderQualityTier>(i);
                }
            }
            return RenderQualityTier::Medium;
        }

        void Save(const std::string& filePath = "settings.json") {
            nlohmann::json j;
            j["cameraZoom"] = cameraZoom;
            j["shakeIntensity"] = shakeIntensity;
            j["targetFPS"] = targetFPS;
            j["renderQualityTier"] =
                std::string(RenderQualityTierToStringView(renderQualityTier));
            
            std::ofstream file(filePath);
            if (file.is_open()) {
                file << j.dump(4);
            }
        }

        void Load(const std::string& filePath = "settings.json") {
            if (!std::filesystem::exists(filePath)) {
                Save(filePath);
                return;
            }

            try {
                std::ifstream file(filePath);
                nlohmann::json j;
                file >> j;

                if (j.contains("cameraZoom")) cameraZoom = j["cameraZoom"].get<float>();
                if (j.contains("shakeIntensity")) shakeIntensity = j["shakeIntensity"].get<float>();
                if (j.contains("targetFPS")) targetFPS = j["targetFPS"].get<int>();
                if (j.contains("renderQualityTier")) {
                    const auto& tierValue = j["renderQualityTier"];
                    if (tierValue.is_string()) {
                        renderQualityTier =
                            RenderQualityTierFromString(tierValue.get<std::string>());
                    } else if (tierValue.is_number_integer()) {
                        const int idx = tierValue.get<int>();
                        if (idx >= static_cast<int>(RenderQualityTier::Low) &&
                            idx <= static_cast<int>(RenderQualityTier::Ultra)) {
                            renderQualityTier = static_cast<RenderQualityTier>(idx);
                        }
                    }
                }
            } catch (...) {
                // Fallback to defaults on error
            }
        }
    };

}
