#pragma once
#include "doctest.h"
#include "game/application/render/LootLabelBudget.hpp"
#include <vector>

TEST_SUITE("LootLabelBudget") {
    using NoMoreDay::render::LootLabelBudget;
    using NoMoreDay::render::LootLabelCandidate;

    static LootLabelCandidate Item(int rarity, float dist, bool emph = false,
                                   float scale = 1.0f, bool visible = true) {
        LootLabelCandidate c;
        c.isGold = false;
        c.emphasized = emph;
        c.rarityOrdinal = rarity;
        c.distSq = dist;
        c.goldAmount = 0;
        c.scale = scale;
        c.visible = visible;
        return c;
    }

    static LootLabelCandidate Gold(int amount, float dist) {
        LootLabelCandidate c;
        c.isGold = true;
        c.emphasized = false;
        c.rarityOrdinal = 0;
        c.distSq = dist;
        c.goldAmount = amount;
        c.scale = 1.0f;
        c.visible = true;
        return c;
    }

    static bool SameSelection(const std::vector<LootLabelCandidate>& sel,
                              const std::vector<size_t>& keys) {
        if (sel.size() != keys.size()) {
            return false;
        }
        for (size_t i = 0; i < keys.size(); ++i) {
            if (sel[i].stableKey != keys[i]) {
                return false;
            }
        }
        return true;
    }

    TEST_CASE("[Unit] LootLabelBudget - priority order emphasized > rarity > distance") {
        // All items, no budget pressure beyond ordering.
        std::vector<LootLabelCandidate> in;
        in.push_back(Item(1, 100.0f));   // Magic, far
        in.push_back(Item(5, 10.0f));    // Epic, near
        in.push_back(Item(5, 50.0f, true, 1.5f));  // Epic emphasized, mid
        in.push_back(Item(3, 5.0f));     // Uncommon, nearest
        in.push_back(Item(5, 40.0f));    // Epic, mid
        for (size_t i = 0; i < in.size(); ++i) {
            in[i].stableKey = i;
        }

        auto sel = LootLabelBudget::SelectLootLabels(in);

        // Expected: emphasized Epic first, then Epic by distance (10, 40, 50),
        // then Uncommon(5), then Magic(100).
        CHECK(SameSelection(sel, {2, 1, 4, 3, 0}));
    }

    TEST_CASE("[Unit] LootLabelBudget - items before gold") {
        std::vector<LootLabelCandidate> in;
        in.push_back(Item(1, 100.0f));    // Magic item, far
        in.push_back(Gold(5000, 5.0f));   // large gold, near
        in.push_back(Item(4, 10.0f));     // Set item, near
        for (size_t i = 0; i < in.size(); ++i) {
            in[i].stableKey = i;
        }

        auto sel = LootLabelBudget::SelectLootLabels(in);

        // Items before gold regardless of gold amount.
        CHECK(SameSelection(sel, {2, 0, 1}));
    }

    TEST_CASE("[Unit] LootLabelBudget - gold sorted by amount desc then distance") {
        std::vector<LootLabelCandidate> in;
        in.push_back(Gold(100, 50.0f));
        in.push_back(Gold(500, 5.0f));
        in.push_back(Gold(500, 3.0f));
        in.push_back(Gold(100, 1.0f));
        for (size_t i = 0; i < in.size(); ++i) {
            in[i].stableKey = i;
        }

        auto sel = LootLabelBudget::SelectLootLabels(in);

        CHECK(SameSelection(sel, {2, 1, 3, 0}));
    }

    TEST_CASE("[Unit] LootLabelBudget - total budget cap 64") {
        std::vector<LootLabelCandidate> in;
        // All Rare+ (ordinal >= Rare=2) so the non-Rare rule never kicks in:
        // only kMaxLabels caps the selection.
        for (int i = 0; i < 80; ++i) {
            in.push_back(Item(2, static_cast<float>(i)));
            in.back().stableKey = static_cast<uint64_t>(i);
        }

        auto sel = LootLabelBudget::SelectLootLabels(in);

        CHECK(sel.size() == 64);
        // Nearest 64 selected.
        for (size_t i = 0; i < sel.size(); ++i) {
            CHECK(sel[i].stableKey == i);
        }
    }

    TEST_CASE("[Unit] LootLabelBudget - non-Rare skipped after 32 labels") {
        std::vector<LootLabelCandidate> in;
        // 30 Common items near.
        for (int i = 0; i < 30; ++i) {
            in.push_back(Item(0, static_cast<float>(i)));
            in.back().stableKey = static_cast<uint64_t>(i);
        }
        // 30 Rare items slightly farther.
        for (int i = 0; i < 30; ++i) {
            in.push_back(Item(2, 1000.0f + static_cast<float>(i)));
            in.back().stableKey = static_cast<uint64_t>(30 + i);
        }
        auto sel = LootLabelBudget::SelectLootLabels(in);

        // Sort is rarity desc: all 30 Rares first (size 0 -> 30), then Commons.
        // While selected.size() <= 32 a Common is allowed, so 3 Commons slip in
        // (sizes 31,32,33); from size 33 onward Commons are skipped.
        CHECK(sel.size() == 33);
        size_t commons = 0;
        size_t rares = 0;
        for (const auto& c : sel) {
            if (c.stableKey < 30) {
                ++commons;
            } else {
                ++rares;
            }
        }
        CHECK(commons == 3);
        CHECK(rares == 30);
    }

    TEST_CASE("[Unit] LootLabelBudget - small gold skipped after 48 labels") {
        std::vector<LootLabelCandidate> in;
        // 60 golds: 50 large, 10 small, interleaved.
        for (int i = 0; i < 60; ++i) {
            bool small = (i >= 50);
            in.push_back(Gold(small ? 50 : 200, static_cast<float>(i)));
            in.back().stableKey = static_cast<uint64_t>(i);
        }
        auto sel = LootLabelBudget::SelectLootLabels(in);

        // Large golds (amount >= 100) all kept; small ones (amount < 100)
        // skipped once > 48 labels already selected.
        CHECK(sel.size() == 50);
        for (const auto& c : sel) {
            CHECK(c.goldAmount >= 100);
        }
    }

    TEST_CASE("[Unit] LootLabelBudget - selection independent of input order") {
        std::vector<LootLabelCandidate> base;
        base.push_back(Item(1, 100.0f));   // 0
        base.push_back(Item(5, 10.0f));    // 1
        base.push_back(Item(0, 5.0f));     // 2
        base.push_back(Item(5, 40.0f, true, 1.5f));  // 3 emphasized
        base.push_back(Gold(500, 1.0f));   // 4
        base.push_back(Gold(50, 2.0f));    // 5 small
        base.push_back(Item(2, 200.0f));   // 6
        for (size_t i = 0; i < base.size(); ++i) {
            base[i].stableKey = i;
        }

        // Row-major (grid traversal) order: by index ascending.
        auto selRowMajor = LootLabelBudget::SelectLootLabels(base);

        // Shuffled input with same stableKeys.
        std::vector<LootLabelCandidate> shuffled = {base[4], base[0], base[6],
                                                    base[2], base[1], base[5],
                                                    base[3]};
        auto selShuffled = LootLabelBudget::SelectLootLabels(shuffled);

        REQUIRE(selRowMajor.size() == selShuffled.size());
        for (size_t i = 0; i < selRowMajor.size(); ++i) {
            CHECK(selRowMajor[i].stableKey == selShuffled[i].stableKey);
        }
    }

    TEST_CASE("[Unit] LootLabelBudget - boundary exactly 64 / 32 / 48") {
        // Exactly 64 items, all non-Rare (Magic, rarity 1 < Rare=2):
        // budget rule `selected.size() > kNonRareBudget` allows the first 33
        // (0..32 pass while size<=32), then all further non-Rare are skipped.
        std::vector<LootLabelCandidate> in64;
        for (int i = 0; i < 64; ++i) {
            in64.push_back(Item(1, static_cast<float>(i)));
            in64.back().stableKey = static_cast<uint64_t>(i);
        }
        CHECK(LootLabelBudget::SelectLootLabels(in64).size() == 33);

        // 32 Commons + 1 Rare: Rare first, then 32 Commons (size reaches 33);
        // the extra far Common is skipped once size > 32.
        std::vector<LootLabelCandidate> in33;
        for (int i = 0; i < 32; ++i) {
            in33.push_back(Item(0, static_cast<float>(i)));
            in33.back().stableKey = static_cast<uint64_t>(i);
        }
        in33.push_back(Item(0, 1000.0f));
        in33.back().stableKey = 100;
        in33.push_back(Item(2, 2000.0f));
        in33.back().stableKey = 200;
        auto sel33 = LootLabelBudget::SelectLootLabels(in33);
        CHECK(sel33.size() == 33);

        // Exactly 48 large golds + 1 small gold: `selected.size() > kSmallGoldBudget`
        // is false at size==48, so the 49th (small) gold is still accepted.
        std::vector<LootLabelCandidate> inGold;
        for (int i = 0; i < 48; ++i) {
            inGold.push_back(Gold(200, static_cast<float>(i)));
            inGold.back().stableKey = static_cast<uint64_t>(i);
        }
        inGold.push_back(Gold(50, 1000.0f));
        inGold.back().stableKey = 1000;
        auto selGold = LootLabelBudget::SelectLootLabels(inGold);
        CHECK(selGold.size() == 49);

        // Hard cap: 64 Rare items -> exactly 64 (kMaxLabels).
        std::vector<LootLabelCandidate> inRare64;
        for (int i = 0; i < 70; ++i) {
            inRare64.push_back(Item(2, static_cast<float>(i)));
            inRare64.back().stableKey = static_cast<uint64_t>(i);
        }
        CHECK(LootLabelBudget::SelectLootLabels(inRare64).size() == 64);
    }
}
