#include "doctest.h"

#include "game/application/ui/UiViewport.hpp"

#include <fstream>
#include <iterator>
#include <string>

namespace NoMoreDay::ui {

TEST_CASE("[Unit] UI Viewport - preserves the 16 by 9 reference area") {
  const UiViewport viewport = UiViewport::Fit({2560.0f, 1440.0f});

  CHECK(viewport.IsValid());
  CHECK(viewport.Scale() == doctest::Approx(1.0f));
  CHECK(viewport.ContentRect().origin.x == doctest::Approx(0.0f));
  CHECK(viewport.ContentRect().origin.y == doctest::Approx(0.0f));
  CHECK(viewport.ContentRect().size.x == doctest::Approx(2560.0f));
  CHECK(viewport.ContentRect().size.y == doctest::Approx(1440.0f));
}

TEST_CASE("[Unit] UI Viewport - centers pillarboxed ultrawide content") {
  const UiViewport viewport = UiViewport::Fit({3440.0f, 1440.0f});

  CHECK(viewport.Scale() == doctest::Approx(1.0f));
  CHECK(viewport.ContentRect().origin.x == doctest::Approx(440.0f));
  CHECK(viewport.ContentRect().origin.y == doctest::Approx(0.0f));
  CHECK_FALSE(viewport.ContainsPixel({439.99f, 720.0f}));
  CHECK(viewport.ContainsPixel({440.0f, 720.0f}));
  CHECK_FALSE(viewport.ContainsPixel({3000.0f, 720.0f}));
}

TEST_CASE("[Unit] UI Viewport - centers letterboxed four by three content") {
  const UiViewport viewport = UiViewport::Fit({1920.0f, 1440.0f});

  CHECK(viewport.Scale() == doctest::Approx(0.75f));
  CHECK(viewport.ContentRect().origin.x == doctest::Approx(0.0f));
  CHECK(viewport.ContentRect().origin.y == doctest::Approx(180.0f));
  CHECK(viewport.ContentRect().size.x == doctest::Approx(1920.0f));
  CHECK(viewport.ContentRect().size.y == doctest::Approx(1080.0f));
  CHECK_FALSE(viewport.ContainsPixel({960.0f, 179.99f}));
  CHECK(viewport.ContainsPixel({960.0f, 180.0f}));
}

TEST_CASE("[Unit] UI Viewport - round trips points with safe insets") {
  const UiInsets safeInsets{10.0f, 20.0f, 30.0f, 40.0f};
  const UiViewport viewport = UiViewport::Fit(
      {3440.0f, 1440.0f}, kDefaultUiLogicalSize, safeInsets);
  const UiVec2 logicalPoint{1234.5f, 678.25f};

  const UiVec2 pixelPoint = viewport.ToPixel(logicalPoint);
  const UiVec2 roundTripped = viewport.ToLogical(pixelPoint);

  CHECK(roundTripped.x == doctest::Approx(logicalPoint.x));
  CHECK(roundTripped.y == doctest::Approx(logicalPoint.y));
}

TEST_CASE("[Unit] UI Runtime Types - intersects positive-area rectangles") {
  const UiRect first{{10.0f, 10.0f}, {50.0f, 40.0f}};
  const UiRect second{{40.0f, 30.0f}, {50.0f, 40.0f}};

  const UiRect overlap = first.Intersection(second);

  CHECK(overlap.origin.x == doctest::Approx(40.0f));
  CHECK(overlap.origin.y == doctest::Approx(30.0f));
  CHECK(overlap.size.x == doctest::Approx(20.0f));
  CHECK(overlap.size.y == doctest::Approx(20.0f));
  CHECK(overlap.Contains({40.0f, 30.0f}));
  CHECK_FALSE(overlap.Contains({60.0f, 50.0f}));
}

TEST_CASE("[Unit] UI Viewport - rejects empty pixel areas") {
  const UiViewport viewport = UiViewport::Fit({0.0f, 1440.0f});

  CHECK_FALSE(viewport.IsValid());
  CHECK_FALSE(viewport.ContainsPixel({0.0f, 0.0f}));
  CHECK(viewport.ToLogical({10.0f, 10.0f}).x == doctest::Approx(0.0f));
  CHECK(viewport.ToLogical({10.0f, 10.0f}).y == doctest::Approx(0.0f));
}

TEST_CASE("[Unit] UI Runtime Types - excludes backend and gameplay dependencies") {
  std::ifstream source("src/game/application/ui/UiRuntimeTypes.hpp");
  REQUIRE(source.is_open());
  const std::string contents{std::istreambuf_iterator<char>(source),
                             std::istreambuf_iterator<char>()};

  CHECK(contents.find("raylib.h") == std::string::npos);
  CHECK(contents.find("entt/") == std::string::npos);
  CHECK(contents.find("UiShared") == std::string::npos);
  CHECK(contents.find("InventorySystem") == std::string::npos);
}

} // namespace NoMoreDay::ui
