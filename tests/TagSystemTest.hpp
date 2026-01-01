#pragma once
#include "TestCommon.hpp"
#include "core/TagRegistry.hpp"

using namespace NoMoreDay;

TEST_CASE("Tag Registry System") {
    SUBCASE("Bitmask Operations") {
        Tag t1 = Tag::Physical;
        Tag t2 = Tag::Fire;
        Tag t3 = Tag::Melee;

        // OR operation
        Tag combination = t1 | t2;
        CHECK(HasTag(combination, Tag::Physical));
        CHECK(HasTag(combination, Tag::Fire));
        CHECK_FALSE(HasTag(combination, Tag::Melee));

        // AND operation
        CHECK((combination & Tag::Physical) == Tag::Physical);
        CHECK((combination & Tag::Melee) == Tag::None);

        // Complex combination
        Tag complex = t1 | t2 | t3;
        CHECK(HasTag(complex, Tag::Physical));
        CHECK(HasTag(complex, Tag::Fire));
        CHECK(HasTag(complex, Tag::Melee));
    }

    SUBCASE("String Conversion") {
        CHECK(GetTagName(Tag::Physical) == "Physical");
        CHECK(GetTagName(Tag::Fire) == "Fire");
        CHECK(GetTagName(Tag::Shadow) == "Shadow");
        CHECK(GetTagName(Tag::Melee) == "Melee");
        CHECK(GetTagName(Tag::Hit) == "Hit");
        CHECK(GetTagName(Tag::Bleeding) == "Bleeding");
        
        // Test unknown/combined tag behavior (GetTagName handles single tags)
        // For combined tags, it should likely fall through to default or behavior undefined in current helper.
        // The current helper implementation uses switch/case, so a combined tag won't match any case unless explicitly handled or valid enum value.
        // It returns "Unknown" for combined values in current implementation.
        CHECK(GetTagName(Tag::Physical | Tag::Fire) == "Unknown"); 
    }

    SUBCASE("Tag Categories") {
        // Verify values are distinct
        CHECK(Tag::Physical != Tag::Fire);
        CHECK(Tag::Physical != Tag::Melee);
        
        // Verify category ranges (sanity check on bit positions)
        // Physical is bit 0
        CHECK((static_cast<uint64_t>(Tag::Physical) & 0xFFFF) != 0); // DamageType range 0-15
        
        // Melee is bit 16
        CHECK((static_cast<uint64_t>(Tag::Melee) & 0xFFFF0000) != 0); // Form range 16-31
    }
}
