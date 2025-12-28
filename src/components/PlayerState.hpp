#pragma once

struct PlayerLevel {
    int value = 1;
};

struct PlayerStats {
    int killCount = 0;
    int level = 1;
    float current_xp = 0.0f;
    float required_xp = 100.0f;
    int available_attribute_points = 0;
    int available_skill_points = 0;
};

// Dash Skill Component
struct DashComponent {
    float cooldownTimer = 0.0f;
    float cooldownDuration = 2.0f;
    int charges = 2;
    int maxCharges = 2;
    
    bool isDashing = false;
    float dashTimer = 0.0f;
    float dashDuration = 0.2f;
    float dashSpeed = 1200.0f;
    float dirX = 0.0f;
    float dirY = 0.0f;
    
    // UI Feedback
    bool uiFlash = false;
    float uiFlashTimer = 0.0f;
};