#include "game/systems/item/storage/ItemStore.hpp"
#include <utility>

namespace NoMoreDay {

ItemStore::ItemStore() {
  m_instances.resize(1);
  m_generations.resize(1, 0);
  m_occupied.resize(1, 0);
  m_activeCount = 0;
  m_version.store(1, std::memory_order_relaxed);
}

ItemStore::ItemStore(const ItemStore &other)
    : m_instances(other.m_instances),
      m_generations(other.m_generations),
      m_freeList(other.m_freeList),
      m_sideTables(other.m_sideTables),
      m_occupied(other.m_occupied),
      m_version(other.m_version.load(std::memory_order_relaxed)),
      m_activeCount(other.m_activeCount) {}

ItemStore &ItemStore::operator=(const ItemStore &other) {
  if (this != &other) {
    m_instances = other.m_instances;
    m_generations = other.m_generations;
    m_freeList = other.m_freeList;
    m_sideTables = other.m_sideTables;
    m_occupied = other.m_occupied;
    m_version.store(other.m_version.load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
    m_activeCount = other.m_activeCount;
  }
  return *this;
}

ItemStore::ItemStore(ItemStore &&other) noexcept
    : m_instances(std::move(other.m_instances)),
      m_generations(std::move(other.m_generations)),
      m_freeList(std::move(other.m_freeList)),
      m_sideTables(std::move(other.m_sideTables)),
      m_occupied(std::move(other.m_occupied)),
      m_version(other.m_version.load(std::memory_order_relaxed)),
      m_activeCount(other.m_activeCount) {
  other.clear();
}

ItemStore &ItemStore::operator=(ItemStore &&other) noexcept {
  if (this != &other) {
    m_instances = std::move(other.m_instances);
    m_generations = std::move(other.m_generations);
    m_freeList = std::move(other.m_freeList);
    m_sideTables = std::move(other.m_sideTables);
    m_occupied = std::move(other.m_occupied);
    m_version.store(other.m_version.load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
    m_activeCount = other.m_activeCount;
    other.clear();
  }
  return *this;
}

ItemHandle ItemStore::create(const ItemInstance &proto) noexcept {
  uint32_t idx = 0;
  uint32_t gen = 1;

  if (!m_freeList.empty()) {
    idx = m_freeList.back();
    m_freeList.pop_back();
    gen = m_generations[idx];
    m_instances[idx] = proto;
    m_occupied[idx] = 1;
  } else {
    if (m_instances.empty()) {
      m_instances.resize(1);
      m_generations.resize(1, 0);
      m_occupied.resize(1, 0);
    }
    idx = static_cast<uint32_t>(m_instances.size());
    gen = 1;
    m_instances.push_back(proto);
    m_generations.push_back(gen);
    m_occupied.push_back(1);
  }

  ++m_activeCount;
  m_version.fetch_add(1, std::memory_order_relaxed);
  return ItemHandle{idx, gen};
}

const ItemInstance *ItemStore::get(ItemHandle h) const noexcept {
  if (!isValid(h)) {
    return nullptr;
  }
  return &m_instances[h.index];
}

ItemInstance *ItemStore::getMutable(ItemHandle h) noexcept {
  if (!isValid(h)) {
    return nullptr;
  }
  return &m_instances[h.index];
}

bool ItemStore::isValid(ItemHandle h) const noexcept {
  if (h.index == 0 || h.index >= m_instances.size()) {
    return false;
  }
  if (m_occupied[h.index] == 0) {
    return false;
  }
  return m_generations[h.index] == h.gen;
}

bool ItemStore::mutate(ItemHandle h,
                       const std::function<void(ItemInstance &)> &fn) noexcept {
  if (!isValid(h)) {
    return false;
  }
  if (fn) {
    fn(m_instances[h.index]);
    m_version.fetch_add(1, std::memory_order_relaxed);
    return true;
  }
  return false;
}

void ItemStore::destroy(ItemHandle h) noexcept {
  if (!isValid(h)) {
    return;
  }
  m_instances[h.index] = ItemInstance{};
  m_sideTables.erase(h.index);
  m_occupied[h.index] = 0;
  ++m_generations[h.index];
  if (m_generations[h.index] == 0) {
    m_generations[h.index] = 1;
  }
  m_freeList.push_back(h.index);
  --m_activeCount;
  m_version.fetch_add(1, std::memory_order_relaxed);
}

void ItemStore::setSideTable(ItemHandle h, ItemSideTableData data) {
  if (!isValid(h)) {
    return;
  }
  if (data.empty()) {
    m_sideTables.erase(h.index);
  } else {
    m_sideTables[h.index] = std::move(data);
  }
  m_version.fetch_add(1, std::memory_order_relaxed);
}

const ItemSideTableData *ItemStore::getSideTable(ItemHandle h) const noexcept {
  if (!isValid(h)) {
    return nullptr;
  }
  auto it = m_sideTables.find(h.index);
  if (it != m_sideTables.end()) {
    return &it->second;
  }
  return nullptr;
}

ItemSideTableData *ItemStore::getSideTableMutable(ItemHandle h) noexcept {
  if (!isValid(h)) {
    return nullptr;
  }
  auto it = m_sideTables.find(h.index);
  if (it != m_sideTables.end()) {
    return &it->second;
  }
  return nullptr;
}

void ItemStore::clear() noexcept {
  m_instances.clear();
  m_generations.clear();
  m_occupied.clear();
  m_freeList.clear();
  m_sideTables.clear();

  m_instances.resize(1);
  m_generations.resize(1, 0);
  m_occupied.resize(1, 0);
  m_activeCount = 0;
  m_version.fetch_add(1, std::memory_order_relaxed);
}

void ItemStore::reserve(size_t cap) {
  m_instances.reserve(cap + 1);
  m_generations.reserve(cap + 1);
  m_occupied.reserve(cap + 1);
  m_freeList.reserve(cap);
}

} // namespace NoMoreDay
