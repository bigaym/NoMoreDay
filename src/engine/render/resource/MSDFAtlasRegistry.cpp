#include "engine/render/resource/MSDFAtlasRegistry.hpp"

namespace NoMoreDay::render {

bool MSDFAtlasRegistry::Register(const Texture2D texture,
                                 const std::vector<MSDFGlyphMetric> &glyphs,
                                 const float distanceRange, const float emSize) {
  m_glyphs = glyphs;
  m_index.clear();
  m_index.reserve(m_glyphs.size());
  for (size_t i = 0; i < m_glyphs.size(); ++i) {
    m_index.emplace(m_glyphs[i].codepoint, i);
  }
  m_texture = texture;
  m_distanceRange = distanceRange;
  m_emSize = emSize;
  m_available = true;
  return true;
}

void MSDFAtlasRegistry::Clear() noexcept {
  m_available = false;
  m_texture = {};
  m_distanceRange = 0.0f;
  m_emSize = 0.0f;
  m_glyphs.clear();
  m_index.clear();
}

const MSDFGlyphMetric *MSDFAtlasRegistry::Find(
    const uint32_t codepoint) const noexcept {
  if (!m_available) {
    return nullptr;
  }
  const auto it = m_index.find(codepoint);
  if (it == m_index.end()) {
    return nullptr;
  }
  return &m_glyphs[it->second];
}

} // namespace NoMoreDay::render
