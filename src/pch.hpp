#pragma once

// STL
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_map>
#include <iostream>
#include <array>
#include <functional>
#include <chrono>
#include <filesystem>
#include <utility>
#include <optional>
#include <variant>
#include <span>
#include <concepts>
#include <numbers>
#include <cassert>
#include <type_traits>
#include <string_view>
#include <numeric>
#include <cstdint>
#include <cstddef>

// Third Party - Heavy Headers
#include <raylib.h>
#include <raymath.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

// Fix for rlgl.h in Unity Build / PCH:
// rlgl.h contains implementation code. We force these functions to be 'inline'
// to prevent "redefinition" errors in Unity Builds and "multiple definition" errors at link time.
// Using 'static inline' instead of 'inline' prevents "used but never defined" warnings
// when certain functions (like SSBO ones) are guarded by macros.
#if defined(RLAPI)
    #undef RLAPI
#endif
#ifndef GRAPHICS_API_OPENGL_43
    #define GRAPHICS_API_OPENGL_43
#endif
#define RLAPI static inline
#include <rlgl.h>
#undef RLAPI

#include <entt/entt.hpp>
#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/for_each.hpp>

// Project Utilities
#include "tools/Logger.hpp"
#include "components/Stats.hpp"
#include "components/Common.hpp"
