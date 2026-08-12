#pragma once

#include "doctest.h"

#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/MonsterHealthBarController.hpp"
#include "game/application/ui/PlayerHudController.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiViewport.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <vector>

namespace NoMoreDay {

namespace {

ui::UiRect MakeRect(float x, float y, float w, float h) {
  return {{x, y}, {w, h}};
}

// Builds a stress-scale monster view-model: `count` damaged monsters spread
// across the 1920x1080 viewport so the health-bar batch stays under the
// controller's 256-bar cap (B-01/C-01 R5 evidence: bars are culled by the
// viewport and only damaged monsters emit commands).
std::vector<ui::GameUiMonsterHealthView> MakeMonsters(std::size_t count) {
  std::vector<ui::GameUiMonsterHealthView> monsters;
  monsters.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    ui::GameUiMonsterHealthView m{};
    m.domainId = 9000 + static_cast<std::uint64_t>(i);
    m.current = 50.0f + static_cast<float>(i % 50);
    m.max = 100.0f;
    m.isElite = (i % 5) == 0;
    // Spread across the world view so culling keeps most of them visible.
    m.worldX = 200.0f + static_cast<float>(i % 24) * 60.0f;
    m.worldY = 200.0f + static_cast<float>(i / 24) * 60.0f;
    m.raceType = static_cast<std::uint8_t>(i % 4);
    m.rarity = static_cast<std::uint8_t>(m.isElite ? 1 : 0);
    m.radius = 12.0f;
    m.affixCount = static_cast<std::uint8_t>(i % 5);
    for (std::uint8_t a = 0; a < m.affixCount && a < 4; ++a) {
      m.affixTypes[a] = static_cast<std::uint8_t>((i + a) % 16);
    }
    monsters.push_back(m);
  }
  return monsters;
}

// Builds the stress-scale summon rows (10 grouped summon keys, the upper bound
// of the R5 HUD widget; the controller copies into 16 fixed slots).
std::vector<ui::GameUiSummonGroupView> MakeSummons(std::size_t count) {
  std::vector<ui::GameUiSummonGroupView> groups;
  groups.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    ui::GameUiSummonGroupView g{};
    g.key = static_cast<std::uint32_t>(i + 1);
    g.skillId = static_cast<std::uint32_t>(i + 1);
    g.archetypeId = static_cast<std::uint32_t>(i % 3);
    g.iconId = static_cast<std::uint32_t>(10 + i);
    g.count = static_cast<std::uint32_t>(1 + i % 3);
    g.maxLifeRatio = 0.4f + static_cast<float>(i % 5) * 0.1f;
    groups.push_back(g);
  }
  return groups;
}

} // namespace

// R9 (H-03 evidence): draw-list steady-state at the host stress scale. The
// host reserves 1024 commands / 16384 text bytes; this benchmark drives 256
// commands + ~16KB of text per frame through Finalize and asserts the reserved
// buffers never move, grow, or overflow across 16 frames (C-01 evidence: the
// steady frame path is allocation-free).
TEST_CASE("[Performance] UiDrawList - steady state at stress scale "
          "(256 commands / 16384 text bytes)") {
  constexpr int kCommandCount = 256;
  constexpr int kTextBytes = 16384;

  ui::UiDrawList list;
  list.Reserve(kCommandCount);
  list.ReserveText(kTextBytes);

  const ui::UiDrawCommand *commandsPtr = list.Commands().data();
  const std::size_t commandCapacity = list.CommandCapacity();
  const std::size_t clipCapacity = list.ClipCapacity();
  const std::size_t textCapacity = list.TextCapacity();

  auto start = std::chrono::high_resolution_clock::now();
  for (int frame = 0; frame < 16; ++frame) {
    list.PushClip(MakeRect(0.0f, 0.0f, 1920.0f, 1080.0f));
    for (int i = 0; i < kCommandCount; ++i) {
      const float fx = static_cast<float>((i * 7) % 1920);
      const float fy = static_cast<float>((i * 13) % 1080);
      switch (i % 6) {
      case 0:
        list.FillRect(ui::UiDrawLayer::Panels, static_cast<ui::UiId>(i),
                      MakeRect(fx, fy, 40.0f, 20.0f), {});
        break;
      case 1:
        list.StrokeRect(ui::UiDrawLayer::Panels, static_cast<ui::UiId>(i),
                        MakeRect(fx, fy, 60.0f, 30.0f), {}, 2.0f);
        break;
      case 2:
        list.Line(ui::UiDrawLayer::Panels, static_cast<ui::UiId>(i),
                  {fx, fy}, {fx + 50.0f, fy + 25.0f}, {});
        break;
      case 3:
        list.Text(ui::UiDrawLayer::Panels, static_cast<ui::UiId>(i),
                  "stress label text", {fx, fy}, 16.0f, {});
        break;
      case 4:
        list.Image(ui::UiDrawLayer::Panels, static_cast<ui::UiId>(i),
                   MakeRect(fx, fy, 32.0f, 32.0f), 5, {});
        break;
      default:
        list.Custom(ui::UiDrawLayer::Panels, static_cast<ui::UiId>(i),
                    MakeRect(fx, fy, 24.0f, 24.0f), 9);
        break;
      }
    }
    list.PopClip();
    list.Finalize();

    // C-01 gate: buffers must never move/grow on the steady frame path.
    CHECK(list.Commands().data() == commandsPtr);
    CHECK(list.CommandCapacity() == commandCapacity);
    CHECK(list.ClipCapacity() == clipCapacity);
    CHECK(list.TextCapacity() == textCapacity);
    CHECK(list.CommandOverflow() == 0);
    CHECK(list.ClipOverflow() == 0);
    CHECK(list.TextOverflow() == 0);
    CHECK(list.ClipBalanced());

    list.Clear();
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto total = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  printf("\n[Benchmark] UiDrawList steady state %d commands / %d text bytes x16 frames: "
         "%lld us total (avg %.2f us/frame, p50 stable)\n",
         kCommandCount, kTextBytes, (long long)total.count(),
         (double)total.count() / 16.0);
}

// R9 (H-03 evidence): full Update+Paint pipeline at the R0 stress scale
// (120 monsters / 10 summons) driven headless through the migrated
// controllers. Revision flips and panel visibility toggles exercise the
// host-per-frame paths; overflow telemetry stays zero (C-01 evidence: the
// paint path never reallocates or overflows the reserved draw list).
TEST_CASE("[Performance] UiDrawList - full Update+Paint pipeline stress "
          "(120 monsters / 10 summons)") {
  constexpr int kMonsterCount = 120;
  constexpr int kSummonCount = 10;
  constexpr int kFrames = 240;

  ui::UiRuntime runtime;
  ui::UiViewport viewport = ui::UiViewport::Fit({1920.0f, 1080.0f});

  ui::MonsterHealthBarController monsterBars(runtime);
  ui::PlayerHudController playerHud(runtime);
  monsterBars.EnterGameplay();
  playerHud.EnterGameplay();

  ui::GameUiSnapshot snapshot;
  snapshot.revision = 1;
  snapshot.player.hasPlayer = true;
  snapshot.player.health = 75.0f;
  snapshot.player.maxHealth = 100.0f;
  snapshot.player.mana = 50.0f;
  snapshot.player.maxMana = 100.0f;
  snapshot.player.hasWorldPosition = true;
  snapshot.player.worldX = 0.0f;
  snapshot.player.worldY = 0.0f;
  snapshot.monsters = MakeMonsters(kMonsterCount);
  snapshot.player.summonGroups = MakeSummons(kSummonCount);

  ui::UiDrawList drawList;
  drawList.Reserve(1024);
  drawList.ReserveText(16384);

  std::vector<std::uint64_t> frameUs;
  frameUs.reserve(kFrames);
  std::size_t peakCommands = 0;
  std::size_t peakTextBytes = 0;

  auto start = std::chrono::high_resolution_clock::now();
  for (int frame = 0; frame < kFrames; ++frame) {
    // Every 60th frame flips the revision (forces HUD text rebuild) and
    // toggles panel visibility (R5/R8 host paths: EnterGameplay toggles).
    if ((frame % 60) == 0) {
      ++snapshot.revision;
      playerHud.SetVisible((frame / 60) % 2 == 0);
    }

    auto fStart = std::chrono::high_resolution_clock::now();
    monsterBars.Update(snapshot, 0.0f, 0.0f, 960.0f, 540.0f, 1.0f,
                       960.0f, 540.0f, 1920, 1080);
    playerHud.Update(snapshot, 60, static_cast<float>(frame) / 60.0f);
    drawList.Clear();
    monsterBars.Paint(drawList, viewport);
    playerHud.Paint(drawList, viewport);
    drawList.Finalize();
    auto fEnd = std::chrono::high_resolution_clock::now();
    frameUs.push_back(
        static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(fEnd - fStart)
                .count()));

    // R0 capacity gate: host-scale reservations must hold at this stress.
    CHECK(drawList.CommandOverflow() == 0);
    CHECK(drawList.TextOverflow() == 0);
    CHECK(drawList.ClipOverflow() == 0);
    CHECK(drawList.ClipBalanced());
    peakCommands = std::max(peakCommands, drawList.CommandCount());
    peakTextBytes = std::max(peakTextBytes, drawList.TextBytesUsed());
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto total = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  // p95 of the per-frame Update+Paint cost (H-03 CPU p95 comparison).
  std::sort(frameUs.begin(), frameUs.end());
  const std::uint64_t p50 = frameUs[kFrames / 2];
  const std::uint64_t p95 = frameUs[static_cast<std::size_t>(kFrames * 0.95)];
  const std::uint64_t p99 = frameUs[static_cast<std::size_t>(kFrames * 0.99)];
  printf("\n[Benchmark] Update+Paint pipeline 120 monsters / 10 summons x%d frames: "
         "%lld us total (avg %.2f us/frame, p50 %llu us, p95 %llu us, p99 %llu us) "
         "| commands/frame peak %zu, text bytes peak %zu\n",
         kFrames, (long long)total.count(), (double)total.count() / kFrames,
         (unsigned long long)p50, (unsigned long long)p95,
         (unsigned long long)p99, peakCommands, peakTextBytes);

  monsterBars.LeaveGameplay();
  playerHud.LeaveGameplay();
}

} // namespace NoMoreDay
