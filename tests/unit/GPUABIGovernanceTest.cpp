#include "doctest.h"

#include "engine/render/GPUABIContract.hpp"
#include "engine/render/GPUData.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

TEST_CASE("[Unit] GPU ABI - Generated include matches CPU contract") {
  using namespace NoMoreDay::render::abi;

  const ShaderABIInfo info =
      ReadShaderABIInfo("assets/shaders/generated/gpu_abi.glslinc");
  CHECK(info.version == static_cast<int>(GPU_ABI_VERSION));
  CHECK(info.compatMinVersion == static_cast<int>(GPU_ABI_COMPAT_MIN_VERSION));
  CHECK(ValidateGeneratedShaderABI("assets/shaders/generated/gpu_abi.glslinc",
                                   false));
}

TEST_CASE("[Unit] GPU ABI - Version mismatch hard fail policy") {
  using namespace NoMoreDay::render::abi;
  namespace fs = std::filesystem;

  const fs::path path = fs::path("bin") / "tmp_gpu_abi_mismatch.glslinc";
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    REQUIRE(out.is_open());
    out << "#define GPU_ABI_VERSION " << (GPU_ABI_VERSION + 2) << "\n";
    out << "#define GPU_ABI_COMPAT_MIN_VERSION " << (GPU_ABI_VERSION + 1)
        << "\n";
  }

  CHECK(ValidateGeneratedShaderABI(path.string(), false) == false);
  CHECK_THROWS_AS(ValidateGeneratedShaderABI(path.string(), true),
                  std::logic_error);

  std::error_code ec;
  (void)fs::remove(path, ec);
}

TEST_CASE("[Unit] GPU ABI - Generator reproducibility and runtime budget") {
  using clock = std::chrono::steady_clock;
  const auto start = clock::now();
  const int rc = std::system(
      "python tools/render_abi/generate_gpu_abi.py --check > NUL 2>&1");
  const auto end = clock::now();
  const double elapsedMs =
      std::chrono::duration<double, std::milli>(end - start).count();

  CHECK(rc == 0);
  CHECK(elapsedMs <= 2000.0);
}
