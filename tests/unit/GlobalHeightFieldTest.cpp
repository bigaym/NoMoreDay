#include "doctest.h"

#include "engine/render/lighting/GlobalHeightField.hpp"

#include <span>
#include <vector>

using namespace NoMoreDay;

namespace {
using HeightStamp = render::lighting::GlobalHeightField::HeightStamp;

HeightStamp MakeTile(const int tileX, const int tileY, const float height) {
  HeightStamp stamp = {};
  stamp.kind = HeightStamp::Kind::Tile;
  stamp.tileX = tileX;
  stamp.tileY = tileY;
  stamp.height = height;
  stamp.dynamic = false;
  return stamp;
}

HeightStamp MakeDisc(const float worldX, const float worldY, const float radius,
                     const float height, const bool dynamic) {
  HeightStamp stamp = {};
  stamp.kind = HeightStamp::Kind::Disc;
  stamp.worldX = worldX;
  stamp.worldY = worldY;
  stamp.worldRadius = radius;
  stamp.height = height;
  stamp.dynamic = dynamic;
  return stamp;
}
} // namespace

TEST_CASE("[Unit] GlobalHeightField - terrain/static compose") {
  // Projection-equivalent of two ECS MapTileComponent tiles: floor(1,1) +
  // wall(5,5). The adapter turns WALL into 0.85 and everything else into 0.10.
  std::vector<HeightStamp> stamps;
  stamps.push_back(MakeTile(1, 1, 0.10f));
  stamps.push_back(MakeTile(5, 5, 0.85f));

  render::lighting::GlobalHeightField field;
  render::lighting::GlobalHeightField::Config cfg = {};
  cfg.textureWidth = 128;
  cfg.textureHeight = 128;
  cfg.chunkSize = 16;
  cfg.worldWidth = 1280.0f;
  cfg.worldHeight = 1280.0f;
  REQUIRE(field.Initialize(cfg));

  field.Update(stamps);

  const float floorH = field.SampleNormalizedHeight(15.0f, 15.0f);
  const float wallH = field.SampleNormalizedHeight(55.0f, 55.0f);
  CHECK(wallH > floorH);
  CHECK(field.GetLastStats().didFullRebuild);
}

TEST_CASE("[Unit] GlobalHeightField - dynamic chunk incremental update") {
  // Projection-equivalent of one ECS dynamic ShadowCasterComponent at (200,200)
  // with occluderHeight 0.95. The adapter turns it into a radius-18 dynamic disc.
  std::vector<HeightStamp> stamps;
  stamps.push_back(MakeDisc(200.0f, 200.0f, 18.0f, 0.95f, true));

  render::lighting::GlobalHeightField field;
  render::lighting::GlobalHeightField::Config cfg = {};
  cfg.textureWidth = 256;
  cfg.textureHeight = 256;
  cfg.chunkSize = 32;
  cfg.worldWidth = 512.0f;
  cfg.worldHeight = 512.0f;
  REQUIRE(field.Initialize(cfg));

  field.Update(stamps);
  const float firstSpot = field.SampleNormalizedHeight(200.0f, 200.0f);
  REQUIRE(firstSpot > 0.1f);

  // Projection-equivalent of the ECS caster moved to (340,330): fresh stamp set
  // with only the new disc, exactly as the adapter rebuilds it each frame.
  std::vector<HeightStamp> movedStamps;
  movedStamps.push_back(MakeDisc(340.0f, 330.0f, 18.0f, 0.95f, true));
  field.Update(movedStamps);
  const float oldSpot = field.SampleNormalizedHeight(200.0f, 200.0f);
  const float newSpot = field.SampleNormalizedHeight(340.0f, 330.0f);

  CHECK(newSpot > oldSpot);
  CHECK(field.GetLastStats().dirtyChunkCount > 0u);
}
