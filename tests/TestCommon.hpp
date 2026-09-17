#pragma once

#include "core/logging/Logger.hpp"
#include "doctest.h"
#include "game/systems/item/ItemFactory.hpp" // If TestSetupScope uses it
#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <system_error>
#include <vector>

using namespace NoMoreDay;

// 从构建产物强制重载 Modifier Runtime V2 registry。
// 该 registry 是进程级单例，可能被其它用例注入的合成数据覆盖；
// 依赖真实生成数据的用例（地图/怪物适配器、AttributePipeline、怪物词缀行为）
// 应在用例开始时显式重载，避免受执行顺序影响。
//
// 失败原因区分“资产缺失”与“解析失败”并各自 WARN 输出，便于在未生成 .bin
// 的机器上定位；资产缺失时提示先运行生成脚本。
inline bool ReloadModifierRuntimeFromAsset() {
  constexpr const char *kAssetPath = "assets/generated/modifier_runtime_v2.bin";
  NoMoreDay::ModifierRuntimeRegistry &registry =
      NoMoreDay::ModifierRuntimeRegistry::Get();

  std::error_code ec;
  if (!std::filesystem::exists(kAssetPath, ec)) {
    DOCTEST_WARN_MESSAGE(
        false, "modifier runtime asset missing: " << kAssetPath
               << " (run: python scripts/gen_modifier_runtime_v2.py --build)");
    return false;
  }

  if (!registry.Reload(kAssetPath)) {
    DOCTEST_WARN_MESSAGE(
        false, "modifier runtime asset failed to parse: " << kAssetPath);
    return false;
  }

  return true;
}

// RAII Helper for Logger
struct LoggerScope {
  LoggerScope() { tools::Logger::Init(); }
  ~LoggerScope() { /* tools::Logger::Shutdown(); */ }
};

#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/contracts/impl/CombatTelemetry.hpp"
#include "game/contracts/impl/ProcBudgetManager.hpp"
#include "game/contracts/impl/StatsSystem.hpp"
#include "game/application/persistence/SaveManager.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"

// RAII Helper for Logger and ItemFactory
//
// settings.json 还原契约：部分用例（如 QualityTierManager::Initialize）会把
// 基准分数/时间戳写回仓库根的 settings.json。本夹具在构造时对原始字节做快照，
// 仅在析构时确认内容确实变化才写回，避免内容未变时的无条件写回（会刷新 mtime、
// 掩盖真实改动）。若夹具未能生效（例如进程被强杀），仍以提交前手动执行
// `git checkout -- settings.json` 作为兜底。
//
// 覆盖门限：本夹具是进程级且仅保护显式声明 TestSetupScope 的用例。同一进程内
// 未被覆盖的写入点不会被还原，甚至会被后续作用域在构造时当作"原始内容"快照固化，
// 因此写入点必须逐一穷举接入：显式字面量传参 / 无参默认实参调用 / 经其它入口的间接调用。
struct TestSetupScope {
  TestSetupScope() {
    SnapshotSettingsFile();
    tools::Logger::Init();
    ItemFactory::initialize();
    ProcBudgetManager::Get().ResetForTests();
    CombatEventDispatcher::Init();
    CombatTelemetry::Get().ResetForTests();
    CombatTelemetry::Get().SetRuntimeEnabled(false);
    StatsSystem::Reset(); // Clear static cache from previous tests
    ResetSkillRegistries();
  }
  ~TestSetupScope() {
    ProcBudgetManager::Get().ResetForTests();
    CombatTelemetry::Get().ResetForTests();
    CombatTelemetry::Get().SetRuntimeEnabled(false);
    StatsSystem::Reset(); // Clean up
    ResetSkillRegistries();
    RestoreSettingsFileIfChanged();
                          // tools::Logger::Shutdown();
  }

private:
  static constexpr const char *kSettingsFilePath = "settings.json";

  // 构造快照：记录 settings.json 是否原本存在及其原始字节。
  // 状态机不变量：m_settingsFileExisted 一旦置位即表示"原文件在场"，此后任何
  // 放弃分支都不得再走析构期的"删除用例产物"路径，否则会把真实文件当产物删除。
  void SnapshotSettingsFile() {
    std::error_code ec;
    if (!std::filesystem::exists(kSettingsFilePath, ec)) {
      return; // 原本不存在：析构时清理用例新建的文件。
    }
    m_settingsFileExisted = true;
    const std::uintmax_t expectedSize =
        std::filesystem::file_size(kSettingsFilePath, ec);
    if (ec) {
      m_settingsSnapshotAbandoned = true; // 无法取得大小：放弃快照（原文件保持不动）。
      return;
    }
    std::ifstream in(kSettingsFilePath, std::ios::binary);
    if (!in.is_open()) {
      m_settingsSnapshotAbandoned = true; // 存在但不可读：放弃快照（原文件保持不动）。
      return;
    }
    m_settingsFileBytes.assign(std::istreambuf_iterator<char>(in),
                               std::istreambuf_iterator<char>());
    // istreambuf_iterator 遇 I/O 错误会静默结束，可能只读到半截；必须按文件大小
    // 校验完整性，否则析构时会用残缺快照覆盖原文件（正是本守卫要防的损坏）。
    if (in.bad() || m_settingsFileBytes.size() != expectedSize) {
      m_settingsFileBytes.clear();
      m_settingsSnapshotAbandoned = true;
      return;
    }
    in.close();
  }

  // 析构还原：仅当当前内容与快照不同才写回原始字节；内容一致时禁止写回。
  // 快照时文件不存在（干净的检出/CI）时，需要删除用例新建的产物（如
  // QualityTierManager 空对象首写），否则仓库根会留下未跟踪文件。
  // 快照放弃时既不删除也不写回，仅告警提示人工兜底。
  void RestoreSettingsFileIfChanged() const {
    if (!m_settingsFileExisted) {
      std::error_code ec;
      std::filesystem::remove(kSettingsFilePath, ec);
      if (ec) {
        DOCTEST_WARN_MESSAGE(false, "settings.json cleanup failed: "
                                        << ec.message()
                                        << "; remove it manually");
      }
      return;
    }
    if (m_settingsSnapshotAbandoned) {
      DOCTEST_WARN_MESSAGE(
          false, "settings.json snapshot unavailable; run: git checkout -- "
                 << kSettingsFilePath);
      return;
    }
    std::vector<char> currentBytes;
    std::ifstream in(kSettingsFilePath, std::ios::binary);
    if (in.is_open()) {
      currentBytes.assign(std::istreambuf_iterator<char>(in),
                          std::istreambuf_iterator<char>());
      if (currentBytes == m_settingsFileBytes) {
        return; // 内容未变：保持文件原样，避免刷新 mtime 并掩盖真实改动。
      }
    }
    // 内容被用例改写（或文件已被删除）时恢复快照字节。
    std::ofstream out(kSettingsFilePath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
      DOCTEST_WARN_MESSAGE(
          false, "settings.json restore failed to open; run: git checkout -- "
                 << kSettingsFilePath);
      return;
    }
    out.write(m_settingsFileBytes.data(),
              static_cast<std::streamsize>(m_settingsFileBytes.size()));
    out.flush();
    if (!out.good()) {
      DOCTEST_WARN_MESSAGE(
          false, "settings.json restore failed; run: git checkout -- "
                 << kSettingsFilePath);
    }
  }

  bool m_settingsFileExisted = false;
  bool m_settingsSnapshotAbandoned = false;
  std::vector<char> m_settingsFileBytes;

  // 技能相关单例是进程级状态：用例注册的临时技能/机制值必须显式复位，
  // 否则会跨用例泄漏（如 BladeWard470 的偶发失败）。
  static void ResetSkillRegistries() {
    // 机制数值表提供专用测试复位接口；技能行为表仅由静态初始化注册，
    // 无按用例可变数据，故无需（也不可）在此清空。
    data::SkillMechanicsRegistry::Get().ResetForTests();
    // 技能数据表没有独立清空接口，用权威数据重载以清除用例注册的临时技能与技能树。
    // LoadFromJson 无返回值：加载失败会静默清空数据表，故显式断言非空防止空跑。
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    CHECK_FALSE(SkillRegistry::Get().GetAllSkills().empty());
  }
};
