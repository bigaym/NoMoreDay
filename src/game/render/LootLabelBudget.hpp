#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace NoMoreDay::render {

// Pure label-budget selector for world-space loot labels (design §4.1).
// Independent of grid traversal order: selection is based solely on the
// candidate set and its priority rules, never on input ordering.
struct LootLabelCandidate {
  bool isGold = false;
  bool emphasized = false;
  int rarityOrdinal = 0;
  float distSq = 0.0f;
  int goldAmount = 0;
  float scale = 1.0f;
  bool visible = true;
  uint64_t stableKey = 0;  // deterministic tiebreak (e.g. entt id) for total order
};

class LootLabelBudget {
 public:
  static constexpr int kMaxLabels = 64;
  static constexpr int kNonRareBudget = 32;      // after this many labels, skip non-Rare items
  static constexpr int kSmallGoldBudget = 48;    // after this many labels, skip gold < threshold
  static constexpr int kSmallGoldAmount = 100;   // gold below this is "small"
  static constexpr int kDefaultRareOrdinal = 2;  // matches Rarity::Rare ordinal

  // Selects the subset of candidates that earn labels, in priority order:
  //   items before gold; items: emphasized > rarity desc > dist asc;
  //   gold: amount desc > dist asc. Then applies the budget rules:
  //   total <= kMaxLabels; after kNonRareBudget labels, non-Rare items with
  //   (!visible || scale<=1.0) are skipped; after kSmallGoldBudget labels,
  //   gold with amount < kSmallGoldAmount is skipped.
  // The result is a total order: identical input set always yields the same
  // selected subset regardless of input ordering.
  static std::vector<LootLabelCandidate> SelectLootLabels(
      const std::vector<LootLabelCandidate>& candidates,
      int rareOrdinal = kDefaultRareOrdinal);
};

inline std::vector<LootLabelCandidate> LootLabelBudget::SelectLootLabels(
    const std::vector<LootLabelCandidate>& candidates, int rareOrdinal) {
  std::vector<LootLabelCandidate> sorted = candidates;
  // Priority (desc): items group before gold group; items by
  // emphasized > rarity desc > dist asc; gold by amount desc > dist asc.
  // stableKey gives a total order so the result is independent of input order.
  std::sort(sorted.begin(), sorted.end(),
            [](const LootLabelCandidate& a, const LootLabelCandidate& b) {
              if (a.isGold != b.isGold) {
                return !a.isGold;
              }
              if (a.emphasized != b.emphasized) {
                return a.emphasized;
              }
              if (a.isGold) {
                if (a.goldAmount != b.goldAmount) {
                  return a.goldAmount > b.goldAmount;
                }
              } else {
                if (a.rarityOrdinal != b.rarityOrdinal) {
                  return a.rarityOrdinal > b.rarityOrdinal;
                }
              }
              if (a.distSq != b.distSq) {
                return a.distSq < b.distSq;
              }
              return a.stableKey < b.stableKey;
            });

  std::vector<LootLabelCandidate> selected;
  selected.reserve(candidates.size());
  for (const auto& cand : sorted) {
    if (static_cast<int>(selected.size()) >= kMaxLabels) {
      break;
    }
    if (cand.isGold) {
      if (static_cast<int>(selected.size()) > kSmallGoldBudget &&
          cand.goldAmount < kSmallGoldAmount) {
        continue;
      }
    } else {
      if (static_cast<int>(selected.size()) > kNonRareBudget &&
          cand.rarityOrdinal < rareOrdinal &&
          (!cand.visible || cand.scale <= 1.0f)) {
        continue;
      }
    }
    selected.push_back(cand);
  }
  return selected;
}

}  // namespace NoMoreDay::render
