#pragma once

#include "raylib.h"
#include "rlgl.h"
#include "GLFW/glfw3.h"
#include <vector>
#include <cstdint>

// Forward declaration of GL sync primitive
#ifndef __gl_h_
typedef struct __GLsync *GLsync;
#endif

namespace NoMoreDay::render {

class PersistentBuffer {
public:
    enum class Mode {
        Compat,     // Fallback using glBufferSubData
        Persistent  // High-performance persistent mapping
    };

    PersistentBuffer();
    ~PersistentBuffer();

    // Check if hardware supports persistent mapping
    static bool IsSupported();

    // specific slotSize is the size of one frame's data
    // total size allocated will be slotSize * 3 for Persistent mode
    void Create(size_t slotSize, unsigned int usageHint = 0 /* unused in persistent mode */);
    
    void Destroy();

    // Get pointer to write current frame data
    // In Persistent mode: Waits for Fence if needed
    // In Compat mode: Returns pointer to staging buffer
    void* BeginWrite();

    // Finish writing (Flush CPU cache)
    void Flush();

    // Mark end of GPU usage for current frame (Insert Fence and Advance Slot)
    void Lock();

    // Read data from the buffer
    // In Persistent mode: Copies from the currently mapped slot (Safe if called after BeginWrite)
    // In Compat mode: Downloads from GPU buffer
    void Read(void* data, size_t size);

    // Bind the buffer for reading by shaders
    // automatically handles offset for the correct frame (N-2 for Persistent)
    void BindBase(unsigned int bindingPoint) const;

    unsigned int GetId() const { return m_bufferId; } // Needed for some raw bindings
    size_t GetSize() const { return m_slotSize; }
    Mode GetMode() const { return m_mode; }

private:
    void CreatePersistent(size_t size);
    void CreateCompat(size_t size);

    // Wait for the fence associated with the current write slot
    void WaitForFence(void*& fence);

    Mode m_mode = Mode::Compat;
    
    // Common
    unsigned int m_bufferId = 0;
    size_t m_slotSize = 0;
    
    // Persistent Mode State
    size_t m_totalSize = 0;
    uint8_t* m_mappedPtr = nullptr;
    int m_writeSlot = 0; // 0, 1, 2
    void* m_fences[3] = {nullptr, nullptr, nullptr};
    
    // Compat Mode State
    std::vector<uint8_t> m_stagingBuffer;
};

} // namespace NoMoreDay::render
