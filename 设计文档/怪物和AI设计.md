# 2D 暗黑Like Roguelite RPG - 怪物与AI设计 V0.1

## 1. 设计哲学 (Design Philosophy)

- **基于行为模板 (Archetype-Based)**：怪物由“种族（外观/属性倾向）”+“职业（AI行为模式）”组合而成。
- **群聚协同 (Synergy)**：怪物不应只是单独送死，不同类型的怪物混合生成能产生化学反应（例如：肉盾在前面挡住，法师在后面加血）。
- **视觉辨识度**：玩家一眼就能通过颜色和体型判断出怪物的威胁等级（普通、精英、首领）。

### 1.1 物理与群集 (Physics & Flocking)

为了解决海量怪物的堆叠问题，同时保持高性能，我们不使用刚体碰撞，而是采用 **Boids (群集)** 算法中的 **分离 (Separation)** 逻辑。

- **软碰撞 (Soft Collision)**：
  - 怪物之间**允许重叠**。
  - **排斥力场**：当两个怪物距离 < (R1 + R2) 时，产生一个反向推力向量 `F = (PosB - PosA).Normalized() * RepelStrength`。
  - **质量 (Mass)**：精英怪和坦克怪拥有更高质量，小怪会被轻易推开，而小怪推不动大怪。
- **效果**：尸潮会像“流体”一样自然散开包围玩家，而不是缩成一个点。

## 2. 怪物种族 (Monster Races)

种族决定了怪物的**基础属性倾向**、**抗性**以及**美术风格**。

### 2.1 亡灵 (The Undead)

- **风格**：骷髅、僵尸、幽灵。灰白色调，动作僵硬。
- **特性**：对流血/毒素免疫（无肉体），但畏惧神圣/火焰伤害。
- **AI倾向**：数量庞大，移动缓慢，不知疲倦地追击。

### 2.2 异魔 (The Void / Demons)

- **风格**：长着触手的眼球、虚空行者、小鬼。紫色/红色调。
- **特性**：高魔法抗性，攻击带有元素属性（火/暗影）。
- **AI倾向**：攻击频率高，喜欢瞬间移动或远程轰炸。

### 2.3 腐化生物 (Corrupted Beasts)

- **风格**：变异巨鼠、瘟疫蜘蛛、狂暴狼人。绿色/棕色调。
- **特性**：移动速度极快，高暴击率，防御力低。
- **AI倾向**：喜欢成群结队（Swarm），具有很高的仇恨连锁范围。

### 2.4 邪教徒 (Cultists)

- **风格**：穿着长袍的人类、重甲骑士。
- **特性**：属性均衡，会使用战术（如逃跑喝药）。
- **AI倾向**：只有这个种族拥有“治疗者”和“指挥官”角色。

## 3. 怪物职业/行为模板 (Archetypes & AI)

这是AI编程的核心。每一类职业对应一个状态机（FSM）。

### 3.1 炮灰 / 蜂群 (Fodder / Swarmer)

- **代表怪物**：骷髅兵、小蜘蛛、僵尸。
- **属性**：低血量，低攻击，数量极多。
- **AI逻辑**：
  - `Idle`：原地发呆。
  - `Chase`：无脑直线冲向玩家。
  - `Attack`：到达近战范围即攻击。
  - *特殊*：无碰撞体积（或很小），允许互相重叠以形成“尸潮”。

### 3.2 坦克 / 阻挡者 (Tank / Brute)

- **代表怪物**：缝合怪、重盾骑士、巨熊。
- **属性**：极高血量，高护甲，移动缓慢，体型巨大（阻挡弹道）。
- **AI逻辑**：
  - `Block`：总是试图移动到“玩家”和“友方远程怪”之间。
  - `Stun`：攻击带有击退或眩晕效果。

### 3.3 游击兵 / 射手 (Ranger / Skirmisher)

- **代表怪物**：骷髅弓手、虚空法师、投掷地精。
- **属性**：高伤害，脆皮。
- **AI逻辑 (Kiting)**：
  - `Attack`：在最大射程处攻击。
  - `Flee`：如果玩家靠近（比如距离<3米），立即向反方向移动。
  - `Reposition`：每攻击几次就横向移动，避免被预判。

### 3.4 刺客 / 突进者 (Assassin / Rusher)

- **代表怪物**：跳跃蜘蛛、幽灵、影魔。
- **属性**：高爆发，拥有位移技能。
- **AI逻辑**：
  - `Ambush`：平时隐身或在屏幕外等待。
  - `Dash`：当玩家背对它或在大招冷却好时，瞬间突进到玩家身边。

## 4. 怪物属性系统 (Attribute System)

### 4.1 基础属性

1. **HP (生命值)**：`Base_HP * Level_Multiplier * Rarity_Mod`
2. **Atk (攻击力)**：决定造成的基础伤害。
3. **Def (防御力/护甲)**：按百分比或固定数值减免物理伤害。
4. **MovSpd (移动速度)**：像素/秒。
5. **AtkSpd (攻击速度)**：攻击间隔。
6. **Range (射程)**：近战通常为 50-80px，远程为 400-800px。

### 4.2 动态难度等级 (Scaling)

怪物的强度随**地图等级 (Map Level)** 成长。建议采用指数或线性增长公式。

- 公式参考：

  

  $$Stats = Base \times (1 + 0.1 \times (Level - 1))^{1.5}$$

  

  （这意味着每10级怪物属性翻倍不止，迫使玩家更新装备）

### 4.3 怪物稀有度 (Monster Rarity)

通过给普通怪添加“词缀 (Affix)”来生成精英怪。

1. **普通 (Normal)**：白名，无特殊能力。
2. **精英 (Elite/Champion)**：蓝名/黄名。
   - 体积 x 1.5
   - 血量 x 3, 伤害 x 1.5
   - **拥有 1-2 个随机词缀**。
3. **首领 (Boss)**：红名。
   - 拥有独立AI和技能组，不共用通用模板。

### 4.4 词缀库示例 (Affix Pool)

- **迅捷 (Fast)**：移动速度 +50%。
- **强壮 (Tanky)**：生命值 +100%。
- **吸血 (Vampiric)**：造成伤害的 50% 转为自身治疗。
- **冰霜 (Frozen)**：攻击使玩家减速。
- **熔火 (Molten)**：行走留下一条伤害火径。
- **护盾 (Shielding)**：给周围友军提供无敌光环（优先击杀目标）。

## 5. C++20 架构实现思路

### 5.1 组件设计 (ECS Components)

利用 `Entt` 库，我们可以这样拆分怪物：

```
// 1. 身份组件
struct MonsterInfo {
    RaceID race;
    ArchetypeID type;
    int level;
};

// 2. 属性组件 (数值)
struct CombatStats {
    float hp, max_hp;
    float attack_power;
    float defense;
    float move_speed;
    float attack_range;
};

// 3. 状态机组件 (AI逻辑核心)
struct AIState {
    enum class State { Idle, Chase, Attack, Flee, Patrol };
    State current_state;
    float reaction_timer; // 反应延迟，模拟不同怪物的呆滞程度
    EntityID target;      // 当前仇恨目标（通常是玩家）
};

// 4. 技能/行为组件 (可选)
struct AbilityComponent {
    float cooldown_timer;
    // 使用 std::variant 存储不同的技能数据
    std::variant<MeleeAttackData, ProjectileData, TeleportData> ability_data;
};
```

### 5.2 行为树 vs 状态机 (Behavior Tree vs FSM)

- 对于成千上万的怪物，建议使用**简化的有限状态机 (Simple FSM)**。

- 逻辑写在 `MonsterSystem` 中：

  ```
  void UpdateMonsterAI(Registry& reg, float dt) {
      auto view = reg.view<AIState, CombatStats, Transform, PhysicsBody>();
  
      for(auto entity : view) {
          // 1. 检查距离 (Sensing)
          float dist_to_player = CalculateDistance(entity, player);
  
          // 2. 状态转换 (Transition)
          switch(ai.current_state) {
              case Idle:
                  if (dist_to_player < aggro_range) ai.current_state = Chase;
                  break;
              case Chase:
                  if (dist_to_player <= stats.attack_range) ai.current_state = Attack;
                  break;
              // ...
          }
  
          // 3. 执行行为 (Action)
          if (ai.current_state == Chase) {
              MoveTowards(entity, player_pos, stats.move_speed);
          }
      }
  }
  ```