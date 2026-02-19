#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <raylib.h>

namespace NoMoreDay::render {

enum class MSDFAtlasCompression : uint8_t {
  None = 0,
  BC4 = 1,
  BC5 = 2,
};

struct MSDFGlyphMetric {
  uint32_t codepoint = 0;
  float uvRect[4] = {0.0f, 0.0f, 0.0f, 0.0f};   // u0, v0, u1, v1
  float bearing[2] = {0.0f, 0.0f};              // left, bottom
  float size[2] = {0.0f, 0.0f};                 // width, height
  float advance = 0.0f;
};

struct MSDFAtlasData {
  Texture2D texture = {};
  int width = 0;
  int height = 0;
  float distanceRange = 0.0f;
  std::vector<MSDFGlyphMetric> glyphs;
};

class MSDFAtlasLoader {
public:
  static bool Load(const std::string &atlasPath, const std::string &metricsBinPath,
                   MSDFAtlasCompression compression, MSDFAtlasData &outData);

  static void Unload(MSDFAtlasData &data);

private:
  static std::string ResolveAtlasPath(const std::string &atlasPath,
                                      MSDFAtlasCompression compression);
  static bool LoadMetricsBinary(const std::string &metricsBinPath,
                                MSDFAtlasData &outData);
};

} // namespace NoMoreDay::render

