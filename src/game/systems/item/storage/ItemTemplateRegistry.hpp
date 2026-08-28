#pragma once

#include "game/systems/item/storage/ItemTemplate.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NoMoreDay {

/**
 * @brief Centralized registry and single source of truth for immutable ItemTemplates.
 */
struct StringHash {
  using is_transparent = void;
  size_t operator()(std::string_view sv) const noexcept {
    return std::hash<std::string_view>{}(sv);
  }
};

class ItemTemplateRegistry {
public:
  /**
   * @brief Access the global singleton instance of ItemTemplateRegistry.
   */
  static ItemTemplateRegistry &Instance() noexcept;
  static ItemTemplateRegistry &Get() noexcept { return Instance(); }

  /**
   * @brief Register an item template into the registry.
   */
  void registerTemplate(ItemTemplate t);

  /**
   * @brief Find an item template by its baseId.
   * @return Pointer to immutable template, or nullptr if not found.
   */
  const ItemTemplate *find(uint32_t baseId) const noexcept;

  /**
   * @brief Find an item template by exact base name.
   * @return Pointer to immutable template, or nullptr if not found.
   */
  const ItemTemplate *findByName(std::string_view name) const noexcept;

  /**
   * @brief Check whether a template with given baseId exists.
   */
  bool hasTemplate(uint32_t baseId) const noexcept {
    return find(baseId) != nullptr;
  }

  /**
   * @brief Get count of registered templates.
   */
  size_t count() const noexcept { return m_templates.size(); }

  /**
   * @brief Clear all registered templates (primarily for testing).
   */
  void clear() noexcept;

  /**
   * @brief Populate all built-in baseline templates (weapons, armor, jewelry, consumables, bags, materials).
   */
  void initializeDefaults();

  /**
   * @brief Get read-only view of all registered templates.
   */
  const std::unordered_map<uint32_t, ItemTemplate> &
  getAllTemplates() const noexcept {
    return m_templates;
  }

  /**
   * @brief Query templates matching a specific weapon subtype, sorted by minLevel.
   */
  std::vector<const ItemTemplate *>
  getTemplatesByWeaponSubtype(WeaponSubtype subtype) const;

  /**
   * @brief Query templates matching a specific equipment slot, sorted by minLevel.
   */
  std::vector<const ItemTemplate *>
  getTemplatesBySlot(EquipmentSlot slot) const;

  /**
   * @brief Query templates matching a specific item kind.
   */
  std::vector<const ItemTemplate *> getTemplatesByKind(ItemKind kind) const;

  /**
   * @brief Query templates matching a specific item type.
   */
  std::vector<const ItemTemplate *> getTemplatesByType(ItemType type) const;

private:
  ItemTemplateRegistry() = default;
  ~ItemTemplateRegistry() = default;
  ItemTemplateRegistry(const ItemTemplateRegistry &) = delete;
  ItemTemplateRegistry &operator=(const ItemTemplateRegistry &) = delete;

  std::unordered_map<uint32_t, ItemTemplate> m_templates;
  std::unordered_map<std::string, uint32_t, StringHash, std::equal_to<>> m_nameIndex;
};

} // namespace NoMoreDay
