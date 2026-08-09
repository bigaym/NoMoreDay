#pragma once

namespace NoMoreDay::Constants {

  namespace Astrolabe
  {
    constexpr float INITIAL_ZOOM = 2.0f;    // Default zoom (smaller = zoomed out)
    constexpr float MIN_ZOOM = 0.4f;        // Minimum zoom level
    constexpr float MAX_ZOOM = 5.0f;
    constexpr float PAN_SPEED = 1.0f;
    constexpr float ZOOM_SPEED = 0.12f;

    // Galaxy Rendering Constants (for background shader)
    // Galaxy center in world coordinates - aligned with origin node (0, 0)
    constexpr float GALAXY_CENTER_X = 0.0f;
    constexpr float GALAXY_CENTER_Y = 0.0f;
    // Scale factor: worldPos * SCALE -> uv. 
    // At Zoom=1, screen edge ~1280 world units, galaxy falloff at r~5
    // Scale = 5.0 / 1280 ≈ 0.004, using 0.003 for wider coverage
    constexpr float GALAXY_SCALE = 0.003f;
    
    // ============================================================
    // 六扇区布局参数 (V1.1)
    // ============================================================
    
    // 职业数量
    constexpr int PROFESSION_COUNT = 6;
    constexpr float SECTOR_ANGLE = 360.0f / PROFESSION_COUNT;  // 60°
    
    // 轨道半径 (世界单位)
    constexpr float ORBIT_R1 = 150.0f;   // 本命星轨道
    constexpr float ORBIT_R2 = 300.0f;   // Tier 1 节点轨道
    constexpr float ORBIT_R3 = 500.0f;   // Tier 2 节点轨道
    constexpr float ORBIT_R4 = 750.0f;   // Tier 3 / Core 节点轨道
    
    // 节点大小
    constexpr float NODE_RADIUS_MINOR = 12.0f;
    constexpr float NODE_RADIUS_MAJOR = 12.0f;
    constexpr float NODE_RADIUS_CORE  = 16.0f;
    constexpr float PROFESSION_STAR_RADIUS = 35.0f;
    
    // 扇区 Angular Padding (避免边缘拥挤)
    constexpr float SECTOR_PADDING_DEG = 5.0f;

    // File Paths
    constexpr const char* TALENT_DATA_PATH = "assets/data/profession_talents.json";
    
  } // namespace Astrolabe

} // namespace NoMoreDay::Constants
