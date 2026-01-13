// src/engine/audio/AudioSystem.hpp
// 音效系统 - 封装 Raylib 音频 API
#pragma once

#include "core/logging/Logger.hpp"
#include <memory>
#include <raylib.h>
#include <string>
#include <string_view>
#include <unordered_map>


namespace NoMoreDay {

/// @brief 音效类型 (用于音量分组)
enum class AudioChannel : uint8_t {
  Master, // 主音量
  Music,  // 背景音乐
  SFX,    // 音效
  UI,     // UI 音效
  Ambient // 环境音
};

/// @brief 音效系统 - 管理所有音频资源和播放
class AudioSystem {
public:
  /// 获取单例
  [[nodiscard]] static AudioSystem &Get() {
    static AudioSystem instance;
    return instance;
  }

  /// 初始化音频设备
  void Init() {
    if (m_initialized)
      return;

    InitAudioDevice();
    m_initialized = true;
    LOG_INFO("[AudioSystem] Initialized audio device.");
  }

  /// 关闭音频设备
  void Shutdown() {
    if (!m_initialized)
      return;

    // 停止所有音乐
    for (auto &[name, music] : m_musicCache) {
      StopMusicStream(music);
      UnloadMusicStream(music);
    }
    m_musicCache.clear();

    // 卸载所有音效
    for (auto &[name, sound] : m_soundCache) {
      UnloadSound(sound);
    }
    m_soundCache.clear();

    CloseAudioDevice();
    m_initialized = false;
    LOG_INFO("[AudioSystem] Shutdown complete.");
  }

  /// 加载音效文件
  bool LoadSound(std::string_view name, std::string_view path) {
    Sound sound = ::LoadSound(std::string(path).c_str());
    if (sound.frameCount == 0) {
      LOG_ERROR("[AudioSystem] Failed to load sound: {}", path);
      return false;
    }
    m_soundCache[std::string(name)] = sound;
    LOG_DEBUG("[AudioSystem] Loaded sound: {} from {}", name, path);
    return true;
  }

  /// 加载音乐文件
  bool LoadMusic(std::string_view name, std::string_view path) {
    Music music = ::LoadMusicStream(std::string(path).c_str());
    if (music.frameCount == 0) {
      LOG_ERROR("[AudioSystem] Failed to load music: {}", path);
      return false;
    }
    m_musicCache[std::string(name)] = music;
    LOG_DEBUG("[AudioSystem] Loaded music: {} from {}", name, path);
    return true;
  }

  /// 播放音效
  void PlaySound(std::string_view name,
                 AudioChannel channel = AudioChannel::SFX) {
    auto it = m_soundCache.find(std::string(name));
    if (it == m_soundCache.end()) {
      LOG_WARN("[AudioSystem] Sound not found: {}", name);
      return;
    }

    float volume = m_channelVolumes[static_cast<int>(AudioChannel::Master)] *
                   m_channelVolumes[static_cast<int>(channel)];
    SetSoundVolume(it->second, volume);
    ::PlaySound(it->second);
  }

  /// 播放背景音乐
  void PlayMusic(std::string_view name, bool loop = true) {
    auto it = m_musicCache.find(std::string(name));
    if (it == m_musicCache.end()) {
      LOG_WARN("[AudioSystem] Music not found: {}", name);
      return;
    }

    // 停止当前音乐
    if (!m_currentMusic.empty()) {
      auto cur = m_musicCache.find(m_currentMusic);
      if (cur != m_musicCache.end()) {
        StopMusicStream(cur->second);
      }
    }

    it->second.looping = loop;
    float volume = m_channelVolumes[static_cast<int>(AudioChannel::Master)] *
                   m_channelVolumes[static_cast<int>(AudioChannel::Music)];
    SetMusicVolume(it->second, volume);
    PlayMusicStream(it->second);

    m_currentMusic = std::string(name);
  }

  /// 停止所有音乐
  void StopMusic() {
    if (!m_currentMusic.empty()) {
      auto it = m_musicCache.find(m_currentMusic);
      if (it != m_musicCache.end()) {
        StopMusicStream(it->second);
      }
      m_currentMusic.clear();
    }
  }

  /// 更新音乐流 (每帧调用)
  void Update() {
    if (!m_currentMusic.empty()) {
      auto it = m_musicCache.find(m_currentMusic);
      if (it != m_musicCache.end()) {
        UpdateMusicStream(it->second);
      }
    }
  }

  /// 设置通道音量
  void SetChannelVolume(AudioChannel channel, float volume) {
    m_channelVolumes[static_cast<int>(channel)] =
        std::clamp(volume, 0.0f, 1.0f);

    // 更新正在播放的音乐音量
    if (channel == AudioChannel::Master || channel == AudioChannel::Music) {
      if (!m_currentMusic.empty()) {
        auto it = m_musicCache.find(m_currentMusic);
        if (it != m_musicCache.end()) {
          float vol = m_channelVolumes[static_cast<int>(AudioChannel::Master)] *
                      m_channelVolumes[static_cast<int>(AudioChannel::Music)];
          SetMusicVolume(it->second, vol);
        }
      }
    }
  }

  [[nodiscard]] float GetChannelVolume(AudioChannel channel) const {
    return m_channelVolumes[static_cast<int>(channel)];
  }

  /// 检查音效是否已加载
  [[nodiscard]] bool HasSound(std::string_view name) const {
    return m_soundCache.find(std::string(name)) != m_soundCache.end();
  }

  [[nodiscard]] bool HasMusic(std::string_view name) const {
    return m_musicCache.find(std::string(name)) != m_musicCache.end();
  }

private:
  AudioSystem() {
    // 默认音量
    for (auto &vol : m_channelVolumes) {
      vol = 1.0f;
    }
  }

  ~AudioSystem() { Shutdown(); }

  bool m_initialized{false};
  std::unordered_map<std::string, Sound> m_soundCache;
  std::unordered_map<std::string, Music> m_musicCache;
  std::string m_currentMusic;
  float m_channelVolumes[5]{1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
};

} // namespace NoMoreDay
