#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_set>

namespace NoMoreDay {

/**
 * @brief 缺失 UMR 记录的一次性、有上限告警器。
 *
 * 适配器在失败分支调用：同一 recordId 只报告一次，且累计报告数有上限，
 * 避免热路径每帧刷屏；同时允许报告多个不同 recordId，避免多点失效被
 * 误判为单点。
 *
 * 线程安全：使用原子计数做无锁快路径，仅在未达上限时才进入互斥区做去重，
 * 因此成功路径（不调用本类）零开销，失败路径一次锁一次集合插入。
 */
class ModifierRuntimeMissingRecordWarnLimiter {
public:
  // 单次运行最多报告的缺失记录条数，防止大量失效时日志被淹没。
  static constexpr uint32_t kMaxReports = 8;

  // 返回 true 表示本次应打印告警（调用方随后自行 LOG_WARN）；
  // 返回 false 表示该 recordId 已报告过或已达上报上限。
  [[nodiscard]] bool ShouldReport(const uint32_t recordId) {
    if (m_reportedCount.load(std::memory_order_relaxed) >= kMaxReports) {
      return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_reportedIds.size() >= kMaxReports) {
      m_reportedCount.store(kMaxReports, std::memory_order_relaxed);
      return false;
    }

    const bool inserted = m_reportedIds.insert(recordId).second;
    if (inserted) {
      m_reportedCount.fetch_add(1u, std::memory_order_relaxed);
    }
    return inserted;
  }

private:
  std::mutex m_mutex;
  std::unordered_set<uint32_t> m_reportedIds;
  std::atomic<uint32_t> m_reportedCount{0};
};

} // namespace NoMoreDay
