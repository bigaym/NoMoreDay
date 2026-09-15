#include "TestCommon.hpp"

#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/MonsterAffixRegistry.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"
#include "game/systems/modifier/MonsterModifierAdapter.hpp"

#include <array>
#include <cstdint>

// 同进程内其它单测会向全局 ModifierRuntimeRegistry 注入合成数据，
// 这里用共享助手从构建产物强制重新加载，保证本文件用例消费真实生成数据。

TEST_CASE("[Unit] MonsterModifierAdapter - affix list resolves stat deltas") {
  REQUIRE(ReloadModifierRuntimeFromAsset());

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
  REQUIRE(ReloadModifierRuntimeFromAsset());

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
  REQUIRE(ReloadModifierRuntimeFromAsset());

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
  REQUIRE(ReloadModifierRuntimeFromAsset());

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

  NoMoreDay::MonsterAffixComponent updateBehaviorOpsOnly;
  updateBehaviorOpsOnly.AddAffix(NoMoreDay::MonsterAffixType::ManaSiphon);
  updateBehaviorOpsOnly.AddAffix(NoMoreDay::MonsterAffixType::Shielding);
  updateBehaviorOpsOnly.AddAffix(NoMoreDay::MonsterAffixType::Vortex);
  updateBehaviorOpsOnly.AddAffix(NoMoreDay::MonsterAffixType::Waller);

  const auto updateBehaviorOps =
      NoMoreDay::MonsterModifierAdapter::EvaluateBehaviorOps(updateBehaviorOpsOnly);

  CHECK(updateBehaviorOps.HasOnUpdateOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_MANA_SIPHON_UPDATE));
  CHECK(updateBehaviorOps.HasOnUpdateOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SHIELDING_UPDATE));
  CHECK(updateBehaviorOps.HasOnUpdateOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VORTEX_UPDATE));
  CHECK(updateBehaviorOps.HasOnUpdateOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_WALLER_UPDATE));

  NoMoreDay::MonsterAffixComponent suppressorAndSoulLink;
  suppressorAndSoulLink.AddAffix(NoMoreDay::MonsterAffixType::Suppressor);
  suppressorAndSoulLink.AddAffix(NoMoreDay::MonsterAffixType::SoulLink);

  const auto suppressorAndSoulLinkOps =
      NoMoreDay::MonsterModifierAdapter::EvaluateBehaviorOps(
          suppressorAndSoulLink);

  CHECK(suppressorAndSoulLinkOps.HasOnUpdateOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SUPPRESSOR_UPDATE));
  CHECK(suppressorAndSoulLinkOps.HasOnUpdateOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SOUL_LINK_UPDATE));

  NoMoreDay::MonsterAffixComponent berserkerVoidZoneAndStorm;
  berserkerVoidZoneAndStorm.AddAffix(NoMoreDay::MonsterAffixType::Berserker);
  berserkerVoidZoneAndStorm.AddAffix(NoMoreDay::MonsterAffixType::VoidZone);
  berserkerVoidZoneAndStorm.AddAffix(NoMoreDay::MonsterAffixType::Storm);

  const auto berserkerAndVoidZoneOps =
      NoMoreDay::MonsterModifierAdapter::EvaluateBehaviorOps(
          berserkerVoidZoneAndStorm);

  CHECK(berserkerAndVoidZoneOps.HasOnUpdateOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_BERSERKER_UPDATE));
  CHECK(berserkerAndVoidZoneOps.HasOnUpdateOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VOIDZONE_UPDATE));
  CHECK(berserkerAndVoidZoneOps.HasOnUpdateOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_STORM_UPDATE));

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

  NoMoreDay::MonsterAffixComponent voidOnly;
  voidOnly.AddAffix(NoMoreDay::MonsterAffixType::Void);

  const auto voidOnlyOps =
      NoMoreDay::MonsterModifierAdapter::EvaluateBehaviorOps(voidOnly);

  CHECK(voidOnlyOps.HasOnHitOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VOID_ON_HIT));

  NoMoreDay::MonsterAffixComponent toxicOnly;
  toxicOnly.AddAffix(NoMoreDay::MonsterAffixType::Toxic);
  toxicOnly.AddAffix(NoMoreDay::MonsterAffixType::SoulEater);
  toxicOnly.AddAffix(NoMoreDay::MonsterAffixType::Avenger);

  const auto toxicOps =
      NoMoreDay::MonsterModifierAdapter::EvaluateBehaviorOps(toxicOnly);

  CHECK(toxicOps.HasOnDeathOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_TOXIC_ON_DEATH));
  CHECK(toxicOps.HasOnDeathOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_SOUL_EATER_ON_ENEMY_DEATH));
  CHECK(toxicOps.HasOnDeathOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_AVENGER_ON_NEARBY_DEATH));
}

TEST_CASE("[Unit] MonsterModifierAdapter - suppresses vampiric stat life-steal when behavior op exists") {
  REQUIRE(ReloadModifierRuntimeFromAsset());

  NoMoreDay::MonsterAffixComponent affixComponent;
  affixComponent.AddAffix(NoMoreDay::MonsterAffixType::Vampiric);

  const auto delta = NoMoreDay::MonsterModifierAdapter::EvaluateAffixDelta(
      affixComponent);

  const auto lifeStealStat =
      static_cast<uint32_t>(NoMoreDay::StatType::LifeSteal);
  CHECK(delta.flat.find(lifeStealStat) == delta.flat.end());

  // 属性被抑制的同时，行为与事件 op 仍必须保留，吸血只由行为系统计一次。
  CHECK(delta.monster_behavior_on_hit_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT)));

  const auto events =
      NoMoreDay::MonsterModifierAdapter::EvaluateAffixEvents(affixComponent);
  CHECK(events.onHitAffixIds.contains(
      static_cast<uint32_t>(NoMoreDay::MonsterAffixType::Vampiric)));

  const auto behaviorOps =
      NoMoreDay::MonsterModifierAdapter::EvaluateBehaviorOps(affixComponent);
  CHECK(behaviorOps.HasOnHitOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT));
}

TEST_CASE("[Unit] MonsterModifierAdapter - three entry points expose disjoint field groups") {
  REQUIRE(ReloadModifierRuntimeFromAsset());

  NoMoreDay::MonsterAffixComponent affixComponent;
  affixComponent.AddAffix(NoMoreDay::MonsterAffixType::Molten);
  affixComponent.AddAffix(NoMoreDay::MonsterAffixType::Vampiric);

  const auto delta = NoMoreDay::MonsterModifierAdapter::EvaluateAffixDelta(
      affixComponent);
  // EvaluateAffixDelta = 属性 + 行为，不暴露任何事件集合。
  CHECK(delta.monster_event_on_update_affix_ids.empty());
  CHECK(delta.monster_event_on_hit_affix_ids.empty());
  CHECK(delta.monster_event_on_death_affix_ids.empty());
  CHECK(delta.monster_behavior_on_update_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_MOLTEN_UPDATE)));
  CHECK(delta.monster_behavior_on_hit_opcodes.contains(static_cast<uint16_t>(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT)));
  // 掩码实现下窄入口只插入本组：Molten 无 onDeath 行为，该组必须为空。
  CHECK(delta.monster_behavior_on_death_opcodes.empty());

  const auto events =
      NoMoreDay::MonsterModifierAdapter::EvaluateAffixEvents(affixComponent);
  CHECK(events.onUpdateAffixIds.contains(
      static_cast<uint32_t>(NoMoreDay::MonsterAffixType::Molten)));
  CHECK(events.onHitAffixIds.contains(
      static_cast<uint32_t>(NoMoreDay::MonsterAffixType::Vampiric)));
  // 事件入口恰好产出这两个词缀，不含其它组。
  CHECK(events.onUpdateAffixIds.size() == 1);
  CHECK(events.onHitAffixIds.size() == 1);
  CHECK(events.onDeathAffixIds.empty());

  const auto behaviorOps =
      NoMoreDay::MonsterModifierAdapter::EvaluateBehaviorOps(affixComponent);
  CHECK(behaviorOps.HasOnUpdateOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_MOLTEN_UPDATE));
  CHECK(behaviorOps.HasOnHitOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT));
  CHECK_FALSE(behaviorOps.HasOnDeath());
  CHECK(behaviorOps.onUpdateOpcodes.size() == 1);
  CHECK(behaviorOps.onHitOpcodes.size() == 1);
  CHECK(behaviorOps.onDeathOpcodes.empty());
}

TEST_CASE("[Unit] MonsterModifierAdapter - emits behavior ops for Storm/Void") {
  REQUIRE(ReloadModifierRuntimeFromAsset());

  NoMoreDay::MonsterAffixComponent affixComponent;
  affixComponent.AddAffix(NoMoreDay::MonsterAffixType::Storm);
  affixComponent.AddAffix(NoMoreDay::MonsterAffixType::Void);

  const auto behaviorOps =
      NoMoreDay::MonsterModifierAdapter::EvaluateBehaviorOps(affixComponent);

  CHECK(behaviorOps.HasOnUpdateOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_STORM_UPDATE));
  CHECK(behaviorOps.HasOnHitOpcode(
      NoMoreDay::ModifierOpCode::MONSTER_BEHAVIOR_VOID_ON_HIT));
  CHECK(behaviorOps.onDeathOpcodes.empty());
}

TEST_CASE("[Unit] MonsterModifierAdapter - behavior opcode contract covers implemented and behavior-less affixes") {
  REQUIRE(ReloadModifierRuntimeFromAsset());

  struct BehaviorContractRow {
    NoMoreDay::MonsterAffixType affixType;
    bool expectsBehaviorOps;
    const char *name;
  };

  static constexpr std::array<BehaviorContractRow, 25> kRows = {{
      {NoMoreDay::MonsterAffixType::Fast, false, "Fast"},
      {NoMoreDay::MonsterAffixType::Tanky, false, "Tanky"},
      {NoMoreDay::MonsterAffixType::Powerful, false, "Powerful"},
      {NoMoreDay::MonsterAffixType::Accurate, false, "Accurate"},
      {NoMoreDay::MonsterAffixType::Molten, true, "Molten"},
      {NoMoreDay::MonsterAffixType::Frozen, true, "Frozen"},
      {NoMoreDay::MonsterAffixType::Storm, true, "Storm"},
      {NoMoreDay::MonsterAffixType::Toxic, true, "Toxic"},
      {NoMoreDay::MonsterAffixType::Void, true, "Void"},
      {NoMoreDay::MonsterAffixType::VoidZone, true, "VoidZone"},
      {NoMoreDay::MonsterAffixType::StormStrider, true, "StormStrider"},
      {NoMoreDay::MonsterAffixType::Teleporter, true, "Teleporter"},
      {NoMoreDay::MonsterAffixType::Nullifier, true, "Nullifier"},
      {NoMoreDay::MonsterAffixType::Shielding, true, "Shielding"},
      {NoMoreDay::MonsterAffixType::Waller, true, "Waller"},
      {NoMoreDay::MonsterAffixType::Vampiric, true, "Vampiric"},
      {NoMoreDay::MonsterAffixType::Berserker, true, "Berserker"},
      {NoMoreDay::MonsterAffixType::Vortex, true, "Vortex"},
      {NoMoreDay::MonsterAffixType::Entangler, true, "Entangler"},
      {NoMoreDay::MonsterAffixType::Avenger, true, "Avenger"},
      {NoMoreDay::MonsterAffixType::SoulLink, true, "SoulLink"},
      {NoMoreDay::MonsterAffixType::MirrorImage, true, "MirrorImage"},
      {NoMoreDay::MonsterAffixType::SoulEater, true, "SoulEater"},
      {NoMoreDay::MonsterAffixType::Suppressor, true, "Suppressor"},
      {NoMoreDay::MonsterAffixType::ManaSiphon, true, "ManaSiphon"},
  }};

  for (const auto &row : kRows) {
    NoMoreDay::MonsterAffixComponent affixComponent;
    affixComponent.AddAffix(row.affixType);

    const auto behaviorOps =
        NoMoreDay::MonsterModifierAdapter::EvaluateBehaviorOps(affixComponent);
    const bool hasAnyBehaviorOps = behaviorOps.HasOnUpdate() ||
                                   behaviorOps.HasOnHit() ||
                                   behaviorOps.HasOnDeath();

    CHECK_MESSAGE(hasAnyBehaviorOps == row.expectsBehaviorOps,
                  row.name << " behavior opcode contract mismatch");
  }
}
