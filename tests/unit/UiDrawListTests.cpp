#include "doctest.h"

#include "game/application/ui/UiDrawList.hpp"

#include <fstream>
#include <iterator>
#include <string>

namespace NoMoreDay::ui {
namespace {

UiRect MakeRect(float x, float y, float width, float height) {
  return {{x, y}, {width, height}};
}

} // namespace

TEST_CASE("[Unit] UI draw list appends clears and reserves commands") {
  UiDrawList list;
  CHECK(list.IsEmpty());
  CHECK(list.CommandCount() == 0);
  CHECK_FALSE(list.IsFinalized());

  list.Reserve(16);
  CHECK(list.Commands().capacity() >= 16);

  list.FillRect(UiDrawLayer::Hud, 1, MakeRect(0.0f, 0.0f, 10.0f, 10.0f), {});
  list.FillRect(UiDrawLayer::Panels, 2, MakeRect(0.0f, 0.0f, 10.0f, 10.0f), {});
  CHECK_FALSE(list.IsEmpty());
  CHECK(list.CommandCount() == 2);
  CHECK_FALSE(list.IsFinalized());

  list.Finalize();
  CHECK(list.IsFinalized());
  CHECK(list.CommandCount() == 2);

  list.Clear();
  CHECK(list.IsEmpty());
  CHECK(list.CommandCount() == 0);
  CHECK(list.Commands().empty());
  CHECK(list.Clips().empty());
  CHECK_FALSE(list.IsFinalized());
}

TEST_CASE("[Unit] UI draw list finalizes into total order by layer then node") {
  UiDrawList list;
  list.FillRect(UiDrawLayer::Panels, 5, MakeRect(0.0f, 0.0f, 1.0f, 1.0f), {});
  list.FillRect(UiDrawLayer::Hud, 9, MakeRect(0.0f, 0.0f, 1.0f, 1.0f), {});
  list.FillRect(UiDrawLayer::Panels, 2, MakeRect(0.0f, 0.0f, 1.0f, 1.0f), {});
  list.FillRect(UiDrawLayer::Modal, 1, MakeRect(0.0f, 0.0f, 1.0f, 1.0f), {});
  list.FillRect(UiDrawLayer::DragPreview, 7, MakeRect(0.0f, 0.0f, 1.0f, 1.0f),
                {});
  list.FillRect(UiDrawLayer::Tooltip, 3, MakeRect(0.0f, 0.0f, 1.0f, 1.0f), {});
  list.FillRect(UiDrawLayer::Debug, 6, MakeRect(0.0f, 0.0f, 1.0f, 1.0f), {});

  list.Finalize();
  const auto &commands = list.Commands();
  REQUIRE(commands.size() == 7);
  CHECK(commands[0].layer == UiDrawLayer::Hud);
  CHECK(commands[0].nodeId == 9);
  CHECK(commands[1].layer == UiDrawLayer::Panels);
  CHECK(commands[1].nodeId == 2);
  CHECK(commands[2].layer == UiDrawLayer::Panels);
  CHECK(commands[2].nodeId == 5);
  CHECK(commands[3].layer == UiDrawLayer::DragPreview);
  CHECK(commands[3].nodeId == 7);
  CHECK(commands[4].layer == UiDrawLayer::Modal);
  CHECK(commands[4].nodeId == 1);
  CHECK(commands[5].layer == UiDrawLayer::Tooltip);
  CHECK(commands[5].nodeId == 3);
  CHECK(commands[6].layer == UiDrawLayer::Debug);
  CHECK(commands[6].nodeId == 6);
}

TEST_CASE("[Unit] UI draw list append sequence breaks ties deterministically") {
  UiDrawList list;
  // Same (layer, nodeId) key appended twice; the append sequence must keep the
  // append order after Finalize (total order, deterministic across builds).
  list.FillRect(UiDrawLayer::Hud, 4, MakeRect(0.0f, 0.0f, 10.0f, 10.0f),
                {255, 0, 0, 255});
  list.FillRect(UiDrawLayer::Hud, 4, MakeRect(0.0f, 0.0f, 20.0f, 20.0f),
                {0, 255, 0, 255});

  list.Finalize();
  const auto &commands = list.Commands();
  REQUIRE(commands.size() == 2);
  CHECK(commands[0].rect.size.x == doctest::Approx(10.0f));
  CHECK(commands[0].color.r == 255);
  CHECK(commands[0].color.g == 0);
  CHECK(commands[1].rect.size.x == doctest::Approx(20.0f));
  CHECK(commands[1].color.g == 255);
}

TEST_CASE("[Unit] UI draw list text commands copy into the arena") {
  UiDrawList list;
  std::string label = "potions";
  list.Text(UiDrawLayer::Hud, 7, label, {10.0f, 20.0f}, 24.0f, {});
  label = "mutated after append";

  const auto &commands = list.Commands();
  REQUIRE(commands.size() == 1);
  CHECK(std::string(list.TextAt(commands[0])) == "potions");

  const std::string moved = "temporary text";
  list.Text(UiDrawLayer::Panels, 8, moved, {0.0f, 0.0f}, 16.0f, {});
  REQUIRE(commands.size() == 2);
  CHECK(std::string(list.TextAt(commands[1])) == "temporary text");

  // The arena bytes are valid until the next Clear().
  CHECK(list.TextBytesUsed() ==
        std::string("potions").size() + std::string("temporary text").size());
  CHECK(list.TextOverflow() == 0);
}

TEST_CASE("[Unit] UI draw list text arena is reused across Clear") {
  UiDrawList list;
  list.Text(UiDrawLayer::Hud, 1, "abcd", {0.0f, 0.0f}, 20.0f, {});
  CHECK(list.TextBytesUsed() == 4);
  CHECK(std::string(list.TextAt(list.Commands()[0])) == "abcd");

  // Clear() resets the cursor; the next frame's text reuses the same arena
  // block (no reallocation, TextCapacity is unchanged).
  const std::size_t capacity = list.TextCapacity();
  list.Clear();
  CHECK(list.TextBytesUsed() == 0);
  CHECK(list.TextCapacity() == capacity);

  list.Text(UiDrawLayer::Hud, 2, "ef", {0.0f, 0.0f}, 20.0f, {});
  CHECK(list.TextBytesUsed() == 2);
  CHECK(std::string(list.TextAt(list.Commands()[0])) == "ef");
}

TEST_CASE("[Unit] UI draw list encodes per kind command fields") {
  UiDrawList list;
  list.Line(UiDrawLayer::Hud, 2, {5.0f, 6.0f}, {15.0f, 26.0f},
            {255, 0, 0, 255}, 2.0f);
  list.StrokeRect(UiDrawLayer::Panels, 1, MakeRect(1.0f, 2.0f, 30.0f, 40.0f),
                  {10, 20, 30, 255}, 3.0f);
  list.Image(UiDrawLayer::Modal, 4, MakeRect(0.0f, 0.0f, 64.0f, 64.0f), 99,
             {255, 255, 255, 128});
  list.Text(UiDrawLayer::Tooltip, 3, std::string("hi"), {8.0f, 9.0f}, 22.0f,
            {0, 0, 0, 255}, 77);
  list.Custom(UiDrawLayer::Debug, 5, MakeRect(10.0f, 10.0f, 100.0f, 50.0f),
              123);

  const auto &commands = list.Commands();
  REQUIRE(commands.size() == 5);

  const auto &line = commands[0];
  CHECK(line.kind == UiDrawKind::Line);
  CHECK(line.rect.origin.x == doctest::Approx(5.0f));
  CHECK(line.rect.origin.y == doctest::Approx(6.0f));
  CHECK(line.rect.size.x == doctest::Approx(10.0f));
  CHECK(line.rect.size.y == doctest::Approx(20.0f));
  CHECK(line.color.r == 255);
  CHECK(line.strokeThickness == doctest::Approx(2.0f));

  const auto &stroke = commands[1];
  CHECK(stroke.kind == UiDrawKind::StrokeRect);
  CHECK(stroke.rect.origin.x == doctest::Approx(1.0f));
  CHECK(stroke.rect.size.y == doctest::Approx(40.0f));
  CHECK(stroke.color.g == 20);
  CHECK(stroke.strokeThickness == doctest::Approx(3.0f));

  const auto &image = commands[2];
  CHECK(image.kind == UiDrawKind::Image);
  CHECK(image.rect.size.x == doctest::Approx(64.0f));
  CHECK(image.resourceId == 99);
  CHECK(image.color.a == 128);

  const auto &text = commands[3];
  CHECK(text.kind == UiDrawKind::Text);
  CHECK(text.rect.origin.x == doctest::Approx(8.0f));
  CHECK(text.rect.size.y == doctest::Approx(22.0f));
  CHECK(text.resourceId == 77);
  CHECK(std::string(list.TextAt(text)) == "hi");

  const auto &custom = commands[4];
  CHECK(custom.kind == UiDrawKind::Custom);
  CHECK(custom.rect.origin.y == doctest::Approx(10.0f));
  CHECK(custom.rect.size.x == doctest::Approx(100.0f));
  CHECK(custom.resourceId == 123);
}

TEST_CASE("[Unit] UI draw list tracks clip balance and current clip index") {
  UiDrawList list;
  CHECK(list.ClipBalanced());
  CHECK(list.CurrentClipIndex() == kNoClipIndex);

  list.PushClip(MakeRect(0.0f, 0.0f, 800.0f, 600.0f));
  CHECK(list.CurrentClipIndex() == 0);
  list.FillRect(UiDrawLayer::Panels, 1, MakeRect(5.0f, 5.0f, 10.0f, 10.0f),
                {});
  list.PushClip(MakeRect(10.0f, 10.0f, 100.0f, 100.0f));
  CHECK(list.CurrentClipIndex() == 1);
  list.FillRect(UiDrawLayer::Hud, 2, MakeRect(0.0f, 0.0f, 5.0f, 5.0f), {});
  CHECK_FALSE(list.ClipBalanced());

  list.PopClip();
  CHECK_FALSE(list.ClipBalanced());
  list.PopClip();
  CHECK(list.ClipBalanced());
  CHECK(list.CurrentClipIndex() == kNoClipIndex);

  // The pipeline contract: commands are appended in paint order and only
  // sorted into the total draw order (layer, node, append sequence) by
  // Finalize() before the backend submits them.
  list.Finalize();

  const auto &commands = list.Commands();
  REQUIRE(commands.size() == 2);
  CHECK(commands[0].nodeId == 2);
  CHECK(commands[0].clipIndex == 1);
  CHECK(commands[1].nodeId == 1);
  CHECK(commands[1].clipIndex == 0);

  const auto &clips = list.Clips();
  REQUIRE(clips.size() == 2);
  CHECK(clips[0].size.x == doctest::Approx(800.0f));
  CHECK(clips[1].origin.x == doctest::Approx(10.0f));

  list.Clear();
  CHECK(list.ClipBalanced());
  CHECK(list.CurrentClipIndex() == kNoClipIndex);
}

TEST_CASE("[Unit] UI draw list spurious pop at empty stack does not crash") {
  UiDrawList list;
  list.PopClip();
  CHECK_FALSE(list.ClipBalanced());

  list.PushClip(MakeRect(0.0f, 0.0f, 10.0f, 10.0f));
  list.PopClip();
  CHECK_FALSE(list.ClipBalanced());

  list.Clear();
  CHECK(list.ClipBalanced());
}

TEST_CASE("[Unit] UI draw list is deterministic across identical builds") {
  const auto Build = [] {
    UiDrawList list;
    list.PushClip(MakeRect(0.0f, 0.0f, 800.0f, 600.0f));
    list.FillRect(UiDrawLayer::Panels, 3, MakeRect(1.0f, 2.0f, 30.0f, 40.0f),
                  {10, 20, 30, 255});
    list.Text(UiDrawLayer::Hud, 1, std::string("hp"), {5.0f, 6.0f}, 18.0f,
              {255, 255, 255, 255});
    list.Line(UiDrawLayer::Debug, 9, {0.0f, 0.0f}, {10.0f, 10.0f},
              {255, 0, 0, 255}, 2.0f);
    list.Image(UiDrawLayer::Modal, 7, MakeRect(0.0f, 0.0f, 64.0f, 64.0f), 99,
               {255, 255, 255, 128});
    list.Custom(UiDrawLayer::DragPreview, 5,
                MakeRect(10.0f, 10.0f, 100.0f, 50.0f), 123);
    list.PopClip();
    list.Finalize();
    return list;
  };

  const UiDrawList first = Build();
  const UiDrawList second = Build();

  REQUIRE(first.CommandCount() == second.CommandCount());
  REQUIRE(first.Clips().size() == second.Clips().size());
  REQUIRE(first.CommandCount() == 5);
  REQUIRE(first.Clips().size() == 1);

  for (std::size_t i = 0; i < first.CommandCount(); ++i) {
    const UiDrawCommand &a = first.Commands()[i];
    const UiDrawCommand &b = second.Commands()[i];
    CHECK(a.kind == b.kind);
    CHECK(a.layer == b.layer);
    CHECK(a.nodeId == b.nodeId);
    CHECK(a.clipIndex == b.clipIndex);
    CHECK(a.rect.origin.x == doctest::Approx(b.rect.origin.x));
    CHECK(a.rect.origin.y == doctest::Approx(b.rect.origin.y));
    CHECK(a.rect.size.x == doctest::Approx(b.rect.size.x));
    CHECK(a.rect.size.y == doctest::Approx(b.rect.size.y));
    CHECK(a.color.r == b.color.r);
    CHECK(a.color.g == b.color.g);
    CHECK(a.color.b == b.color.b);
    CHECK(a.color.a == b.color.a);
    CHECK(a.resourceId == b.resourceId);
    CHECK(a.textOffset == b.textOffset);
    CHECK(a.textLength == b.textLength);
    CHECK(a.appendSequence == b.appendSequence);
    CHECK(std::string(first.TextAt(a)) == std::string(second.TextAt(b)));
    CHECK(a.strokeThickness == doctest::Approx(b.strokeThickness));
  }

  for (std::size_t i = 0; i < first.Clips().size(); ++i) {
    CHECK(first.Clips()[i].origin.x ==
          doctest::Approx(second.Clips()[i].origin.x));
    CHECK(first.Clips()[i].origin.y ==
          doctest::Approx(second.Clips()[i].origin.y));
    CHECK(first.Clips()[i].size.x ==
          doctest::Approx(second.Clips()[i].size.x));
    CHECK(first.Clips()[i].size.y ==
          doctest::Approx(second.Clips()[i].size.y));
  }
}

TEST_CASE("[Unit] UI draw list append and finalize never reallocate buffers") {
  // R4 (C-01): the steady frame path is allocation-free. Appending and
  // finalizing must not change the reserved buffers' addresses or capacities
  // (std::sort is in-place and allocation-free by the standard; the text arena
  // is a fixed block written via memcpy). A pointer/capacity change here would
  // mean a hot-path reallocation.
  UiDrawList list;
  list.Reserve(32);
  list.ReserveText(256);

  const UiDrawCommand *commandsPtr = list.Commands().data();
  const std::size_t commandCapacity = list.CommandCapacity();
  const std::size_t clipCapacity = list.ClipCapacity();
  const std::size_t textCapacity = list.TextCapacity();

  for (int frame = 0; frame < 16; ++frame) {
    list.Text(UiDrawLayer::Hud, 1, "frame text", {0.0f, 0.0f}, 20.0f, {});
    list.FillRect(UiDrawLayer::Panels, 2, MakeRect(0.0f, 0.0f, 10.0f, 10.0f),
                  {});
    list.PushClip(MakeRect(0.0f, 0.0f, 800.0f, 600.0f));
    list.Custom(UiDrawLayer::Modal, 3, MakeRect(1.0f, 1.0f, 2.0f, 2.0f), 9);
    list.PopClip();
    list.Finalize();

    CHECK(list.Commands().data() == commandsPtr);
    CHECK(list.CommandCapacity() == commandCapacity);
    CHECK(list.ClipCapacity() == clipCapacity);
    CHECK(list.TextCapacity() == textCapacity);
    CHECK(list.CommandOverflow() == 0);
    CHECK(list.ClipOverflow() == 0);
    CHECK(list.TextOverflow() == 0);

    list.Clear();
  }
}

TEST_CASE("[Unit] UI draw list command overflow records telemetry and drops") {
  // Tiny explicit capacities drive the overflow path (Reserve is grow-only).
  UiDrawList list(2, 2, 8);
  list.FillRect(UiDrawLayer::Hud, 1, MakeRect(0.0f, 0.0f, 1.0f, 1.0f), {});
  list.FillRect(UiDrawLayer::Hud, 2, MakeRect(0.0f, 0.0f, 1.0f, 1.0f), {});
  list.FillRect(UiDrawLayer::Hud, 3, MakeRect(0.0f, 0.0f, 1.0f, 1.0f), {});

  CHECK(list.CommandCount() == 2);
  CHECK(list.CommandCapacity() == 2);
  CHECK(list.CommandOverflow() == 1);
  // The retained commands are the first two (dropped command never appended).
  CHECK(list.Commands()[0].nodeId == 1);
  CHECK(list.Commands()[1].nodeId == 2);
}

TEST_CASE("[Unit] UI draw list clip overflow records telemetry without growing") {
  // Tiny explicit capacities drive the overflow path (Reserve is grow-only).
  UiDrawList list(2, 2, 8);
  list.PushClip(MakeRect(0.0f, 0.0f, 100.0f, 100.0f));
  list.PushClip(MakeRect(200.0f, 200.0f, 50.0f, 50.0f));
  list.PushClip(MakeRect(400.0f, 400.0f, 25.0f, 25.0f));  // overflow

  CHECK(list.ClipOverflow() == 1);
  CHECK(list.ClipCapacity() == 2);
  // Commands inside the overflowed clip clamp to the last valid clip.
  list.FillRect(UiDrawLayer::Hud, 1, MakeRect(0.0f, 0.0f, 1.0f, 1.0f), {});
  CHECK(list.Commands()[0].clipIndex == 1);

  // Depth stays balanced: pops unwind without imbalance.
  list.PopClip();
  list.PopClip();
  list.PopClip();
  CHECK(list.ClipBalanced());
}

TEST_CASE("[Unit] UI draw list text overflow records telemetry and drops") {
  UiDrawList list;
  list.ReserveText(8);
  list.Text(UiDrawLayer::Hud, 1, "0123456789", {0.0f, 0.0f}, 20.0f, {});

  CHECK(list.TextOverflow() == 1);
  CHECK(list.TextCapacity() == 8);
  CHECK(list.TextBytesUsed() == 0);
  // The command survives with an empty text payload; nothing is drawn.
  CHECK(list.CommandCount() == 1);
  CHECK(list.Commands()[0].textLength == 0);
  CHECK(std::string(list.TextAt(list.Commands()[0])).empty());

  // A payload that fits is still stored.
  list.Text(UiDrawLayer::Hud, 2, "abc", {0.0f, 0.0f}, 20.0f, {});
  CHECK(list.TextBytesUsed() == 3);
  CHECK(std::string(list.TextAt(list.Commands()[1])) == "abc");
}

TEST_CASE("[Unit] UI runtime types color defaults to opaque white") {
  const UiColor color;
  CHECK(color.r == 255);
  CHECK(color.g == 255);
  CHECK(color.b == 255);
  CHECK(color.a == 255);

  const UiColor tinted{10, 20, 30, 40};
  CHECK(tinted.r == 10);
  CHECK(tinted.g == 20);
  CHECK(tinted.b == 30);
  CHECK(tinted.a == 40);
}

TEST_CASE("[Unit] UI draw list header excludes backend and gameplay deps") {
  std::ifstream source("src/game/application/ui/UiDrawList.hpp");
  REQUIRE(source.is_open());
  const std::string contents{std::istreambuf_iterator<char>(source),
                             std::istreambuf_iterator<char>()};

  CHECK(contents.find("raylib.h") == std::string::npos);
  CHECK(contents.find("entt/") == std::string::npos);
  CHECK(contents.find("UiShared") == std::string::npos);
  CHECK(contents.find("InventorySystem") == std::string::npos);
  // C-01: no per-frame std::string payloads in the command struct (text lives
  // in the draw list's fixed text arena, referenced by offset+length).
  CHECK(contents.find("std::string text") == std::string::npos);
  CHECK(contents.find("std::string_view text") != std::string::npos);
}

} // namespace NoMoreDay::ui
