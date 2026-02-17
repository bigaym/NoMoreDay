#pragma once

#include <cstdint>
#include <type_traits>

namespace NoMoreDay {

enum class ShadowOccluderShape : uint32_t {
  Circle = 0,
  Capsule = 1,
  Box = 2,
};

struct ShadowCasterComponent {
  ShadowOccluderShape shape = ShadowOccluderShape::Circle;
  float occluderHeight = 1.0f;
  uint32_t dynamicFlag = 0;
};

static_assert(std::is_standard_layout_v<ShadowCasterComponent>,
              "ShadowCasterComponent must be standard layout");
static_assert(std::is_trivially_copyable_v<ShadowCasterComponent>,
              "ShadowCasterComponent must be trivially copyable");

} // namespace NoMoreDay
