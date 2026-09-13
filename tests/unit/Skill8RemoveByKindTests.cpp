#include "TestCommon.hpp"
#include "game/foundation/components/Buff.hpp"

namespace NoMoreDay {

namespace {

// 构造仅携带 RemoveByKind 所需字段的效果，避免测试关注无关数据。
BuffEffect MakeKindEffect(const char *id, BuffKind kind, int source_skill_id) {
    BuffEffect effect;
    effect.id = id;
    effect.kind = kind;
    effect.source_skill_id = source_skill_id;
    effect.duration = 10.0f;
    effect.remaining = 10.0f;
    return effect;
}

// 组装同时含多个来源 FreeCast 与一个异类别效果的容器。
ActiveEffectsComponent MakeMixedContainer() {
    ActiveEffectsComponent component;
    component.effects.push_back(MakeKindEffect("fc_skill8", BuffKind::FreeCast, 8));
    component.effects.push_back(MakeKindEffect("fc_skill3", BuffKind::FreeCast, 3));
    component.effects.push_back(MakeKindEffect("fc_wildcard", BuffKind::FreeCast, 0));
    component.effects.push_back(MakeKindEffect("brand_skill8", BuffKind::QiBrand, 8));
    return component;
}

} // namespace

// N4-2 回归：带来源过滤的重载只清除指定来源与无归属(0)通配的效果。
TEST_CASE("[Unit] Buff RemoveByKind - source_skill_id filter") {
    ActiveEffectsComponent component = MakeMixedContainer();

    SUBCASE("removes matching source and wildcard, keeps other sources") {
        component.RemoveByKind(BuffKind::FreeCast, 8u);

        REQUIRE(component.effects.size() == 2);
        CHECK(component.effects[0].id == "fc_skill3");
        CHECK(component.effects[1].id == "brand_skill8");
    }

    SUBCASE("non-matching source survives when no wildcard exists") {
        ActiveEffectsComponent only_skill3;
        only_skill3.effects.push_back(
            MakeKindEffect("fc_skill3", BuffKind::FreeCast, 3));

        only_skill3.RemoveByKind(BuffKind::FreeCast, 8u);

        REQUIRE(only_skill3.effects.size() == 1);
        CHECK(only_skill3.effects[0].id == "fc_skill3");
    }

    SUBCASE("source 0 removes only wildcard effects") {
        component.RemoveByKind(BuffKind::FreeCast, 0u);

        REQUIRE(component.effects.size() == 3);
        CHECK(component.effects[0].id == "fc_skill8");
        CHECK(component.effects[1].id == "fc_skill3");
        CHECK(component.effects[2].id == "brand_skill8");
    }
}

// 回归：不加来源的无参重载语义保持不变，仍按 kind 清除全部匹配效果。
TEST_CASE("[Unit] Buff RemoveByKind - unfiltered kind removal unchanged") {
    ActiveEffectsComponent component = MakeMixedContainer();

    component.RemoveByKind(BuffKind::FreeCast);

    REQUIRE(component.effects.size() == 1);
    CHECK(component.effects[0].id == "brand_skill8");
}

} // namespace NoMoreDay
