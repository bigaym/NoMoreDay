#pragma once

namespace NoMoreDay::Constants
{
  // AI 行为相关常量
  namespace AI
  {
    constexpr float NORMALIZE_THRESHOLD = 0.01f;    // 向量归一化的极小阈值
    constexpr float FRICTION = 0.90f;               // AI移动时的摩擦力（速度衰减）
    constexpr float LEASH_RESET_MULTIPLIER = 4.0f;  // 怪物脱战后重置距离的倍数
    constexpr float HEALTH_REGEN_THRESHOLD = 0.95f; // 开始脱战回血的生命值阈值
    constexpr float HEALTH_REGEN_PER_SEC = 0.10f;   // 脱战后每秒回血比例 (10%)
    constexpr float FLOW_CELL_SIZE = 10.0f;         // 流场单元格大小

    // 性能优化：基于距离的分级更新
    constexpr float DORMANCY_THRESHOLD = 1600.0f;    // 进入休眠状态的距离阈值
    constexpr float UPDATE_DIST_MEDIUM = 600.0f;     // 中距离更新范围
    constexpr float UPDATE_DIST_FAR = 1000.0f;       // 远距离更新范围
    constexpr float UPDATE_INTERVAL_MEDIUM = 0.066f; // 中距离更新频率 (15Hz)
    constexpr float UPDATE_INTERVAL_FAR = 0.166f;    // 远距离更新频率 (6Hz)

    // 具体行为策略常量
    namespace Support
    {
      constexpr float MIN_SAFE_DIST = 150.0f;     // 辅助怪物的最小安全距离
      constexpr float MAX_SAFE_DIST = 400.0f;     // 辅助怪物的最大安全距离
      constexpr float FLEE_SPEED_MULT = 1.2f;     // 逃跑时的速度加成
      constexpr float APPROACH_SPEED_MULT = 0.5f; // 靠近友军的速度
      constexpr float RETREAT_SPEED_MULT = 0.6f;  // 撤退时的速度
      constexpr float BUFF_DURATION = 5.0f;       // 给友军施加Buff的持续时间
      constexpr float ARMOR_MOD_VALUE = 0.30f;    // 护甲强化Buff的数值
    } // namespace Support

    namespace Assassin
    {
      constexpr float BACKSTAB_DIST = 250.0f;         // 背刺触发距离
      constexpr float LURKER_DIST = 350.0f;           // 潜行怪物的警戒距离
      constexpr float STEALTH_TIMER_THRESHOLD = 2.0f; // 再次进入潜行所需的冷却
      constexpr float STALK_SPEED_MULT = 0.6f;        // 跟踪时的速度倍率
      constexpr float FLEE_DIST = 100.0f;             // 触发逃跑的距离
      constexpr float TELEPORT_OFFSET = 30.0f;        // 瞬移到背后的偏移
      constexpr float BUFF_DURATION = 1.0f;           // 爆发Buff的持续时间
      constexpr float BACKSTAB_DOT_THRESHOLD = 0.5f;  // 背刺角度阈值 (dot > 0.5)
    } // namespace Assassin

    namespace Tank
    {
      constexpr float SEARCH_RADIUS = 300.0f; // 寻找嘲讽目标的半径
      constexpr float MIDPOINT_OFFSET = 0.3f; // 拦截路径的偏移比例
      constexpr float ARRIVAL_DIST = 20.0f;   // 到达目标的判定距离
      constexpr float SPEED_MULT = 0.8f;      // 坦克怪物的速度倍率
    } // namespace Tank

    namespace Patrol
    {
      constexpr float ARRIVAL_DIST = 10.0f; // 巡逻点到达判定距离
      constexpr float MIN_STEP_DIST = 0.1f; // 有效移动的最小步长
      constexpr float SPEED = 15.0f;        // 默认巡逻移动速度
      constexpr float WAIT_TIME = 2.0f;     // 巡逻点停留时间
    } // namespace Patrol

    namespace Chase
    {
      constexpr float SPEED_FALLBACK = 50.0f;   // 追逐时的保底速度
      constexpr float ATTACK_EXIT_MULT = 1.2f;  // 退出攻击状态所需的距离倍数
      constexpr float HUNTER_SPEED_MULT = 1.2f; // 猎杀者加速倍率
    } // namespace Chase
  } // namespace AI
} // namespace NoMoreDay::Constants
