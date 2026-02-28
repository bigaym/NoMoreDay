#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"

#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace NoMoreDay {
namespace {

uint32_t ComputeCrc32(std::span<const uint8_t> bytes) {
  uint32_t crc = 0xFFFFFFFFu;
  for (const auto value : bytes) {
    crc ^= value;
    for (int bit = 0; bit < 8; ++bit) {
      const bool lsb = (crc & 1u) != 0u;
      crc >>= 1u;
      if (lsb) {
        crc ^= 0xEDB88320u;
      }
    }
  }
  return crc ^ 0xFFFFFFFFu;
}

bool ValidateCrc32(std::span<const uint8_t> bytes,
                   const ModifierRuntimeHeader &header) {
  if (bytes.size() < sizeof(ModifierRuntimeHeader)) {
    return false;
  }

  if (header.crc32 == 0u) {
    return true;
  }

  const auto payload = bytes.subspan(sizeof(ModifierRuntimeHeader));
  return ComputeCrc32(payload) == header.crc32;
}

bool ValidateOffsets(std::span<const uint8_t> bytes,
                     const ModifierRuntimeHeader &header) {
  const size_t totalSize = bytes.size();
  const size_t recordsOffset = static_cast<size_t>(header.records_offset);
  const size_t filtersOffset = static_cast<size_t>(header.filters_offset);
  const size_t opsOffset = static_cast<size_t>(header.ops_offset);
  const size_t indexOffset = static_cast<size_t>(header.index_offset);

  const size_t recordsSize =
      static_cast<size_t>(header.record_count) * sizeof(ModifierRuntimeRecord);
  const size_t filtersSize =
      static_cast<size_t>(header.filter_count) * sizeof(ModifierRuntimeFilter);
  const size_t opsSize =
      static_cast<size_t>(header.op_count) * sizeof(ModifierRuntimeOp);
  const size_t indexSize =
      static_cast<size_t>(header.index_count) * sizeof(uint32_t);

  if (recordsOffset < sizeof(ModifierRuntimeHeader) || recordsOffset > totalSize) {
    return false;
  }

  if (!(recordsOffset <= filtersOffset && filtersOffset <= opsOffset &&
        opsOffset <= indexOffset && indexOffset <= totalSize)) {
    return false;
  }

  if (recordsOffset + recordsSize != filtersOffset) {
    return false;
  }
  if (filtersOffset + filtersSize != opsOffset) {
    return false;
  }
  if (opsOffset + opsSize != indexOffset) {
    return false;
  }
  if (indexOffset + indexSize > totalSize) {
    return false;
  }

  return true;
}

bool ValidateRanges(const std::span<const ModifierRuntimeRecord> records,
                    const std::span<const ModifierRuntimeFilter> filters,
                    const std::span<const ModifierRuntimeOp> ops,
                    const std::span<const uint32_t> index) {
  for (const auto &record : records) {
    if (record.filter_index >= filters.size()) {
      return false;
    }

    const size_t opOffset = static_cast<size_t>(record.op_offset);
    const size_t opCount = static_cast<size_t>(record.op_count);
    if (opOffset > ops.size() || opCount > (ops.size() - opOffset)) {
      return false;
    }
  }

  for (const auto &filter : filters) {
    const size_t skillOffset = static_cast<size_t>(filter.skill_whitelist_offset);
    const size_t skillCount = static_cast<size_t>(filter.skill_whitelist_count);
    if (skillOffset > index.size() || skillCount > (index.size() - skillOffset)) {
      return false;
    }

    const size_t nodeOffset = static_cast<size_t>(filter.node_whitelist_offset);
    const size_t nodeCount = static_cast<size_t>(filter.node_whitelist_count);
    if (nodeOffset > index.size() || nodeCount > (index.size() - nodeOffset)) {
      return false;
    }
  }

  return true;
}

} // namespace

ModifierRuntimeRegistry &ModifierRuntimeRegistry::Get() {
  static ModifierRuntimeRegistry instance;
  return instance;
}

void ModifierRuntimeRegistry::Clear() {
  m_header = {};
  m_records.clear();
  m_filters.clear();
  m_ops.clear();
  m_index.clear();
  m_recordIndexById.clear();
  m_loaded = false;
}

bool ModifierRuntimeRegistry::EnsureLoaded(const std::string_view path) {
  if (m_loaded) {
    return true;
  }

  std::ifstream file(std::string(path), std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  std::vector<uint8_t> bytes;
  const auto begin = std::istreambuf_iterator<char>(file);
  const auto end = std::istreambuf_iterator<char>();
  bytes.assign(begin, end);

  if (!LoadFromBytes(bytes)) {
    return false;
  }

  m_loaded = true;
  return true;
}

bool ModifierRuntimeRegistry::LoadFromBytes(const std::span<const uint8_t> bytes) {
  Clear();

  if (bytes.size() < sizeof(ModifierRuntimeHeader)) {
    return false;
  }

  ModifierRuntimeHeader header = {};
  std::memcpy(&header, bytes.data(), sizeof(ModifierRuntimeHeader));

  if (header.magic != ModifierRuntimeHeader::kMagic) {
    return false;
  }
  if (header.format_version != 2) {
    return false;
  }
  if (header.endian != 1u) {
    return false;
  }
  if (!ValidateOffsets(bytes, header)) {
    return false;
  }
  if (!ValidateCrc32(bytes, header)) {
    return false;
  }

  const auto recordCount = static_cast<size_t>(header.record_count);
  const auto filterCount = static_cast<size_t>(header.filter_count);
  const auto opCount = static_cast<size_t>(header.op_count);
  const auto indexCount = static_cast<size_t>(header.index_count);

  m_records.resize(recordCount);
  m_filters.resize(filterCount);
  m_ops.resize(opCount);
  m_index.resize(indexCount);

  if (!m_records.empty()) {
    std::memcpy(m_records.data(), bytes.data() + header.records_offset,
                m_records.size() * sizeof(ModifierRuntimeRecord));
  }
  if (!m_filters.empty()) {
    std::memcpy(m_filters.data(), bytes.data() + header.filters_offset,
                m_filters.size() * sizeof(ModifierRuntimeFilter));
  }
  if (!m_ops.empty()) {
    std::memcpy(m_ops.data(), bytes.data() + header.ops_offset,
                m_ops.size() * sizeof(ModifierRuntimeOp));
  }
  if (!m_index.empty()) {
    std::memcpy(m_index.data(), bytes.data() + header.index_offset,
                m_index.size() * sizeof(uint32_t));
  }

  if (!ValidateRanges(std::span<const ModifierRuntimeRecord>(m_records),
                      std::span<const ModifierRuntimeFilter>(m_filters),
                      std::span<const ModifierRuntimeOp>(m_ops),
                      std::span<const uint32_t>(m_index))) {
    Clear();
    return false;
  }

  m_recordIndexById.reserve(m_records.size());
  for (size_t i = 0; i < m_records.size(); ++i) {
    m_recordIndexById[m_records[i].id] = static_cast<uint32_t>(i);
  }

  m_header = header;
  m_loaded = true;
  return true;
}

uint32_t ModifierRuntimeRegistry::RecordCount() const { return m_header.record_count; }

std::span<const ModifierRuntimeRecord> ModifierRuntimeRegistry::GetRecords() const {
  return std::span<const ModifierRuntimeRecord>(m_records);
}

const ModifierRuntimeRecord *ModifierRuntimeRegistry::FindRecordById(
    const uint32_t id) const {
  const auto it = m_recordIndexById.find(id);
  if (it == m_recordIndexById.end()) {
    return nullptr;
  }

  const size_t index = static_cast<size_t>(it->second);
  if (index >= m_records.size()) {
    return nullptr;
  }
  return &m_records[index];
}

const ModifierRuntimeFilter *ModifierRuntimeRegistry::GetFilter(
    const ModifierRuntimeRecord &record) const {
  const size_t filterIndex = static_cast<size_t>(record.filter_index);
  if (filterIndex >= m_filters.size()) {
    return nullptr;
  }
  return &m_filters[filterIndex];
}

std::span<const ModifierRuntimeOp>
ModifierRuntimeRegistry::GetOps(const ModifierRuntimeRecord &record) const {
  const size_t opOffset = static_cast<size_t>(record.op_offset);
  const size_t opCount = static_cast<size_t>(record.op_count);
  if (opOffset > m_ops.size() || opCount > (m_ops.size() - opOffset)) {
    return {};
  }
  return std::span<const ModifierRuntimeOp>(m_ops).subspan(opOffset, opCount);
}

std::span<const uint32_t>
ModifierRuntimeRegistry::GetSkillWhitelist(const ModifierRuntimeFilter &filter) const {
  const size_t offset = static_cast<size_t>(filter.skill_whitelist_offset);
  const size_t count = static_cast<size_t>(filter.skill_whitelist_count);
  if (offset > m_index.size() || count > (m_index.size() - offset)) {
    return {};
  }
  return std::span<const uint32_t>(m_index).subspan(offset, count);
}

std::span<const uint32_t>
ModifierRuntimeRegistry::GetNodeWhitelist(const ModifierRuntimeFilter &filter) const {
  const size_t offset = static_cast<size_t>(filter.node_whitelist_offset);
  const size_t count = static_cast<size_t>(filter.node_whitelist_count);
  if (offset > m_index.size() || count > (m_index.size() - offset)) {
    return {};
  }
  return std::span<const uint32_t>(m_index).subspan(offset, count);
}

} // namespace NoMoreDay
