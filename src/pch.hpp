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

#include <entt/entt.hpp>
#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/for_each.hpp>

// Project Utilities
#include "core/logging/Logger.hpp"

#include "game/components/Common.hpp"

// Engine
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUEntitySystem.hpp"