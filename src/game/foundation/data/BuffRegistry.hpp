#pragma once
#include <string>
#include <unordered_map>
#include "raylib.h"
#include "game/foundation/components/Buff.hpp"

namespace assets { struct TextureAsset; }

namespace NoMoreDay {

struct BuffVisualData {
    std::string icon_text;
    Color border_color;
    std::string name;
    std::string description;
    bool is_debuff = false;
    const assets::TextureAsset* icon_asset = nullptr; // Added for icon support
};

class BuffRegistry {
public:
    static void Initialize();
    static void Shutdown();
    static const BuffVisualData& GetVisualData(BuffType type);

    // True when the visual data is the registry's fallback "Unknown" entry
    // (i.e. the buff type has no registered visual data).
    [[nodiscard]] static bool IsUnknownVisual(const BuffVisualData& data);

private:
    static std::unordered_map<BuffType, BuffVisualData> registry;
    static BuffVisualData default_data;
};

} // namespace NoMoreDay