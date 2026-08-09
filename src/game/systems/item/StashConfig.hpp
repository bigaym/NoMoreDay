#pragma once

namespace NoMoreDay::Constants {

  namespace StashConfig
  {
    constexpr int MAX_TABS = 10;
    // 解锁每一页的费用 (第1页免费，数组从索引1开始对应第2页解锁费)
    constexpr int UNLOCK_COSTS[] = {
        0,        // Tab 1 (Free)
        5000,     // Tab 2
        15000,    // Tab 3
        50000,    // Tab 4
        150000,   // Tab 5
        500000,   // Tab 6
        1500000,  // Tab 7
        5000000,  // Tab 8
        10000000, // Tab 9
        20000000  // Tab 10
    };

    constexpr int getUnlockCost(int tabIndex)
    {
      if (tabIndex < 0 || tabIndex >= MAX_TABS)
        return -1;
      return UNLOCK_COSTS[tabIndex];
    }
  } // namespace StashConfig

} // namespace NoMoreDay::Constants
