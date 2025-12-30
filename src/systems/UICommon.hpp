#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "../components/ItemComponent.hpp"
#include "../core/UIContext.hpp"

namespace NoMoreDay {

// Alias for backward compatibility during refactor
using UIState_t = UIContext;

// UI Reference Resolution (2K)
constexpr float UI_REF_WIDTH = 2560.0f;
constexpr float UI_REF_HEIGHT = 1440.0f;

} // namespace NoMoreDay