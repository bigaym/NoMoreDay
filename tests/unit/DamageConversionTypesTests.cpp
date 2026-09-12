#include "doctest.h"

#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/TagRegistry.hpp"
#include "game/systems/combat/damage/DamageConversion.hpp"
#include "game/systems/combat/damage/DamageTypes.hpp"

#include <array>

namespace NoMoreDay {
namespace {

using damage::ApplyConversion;
using damage::ConversionOutput;
using damage::ConversionRule;
using damage::kElementCount;
using damage::TransformTagsOnConversion;

// 元素池索引（Tag 位序，DamageConversion.hpp 契约）：
//   0=Physical, 1=Fire, 2=Cold, 3=Lightning, 4=Shadow, 5=Poison
// 注意与 DamageType 枚举序（4=Poison、5=Shadow）在 4/5 位互换。
constexpr int kPoolFire = 1;
constexpr int kPoolCold = 2;
constexpr int kPoolShadow = 4;
constexpr int kPoolPoison = 5;

} // namespace

// B1/N3：ElementTagOf 必须按池位序映射，禁止 1ULL << 枚举值的裸位移。
TEST_CASE("[Unit] DamageConversionTypes - ElementTagOf respects pool order") {
  CHECK(damage::ElementTagOf(DamageType::Physical) == Tag::Physical);
  CHECK(damage::ElementTagOf(DamageType::Fire) == Tag::Fire);
  CHECK(damage::ElementTagOf(DamageType::Cold) == Tag::Cold);
  CHECK(damage::ElementTagOf(DamageType::Lightning) == Tag::Lightning);
  // 关键回归：DamageType::Shadow(5) 必须得到 Tag::Shadow(1<<4)，而非 Tag::Poison(1<<5)。
  CHECK(damage::ElementTagOf(DamageType::Shadow) == Tag::Shadow);
  CHECK(damage::ElementTagOf(DamageType::Poison) == Tag::Poison);
  // 非元素类型不产生元素标签。
  CHECK(damage::ElementTagOf(DamageType::True) == Tag::None);
  CHECK(damage::ElementTagOf(DamageType::Count) == Tag::None);
}

// B1：转换标签剥离全部旧元素标签，只保留目标元素标签与非元素动作标签。
TEST_CASE("[Unit] DamageConversionTypes - TransformTagsOnConversion Shadow/Poison") {
  const Tag action_tags = Tag::Hit | Tag::Projectile;

  const Tag to_shadow = TransformTagsOnConversion(
      Tag::Fire | Tag::Cold | action_tags, DamageType::Shadow);
  CHECK((to_shadow & Tag::Shadow) != Tag::None);
  CHECK((to_shadow & Tag::Poison) == Tag::None); // 反向错误映射回归
  CHECK((to_shadow & Tag::Fire) == Tag::None);
  CHECK((to_shadow & Tag::Cold) == Tag::None);
  CHECK((to_shadow & action_tags) == action_tags);

  // 反向：Shadow -> Poison 同样不得残留 Shadow。
  const Tag to_poison =
      TransformTagsOnConversion(Tag::Shadow | action_tags, DamageType::Poison);
  CHECK((to_poison & Tag::Poison) != Tag::None);
  CHECK((to_poison & Tag::Shadow) == Tag::None);
  CHECK((to_poison & action_tags) == action_tags);
}

// B1：ApplyConversion 数组按池索引对齐，规则字段为真实 DamageType。
TEST_CASE("[Unit] DamageConversionTypes - ApplyConversion uses pool-index arrays") {
  std::array<float, kElementCount> values{};
  std::array<Tag, kElementCount> tags{};
  // 与管线契约一致：每个元素池都携带非元素动作标签（combined_hit_tags）。
  for (int i = 0; i < kElementCount; ++i) {
    tags[static_cast<size_t>(i)] = static_cast<Tag>(1ULL << i) | Tag::Hit;
  }
  values[kPoolShadow] = 100.0f; // 池 4 = Shadow

  // Shadow(枚举 5) -> Poison(枚举 4)：落点必须是池 5，不是池 1<<4。
  const ConversionRule rules[] = {
      {DamageType::Shadow, DamageType::Poison, 1.0f, false}};
  const ConversionOutput out = ApplyConversion(values, tags, rules);

  CHECK(out.values[kPoolShadow] == doctest::Approx(0.0f));
  CHECK(out.values[kPoolPoison] == doctest::Approx(100.0f));
  CHECK((out.tags[kPoolPoison] & Tag::Poison) != Tag::None);
  CHECK((out.tags[kPoolPoison] & Tag::Shadow) == Tag::None);
  // 非元素动作标签必须保留。
  CHECK((out.tags[kPoolPoison] & Tag::Hit) != Tag::None);

  // 逆向：Poison(池 5) -> Shadow(池 4)，50% 转换，守恒。
  std::array<float, kElementCount> poison_values{};
  std::array<Tag, kElementCount> poison_tags{};
  poison_values[kPoolPoison] = 100.0f;
  poison_tags[kPoolPoison] = Tag::Poison;
  const ConversionRule inverse[] = {
      {DamageType::Poison, DamageType::Shadow, 0.5f, false}};
  const ConversionOutput inv_out =
      ApplyConversion(poison_values, poison_tags, inverse);

  CHECK(inv_out.values[kPoolPoison] == doctest::Approx(50.0f));
  CHECK(inv_out.values[kPoolShadow] == doctest::Approx(50.0f));
  CHECK((inv_out.tags[kPoolShadow] & Tag::Shadow) != Tag::None);
  CHECK(inv_out.values[kPoolPoison] + inv_out.values[kPoolShadow] ==
        doctest::Approx(100.0f));
}

// S2：转换无单向顺序限制，Cold->Fire 与 Fire->Cold 都按同一单遍算法结算。
TEST_CASE("[Unit] DamageConversionTypes - reverse direction conversion allowed") {
  // 正向 Cold -> Fire 25%。
  std::array<float, kElementCount> cold_values{};
  std::array<Tag, kElementCount> cold_tags{};
  cold_values[kPoolCold] = 100.0f;
  cold_tags[kPoolCold] = Tag::Cold;
  const ConversionRule cold_to_fire[] = {
      {DamageType::Cold, DamageType::Fire, 0.25f, false}};
  const ConversionOutput forward =
      ApplyConversion(cold_values, cold_tags, cold_to_fire);

  CHECK(forward.values[kPoolCold] == doctest::Approx(75.0f));
  CHECK(forward.values[kPoolFire] == doctest::Approx(25.0f));
  CHECK(forward.values[kPoolCold] + forward.values[kPoolFire] ==
        doctest::Approx(100.0f));

  // 逆向 Fire -> Cold 100%（旧 IsValidConversion 会丢弃该规则）。
  std::array<float, kElementCount> fire_values{};
  std::array<Tag, kElementCount> fire_tags{};
  fire_values[kPoolFire] = 100.0f;
  fire_tags[kPoolFire] = Tag::Fire;
  const ConversionRule fire_to_cold[] = {
      {DamageType::Fire, DamageType::Cold, 1.0f, false}};
  const ConversionOutput reverse =
      ApplyConversion(fire_values, fire_tags, fire_to_cold);

  CHECK(reverse.values[kPoolFire] == doctest::Approx(0.0f));
  CHECK(reverse.values[kPoolCold] == doctest::Approx(100.0f));
  CHECK((reverse.tags[kPoolCold] & Tag::Cold) != Tag::None);
  CHECK(reverse.values[kPoolFire] + reverse.values[kPoolCold] ==
        doctest::Approx(100.0f));
}

} // namespace NoMoreDay
