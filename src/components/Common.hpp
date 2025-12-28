#pragma once

#include "raylib.h"
#include <entt/entt.hpp>

// Game World Constants
namespace WorldConstants {
    constexpr int WORLD_WIDTH = 5000;
    constexpr int WORLD_HEIGHT = 5000;
    constexpr float GRID_CELL_SIZE = 32.0f;
    constexpr int GRID_COLS = WORLD_WIDTH / (int)GRID_CELL_SIZE + 1;
    constexpr int GRID_ROWS = WORLD_HEIGHT / (int)GRID_CELL_SIZE + 1;
}

// Basic Transform Component
struct Position {
    float x;
    float y;
};

struct Velocity {
    float vx;
    float vy;
};

// Visual Components
struct ColorComponent {
    Color color;
};

struct SpriteComponent {
    Texture2D texture;
    float scale;
    // float rotation; // Future extension
    // Rectangle sourceRect; // Future for spritesheets
};

// Tag to identify the player entity
struct PlayerTag {};

// Stores raw input state for an entity
struct InputComponent {
    float moveX; // -1.0 to 1.0
    float moveY; // -1.0 to 1.0
    bool attack;
    bool dash;
};

// Combat Stats
struct HealthComponent {
    float current;
    float max;
};

// Vision/Sight Component
struct VisionComponent {
    float radius;
};

// Simple Melee Weapon Definition
struct WeaponComponent {
    float damage;
    float range;          // Attack radius
    float cooldown;       // Seconds between attacks
    float knockback;      // Force applied to target
    
    // Internal State
    float cooldownTimer;  // 0.0f means ready
};

// Tag component for entities that have just been killed

struct KilledTag {

    entt::entity killer; // The entity that delivered the killing blow

};



// Loot components

struct GoldComponent {

    uint32_t amount;

};
