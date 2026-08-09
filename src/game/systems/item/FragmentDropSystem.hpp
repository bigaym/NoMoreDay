#pragma once
#include "game/foundation/components/MapFragmentComponent.hpp"
#include <entt/entt.hpp>
#include <mutex>
#include <vector>

class LevelManager;

namespace NoMoreDay {

/**
 * @brief 碎片掉落系统
 *
 * 监听敌人死亡事件，按概率生成地图碎片掉落。
 * 碎片的类型、元素和属性基于敌人和区域特性随机生成。
 * 
 * 采用 Command Buffer 模式确保线程安全：事件处理器仅记录掉落请求，
 * 实际的实体创建在主线程的 Update 中完成。
 */
class FragmentDropSystem {
public:
  // 掉落请求数据
  struct DropRequest {
    int areaLevel;
    float magicFind;
    float posX;
    float posY;
    FragmentElement areaElement;
  };

  // 初始化系统 (注册事件监听器)
  static void Init();
  static void SetLevelManager(::LevelManager* lm) { s_levelManager = lm; }

  // 关闭系统 (取消事件监听器)
  static void Shutdown();

  // 每帧更新 (在主线程处理掉落请求)
  static void Update(entt::registry &registry);

  // 在敌人死亡时检查并生成碎片 (由 CombatEventDispatcher 调用)
  static void OnEnemyKilled(entt::registry &registry, entt::entity killer,
                            entt::entity victim);

  // 创建一个随机碎片实体
  static entt::entity CreateRandomFragment(entt::registry &registry,
                                           int areaLevel, float magicFind,
                                           FragmentElement areaElement = FragmentElement::None);

  // 创建指定属性的碎片实体
  static entt::entity CreateFragment(entt::registry &registry,
                                     FragmentType type, FragmentElement element,
                                     Rarity rarity);

private:
  static uint32_t s_killHandlerId;
  static bool s_initialized;
  static ::LevelManager* s_levelManager;
  static std::vector<DropRequest> s_pendingRequests;
  static std::mutex s_requestMutex;

  // 碎片生成概率计算
  static float GetFragmentDropChance(int victimLevel, bool isElite,
                                     bool isBoss);

  // 随机生成碎片类型
  static FragmentType RollFragmentType(float luck);

  // 随机生成碎片元素 (基于区域)
  static FragmentElement RollFragmentElement(FragmentElement areaElement);

  // 随机生成碎片稀有度
  static Rarity RollFragmentRarity(float magicFind, bool isElite, bool isBoss);

  // 根据稀有度生成属性值
  static void RollFragmentStats(MapFragmentComponent &fragment);
};

} // namespace NoMoreDay
