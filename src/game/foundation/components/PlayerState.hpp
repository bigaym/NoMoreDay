#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>

struct PlayerLevel {
    int value = 1;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerLevel, value)

struct PlayerStats {
    uint64_t killCount = 0; // 击杀计数
    uint32_t current_map_kills = 0; // 当前地图击杀数
    uint64_t deathCount = 0; // 死亡计数 (预留)
    int level = 1; // 等级
    float current_xp = 0.0f; // 当前经验值
    float required_xp = 100.0f; // 升级所需经验值
    int available_attribute_points = 0; // 可用属性点
    int available_skill_points = 0; // 可用技能点

    // 基础属性 (不含装备加成) - 作为属性计算的源头
    int base_strength = 10;
    int base_dexterity = 10;
    int base_intelligence = 10;
    int base_vitality = 10;

    // Skill System State
    float last_shadow_trigger_time = -10.0f; // Internal Cooldown (ICD) for Shadow Kill Array
    
    // Status Flags
    bool isRooted = false;
    bool isSilenced = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerStats, killCount, current_map_kills, deathCount, level, current_xp, required_xp, available_attribute_points, available_skill_points, base_strength, base_dexterity, base_intelligence, base_vitality, last_shadow_trigger_time, isRooted, isSilenced)

// 冲刺技能组件
struct DashComponent {
    float cooldownTimer = 0.0f; // 冷却计时器
    float cooldownDuration = 2.0f; // 冷却时间
    int charges = 2; // 当前充能数
    int maxCharges = 2; // 最大充能数
    
    bool isDashing = false; // 是否正在冲刺
    float dashTimer = 0.0f; // 冲刺计时器
    float dashDuration = 0.3f; // 冲刺持续时间
    float dashSpeed = 400.0f; // 冲刺速度
    float dirX = 0.0f; // 冲刺方向X
    float dirY = 0.0f; // 冲刺方向Y
    
    // UI反馈
    bool uiFlash = false; // UI闪烁
    float uiFlashTimer = 0.0f; // UI闪烁计时器
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DashComponent, cooldownTimer, cooldownDuration, charges, maxCharges, isDashing, dashTimer, dashDuration, dashSpeed, dirX, dirY, uiFlash, uiFlashTimer)

// 属性分配 UI 状态组件
struct AttributeUIComponent {
    bool isOpen = false;
    bool showConfirmPopup = false;
    
    // 临时分配的点数 (尚未确认)
    int tempStr = 0;
    int tempDex = 0;
        int tempInt = 0;
        int tempVit = 0;
    };
    
    enum class MovementStance : uint8_t {
        Walking,
        SwordRiding
    };
    
    struct MovementStanceComponent {
        MovementStance stance = MovementStance::Walking;
        float movingTimer = 0.0f;
        float requiredMoveTime = 2.0f; // 2s continuous movement to enter sword riding
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MovementStanceComponent, stance, movingTimer, requiredMoveTime)
    