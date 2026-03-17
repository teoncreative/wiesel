
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "asset/w_asset_handle.hpp"
#include "w_pch.hpp"

namespace Wiesel {

enum class AudioBus : int {
  Master = 0,
  SFX = 1,
  Music = 2,
};

class SoundHandle {
 public:
  SoundHandle() = default;

  bool IsValid() const { return id_ != 0; }
  uint64_t Id() const { return id_; }

 private:
  friend class AudioManager;
  explicit SoundHandle(uint64_t id) : id_(id) {}
  uint64_t id_ = 0;
};

struct SoundParams {
  AudioBus bus = AudioBus::SFX;
  float volume = 1.0f;
  float pitch = 1.0f;
  bool loop = false;
  float spatial_blend = 0.0f;   // 0 = 2D, 1 = fully 3D
  glm::vec3 position = {0, 0, 0};
  float min_distance = 1.0f;
  float max_distance = 100.0f;
};

// Audio backend is entirely hidden behind the implementation.
class AudioManager {
 public:
  AudioManager();
  ~AudioManager();

  bool Init();
  void Shutdown();

  // Resolve an asset handle to its VFS path. Returns empty if invalid.
  static std::string ResolveClipPath(const AssetHandle& handle);

  // Play by VFS path
  SoundHandle Play(const std::string& path, const SoundParams& params = {});

  // Play by asset handle
  SoundHandle Play(const AssetHandle& clip, const SoundParams& params = {});

  // Fire-and-forget convenience
  void PlaySound(const AssetHandle& clip, AudioBus bus = AudioBus::SFX,
                 float volume = 1.0f);
  void PlaySound(const std::string& path, AudioBus bus = AudioBus::SFX,
                 float volume = 1.0f);

  // Fire-and-forget 3D
  void PlaySoundAt(const AssetHandle& clip, const glm::vec3& position,
                   AudioBus bus = AudioBus::SFX, float volume = 1.0f,
                   float min_distance = 1.0f, float max_distance = 100.0f);

  void Stop(SoundHandle handle);
  void StopAll();

  // Update a playing sound's position/volume
  void SetSoundPosition(SoundHandle handle, const glm::vec3& position);
  void SetSoundVolume(SoundHandle handle, float volume);

  // Music (auto-loops, stops previous)
  void PlayMusic(const std::string& path, float volume = 1.0f);
  void PlayMusic(const AssetHandle& clip, float volume = 1.0f);
  void StopMusic();
  bool IsMusicPlaying() const;

  // Volume (0.0 - 1.0)
  void SetMasterVolume(float volume);
  void SetSFXVolume(float volume);
  void SetMusicVolume(float volume);
  float GetMasterVolume() const;
  float GetSFXVolume() const;
  float GetMusicVolume() const;

  // 3D listener
  void SetListenerPosition(const glm::vec3& position);
  void SetListenerDirection(const glm::vec3& forward, const glm::vec3& up);

  void Update();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// ECS component - the "speaker" on an entity.
// Has its own spatial/volume settings. Can play its default clip or any clip.
struct AudioSourceComponent {
  // Default clip (serialized as asset handle)
  AssetHandle clip;

  // Source settings
  AudioBus bus = AudioBus::SFX;
  float volume = 1.0f;
  float pitch = 1.0f;
  bool loop = false;
  bool play_on_start = false;
  bool mute = false;
  float spatial_blend = 0.0f;   // 0 = fully 2D, 1 = fully 3D
  float min_distance = 1.0f;
  float max_distance = 100.0f;

  // Runtime state (not serialized)
  SoundHandle playing_handle_;
  bool started_ = false;

  // Build SoundParams from this source's settings + a world position
  SoundParams MakeParams(const glm::vec3& position) const {
    SoundParams p;
    p.bus = bus;
    p.volume = volume;
    p.pitch = pitch;
    p.loop = loop;
    p.spatial_blend = spatial_blend;
    p.position = position;
    p.min_distance = min_distance;
    p.max_distance = max_distance;
    return p;
  }
};

}  // namespace Wiesel