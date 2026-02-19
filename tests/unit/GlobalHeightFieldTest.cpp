#include "doctest.h"

#include "engine/render/lighting/GlobalHeightField.hpp"
#include "game/components/Common.hpp"
#include "game/components/MapComponent.hpp"
#include "game/components/ShadowCasterComponent.hpp"

using namespace NoMoreDay;

TEST_CASE("[Unit] GlobalHeightField - terrain/static compose") {
  entt::registry registry;
  const entt::entity floorTile = registry.create();
  registry.emplace<MapTileComponent>(floorTile, 1, 1, Tile::Type::FLOOR);
  const entt::entity wallTile = registry.create();
  registry.emplace<MapTileComponent>(wallTile, 5, 5, Tile::Type::WALL);

  render::lighting::GlobalHeightField field;
  render::lighting::GlobalHeightField::Config cfg = {};
  cfg.textureWidth = 128;
  cfg.textureHeight = 128;
  cfg.chunkSize = 16;
  cfg.worldWidth = 1280.0f;
  cfg.worldHeight = 1280.0f;
  REQUIRE(field.Initialize(cfg));

  field.Update(registry);

  const float floorH = field.SampleNormalizedHeight(15.0f, 15.0f);
  const float wallH = field.SampleNormalizedHeight(55.0f, 55.0f);
  CHECK(wallH > floorH);
  CHECK(field.GetLastStats().didFullRebuild);
}

TEST_CASE("[Unit] GlobalHeightField - dynamic chunk incremental update") {
  entt::registry registry;
  const entt::entity dynamicCaster = registry.create();
  registry.emplace<Position>(dynamicCaster, 200.0f, 200.0f);
  registry.emplace<NoMoreDay::ShadowCasterComponent>(
      dynamicCaster, NoMoreDay::ShadowCasterComponent{
                         .shape = NoMoreDay::ShadowOccluderShape::Circle,
                         .occluderHeight = 0.95f,
                         .dynamicFlag = 1u});

  render::lighting::GlobalHeightField field;
  render::lighting::GlobalHeightField::Config cfg = {};
  cfg.textureWidth = 256;
  cfg.textureHeight = 256;
  cfg.chunkSize = 32;
  cfg.worldWidth = 512.0f;
  cfg.worldHeight = 512.0f;
  REQUIRE(field.Initialize(cfg));

  field.Update(registry);
  const float firstSpot = field.SampleNormalizedHeight(200.0f, 200.0f);
  REQUIRE(firstSpot > 0.1f);

  registry.patch<Position>(dynamicCaster, [](Position &p) {
    p.x = 340.0f;
    p.y = 330.0f;
  });
  field.Update(registry);
  const float oldSpot = field.SampleNormalizedHeight(200.0f, 200.0f);
  const float newSpot = field.SampleNormalizedHeight(340.0f, 330.0f);

  CHECK(newSpot > oldSpot);
  CHECK(field.GetLastStats().dirtyChunkCount > 0u);
}

