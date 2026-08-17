#include "doctest.h"

#include "engine/render/GPUData.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

TEST_CASE("[Unit] GPUPackedEntityInstance - 32B ABI Layout & Assertions") {
  using namespace NoMoreDay::components;

  CHECK(std::is_standard_layout_v<GPUPackedEntityInstance>);
  CHECK(sizeof(GPUPackedEntityInstance) == 32);
  CHECK(alignof(GPUPackedEntityInstance) == 16);

  CHECK(offsetof(GPUPackedEntityInstance, position) == 0);
  CHECK(offsetof(GPUPackedEntityInstance, prevPosition) == 8);
  CHECK(offsetof(GPUPackedEntityInstance, words) == 16);
  CHECK(sizeof(GPUPackedEntityInstance::words) == 16);
}

TEST_CASE("[Unit] GPUPackedEntityInstance - Position Precision (Float32)") {
  using namespace NoMoreDay::components;

  GPUPackedEntityInstance inst;
  // Test large world coordinates where fp16 has ULP 1.0~2.0
  const float largeX = 2048.5f;
  const float largeY = 4095.75f;
  const float prevX = 2047.25f;
  const float prevY = 4094.125f;

  inst.position = Vector2{largeX, largeY};
  inst.prevPosition = Vector2{prevX, prevY};

  CHECK(inst.position.x == largeX);
  CHECK(inst.position.y == largeY);
  CHECK(inst.prevPosition.x == prevX);
  CHECK(inst.prevPosition.y == prevY);

  // Interpolation check
  const float alpha = 0.5f;
  const float interpX = inst.prevPosition.x * (1.0f - alpha) + inst.position.x * alpha;
  const float interpY = inst.prevPosition.y * (1.0f - alpha) + inst.position.y * alpha;
  CHECK(doctest::Approx(interpX).epsilon(0.0001f) == (prevX + largeX) * 0.5f);
  CHECK(doctest::Approx(interpY).epsilon(0.0001f) == (prevY + largeY) * 0.5f);
}

TEST_CASE("[Unit] GPUPackedEntityInstance - Rotation SNorm16 Encoding & Thresholds") {
  using namespace NoMoreDay::components::GPUPackedInstanceUtils;

  // Test zero rotation (identity)
  {
    float angle = 0.0f;
    float s = std::sin(angle);
    float c = std::cos(angle);
    int16_t snormSin = FloatToSNorm16(s);
    int16_t snormCos = FloatToSNorm16(c);
    CHECK(snormSin == 0);
    CHECK(snormCos == 32767);

    float decSin = SNorm16ToFloat(snormSin);
    float decCos = SNorm16ToFloat(snormCos);
    CHECK(doctest::Approx(decSin).epsilon(0.001f) == 0.0f);
    CHECK(doctest::Approx(decCos).epsilon(0.001f) == 1.0f);
  }

  // Test 90 degrees, 180 degrees, -90 degrees, 45 degrees
  const float angles[] = {0.0f, 0.785398f, 1.570796f, 3.1415926f, -1.570796f, -2.356194f};
  for (float angle : angles) {
    float s = std::sin(angle);
    float c = std::cos(angle);
    int16_t snormSin = FloatToSNorm16(s);
    int16_t snormCos = FloatToSNorm16(c);

    float decSin = SNorm16ToFloat(snormSin);
    float decCos = SNorm16ToFloat(snormCos);

    CHECK(std::abs(decSin - s) <= (1.0f / 32767.0f + 0.0001f));
    CHECK(std::abs(decCos - c) <= (1.0f / 32767.0f + 0.0001f));
  }

  // Test velocity threshold (< 0.1 length -> 0 rotation)
  {
    Vector2 velLow{0.05f, 0.05f}; // length ~ 0.0707 < 0.1
    float len = std::sqrt(velLow.x * velLow.x + velLow.y * velLow.y);
    float rotation = (len > 0.1f) ? std::atan2(velLow.y, velLow.x) : 0.0f;
    CHECK(rotation == 0.0f);
  }
  {
    Vector2 velHigh{1.0f, 0.0f}; // length 1.0 > 0.1
    float len = std::sqrt(velHigh.x * velHigh.x + velHigh.y * velHigh.y);
    float rotation = (len > 0.1f) ? std::atan2(velHigh.y, velHigh.x) : 0.0f;
    CHECK(rotation == 0.0f);
  }
}

TEST_CASE("[Unit] GPUPackedEntityInstance - Binary16 Radius & TextureId Word1") {
  using namespace NoMoreDay::components::GPUPackedInstanceUtils;

  // Test zero radius
  {
    uint16_t h = FloatToHalf(0.0f);
    CHECK(HalfToFloat(h) == 0.0f);
  }

  // Test standard render radius range (e.g. radius 16.0 * 4.0 = 64.0)
  const float radii[] = {0.5f, 1.0f, 4.0f, 16.0f, 64.0f, 128.0f, 256.0f, 1024.0f};
  for (float r : radii) {
    float renderRadius = r * 4.0f;
    uint16_t h = FloatToHalf(renderRadius);
    float decoded = HalfToFloat(h);
    CHECK(doctest::Approx(decoded).epsilon(0.002f) == renderRadius);
  }

  // Test textureId packing: [0..65535] and -1 (0xFFFF)
  {
    uint16_t tex0 = 0;
    uint16_t tex42 = 42;
    uint16_t texMax = 65535;

    uint32_t w1_0 = FloatToHalf(64.0f) | (static_cast<uint32_t>(tex0) << 16);
    uint32_t w1_42 = FloatToHalf(64.0f) | (static_cast<uint32_t>(tex42) << 16);
    uint32_t w1_sdf = FloatToHalf(64.0f) | (static_cast<uint32_t>(texMax) << 16);

    CHECK((w1_0 >> 16) == 0);
    CHECK((w1_42 >> 16) == 42);
    CHECK((w1_sdf >> 16) == 65535);

    // Decode check in shader convention
    uint32_t rawTexSdf = (w1_sdf >> 16) & 0xFFFFu;
    int vTextureIndexSdf = (rawTexSdf == 0xFFFFu) ? -1 : static_cast<int>(rawTexSdf);
    CHECK(vTextureIndexSdf == -1);

    uint32_t rawTex42 = (w1_42 >> 16) & 0xFFFFu;
    int vTextureIndex42 = (rawTex42 == 0xFFFFu) ? -1 : static_cast<int>(rawTex42);
    CHECK(vTextureIndex42 == 42);
  }
}

TEST_CASE("[Unit] GPUPackedEntityInstance - Word2 (MaterialId, Glow UNORM8, StatusMask) & Word3") {
  using namespace NoMoreDay::components::GPUFlags;

  uint32_t flags = 0;
  PackMaterialId(flags, 25);
  flags |= (1 << 0); // KINEMATIC bit

  int unpackedMat = UnpackMaterialId(flags);
  CHECK(unpackedMat == 25);

  uint16_t materialId = static_cast<uint16_t>(unpackedMat);
  float glowIntensity = 0.75f;
  uint8_t glowU8 = static_cast<uint8_t>(glowIntensity * 255.0f + 0.5f);
  uint8_t statusMask = 0x03; // Frozen (1) | Burning (2)

  uint32_t w2 = (materialId & 0xFFFFu) | (static_cast<uint32_t>(glowU8) << 16) | (static_cast<uint32_t>(statusMask) << 24);
  uint32_t w3 = flags & 0xFFFFu;

  // Shader decode check
  uint32_t decMaterialId = w2 & 0xFFFFu;
  float decGlow = static_cast<float>((w2 >> 16) & 0xFFu) / 255.0f;
  uint32_t decStatusMask = (w2 >> 24) & 0xFFu;
  uint32_t vFlags = (w3 & 0xFFFFu) | (decMaterialId << 16);

  CHECK(decMaterialId == 25u);
  CHECK(doctest::Approx(decGlow).epsilon(0.01f) == 0.75f);
  CHECK(decStatusMask == 0x03u);
  CHECK(UnpackMaterialId(vFlags) == 25);
  CHECK((vFlags & 0x1) == 0x1);
}

TEST_CASE("[Unit] GPUPackedEntityInstance - Fail-Closed Radius Half (H8/H9)") {
  using namespace NoMoreDay::components::GPUPackedInstanceUtils;

  // NaN / +inf / -inf / negative flush to +0, matching instance_pack.compute
  // (isnan || isinf || < 0.0 -> 0.0) — never a NaN/negative half.
  CHECK(FloatToHalf(std::numeric_limits<float>::quiet_NaN()) == 0);
  CHECK(FloatToHalf(std::numeric_limits<float>::infinity()) == 0);
  CHECK(FloatToHalf(-std::numeric_limits<float>::infinity()) == 0);
  CHECK(FloatToHalf(-3.5f) == 0);
  CHECK(HalfToFloat(FloatToHalf(-3.5f)) == 0.0f);

  // Above half-float max clamps to 65504.0 (half 0x7BFF), never an inf half.
  CHECK(FloatToHalf(65504.0f) == 0x7BFF);
  CHECK(FloatToHalf(65504.5f) == 0x7BFF);
  CHECK(FloatToHalf(70000.0f) == 0x7BFF);
  CHECK(HalfToFloat(FloatToHalf(70000.0f)) == 65504.0f);

  // Normal path unaffected: exact zero and unit radius round-trip.
  CHECK(FloatToHalf(0.0f) == 0);
  CHECK(HalfToFloat(FloatToHalf(0.0f)) == 0.0f);
  CHECK(HalfToFloat(FloatToHalf(1.0f)) == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] GPUPackedEntityInstance - TextureId Out-of-Range Fail-Closed (H9)") {
  using namespace NoMoreDay::components::GPUPackedInstanceUtils;

  // Mirrors instance_pack.compute textureId selection (AD-7 plan §1.3):
  // type == -1 -> 0xFFFF (SDF circle); 0 <= type <= 65535 -> type; else 0xFFFF.
  auto packTextureId = [](int type) -> uint16_t {
    if (type == -1) return 0xFFFFu;
    if (type >= 0 && type <= 65535) return static_cast<uint16_t>(type);
    return 0xFFFFu;
  };

  CHECK(packTextureId(-1) == 0xFFFFu);
  CHECK(packTextureId(0) == 0u);
  CHECK(packTextureId(65535) == 0xFFFFu);
  CHECK(packTextureId(65536) == 0xFFFFu); // > 65535 -> fail-closed sentinel
  CHECK(packTextureId(-2) == 0xFFFFu);    // negative (non -1) -> fail-closed
  CHECK(packTextureId(999999) == 0xFFFFu);
}

TEST_CASE("[Unit] GPUPackedEntityInstance - Packing Parity (NO_ROTATION, Glow Clamp, Status Mask) (H9)") {
  using namespace NoMoreDay::components::GPUPackedInstanceUtils;

  // NO_ROTATION_MASK = bit 3 (GPU_ENTITY_FLAG_NO_ROTATION, contract)
  constexpr uint32_t NO_ROTATION_MASK = 8u;

  // NO_ROTATION set -> rotation stays 0 even with strong velocity
  {
    Vector2 vel{1.0f, 0.0f};
    uint32_t flags = NO_ROTATION_MASK;
    float rotation = 0.0f;
    if ((flags & NO_ROTATION_MASK) == 0u) {
      if (std::sqrt(vel.x * vel.x + vel.y * vel.y) > 0.1f) {
        rotation = std::atan2(vel.y, vel.x);
      }
    }
    CHECK(rotation == 0.0f);

    uint32_t w0 = (static_cast<uint32_t>(static_cast<uint16_t>(FloatToSNorm16(std::cos(rotation)))) << 16) |
                  static_cast<uint32_t>(static_cast<uint16_t>(FloatToSNorm16(std::sin(rotation))));
    // Shader decode (unpackSnorm2x16)
    float decSin = SNorm16ToFloat(static_cast<int16_t>(w0 & 0xFFFFu));
    float decCos = SNorm16ToFloat(static_cast<int16_t>((w0 >> 16) & 0xFFFFu));
    CHECK(doctest::Approx(decSin).epsilon(0.001f) == 0.0f);
    CHECK(doctest::Approx(decCos).epsilon(0.001f) == 1.0f);
  }

  // Glow out-of-range clamps to [0,1] then UNORM8 [0,255] (shader clamp/round)
  {
    auto glowU8 = [](float glow) -> uint8_t {
      float clamped = glow < 0.0f ? 0.0f : (glow > 1.0f ? 1.0f : glow);
      return static_cast<uint8_t>(clamped * 255.0f + 0.5f);
    };
    CHECK(glowU8(2.0f) == 255); // >1 clamps to 255
    CHECK(glowU8(-1.0f) == 0);  // <0 clamps to 0
    CHECK(glowU8(1.0f) == 255);
    CHECK(glowU8(0.0f) == 0);
    CHECK(glowU8(0.75f) == 191); // 0.75*255 = 191.25 -> 191
  }

  // activeStatusMask is masked to its low 8 bits in w2
  {
    uint32_t activeStatusMask = 0x1FFu;
    uint32_t statusU8 = activeStatusMask & 0xFFu;
    CHECK(statusU8 == 0xFFu);
  }
}

TEST_CASE("[Unit] GPUPackedEntityInstance - Golden (pack parity, GPU) [NOT_RUN]") {
  using namespace NoMoreDay::components;
  using namespace NoMoreDay::components::GPUPackedInstanceUtils;

  // Golden verification — rendering before/after instance packing must be
  // pixel-identical — requires a GPU and a full render frame. CI has no GPU, so
  // the real golden comparison is NOT_RUN here and MUST NOT be faked.
  // Manual verification steps:
  //   1. Run the game (RelWithDebInfo build) with a fixed camera + world seed.
  //   2. Capture a reference frame (legacy 128B entity path), then a frame with
  //      the AD-7 packed path (instance_pack.compute + entity_mdi.vert) for the
  //      same inputs.
  //   3. Compare per-pixel: expect identical output except radius quantization
  //      (half-float w1, see Binary16 Radius test, epsilon 0.002).
  //   4. Attach screenshots + diff to the review as golden evidence.
  INFO("Golden (pack parity) requires GPU; NOT_RUN in CI. Manual steps: see test comment.");
  INFO("CPU-side anchor: FloatToHalf(radius*4) round-trips within 0.002 (Binary16 Radius test).");

  // CPU-side skeleton: encode one reference instance per the AD-7 mapping table
  // (plan §1.3) so the word contract stays executable without a GPU.
  GPUPackedEntityInstance inst;
  inst.position = Vector2{100.0f, 200.0f};
  inst.prevPosition = Vector2{99.5f, 199.5f};

  uint32_t flags = 8u; // NO_ROTATION (bit 3)
  flags |= (static_cast<uint32_t>(7) << 16); // materialId 7

  const float radius = 16.0f;
  float renderRadius = radius * 4.0f;
  uint32_t w0 = (static_cast<uint32_t>(static_cast<uint16_t>(FloatToSNorm16(1.0f))) << 16) |
                static_cast<uint32_t>(static_cast<uint16_t>(FloatToSNorm16(0.0f)));
  uint32_t w1 = FloatToHalf(renderRadius) | (0xFFFFu << 16u); // type == -1 (SDF circle)
  uint32_t w2 = static_cast<uint16_t>((flags >> 16u) & 0xFFFFu);
  uint32_t w3 = flags & 0xFFFFu;
  inst.words[0] = w0;
  inst.words[1] = w1;
  inst.words[2] = w2;
  inst.words[3] = w3;

  CHECK(inst.words[0] != 0u);
  CHECK((inst.words[1] & 0xFFFFu) == FloatToHalf(64.0f));
  CHECK(((inst.words[1] >> 16) & 0xFFFFu) == 0xFFFFu);
  CHECK((inst.words[2] & 0xFFFFu) == 7u);
  CHECK((inst.words[3] & 0xFFFFu) == 8u);
}
