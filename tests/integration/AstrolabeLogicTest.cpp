#include "TestCommon.hpp"
#include "game/foundation/components/Progression.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/AstrolabeRegistry.hpp"
#include "game/foundation/stats/AttributePipeline.hpp"

TEST_SUITE("AstrolabeLogic") {

    TEST_CASE("Astrolabe Logic Verification") {
        TestSetupScope setup;
        entt::registry registry;
        auto entity = registry.create();

        // 1. Setup Mock Graph
        TalentGraph originalGraph = AstrolabeRegistry::Get().GetGraph();
        TalentGraph mockGraph;

        // Node 9999: Stat Scaling (Strength + 10)
        AstrolabeTalentNode nodeStats;
        nodeStats.id = 9999;
        nodeStats.maxPoints = 5;
        nodeStats.modifiers.push_back({
            .value = 10.0f,
            .type = StatType::Strength,
            .mode = ModifierMode::Flat
        });
        mockGraph.nodes[9999] = nodeStats;

        // Node 9998: Grant Component (SwordIntentUnlock)
        AstrolabeTalentNode nodeGrant;
        nodeGrant.id = 9998;
        nodeGrant.maxPoints = 1;
        nodeGrant.effects.push_back({
            .type = AstrolabeEffectType::GrantComponent,
            .trait_id = TraitID::SwordIntentUnlock
        });
        mockGraph.nodes[9998] = nodeGrant;

        // Node 9997: Modify Intent (MaxSwordIntent + 2)
        AstrolabeTalentNode nodeModIntent;
        nodeModIntent.id = 9997;
        nodeModIntent.maxPoints = 3;
        nodeModIntent.effects.push_back({
            .type = AstrolabeEffectType::ModifyIntent,
            .trait_id = TraitID::MaxSwordIntent,
            .value = "2.0",
            .numeric_value = 2.0f // Pre-parsed
        });
        mockGraph.nodes[9997] = nodeModIntent;

        // Node 9996: Special Behavior (IntToCritMult:0.5)
        AstrolabeTalentNode nodeSpecial;
        nodeSpecial.id = 9996;
        nodeSpecial.maxPoints = 1;
        nodeSpecial.effects.push_back({
            .type = AstrolabeEffectType::SpecialBehavior,
            .value = "IntToCritMult:0.5",
            .ratio = 0.5f // Pre-parsed
        });
        mockGraph.nodes[9996] = nodeSpecial;

        AstrolabeRegistry::Get().SetGraph(mockGraph);

        // Setup Player Components
        registry.emplace<PlayerTag>(entity);
        registry.emplace<CombatStats>(entity);
        registry.emplace<PrimaryStats>(entity, 10.0f, 10.0f, 10.0f, 10.0f); // Base stats
        auto& astrolabe = registry.emplace<AstrolabeComponent>(entity);

        SUBCASE("AC-1: Stat Scaling") {
            // 0 Points
            StatsSystem::Recalculate(registry, entity);
            float str = StatsSystem::GetStatWithTags(registry, entity, StatType::Strength, Tag::None);
            CHECK(str == 10.0f); // Base

            // 1 Point
            astrolabe.nodePoints[9999] = 1;
            StatsSystem::Recalculate(registry, entity);
            str = StatsSystem::GetStatWithTags(registry, entity, StatType::Strength, Tag::None);
            CHECK(str == 20.0f); // 10 base + 10 * 1

            // 5 Points
            astrolabe.nodePoints[9999] = 5;
            StatsSystem::Recalculate(registry, entity);
            str = StatsSystem::GetStatWithTags(registry, entity, StatType::Strength, Tag::None);
            CHECK(str == 60.0f); // 10 base + 10 * 5
        }

        SUBCASE("AC-2: Grant Component") {
            astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);

            CHECK_FALSE(registry.all_of<BladeResourceComponent>(entity));

            astrolabe.nodePoints[9998] = 1;
            StatsSystem::Recalculate(registry, entity);
            
            CHECK(registry.all_of<BladeResourceComponent>(entity));
            CHECK(registry.all_of<SwordIntentComponent>(entity));

            // Remove point
            astrolabe.nodePoints.erase(9998);
            StatsSystem::Recalculate(registry, entity);
            
            CHECK(registry.all_of<BladeResourceComponent>(entity));
        }

        SUBCASE("AC-3: Max Sword Intent") {
            astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);
            astrolabe.nodePoints[9998] = 1;
            StatsSystem::Recalculate(registry, entity);
            
            auto* resource = registry.try_get<BladeResourceComponent>(entity);
            REQUIRE(resource != nullptr);
            int baseMax = resource->max;
            CHECK(baseMax == SkillConstants::DEFAULT_MAX_SWORD_INTENT);

            // Add 1 point to MaxIntent (+2)
            astrolabe.nodePoints[9997] = 1;
            StatsSystem::Recalculate(registry, entity);
            CHECK(resource->max == baseMax + 2);

            // Add 3 points (+6)
            astrolabe.nodePoints[9997] = 3;
            StatsSystem::Recalculate(registry, entity);
            CHECK(resource->max == baseMax + 6);
        }

        SUBCASE("AC-4: Int to CritMult Conversion") {
            // Base Intelligence = 10.
            // Node 9996 adds 0.5 * Int to CritDamage.
            // 10 * 0.5 = 5.
            // Base CritDamage = 150.0.
            
            // First check base
            StatsSystem::Recalculate(registry, entity);
            float cd = StatsSystem::GetStatWithTags(registry, entity, StatType::CritDamage, Tag::None);
            
            CHECK(cd == 150.0f);

            // Activate Node
            astrolabe.nodePoints[9996] = 1;
            StatsSystem::Recalculate(registry, entity);
            
            // Int = 10. Bonus = 10 * 0.5 = 5.
            // New CritDamage = 150 + 5 = 155.
            cd = StatsSystem::GetStatWithTags(registry, entity, StatType::CritDamage, Tag::None);
            CHECK(cd == 155.0f);
            
            // Increase Int
            registry.get<PrimaryStats>(entity).intelligence = 100.0f;
            StatsSystem::Recalculate(registry, entity);
            // Int = 100. Bonus = 100 * 0.5 = 50.
            // New CritDamage = 150 + 50 = 200.
            cd = StatsSystem::GetStatWithTags(registry, entity, StatType::CritDamage, Tag::None);
            CHECK(cd == 200.0f);
        }

        // Restore Graph
        AstrolabeRegistry::Get().SetGraph(originalGraph);
    }
}
