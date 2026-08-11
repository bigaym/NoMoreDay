#include "game/application/ui/UiRuntime.hpp"

#include "doctest.h"

namespace NoMoreDay::ui {
namespace {

constexpr float kEpsilon = 0.001f;

UiNodeDesc MakeNode(UiId id) {
  UiNodeDesc desc;
  desc.id = id;
  return desc;
}

} // namespace

TEST_CASE("[Unit] UI retained layout arranges row pixels fraction and padding") {
  UiRuntime runtime;
  UiLayoutStyle rootLayout;
  rootLayout.kind = UiLayoutKind::Row;
  rootLayout.padding = {10.0f, 5.0f, 10.0f, 5.0f};
  rootLayout.gap = 10.0f;
  runtime.SetRootLayout(rootLayout);

  UiNodeDesc fixed = MakeNode(2);
  fixed.layout.width = UiLength::Pixels(100.0f);
  fixed.layout.height = UiLength::Auto();
  fixed.layout.verticalAlignment = UiAlignment::Stretch;
  CHECK(runtime.CreateNode(fixed));

  UiNodeDesc fraction = MakeNode(3);
  fraction.layout.width = UiLength::Fraction(1.0f);
  fraction.layout.height = UiLength::Auto();
  fraction.layout.verticalAlignment = UiAlignment::Stretch;
  CHECK(runtime.CreateNode(fraction));

  runtime.Arrange({{0.0f, 0.0f}, {1000.0f, 100.0f}});

  const auto fixedNode = runtime.GetNode(2);
  const auto fractionNode = runtime.GetNode(3);
  REQUIRE(fixedNode.has_value());
  REQUIRE(fractionNode.has_value());
  CHECK(fixedNode->arrangedRect.origin.x == doctest::Approx(10.0f));
  CHECK(fixedNode->arrangedRect.origin.y == doctest::Approx(5.0f));
  CHECK(fixedNode->arrangedRect.size.x == doctest::Approx(100.0f));
  CHECK(fixedNode->arrangedRect.size.y == doctest::Approx(90.0f));
  CHECK(fractionNode->arrangedRect.origin.x == doctest::Approx(120.0f));
  CHECK(fractionNode->arrangedRect.size.x == doctest::Approx(870.0f));
  CHECK(fractionNode->arrangedRect.size.y == doctest::Approx(90.0f));
}

TEST_CASE("[Unit] UI retained layout clamps sizes and honors anchors") {
  UiRuntime runtime;
  UiLayoutStyle rootLayout;
  rootLayout.kind = UiLayoutKind::Anchor;
  runtime.SetRootLayout(rootLayout);

  UiNodeDesc anchored = MakeNode(2);
  anchored.layout.width = UiLength::Pixels(1200.0f);
  anchored.layout.height = UiLength::Pixels(30.0f);
  anchored.layout.maxSize.x = 400.0f;
  anchored.layout.minSize.y = 50.0f;
  anchored.layout.anchor.left = true;
  anchored.layout.anchor.leftOffset = 20.0f;
  anchored.layout.anchor.top = true;
  anchored.layout.anchor.topOffset = 10.0f;
  CHECK(runtime.CreateNode(anchored));

  UiNodeDesc stretched = MakeNode(3);
  stretched.layout.anchor.left = true;
  stretched.layout.anchor.leftOffset = 20.0f;
  stretched.layout.anchor.right = true;
  stretched.layout.anchor.rightOffset = 30.0f;
  stretched.layout.anchor.top = true;
  stretched.layout.anchor.topOffset = 60.0f;
  stretched.layout.height = UiLength::Pixels(20.0f);
  CHECK(runtime.CreateNode(stretched));

  runtime.Arrange({{0.0f, 0.0f}, {1000.0f, 200.0f}});

  const auto anchoredNode = runtime.GetNode(2);
  const auto stretchedNode = runtime.GetNode(3);
  REQUIRE(anchoredNode.has_value());
  REQUIRE(stretchedNode.has_value());
  CHECK(anchoredNode->arrangedRect.origin.x == doctest::Approx(20.0f));
  CHECK(anchoredNode->arrangedRect.origin.y == doctest::Approx(10.0f));
  CHECK(anchoredNode->arrangedRect.size.x == doctest::Approx(400.0f));
  CHECK(anchoredNode->arrangedRect.size.y == doctest::Approx(50.0f));
  CHECK(stretchedNode->arrangedRect.origin.x == doctest::Approx(20.0f));
  CHECK(stretchedNode->arrangedRect.size.x == doctest::Approx(950.0f));
  CHECK(stretchedNode->arrangedRect.size.y == doctest::Approx(20.0f));
}

TEST_CASE("[Unit] UI retained layout is deterministic across arrangements") {
  UiRuntime runtime;
  UiLayoutStyle rootLayout;
  rootLayout.kind = UiLayoutKind::Column;
  rootLayout.gap = 5.0f;
  runtime.SetRootLayout(rootLayout);

  UiNodeDesc first = MakeNode(2);
  first.layout.height = UiLength::Pixels(40.0f);
  first.layout.horizontalAlignment = UiAlignment::Stretch;
  CHECK(runtime.CreateNode(first));

  UiNodeDesc second = MakeNode(3);
  second.layout.height = UiLength::Fraction(1.0f);
  second.layout.horizontalAlignment = UiAlignment::Stretch;
  CHECK(runtime.CreateNode(second));

  const UiRect root{{2.0f, 3.0f}, {200.0f, 120.0f}};
  runtime.Arrange(root);
  const auto firstNode = runtime.GetNode(2);
  const auto secondNode = runtime.GetNode(3);
  REQUIRE(firstNode.has_value());
  REQUIRE(secondNode.has_value());
  const UiRect firstPass = firstNode->arrangedRect;
  const UiRect secondPass = secondNode->arrangedRect;
  runtime.Arrange(root);
  const auto firstAfter = runtime.GetNode(2);
  const auto secondAfter = runtime.GetNode(3);
  REQUIRE(firstAfter.has_value());
  REQUIRE(secondAfter.has_value());

  CHECK(firstAfter->arrangedRect.origin.x ==
        doctest::Approx(firstPass.origin.x).epsilon(kEpsilon));
  CHECK(firstAfter->arrangedRect.origin.y ==
        doctest::Approx(firstPass.origin.y).epsilon(kEpsilon));
  CHECK(firstAfter->arrangedRect.size.x ==
        doctest::Approx(firstPass.size.x).epsilon(kEpsilon));
  CHECK(firstAfter->arrangedRect.size.y ==
        doctest::Approx(firstPass.size.y).epsilon(kEpsilon));
  CHECK(secondAfter->arrangedRect.origin.x ==
        doctest::Approx(secondPass.origin.x).epsilon(kEpsilon));
  CHECK(secondAfter->arrangedRect.origin.y ==
        doctest::Approx(secondPass.origin.y).epsilon(kEpsilon));
  CHECK(secondAfter->arrangedRect.size.x ==
        doctest::Approx(secondPass.size.x).epsilon(kEpsilon));
  CHECK(secondAfter->arrangedRect.size.y ==
        doctest::Approx(secondPass.size.y).epsilon(kEpsilon));
}

TEST_CASE("[Unit] UI retained layout keeps oversized padding inside its parent") {
  UiRuntime runtime;
  UiLayoutStyle rootLayout;
  rootLayout.kind = UiLayoutKind::Overlay;
  rootLayout.padding = {120.0f, 80.0f, 120.0f, 80.0f};
  runtime.SetRootLayout(rootLayout);

  UiNodeDesc child = MakeNode(2);
  child.layout.horizontalAlignment = UiAlignment::Stretch;
  child.layout.verticalAlignment = UiAlignment::Stretch;
  CHECK(runtime.CreateNode(child));

  runtime.Arrange({{10.0f, 20.0f}, {100.0f, 60.0f}});

  const auto childNode = runtime.GetNode(2);
  REQUIRE(childNode.has_value());
  CHECK(childNode->arrangedRect.origin.x == doctest::Approx(110.0f));
  CHECK(childNode->arrangedRect.origin.y == doctest::Approx(80.0f));
  CHECK(childNode->arrangedRect.size.x == doctest::Approx(0.0f));
  CHECK(childNode->arrangedRect.size.y == doctest::Approx(0.0f));
}

TEST_CASE("[Unit] UI retained layout reserves fraction margins") {
  UiRuntime runtime;
  UiLayoutStyle rootLayout;
  rootLayout.kind = UiLayoutKind::Row;
  runtime.SetRootLayout(rootLayout);

  UiNodeDesc fraction = MakeNode(2);
  fraction.layout.width = UiLength::Fraction(1.0f);
  fraction.layout.margin = {10.0f, 0.0f, 10.0f, 0.0f};
  fraction.layout.verticalAlignment = UiAlignment::Stretch;
  CHECK(runtime.CreateNode(fraction));

  runtime.Arrange({{0.0f, 0.0f}, {100.0f, 20.0f}});

  const auto fractionNode = runtime.GetNode(2);
  REQUIRE(fractionNode.has_value());
  CHECK(fractionNode->arrangedRect.origin.x == doctest::Approx(10.0f));
  CHECK(fractionNode->arrangedRect.size.x == doctest::Approx(80.0f));
}

TEST_CASE("[Unit] UI retained layout measures auto containers from children") {
  UiRuntime runtime;

  UiNodeDesc container = MakeNode(2);
  container.layout.padding = {5.0f, 5.0f, 5.0f, 5.0f};
  CHECK(runtime.CreateNode(container));

  UiNodeDesc child = MakeNode(3);
  child.parent = 2;
  child.layout.width = UiLength::Pixels(80.0f);
  child.layout.height = UiLength::Pixels(30.0f);
  CHECK(runtime.CreateNode(child));

  runtime.Arrange({{0.0f, 0.0f}, {200.0f, 100.0f}});

  const auto containerNode = runtime.GetNode(2);
  const auto childNode = runtime.GetNode(3);
  REQUIRE(containerNode.has_value());
  REQUIRE(childNode.has_value());
  CHECK(containerNode->measuredSize.x == doctest::Approx(90.0f));
  CHECK(containerNode->measuredSize.y == doctest::Approx(40.0f));
  CHECK(containerNode->arrangedRect.size.x == doctest::Approx(90.0f));
  CHECK(containerNode->arrangedRect.size.y == doctest::Approx(40.0f));
  CHECK(childNode->arrangedRect.origin.x == doctest::Approx(5.0f));
  CHECK(childNode->arrangedRect.origin.y == doctest::Approx(5.0f));
}

} // namespace NoMoreDay::ui
