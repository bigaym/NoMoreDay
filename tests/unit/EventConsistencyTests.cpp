#include "TestCommon.hpp"
#include "game/components/Common.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include <unordered_map>
#include <vector>

namespace NoMoreDay {
namespace {

enum class HitEventKind : uint8_t { Melee, Projectile, Area };

struct EventCapture {
  std::vector<float> deal;
  std::vector<float> take;
  std::vector<float> melee;
  std::vector<float> projectile;
  std::vector<float> area;
  std::vector<float> crit;
};

using EventCaptureByTarget = std::unordered_map<uint32_t, EventCapture>;
using HandlerToken = std::pair<CombatEventType, uint32_t>;

uint32_t EntityKey(entt::entity entity) { return static_cast<uint32_t>(entity); }

CombatStats &CreateAttacker(entt::registry &registry, entt::entity attacker,
                            float critChance, float critDamage = 1.5f) {
  auto &stats = registry.emplace<CombatStats>(attacker);
  stats.crit_chance = critChance;
  stats.crit_damage = critDamage;
  stats.cached_area_level = 1;
  return stats;
}

entt::entity CreateTarget(entt::registry &registry, float healthValue) {
  const auto target = registry.create();
  registry.emplace<Position>(target, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(target, healthValue, healthValue);
  auto &stats = registry.emplace<CombatStats>(target);
  stats.cached_area_level = 1;
  return target;
}

class EventCaptureScope {
public:
  EventCaptureByTarget captures;

  EventCaptureScope() {
    tokens_.push_back(
        {CombatEventType::OnDealDamage,
         CombatEventDispatcher::Register(
             CombatEventType::OnDealDamage,
             [this](entt::registry &, const CombatEvent &evt) {
               captures[EntityKey(evt.target)].deal.push_back(evt.value);
             },
             1000)});
    tokens_.push_back(
        {CombatEventType::OnTakeDamage,
         CombatEventDispatcher::Register(
             CombatEventType::OnTakeDamage,
             [this](entt::registry &, const CombatEvent &evt) {
               captures[EntityKey(evt.source)].take.push_back(evt.value);
             },
             1000)});
    tokens_.push_back(
        {CombatEventType::OnMeleeHit,
         CombatEventDispatcher::Register(
             CombatEventType::OnMeleeHit,
             [this](entt::registry &, const CombatEvent &evt) {
               captures[EntityKey(evt.target)].melee.push_back(evt.value);
             },
             1000)});
    tokens_.push_back(
        {CombatEventType::OnProjectileHit,
         CombatEventDispatcher::Register(
             CombatEventType::OnProjectileHit,
             [this](entt::registry &, const CombatEvent &evt) {
               captures[EntityKey(evt.target)].projectile.push_back(evt.value);
             },
             1000)});
    tokens_.push_back(
        {CombatEventType::OnAreaHit,
         CombatEventDispatcher::Register(
             CombatEventType::OnAreaHit,
             [this](entt::registry &, const CombatEvent &evt) {
               captures[EntityKey(evt.target)].area.push_back(evt.value);
             },
             1000)});
    tokens_.push_back(
        {CombatEventType::OnCrit,
         CombatEventDispatcher::Register(
             CombatEventType::OnCrit,
             [this](entt::registry &, const CombatEvent &evt) {
               captures[EntityKey(evt.target)].crit.push_back(evt.value);
             },
             1000)});
  }

  ~EventCaptureScope() {
    for (const auto &[type, id] : tokens_) {
      CombatEventDispatcher::Unregister(type, id);
    }
  }

private:
  std::vector<HandlerToken> tokens_;
};

const EventCapture &CaptureFor(const EventCaptureByTarget &captures,
                               entt::entity target) {
  const auto it = captures.find(EntityKey(target));
  REQUIRE(it != captures.end());
  return it->second;
}

void ExpectSingleValue(const std::vector<float> &values, float expected) {
  REQUIRE(values.size() == 1);
  CHECK(values.front() == doctest::Approx(expected).epsilon(0.0001f));
}

void ExpectHitEvent(const EventCapture &capture, HitEventKind kind,
                    float expected) {
  switch (kind) {
  case HitEventKind::Melee:
    ExpectSingleValue(capture.melee, expected);
    CHECK(capture.projectile.empty());
    CHECK(capture.area.empty());
    break;
  case HitEventKind::Projectile:
    ExpectSingleValue(capture.projectile, expected);
    CHECK(capture.melee.empty());
    CHECK(capture.area.empty());
    break;
  case HitEventKind::Area:
    ExpectSingleValue(capture.area, expected);
    CHECK(capture.melee.empty());
    CHECK(capture.projectile.empty());
    break;
  }
}

} // namespace

TEST_CASE("[Unit] EventConsistency - single target event damage equals applied damage") {
  TestSetupScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  CreateAttacker(registry, attacker, 0.0f);
  const auto target = CreateTarget(registry, 300.0f);

  DamagePool basePool;
  basePool.Add(Tag::Physical, 64.0f);

  EventCaptureScope captureScope;

  const float hpBefore = registry.get<HealthComponent>(target).current;
  const std::vector<entt::entity> defenders = {target};
  DamagePipeline::CalculateBatch(registry, attacker, defenders, 940001, basePool,
                                 Tag::Melee);
  const float hpAfter = registry.get<HealthComponent>(target).current;
  const float appliedDamage = hpBefore - hpAfter;

  CHECK(appliedDamage > 0.0f);
  const auto &capture = CaptureFor(captureScope.captures, target);
  ExpectSingleValue(capture.deal, appliedDamage);
  ExpectSingleValue(capture.take, appliedDamage);
  ExpectSingleValue(capture.melee, appliedDamage);
  CHECK(capture.crit.empty());
  CHECK(capture.projectile.empty());
  CHECK(capture.area.empty());
}

TEST_CASE("[Unit] EventConsistency - batch events use final_damage for every target") {
  struct CaseItem {
    const char *name;
    Tag hitTag;
    HitEventKind kind;
  };

  const CaseItem cases[] = {
      {"melee", Tag::Melee, HitEventKind::Melee},
      {"projectile", Tag::Projectile, HitEventKind::Projectile},
      {"area", Tag::Area, HitEventKind::Area},
  };

  for (const auto &item : cases) {
    SUBCASE(item.name) {
      TestSetupScope scope;
      entt::registry registry;

      const auto attacker = registry.create();
      CreateAttacker(registry, attacker, 10000.0f, 2.0f);

      const auto normalTarget = CreateTarget(registry, 500.0f);
      const auto counterTarget = CreateTarget(registry, 500.0f);
      auto &pf = registry.emplace<PhantomFlashComponent>(counterTarget);
      pf.counter_window = 0.5f;
      pf.triggered = false;

      DamagePool basePool;
      basePool.Add(Tag::Physical, 80.0f);

      EventCaptureScope captureScope;

      const float normalBefore = registry.get<HealthComponent>(normalTarget).current;
      const float counterBefore =
          registry.get<HealthComponent>(counterTarget).current;
      const std::vector<entt::entity> defenders = {normalTarget, counterTarget};
      DamagePipeline::CalculateBatch(registry, attacker, defenders, 940002,
                                     basePool, item.hitTag);
      const float normalApplied =
          normalBefore - registry.get<HealthComponent>(normalTarget).current;
      const float counterApplied =
          counterBefore - registry.get<HealthComponent>(counterTarget).current;

      CHECK(normalApplied > 0.0f);
      CHECK(counterApplied == doctest::Approx(0.0f).epsilon(0.0001f));

      const auto &normalCapture = CaptureFor(captureScope.captures, normalTarget);
      ExpectSingleValue(normalCapture.deal, normalApplied);
      ExpectSingleValue(normalCapture.take, normalApplied);
      ExpectSingleValue(normalCapture.crit, normalApplied);
      ExpectHitEvent(normalCapture, item.kind, normalApplied);

      const auto &counterCapture =
          CaptureFor(captureScope.captures, counterTarget);
      ExpectSingleValue(counterCapture.deal, counterApplied);
      ExpectSingleValue(counterCapture.take, counterApplied);
      ExpectSingleValue(counterCapture.crit, counterApplied);
      ExpectHitEvent(counterCapture, item.kind, counterApplied);
    }
  }
}

TEST_CASE("[Unit] EventConsistency - mitigation path reports post-mitigation value") {
  TestSetupScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  CreateAttacker(registry, attacker, 0.0f);
  const auto target = CreateTarget(registry, 400.0f);
  auto &targetStats = registry.get<CombatStats>(target);
  targetStats.resistances[(int)DamageType::Fire] = 0.5f;
  targetStats.damage_reduction = 0.25f;

  DamagePool basePool;
  basePool.Add(Tag::Fire, 80.0f);

  DamageRequest request;
  request.attacker = attacker;
  request.defender = target;
  request.skill_id = 940003;
  request.base_pool = basePool;
  request.additional_tags = Tag::Area;
  request.is_simulation = true;
  const float expectedDamage = DamagePipeline::Calculate(registry, request).total_damage;

  EventCaptureScope captureScope;

  const float hpBefore = registry.get<HealthComponent>(target).current;
  const std::vector<entt::entity> defenders = {target};
  DamagePipeline::CalculateBatch(registry, attacker, defenders, request.skill_id,
                                 basePool, request.additional_tags);
  const float hpAfter = registry.get<HealthComponent>(target).current;
  const float appliedDamage = hpBefore - hpAfter;

  CHECK(appliedDamage == doctest::Approx(expectedDamage).epsilon(0.0001f));
  const auto &capture = CaptureFor(captureScope.captures, target);
  ExpectSingleValue(capture.deal, appliedDamage);
  ExpectSingleValue(capture.take, appliedDamage);
  ExpectSingleValue(capture.area, appliedDamage);
  CHECK(capture.crit.empty());
  CHECK(capture.melee.empty());
  CHECK(capture.projectile.empty());
}

} // namespace NoMoreDay
