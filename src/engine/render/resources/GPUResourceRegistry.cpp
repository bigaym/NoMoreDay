#include "engine/render/resources/GPUResourceRegistry.hpp"

#include <algorithm>
#include <sstream>

namespace NoMoreDay::render::resources {

namespace {
// References created within this many frames are still considered pending
// (not yet quiesced). Matches the timer-query pending-overage window
// (3 x GPUTimerQueryRing::kRingDepth = 9 frames).
constexpr uint64_t kPendingReferenceFrames = 9;
} // namespace

GPUResourceRegistry &GPUResourceRegistry::Get() {
  static GPUResourceRegistry instance;
  return instance;
}

void GPUResourceRegistry::RegisterResource(uint32_t handle, graph::ResourceKind kind,
                                            graph::RenderOwnerTag ownerTag, size_t sizeBytes,
                                            std::string_view name) {
  if (handle == 0) return;
  std::lock_guard<std::mutex> lock(m_mutex);

  const uint64_t key = MakeKey(handle, kind);
  GPUResourceRecord record = {};
  record.handle = handle;
  record.kind = kind;
  record.ownerTag = ownerTag;
  record.sizeBytes = sizeBytes;
  record.name = name;
  record.creationFrame = m_currentFrame;

  m_records[key] = record;
  m_stats.currentTotalBytes += sizeBytes;
  m_stats.peakTotalBytes = std::max(m_stats.peakTotalBytes, m_stats.currentTotalBytes);
  m_stats.activeCount++;
  m_stats.totalCreatedCount++;
  m_stats.bytesByKind[static_cast<uint8_t>(kind)] += sizeBytes;
  m_stats.bytesByOwner[static_cast<uint8_t>(ownerTag)] += sizeBytes;
}

void GPUResourceRegistry::UnregisterResource(uint32_t handle, graph::ResourceKind kind) {
  if (handle == 0) return;
  std::lock_guard<std::mutex> lock(m_mutex);

  const uint64_t key = MakeKey(handle, kind);
  auto it = m_records.find(key);
  if (it != m_records.end()) {
    const size_t bytes = it->second.sizeBytes;
    const auto ownerTag = it->second.ownerTag;

    if (m_stats.currentTotalBytes >= bytes) {
      m_stats.currentTotalBytes -= bytes;
    } else {
      m_stats.currentTotalBytes = 0;
    }

    if (m_stats.activeCount > 0) {
      m_stats.activeCount--;
    }
    m_stats.totalDestroyedCount++;

    auto &kindBytes = m_stats.bytesByKind[static_cast<uint8_t>(kind)];
    kindBytes = (kindBytes >= bytes) ? (kindBytes - bytes) : 0;

    auto &ownerBytes = m_stats.bytesByOwner[static_cast<uint8_t>(ownerTag)];
    ownerBytes = (ownerBytes >= bytes) ? (ownerBytes - bytes) : 0;

    m_records.erase(it);
  }
}

void GPUResourceRegistry::UpdateResourceSize(uint32_t handle, graph::ResourceKind kind, size_t newSizeBytes) {
  if (handle == 0) return;
  std::lock_guard<std::mutex> lock(m_mutex);

  const uint64_t key = MakeKey(handle, kind);
  auto it = m_records.find(key);
  if (it != m_records.end()) {
    const size_t oldBytes = it->second.sizeBytes;
    const auto ownerTag = it->second.ownerTag;

    it->second.sizeBytes = newSizeBytes;

    m_stats.currentTotalBytes = m_stats.currentTotalBytes - oldBytes + newSizeBytes;
    m_stats.peakTotalBytes = std::max(m_stats.peakTotalBytes, m_stats.currentTotalBytes);

    auto &kindBytes = m_stats.bytesByKind[static_cast<uint8_t>(kind)];
    kindBytes = kindBytes - oldBytes + newSizeBytes;

    auto &ownerBytes = m_stats.bytesByOwner[static_cast<uint8_t>(ownerTag)];
    ownerBytes = ownerBytes - oldBytes + newSizeBytes;
  }
}

void GPUResourceRegistry::AdvanceFrame() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_currentFrame++;
}

void GPUResourceRegistry::Reset() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_records.clear();
  m_stats = {};
  m_currentFrame = 0;
  m_snapshotEpoch = {};
  m_snapshotEpochSet = false;
}

GPUResourceStats GPUResourceRegistry::GetStats() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_stats;
}

std::vector<GPUResourceRecord> GPUResourceRegistry::GetActiveResources() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  std::vector<GPUResourceRecord> result;
  result.reserve(m_records.size());
  for (const auto &[key, rec] : m_records) {
    result.push_back(rec);
  }
  return result;
}

GPUResourceSnapshot GPUResourceRegistry::TakeSnapshot() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_snapshotEpochSet) {
    m_snapshotEpoch = std::chrono::steady_clock::now();
    m_snapshotEpochSet = true;
  }
  const auto now = std::chrono::steady_clock::now();
  const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_snapshotEpoch);

  GPUResourceSnapshot snapshot;
  snapshot.frameIndex = m_currentFrame;
  snapshot.wallClockMs = static_cast<uint64_t>(std::max<int64_t>(elapsedMs.count(), 0));
  snapshot.activeResourceCount = m_stats.activeCount;
  snapshot.currentTotalBytes = m_stats.currentTotalBytes;
  snapshot.peakTotalBytes = m_stats.peakTotalBytes;
  snapshot.totalCreatedCount = m_stats.totalCreatedCount;
  snapshot.totalDestroyedCount = m_stats.totalDestroyedCount;
  snapshot.liveReferenceCount = m_stats.activeCount;

  size_t pendingCount = 0;
  for (const auto &[key, rec] : m_records) {
    const uint64_t age =
        (m_currentFrame >= rec.creationFrame) ? (m_currentFrame - rec.creationFrame) : 0;
    if (age <= kPendingReferenceFrames) {
      ++pendingCount;
    }
  }
  snapshot.pendingReferenceCount = pendingCount;
  return snapshot;
}

uint64_t GPUResourceRegistry::GetFrameIndex() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_currentFrame;
}

std::vector<GPUResourceRecord> GPUResourceRegistry::DetectLeakCandidates(uint64_t ageInFramesThreshold) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  std::vector<GPUResourceRecord> leaks;
  for (const auto &[key, rec] : m_records) {
    if (m_currentFrame > rec.creationFrame && (m_currentFrame - rec.creationFrame) >= ageInFramesThreshold) {
      leaks.push_back(rec);
    }
  }
  return leaks;
}

std::string GPUResourceRegistry::GenerateReportJson() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  std::ostringstream ss;
  ss << "{\n";
  ss << "  \"currentTotalBytes\": " << m_stats.currentTotalBytes << ",\n";
  ss << "  \"peakTotalBytes\": " << m_stats.peakTotalBytes << ",\n";
  ss << "  \"activeCount\": " << m_stats.activeCount << ",\n";
  ss << "  \"totalCreatedCount\": " << m_stats.totalCreatedCount << ",\n";
  ss << "  \"totalDestroyedCount\": " << m_stats.totalDestroyedCount << ",\n";
  ss << "  \"currentFrame\": " << m_currentFrame << ",\n";
  ss << "  \"activeResources\": [\n";
  size_t idx = 0;
  for (const auto &[key, rec] : m_records) {
    ss << "    {\n";
    ss << "      \"handle\": " << rec.handle << ",\n";
    ss << "      \"kind\": \"" << graph::ToResourceKindName(rec.kind) << "\",\n";
    ss << "      \"owner\": \"" << graph::ToOwnerName(rec.ownerTag) << "\",\n";
    ss << "      \"sizeBytes\": " << rec.sizeBytes << ",\n";
    ss << "      \"name\": \"" << rec.name << "\",\n";
    ss << "      \"creationFrame\": " << rec.creationFrame << "\n";
    ss << "    }" << (idx + 1 < m_records.size() ? "," : "") << "\n";
    idx++;
  }
  ss << "  ]\n";
  ss << "}\n";
  return ss.str();
}

} // namespace NoMoreDay::render::resources
