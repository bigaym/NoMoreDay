#pragma once
#include "glad.h"
#include <vector>
#include <cstddef>

namespace NoMoreDay::core {

enum class BufferUsage {
    Static = GL_STATIC_DRAW,
    Dynamic = GL_DYNAMIC_DRAW,
    Stream = GL_STREAM_DRAW
};

class ComputeBuffer {
public:
    ComputeBuffer() = default;
    ~ComputeBuffer() {
        if (m_id != 0) {
            glDeleteBuffers(1, &m_id);
        }
    }

    // Disable copy
    ComputeBuffer(const ComputeBuffer&) = delete;
    ComputeBuffer& operator=(const ComputeBuffer&) = delete;

    // Allow move
    ComputeBuffer(ComputeBuffer&& other) noexcept : m_id(other.m_id), m_size(other.m_size) {
        other.m_id = 0;
        other.m_size = 0;
    }
    ComputeBuffer& operator=(ComputeBuffer&& other) noexcept {
        if (this != &other) {
            if (m_id != 0) glDeleteBuffers(1, &m_id);
            m_id = other.m_id;
            m_size = other.m_size;
            other.m_id = 0;
            other.m_size = 0;
        }
        return *this;
    }

    void Create(size_t size, const void* data = nullptr, BufferUsage usage = BufferUsage::Dynamic) {
        if (m_id == 0) glGenBuffers(1, &m_id);
        m_size = size;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_id);
        glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, static_cast<GLenum>(usage));
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    void Update(const void* data, size_t size, size_t offset = 0) {
        if (m_id == 0) return;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_id);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    void BindBase(unsigned int index) const {
        if (m_id == 0) return;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, m_id);
    }

    void Read(void* outData, size_t size, size_t offset = 0) const {
        if (m_id == 0) return;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_id);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, outData);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    unsigned int GetId() const { return m_id; }
    size_t GetSize() const { return m_size; }

private:
    unsigned int m_id = 0;
    size_t m_size = 0;
};

} // namespace NoMoreDay::core
