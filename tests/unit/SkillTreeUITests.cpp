#include "doctest.h"

#include "game/application/ui/UISkillTalentTree.hpp"
#include "game/application/ui/UISystem.hpp"

#include <cctype>
#include <fstream>
#include <iterator>
#include <string>

using NoMoreDay::SkillTreeUI;

TEST_CASE("[Unit] SkillTreeUI ComputeTooltipLayoutMetrics is pure") {
  // Layout constants (see UISkillTalentTree.cpp): descriptionTop = 108,
  // descriptionMinHeight = 56, footerTopPad = 12, footerLineHeight = 24,
  // footerGap = 12, footerBottomPad = 16.
  // quantitativeHeight = count == 0 ? 0 : 12 + 24*count (same for footer).
  // minimumHeight = 108 + 56 + quantitativeHeight + 12 + footerHeight + 16.

  // No quantitative/footer lines, generous height -> input wins.
  const auto metrics0 = SkillTreeUI::ComputeTooltipLayoutMetrics(280.0f, 0, 0);
  CHECK(metrics0.tooltipHeight == doctest::Approx(280.0f));
  CHECK(metrics0.descriptionTop == doctest::Approx(108.0f));
  CHECK(metrics0.footerGap == doctest::Approx(12.0f));
  CHECK(metrics0.quantitativeHeight == doctest::Approx(0.0f));
  CHECK(metrics0.footerHeight == doctest::Approx(0.0f));
  CHECK(metrics0.footerTop == doctest::Approx(280.0f - 16.0f));          // 264
  CHECK(metrics0.quantitativeTop == doctest::Approx(264.0f));
  CHECK(metrics0.descriptionHeight == doctest::Approx(264.0f - 12.0f - 108.0f)); // 144
  CHECK(metrics0.descriptionBottom == doctest::Approx(108.0f + 144.0f));         // 252

  // Height below the minimum -> clamped up to minimumHeight = 192.
  const auto metricsMin = SkillTreeUI::ComputeTooltipLayoutMetrics(100.0f, 0, 0);
  CHECK(metricsMin.tooltipHeight == doctest::Approx(192.0f));
  CHECK(metricsMin.footerTop == doctest::Approx(192.0f - 16.0f));        // 176
  CHECK(metricsMin.quantitativeTop == doctest::Approx(176.0f));
  CHECK(metricsMin.descriptionHeight == doctest::Approx(176.0f - 12.0f - 108.0f)); // 56
  CHECK(metricsMin.descriptionBottom == doctest::Approx(108.0f + 56.0f));          // 164

  // 2 quantitative + 1 footer line: minimum grows to 108+56+60+12+36+16 = 288.
  const auto metricsQ = SkillTreeUI::ComputeTooltipLayoutMetrics(280.0f, 2, 1);
  CHECK(metricsQ.quantitativeHeight == doctest::Approx(12.0f + 24.0f * 2.0f)); // 60
  CHECK(metricsQ.footerHeight == doctest::Approx(12.0f + 24.0f * 1.0f));       // 36
  CHECK(metricsQ.tooltipHeight == doctest::Approx(288.0f));
  CHECK(metricsQ.footerTop == doctest::Approx(288.0f - 16.0f - 36.0f)); // 236
  CHECK(metricsQ.quantitativeTop == doctest::Approx(236.0f - 60.0f));   // 176
  CHECK(metricsQ.descriptionHeight == doctest::Approx(176.0f - 12.0f - 108.0f)); // 56
  CHECK(metricsQ.descriptionBottom == doctest::Approx(164.0f));

  // Tall tooltip with 3 quantitative + 2 footer lines: input height wins.
  const auto metricsTall = SkillTreeUI::ComputeTooltipLayoutMetrics(500.0f, 3, 2);
  CHECK(metricsTall.tooltipHeight == doctest::Approx(500.0f));
  CHECK(metricsTall.quantitativeHeight == doctest::Approx(12.0f + 24.0f * 3.0f)); // 84
  CHECK(metricsTall.footerHeight == doctest::Approx(12.0f + 24.0f * 2.0f));       // 60
  CHECK(metricsTall.footerTop == doctest::Approx(500.0f - 16.0f - 60.0f)); // 424
  CHECK(metricsTall.quantitativeTop == doctest::Approx(424.0f - 84.0f));   // 340
  CHECK(metricsTall.descriptionHeight == doctest::Approx(340.0f - 12.0f - 108.0f)); // 220
  CHECK(metricsTall.descriptionBottom == doctest::Approx(108.0f + 220.0f));         // 328
}

TEST_CASE("[Unit] SkillTreeUI Draw is headless-safe") {
  // The test harness (tests/main.cpp) opens a hidden raylib window, but the
  // talent tree must still guard its early exits. skillTreeAlpha defaults to
  // 0 (UISystem::State), which makes Draw return before any GL call. Even
  // with alpha forced to 1, an unmapped skill id makes GetSkill return
  // nullptr, so Draw returns before touching the renderer or the registry.
  NoMoreDay::SkillTreeUI treeUI;
  entt::registry registry;
  const entt::entity player = registry.create();

  const float savedAlpha = UISystem::State.skillTreeAlpha;
  UISystem::State.skillTreeAlpha = 0.0f;
  treeUI.Draw(registry, player, NoMoreDay::INVALID_SKILL_ID);
  UISystem::State.skillTreeAlpha = 1.0f;
  treeUI.Draw(registry, player, NoMoreDay::INVALID_SKILL_ID);
  UISystem::State.skillTreeAlpha = savedAlpha;
}

TEST_CASE("[Unit] SkillTreeUI instances are independent") {
  // U7 cleanup: the talent tree is an instance type. Two instances must not
  // share mutable state (no static members allowed) and must both be drivable
  // with the same headless-safe inputs.
  NoMoreDay::SkillTreeUI a;
  NoMoreDay::SkillTreeUI b;
  entt::registry registry;
  const entt::entity player = registry.create();

  const float savedAlpha = UISystem::State.skillTreeAlpha;
  UISystem::State.skillTreeAlpha = 1.0f;
  a.Draw(registry, player, NoMoreDay::INVALID_SKILL_ID);
  b.Draw(registry, player, NoMoreDay::INVALID_SKILL_ID);
  UISystem::State.skillTreeAlpha = savedAlpha;
}

TEST_CASE("[Unit] SkillTreeUI header holds no static mutable state") {
  const std::string path = "src/game/application/ui/UISkillTalentTree.hpp";
  std::ifstream in(path);
  REQUIRE_MESSAGE(in.good(), "cannot open ", path);
  std::string contents((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());

  const std::string classNeedle = "class SkillTreeUI";
  std::string::size_type classPos = contents.find(classNeedle);
  REQUIRE_MESSAGE(classPos != std::string::npos, "class declaration not found");
  const std::string body = contents.substr(classPos);

  // Static data members are forbidden; only ComputeTooltipLayoutMetrics stays
  // static (a pure member function).
  for (const char* needle : {"static Vec2", "static float", "static uint32_t",
                             "static bool"}) {
    CHECK_MESSAGE(body.find(needle) == std::string::npos, needle,
                  " must not appear as a data member in SkillTreeUI");
  }

  const std::string needle = "static ";
  std::string::size_type pos = body.find(needle);
  while (pos != std::string::npos) {
    const char after = body[pos + needle.size()];
    // Pure static member functions are fine; static data members are not.
    CHECK_MESSAGE(
        std::isalpha(static_cast<unsigned char>(after)) != 0,
        "static data member must not exist in SkillTreeUI: '",
        body.substr(pos, body.find_first_of(";{}", pos) - pos + 1), "'");
    pos = body.find(needle, pos + needle.size());
  }
}
