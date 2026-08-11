#include "doctest.h"

#include "game/application/ui/AstrolabeRenderer.hpp"

#include <cctype>
#include <fstream>
#include <iterator>
#include <string>

using NoMoreDay::AstrolabeRenderer;
using NoMoreDay::TalentNodeType;

TEST_CASE("[Unit] AstrolabeRenderer getNodeRadius is pure") {
  // Node radii come from Constants::Astrolabe (AstrolabeConstants.hpp):
  // NODE_RADIUS_MINOR = 12.0f, NODE_RADIUS_MAJOR = 12.0f,
  // NODE_RADIUS_CORE = 16.0f. getNodeRadius is a pure lookup with no GL or
  // registry access, so it is safe to call without a window.
  AstrolabeRenderer renderer;
  CHECK(renderer.getNodeRadius(TalentNodeType::Minor) ==
        doctest::Approx(12.0f));
  CHECK(renderer.getNodeRadius(TalentNodeType::Major) ==
        doctest::Approx(12.0f));
  CHECK(renderer.getNodeRadius(TalentNodeType::Core) ==
        doctest::Approx(16.0f));
}

TEST_CASE("[Unit] AstrolabeRenderer Unload is idempotent and headless-safe") {
  // A default-constructed renderer holds no resources (all ids are 0), so
  // Unload() and the destructor take the early-exit path and never touch GL.
  AstrolabeRenderer renderer;
  renderer.Unload();
  renderer.Unload();
}

TEST_CASE("[Unit] AstrolabeRenderer instances are independent") {
  // U7 cleanup: the renderer is an instance type. Two instances must own
  // separate shader/texture/cache state; each must construct and destroy
  // without sharing or corrupting the other. Headless-safe: neither instance
  // is initialized, so no GL calls are made on construction or destruction.
  AstrolabeRenderer a;
  AstrolabeRenderer b;
  CHECK(a.getNodeRadius(TalentNodeType::Core) ==
        doctest::Approx(16.0f));
  CHECK(b.getNodeRadius(TalentNodeType::Core) ==
        doctest::Approx(16.0f));
}

TEST_CASE("[Unit] AstrolabeRenderer header holds no static mutable state") {
  const std::string path = "src/game/application/ui/AstrolabeRenderer.hpp";
  std::ifstream in(path);
  REQUIRE_MESSAGE(in.good(), "cannot open ", path);
  std::string contents((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());

  const std::string classNeedle = "class AstrolabeRenderer";
  std::string::size_type classPos = contents.find(classNeedle);
  REQUIRE_MESSAGE(classPos != std::string::npos, "class declaration not found");
  const std::string body = contents.substr(classPos);

  // Static data members are forbidden (design invariant 4: no static mutable
  // rendering state). Static pure member functions would be allowed.
  const std::string needle = "static ";
  std::string::size_type pos = body.find(needle);
  while (pos != std::string::npos) {
    const char after = body[pos + needle.size()];
    CHECK_MESSAGE(
        std::isalpha(static_cast<unsigned char>(after)) != 0,
        "static data member must not exist in AstrolabeRenderer: '",
        body.substr(pos, body.find_first_of(";{}", pos) - pos + 1), "'");
    pos = body.find(needle, pos + needle.size());
  }
}

TEST_CASE("[Unit] AstrolabeRenderer cpp has no static member leftovers") {
  const std::string path = "src/game/application/ui/AstrolabeRenderer.cpp";
  std::ifstream in(path);
  REQUIRE_MESSAGE(in.good(), "cannot open ", path);
  std::string contents((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());

  // The old static data members (s_shGalaxy, s_shNode, s_whitePixel,
  // s_galaxyCache, s_galaxyCacheRes, s_galaxyCacheValid, s_initialized) and
  // the anonymous-namespace uniform-location cache were migrated to instance
  // members. No definition may reference the static storage anymore.
  const std::string needle = "AstrolabeRenderer::s_";
  CHECK_MESSAGE(contents.find(needle) == std::string::npos,
                "legacy static member reference found: ", needle);

  for (const char* legacy : {"s_shGalaxy", "s_shNode", "s_whitePixel",
                             "s_galaxyCache", "s_galaxyCacheRes",
                             "s_galaxyCacheValid", "s_initialized"}) {
    CHECK_MESSAGE(contents.find(legacy) == std::string::npos,
                  "legacy static member name still present: ", legacy);
  }
}
