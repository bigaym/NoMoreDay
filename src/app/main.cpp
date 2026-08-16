#include "app/Game.hpp"
#include "core/logging/Logger.hpp"
#include "core/logging/CrashHandler.hpp"
#include "engine/render/validation/GPUHardwareValidationGate.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

// Diagnostic build: Force asset sync for particle fix
#if defined(_WIN32) && defined(__GNUC__)
#include <windows.h>
extern int main(int argc, char* argv[]);
extern "C" int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return main(__argc, __argv);
}
#endif

namespace {

// MS-8 W6 (M0-C): --gpu-gate parameter contract. argv takes precedence over the
// NMD_GATE_* environment variables (the cross-boundary mechanism shared with
// the Python runner); missing values fall back to the S8 defaults.
struct GpuGateArgs {
    std::string revision = "HEAD";
    int sampleFrames = 120;
    int toggleLoops = 100;
    bool stressTest1Min = true;
};

int GateEnvIntOr(const char* name, int defaultValue) {
    const char* raw = std::getenv(name);
    if (raw == nullptr || *raw == '\0') {
        return defaultValue;
    }
    char* end = nullptr;
    const long value = std::strtol(raw, &end, 10);
    if (end == raw || *end != '\0') {
        return defaultValue;
    }
    return static_cast<int>(value);
}

bool GateEnvBoolOr(const char* name, bool defaultValue) {
    const char* raw = std::getenv(name);
    if (raw == nullptr || *raw == '\0') {
        return defaultValue;
    }
    const std::string value(raw);
    return value == "1" || value == "true" || value == "TRUE";
}

// W6 (M0-C): UTC timestamp in the same format GateReport uses, so even the
// startup-exception NOT_RUN report carries a valid `timestamp`.
std::string UtcNowIso() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    struct tm tmBuf {};
#if defined(_WIN32)
    gmtime_s(&tmBuf, &nowTime);
#else
    gmtime_r(&nowTime, &tmBuf);
#endif
    char timeBuf[64] = {0};
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%SZ", &tmBuf);
    return std::string(timeBuf);
}

GpuGateArgs ParseGpuGateArgs(int argc, char* argv[]) {
    GpuGateArgs args;
    args.sampleFrames = GateEnvIntOr("NMD_GATE_SAMPLES", args.sampleFrames);
    args.toggleLoops = GateEnvIntOr("NMD_GATE_TOGGLE_LOOPS", args.toggleLoops);
    args.stressTest1Min = GateEnvBoolOr("NMD_GATE_STRESS", args.stressTest1Min);

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--revision" && i + 1 < argc) {
            args.revision = argv[++i];
        } else if (arg == "--samples" && i + 1 < argc) {
            args.sampleFrames = std::atoi(argv[++i]);
        } else if (arg == "--toggle-loops" && i + 1 < argc) {
            args.toggleLoops = std::atoi(argv[++i]);
        } else if (arg == "--stress-test-1min") {
            args.stressTest1Min = true;
        } else if (arg == "--no-stress-test-1min") {
            args.stressTest1Min = false;
        }
    }
    return args;
}

} // namespace

int main(int argc, char* argv[]) {
    NoMoreDay::CrashHandler::Init(); // 初始化崩溃捕获
    tools::Logger::Init();
    LOG_INFO("Initializing NoMoreDay Engine...");

    // Non-interactive smoke entry: prove the Release executable launches and
    // exits cleanly without creating a window or GL context (used by the
    // MS-8 W4 Release/LTO verification harness).
    if (argc > 1 && std::string(argv[1]) == "--smoke-test") {
        LOG_INFO("NoMoreDay smoke-test OK");
        tools::Logger::Shutdown();
        return 0;
    }

    // MS-8 W6 (M0-C): production GPU hardware validation gate. Runs after the
    // normal Game/App startup initialization completes (same constructor path
    // the game uses, so real GL context/registry/hooks/render path are live)
    // and drives the gate through the game-binary FixtureRenderDriver. The
    // process exits after emitting exactly one status marker + versioned JSON
    // report; the Python runner decides pass/fail from the artifact.
    if (argc > 1 && std::string(argv[1]) == "--gpu-gate") {
        const GpuGateArgs args = ParseGpuGateArgs(argc, argv);
        try {
            // Normal initialization (window + GL context + all GPU systems).
            Game game(2560, 1440, "NoMoreDay - GPU Hardware Validation Gate");
            return game.runGpuGate(args.revision, args.sampleFrames,
                                   args.stressTest1Min, args.toggleLoops);
        } catch (const std::exception& ex) {
            // W6 (M0-C) High-4: the startup-exception path also emits a
            // complete, versioned NOT_RUN report (marker + BEGIN/JSON/END) so
            // every invocation yields exactly one marker and one report and the
            // runner can never mistake a missing report for a pass. The report
            // records the failure reason and the available provenance.
            NoMoreDay::render::validation::GateReport report;
            report.revision = args.revision;
            report.timestamp = UtcNowIso();
            report.requestedSampleFrames = args.sampleFrames;
            report.requestedToggleLoops = args.toggleLoops;
            report.actualSampleFrames = args.sampleFrames;
            report.actualToggleLoops = args.toggleLoops;
            report.nonExhaustive = (args.sampleFrames < 120 || args.toggleLoops < 100);
            report.occupancyStatus = "missing_pending_m0a";
            report.occupancyReason =
                "Gate aborted during startup; occupancy/disocclusion probes "
                "pending M0-A R3 (fail-closed)";
            report.status = NoMoreDay::render::validation::GateStatus::NotRun;
            report.globalFailures.push_back(
                "GPU hardware validation gate aborted during startup: " +
                std::string(ex.what()));
            std::cout << "GPU_HARDWARE_GATE_RESULT status=NOT_RUN\n";
            std::cout << "GPU_HARDWARE_GATE_REPORT_BEGIN\n"
                      << report.ToJsonString() << "\n"
                      << "GPU_HARDWARE_GATE_REPORT_END\n"
                      << std::flush;
            LOG_ERROR("GPU hardware validation gate aborted during startup: {}",
                      ex.what());
            tools::Logger::Shutdown();
            return 1;
        }
    }

    try {
        // 确保 Game 对象在 Logger::Shutdown 之前析构
        // Create Game Instance
        Game game(2560, 1440, "NoMoreDay - High Performance ECS");
        
        // Run Loop
        game.run();
    } catch (const std::exception &ex) {
        LOG_CRITICAL("Game initialization/execution failed: {}", ex.what());
        tools::Logger::Shutdown();
        return 1;
    }

    LOG_INFO("Engine Shutdown.");
    tools::Logger::Shutdown();
    return 0;
}