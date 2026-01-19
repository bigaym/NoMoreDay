# Specification: Nemesis Evolution System (Monster Affix V2 - Part 4)

## 1. Overview
本 Track 实现元游戏层面的宿敌进化系统。游戏将记录玩家的战斗风格（Damage Profile, Playstyle），并据此生成针对性的“宿敌” (Nemesis)。这是 Roguelite 循环中连接单局与整体进程的关键一环。

## 2. Technical Architecture

### 2.1 Data Persistence

#### `PlayerCombatHistory`
一个单例组件或资源，用于在运行时累积数据，并序列化到存档中。
```cpp
struct PlayerCombatHistory {
    // Damage Profile (Rolling Average)
    float damageDealtPhysical;
    float damageDealtFire;
    float damageDealtCold;
    float damageDealtLightning;
    float damageDealtPoison;
    
    // Playstyle Metrics
    float avgEngagementDistance; // 玩家造成伤害时的平均距离
    float avgKillTime;           // 对精英怪的平均击杀耗时
    float burstDamagePeak;       // 单次最大伤害峰值
    
    // Counters
    int elitesKilled;
    int deathsToTraps;
};
```

### 2.2 Core Systems

#### `CombatHistorySystem`
*   监听 `OnDealDamage` 和 `OnKill` 事件。
*   更新 `PlayerCombatHistory` 中的统计数据。
*   使用**指数移动平均 (Exponential Moving Average)** 来平滑数据，使其更反映“近期”表现而非“历史总和”。

#### `NemesisGenerator` (Update)
*   在生成地图或宿敌时调用。
*   读取 `PlayerCombatHistory`。
*   根据规则库选择 `Evolution Traits`。

### 3. Evolution Logic (Rules)

系统将根据统计数据的显著特征选择进化词缀。

#### Rule 1: Adaptive Resistance (适应性抗性)
*   **Condition**: 某一种伤害类型占比 > 60%。
*   **Evolution**:
    *   若 Fire > 60% -> 添加 `Fire Resistant` (火抗 +75%) + `Molten` (以火攻火)。
    *   若 Phys > 60% -> 添加 `Armored` (护甲 +75%) + `Thorns` (反伤)。

#### Rule 2: Anti-Kite (反风筝)
*   **Condition**: `avgEngagementDistance` > 300px (远程风筝流)。
*   **Evolution**:
    *   添加 `Teleporter` (瞬移贴脸)。
    *   添加 `Vortex` (强制拉扯)。
    *   添加 `Waller` (限制走位)。

#### Rule 3: Anti-Burst (反爆发/秒杀)
*   **Condition**: `avgKillTime` < 1.0s (秒杀流)。
*   **Evolution**:
    *   添加 `Phase Shield`: 生命值低于 50% 时无敌 2秒。
    *   添加 `Second Life`: 死亡后 3秒 复活，拥有 40% 血量。

#### Rule 4: Glass Cannon Punisher (玻璃大炮惩罚)
*   **Condition**: 玩家闪避率高但血量低（难以直接统计，可用“造成伤害高但承受伤害次数少”推断）。
*   **Evolution**:
    *   添加 `Homing` (追踪弹，难以躲避)。
    *   添加 `Storm Strider` (必中/范围电击)。

## 4. UI & Feedback

### 4.1 Nemesis Alert
当宿敌生成时，UI 应展示其进化详情，让玩家知道“这是因为你太强了/太猥琐了而进化的”。
*   Title: "Nemesis Evolved: The Mage Slayer"
*   Subtitle: "Adapted to High Fire Damage & Long Range"

### 4.2 Visuals
*   **Resistances**: 怪物模型带有对应颜色的护盾光泽 (红=火抗, 蓝=冰抗)。
*   **Scale**: 宿敌体型通常比普通精英大 30%。

## 5. Integration Plan
1.  **Persistence**: 修改 `SaveManager` 支持 `PlayerCombatHistory` 的读写。
2.  **Tracking**: 实现 `CombatHistorySystem` 并注册到 `GameLoop`。
3.  **Generator**: 重写 `NemesisGenerator::Generate()` 逻辑，从随机改为基于权重。
4.  **UI**: 扩展 `BossHUD` 显示宿敌的特殊词缀信息。
