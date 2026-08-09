#pragma once

namespace NoMoreDay::Constants
{
  // 地图生成相关常量
  namespace Generator
  {
    namespace Cave
    {
      constexpr float INITIAL_WALL_PROB = 0.05f; // 初始生存细胞生成墙壁的概率
      constexpr int START_SEARCH_RADIUS = 20;    // 开始搜索可行走区域的半径
      constexpr int EXIT_ATTEMPTS = 1000;        // 寻找出口位置的最大尝试次数
      constexpr int SMOOTH_THRESHOLD = 4;        // 细胞自动机平滑操作的阈值
      constexpr int ROCK_SIZE_MIN = 100;         // 岩石簇的最小尺寸
      constexpr int ROCK_SIZE_MAX = 400;         // 岩石簇的最大尺寸
      constexpr int ROCK_DENSITY_DIVISOR = 1200; // 岩石密度系数
      constexpr int ROCK_MIN_COUNT = 12;         // 场景中岩石的最少数量
      constexpr int ROCK_EXPANSION_CHANCE = 85;  // 岩石向外扩展生成的概率
      constexpr int ROCK_SMOOTH_ITERATIONS = 4;  // 岩石边缘平滑的迭代次数
      constexpr int REGION_THRESHOLD_WALL = 80;  // 移除小于此面积的孤立墙壁区域
      constexpr int REGION_THRESHOLD_FLOOR = 40; // 移除小于此面积的孤立地板区域
    } // namespace Cave
  } // namespace Generator
} // namespace NoMoreDay::Constants
