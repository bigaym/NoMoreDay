# 2D 暗黑Like Roguelite RPG - 怪物与AI设计 V0.5 (已重构)

## 1. 设计哲学：标签驱动的生态 (Tag-Driven Ecology)

在 **NoMoreDay** 中，怪物不仅仅是贴图和数值的堆砌，它们是 **Tags (标签)** 的容器。
`CombatSystem` 不关心“这是什么怪”，只关心它是否拥有 `[Fire]`、`[Undead]` 或 `[Elite]` 标签。

*   **可视化原则**: 玩家应能一眼识别出怪物的威胁等级（颜色、体型）和主要属性（比如燃烧的特效代表 `[Fire]`）。
*   **群聚协同 (Synergy)**: 怪物之间的配合通过 AI 状态机实现，例如“拥有 `[Shield]` 标签的怪物会优先保护 `[Ranger]`”。

---

## 2. 怪物种族与标签 (Races & Tags)

种族决定了怪物的**基础属性倾向**和**通过标签定义的抗性/弱点**。

### 2.1 亡灵 (The Undead)
*   **Tags**: `[Undead]`, `[Physical]`
*   **特性**: 免疫 `[Bleed]`, `[Poison]`。
*   **弱点**: 受到 `[Fire]` 和 `[Holy]` 伤害增加 20%。
*   **AI**: 数量庞大，无视恐惧 (Fear)。

### 2.2 异魔 (The Void)
*   **Tags**: `[Void]`, `[Eldritch]`
*   **特性**: 拥有 40% 元素抗性 (`[Fire]`, `[Ice]`, `[Lightning]`)。
*   **弱点**: 物理护甲极低。
*   **AI**: 倾向于瞬移突袭玩家后排。

### 2.3 腐化生物 (Corrupted)
*   **Tags**: `[Nature]`, `[Beast]`
*   **特性**: 极高的生命回复速度。
*   **AI**: 具有“群体狂暴”机制，当周围同类死亡时攻速提升。

---

## 3. 宿敌系统集成 (The Nemesis Integration)

这是链接 **Roguelite 循环** 与 **终局 Boss** 的桥梁。

### 3.1 宿敌的原型 (Nemesis Archetype)
当某个阵营的仇恨值（Faction Aggro）达到 100% 时，下一次“维度拼接”的地图中将必定出现一名宿敌。
宿敌不是预设的 Boss，而是根据玩家杀死的怪物**动态合成**的。

*   **合成逻辑**: 
    1.  系统记录玩家最近 50 次击杀的“精英怪词缀”。
    2.  如果玩家杀死了大量带有 `[Fire]` 和 `[Fast]` 的精英，宿敌将生成为 **“疾风烈焰·复仇者”**。
    3.  **针对性进化**: 如果玩家主要造成 `[Ice]` 伤害，宿敌会自动获得 `[IceResist]` (冰抗) 和 `[Unstoppable]` (无法冻结)。

### 3.2 宿敌 AI
*   **Hunter Mode**: 宿敌不会像普通 Boss 那样呆在一个房间，它会主动在地图中**巡逻并寻找玩家**。
*   **Ambush**: 宿敌出现时伴随全图红色警报和专属 BGM。

---

## 4. 怪物职业/行为模板 (Archetypes & AI)

AI 状态机 (FSM) 驱动怪物的行为。

### 4.1 炮灰 / 蜂群 (Fodder)
*   **行为**: 仅仅使用 **GPU 流场 (Flow Field)** 移动。
*   **性能**: 因为不运行复杂的行为树，同屏可支持 5000+ 单位。
*   **目的**: 阻挡玩家移动，消耗玩家 AOE 冷却。

### 4.2 坦克 / 阻挡者 (Tank)
*   **行为**: `BlockLineOfSight`。总是试图移动到“玩家”和“友方远程怪”之间。
*   **物理**: 质量极大，难以被玩家击退。

### 4.3 支援者 (Support) - **[新增]**
*   **行为**: `Flee` (远离玩家) + `CastBuff`。
*   **逻辑**: 每 5 秒向周围半径 300 内的友军投射一个 `[Shield]` 或 `[Frenzy]` Buff。

### 4.4 刺客 (Assassin)
*   **行为**: `Stealth` (隐身) + `Burst`。
*   **逻辑**: 当玩家施放长硬直技能时，瞬移到玩家背后发动背刺（必爆）。

---

## 5. 精英词缀库 (Elite Modifiers)

精英怪（Elite）会随机获得 1-3 个词缀，这些词缀直接映射为 ECS 组件。

### 5.1 进攻类
*   **Molten (熔火)**: 攻击造成额外 `[Fire]` 伤害，移动留下火径。
*   **Vampiric (吸血)**: 造成伤害的 50% 治疗自身。
*   **Sunder (破甲)**: 攻击使玩家护甲降低 20%，可叠加。

### 5.2 防御类
*   **Stone Skin (石肤)**: 物理抗性 +50%，免疫击退。
*   **Mirror Image (镜像)**: 受击时分裂出 2 个幻象（只有 10% 血量）。
*   **Nullifier (虚无)**: 周期性清除玩家身上的 Buff。

### 5.3 机制类
*   **Avenger (复仇)**: 周围每死亡一个友军，体型变大 10%，伤害增加 10%。
*   **Link (灵魂链接)**: 与周围 5 个怪物共享血量池（AOE 技能的克星）。

---

## 6. 技术实现概念 (Technical Implementation)

### 6.1 ECS 组件设计
```cpp
struct AIComponent {
    Archetype type;      // Fodder, Tank, etc.
    float reaction_time; // 反应延迟，模拟笨重感
    EntityID target;
    FSMState current_state;
};

struct FactionComponent {
    FactionType faction; // Undead, Void...
    float aggro_value;   // 仇恨值
};
```

### 6.2 性能优化：混合驱动 (Hybrid Driver)
*   **Level 0 (Screen)**: 屏幕内的怪物每帧更新完整行为树。
*   **Level 1 (Near)**: 屏幕外 500px 范围内的怪物，每 10 帧更新一次逻辑，只进行简单的流场移动。
*   **Level 2 (Far)**: 更远的怪物完全冻结（Dormant），或被抽象为“宏观据点数据”。

### 6.3 动态难度 (Scaling)
怪物的属性随 **Map Tier (地图层数)** 和 **Corruption (腐化值)** 动态计算：
$$Stats = Base \times (1.08)^{Tier} \times (1 + \frac{Corruption}{100})$$
这保证了即使是同一张地图，高腐化下的怪物也会变得异常致命。
