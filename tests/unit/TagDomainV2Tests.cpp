#include "doctest.h"

#include "game/combat_v2/TagDomain.hpp"

TEST_CASE("[Unit] TagDomainV2 - canonical and alias resolve to same ID") {
    NoMoreDay::CombatV2::TagDomain domain;

    const auto canonical = domain.Resolve("Fire");
    const auto alias = domain.Resolve("Burn");

    REQUIRE(canonical.status == NoMoreDay::CombatV2::TagDomain::ResolveStatus::Ok);
    REQUIRE(alias.status == NoMoreDay::CombatV2::TagDomain::ResolveStatus::Ok);
    CHECK(canonical.tagId == alias.tagId);
}

TEST_CASE("[Unit] TagDomainV2 - unknown tag hard-fail signal path") {
    NoMoreDay::CombatV2::TagDomain domain;

    const auto result = domain.Resolve("TotallyUnknownTagV2");
    CHECK(result.status == NoMoreDay::CombatV2::TagDomain::ResolveStatus::UnknownTag);
}

TEST_CASE("[Unit] TagDomainV2 - bitset all/any semantics") {
    NoMoreDay::CombatV2::TagDomain domain;

    const auto owned = domain.BuildBitset({1, 7});
    const auto requireAll = domain.BuildBitset({1, 7});
    const auto requireAny = domain.BuildBitset({7, 9});
    const auto requireTooMany = domain.BuildBitset({1, 7, 9});
    const auto requireAbsent = domain.BuildBitset({9, 10});

    CHECK(owned.HasAll(requireAll));
    CHECK(owned.HasAny(requireAny));
    CHECK_FALSE(owned.HasAll(requireTooMany));
    CHECK_FALSE(owned.HasAny(requireAbsent));
}
