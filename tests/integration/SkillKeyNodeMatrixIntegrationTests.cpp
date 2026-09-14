#include "SkillKeyNodeMatrixTestHelpers.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/foundation/components/AdvancedAffixComponents.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/Progression.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/systems/skill/components/PersistentFieldComponents.hpp"
#include "game/foundation/components/TriggerRuleComponent.hpp"
#include "game/foundation/data/BladeMasteryRegistry.hpp"
#include "game/systems/skill/BladeMasteryService.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include <array>

namespace NoMoreDay {
namespace sknm = test::skill_keynode_matrix;

namespace test::skill_keynode_matrix::integration {

inline void InitContext(entt::registry &) {
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  CombatEventDispatcher::Clear();
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();
}

inline void RunTicks(entt::registry &registry, systems::SpatialHashGrid &grid,
                     int count, float dt) {
  for (int i = 0; i < count; ++i) {
    SkillSystem::Update(registry, grid, dt);
  }
}

inline bool HasArrayFlags(entt::registry &registry) {
  auto view = registry.view<SwordArrayComponent>();
  if (view.begin() == view.end()) {
    return false;
  }
  const auto ent = *view.begin();
  const auto &array = view.get<SwordArrayComponent>(ent);
  return array.has_execute && array.gain_intent_on_tick;
}

inline bool HasBoomerangProjectiles(entt::registry &registry, int min_count) {
  auto view = registry.view<Projectile, BoomerangComponent>();
  int count = 0;
  for (const auto entity : view) {
    (void)entity;
    ++count;
  }
  return count >= min_count;
}

} // namespace test::skill_keynode_matrix::integration

TEST_CASE("[Integration] SkillKeyNodeMatrix - Per-skill runtime scenarios (1..12)") {
  const auto fixture_nodes = sknm::LoadFixtureKeyNodes();
  REQUIRE(fixture_nodes.size() == sknm::MatrixSkillIds().size());

  for (const uint32_t skill_id : sknm::MatrixSkillIds()) {
    CAPTURE(skill_id);
    REQUIRE(fixture_nodes.contains(skill_id));

    entt::registry registry;
    test::skill_keynode_matrix::integration::InitContext(registry);
    systems::SpatialHashGrid grid(1024, 1024, 64);

    const auto caster = sknm::CreateCaster(registry, 2000.0f);
    const auto target = sknm::CreateTarget(registry, {120.0f, 0.0f});
    sknm::ConfigureSkillSlot(registry, caster, skill_id, 0, 2);
    sknm::ConfigureSpecialization(
        registry, caster, skill_id,
        sknm::AsAllocatedPoints(
            sknm::CastSmokeNodes(skill_id, fixture_nodes.at(skill_id)), 1));

    if (skill_id >= 10) {
      REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
          "assets/data/blade_masteries.json"));
      auto &stats = registry.get_or_emplace<PlayerStats>(caster);
      stats.level = 50;
      auto &astrolabe = registry.get_or_emplace<AstrolabeComponent>(caster);
      astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);
      systems::BladeMasteryService::RefreshPlayerState(registry, caster);
      const BladeMasteryId mastery_id = skill_id == 10
                                            ? BladeMasteryId::SwordSaint
                                            : (skill_id == 11 ? BladeMasteryId::HeavenlySword
                                                              : BladeMasteryId::DemonBlade);
      REQUIRE(systems::BladeMasteryService::SelectMastery(registry, caster,
                                                          mastery_id));
      if (skill_id == 10 || skill_id == 11) {
        REQUIRE(systems::BladeResourceService::Gain(registry, caster, 5, skill_id));
      } else {
        REQUIRE(systems::BladeResourceService::Gain(registry, caster, 6, skill_id));
      }

      if (skill_id == 11) {
        registry.get<BladeMasteryComponent>(caster).heavenly_attunement =
            BladeAttunement::Lightning;
      }
    }

    CHECK(SkillSystem::TryCast(registry, caster, 0, {120.0f, 0.0f}));
    test::skill_keynode_matrix::integration::RunTicks(registry, grid, 3, 0.08f);

    switch (skill_id) {
    case 1:
      {
        const bool has_phase = registry.any_of<PhaseTag>(caster);
        const bool has_swift = sknm::HasEffectById(
            registry, caster, BuffIdToString(BuffId::SwordStep));
        const bool has_signal = has_phase || has_swift;
        CHECK(has_signal);
      }
      break;
    case 2:
      {
        auto proj_view = registry.view<Projectile>();
        CHECK(proj_view.begin() != proj_view.end());
      }
      break;
    case 3:
      CHECK(registry.all_of<BladeFormationComponent>(caster));
      break;
    case 4:
      REQUIRE(registry.all_of<BladeWardComponent>(caster));
      CHECK(registry.get<BladeWardComponent>(caster).is_lightning_ward);
      break;
    case 5:
      REQUIRE(registry.all_of<BeamChannelComponent>(caster));
      CHECK(registry.get<BeamChannelComponent>(caster).skill_id == 5);
      CHECK(registry.get<BeamChannelComponent>(caster).mode ==
            BeamChannelMode::BarrageEmitter);
      break;
    case 6:
      CHECK(test::skill_keynode_matrix::integration::HasArrayFlags(registry));
      break;
    case 7:
      REQUIRE(registry.all_of<BeamChannelComponent>(caster));
      CHECK(registry.get<BeamChannelComponent>(caster).skill_id == 7u);
      break;
    case 8:
      // 冒烟配置不含 830 幻影回旋，故仅有主剑一柄弹体
      CHECK(test::skill_keynode_matrix::integration::HasBoomerangProjectiles(
          registry, 1));
      break;
    case 9:
      REQUIRE(registry.all_of<PhantomTranceComponent>(caster));
      {
        const auto &trance = registry.get<PhantomTranceComponent>(caster);
        CHECK(trance.remaining > 0.0f);
        CHECK(trance.params.duration_sec == doctest::Approx(3.0f));
        // 冒烟节点含 981 逆脉与 980 虚灵之躯
        CHECK(trance.params.death_seal);
        CHECK(trance.params.void_body);
      }
      break;
    case 10:
      CHECK(registry.any_of<InvulnerableComponent>(caster));
      break;
    case 11:
      {
        auto view = registry.view<HeavenlySwordFieldComponent>();
        REQUIRE(view.begin() != view.end());
        const auto field = *view.begin();
        const auto &fieldComp = view.get<HeavenlySwordFieldComponent>(field);
        CHECK(fieldComp.header.owner == caster);
        CHECK(fieldComp.attunement == BladeAttunement::Lightning);
        CHECK(fieldComp.spent_tiers > 0);
      }
      break;
    case 12:
      {
        auto view = registry.view<BloodSeaFieldComponent>();
        REQUIRE(view.begin() != view.end());
        const auto field = *view.begin();
        const auto &fieldComp = view.get<BloodSeaFieldComponent>(field);
        CHECK(fieldComp.header.owner == caster);
        CHECK(fieldComp.consumed_bloodthirst > 0);
        CHECK(registry.get<CombatStats>(caster).health <
              registry.get<CombatStats>(caster).max_health);
      }
      break;
    default:
      FAIL("Unexpected skill id in key-node matrix test");
      break;
    }

    (void)target;
  }
}

TEST_CASE("[Integration] SkillKeyNodeMatrix - Demon Blade node 1219 extends Blood Sea lifetime") {
  const auto remainingDurationWithNodes =
      [](const std::vector<std::pair<uint32_t, int>> &nodes) {
        entt::registry registry;
        test::skill_keynode_matrix::integration::InitContext(registry);
        systems::SpatialHashGrid grid(1024, 1024, 64);

        REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
            "assets/data/blade_masteries.json"));

        const auto caster = sknm::CreateCaster(registry, 2000.0f);
        auto &stats = registry.get_or_emplace<PlayerStats>(caster);
        stats.level = 50;
        auto &astrolabe = registry.get_or_emplace<AstrolabeComponent>(caster);
        astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);
        systems::BladeMasteryService::RefreshPlayerState(registry, caster);
        REQUIRE(systems::BladeMasteryService::SelectMastery(
            registry, caster, BladeMasteryId::DemonBlade));
        REQUIRE(systems::BladeResourceService::Gain(registry, caster, 6, 12));

        sknm::ConfigureSkillSlot(registry, caster, 12, 0, 2);
        sknm::ConfigureSpecialization(registry, caster, 12, nodes);

        REQUIRE(SkillSystem::TryCast(registry, caster, 0, {120.0f, 0.0f}));
        test::skill_keynode_matrix::integration::RunTicks(registry, grid, 2, 0.08f);

        auto view = registry.view<BloodSeaFieldComponent>();
        REQUIRE(view.begin() != view.end());
        return view.get<BloodSeaFieldComponent>(*view.begin()).header.duration;
      };

  const float baselineRemaining = remainingDurationWithNodes({});
  const float extendedRemaining = remainingDurationWithNodes({{1219, 1}});

  CHECK(extendedRemaining > baselineRemaining);
}

TEST_CASE("[Integration] SkillKeyNodeMatrix - Trigger chain matrix covers all trigger nodes") {
  const auto compact = sknm::LoadCompactContractBuckets();
  // 技能2 的 254 由行为层 DoCast 轨道处理（trigger_skill_id==0），不在引擎触发矩阵内。
  CHECK(compact.trigger_node_by_skill.size() ==
        sknm::EngineTriggerMatrixSkillIds().size());

  int scenario_count = 0;
  for (const auto &[skill_id, trigger_node] : compact.trigger_node_by_skill) {
    CAPTURE(skill_id);
    CAPTURE(trigger_node);
    REQUIRE(compact.trigger_skill_by_node.contains(trigger_node));
    const uint32_t trigger_skill = compact.trigger_skill_by_node.at(trigger_node);

    entt::registry registry;
    test::skill_keynode_matrix::integration::InitContext(registry);
    const auto caster = sknm::CreateCaster(registry, 1500.0f);
    const auto target = sknm::CreateTarget(registry, {30.0f, 0.0f});
    sknm::ConfigureSpecialization(registry, caster, skill_id,
                                  {{trigger_node, 1}});
    // 技能8 855 巨剑共鸣：需施法者已专精技能3节点330（巨剑术），且本次命中为暴击。
    const bool is_crit = (skill_id == 8u);
    if (is_crit) {
      sknm::ConfigureSpecialization(registry, caster, 3u, {{330, 1}}, 1);
    }
    // 935 逆命反噬需要逆脉（DeathSeal）窗口；同时显式烘培触发规则
    sknm::ConfigureTriggerSourcePrerequisites(registry, caster, skill_id);
    sknm::ConfigureSkillSlot(registry, caster, skill_id, 0, 1);
    SkillSystem::RebakeSkillProfiles(registry, caster);
    // 935 基础概率 0.2 会引入随机性，触发矩阵只验证链路，这里置为必触发
    if (auto *triggers = registry.try_get<TriggerRuleComponent>(caster)) {
      for (uint8_t i = 0; i < triggers->rule_count; ++i) {
        if (triggers->rules[i].rule_id == trigger_node &&
            triggers->rules[i].base_chance < 1.0f) {
          triggers->rules[i].base_chance = 1.0f;
        }
      }
    }

    // 513 天诛要求目标带满层命印：为技能 5 构造满层 FateMark 命印，
    // 构造须在配置专精之后、派发命中之前（与 unit 矩阵测试口径一致）。
    if (skill_id == 5u) {
      auto &vicEffects =
          registry.get_or_emplace<ActiveEffectsComponent>(target);
      BuffEffect mark{
          .id = "FateMark",
          .name = "Fate Mark",
          .type = BuffType::DefenseDown,
          .kind = BuffKind::FateMark,
          .duration = 5.0f,
          .remaining = 5.0f,
          .stacks = 5,
          .max_stacks = 5,
          .is_debuff = true};
      vicEffects.AddOrRefresh(mark);
    } else if (skill_id == 6u) {
      registry.emplace<KilledTag>(target, caster);
      registry.emplace<ExecutedTag>(target);
    }

    const auto before = registry.storage<SkillExecution>().size();
    sknm::DispatchSkillHit(registry, caster, target, skill_id,
                           static_cast<uint64_t>(7000 + skill_id),
                           Tag::Hit | Tag::Melee, is_crit);
    const auto after = registry.storage<SkillExecution>().size();
    CHECK(after > before);

    const auto *runtime =
        registry.try_get<SkillContractRuntimeComponent>(caster);
    REQUIRE(runtime != nullptr);
    CHECK(runtime->trigger_cooldowns.contains(trigger_node));

    bool found_triggered_skill = false;
    auto view = registry.view<SkillExecution>();
    for (const auto exec_entity : view) {
      const auto &exec = view.get<SkillExecution>(exec_entity);
      if (exec.skill_id == trigger_skill && exec.trigger_depth == 1) {
        found_triggered_skill = true;
        break;
      }
    }
    CHECK(found_triggered_skill);
    ++scenario_count;
  }

  CHECK(scenario_count ==
        static_cast<int>(sknm::EngineTriggerMatrixSkillIds().size()));
}

TEST_CASE("[Integration] SkillKeyNodeMatrix - Cross-skill and visual-signal guard matrix >= 12") {
  const auto compact = sknm::LoadCompactContractBuckets();
  int scenario_count = 0;

  for (const auto &[skill_id, trigger_node] : compact.trigger_node_by_skill) {
    entt::registry registry;
    test::skill_keynode_matrix::integration::InitContext(registry);
    const auto caster = sknm::CreateCaster(registry, 1200.0f);
    const auto target = sknm::CreateTarget(registry, {16.0f, 0.0f});
    sknm::ConfigureSpecialization(registry, caster, skill_id,
                                  {{trigger_node, 1}});
    // 技能8 855 巨剑共鸣：需施法者已专精技能3节点330且本次命中为暴击。
    const bool is_crit = (skill_id == 8u);
    if (is_crit) {
      sknm::ConfigureSpecialization(registry, caster, 3u, {{330, 1}}, 1);
    }
    // 935 逆命反噬需要逆脉（DeathSeal）窗口；同时显式烘培触发规则
    sknm::ConfigureTriggerSourcePrerequisites(registry, caster, skill_id);
    sknm::ConfigureSkillSlot(registry, caster, skill_id, 0, 1);
    SkillSystem::RebakeSkillProfiles(registry, caster);
    if (auto *triggers = registry.try_get<TriggerRuleComponent>(caster)) {
      for (uint8_t i = 0; i < triggers->rule_count; ++i) {
        if (triggers->rules[i].rule_id == trigger_node &&
            triggers->rules[i].base_chance < 1.0f) {
          triggers->rules[i].base_chance = 1.0f;
        }
      }
    }

    // 513 天诛要求目标带满层命印：为技能 5 构造满层 FateMark 命印，
    // 构造须在配置专精之后、派发命中之前（与 unit 矩阵测试口径一致）。
    if (skill_id == 5u) {
      auto &vicEffects =
          registry.get_or_emplace<ActiveEffectsComponent>(target);
      BuffEffect mark{
          .id = "FateMark",
          .name = "Fate Mark",
          .type = BuffType::DefenseDown,
          .kind = BuffKind::FateMark,
          .duration = 5.0f,
          .remaining = 5.0f,
          .stacks = 5,
          .max_stacks = 5,
          .is_debuff = true};
      vicEffects.AddOrRefresh(mark);
    } else if (skill_id == 6u) {
      registry.emplace<KilledTag>(target, caster);
      registry.emplace<ExecutedTag>(target);
    }

    const auto before = registry.storage<SkillExecution>().size();
    sknm::DispatchSkillHit(registry, caster, target, skill_id,
                           static_cast<uint64_t>(8200 + skill_id),
                           Tag::Hit | Tag::Melee, is_crit);
    const auto after = registry.storage<SkillExecution>().size();
    CHECK(after > before);
    ++scenario_count;
  }

  {
    entt::registry registry;
    test::skill_keynode_matrix::integration::InitContext(registry);
    systems::SpatialHashGrid grid(1024, 1024, 64);
    const auto caster = sknm::CreateCaster(registry, 1000.0f);
    sknm::ConfigureSkillSlot(registry, caster, 1, 0, 1);
    sknm::ConfigureSpecialization(registry, caster, 1, {{130, 1}});
    CHECK(SkillSystem::TryCast(registry, caster, 0, {80.0f, 0.0f}));
    test::skill_keynode_matrix::integration::RunTicks(registry, grid, 8, 0.08f);
    auto shadow_view = registry.view<ShadowComponent>();
    CHECK(shadow_view.begin() != shadow_view.end());
    ++scenario_count;
  }

  {
    entt::registry registry;
    test::skill_keynode_matrix::integration::InitContext(registry);
    systems::SpatialHashGrid grid(1024, 1024, 64);
    const auto caster = sknm::CreateCaster(registry, 1000.0f);
    sknm::ConfigureSkillSlot(registry, caster, 6, 0, 1);
    sknm::ConfigureSpecialization(registry, caster, 6,
                                  {{630, 1}, {633, 1}, {652, 1}, {670, 1}});
    CHECK(SkillSystem::TryCast(registry, caster, 0, {20.0f, 0.0f}));
    test::skill_keynode_matrix::integration::RunTicks(registry, grid, 10, 0.08f);
    CHECK(test::skill_keynode_matrix::integration::HasArrayFlags(registry));
    ++scenario_count;
  }

  {
    entt::registry registry;
    test::skill_keynode_matrix::integration::InitContext(registry);
    systems::SpatialHashGrid grid(1024, 1024, 64);
    const auto caster = sknm::CreateCaster(registry, 1000.0f);
    sknm::ConfigureSkillSlot(registry, caster, 8, 0, 1);
    sknm::ConfigureSpecialization(registry, caster, 8, {{876, 1}});
    // 生产链路里专精变更经由属性脏标记触发重烘培，测试需显式重烘培
    SkillSystem::RebakeSkillProfiles(registry, caster);
    CHECK(SkillSystem::TryCast(registry, caster, 0, {120.0f, 0.0f}));
    test::skill_keynode_matrix::integration::RunTicks(registry, grid, 8, 0.08f);
    // 876 元素护体：守护语义烘培为元素绝对减伤比例，运行期由护卫管线消费
    const auto *profile = SkillSystem::GetBakedSkillProfile(registry, caster, 8u);
    REQUIRE(profile != nullptr);
    CHECK(profile->delivery.element_shield_pct > 0.0f);
    ++scenario_count;
  }

  CHECK(scenario_count >= 12);
}

// 834 御剑接踵消费侧：接刃获得的 FreeCast 让来源技能下次施放免蓝且只生效一次
TEST_CASE("[Integration] SkillKeyNodeMatrix - 834 free cast is consumed once") {
  entt::registry registry;
  test::skill_keynode_matrix::integration::InitContext(registry);

  const auto caster = sknm::CreateCaster(registry, 500.0f);
  sknm::ConfigureSkillSlot(registry, caster, 8, 0, 3);
  sknm::ConfigureSpecialization(registry, caster, 8, {{834, 3}});
  SkillSystem::RebakeSkillProfiles(registry, caster);

  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(caster);
  BuffEffect freeCast;
  freeCast.id = "test_free_cast";
  freeCast.kind = BuffKind::FreeCast;
  freeCast.source_skill_id = 8u;
  freeCast.duration = 5.0f;
  freeCast.remaining = 5.0f;
  effects.AddOrRefresh(freeCast);
  REQUIRE(effects.GetByKind(BuffKind::FreeCast) != nullptr);

  const float mana_before = registry.get<CombatStats>(caster).mana;
  REQUIRE(SkillSystem::TryCast(registry, caster, 0, {20.0f, 0.0f}));
  CHECK(registry.get<CombatStats>(caster).mana == doctest::Approx(mana_before));
  CHECK(effects.GetByKind(BuffKind::FreeCast) == nullptr);

  // 对照组：无 FreeCast 时施放正常消耗法力
  const auto control = sknm::CreateCaster(registry, 500.0f);
  sknm::ConfigureSkillSlot(registry, control, 8, 0, 3);
  sknm::ConfigureSpecialization(registry, control, 8, {{834, 3}});
  SkillSystem::RebakeSkillProfiles(registry, control);

  const float control_mana_before = registry.get<CombatStats>(control).mana;
  REQUIRE(SkillSystem::TryCast(registry, control, 0, {20.0f, 0.0f}));
  CHECK(registry.get<CombatStats>(control).mana < control_mana_before);
}

} // namespace NoMoreDay
