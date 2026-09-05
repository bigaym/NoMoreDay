#include "game/application/states/GameplayStateInternal.hpp"

namespace NoMoreDay {

void GameplayState::InitializeEntities() {
  auto &registry = *m_context->registry;
  auto &resourceManager = *m_context->resources;
  const auto &playerAsset = assets::textures::Player_Warrior;

  auto player = registry.create();
  LOG_DEBUG("Created player entity with ID: {}", (uint32_t)player);

  using namespace NoMoreDay::Constants::World;
  float startX = (float)WORLD_WIDTH / 2.0f;
  float startY = (float)WORLD_HEIGHT / 2.0f;

  // Use Map System to find a safe spawn (STAIRS_UP or nearest walkable)
  const auto &map = m_context->levelManager->getMapSystem();
  bool spawnFound = false;
  for (int y = 0; y < map.getHeight(); y++) {
    for (int x = 0; x < map.getWidth(); x++) {
      if (map.getTileType(x, y) == Tile::Type::STAIRS_UP) {
        using namespace NoMoreDay::Constants::World;
        startX = x * GRID_TILE_SIZE + (GRID_TILE_SIZE * 0.5f);
        startY = y * GRID_TILE_SIZE + (GRID_TILE_SIZE * 0.5f);
        spawnFound = true;
        break;
      }
    }
    if (spawnFound)
      break;
  }

  if (!spawnFound) {
    int cx = (int)(startX / 10.0f);
    int cy = (int)(startY / 10.0f);
    for (int r = 0; r < 50 && !spawnFound; r++) {
      for (int dx = -r; dx <= r; dx++) {
        for (int dy = -r; dy <= r; dy++) {
          if (map.isWalkable(cx + dx, cy + dy)) {
            using namespace NoMoreDay::Constants::World;
            startX = (cx + dx) * GRID_TILE_SIZE + (GRID_TILE_SIZE * 0.5f);
            startY = (cy + dy) * GRID_TILE_SIZE + (GRID_TILE_SIZE * 0.5f);
            spawnFound = true;
            break;
          }
        }
        if (spawnFound)
          break;
      }
    }
  }

  registry.emplace<Position>(player, startX, startY);
  registry.emplace<IDComponent>(player, Utils::UUID::from("Player"));
  registry.emplace<Velocity>(player, 0.0f, 0.0f);
  registry.emplace<Radius>(player, 5.0f);
  registry.emplace<GPUIndex>(player, -1);
  registry.emplace<PlayerTag>(player);
  registry.emplace<PlayerName>(player);
  registry.emplace<PlayerPlaytime>(player, 0, static_cast<double>(GetTime()));
  registry.emplace<PersistentTag>(player);
  registry.emplace<InputComponent>(player);
  registry.emplace<PlayerLevel>(player);
  registry.emplace<PlayerStats>(player);
  registry.emplace<PrimaryStats>(player, 10.0f, 10.0f, 10.0f, 10.0f);
  registry.emplace<CombatStats>(player);
  registry.emplace<VisionComponent>(player, 600.0f);
  registry.emplace<StatsDirty>(player);
  registry.emplace<DashComponent>(player);
  registry.emplace<InventoryComponent>(player);
  registry.emplace<MaterialBankComponent>(player);
  {
    auto &stash = registry.emplace<PersonalStashComponent>(player);
    stash.unlockedTabs = 1;
    stash.tabs.resize(1);
    stash.tabs[0].name = "Stash 1";
  }
  registry.emplace<EquipmentComponent>(player);
  registry.emplace<AttackState>(player);
  using namespace NoMoreDay::Constants::Combat;
  registry.emplace<HealthComponent>(player, DEFAULT_MAX_HEALTH,
                                    DEFAULT_MAX_HEALTH);
  registry.emplace<TextureIDComponent>(player, playerAsset.id);
  registry.emplace<MovementStanceComponent>(player);
  registry.emplace<MovementAccumulator>(player);
  registry.emplace<PlayerCombatHistory>(player);
  {
    auto &light = registry.emplace<LightComponent>(player);
    light.type = components::LightType::PointLight;
    light.radius = 150.0f;
    light.intensity = 0.4f;
    light.colorR = 1.0f;
    light.colorG = 0.95f;
    light.colorB = 0.88f;
    light.priority = 255;
    light.enabled = true;
  }

  // Set up Inventory

  // Astrolabe
  auto &astro = registry.emplace<AstrolabeComponent>(player);
  astro.available_points = 5; // Start with 5 points for testing

  // Test Equipment
  auto sword = ItemFactory::createWeapon(registry, 10, Rarity::Legendary);
  registry.emplace_or_replace<IDComponent>(sword, Utils::UUID::generate());
  registry.emplace<TextureIDComponent>(sword,
                                       assets::textures::Weapon_Sword.id);
  registry.emplace<PersistentTag>(sword); // Persist across scene transitions
  registry.get<EquipmentComponent>(player).set(EquipmentSlot::MainHand, sword);

  // Skill Setup
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = 1; // Q -> Flowing Thrust
  active.slots[4].id = 2; // RMB -> Rending Wave

  // Initialize charges
  for (auto &slot : active.slots) {
    if (slot.id != 0) {
      if (const auto *data = SkillRegistry::Get().GetSkill(slot.id)) {
        slot.current_charges = data->max_charges;
      }
    }
  }

  // IMPORTANT: Link skills to specialized slots so talents can be tracked
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[1].skill_id = 2;
  active.available_talent_points = 49; // Give 49 points for testing

  // Ensure some mana
  auto &stats = registry.get<CombatStats>(player);
  using namespace NoMoreDay::Constants::Combat;
  stats.mana = DEFAULT_MAX_MANA;
  stats.max_mana = DEFAULT_MAX_MANA;

  // Test Potions
  auto &inv = registry.get<InventoryComponent>(player);
  auto redPot = ItemFactory::createPotion(registry, 0, 10);
  registry.emplace_or_replace<IDComponent>(redPot, Utils::UUID::generate());
  registry.emplace<PersistentTag>(redPot); // Persist across scene transitions
  inv.items.push_back(redPot);
  auto bluePot = ItemFactory::createPotion(registry, 1, 10);
  registry.emplace_or_replace<IDComponent>(bluePot, Utils::UUID::generate());
  registry.emplace<PersistentTag>(bluePot); // Persist across scene transitions
  inv.items.push_back(bluePot);

  // --- Legendary Merging Test Items ---
  // 1. Catalyst
  auto core = ItemFactory::createMaterial(registry, 10001, 5);
  registry.emplace_or_replace<IDComponent>(core, Utils::UUID::generate());
  registry.emplace<PersistentTag>(core);
  inv.items.push_back(core);

  // 2. Base Item (Unique/Mythic with LP)
  auto baseFunc = ItemFactory::createWeapon(registry, 10, Rarity::Mythic);
  auto &baseItem = registry.get<ItemComponent>(baseFunc);
  baseItem.legendaryPotential = 2; // LP 2
  baseItem.name = "Blade of Testing (LP2)";
  registry.emplace_or_replace<IDComponent>(baseFunc, Utils::UUID::generate());
  registry.emplace<PersistentTag>(baseFunc);
  inv.items.push_back(baseFunc);

  // 3. Fodder Item (Exalted with 4 T6/T7)
  auto fodderFunc = ItemFactory::createWeapon(
      registry, 10,
      Rarity::Uncommon); // Exalted typically Uncommon base? Or Rare?
  auto &fodderItem = registry.get<ItemComponent>(fodderFunc);
  fodderItem.name = "Exalted Fodder";
  fodderItem.affixes.clear(); // Clear default
  // Add 4 high tier affixes
  for (int i = 0; i < 4; ++i) {
    Affix aff;
    aff.type = (AffixType)(i % 5); // Str, Dex, Int, Vit, FlatPhys
    aff.tier = (i < 2) ? 7 : 6;    // Two T7, Two T6
    aff.value = 50.0f;
    fodderItem.affixes.push_back(aff);
  }
  registry.emplace_or_replace<IDComponent>(fodderFunc, Utils::UUID::generate());
  registry.emplace<PersistentTag>(fodderFunc);
  inv.items.push_back(fodderFunc);

  // Texture
  Texture2D playerTexture =
      resourceManager.getTexture(playerAsset.id); // Should be loaded
  if (playerTexture.id > 0) {
    registry.emplace<SpriteComponent>(player, playerTexture, 0.4f);
  }
}

} // namespace NoMoreDay
