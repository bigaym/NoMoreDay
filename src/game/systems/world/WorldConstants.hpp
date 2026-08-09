#pragma once

#include <cstdint>

namespace NoMoreDay::Constants
{
  // 游戏世界相关常量
  namespace World
  {
    constexpr int WORLD_WIDTH = 5000;       // 世界总宽度（像素）
    constexpr int WORLD_HEIGHT = 5000;      // 世界总高度（像素）
    constexpr float GRID_TILE_SIZE = 10.0f; // 瓷砖地图块的基本大小
    constexpr float GRID_CELL_SIZE = 32.0f; // 用于寻找路径/网格划分的单元格大小
    constexpr int GRID_COLS =
        WORLD_WIDTH / static_cast<int>(GRID_CELL_SIZE) + 1; // 总列数
    constexpr int GRID_ROWS =
        WORLD_HEIGHT / static_cast<int>(GRID_CELL_SIZE) + 1; // 总行数
    constexpr float MAP_BOUNDARY = 5000.0f;                  // 地图物理边界范围

    namespace Map
    {
      constexpr uint8_t COST_WALL = 255;        // 墙壁的寻路代价（不可通行）
      constexpr uint8_t COST_FLOOR = 1;         // 地板的寻路代价（默认）
      constexpr int TOWN_EXIT_X_OFFSET = 12;      // 城镇出口 X 偏移 (向右移)
constexpr int TOWN_EXIT_Y_OFFSET = -8;      // 城镇出口 Y 偏移 (稍微向上)
      constexpr int FLOW_FIELD_MAX_DEPTH = 100; // 流场寻路的最大搜索深度
      constexpr float RENDER_PADDING = 4.0f;    // 地图渲染时的额外缓冲距离
    } // namespace Map

    namespace Fog
    {
      constexpr float VIEW_RADIUS_BUFFER = 2.0f;    // 战争迷雾视野半径的缓冲
      constexpr int COMPUTE_GROUP_SIZE = 16;        // GPU计算迷雾时的线程组大小
      constexpr float BACKGROUND_PADDING = 5000.0f; // 迷雾底图的边距
    } // namespace Fog
  } // namespace World
} // namespace NoMoreDay::Constants
