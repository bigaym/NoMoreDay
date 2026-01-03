#pragma once
#include "TestCommon.hpp"
#include "../src/core/SkillRegistry.hpp"
#include <unordered_set>

TEST_CASE("SkillSystem: Comprehensive Load Verification") {
    LoggerScope scope;
    auto& registry = SkillRegistry::Get();
    registry.LoadFromJson("assets/data/skills.json");

    SUBCASE("All 9 Skills Loaded") {
        for (uint32_t i = 1; i <= 9; ++i) {
            const auto* skill = registry.GetSkill(i);
            REQUIRE_MESSAGE(skill != nullptr, "Skill ID " << i << " should be loaded");
        }
    }

    SUBCASE("Unique Talent Node IDs") {
        std::unordered_set<uint32_t> all_node_ids;
        for (uint32_t i = 1; i <= 9; ++i) {
            const auto* tree = registry.GetSkillTree(i);
            if (!tree) continue;
            for (auto const& [node_id, node] : tree->nodes) {
                bool inserted = all_node_ids.insert(node_id).second;
                REQUIRE_MESSAGE(inserted, "Node ID " << node_id << " in Skill " << i << " is not unique across all skills");
            }
        }
    }

    SUBCASE("Verification of New Skills Data") {
        // Skill 3: Blade Formation
        const auto* skill3 = registry.GetSkill(3);
        REQUIRE(skill3 != nullptr);
        CHECK(skill3->name_key == "万剑诀");
        CHECK(HasTag(skill3->tags, Tag::Spell));
        CHECK(HasTag(skill3->tags, Tag::Projectile));

        // Skill 9: Phantom Flash
        const auto* skill9 = registry.GetSkill(9);
        REQUIRE(skill9 != nullptr);
        CHECK(skill9->name_key == "绝影闪");
        CHECK(HasTag(skill9->tags, Tag::Movement));
        CHECK(HasTag(skill9->tags, Tag::Attack));
    }
    
    SUBCASE("Stat Modifiers Parsing") {
        // Check Skill 9 node 920 (Dodge Chance)
        const auto* tree9 = registry.GetSkillTree(9);
        REQUIRE(tree9 != nullptr);
        auto it = tree9->nodes.find(920);
        REQUIRE(it != tree9->nodes.end());
        REQUIRE(it->second.stat_modifiers.size() > 0);
        CHECK(it->second.stat_modifiers[0].type == StatType::DodgeChance);
        CHECK(it->second.stat_modifiers[0].value == 10.0f);
    }
}
