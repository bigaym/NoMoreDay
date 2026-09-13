// 技能 12（血海）统一施法信封与节点功能测试（计划 A2-3 的 T10.1/T10.3）。
//
// 覆盖三部分：
// 1) 生产 ResolveBloodSeaCastSpec 的点读 / 点亮标志 / 机制系数映射（表驱动绑定表）；
// 2) 外置到 skill_mechanics.json 技能 12 的调平数值经施法写入
//    BloodSeaFieldComponent 的行为等价（default_value 与迁移前字面量逐一致）。
// 3) 绝影共噬（B2-22/A-03，节点 1217）：逆脉/免死窗口门控的前 2 秒增伤/增疗窗口；
//    联动脉冲改由节点 1207 无间血狱门控（设计 §5.3:1236），与 1217 解耦。

#include "TestCommon.hpp"
#include "SkillKeyNodeMatrixTestHelpers.hpp"

#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/BloodSea.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace NoMoreDay {
namespace {

constexpr uint32_t kSkillId = skills::kBloodSeaSkillId;
namespace Nodes = skills::BloodSeaNodes;

void EnsureSkillMechanics() {
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile(
      "assets/data/skill_mechanics.json"));
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
}

// 组装技能 12 施法者：血怒资源 + 血海专精 + 指定节点投入。
void ConfigureCaster(entt::registry &registry, entt::entity caster,
                     const std::unordered_map<uint32_t, int> &allocated_nodes = {},
                     int bloodthirst = 5) {
  auto &mastery = registry.emplace<BladeMasteryComponent>(caster);
  mastery.selected = BladeMasteryId::DemonBlade;
  mastery.blood_oath_active = true;

  auto &resource = registry.emplace<BladeResourceComponent>(caster);
  resource.kind = BladeResourceKind::Bloodthirst;
  resource.current = bloodthirst;
  resource.max = 10;

  std::vector<std::pair<uint32_t, int>> allocated;
  allocated.reserve(allocated_nodes.size());
  for (const auto &[node, points] : allocated_nodes) {
    allocated.emplace_back(node, points);
  }
  test::skill_keynode_matrix::ConfigureSpecialization(registry, caster, kSkillId,
                                                      allocated);
}

void CastBloodSea(entt::registry &registry, entt::entity caster,
                  Vector2 target_pos) {
  SkillExecution exec{};
  exec.skill_id = kSkillId;
  exec.owner = caster;
  exec.target_pos = target_pos;

  auto cast = SkillBehaviorRegistry::GetCast(kSkillId);
  REQUIRE(cast != nullptr);
  cast(registry, caster, exec);
}

const BloodSeaFieldComponent *FindField(const entt::registry &registry) {
  const auto view = registry.view<BloodSeaFieldComponent>();
  if (view.begin() == view.end()) {
    return nullptr;
  }
  return &view.get<BloodSeaFieldComponent>(*view.begin());
}

} // namespace

TEST_CASE("[Functional] Skill 12 - Cast spec resolves point and flag bindings") {
  entt::registry registry;
  const entt::entity owner = test::skill_keynode_matrix::CreateCaster(registry);
  ConfigureCaster(registry, owner,
                  {{Nodes::BloodCurtainOpening, 3},
                   {Nodes::PressureTideRise, 2},
                   {Nodes::LingeringBloodMist, 4},
                   {Nodes::MiasmaShred, 2},
                   {Nodes::BottomlessPurgatory, 1},
                   {Nodes::FreshBloodReturn, 1},
                   {Nodes::PhantomDevour, 1},
                   {Nodes::VoidErosionMiasma, 1},
                   {Nodes::CrimsonTorrent, 1}});

  const skills::BloodSeaCastSpec spec =
      skills::ResolveBloodSeaCastSpec(registry, owner);

  CHECK(spec.bloodCurtainOpeningPoints == 3);
  CHECK(spec.pressureTideRisePoints == 2);
  CHECK(spec.lingeringBloodMistPoints == 4);
  CHECK(spec.miasmaShredPoints == 2);
  CHECK(spec.bloodMistPursuitPoints == 0);
  CHECK(spec.bottomlessPurgatory == true);
  CHECK(spec.triggerBurst == true);
  CHECK(spec.sharedDevouring == true);
  CHECK(spec.voidKeystone == true);
  CHECK(spec.torrentForm == true);
  CHECK(spec.ringForm == false);
  CHECK(spec.recoveryKeystone == false);
  // 机制系数 default_value 与迁移前字面量一致（即使未加载数据也等价）。
  CHECK(spec.lowLifeThreshold == doctest::Approx(0.35f));
  CHECK(spec.radiusPerPoint == doctest::Approx(8.0f));
  CHECK(spec.resistShredPerPoint == doctest::Approx(2.0f));
  CHECK(spec.tickIntervalFloor == doctest::Approx(0.7f));
}

TEST_CASE("[Functional] Skill 12 - externalized mechanics match legacy literals") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  // 逐 key 断言外置值与迁移前字面量等价；技能级默认参数位于 node 0。
  CHECK(skills::GetMech(12, 1200, "radius_per_point", -1.0f) ==
        doctest::Approx(8.0f));
  CHECK(skills::GetMech(12, 1201, "damage_per_point", -1.0f) ==
        doctest::Approx(0.06f));
  CHECK(skills::GetMech(12, 1202, "damage_per_point_per_bloodthirst", -1.0f) ==
        doctest::Approx(0.025f));
  CHECK(skills::GetMech(12, 1203, "move_speed_per_point", -1.0f) ==
        doctest::Approx(1.0f));
  CHECK(skills::GetMech(12, 1204, "low_life_damage_per_point", -1.0f) ==
        doctest::Approx(0.08f));
  CHECK(skills::GetMech(12, 1205, "pursuit_damage_per_point", -1.0f) ==
        doctest::Approx(0.08f));
  CHECK(skills::GetMech(12, 1206, "low_life_pressure_damage_per_point", -1.0f) ==
        doctest::Approx(0.18f));
  CHECK(skills::GetMech(12, 1207, "damage_mult", -1.0f) ==
        doctest::Approx(1.1f));
  CHECK(skills::GetMech(12, 1208, "aftershock_damage_per_point", -1.0f) ==
        doctest::Approx(0.05f));
  CHECK(skills::GetMech(12, 1209, "leech_per_point_per_bloodthirst", -1.0f) ==
        doctest::Approx(0.01f));
  CHECK(skills::GetMech(12, 1210, "low_life_leech_per_point", -1.0f) ==
        doctest::Approx(0.025f));
  CHECK(skills::GetMech(12, 1211, "burst_base_damage", -1.0f) ==
        doctest::Approx(16.0f));
  CHECK(skills::GetMech(12, 1211, "burst_damage_per_bloodthirst", -1.0f) ==
        doctest::Approx(6.0f));
  CHECK(skills::GetMech(12, 1211, "missing_health_heal_ratio", -1.0f) ==
        doctest::Approx(0.1f));
  CHECK(skills::GetMech(12, 1211, "bloodthirst_gain", -1.0f) ==
        doctest::Approx(2.0f));
  CHECK(skills::GetMech(12, 1212, "leech_per_point", -1.0f) ==
        doctest::Approx(0.02f));
  CHECK(skills::GetMech(12, 1213, "leech_bonus", -1.0f) ==
        doctest::Approx(0.08f));
  CHECK(skills::GetMech(12, 1214, "empower_bonus_per_point", -1.0f) ==
        doctest::Approx(0.08f));
  CHECK(skills::GetMech(12, 1214, "return_empower_duration", -1.0f) ==
        doctest::Approx(1.5f));
  CHECK(skills::GetMech(12, 1215, "close_pressure_per_point", -1.0f) ==
        doctest::Approx(0.07f));
  CHECK(skills::GetMech(12, 1216, "tick_interval_reduction_per_point", -1.0f) ==
        doctest::Approx(0.06f));
  CHECK(skills::GetMech(12, 1216, "tick_interval_floor", -1.0f) ==
        doctest::Approx(0.7f));
  CHECK(skills::GetMech(12, 1217, "empower_duration", -1.0f) ==
        doctest::Approx(2.0f));
  CHECK(skills::GetMech(12, 1217, "empower_damage_mult", -1.0f) ==
        doctest::Approx(0.2f));
  CHECK(skills::GetMech(12, 1217, "empower_heal_mult", -1.0f) ==
        doctest::Approx(0.2f));
  CHECK(skills::GetMech(12, 1218, "linked_pressure_per_point", -1.0f) ==
        doctest::Approx(0.08f));
  CHECK(skills::GetMech(12, 1219, "duration_per_point", -1.0f) ==
        doctest::Approx(0.6f));
  CHECK(skills::GetMech(12, 1220, "damage_mult", -1.0f) ==
        doctest::Approx(1.18f));
  CHECK(skills::GetMech(12, 1220, "resist_shred_bonus", -1.0f) ==
        doctest::Approx(4.0f));
  CHECK(skills::GetMech(12, 1220, "physical_ratio", -1.0f) ==
        doctest::Approx(0.45f));
  CHECK(skills::GetMech(12, 1220, "pulse_damage_mult", -1.0f) ==
        doctest::Approx(1.08f));
  CHECK(skills::GetMech(12, 1221, "move_speed", -1.0f) ==
        doctest::Approx(14.0f));
  CHECK(skills::GetMech(12, 1221, "radius_mult", -1.0f) ==
        doctest::Approx(1.15f));
  CHECK(skills::GetMech(12, 1221, "tick_interval_mult", -1.0f) ==
        doctest::Approx(0.85f));
  CHECK(skills::GetMech(12, 1221, "linked_pulse_cooldown", -1.0f) ==
        doctest::Approx(0.12f));
  CHECK(skills::GetMech(12, 1222, "radius_mult", -1.0f) ==
        doctest::Approx(0.8f));
  CHECK(skills::GetMech(12, 1222, "leech_bonus", -1.0f) ==
        doctest::Approx(0.1f));
  CHECK(skills::GetMech(12, 1222, "damage_mult", -1.0f) ==
        doctest::Approx(1.15f));
  CHECK(skills::GetMech(12, 1222, "physical_ratio_delta", -1.0f) ==
        doctest::Approx(0.05f));
  CHECK(skills::GetMech(12, 1222, "pulse_damage_mult", -1.0f) ==
        doctest::Approx(1.1f));
  CHECK(skills::GetMech(12, 1223, "miasma_duration_per_point", -1.0f) ==
        doctest::Approx(0.25f));
  CHECK(skills::GetMech(12, 1223, "void_damage_per_point", -1.0f) ==
        doctest::Approx(0.06f));
  CHECK(skills::GetMech(12, 1224, "resist_shred_per_point", -1.0f) ==
        doctest::Approx(2.0f));
  CHECK(skills::GetMech(12, 0, "low_life_threshold", -1.0f) ==
        doctest::Approx(0.35f));
  CHECK(skills::GetMech(12, 0, "field_duration_per_bloodthirst", -1.0f) ==
        doctest::Approx(0.2f));
  CHECK(skills::GetMech(12, 0, "field_radius_per_bloodthirst", -1.0f) ==
        doctest::Approx(6.0f));
  // 技能级参数单源自 skills.json（contract）：原 mech 节点 0 同名键应已删除，
  // 缺失时 GetMech 返回调用方兜底值；实际数值改由 skills.json params 提供。
  CHECK(skills::GetMech(12, 0, "field_duration_default", -1.0f) ==
        doctest::Approx(-1.0f));
  CHECK(skills::GetMech(12, 0, "field_radius_default", -1.0f) ==
        doctest::Approx(-1.0f));
  CHECK(skills::GetMech(12, 0, "field_tick_default", -1.0f) ==
        doctest::Approx(-1.0f));
  CHECK(skills::GetMech(12, 0, "bloodthirst_damage_bonus_default", -1.0f) ==
        doctest::Approx(-1.0f));
  CHECK(skills::GetMech(12, 0, "leech_ratio_default", -1.0f) ==
        doctest::Approx(-1.0f));
  const auto *skill12 = SkillRegistry::Get().GetSkill(12);
  REQUIRE(skill12 != nullptr);
  CHECK(skill12->GetParam("field_duration", -1.0f) == doctest::Approx(4.8f));
  CHECK(skill12->GetParam("field_radius", -1.0f) == doctest::Approx(120.0f));
  CHECK(skill12->GetParam("field_tick", -1.0f) == doctest::Approx(0.25f));
  CHECK(skill12->GetParam("bloodthirst_damage_bonus", -1.0f) ==
        doctest::Approx(0.12f));
  CHECK(skill12->GetParam("leech_ratio", -1.0f) == doctest::Approx(0.12f));
  CHECK(skills::GetMech(12, 0, "physical_ratio_base", -1.0f) ==
        doctest::Approx(0.6f));
  CHECK(skills::GetMech(12, 0, "physical_ratio_min", -1.0f) ==
        doctest::Approx(0.25f));
  CHECK(skills::GetMech(12, 0, "physical_ratio_max", -1.0f) ==
        doctest::Approx(0.8f));
  CHECK(skills::GetMech(12, 0, "miasma_base_duration", -1.0f) ==
        doctest::Approx(1.0f));
  CHECK(skills::GetMech(12, 0, "pulse_base_damage", -1.0f) ==
        doctest::Approx(14.0f));
  CHECK(skills::GetMech(12, 0, "pulse_damage_per_bloodthirst", -1.0f) ==
        doctest::Approx(4.0f));
  CHECK(skills::GetMech(12, 0, "linked_pulse_base_damage", -1.0f) ==
        doctest::Approx(12.0f));
  CHECK(skills::GetMech(12, 0, "linked_pulse_damage_per_bloodthirst", -1.0f) ==
        doctest::Approx(2.0f));
  CHECK(skills::GetMech(12, 0, "linked_pulse_cooldown", -1.0f) ==
        doctest::Approx(0.2f));
}

TEST_CASE("[Functional] Skill 12 - externalized coefficients drive field on cast") {
  TestSetupScope setup;
  EnsureSkillMechanics();
  ProcBudgetManager::Get().ResetForTests();
  CombatEventDispatcher::Init();

  SUBCASE("BloodCurtainOpening and LingeringBloodMist scale radius and duration") {
    entt::registry registry;
    const entt::entity owner = test::skill_keynode_matrix::CreateCaster(registry);
    ConfigureCaster(registry, owner,
                    {{Nodes::BloodCurtainOpening, 3},
                     {Nodes::LingeringBloodMist, 4}});
    CastBloodSea(registry, owner, {0.0f, 0.0f});

    const auto *field = FindField(registry);
    REQUIRE(field != nullptr);
    // radius = 120 + 5*6 + 3*8 = 174；duration = 4.8 + 5*0.2 + 4*0.6 = 8.2。
    CHECK(field->header.radius == doctest::Approx(174.0f));
    CHECK(field->header.duration == doctest::Approx(8.2f));
  }

  SUBCASE("CrimsonTorrent and BloodRingDevour diverge in form") {
    {
      entt::registry registry;
      const entt::entity owner = test::skill_keynode_matrix::CreateCaster(registry);
      ConfigureCaster(registry, owner, {{Nodes::CrimsonTorrent, 1}});
      CastBloodSea(registry, owner, {0.0f, 0.0f});

      const auto *field = FindField(registry);
      REQUIRE(field != nullptr);
      CHECK(field->torrent_form);
      // radius = 150 * 1.15 = 172.5；tick = 0.25 * 0.85 = 0.2125；move = 14。
      CHECK(field->header.radius == doctest::Approx(172.5f));
      CHECK(field->header.tick_interval == doctest::Approx(0.2125f));
      CHECK(field->move_follow_speed == doctest::Approx(14.0f));
    }
    {
      entt::registry registry;
      const entt::entity owner = test::skill_keynode_matrix::CreateCaster(registry);
      ConfigureCaster(registry, owner, {{Nodes::BloodRingDevour, 1}});
      CastBloodSea(registry, owner, {0.0f, 0.0f});

      const auto *field = FindField(registry);
      REQUIRE(field != nullptr);
      CHECK(field->ring_form);
      // radius = 150 * 0.8 = 120；leech = 0.12 + 0.1 = 0.22；
      // bonus = (1 + 5*0.12) * 1.15 = 1.84。
      CHECK(field->header.radius == doctest::Approx(120.0f));
      CHECK(field->leech_ratio == doctest::Approx(0.22f));
      CHECK(field->bonus_damage_mult == doctest::Approx(1.84f));
    }
  }

  SUBCASE("MiasmaShred and VoidErosionMiasma raise resist shred") {
    entt::registry registry;
    const entt::entity owner = test::skill_keynode_matrix::CreateCaster(registry);
    ConfigureCaster(registry, owner,
                    {{Nodes::MiasmaShred, 2}, {Nodes::VoidErosionMiasma, 1}},
                    /*bloodthirst=*/3);
    CastBloodSea(registry, owner, {0.0f, 0.0f});

    const auto *field = FindField(registry);
    REQUIRE(field != nullptr);
    // resist_shred = 2*2 + 4 = 8；bonus = (1 + 3*0.12) * 1.18 = 1.6048。
    CHECK(field->resist_shred == doctest::Approx(8.0f));
    CHECK(field->has_void_keystone);
    CHECK(field->bonus_damage_mult == doctest::Approx(1.6048f));
  }

  SUBCASE("FreshBloodReturn burst uses externalized gain and heal values") {
    entt::registry registry;
    const entt::entity owner = test::skill_keynode_matrix::CreateCaster(registry);
    registry.get<CombatStats>(owner).max_health = 1000.0f;
    registry.get<CombatStats>(owner).health = 500.0f;
    registry.get<HealthComponent>(owner).max = 1000.0f;
    registry.get<HealthComponent>(owner).current = 500.0f;
    const entt::entity target =
        test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});
    ConfigureCaster(registry, owner, {{Nodes::FreshBloodReturn, 1}},
                    /*bloodthirst=*/4);
    CastBloodSea(registry, owner, {0.0f, 0.0f});

    const auto *field = FindField(registry);
    REQUIRE(field != nullptr);
    CHECK(field->has_trigger_burst);
    CHECK(field->pulses_triggered == 1);
    // ConsumeAll(4) 后鲜血回灌 +2 层。
    CHECK(registry.get<BladeResourceComponent>(owner).current == 2);
    // 10% * 500 缺失生命 = 50（后续脉冲吸血只会更高）。
    CHECK(registry.get<CombatStats>(owner).health >= doctest::Approx(550.0f));
    CHECK(registry.get<HealthComponent>(target).current < 1000.0f);
  }
}

TEST_CASE("[Functional] Skill 12 - node 1217 gated 2s empower window") {
  TestSetupScope setup;
  EnsureSkillMechanics();
  ProcBudgetManager::Get().ResetForTests();
  CombatEventDispatcher::Init();

  // 组装场景：has_1217 决定是否点亮 1217；trance_active 决定技能9 逆脉窗口是否存在。
  // 窗口存在性口径 = IsDeathSealActive（death_seal && remaining > 0 && !ending）。
  const auto configure = [](entt::registry &registry, entt::entity owner,
                            bool has_1217, bool trance_active) {
    if (has_1217) {
      ConfigureCaster(registry, owner, {{Nodes::PhantomDevour, 1}});
    } else {
      ConfigureCaster(registry, owner, {});
    }
    if (trance_active) {
      auto &trance = registry.emplace<PhantomTranceComponent>(owner);
      trance.params.death_seal = true;
      trance.remaining = 1.0f;
    }
  };

  SUBCASE("window is written only when 1217 is lit and the window is active") {
    struct Scenario {
      bool has_1217;
      bool trance_active;
      float expected_timer;
    };
    const Scenario scenarios[] = {
        {true, true, 2.0f}, {true, false, 0.0f}, {false, true, 0.0f}};
    for (const auto &scenario : scenarios) {
      entt::registry registry;
      const entt::entity owner =
          test::skill_keynode_matrix::CreateCaster(registry);
      configure(registry, owner, scenario.has_1217, scenario.trance_active);
      CastBloodSea(registry, owner, {0.0f, 0.0f});

      const auto *field = FindField(registry);
      REQUIRE(field != nullptr);
      CHECK(field->shared_devour_timer ==
            doctest::Approx(scenario.expected_timer));
      const float expected_mult =
          scenario.expected_timer > 0.0f ? 0.2f : 0.0f;
      CHECK(field->shared_devour_damage_mult == doctest::Approx(expected_mult));
      CHECK(field->shared_devour_heal_mult == doctest::Approx(expected_mult));
    }
  }

  SUBCASE("damage and leech are amplified in the window and expire with it") {
    // 直接驱动场更新（不经 SkillSystem），隔离 1217 窗口对脉冲伤害/吸血的影响。
    const auto run = [&configure](bool has_1217, float &out_damage,
                                  float &out_heal, float &out_timer_after) {
      entt::registry registry;
      systems::SpatialHashGrid grid(100, 100, 50);
      const entt::entity owner =
          test::skill_keynode_matrix::CreateCaster(registry);
      registry.get<CombatStats>(owner).max_health = 1000.0f;
      registry.get<CombatStats>(owner).health = 500.0f;
      registry.get<HealthComponent>(owner).max = 1000.0f;
      registry.get<HealthComponent>(owner).current = 500.0f;
      const entt::entity target =
          test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});
      configure(registry, owner, has_1217, /*trance_active=*/true);
      CastBloodSea(registry, owner, {0.0f, 0.0f});

      auto view = registry.view<BloodSeaFieldComponent>();
      REQUIRE(view.begin() != view.end());
      const entt::entity field_entity = *view.begin();
      auto &field = view.get<BloodSeaFieldComponent>(field_entity);

      // 第一跳：窗口内、逆脉仍激活 → 伤害被放大（治疗被逆脉禁疗拦截）。
      const float target_before = registry.get<HealthComponent>(target).current;
      skills::BloodSea::UpdateField(registry, field_entity, field, 0.1f, grid);
      out_damage = target_before - registry.get<HealthComponent>(target).current;

      // 逆脉窗口关闭（血海增伤/增疗窗口仍在）→ 第二跳观察治疗增益。
      registry.get<PhantomTranceComponent>(owner).remaining = 0.0f;
      const float owner_before = registry.get<CombatStats>(owner).health;
      skills::BloodSea::UpdateField(registry, field_entity, field, 0.3f, grid);
      out_heal = registry.get<CombatStats>(owner).health - owner_before;

      // 推进超过 2 秒窗口后归零，增益失效。
      skills::BloodSea::UpdateField(registry, field_entity, field, 2.0f, grid);
      out_timer_after = field.shared_devour_timer;
    };

    float damage_on = 0.0f;
    float heal_on = 0.0f;
    float timer_on = -1.0f;
    float damage_off = 0.0f;
    float heal_off = 0.0f;
    float timer_off = -1.0f;
    run(true, damage_on, heal_on, timer_on);
    run(false, damage_off, heal_off, timer_off);

    CHECK(damage_on > 0.0f);
    CHECK(damage_on > damage_off);
    CHECK(heal_on > heal_off);
    CHECK(timer_on == doctest::Approx(0.0f));
    CHECK(timer_off == doctest::Approx(0.0f));
  }

  SUBCASE("default 3s death seal window amplifies damage but blocks all healing") {
    // 默认逆脉/免死窗口为 3s（remaining=3.0 且未进入结束结算）。该状态下整个 1217
    // 增伤/增疗窗都被禁疗覆盖，因此脉冲伤害吃 +20%，而治疗被 ApplyHealing 归零。
    const auto run = [&configure](bool has_1217, float &out_damage,
                                  float &out_heal) {
      entt::registry registry;
      systems::SpatialHashGrid grid(100, 100, 50);
      const entt::entity owner =
          test::skill_keynode_matrix::CreateCaster(registry);
      registry.get<CombatStats>(owner).max_health = 1000.0f;
      registry.get<CombatStats>(owner).health = 500.0f;
      registry.get<HealthComponent>(owner).max = 1000.0f;
      registry.get<HealthComponent>(owner).current = 500.0f;
      const entt::entity target =
          test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});
      configure(registry, owner, has_1217, /*trance_active=*/true);
      auto &trance = registry.get<PhantomTranceComponent>(owner);
      trance.remaining = 3.0f;
      trance.ending = false;
      CastBloodSea(registry, owner, {0.0f, 0.0f});

      auto view = registry.view<BloodSeaFieldComponent>();
      REQUIRE(view.begin() != view.end());
      const entt::entity field_entity = *view.begin();
      auto &field = view.get<BloodSeaFieldComponent>(field_entity);

      const float target_before = registry.get<HealthComponent>(target).current;
      const float owner_before = registry.get<CombatStats>(owner).health;
      skills::BloodSea::UpdateField(registry, field_entity, field, 0.1f, grid);
      out_damage = target_before - registry.get<HealthComponent>(target).current;
      out_heal = registry.get<CombatStats>(owner).health - owner_before;
    };

    float damage_on = 0.0f;
    float heal_on = -1.0f;
    float damage_off = 0.0f;
    float heal_off = -1.0f;
    run(true, damage_on, heal_on);
    run(false, damage_off, heal_off);

    CHECK(damage_off > 0.0f);
    // 窗口内伤害脉冲吃 +20%。
    CHECK(damage_on == doctest::Approx(damage_off * 1.2f).epsilon(0.02));
    // 逆脉禁疗：有/无 1217 时实际治疗量均为 0，固化「增伤体现、增疗不体现」的交互。
    CHECK(heal_on == doctest::Approx(0.0f));
    CHECK(heal_off == doctest::Approx(0.0f));
  }

  SUBCASE("sub-2s death seal window lets the +20% heal amplifier land") {
    // 逆脉窗口仅剩 1s（< 2s 增疗窗）：逆脉先结束，2s 增疗窗仍有约 1s 余量。
    // 此时治疗不再被禁疗，单位伤害吸血量相对无 1217 基线提高 20%。
    const auto run = [&configure](bool has_1217, float &out_damage,
                                  float &out_heal) {
      entt::registry registry;
      systems::SpatialHashGrid grid(100, 100, 50);
      const entt::entity owner =
          test::skill_keynode_matrix::CreateCaster(registry);
      registry.get<CombatStats>(owner).max_health = 1000.0f;
      registry.get<CombatStats>(owner).health = 500.0f;
      registry.get<HealthComponent>(owner).max = 1000.0f;
      registry.get<HealthComponent>(owner).current = 500.0f;
      const entt::entity target =
          test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});
      configure(registry, owner, has_1217, /*trance_active=*/true);
      CastBloodSea(registry, owner, {0.0f, 0.0f});

      auto view = registry.view<BloodSeaFieldComponent>();
      REQUIRE(view.begin() != view.end());
      const entt::entity field_entity = *view.begin();
      auto &field = view.get<BloodSeaFieldComponent>(field_entity);

      // 推进 1s：逆脉禁疗拦截本跳治疗，同时把 2s 增疗窗递减到约 1s。
      skills::BloodSea::UpdateField(registry, field_entity, field, 1.0f, grid);
      registry.get<PhantomTranceComponent>(owner).remaining = 0.0f;
      if (has_1217) {
        REQUIRE(field.shared_devour_timer > 0.0f);
      }

      // 逆脉已结束、增疗窗仍激活的观测跳。
      const float target_before = registry.get<HealthComponent>(target).current;
      const float owner_before = registry.get<CombatStats>(owner).health;
      skills::BloodSea::UpdateField(registry, field_entity, field, 0.3f, grid);
      out_damage = target_before - registry.get<HealthComponent>(target).current;
      out_heal = registry.get<CombatStats>(owner).health - owner_before;
    };

    float damage_on = 0.0f;
    float heal_on = 0.0f;
    float damage_off = 0.0f;
    float heal_off = 0.0f;
    run(true, damage_on, heal_on);
    run(false, damage_off, heal_off);

    REQUIRE(damage_off > 0.0f);
    REQUIRE(heal_off > 0.0f);
    // 增伤窗内伤害 +20%。
    CHECK(damage_on == doctest::Approx(damage_off * 1.2f).epsilon(0.02));
    // 治疗口径：单位伤害的吸血量 +20%（排除伤害乘数后的纯增疗效应）。
    const float leech_on = heal_on / damage_on;
    const float leech_off = heal_off / damage_off;
    CHECK(leech_on == doctest::Approx(leech_off * 1.2f).epsilon(0.02));
  }
}

} // namespace NoMoreDay
