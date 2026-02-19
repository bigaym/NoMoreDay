#include "engine/render/resource/MSDFAtlasLoader.hpp"

#include "core/logging/Logger.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace NoMoreDay::render {
namespace {

constexpr std::array<char, 4> kMetricsMagic = {'M', 'S', 'G', 'M'};
constexpr uint32_t kMetricsVersion = 1u;
constexpr uint32_t kRecordStrideBytes = 44u; // uint32 + 10 floats

struct MetricsHeader {
  char magic[4] = {};
  uint32_t version = 0;
  uint32_t glyphCount = 0;
  uint32_t recordStrideBytes = 0;
  float atlasWidth = 0.0f;
  float atlasHeight = 0.0f;
  float distanceRange = 0.0f;
};

bool ReadExact(std::ifstream &stream, void *dst, const std::streamsize size) {
  stream.read(static_cast<char *>(dst), size);
  return stream.gcount() == size;
}

} // namespace

bool MSDFAtlasLoader::Load(const std::string &atlasPath,
                           const std::string &metricsBinPath,
                           const MSDFAtlasCompression compression,
                           MSDFAtlasData &outData) {
  Unload(outData);

  const std::string resolvedAtlasPath = ResolveAtlasPath(atlasPath, compression);
  if (!FileExists(resolvedAtlasPath.c_str())) {
    LOG_ERROR("MSDFAtlasLoader: atlas file missing '{}'", resolvedAtlasPath);
    return false;
  }

  if (!LoadMetricsBinary(metricsBinPath, outData)) {
    LOG_ERROR("MSDFAtlasLoader: metrics load failed '{}'", metricsBinPath);
    return false;
  }

  outData.texture = LoadTexture(resolvedAtlasPath.c_str());
  if (outData.texture.id == 0) {
    LOG_ERROR("MSDFAtlasLoader: texture load failed '{}'", resolvedAtlasPath);
    outData.glyphs.clear();
    return false;
  }

  SetTextureFilter(outData.texture, TEXTURE_FILTER_BILINEAR);
  LOG_INFO("MSDFAtlasLoader: loaded atlas='{}' metrics='{}' glyphs={} size={}x{}",
           resolvedAtlasPath, metricsBinPath, outData.glyphs.size(), outData.width,
           outData.height);
  return true;
}

void MSDFAtlasLoader::Unload(MSDFAtlasData &data) {
  if (data.texture.id != 0) {
    UnloadTexture(data.texture);
    data.texture = {};
  }
  data.width = 0;
  data.height = 0;
  data.distanceRange = 0.0f;
  data.glyphs.clear();
}

std::string MSDFAtlasLoader::ResolveAtlasPath(const std::string &atlasPath,
                                              const MSDFAtlasCompression compression) {
  if (compression == MSDFAtlasCompression::None) {
    return atlasPath;
  }

  const std::filesystem::path source(atlasPath);
  std::filesystem::path candidate = source;
  if (compression == MSDFAtlasCompression::BC4) {
    candidate.replace_extension(".bc4.dds");
  } else if (compression == MSDFAtlasCompression::BC5) {
    candidate.replace_extension(".bc5.dds");
  }

  if (std::filesystem::exists(candidate)) {
    return candidate.string();
  }

  LOG_WARN("MSDFAtlasLoader: requested compression asset missing '{}', fallback to '{}'",
           candidate.string(), atlasPath);
  return atlasPath;
}

bool MSDFAtlasLoader::LoadMetricsBinary(const std::string &metricsBinPath,
                                        MSDFAtlasData &outData) {
  std::ifstream file(metricsBinPath, std::ios::binary);
  if (!file.is_open()) {
    LOG_ERROR("MSDFAtlasLoader: failed to open metrics '{}'", metricsBinPath);
    return false;
  }

  MetricsHeader header = {};
  if (!ReadExact(file, header.magic, static_cast<std::streamsize>(sizeof(header.magic))) ||
      !ReadExact(file, &header.version, static_cast<std::streamsize>(sizeof(header.version))) ||
      !ReadExact(file, &header.glyphCount, static_cast<std::streamsize>(sizeof(header.glyphCount))) ||
      !ReadExact(file, &header.recordStrideBytes,
                 static_cast<std::streamsize>(sizeof(header.recordStrideBytes))) ||
      !ReadExact(file, &header.atlasWidth, static_cast<std::streamsize>(sizeof(header.atlasWidth))) ||
      !ReadExact(file, &header.atlasHeight, static_cast<std::streamsize>(sizeof(header.atlasHeight))) ||
      !ReadExact(file, &header.distanceRange,
                 static_cast<std::streamsize>(sizeof(header.distanceRange)))) {
    LOG_ERROR("MSDFAtlasLoader: failed to read metrics header '{}'", metricsBinPath);
    return false;
  }

  if (!std::equal(std::begin(kMetricsMagic), std::end(kMetricsMagic),
                  std::begin(header.magic))) {
    LOG_ERROR("MSDFAtlasLoader: invalid metrics magic in '{}'", metricsBinPath);
    return false;
  }

  if (header.version != kMetricsVersion) {
    LOG_ERROR("MSDFAtlasLoader: unsupported metrics version {} in '{}'", header.version,
              metricsBinPath);
    return false;
  }

  if (header.recordStrideBytes != kRecordStrideBytes) {
    LOG_ERROR("MSDFAtlasLoader: unsupported record stride {} in '{}'",
              header.recordStrideBytes, metricsBinPath);
    return false;
  }

  outData.width = static_cast<int>(header.atlasWidth);
  outData.height = static_cast<int>(header.atlasHeight);
  outData.distanceRange = header.distanceRange;
  outData.glyphs.clear();
  outData.glyphs.reserve(static_cast<size_t>(header.glyphCount));

  struct RawRecord {
    uint32_t codepoint = 0;
    float payload[10] = {};
  };

  for (uint32_t i = 0; i < header.glyphCount; ++i) {
    RawRecord raw = {};
    if (!ReadExact(file, &raw, static_cast<std::streamsize>(sizeof(raw)))) {
      LOG_ERROR("MSDFAtlasLoader: truncated metrics '{}' at record {}", metricsBinPath, i);
      outData.glyphs.clear();
      return false;
    }

    MSDFGlyphMetric metric = {};
    metric.codepoint = raw.codepoint;
    metric.uvRect[0] = raw.payload[0];
    metric.uvRect[1] = raw.payload[1];
    metric.uvRect[2] = raw.payload[2];
    metric.uvRect[3] = raw.payload[3];
    metric.bearing[0] = raw.payload[4];
    metric.bearing[1] = raw.payload[5];
    metric.size[0] = raw.payload[6];
    metric.size[1] = raw.payload[7];
    metric.advance = raw.payload[8];
    outData.glyphs.push_back(metric);
  }

  return true;
}

} // namespace NoMoreDay::render
