#include "doctest.h"

#include "game/components/Stats.hpp"
#include "game/data/MonsterAffixRegistry.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"
#include "game/systems/modifier/MonsterModifierAdapter.hpp"

TEST_CASE("[Unit] MonsterModifierAdapter - affix list resolves stat deltas") {
  NoMoreDay::MonsterAffixComponent affixComponent;
  affixComponent.AddAffix(NoMoreDay::MonsterAffixType::Fast);
  affixComponent.AddAffix(NoMoreDay::MonsterAffixType::Tanky);

  const auto delta = NoMoreDay::MonsterModifierAdapter::EvaluateAffixDelta(
      affixComponent);

  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(
            40.0f, static_cast<uint32_t>(NoMoreDay::StatType::MoveSpeed),
            delta) == doctest::Approx(60.0f));
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(
            100.0f, static_cast<uint32_t>(NoMoreDay::StatType::AttackSpeed),
            delta) == doctest::Approx(130.0f));
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(
            100.0f, static_cast<uint32_t>(NoMoreDay::StatType::Armor),
            delta) == doctest::Approx(200.0f));
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(
            100.0f, static_cast<uint32_t>(NoMoreDay::StatType::MaxHealth),
            delta) == doctest::Approx(150.0f));
}

TEST_CASE("[Unit] MonsterModifierAdapter - berserk weapon multiplier remains affix-count based") {
  NoMoreDay::MonsterAffixComponent affixComponent;
  affixComponent.AddAffix(NoMoreDay::MonsterAffixType::Fast);
  affixComponent.AddAffix(NoMoreDay::MonsterAffixType::Berserker);

  affixComponent.isBerserk = false;
  CHECK(NoMoreDay::MonsterModifierAdapter::GetBerserkWeaponDamageMultiplier(
            affixComponent) == doctest::Approx(1.0f));

  affixComponent.isBerserk = true;
  CHECK(NoMoreDay::MonsterModifierAdapter::GetBerserkWeaponDamageMultiplier(
            affixComponent) == doctest::Approx(4.0f));
}

TEST_CASE("[Unit] MonsterModifierAdapter - evaluates affix event sets from registry flags") {
  NoMoreDay::MonsterAffixComponent affixComponent;
  affixComponent.AddAffix(NoMoreDay::MonsterAffixType::Molten);
  affixComponent.AddAffix(NoMoreDay::MonsterAffixType::Nullifier);
  affixComponent.AddAffix(NoMoreDay::MonsterAffixType::Toxic);

  const auto events =
      NoMoreDay::MonsterModifierAdapter::EvaluateAffixEvents(affixComponent);

  CHECK(events.onUpdateAffixIds.contains(
      static_cast<uint32_t>(NoMoreDay::MonsterAffixType::Molten)));
  CHECK(events.onHitAffixIds.contains(
      static_cast<uint32_t>(NoMoreDay::MonsterAffixType::Nullifier)));
  CHECK(events.onDeathAffixIds.contains(
      static_cast<uint32_t>(NoMoreDay::MonsterAffixType::Toxic)));
}

TEST_CASE("[Unit] MonsterModifierAdapter - evaluates behavior ops for monster behavior affixes") {
  NoMoreDay::MonsterAffixComponent updateAndVampiric;
  updateAndVampiric.AddAffix(NoMoreDay::MonsterAffixType::Molten);
  updateAndVampiric.AddAffix(NoMoreDay::MonsterAffixType::Teleporter);
  updateAndVampiric.AddAffix(NoMoreDay::MonsterAffixType::Frozen);
  updateAndVampiric.AddAffix(NoMoreDay::MonsterAffixType::Vampiric);

  const auto updateAndVampiricOps =
      NoMoreDay::MonsterModifierAdapter::EvaluateBehaviorOps(updateAndVampiric);

  CHECK(updateAndVampiricOps.HasOnUpdateOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_MOLTEN_UPDATE));
  CHECK(updateAndVampiricOps.HasOnUpdateOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_TELEPORTER_UPDATE));
  CHECK(updateAndVampiricOps.HasOnUpdateOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_FROZEN_UPDATE));
  CHECK(updateAndVampiricOps.HasOnHitOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT));

  NoMoreDay::MonsterAffixComponent nullifierAndEntangler;
  nullifierAndEntangler.AddAffix(NoMoreDay::MonsterAffixType::Nullifier);
  nullifierAndEntangler.AddAffix(NoMoreDay::MonsterAffixType::Entangler);
  nullifierAndEntangler.AddAffix(NoMoreDay::MonsterAffixType::MirrorImage);
  nullifierAndEntangler.AddAffix(NoMoreDay::MonsterAffixType::StormStrider);

  const auto nullifierAndEntanglerOps =
      NoMoreDay::MonsterModifierAdapter::EvaluateBehaviorOps(
          nullifierAndEntangler);

  CHECK(nullifierAndEntanglerOps.HasOnHitOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_NULLIFIER_ON_HIT));
  CHECK(nullifierAndEntanglerOps.HasOnHitOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_ENTANGLER_ON_HIT));
  CHECK(nullifierAndEntanglerOps.HasOnHitOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_MIRROR_IMAGE_ON_TAKE_DAMAGE));
  CHECK(nullifierAndEntanglerOps.HasOnHitOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_STORM_STRIDER_ON_TAKE_DAMAGE));

  NoMoreDay::MonsterAffixComponent toxicOnly;
  toxicOnly.AddAffix(NoMoreDay::MonsterAffixType::Toxic);
  toxicOnly.AddAffix(NoMoreDay::MonsterAffixType::SoulEater);

  const auto toxicOps =
      NoMoreDay::MonsterModifierAdapter::EvaluateBehaviorOps(toxicOnly);

  CHECK(toxicOps.HasOnDeathOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_TOXIC_ON_DEATH));
  CHECK(toxicOps.HasOnDeathOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SOUL_EATER_ON_ENEMY_DEATH));
}

TEST_CASE("[Unit] MonsterModifierAdapter - suppresses vampiric stat life-steal when behavior op exists") {
  NoMoreDay::MonsterAffixComponent affixComponent;
  affixComponent.AddAffix(NoMoreDay::MonsterAffixType::Vampiric);

  const auto delta = NoMoreDay::MonsterModifierAdapter::EvaluateAffixDelta(
      affixComponent);

  const auto lifeStealStat =
      static_cast<uint32_t>(NoMoreDay::StatType::LifeSteal);
  CHECK(delta.flat.find(lifeStealStat) == delta.flat.end());
}
