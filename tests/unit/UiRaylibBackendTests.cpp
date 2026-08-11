#include "doctest.h"

#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRaylibBackend.hpp"
#include "game/application/ui/UiViewport.hpp"

namespace NoMoreDay::ui {
namespace {

UiViewport MakeViewport(float width, float height) {
  return UiViewport::Fit({width, height});
}

struct PainterProbe {
  UiRect bounds{};
  void *userData = nullptr;
  int calls = 0;
};

void ProbePainter(void *userData, UiRect nativeBounds) {
  auto *probe = static_cast<PainterProbe *>(userData);
  probe->bounds = nativeBounds;
  probe->userData = userData;
  ++probe->calls;
}

} // namespace

TEST_CASE("[Unit] UI Raylib Backend - native conversion matches viewport for reference ratios") {
  const UiViewport viewports[] = {
      MakeViewport(2560.0f, 1440.0f), // 16:9
      MakeViewport(3440.0f, 1440.0f), // 21:9
      MakeViewport(1920.0f, 1440.0f), // 4:3
  };
  const UiVec2 logicalPoint{1234.5f, 678.25f};
  const UiRect logicalRect{{100.0f, 200.0f}, {300.0f, 150.0f}};

  for (const UiViewport &viewport : viewports) {
    REQUIRE(viewport.IsValid());

    const UiVec2 nativePoint =
        UiRaylibBackend::ToNativePoint(viewport, logicalPoint);
    const UiVec2 expectedPoint = viewport.ToPixel(logicalPoint);
    CHECK(nativePoint.x == doctest::Approx(expectedPoint.x).epsilon(0.01f));
    CHECK(nativePoint.y == doctest::Approx(expectedPoint.y).epsilon(0.01f));

    const UiRect nativeRect =
        UiRaylibBackend::ToNativeRect(viewport, logicalRect);
    const UiRect expectedRect = viewport.ToPixel(logicalRect);
    CHECK(nativeRect.origin.x ==
          doctest::Approx(expectedRect.origin.x).epsilon(0.01f));
    CHECK(nativeRect.origin.y ==
          doctest::Approx(expectedRect.origin.y).epsilon(0.01f));
    CHECK(nativeRect.size.x ==
          doctest::Approx(expectedRect.size.x).epsilon(0.01f));
    CHECK(nativeRect.size.y ==
          doctest::Approx(expectedRect.size.y).epsilon(0.01f));

    const UiVec2 roundTripped = viewport.ToLogical(nativePoint);
    CHECK(roundTripped.x == doctest::Approx(logicalPoint.x).epsilon(0.01f));
    CHECK(roundTripped.y == doctest::Approx(logicalPoint.y).epsilon(0.01f));
  }
}

TEST_CASE("[Unit] UI Raylib Backend - rendering an empty draw list does not touch GL") {
  const UiViewport viewport = MakeViewport(2560.0f, 1440.0f);
  const UiDrawList emptyDrawList;
  UiRaylibBackend backend;

  REQUIRE(emptyDrawList.IsEmpty());
  REQUIRE(emptyDrawList.ClipBalanced());

  backend.Render(viewport, emptyDrawList);
}

TEST_CASE("[Unit] UI Raylib Backend - clip-only draw list renders without touching GL") {
  const UiViewport viewport = MakeViewport(2560.0f, 1440.0f);
  UiDrawList drawList;
  UiRaylibBackend backend;

  drawList.PushClip({{0.0f, 0.0f}, {100.0f, 100.0f}});
  drawList.PopClip();

  REQUIRE(drawList.CommandCount() == 0);
  REQUIRE(drawList.IsEmpty());
  REQUIRE(drawList.ClipBalanced());

  backend.Render(viewport, drawList);
}

TEST_CASE("[Unit] UI Raylib Backend - registers, unregisters and re-registers resource ids") {
  UiRaylibBackend backend;
  const UiResourceId fontId = 1;
  const UiResourceId textureId = 2;
  const UiResourceId painterId = 3;

  backend.RegisterFont(fontId, Font{});
  backend.RegisterTexture(textureId, Texture2D{});
  backend.RegisterPainter(painterId, ProbePainter, nullptr);

  CHECK(backend.IsRegistered(fontId));
  CHECK(backend.IsRegistered(textureId));
  CHECK(backend.IsRegistered(painterId));
  CHECK_FALSE(backend.IsRegistered(99));

  backend.Unregister(fontId);
  CHECK_FALSE(backend.IsRegistered(fontId));
  CHECK(backend.IsRegistered(textureId));
  CHECK(backend.IsRegistered(painterId));

  backend.RegisterFont(fontId, Font{});
  CHECK(backend.IsRegistered(fontId));

  backend.UnregisterAll();
  CHECK_FALSE(backend.IsRegistered(fontId));
  CHECK_FALSE(backend.IsRegistered(textureId));
  CHECK_FALSE(backend.IsRegistered(painterId));
}

TEST_CASE("[Unit] UI Raylib Backend - invokes registered painter for custom commands") {
  const UiViewport viewport = MakeViewport(2560.0f, 1440.0f);
  const UiResourceId painterId = 7;
  PainterProbe probe;
  UiRaylibBackend backend;
  backend.RegisterPainter(painterId, ProbePainter, &probe);

  UiDrawList drawList;
  const UiRect bounds{{40.0f, 60.0f}, {200.0f, 80.0f}};
  drawList.Custom(UiDrawLayer::Panels, 42, bounds, painterId);

  backend.Render(viewport, drawList);

  CHECK(probe.calls == 1);
  CHECK(probe.userData == &probe);
  CHECK(probe.bounds.origin.x ==
        doctest::Approx(viewport.ToPixel(bounds.origin).x).epsilon(0.01f));
  CHECK(probe.bounds.size.x ==
        doctest::Approx(viewport.ToPixel(bounds).size.x).epsilon(0.01f));
}

TEST_CASE("[Unit] UI Raylib Backend - skips custom commands without a registered painter") {
  const UiViewport viewport = MakeViewport(2560.0f, 1440.0f);
  PainterProbe probe;
  UiRaylibBackend backend;
  backend.RegisterPainter(10, ProbePainter, &probe);

  UiDrawList drawList;
  drawList.Custom(UiDrawLayer::Panels, 5, {{0.0f, 0.0f}, {50.0f, 50.0f}},
                  99);

  backend.Render(viewport, drawList);

  CHECK(probe.calls == 0);
}

} // namespace NoMoreDay::ui
