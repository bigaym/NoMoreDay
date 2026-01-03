#pragma once

#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <concepts>
#include <string>
#include <vector>
#include <raylib.h>
#include "../utils/UUID.hpp"

#include "../components/Common.hpp"
#include "../components/Stats.hpp"
#include "../components/InventoryComponent.hpp"
#include "../components/EquipmentComponent.hpp" // ADDED THIS LINE
#include "../components/SkillSystem.hpp"
#include "../components/Buff.hpp"
#include "../components/Progression.hpp"
#include "../components/PlayerState.hpp"
#include "../components/AIComponent.hpp"
#include "UISystem.hpp" // Include UISystem

// 定义 Concept：检查类型 T 是否支持 nlohmann/json 序列化
template<typename T>
concept JsonSerializable = requires(T t, nlohmann::json j) {
    to_json(j, t);
    from_json(j, t);
};

class SerializationSystem {
public:
    static bool Update(entt::registry& registry) {
        // 更新通知计时器
        if (m_notificationTimer > 0.0f) {
            m_notificationTimer -= GetFrameTime();
        }

        // 执行延迟的操作 (确保 UI 有机会先渲染 "Saving..." 状态)
        if (m_pendingAction == ActionState::Save) {
            Save(registry, "saves/quicksave.json");
            m_notificationText = "Game Saved";
            m_notificationTimer = 2.0f; // 显示成功消息 2 秒
            m_pendingAction = ActionState::None;
        } else if (m_pendingAction == ActionState::Load) {
            Load(registry, "saves/quicksave.json");
            m_notificationText = "Game Loaded";
            m_notificationTimer = 2.0f;
            m_pendingAction = ActionState::None;
            return true; // 通知外部刚刚发生了读档
        }

        // 输入检测
        if (IsKeyPressed(KEY_F5)) {
            m_notificationText = "Saving...";
            m_notificationTimer = 0.5f; // 保持显示直到下一帧覆盖
            m_pendingAction = ActionState::Save;
        }
        if (IsKeyPressed(KEY_F8)) {
            m_notificationText = "Loading...";
            m_notificationTimer = 0.5f;
            m_pendingAction = ActionState::Load;
        }
        return false;
    }

    static void DrawUI() {
        if (m_notificationTimer > 0.0f && !m_notificationText.empty()) {
            Font font = UISystem::GetFont();
            const float fontSize = 20.0f;
            const float padding = 10.0f;
            
            float textWidth = IsFontValid(font) ? MeasureTextEx(font, m_notificationText.c_str(), fontSize, 1.0f).x : (float)MeasureText(m_notificationText.c_str(), (int)fontSize);
            float screenWidth = (float)GetScreenWidth();
            float screenHeight = (float)GetScreenHeight();
            
            // 计算右下角位置
            float x = screenWidth - textWidth - padding * 2;
            float y = screenHeight - fontSize - padding * 2;
            
            // 绘制半透明背景
            DrawRectangle((int)(x - padding), (int)(y - padding/2), (int)(textWidth + padding * 2), (int)(fontSize + padding), Fade(BLACK, 0.7f));
            
            // 绘制文本
            if (IsFontValid(font)) {
                DrawTextEx(font, m_notificationText.c_str(), { x, y }, fontSize, 1.0f, WHITE);
            } else {
                DrawText(m_notificationText.c_str(), (int)x, (int)y, (int)fontSize, WHITE);
            }

            // 如果正在处理中（Pending 状态），绘制一个简单的旋转图标
            if (m_pendingAction != ActionState::None) {
                float time = (float)GetTime();
                Vector2 iconCenter = { x - 20, y + fontSize / 2.0f };
                
                // 绘制旋转的小圆圈
                DrawCircleLines((int)iconCenter.x, (int)iconCenter.y, 8, WHITE);
                
                // 旋转指针
                float angle = time * 15.0f;
                Vector2 endPos = { 
                    iconCenter.x + cosf(angle) * 8, 
                    iconCenter.y + sinf(angle) * 8 
                };
                DrawLineV(iconCenter, endPos, WHITE);
            }
        }
    }

    /**
     * @brief 保存游戏状态到文件
     * @param filepath 存档路径 (例如 "saves/save01.json")
     */
    static void Save(entt::registry& registry, const std::filesystem::path& filepath) {
        nlohmann::json root;
        root["version"] = "0.1.0";
        root["entities"] = nlohmann::json::array();

        // [Auto-Fix] 自动为缺少 ID 的物品分配 UUID，防止物品丢失
        auto itemWithoutIdView = registry.view<NoMoreDay::ItemComponent>(entt::exclude<IDComponent>);
        for (auto entity : itemWithoutIdView) {
            registry.emplace<IDComponent>(entity, NoMoreDay::Utils::UUID::generate());
            const auto& item = registry.get<NoMoreDay::ItemComponent>(entity);
            std::cout << "[Serialization] Auto-assigned UUID to item: " << item.name << " (Entity " << (uint32_t)entity << ")" << std::endl;
        }

        // 获取所有拥有 IDComponent 的实体，但排除怪物 (EnemyTag)
        // 这样可以大幅减小存档体积，且怪物会在读档时由 SpawnSystem 重新生成
        auto view = registry.view<IDComponent>(entt::exclude<EnemyTag>);

        for (auto entity : view) {
            nlohmann::json entityJson;
            
            // 1. 保存 UUID
            const auto& idComp = view.get<IDComponent>(entity);
            entityJson["uuid"] = idComp.uuid;

            // 2. 序列化各个组件
            // 这里我们需要列出所有想要保存的组件类型
            // 使用辅助函数尝试序列化
            TrySerialize<Position>(registry, entity, entityJson, "Position");
            TrySerialize<Velocity>(registry, entity, entityJson, "Velocity");
            TrySerialize<HealthComponent>(registry, entity, entityJson, "Health");
            TrySerialize<VisionComponent>(registry, entity, entityJson, "Vision");
            TrySerialize<WeaponComponent>(registry, entity, entityJson, "Weapon");
            TrySerialize<GoldComponent>(registry, entity, entityJson, "Gold");
            TrySerialize<TextureIDComponent>(registry, entity, entityJson, "TextureID");
            TrySerialize<NoMoreDay::ItemComponent>(registry, entity, entityJson, "ItemData");
            
            TrySerialize<NoMoreDay::CombatStats>(registry, entity, entityJson, "Stats");
            TrySerialize<NoMoreDay::PrimaryStats>(registry, entity, entityJson, "PrimaryStats");
            TrySerialize<NoMoreDay::ModifierList>(registry, entity, entityJson, "Modifiers");
            
            TrySerialize<PlayerLevel>(registry, entity, entityJson, "PlayerLevel");
            TrySerialize<PlayerStats>(registry, entity, entityJson, "PlayerStats");
            TrySerialize<NoMoreDay::AstrolabeComponent>(registry, entity, entityJson, "Astrolabe");
            TrySerialize<DashComponent>(registry, entity, entityJson, "Dash");
            TrySerialize<NoMoreDay::ActiveSkillsComponent>(registry, entity, entityJson, "ActiveSkills");
            TrySerialize<NoMoreDay::ActiveEffectsComponent>(registry, entity, entityJson, "ActiveEffects");
            
            // 特殊处理包含 Entity 引用的组件
            SerializeInventory(registry, entity, entityJson);
            SerializeEquipment(registry, entity, entityJson);

            // 保存标签组件
            if (registry.all_of<PlayerTag>(entity)) {
                entityJson["PlayerTag"] = true;
            }

            root["entities"].push_back(entityJson);
        }

        // 确保目录存在
        if (filepath.has_parent_path()) {
            std::filesystem::create_directories(filepath.parent_path());
        }

        std::ofstream file(filepath);
        if (file.is_open()) {
            file << root.dump(4); // 缩进4空格，美化输出
        }
    }

    /**
     * @brief 从文件加载游戏状态
     * @param filepath 存档路径
     */
    static void Load(entt::registry& registry, const std::filesystem::path& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return;

        nlohmann::json root;
        try {
            file >> root;
        } catch (const std::exception& e) {
            // 处理 JSON 解析错误
            return;
        }

        registry.clear(); // 清空当前世界

        // 映射表：UUID -> 新的 Entity ID
        std::unordered_map<uint64_t, entt::entity> uuidMap;

        // --- 阶段 1: 创建实体并建立索引 ---
        for (const auto& entityJson : root["entities"]) {
            auto entity = registry.create();

            if (entityJson.contains("uuid")) {
                uint64_t uuid = entityJson["uuid"].get<uint64_t>();
                registry.emplace<IDComponent>(entity, uuid);
                uuidMap[uuid] = entity;
            }
        }

        // --- 阶段 2: 恢复组件并修复引用 ---
        for (const auto& entityJson : root["entities"]) {
            if (!entityJson.contains("uuid")) continue;
            uint64_t uuid = entityJson["uuid"].get<uint64_t>();
            entt::entity entity = uuidMap[uuid];

            TryDeserialize<Position>(registry, entity, entityJson, "Position");
            TryDeserialize<Velocity>(registry, entity, entityJson, "Velocity");
            TryDeserialize<HealthComponent>(registry, entity, entityJson, "Health");
            TryDeserialize<VisionComponent>(registry, entity, entityJson, "Vision");
            TryDeserialize<WeaponComponent>(registry, entity, entityJson, "Weapon");
            TryDeserialize<GoldComponent>(registry, entity, entityJson, "Gold");
            TryDeserialize<TextureIDComponent>(registry, entity, entityJson, "TextureID");
            TryDeserialize<NoMoreDay::ItemComponent>(registry, entity, entityJson, "ItemData");

            TryDeserialize<NoMoreDay::CombatStats>(registry, entity, entityJson, "Stats");
            TryDeserialize<NoMoreDay::PrimaryStats>(registry, entity, entityJson, "PrimaryStats");
            TryDeserialize<NoMoreDay::ModifierList>(registry, entity, entityJson, "Modifiers");

            TryDeserialize<PlayerLevel>(registry, entity, entityJson, "PlayerLevel");
            TryDeserialize<PlayerStats>(registry, entity, entityJson, "PlayerStats");
            TryDeserialize<NoMoreDay::AstrolabeComponent>(registry, entity, entityJson, "Astrolabe");
            TryDeserialize<DashComponent>(registry, entity, entityJson, "Dash");
            TryDeserialize<NoMoreDay::ActiveSkillsComponent>(registry, entity, entityJson, "ActiveSkills");
            TryDeserialize<NoMoreDay::ActiveEffectsComponent>(registry, entity, entityJson, "ActiveEffects");
            
            DeserializeInventory(registry, entity, entityJson, uuidMap);
            DeserializeEquipment(registry, entity, entityJson, uuidMap);

            // 恢复标签组件
            if (entityJson.contains("PlayerTag") && entityJson["PlayerTag"].get<bool>()) {
                registry.emplace<PlayerTag>(entity);
                // 关键修复：恢复玩家的控制权和状态标记
                registry.emplace_or_replace<InputComponent>(entity);
                registry.emplace_or_replace<NoMoreDay::StatsDirty>(entity);
                registry.emplace_or_replace<NoMoreDay::AttackState>(entity);
            }
        }
    }

private:
    enum class ActionState { None, Save, Load };
    static inline ActionState m_pendingAction = ActionState::None;
    
    static inline float m_notificationTimer = 0.0f;
    static inline std::string m_notificationText;

    // 辅助函数：如果实体拥有组件 T 且 T 支持 JSON，则序列化
    template<JsonSerializable T>
    static void TrySerialize(entt::registry& registry, entt::entity entity, nlohmann::json& j, const std::string& key) {
        if (registry.all_of<T>(entity)) {
            j[key] = registry.get<T>(entity); // 自动调用 to_json
        }
    }

    // 辅助函数：如果 JSON 中包含 key 且 T 支持 JSON，则反序列化
    template<JsonSerializable T>
    static void TryDeserialize(entt::registry& registry, entt::entity entity, const nlohmann::json& j, const std::string& key) {
        if (j.contains(key)) {
            T component = j[key].get<T>(); // 自动调用 from_json
            registry.emplace_or_replace<T>(entity, std::move(component));
        }
    }

    // --- 特殊序列化逻辑 ---

    static uint64_t GetEntityUUID(entt::registry& registry, entt::entity entity) {
        if (registry.valid(entity) && registry.all_of<IDComponent>(entity)) {
            return registry.get<IDComponent>(entity).uuid;
        }
        return 0; // 0 表示无效引用
    }

    static void SerializeInventory(entt::registry& registry, entt::entity entity, nlohmann::json& j) {
        if (!registry.all_of<NoMoreDay::InventoryComponent>(entity)) return;
        
        const auto& inv = registry.get<NoMoreDay::InventoryComponent>(entity);
        nlohmann::json invJson;
        invJson["capacity"] = inv.capacity;
        invJson["gold"] = inv.gold;
        
        // 序列化物品列表 (转换为 UUID)
        std::vector<uint64_t> itemUUIDs;
        for (auto itemEntity : inv.items) {
            itemUUIDs.push_back(GetEntityUUID(registry, itemEntity));
        }
        invJson["items"] = itemUUIDs;

        // 序列化背包槽 (转换为 UUID)
        std::vector<uint64_t> bagUUIDs;
        for (auto bagEntity : inv.bag_slots) {
            bagUUIDs.push_back(GetEntityUUID(registry, bagEntity));
        }
        invJson["bag_slots"] = bagUUIDs;

        j["Inventory"] = invJson;
    }

    static void DeserializeInventory(entt::registry& registry, entt::entity entity, const nlohmann::json& j, const std::unordered_map<uint64_t, entt::entity>& uuidMap) {
        if (!j.contains("Inventory")) return;
        
        const auto& invJson = j["Inventory"];
        auto& inv = registry.emplace_or_replace<NoMoreDay::InventoryComponent>(entity);
        
        inv.capacity = invJson.value("capacity", 40);
        inv.gold = invJson.value("gold", 0);
        
        // 恢复物品列表 (保持索引位置)
        if (invJson.contains("items")) {
            inv.items.clear();
            for (uint64_t uuid : invJson["items"]) {
                if (uuid != 0 && uuidMap.count(uuid)) {
                    inv.items.push_back(uuidMap.at(uuid));
                } else {
                    inv.items.push_back(entt::null);
                }
            }
        }
        
        // 确保 items 大小正确
        if ((int)inv.items.size() < inv.capacity) {
            inv.items.resize(inv.capacity, entt::null);
        }
        
        // 恢复背包槽 (std::array)
        if (invJson.contains("bag_slots")) {
            inv.bag_slots.fill(entt::null);
            size_t idx = 0;
            for (uint64_t uuid : invJson["bag_slots"]) {
                if (idx < inv.bag_slots.size()) {
                    if (uuid != 0 && uuidMap.count(uuid)) {
                        inv.bag_slots[idx] = uuidMap.at(uuid);
                    }
                }
                idx++;
            }
        }
    }

    static void SerializeEquipment(entt::registry& registry, entt::entity entity, nlohmann::json& j) {
        if (!registry.all_of<NoMoreDay::EquipmentComponent>(entity)) return;

        const auto& equip = registry.get<NoMoreDay::EquipmentComponent>(entity);
        nlohmann::json equipJson;
        
        // 序列化每个槽位
        for (size_t i = 0; i < equip.slots.size(); ++i) {
            uint64_t uuid = GetEntityUUID(registry, equip.slots[i]);
            if (uuid != 0) {
                equipJson[std::to_string(i)] = uuid;
            }
        }
        
        j["Equipment"] = equipJson;
    }

    static void DeserializeEquipment(entt::registry& registry, entt::entity entity, const nlohmann::json& j, const std::unordered_map<uint64_t, entt::entity>& uuidMap) {
        if (!j.contains("Equipment")) return;

        const auto& equipJson = j["Equipment"];
        auto& equip = registry.emplace_or_replace<NoMoreDay::EquipmentComponent>(entity);

        for (auto& [key, value] : equipJson.items()) {
            size_t slotIndex = std::stoul(key);
            uint64_t uuid = value.get<uint64_t>();
            
            if (slotIndex < equip.slots.size() && uuid != 0 && uuidMap.count(uuid)) {
                equip.slots[slotIndex] = uuidMap.at(uuid);
            }
        }
    }
};