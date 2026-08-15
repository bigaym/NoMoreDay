#pragma once

#include "raylib.h"
#include "engine/render/GPUData.hpp"
#include <cstdint>
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include <vector>

// 基础变换组件
struct Position
{
  float x = 0.0f;
  float y = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Position, x, y)

struct PrevPosition
{
  float x = 0.0f;
  float y = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PrevPosition, x, y) 


struct Rotation
{
  float angle = 0.0f; // In degrees
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Rotation, angle)

struct Velocity
{
  float vx = 0.0f;
  float vy = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Velocity, vx, vy)

// 视觉组件
struct ColorComponent
{
  Color color = WHITE;
};

struct SpriteComponent
{
  Texture2D texture = {0};
  float scale = 1.0f;
  int textureLayerIndex = -1; // Index in GL_TEXTURE_2D_ARRAY (-1 for none)
  // float rotation = 0.0f; // 未来扩展
  // Rectangle sourceRect = { 0 }; // 未来用于精灵图
};

// 用于标识玩家实体的标签
struct PlayerTag{};

// 穿透/相位标签：临时忽略体积碰撞与地图碰撞
struct PhaseTag {};

// 存储实体的原始输入状态
struct InputComponent
{
  float moveX = 0.0f; // -1.0 到 1.0
  float moveY = 0.0f; // -1.0 到 1.0
  bool attack = false;
  bool dash = false;
};

// 战斗属性
struct HealthComponent
{
  float current = 0.0f;
  float max = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(HealthComponent, current, max)

struct MovementAccumulator
{
  float distance = 0.0f;
  float threshold = 100.0f; // Default trigger every 100 distance (approx 1
                            // meter if scale 1:100)
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MovementAccumulator, distance, threshold)

// 视野组件
struct VisionComponent
{
  float radius = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(VisionComponent, radius)

// 简单的近战武器定义
struct WeaponComponent
{
  float damage = 0.0f;
  float range = 0.0f;     // 攻击半径
  float cooldown = 0.0f;  // 两次攻击之间的秒数
  float knockback = 0.0f; // 施加到目标的击退力

  // 内部状态
  float cooldownTimer = 0.0f; // 0.0f 表示就绪
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WeaponComponent, damage, range, cooldown,
                                   knockback, cooldownTimer)

// 刚被击杀实体的标签组件

struct KilledTag
{
  entt::entity killer = entt::null;
};

struct XPProcessedTag{}; // Marks that XP has been awarded for this entity

// 掉落组件

struct GoldComponent
{
  uint32_t amount = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GoldComponent, amount)

// 持久化标签：跨关卡保留 (如：玩家、核心UI)
struct PersistentTag{};

// GPU 索引组件
struct GPUIndex
{
  int index = -1;
};

// 半径组件
struct Radius
{
  float value = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Radius, value)

enum class ColliderType : uint8_t
{
  Dynamic,
  Static,
  Trigger
};
struct ColliderComponent
{
  float width = 0.0f;
  float height = 0.0f;
  ColliderType type = ColliderType::Dynamic;
  uint8_t layer = 1;
  uint8_t mask = 1;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ColliderComponent, width, height, layer,
                                   mask)

// 本地关卡标签：切换关卡时销毁 (如：敌人、掉落物、投射物)
struct LocalLevelTag{};

// 资源 ID 组件 (用于持久化纹理引用)
struct TextureIDComponent
{
  uint32_t id = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TextureIDComponent, id)

// 定义 IDComponent (用于持久化唯一标识)
struct IDComponent
{
  uint64_t uuid = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IDComponent, uuid)

// 延迟销毁组件
struct DelayedDestroyComponent
{
  float timer = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DelayedDestroyComponent, timer)

// 休眠标签：标记实体处于休眠状态，跳过 AI 和 Physics 更新
struct DormantTag{};

// 脏标记组件：标记实体的变换（位置/旋转）是否发生改变，用于加速 GPU 同步
struct DirtyTransform
{
  bool isDirty = true;
};

// 护盾运行时状态组件 (Hybrid Barrier: ES + Ward)
// 护盾值存储在 CombatStats.barrier 中，此组件仅存储运行时状态
struct BarrierComponent
{
  float last_damage_time = 0.0f; // 上次受击时间戳（用于判断回复延迟）
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BarrierComponent, last_damage_time)

// LootTag: 用于标记掉落物（物品或金币），优化空间查询
struct LootTag {};

// 标签缓存组件：用于加速世界坐标中文字标签的渲染
struct LabelCacheComponent {
    char cachedText[64] = {0};
    Vector2 cachedSize = {0, 0};
    int lastFontSize = 0;
    uint32_t lastRarityHash = 0;
    bool isValid = false;
    // 模板来源判别（B5）：true 表示 glyphTemplates 由 MSDF 图集度量
    // （BuildTemplatesMsdf）产出，false 表示位图图集（BuildTemplates）。来源
    // 切换时必须重建模板 —— 两个图集的 UV 与字号换算不可互换。
    bool lastUsedMsdf = false;

    // 字形布局模板（相对文本原点，不含屏幕坐标），由 LootTextBatcher::BuildTemplates 产出
    std::vector<NoMoreDay::components::GlyphTemplate> glyphTemplates;
    // 相对 (0,0) 的绝对坐标字形实例，由 WriteInstances 平移 origin 后复用
    std::vector<NoMoreDay::components::GPUGlyphInstance> cachedGlyphs;

    // Helper to force re-validation
    void Invalidate() { isValid = false; }
};
