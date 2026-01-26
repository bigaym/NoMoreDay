#include "engine/render/PersistentBuffer.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>

// NO_MORE_DAY_GPU_CONSTANTS
namespace NoMoreDay::render {

using namespace NoMoreDay::RenderConstants;

PersistentBuffer::PersistentBuffer() = default;

PersistentBuffer::~PersistentBuffer() { Destroy(); }

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
}

void PersistentBuffer::CreatePersistent(size_t size) {
  m_totalSize = size * m_bufferCount;
  m_fences.resize(m_bufferCount, nullptr);

  utils::GPUUtils::GenBuffers(1, &m_bufferId);
  utils::GPUUtils::BindBuffer(GL::SHADER_STORAGE_BUFFER, m_bufferId);

  // Use FlushExplicit instead of Coherent for better driver-side power management and performance
  uint32_t flags = ToGL(MapFlag::Write | MapFlag::Read | MapFlag::Persistent |
                        MapFlag::FlushExplicit);

  utils::GPUUtils::BufferStorage(GL::SHADER_STORAGE_BUFFER, m_totalSize,
                                 nullptr, flags);

  m_mappedPtr = (uint8_t *)utils::GPUUtils::MapBufferRange(
      GL::SHADER_STORAGE_BUFFER, 0, m_totalSize, flags);

  utils::GPUUtils::BindBuffer(GL::SHADER_STORAGE_BUFFER, 0);

  if (!m_mappedPtr) {
    LOG_ERROR("PersistentBuffer: Failed to map buffer!");
    m_mode = Mode::Compat;
    CreateCompat(size);
  }
}

void PersistentBuffer::CreateCompat(size_t size) {
  utils::GPUUtils::GenBuffers(1, &m_bufferId);
  utils::GPUUtils::BindBuffer(GL::SHADER_STORAGE_BUFFER, m_bufferId);
  utils::GPUUtils::BufferData(GL::SHADER_STORAGE_BUFFER, size, nullptr,
                              0x88E8); // GL_DYNAMIC_DRAW
  utils::GPUUtils::BindBuffer(GL::SHADER_STORAGE_BUFFER, 0);

  m_stagingBuffer.resize(size);
}

void PersistentBuffer::Destroy() {
  if (m_bufferId != 0) {
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
    if (size == 0) return;
    size_t totalOffset = m_writeSlot * m_slotSize + offset;
    utils::GPUUtils::FlushMappedBufferRange(GL::SHADER_STORAGE_BUFFER, totalOffset, size);
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
