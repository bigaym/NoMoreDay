#pragma once

#include "raylib.h"

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

// Simple Melee Weapon Definition
struct WeaponComponent {
    float damage;
    float range;          // Attack radius
    float cooldown;       // Seconds between attacks
    float knockback;      // Force applied to target
    
    // Internal State
    float cooldownTimer;  // 0.0f means ready
};
