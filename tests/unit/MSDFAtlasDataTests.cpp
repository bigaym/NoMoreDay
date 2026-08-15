#include "doctest.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct TestHeader {
  char magic[4] = {};
  uint32_t version = 0;
  uint32_t glyphCount = 0;
  uint32_t recordStrideBytes = 0;
  float atlasWidth = 0.0f;
  float atlasHeight = 0.0f;
  float distanceRange = 0.0f;
};

struct TestRecord {
  uint32_t codepoint = 0;
  float payload[10] = {};
};

std::string FindMetricsBin() {
  const std::array<const char *, 4> kCandidates = {
      "assets/textures/fonts/msdf/v4_msdf_gb2312_4096.metrics.bin",
      "../assets/textures/fonts/msdf/v4_msdf_gb2312_4096.metrics.bin",
      "../../assets/textures/fonts/msdf/v4_msdf_gb2312_4096.metrics.bin",
      "../../../assets/textures/fonts/msdf/v4_msdf_gb2312_4096.metrics.bin",
  };
  for (const char *candidate : kCandidates) {
    std::ifstream probe(candidate, std::ios::binary);
    if (probe.good()) {
      return candidate;
    }
  }
  return {};
}

bool LoadMetricsBin(const std::string &path, TestHeader &header,
                    std::vector<TestRecord> &records) {
  std::ifstream file(path, std::ios::binary);
  if (!file.good()) {
    return false;
  }
  auto readExact = [&file](void *dst, std::streamsize size) {
    file.read(static_cast<char *>(dst), size);
    return file.gcount() == size;
  };
  if (!readExact(header.magic, sizeof(header.magic)) ||
      !readExact(&header.version, sizeof(header.version)) ||
      !readExact(&header.glyphCount, sizeof(header.glyphCount)) ||
      !readExact(&header.recordStrideBytes, sizeof(header.recordStrideBytes)) ||
      !readExact(&header.atlasWidth, sizeof(header.atlasWidth)) ||
      !readExact(&header.atlasHeight, sizeof(header.atlasHeight)) ||
      !readExact(&header.distanceRange, sizeof(header.distanceRange))) {
    return false;
  }
  records.clear();
  records.reserve(header.glyphCount);
  for (uint32_t i = 0; i < header.glyphCount; ++i) {
    TestRecord record;
    if (!readExact(&record, sizeof(record))) {
      return false;
    }
    records.push_back(record);
  }
  return true;
}

} // namespace

TEST_CASE("[Unit] MSDFAtlasData - metrics bin header contract") {
  const std::string path = FindMetricsBin();
  REQUIRE_MESSAGE(!path.empty(), "v4 MSDF metrics.bin not found from test cwd");

  TestHeader header;
  std::vector<TestRecord> records;
  REQUIRE(LoadMetricsBin(path, header, records));

  CHECK(std::string(header.magic, 4) == "MSGM");
  CHECK(header.version == 1u);
  CHECK(header.recordStrideBytes == 44u);
  CHECK(header.atlasWidth == doctest::Approx(4096.0f).epsilon(0.001f));
  CHECK(header.atlasHeight == doctest::Approx(2048.0f).epsilon(0.001f));
  CHECK(header.distanceRange == doctest::Approx(6.0f).epsilon(0.001f));
  CHECK(header.glyphCount == 7537u);
  CHECK(records.size() == header.glyphCount);
}

TEST_CASE("[Unit] MSDFAtlasData - v axis matches flipped y-up atlasBounds") {
  // msdf-atlas-gen writes atlasBounds y-up (origin at atlas bottom) while the
  // atlas texture is sampled y-down. The exporter must flip v: any accidental
  // y-up->y-down regression resurrects the "random radicals" label bug.
  const std::string path = FindMetricsBin();
  REQUIRE_MESSAGE(!path.empty(), "v4 MSDF metrics.bin not found from test cwd");

  TestHeader header;
  std::vector<TestRecord> records;
  REQUIRE(LoadMetricsBin(path, header, records));

  const auto *a = static_cast<const TestRecord *>(nullptr);
  const auto *zhong = static_cast<const TestRecord *>(nullptr);
  for (const TestRecord &record : records) {
    if (record.codepoint == 0x41u) {
      a = &record;
    } else if (record.codepoint == 0x4E2Du) {
      zhong = &record;
    }
  }
  REQUIRE(a != nullptr);
  REQUIRE(zhong != nullptr);

  const float kTol = 0.001f;
  CHECK(a->payload[0] == doctest::Approx(0.994995f).epsilon(kTol));
  CHECK(a->payload[1] == doctest::Approx(0.666260f).epsilon(kTol));
  CHECK(a->payload[2] == doctest::Approx(0.999878f).epsilon(kTol));
  CHECK(a->payload[3] == doctest::Approx(0.679443f).epsilon(kTol));

  CHECK(zhong->payload[0] == doctest::Approx(0.401733f).epsilon(kTol));
  CHECK(zhong->payload[1] == doctest::Approx(0.000244f).epsilon(kTol));
  CHECK(zhong->payload[2] == doctest::Approx(0.408813f).epsilon(kTol));
  CHECK(zhong->payload[3] == doctest::Approx(0.016846f).epsilon(kTol));
}

TEST_CASE("[Unit] MSDFAtlasData - all uv rects stay in unit range") {
  const std::string path = FindMetricsBin();
  REQUIRE_MESSAGE(!path.empty(), "v4 MSDF metrics.bin not found from test cwd");

  TestHeader header;
  std::vector<TestRecord> records;
  REQUIRE(LoadMetricsBin(path, header, records));

  for (const TestRecord &record : records) {
    for (int i = 0; i < 4; ++i) {
      INFO("codepoint ", record.codepoint, " component ", i);
      CHECK(record.payload[i] >= 0.0f);
      CHECK(record.payload[i] <= 1.0f);
    }
    CHECK(record.payload[0] <= record.payload[2]);
    CHECK(record.payload[1] <= record.payload[3]);
  }
}
