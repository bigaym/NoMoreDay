#include "engine/render/PersistentBuffer.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>

// NO_MORE_DAY_GPU_CONSTANTS
namespace NoMoreDay::render {

using namespace NoMoreDay::RenderConstants;

PersistentBuffer::PersistentBuffer() = default;

PersistentBuffer::~PersistentBuffer() { Destroy(); }

PersistentBuffer::PersistentBuffer(PersistentBuffer &&other) noexcept {
  *this = std::move(other);
}

PersistentBuffer &PersistentBuffer::operator=(PersistentBuffer &&other) noexcept {
  if (this != &other) {
    Destroy();
    m_bufferId = other.m_bufferId;
    m_slotSize = other.m_slotSize;
    m_mode = other.m_mode;
    m_totalSize = other.m_totalSize;
    m_mappedPtr = other.m_mappedPtr;
    m_writeSlot = other.m_writeSlot;
    m_bufferCount = other.m_bufferCount;
    m_fences = std::move(other.m_fences);
    m_stagingBuffer = std::move(other.m_stagingBuffer);

    // The moved-from object must return to its default state so Persistent-mode
    // accessors (fence/slot arithmetic) can never touch a cleared buffer.
    other.ResetState();
  }
  return *this;
}

bool PersistentBuffer::IsSupported() {
  return utils::GPUUtils::IsInitialized() &&
         utils::GPUUtils::CheckSupport().persistentMappingSupported;
}

void PersistentBuffer::Create(size_t slotSize, int bufferCount,
                              unsigned int usageHint) {
  if (m_bufferId != 0)
    Destroy();

  const size_t alignment = 256;
  m_slotSize = (slotSize + alignment - 1) & ~(alignment - 1);
  m_bufferCount = bufferCount;
  if (m_bufferCount < 2)
    m_bufferCount = 2;

  m_writeSlot = 0; // Reset slot index

  if (IsSupported()) {
    m_mode = Mode::Persistent;
    LOG_INFO("PersistentBuffer: Creating in PERSISTENT mode. SlotSize={}, "
             "Count={}, Total={}",
             m_slotSize, m_bufferCount, m_slotSize * m_bufferCount);
    CreatePersistent(m_slotSize);
  } else {
    m_mode = Mode::Compat;
    LOG_INFO("PersistentBuffer: Creating in COMPAT mode. SlotSize={}",
             m_slotSize);
    CreateCompat(m_slotSize);
  }

  // W5.4 (RG-3 contract): observe only after a successful allocation. The
  // wrapper remains the sole releaser; the registry never owns the handle.
  if (m_bufferId != 0) {
    auto &registry = resources::GPUResourceRegistry::Get();
    registry.RegisterResource(
        m_bufferId, graph::ResourceKind::StorageBuffer,
        graph::RenderOwnerTag::Unknown,
        m_mode == Mode::Persistent ? m_totalSize : m_slotSize,
        "PersistentBuffer");
    if (m_mode == Mode::Persistent && m_mappedPtr != nullptr) {
      // Explicit mapping record, removed before the backing buffer record.
      registry.RegisterResource(m_bufferId, graph::ResourceKind::PersistentMapping,
                                graph::RenderOwnerTag::Unknown, m_totalSize,
                                "PersistentBufferMapping");
    }
  }
}

void PersistentBuffer::CreatePersistent(size_t size) {
  m_totalSize = size * m_bufferCount;
  m_fences.assign(m_bufferCount, nullptr); // Use assign to reset all to nullptr

  utils::GPUUtils::GenBuffers(1, &m_bufferId);
  utils::GPUUtils::BindBuffer(GL::SHADER_STORAGE_BUFFER, m_bufferId);

  // Storage flags: Must NOT include FlushExplicit (0x0010) as it is only for MapRange
  uint32_t storageFlags = ToGL(MapFlag::Write | MapFlag::Read | MapFlag::Persistent);

  utils::GPUUtils::BufferStorage(GL::SHADER_STORAGE_BUFFER, m_totalSize,
                                 nullptr, storageFlags);

  // Map flags: CAN include FlushExplicit
  uint32_t mapFlags = storageFlags | ToGL(MapFlag::FlushExplicit);

  m_mappedPtr = (uint8_t *)utils::GPUUtils::MapBufferRange(
      GL::SHADER_STORAGE_BUFFER, 0, m_totalSize, mapFlags);

  utils::GPUUtils::BindBuffer(GL::SHADER_STORAGE_BUFFER, 0);

  if (!m_mappedPtr) {
    LOG_ERROR("PersistentBuffer: Failed to map buffer!");
    
    // Clean up the failed persistent buffer before falling back to Compat
    utils::GPUUtils::DeleteBuffers(1, &m_bufferId);
    m_bufferId = 0;
    
    m_mode = Mode::Compat;
    CreateCompat(size);
  }
}

void PersistentBuffer::CreateCompat(size_t size) {
  if (m_bufferId == 0) {
      utils::GPUUtils::GenBuffers(1, &m_bufferId);
  }
  utils::GPUUtils::BindBuffer(GL::SHADER_STORAGE_BUFFER, m_bufferId);
  utils::GPUUtils::BufferData(GL::SHADER_STORAGE_BUFFER, size, nullptr,
                              0x88E8); // GL_DYNAMIC_DRAW
  utils::GPUUtils::BindBuffer(GL::SHADER_STORAGE_BUFFER, 0);

  m_stagingBuffer.resize(size);
}

void PersistentBuffer::Destroy() {
  if (m_bufferId != 0) {
    // W5.4 (RG-3 contract): unregister observer records before any GL release.
    // Persistent mapping record is removed before its backing buffer record.
    auto &registry = resources::GPUResourceRegistry::Get();
    if (m_mode == Mode::Persistent && m_mappedPtr != nullptr) {
      registry.UnregisterResource(m_bufferId,
                                  graph::ResourceKind::PersistentMapping);
    }
    registry.UnregisterResource(m_bufferId, graph::ResourceKind::StorageBuffer);

    for (size_t i = 0; i < m_fences.size(); i++) {
      if (m_fences[i]) {
        utils::GPUUtils::DeleteSync(m_fences[i]);
        m_fences[i] = nullptr;
      }
    }
    m_fences.clear();

    if (m_mode == Mode::Persistent && m_mappedPtr) {
      utils::GPUUtils::BindBuffer(GL::SHADER_STORAGE_BUFFER, m_bufferId);
      utils::GPUUtils::UnmapBuffer(GL::SHADER_STORAGE_BUFFER);
      utils::GPUUtils::BindBuffer(GL::SHADER_STORAGE_BUFFER, 0);
      m_mappedPtr = nullptr;
    }

    utils::GPUUtils::DeleteBuffers(1, &m_bufferId);
    m_bufferId = 0;
  }
  // Reset the full state (without further GL calls) so a destroyed object
  // reports itself as a default Compat buffer with no live handles.
  ResetState();
}

void PersistentBuffer::ResetState() {
  m_bufferId = 0;
  m_slotSize = 0;
  m_totalSize = 0;
  m_mappedPtr = nullptr;
  m_writeSlot = 0;
  m_bufferCount = 2;
  m_mode = Mode::Compat;
  m_fences.clear();
  m_stagingBuffer.clear();
}

void PersistentBuffer::WaitForFence(void *&fencePtr) {
  if (!fencePtr)
    return;
  utils::GPUUtils::ClientWaitSync(fencePtr, ToGL(SyncFlag::FlushCommands),
                                  1000000000);
  utils::GPUUtils::DeleteSync(fencePtr);
  fencePtr = nullptr;
}

void *PersistentBuffer::BeginWrite() {
  if (m_mode == Mode::Persistent) {
    NoMoreDay::utils::ScopedTimer timer("Buffer BeginWrite Stall", 100);
    WaitForFence(m_fences[m_writeSlot]);
    return m_mappedPtr + m_writeSlot * m_slotSize;
  } else {
    return m_stagingBuffer.data();
  }
}

void PersistentBuffer::Flush() {
  FlushRange(0, m_slotSize);
}

void PersistentBuffer::FlushRange(size_t offset, size_t size) {
  if (m_mode == Mode::Persistent) {
    if (size == 0 || m_bufferId == 0) return;
    size_t totalOffset = m_writeSlot * m_slotSize + offset;
    
    // [FIX] glFlushMappedBufferRange requires the buffer to be bound to the specified target
    utils::GPUUtils::BindBuffer(GL::SHADER_STORAGE_BUFFER, m_bufferId);
    utils::GPUUtils::FlushMappedBufferRange(GL::SHADER_STORAGE_BUFFER, totalOffset, size);
    utils::GPUUtils::BindBuffer(GL::SHADER_STORAGE_BUFFER, 0);
    
    utils::GPUUtils::MemoryBarrier(RenderConstants::Barrier::Client);
  } else {
    utils::GPUUtils::BindBuffer(GL::SHADER_STORAGE_BUFFER, m_bufferId);
    utils::GPUUtils::BufferSubData(GL::SHADER_STORAGE_BUFFER, offset, size,
                                   m_stagingBuffer.data() + offset);
    utils::GPUUtils::BindBuffer(GL::SHADER_STORAGE_BUFFER, 0);
  }
}

void PersistentBuffer::Lock() {
  if (m_mode == Mode::Persistent) {
    if (m_fences[m_writeSlot]) {
      utils::GPUUtils::DeleteSync(m_fences[m_writeSlot]);
    }
    m_fences[m_writeSlot] =
        utils::GPUUtils::FenceSync(GL::SYNC_GPU_COMMANDS_COMPLETE, 0);
    m_writeSlot = (m_writeSlot + 1) % m_bufferCount;
  }
}

void PersistentBuffer::Read(void *data, size_t size) const {
  int targetSlot = (m_writeSlot - 1 + m_bufferCount) % m_bufferCount;
  ReadFromSlot(data, size, targetSlot);
}

void PersistentBuffer::ReadFromSlot(void *data, size_t size,
                                    int slotIndex) const {
  if (slotIndex < 0 || slotIndex >= m_bufferCount)
    return;

  if (m_mode == Mode::Persistent) {
    if (m_mappedPtr) {
      const_cast<PersistentBuffer *>(this)->WaitForFence(
          const_cast<PersistentBuffer *>(this)->m_fences[slotIndex]);

      utils::GPUUtils::MemoryBarrier(RenderConstants::Barrier::Client);

      size_t copySize = std::min(size, m_slotSize);
      memcpy(data, m_mappedPtr + slotIndex * m_slotSize, copySize);
    }
  } else {
    utils::GPUUtils::BindBuffer(GL::SHADER_STORAGE_BUFFER, m_bufferId);
    utils::GPUUtils::GetBufferSubData(GL::SHADER_STORAGE_BUFFER, 0, size, data);
    utils::GPUUtils::BindBuffer(GL::SHADER_STORAGE_BUFFER, 0);
  }
}

void PersistentBuffer::BindBase(unsigned int bindingPoint) const {
  if (m_mode == Mode::Persistent) {
    size_t offset = m_writeSlot * m_slotSize;
    utils::GPUUtils::BindBufferRange(GL::SHADER_STORAGE_BUFFER, bindingPoint,
                                     m_bufferId, offset, m_slotSize);
  } else {
    utils::GPUUtils::BindBufferBase(bindingPoint, m_bufferId);
  }
}

void PersistentBuffer::BindPrevious(unsigned int bindingPoint) const {
  if (m_mode == Mode::Persistent && m_bufferId != 0) {
    int prevSlot = (m_writeSlot - 1 + m_bufferCount) % m_bufferCount;
    const_cast<PersistentBuffer *>(this)->WaitForFence(
        const_cast<PersistentBuffer *>(this)->m_fences[prevSlot]);

    size_t offset = prevSlot * m_slotSize;
    utils::GPUUtils::BindBufferRange(GL::SHADER_STORAGE_BUFFER, bindingPoint,
                                     m_bufferId, offset, m_slotSize);
  } else if (m_mode == Mode::Compat) {
    BindBase(bindingPoint);
  }
}

void PersistentBuffer::BindPreviousNoSync(unsigned int bindingPoint) const {
  if (m_mode == Mode::Persistent && m_bufferId != 0) {
    int prevSlot = (m_writeSlot - 1 + m_bufferCount) % m_bufferCount;
    size_t offset = prevSlot * m_slotSize;
    utils::GPUUtils::BindBufferRange(GL::SHADER_STORAGE_BUFFER, bindingPoint,
                                     m_bufferId, offset, m_slotSize);
  } else if (m_mode == Mode::Compat) {
    BindBase(bindingPoint);
  }
}

void PersistentBuffer::BindOldest(unsigned int bindingPoint) const {
  if (m_mode == Mode::Persistent && m_bufferId != 0) {
    int oldestSlot = (m_writeSlot - 2 + m_bufferCount) % m_bufferCount;
    const_cast<PersistentBuffer *>(this)->WaitForFence(
        const_cast<PersistentBuffer *>(this)->m_fences[oldestSlot]);

    size_t offset = oldestSlot * m_slotSize;
    utils::GPUUtils::BindBufferRange(GL::SHADER_STORAGE_BUFFER, bindingPoint,
                                     m_bufferId, offset, m_slotSize);
  } else if (m_mode == Mode::Compat) {
    BindBase(bindingPoint);
  }
}

void PersistentBuffer::Bind(unsigned int target, int slotType) const {
  if (m_bufferId == 0)
    return;
  utils::GPUUtils::BindBuffer(target, m_bufferId);
}

} // namespace NoMoreDay::render
