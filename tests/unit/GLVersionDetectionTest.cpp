#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/DeviceCapabilityMatrix.hpp"

using namespace NoMoreDay::render::core;
using namespace NoMoreDay::utils;

TEST_CASE("[Unit] DeviceCapabilityMatrix - IsDesktopGL43OrNewer predicate truth table") {
  struct TestCase {
    int major;
    int minor;
    bool isGles;
    bool expected;
    const char *description;
  };

  const TestCase testCases[] = {
      {4, 3, false, true, "OpenGL 4.3 Desktop Core Profile -> true"},
      {4, 5, false, true, "OpenGL 4.5 Desktop Core Profile -> true"},
      {4, 6, false, true, "OpenGL 4.6 Desktop Core Profile -> true"},
      {5, 0, false, true, "OpenGL 5.0 Desktop Core Profile -> true"},
      {6, 0, false, true, "OpenGL 6.0 Desktop Core Profile -> true"},
      {4, 4, false, true, "OpenGL 4.4 Desktop Core Profile -> true"},
      {4, 2, false, false, "OpenGL 4.2 Desktop Core Profile (below 4.3) -> false"},
      {3, 3, false, false, "OpenGL 3.3 Desktop Core Profile (legacy) -> false"},
      {4, 5, true, false, "OpenGL ES 4.5 profile (GLES rejected) -> false"},
      {3, 0, true, false, "OpenGL ES 3.0 profile (GLES rejected) -> false"},
      {3, 2, true, false, "OpenGL ES 3.2 profile (GLES rejected) -> false"},
      {0, 0, false, false, "Uninitialized / invalid GL 0.0 -> false"},
      {-1, -1, false, false, "Negative GL version (-1, -1) -> false"},
      {4, -1, false, false, "Negative minor version (4, -1) -> false"},
      {-4, 3, false, false, "Negative major version (-4, 3) -> false"},
      {0, 3, false, false, "Zero major version (0, 3) -> false"},
      {4, 0, false, false, "OpenGL 4.0 (below 4.3) -> false"},
      {2, 1, false, false, "OpenGL 2.1 (legacy) -> false"},
  };

  for (const auto &tc : testCases) {
    CAPTURE(tc.major);
    CAPTURE(tc.minor);
    CAPTURE(tc.isGles);
    CAPTURE(tc.description);
    CHECK(DeviceCapabilityMatrix::IsDesktopGL43OrNewer(tc.major, tc.minor, tc.isGles) == tc.expected);
  }
}

TEST_CASE("[Unit] DeviceCapabilityMatrix - IsDesktopGL43OrNewer specific edge cases") {
  SUBCASE("OpenGL 4.3 desktop core profile is supported") {
    CHECK(DeviceCapabilityMatrix::IsDesktopGL43OrNewer(4, 3, false));
  }

  SUBCASE("OpenGL 4.5 desktop core profile is supported") {
    CHECK(DeviceCapabilityMatrix::IsDesktopGL43OrNewer(4, 5, false));
  }

  SUBCASE("OpenGL 4.6 desktop core profile is supported") {
    CHECK(DeviceCapabilityMatrix::IsDesktopGL43OrNewer(4, 6, false));
  }

  SUBCASE("OpenGL 5.0 future desktop profile is supported") {
    CHECK(DeviceCapabilityMatrix::IsDesktopGL43OrNewer(5, 0, false));
  }

  SUBCASE("OpenGL 4.2 desktop core profile is rejected") {
    CHECK_FALSE(DeviceCapabilityMatrix::IsDesktopGL43OrNewer(4, 2, false));
  }

  SUBCASE("OpenGL 3.3 desktop core profile is rejected") {
    CHECK_FALSE(DeviceCapabilityMatrix::IsDesktopGL43OrNewer(3, 3, false));
  }

  SUBCASE("OpenGL ES profile is rejected even with high version numbers") {
    CHECK_FALSE(DeviceCapabilityMatrix::IsDesktopGL43OrNewer(4, 5, true));
    CHECK_FALSE(DeviceCapabilityMatrix::IsDesktopGL43OrNewer(4, 3, true));
    CHECK_FALSE(DeviceCapabilityMatrix::IsDesktopGL43OrNewer(5, 0, true));
  }

  SUBCASE("Zero version is rejected") {
    CHECK_FALSE(DeviceCapabilityMatrix::IsDesktopGL43OrNewer(0, 0, false));
    CHECK_FALSE(DeviceCapabilityMatrix::IsDesktopGL43OrNewer(0, 3, false));
  }

  SUBCASE("Negative versions fail closed") {
    CHECK_FALSE(DeviceCapabilityMatrix::IsDesktopGL43OrNewer(-1, -1, false));
    CHECK_FALSE(DeviceCapabilityMatrix::IsDesktopGL43OrNewer(4, -1, false));
    CHECK_FALSE(DeviceCapabilityMatrix::IsDesktopGL43OrNewer(-4, 3, false));
  }
}

TEST_CASE("[Unit] GPUUtils - CheckSupport without active GL context fails closed") {
  // If no GL context has been created or GPUUtils initialized in unit test mode,
  // CheckSupport() must safely return zeroed version info and computeShaderSupported == false.
  if (!GPUUtils::IsInitialized()) {
    const GPUSupportInfo info = GPUUtils::CheckSupport();
    CHECK(info.majorVersion == 0);
    CHECK(info.minorVersion == 0);
    CHECK_FALSE(info.computeShaderSupported);
  }
}
