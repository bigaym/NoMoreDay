#include "doctest.h"

#include "game/application/ui/WorldUiFrame.hpp"

namespace NoMoreDay::ui {

// U8 frame token / lifetime contract (plan §11): BeginFrame opens a new
// frame with a fresh frame token and clears the previous frame's data in
// place, so a consumer that holds a reference to the visible-items vector
// across frames observes an empty collection instead of stale previous-frame
// data. Entities are plain ids here; no entt::registry is required.

TEST_CASE(
    "[Unit] WorldUiFrame - BeginFrame rotates the frame token and clears "
    "previous frame data") {
  WorldUiFrame frame;

  CHECK(frame.FrameToken() == 0u);
  frame.BeginFrame(1u);
  CHECK(frame.IsValid());
  CHECK(frame.FrameToken() == 1u);
  CHECK(frame.VisibleItems().empty());

  frame.AddItem(entt::entity{10}, Rectangle{10.0f, 20.0f, 30.0f, 40.0f}, 0.5f);
  frame.SetHovered(entt::entity{10});
  CHECK_FALSE(frame.VisibleItems().empty());
  CHECK(frame.HasHovered());

  frame.BeginFrame(2u);
  CHECK(frame.FrameToken() == 2u);
  CHECK(frame.VisibleItems().empty());
  CHECK_FALSE(frame.HasHovered());
  CHECK((frame.HoveredItem() == entt::null));
}

TEST_CASE("[Unit] WorldUiFrame - is invalid until the first BeginFrame") {
  WorldUiFrame frame;

  CHECK_FALSE(frame.IsValid());
  CHECK(frame.FrameToken() == 0u);
  CHECK(frame.VisibleItems().empty());
  CHECK_FALSE(frame.HasHovered());
  CHECK((frame.HoveredItem() == entt::null));

  frame.BeginFrame(42u);
  CHECK(frame.IsValid());
  CHECK(frame.FrameToken() == 42u);
}

TEST_CASE("[Unit] WorldUiFrame - accumulates visible item proxies in insertion order") {
  WorldUiFrame frame;
  frame.BeginFrame(7u);

  frame.AddItem(entt::entity{1}, Rectangle{10.0f, 20.0f, 30.0f, 40.0f}, 0.1f);
  frame.AddItem(entt::entity{2}, Rectangle{50.0f, 60.0f, 70.0f, 80.0f});
  frame.AddItem(entt::entity{3}, Rectangle{-5.0f, -6.0f, 15.0f, 25.0f}, 3.0f);

  const auto& items = frame.VisibleItems();
  REQUIRE(items.size() == 3u);
  CHECK(items[0].entity == entt::entity{1});
  CHECK(items[0].worldRect.x == doctest::Approx(10.0f));
  CHECK(items[0].worldRect.y == doctest::Approx(20.0f));
  CHECK(items[0].worldRect.width == doctest::Approx(30.0f));
  CHECK(items[0].worldRect.height == doctest::Approx(40.0f));
  CHECK(items[0].depth == doctest::Approx(0.1f));
  CHECK(items[1].entity == entt::entity{2});
  CHECK(items[1].worldRect.x == doctest::Approx(50.0f));
  CHECK(items[1].worldRect.y == doctest::Approx(60.0f));
  CHECK(items[1].depth == doctest::Approx(0.0f)); // default depth
  CHECK(items[2].entity == entt::entity{3});
  CHECK(items[2].worldRect.x == doctest::Approx(-5.0f));
  CHECK(items[2].worldRect.height == doctest::Approx(25.0f));
  CHECK(items[2].depth == doctest::Approx(3.0f));
}

TEST_CASE("[Unit] WorldUiFrame - tracks and clears the hovered item") {
  WorldUiFrame frame;
  frame.BeginFrame(9u);

  CHECK((frame.HoveredItem() == entt::null));
  CHECK_FALSE(frame.HasHovered());

  frame.SetHovered(entt::entity{5});
  CHECK(frame.HoveredItem() == entt::entity{5});
  CHECK(frame.HasHovered());

  frame.ClearHovered();
  CHECK((frame.HoveredItem() == entt::null));
  CHECK_FALSE(frame.HasHovered());

  frame.SetHovered(entt::entity{6});
  CHECK(frame.HoveredItem() == entt::entity{6});
}

TEST_CASE("[Unit] WorldUiFrame - never exposes a stale previous frame "
          "(frame token / lifetime contract)") {
  WorldUiFrame frame;
  frame.BeginFrame(1u);
  frame.AddItem(entt::entity{1}, Rectangle{10.0f, 20.0f, 30.0f, 40.0f}, 0.5f);
  frame.AddItem(entt::entity{2}, Rectangle{50.0f, 60.0f, 70.0f, 80.0f}, 1.5f);
  frame.SetHovered(entt::entity{2});

  // A consumer may hold the previous frame's vector reference (and cache the
  // hovered entity) beyond the frame boundary.
  const auto& staleItems = frame.VisibleItems();
  const entt::entity staleHovered = frame.HoveredItem();
  CHECK_FALSE(staleItems.empty());
  CHECK(staleHovered == entt::entity{2});

  // The next BeginFrame clears in place: the held reference observes an empty
  // collection instead of stale data from frame token 1.
  frame.BeginFrame(2u);
  CHECK(frame.FrameToken() == 2u);
  CHECK(staleItems.empty());
  CHECK(frame.VisibleItems().empty());
  CHECK_FALSE(frame.HasHovered());
  CHECK((frame.HoveredItem() == entt::null));

  // The new frame is writable again.
  frame.AddItem(entt::entity{3}, Rectangle{1.0f, 2.0f, 3.0f, 4.0f});
  frame.SetHovered(entt::entity{3});
  REQUIRE(frame.VisibleItems().size() == 1u);
  CHECK(frame.VisibleItems()[0].entity == entt::entity{3});
  CHECK(frame.HoveredItem() == entt::entity{3});
  CHECK(frame.IsValid());
}

TEST_CASE("[Unit] WorldUiFrame - BeginFrame is idempotent when reusing the same token") {
  WorldUiFrame frame;
  frame.BeginFrame(3u);
  frame.AddItem(entt::entity{1}, Rectangle{10.0f, 20.0f, 30.0f, 40.0f}, 0.25f);
  frame.SetHovered(entt::entity{1});

  // Same token twice: data is cleared, no crash, token unchanged.
  frame.BeginFrame(3u);
  CHECK(frame.FrameToken() == 3u);
  CHECK(frame.VisibleItems().empty());
  CHECK_FALSE(frame.HasHovered());
  CHECK((frame.HoveredItem() == entt::null));

  frame.AddItem(entt::entity{2}, Rectangle{5.0f, 6.0f, 7.0f, 8.0f}, 0.5f);
  CHECK(frame.VisibleItems().size() == 1u);
  CHECK(frame.VisibleItems()[0].entity == entt::entity{2});
}

} // namespace NoMoreDay::ui
