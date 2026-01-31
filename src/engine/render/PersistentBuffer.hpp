#pragma once

#include "raylib.h"
#include "rlgl.h"
#include "GLFW/glfw3.h"
#include <cstdint>
#include <vector>

namespace NoMoreDay::render {

class PersistentBuffer {
public:
  enum class Mode {
    Compat,    // Fallback using glBufferSubData
    Persistent // High-performance persistent mapping
  };

  PersistentBuffer();
  ~PersistentBuffer();

  // Check if hardware supports persistent mapping
  static bool IsSupported();

  // specific slotSize is the size of one frame's data
  // total size allocated will be slotSize * bufferCount for Persistent mode
  void Create(size_t slotSize, int bufferCount = 3, unsigned int usageHint = 0);

  void Destroy();

  // Get pointer to write current frame data
  // In Persistent mode: Waits for Fence if needed
  // In Compat mode: Returns pointer to staging buffer
  void *BeginWrite();

  // Finish writing (Flush CPU cache)
  void Flush();
  void FlushRange(size_t offset, size_t size);

  // Mark end of GPU usage for current frame (Insert Fence and Advance Slot)
  void Lock();

  // Read data from the buffer
  // Copies from the currently mapped slot (Safe if called after BeginWrite)
  void Read(void *data, size_t size) const;

  // Read from a specific slot index
  void ReadFromSlot(void *data, size_t size, int slotIndex) const;

  // Get byte offset for a specific slot
  // In COMPAT mode, this is always 0 as the buffer only has one slot.
  size_t GetSlotOffset(int slotIndex) const { 
      return (m_mode == Mode::Persistent) ? (slotIndex * m_slotSize) : 0; 
  }
  size_t GetCurrentSlotOffset() const { 
      return (m_mode == Mode::Persistent) ? (m_writeSlot * m_slotSize) : 0; 
  }
  size_t GetPreviousSlotOffset() const { 
      if (m_mode == Mode::Compat) return 0;
      return ((m_writeSlot - 1 + m_bufferCount) % m_bufferCount) * m_slotSize; 
  }

  // Bind the buffer for reading by shaders
  // Bind current write slot (for physics/update)
  void BindBase(unsigned int bindingPoint) const;

  // Bind previous slot (for rendering/culling)
  // Offset = ((m_writeSlot - 1 + m_bufferCount) % m_bufferCount) * m_slotSize
  void BindPrevious(unsigned int bindingPoint) const;

  // Bind previous slot WITHOUT waiting for Fence (Performance optimized for
  // rendering)
  void BindPreviousNoSync(unsigned int bindingPoint) const;

  // Bind the slot before previous (for interpolation/sync back)
  void BindOldest(unsigned int bindingPoint) const;

  /**
   * @brief Bind the buffer to a specific OpenGL target (e.g.
   * GL_DRAW_INDIRECT_BUFFER)
   * @param target OpenGL buffer target
   * @param slotType 0 = Current, 1 = Previous, 2 = Oldest
   */
  void Bind(unsigned int target, int slotType = 0) const;

  unsigned int GetId() const { return m_bufferId; }
  size_t GetSize() const { return m_slotSize; }
  Mode m_mode = Mode::Compat;
  Mode GetMode() const { return m_mode; }
  int GetCurrentSlot() const { return m_writeSlot; }
  int GetBufferCount() const { return m_bufferCount; }

private:
  void CreatePersistent(size_t size);
  void CreateCompat(size_t size);

  void WaitForFence(void *&fence);

  // Common
  unsigned int m_bufferId = 0;
  size_t m_slotSize = 0;

  // Persistent Mode State
  size_t m_totalSize = 0;
  uint8_t *m_mappedPtr = nullptr;
  int m_writeSlot = 0; // 0, 1, 2 (Triple-Buffer)
  int m_bufferCount = 2;
  std::vector<void *> m_fences;

  // Compat Mode State
  std::vector<uint8_t> m_stagingBuffer;
};

} // namespace NoMoreDay::render