#pragma once

#include "raylib.h"
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

// 游戏世界常量
namespace WorldConstants {
    constexpr int WORLD_WIDTH = 5000;
    constexpr int WORLD_HEIGHT = 5000;
    constexpr float GRID_CELL_SIZE = 32.0f;
    constexpr int GRID_COLS = WORLD_WIDTH / (int)GRID_CELL_SIZE + 1;
    constexpr int GRID_ROWS = WORLD_HEIGHT / (int)GRID_CELL_SIZE + 1;
}

// 基础变换组件
struct Position {
    float x = 0.0f;
    float y = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Position, x, y)

struct Velocity {
    float vx = 0.0f;
    float vy = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Velocity, vx, vy)

// 视觉组件
struct ColorComponent {
    Color color = WHITE;
};

struct SpriteComponent {
    Texture2D texture = { 0 };
    float scale = 1.0f;
    // float rotation = 0.0f; // 未来扩展
    // Rectangle sourceRect = { 0 }; // 未来用于精灵图
};

// 用于标识玩家实体的标签
struct PlayerTag {};

// 存储实体的原始输入状态
struct InputComponent {
    float moveX = 0.0f; // -1.0 到 1.0
    float moveY = 0.0f; // -1.0 到 1.0
    bool attack = false;
    bool dash = false;
};

// 战斗属性
struct HealthComponent {
    float current = 0.0f;
    float max = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(HealthComponent, current, max)

// 视野组件
struct VisionComponent {
    float radius = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(VisionComponent, radius)

// 简单的近战武器定义
struct WeaponComponent {
    float damage = 0.0f;
    float range = 0.0f;          // 攻击半径
    float cooldown = 0.0f;       // 两次攻击之间的秒数
    float knockback = 0.0f;      // 施加到目标的击退力
    
    // 内部状态
    float cooldownTimer = 0.0f;  // 0.0f 表示就绪
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WeaponComponent, damage, range, cooldown, knockback, cooldownTimer)

// 刚被击杀实体的标签组件

    struct KilledTag {
        entt::entity killer = entt::null;
    };

    struct XPProcessedTag {}; // Marks that XP has been awarded for this entity



// 掉落组件

struct GoldComponent {

    uint32_t amount = 0;

};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GoldComponent, amount)

// 持久化标签：跨关卡保留 (如：玩家、核心UI)
struct PersistentTag {};

// GPU 索引组件
struct GPUIndex {
    int index = -1;
};

// 半径组件
struct Radius {
    float value = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Radius, value)

// 本地关卡标签：切换关卡时销毁 (如：敌人、掉落物、投射物)
struct LocalLevelTag {};

// 资源 ID 组件 (用于持久化纹理引用)
struct TextureIDComponent {
    uint32_t id = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TextureIDComponent, id)

// 定义 IDComponent (用于持久化唯一标识)
struct IDComponent {
    uint64_t uuid = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IDComponent, uuid)
