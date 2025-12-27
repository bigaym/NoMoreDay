#pragma once

struct PlayerStats {
    int killCount = 0;
    int level = 1;
    float experience = 0.0f;
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