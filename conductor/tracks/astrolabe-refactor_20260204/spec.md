# 🌌 星系天赋系统重构规格说明书 (V1.1)

> **Track ID**: `astrolabe-refactor_20260204`
> **设计参考**: 
> - [星系天赋系统设计说明书 V1.0](../../../设计文档/星系天赋系统设计说明书.md)
> - [职业设计草案_剑修](../../../设计文档/职业设计草案_剑修.md)
> **状态**: 📝 设计中
> **重要说明**: ⚠️ 无需考虑存档兼容，可直接重构数据格式

---

## 1. 概述 (Overview)

将现有的"星座-连接"天赋树模型重构为"宇宙同源 (Universal Origin)"六扇区布局，以支持 6 职业多元化成长。核心改变包括：
- **布局模式**: 从任意连接的星座图转变为以黑洞为中心、6 个 60° 扇区向外辐射的同心轨道结构。
- **解锁机制**: 从"前置节点依赖"转变为"职业亲和度阈值解锁"。
- **职业限制**: 引入"职业誓约 (The Vow)"机制，区分主修与辅修。

### 1.1 核心变更对照表

| 维度 | 现有实现 | 目标实现 |
|------|----------|----------|
| **布局** | 任意位置的星座 (`Constellation`) + 连接线 (`prerequisites`) | 六扇区向心布局，基于 `(professionId, tier, sectorOffset)` 动态定位 |
| **解锁** | 必须点亮所有前置节点 (`prerequisites`) | 仅基于该职业的 **亲和总分** 阈值 (Tier 门槛) |
| **职业** | 无职业概念，全局通用 | 6 职业扇区，主修/辅修差异化 (Core 节点锁定) |
| **节点类型** | `Minor`, `Major`, `Keystone` | `Minor`, `Major`, `Core` (原 Keystone -> Core, 主修专属) |
| **数据源** | `astrolabe.json` (平铺星座) | `profession_talents.json` (按职业分组，无坐标，动态计算) |

### 1.2 设计目标
1. **高扩展性**: 通过修改 `PROFESSION_COUNT` 常量即可添加新职业，布局自动适配。
2. **职业特色**: 核心节点 (Core) 仅限主修职业解锁，塑造职业认同感。
3. **自由成长**: 取消节点连线依赖，通过花费点数积累亲和度，自由搭配 Build。
4. **GPU 渲染**: 节点发光、能量流动等特效卸载至 GPU Shader。
5. **快速迭代**: 无需存档兼容，可直接重写 `astrolabe.json` 格式。

---

## 2. 数据模型 (Data Model)

### 2.1 职业与扇区定义

当前已实现 **剑修 (Blade Ascendant)**，规划中职业共 6 个：

| ID | 职业 | 英文名 | 定位 |
|----|------|--------|------|
| 0 | 剑修 | Blade Ascendant | 高机动、技巧型玻璃大炮 (已实现) |
| 1 | 法师 | Mage | 元素魔法远程输出 |
| 2 | 祭祀 | Priest | 治疗/诅咒混合 |
| 3 | 骑士 | Knight | 重甲坦克/近战 |
| 4 | 游侠 | Ranger | 敏捷近战 + 远程弓箭 |
| 5 | 狂战士 | Berserker | 狂暴近战/血量置换 |

```cpp
// ============================================================
// TalentData.hpp - 新增
// ============================================================
namespace NoMoreDay {

// 职业枚举 (六大基础职业)
enum class ProfessionID : uint8_t {
    BladeAscendant = 0,  // 剑修 (已实现)
    Mage           = 1,  // 法师
    Priest     = 2,  // 祭祀
    Knight     = 3,  // 骑士
    Ranger     = 4,  // 游侠
    Berserker  = 5,  // 狂战士
    COUNT      = 6
};

// 节点类型 (重新定义)
enum class TalentNodeType : uint8_t {
    Minor = 0,   // 小节点：普通增益
    Major,       // 大节点：显著增益
    Core         // 核心节点：定义性天赋，仅主修职业可解锁
};

// 轨道层级解锁阈值
struct TierThreshold {
    static constexpr int TIER_1 = 0;   // 基础层，无门槛
    static constexpr int TIER_2 = 10;  // 第二圈，需 10 亲和
    static constexpr int TIER_3 = 25;  // 核心层，需 25 亲和
};

} // namespace NoMoreDay
```

### 2.2 天赋节点数据结构 (Node Definition)

```cpp
// ============================================================
// TalentData.hpp - 重构
// ============================================================
namespace NoMoreDay {

struct TalentNode {
    uint32_t id = 0;
    std::string name_key;
    std::string desc_key;
    
    ProfessionID profession = ProfessionID::BladeAscendant;
    TalentNodeType type = TalentNodeType::Minor;
    uint8_t tier = 1;              // 1, 2, 3
    uint8_t sectorIndex = 0;       // 在扇区内的编号 (用于均匀分布)
    
    // 可投入的最大点数 (如 0/5)
    uint8_t maxPoints = 1;
    
    // 效果列表 (复用现有)
    std::vector<StatModifier> modifiers;
    std::vector<DamageModifier> damage_modifiers;
    std::vector<StatConversion> conversions;
    std::vector<AstrolabeNodeEffect> effects;
    
    std::string icon_id;
    
    // 动态计算的世界坐标 (运行时填充，不序列化)
    mutable float x = 0.0f;
    mutable float y = 0.0f;
};

// 职业本命星 (扇区中心)
struct ProfessionStar {
    ProfessionID profession;
    std::string name_key;
    std::string desc_key;
    // 动态计算的世界坐标
    mutable float x = 0.0f;
    mutable float y = 0.0f;
};

// 完整的天赋图
struct TalentGraph {
    std::array<ProfessionStar, 6> professionStars;
    std::unordered_map<uint32_t, TalentNode> nodes;
    
    void Clear() { nodes.clear(); }
};

} // namespace NoMoreDay
```

### 2.3 玩家天赋组件扩展

```cpp
// ============================================================
// Progression.hpp - 扩展 AstrolabeComponent
// ============================================================
namespace NoMoreDay {

struct AstrolabeComponent {
    // --- 现有字段 (保留) ---
    std::set<uint32_t> activated_nodes;  // 已激活的节点 ID 集合
    int available_points = 0;            // 可分配的星尘点数
    
    // --- 新增字段 ---
    // 职业亲和度 (Profession Affinity)
    // 每个职业投入的点数总和
    std::array<int, 6> professionAffinity = {0, 0, 0, 0, 0, 0};
    
    // 主修职业 (誓约后锁定, -1 表示未选择)
    int mainProfession = -1;
    
    // 节点已投入点数 (支持多点节点 0/5)
    std::unordered_map<uint32_t, uint8_t> nodePoints;
    
    // 获取某职业的亲和度
    int getAffinity(ProfessionID prof) const {
        return professionAffinity[static_cast<uint8_t>(prof)];
    }
    
    // 是否已誓约
    bool hasVow() const { return mainProfession >= 0; }
    
    // 是否为主修职业
    bool isMainProfession(ProfessionID prof) const {
        return mainProfession == static_cast<int>(prof);
    }
};

} // namespace NoMoreDay
```

### 2.4 布局常量

```cpp
// ============================================================
// Common.hpp - namespace Constants::Astrolabe 扩展
// ============================================================
namespace NoMoreDay::Constants::Astrolabe {

    // --- 布局参数 ---
    constexpr int PROFESSION_COUNT = 6;
    constexpr float SECTOR_ANGLE = 360.0f / PROFESSION_COUNT;  // 60°
    
    // 轨道半径 (世界单位)
    constexpr float ORBIT_R1 = 150.0f;   // 本命星轨道
    constexpr float ORBIT_R2 = 300.0f;   // Tier 1 节点轨道
    constexpr float ORBIT_R3 = 500.0f;   // Tier 2 节点轨道
    constexpr float ORBIT_R4 = 750.0f;   // Tier 3 / Core 节点轨道
    
    // 节点大小
    constexpr float NODE_RADIUS_MINOR = 10.0f;
    constexpr float NODE_RADIUS_MAJOR = 16.0f;
    constexpr float NODE_RADIUS_CORE  = 22.0f;
    constexpr float PROFESSION_STAR_RADIUS = 35.0f;
    
    // 扇区 Angular Padding (避免边缘拥挤)
    constexpr float SECTOR_PADDING_DEG = 5.0f;

} // namespace NoMoreDay::Constants::Astrolabe
```

---

## 3. 系统架构 (Architecture)

### 3.1 系统层级

```
┌─────────────────────────────────────────────────────────────┐
│                        UI 层                                │
│   ┌─────────────────┐    ┌─────────────────┐               │
│   │  UIAstrolabe    │    │ AstrolabeRenderer│              │
│   │  (交互/输入)    │    │ (渲染: 背景/节点)|             │
│   └────────┬────────┘    └────────┬────────┘               │
│            │                      │                         │
├────────────┴──────────────────────┴─────────────────────────┤
│                     系统逻辑层                              │
│   ┌─────────────────────────────────────────────────────┐  │
│   │               TalentLayoutService                    │  │
│   │  - computeNodePositions(TalentGraph&)               │  │
│   │  - getSectorBounds(ProfessionID)                    │  │
│   └─────────────────────────────────────────────────────┘  │
│   ┌─────────────────────────────────────────────────────┐  │
│   │                 AstrolabeSystem                      │  │
│   │  - canUnlockNode(nodeId, AstrolabeComponent&)       │  │
│   │  - unlockNode(registry, player, nodeId)             │  │
│   │  - addPointToNode(registry, player, nodeId)         │  │
│   │  - takeVow(registry, player, professionId)          │  │
│   └─────────────────────────────────────────────────────┘  │
│                               │                             │
├───────────────────────────────┼─────────────────────────────┤
│                     数据层                                  │
│   ┌─────────────────┐    ┌─────────────────┐               │
│   │  TalentGraph    │    │AstrolabeComponent│              │
│   │  (静态定义)      │    │ (玩家状态)       │              │
│   └────────┬────────┘    └────────┬────────┘               │
│            │                      │                         │
├────────────┴──────────────────────┴─────────────────────────┤
│                     持久化层                                │
│   ┌─────────────────────────────────────────────────────┐  │
│   │                  SaveManager                         │  │
│   │  - 扩展 AstrolabeComponent 序列化                    │  │
│   └─────────────────────────────────────────────────────┘  │
│                     加载层                                  │
│   ┌─────────────────────────────────────────────────────┐  │
│   │                  TalentLoader                        │  │
│   │  - LoadProfessionTalents("profession_talents.json")  │  │
│   └─────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 核心 API

```cpp
// ============================================================
// AstrolabeSystem.hpp - 重构
// ============================================================
namespace NoMoreDay {

class AstrolabeSystem {
public:
    // --- 解锁逻辑 ---
    
    // 检查节点是否可解锁 (基于亲和度阈值，而非前置节点)
    static bool canUnlockNode(
        const TalentGraph& graph,
        const AstrolabeComponent& comp,
        uint32_t nodeId
    );
    
    // 为节点投入 1 点
    static bool addPointToNode(
        entt::registry& registry,
        entt::entity player,
        uint32_t nodeId
    );
    
    // --- 誓约机制 ---
    
    // 检查是否可以誓约某职业
    static bool canTakeVow(
        const AstrolabeComponent& comp,
        ProfessionID profession
    );
    
    // 执行誓约 (不可逆, 需二次确认)
    static bool takeVow(
        entt::registry& registry,
        entt::entity player,
        ProfessionID profession
    );
    
    // --- 查询接口 ---
    
    // 获取节点解锁状态
    enum class NodeStatus { Locked, Available, Activated };
    static NodeStatus getNodeStatus(
        const TalentGraph& graph,
        const AstrolabeComponent& comp,
        uint32_t nodeId
    );
    
    // 获取节点当前/最大点数
    static std::pair<int, int> getNodePoints(
        const TalentGraph& graph,
        const AstrolabeComponent& comp,
        uint32_t nodeId
    );
    
    // --- 属性应用 ---
    static void applyTalentStats(
        entt::registry& registry,
        entt::entity player
    );
    
private:
    // 内部：检查 Tier 门槛
    static bool meetsTierRequirement(
        const TalentGraph& graph,
        const AstrolabeComponent& comp,
        uint32_t nodeId
    );
    
    // 内部：检查 Core 节点主修权限
    static bool meetsVowRequirement(
        const TalentGraph& graph,
        const AstrolabeComponent& comp,
        uint32_t nodeId
    );
};

} // namespace NoMoreDay
```

```cpp
// ============================================================
// TalentLayoutService.hpp - 新增
// ============================================================
namespace NoMoreDay {

class TalentLayoutService {
public:
    // 计算所有节点的世界坐标 (基于扇区和轨道)
    static void computeNodePositions(TalentGraph& graph);
    
    // 获取职业扇区的起始角度 (度)
    static float getSectorStartAngle(ProfessionID profession);
    
    // 获取指定 Tier 的轨道半径
    static float getOrbitRadius(uint8_t tier);
    
private:
    // 计算扇区内节点的均匀分布角度
    static float computeNodeAngle(
        ProfessionID profession,
        uint8_t tier,
        uint8_t sectorIndex,
        uint8_t totalNodesInTier
    );
};

} // namespace NoMoreDay
```

---

## 4. 持久化契约 (Persistence Contract)

### 4.1 玩家存档扩展 (`slot_X.json`)

```json
{
  "version": 3,
  "astrolabe": {
    "activated_nodes": [1001, 1002, 2001],
    "available_points": 5,
    "profession_affinity": [10, 5, 0, 0, 0, 3],
    "main_profession": 0,
    "node_points": {
      "1001": 3,
      "1002": 1,
      "2001": 5
    }
  }
}
```

### 4.2 静态数据 (`assets/data/profession_talents.json`)

```json
{
  "version": 1,
  "profession_stars": [
    { "profession": 0, "name_key": "剑修之心", "desc_key": "以剑入道，追求极致的机动与爆发" },
    { "profession": 1, "name_key": "法师之魂", "desc_key": "掌控元素魔法的力量" },
    { "profession": 2, "name_key": "祭祀之心", "desc_key": "治疗与诅咒的双重奥义" },
    { "profession": 3, "name_key": "骑士之魂", "desc_key": "铁壁一般的守护与战线统治" },
    { "profession": 4, "name_key": "游侠之心", "desc_key": "远近皆宜的灵活战斗风格" },
    { "profession": 5, "name_key": "狂战之魂", "desc_key": "以血为代价的狂暴破坏力" }
  ],
  "nodes": [
    {
      "id": 1001,
      "profession": 0,
      "type": "Minor",
      "tier": 1,
      "sector_index": 0,
      "max_points": 5,
      "name_key": "强壮",
      "desc_key": "+{value}% 最大生命",
      "modifiers": [{ "stat": "MaxHealth", "mode": "Increase", "value": 2.0 }]
    },
    {
      "id": 1010,
      "profession": 0,
      "type": "Core",
      "tier": 3,
      "sector_index": 0,
      "max_points": 1,
      "name_key": "战争之魂",
      "desc_key": "解锁战士核心被动：狂战之怒",
      "effects": [{ "type": 0, "value": "WarriorRageComponent" }]
    }
  ]
}
```

---

## 5. UI 规格 (UI Specification)

### 5.1 布局视觉

```
                            ▲ Y+
                            │
                      ┌───────────┐
                      │ Berserker │
                      │    (5)    │
                      └─────┬─────┘
                 ____       │       ____
              ┌──────┐      │      ┌──────┐
              │Ranger│      ●      │Knight│
              │ (4)  │   黑洞核心   │ (3)  │
              └──────┘      │      └──────┘
                   ╲        │        ╱
                    ╲       │       ╱
                     ●     ●●●     ●
                    / ╲     │     / ╲
                   /   ╲    │    /   ╲
            ┌──────┐  ┌──────────┐  ┌──────┐
            │Priest│  │  Blade   │  │ Mage │
            │ (2)  │  │Ascendant│  │ (1)  │
            │      │  │   (0)   │  │      │
            └──────┘  └──────────┘  └──────┘
                            │
                            ▼ Y-
```

### 5.2 交互规范

| 操作 | 行为 |
|------|------|
| **左键点击节点** | 若可用，投入 1 点；若锁定，显示解锁条件提示 |
| **右键点击节点** | 显示详细 Tooltip (当前点数、效果数值) |
| **左键点击本命星** | 若未誓约，弹出誓约确认对话框 |
| **鼠标滚轮** | 缩放视图 |
| **右键拖拽** | 平移视图 |
| **N 键** | 重置视图到中心 |

### 5.3 节点状态视觉

| 状态 | 颜色 | 效果 |
|------|------|------|
| **Locked** | 灰色 (#444444) | 暗淡，无光晕 |
| **Available** | 琥珀色 (#FFD700) | 脉动光晕，清晰可见 |
| **Activated (0/N)** | 天蓝色 (#ADD8E6) | 内核发光 |
| **Fully Activated (N/N)** | 金色 (#FFD700) | 饱和发光 + 光芒线条 |
| **Core (Locked by Vow)** | 紫色 (#800080) | 封印纹样覆盖 |

### 5.4 誓约对话框

```
┌─────────────────────────────────────────────────┐
│   ⚠️ 深渊凝视 (The Vow)                          │
├─────────────────────────────────────────────────┤
│                                                 │
│  你即将与 [战士] 职业建立不可逆转的誓约。        │
│                                                 │
│  • 解锁所有 [战士] 核心天赋 (Core Nodes)          │
│  • 其他职业的核心天赋将被永久封印                │
│                                                 │
│            [ 长按此处确认誓约 (2秒) ]             │
│                                                 │
│                    [ 取消 ]                      │
└─────────────────────────────────────────────────┘
```

---

## 6. 风险与缓解 (Risks & Mitigations)

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| **布局密集** | Tier 3 节点过多时拥挤 | 动态调整扇区内角度分布，或采用螺旋式扩展 |
| **误操作誓约** | 职业选择不可逆 | 强制二次确认 (长按 2 秒)，对话框显著警示 |
| **GPU 兼容性** | 低端显卡特效性能 | 提供 Fallback CPU 渲染路径 |
| **职业平衡** | 各职业节点数量/强度不均 | 数据驱动，易于迭代调整 |

---

## 7. 验收标准 (Acceptance Criteria)

- [ ] **AC1**: 星盘 UI 呈现 6 个职业扇区，以黑洞为中心向外辐射。
- [ ] **AC2**: 节点解锁仅依赖职业亲和度阈值，无前置节点逻辑连线。
- [ ] **AC3**: 投入节点的点数自动累加到对应职业的亲和度。
- [ ] **AC4**: 未誓约玩家无法解锁任何 Core 类型节点。
- [ ] **AC5**: 誓约后，非主修职业的 Core 节点显示"封印"状态。
- [ ] **AC6**: 支持多点节点 (如 0/5)，每次点击投入 1 点。
- [ ] **AC7**: 节点位置基于 `(profession, tier, sectorIndex)` 动态计算，无硬编码坐标。
- [ ] **AC8**: 节点光晕、能量流动等特效通过 GPU Shader 渲染。

---

## 8. GPU 渲染规格 (GPU Rendering)

### 8.1 节点特效 Shader

节点的发光、脉动、封印纹理等效果通过 GPU 计算，避免 CPU 开销。

```glsl
// talent_node.fs - 节点渲染 Fragment Shader
uniform float u_time;
uniform int u_status;       // 0=Locked, 1=Available, 2=Activated, 3=FullyActivated, 4=Sealed
uniform float u_progress;   // 当前点数 / 最大点数 (0.0 ~ 1.0)
uniform vec4 u_baseColor;

void main() {
    vec2 uv = gl_FragCoord.xy;
    float dist = length(uv - vec2(0.5));
    
    // 状态驱动的颜色和效果
    vec4 color = u_baseColor;
    float glow = 0.0;
    
    if (u_status == 1) { // Available - 琥珀色脉动
        float pulse = sin(u_time * 3.0) * 0.3 + 0.7;
        glow = pulse * smoothstep(0.5, 0.3, dist);
        color = mix(color, vec4(1.0, 0.84, 0.0, 1.0), 0.5);
    }
    else if (u_status >= 2) { // Activated/FullyActivated
        float ring = smoothstep(0.4, 0.35, dist) - smoothstep(0.35, 0.3, dist);
        glow = ring * u_progress; // 进度环
        
        if (u_status == 3) { // FullyActivated - 金色光芒
            float rays = abs(sin(atan(uv.y - 0.5, uv.x - 0.5) * 6.0 + u_time * 2.0));
            glow += rays * 0.3 * smoothstep(0.5, 0.2, dist);
        }
    }
    else if (u_status == 4) { // Sealed (Core, non-main profession)
        // 紫色封印纹样
        float seal = fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
        color = mix(vec4(0.5, 0.0, 0.5, 1.0), vec4(0.2, 0.0, 0.3, 1.0), seal);
    }
    
    gl_FragColor = color + vec4(glow);
}
```

### 8.2 能量流动特效

当玩家向节点投入点数时，触发从本命星向节点流动的粒子效果。

| 特效 | 实现方式 |
|------|----------|
| **能量光线** | GPU 粒子系统，路径从 ProfessionStar 到 Node |
| **节点爆发** | 点数满时的超新星爆发效果 |
| **封印纹理** | Procedural 纹理，不需额外贴图 |

### 8.3 性能预算

| 指标 | 目标 |
|------|------|
| **节点数量** | 支持 200+ 节点 (6职业 × 30+节点) |
| **渲染开销** | < 1ms / 帧 (GPU) |
| **内存** | 节点数据 < 1MB |

---

*规格版本: 1.1*
*最后更新: 2026-02-04*
