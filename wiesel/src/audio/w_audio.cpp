
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "asset/w_asset_manager.hpp"
#include "audio/w_audio.hpp"
#include "util/w_logger.hpp"
#include "util/w_vfs.hpp"
#include "w_engine.hpp"

namespace Wiesel {

// -- Custom VFS adapter: routes miniaudio file IO through Wiesel VFS --

struct VfsAudioFile {
  VfsFile file;
};

static ma_result ma_vfs_wiesel_open(ma_vfs* pVFS, const char* pFilePath,
                                    ma_uint32 openMode, ma_vfs_file* pFile) {
  (void)pVFS;
  if (openMode & MA_OPEN_MODE_WRITE) {
    return MA_NOT_IMPLEMENTED;
  }

  auto vfs = Engine::vfs();
  if (!vfs) {
    return MA_ERROR;
  }

  std::string path = pFilePath;
  if (!vfs->FileExists(path)) {
    return MA_DOES_NOT_EXIST;
  }

  auto vfs_file = vfs->Open(path);
  if (vfs_file.Size() == 0) {
    return MA_ERROR;
  }

  auto* handle = new VfsAudioFile{std::move(vfs_file)};
  *pFile = (ma_vfs_file)handle;
  return MA_SUCCESS;
}

static ma_result ma_vfs_wiesel_open_w(ma_vfs* pVFS, const wchar_t* pFilePath,
                                      ma_uint32 openMode, ma_vfs_file* pFile) {
  (void)pVFS;
  (void)pFilePath;
  (void)openMode;
  (void)pFile;
  return MA_NOT_IMPLEMENTED;
}

static ma_result ma_vfs_wiesel_close(ma_vfs* pVFS, ma_vfs_file file) {
  (void)pVFS;
  delete static_cast<VfsAudioFile*>(file);
  return MA_SUCCESS;
}

static ma_result ma_vfs_wiesel_read(ma_vfs* pVFS, ma_vfs_file file, void* pDst,
                                    size_t sizeInBytes, size_t* pBytesRead) {
  (void)pVFS;
  auto* f = static_cast<VfsAudioFile*>(file);
  size_t remaining = f->file.Size() - f->file.Tell();
  size_t to_read = std::min(sizeInBytes, remaining);
  if (to_read == 0) {
    if (pBytesRead) {
      *pBytesRead = 0;
    }
    return MA_AT_END;
  }
  f->file.Read(pDst, to_read);
  if (pBytesRead) {
    *pBytesRead = to_read;
  }
  return MA_SUCCESS;
}

static ma_result ma_vfs_wiesel_write(ma_vfs* pVFS, ma_vfs_file file,
                                     const void* pSrc, size_t sizeInBytes,
                                     size_t* pBytesWritten) {
  (void)pVFS;
  (void)file;
  (void)pSrc;
  (void)sizeInBytes;
  if (pBytesWritten) {
    *pBytesWritten = 0;
  }
  return MA_NOT_IMPLEMENTED;
}

static ma_result ma_vfs_wiesel_seek(ma_vfs* pVFS, ma_vfs_file file,
                                    ma_int64 offset, ma_seek_origin origin) {
  (void)pVFS;
  auto* f = static_cast<VfsAudioFile*>(file);
  switch (origin) {
    case ma_seek_origin_start:
      f->file.Seek(static_cast<size_t>(offset));
      break;
    case ma_seek_origin_current:
      f->file.SeekRelative(static_cast<int64_t>(offset));
      break;
    case ma_seek_origin_end:
      f->file.Seek(f->file.Size() + static_cast<size_t>(offset));
      break;
  }
  return MA_SUCCESS;
}

static ma_result ma_vfs_wiesel_tell(ma_vfs* pVFS, ma_vfs_file file,
                                    ma_int64* pCursor) {
  (void)pVFS;
  auto* f = static_cast<VfsAudioFile*>(file);
  *pCursor = static_cast<ma_int64>(f->file.Tell());
  return MA_SUCCESS;
}

static ma_result ma_vfs_wiesel_info(ma_vfs* pVFS, ma_vfs_file file,
                                    ma_file_info* pInfo) {
  (void)pVFS;
  auto* f = static_cast<VfsAudioFile*>(file);
  pInfo->sizeInBytes = f->file.Size();
  return MA_SUCCESS;
}

struct WieselVfs {
  ma_vfs_callbacks cb;
};

static WieselVfs g_wiesel_vfs = {{
    ma_vfs_wiesel_open,
    ma_vfs_wiesel_open_w,
    ma_vfs_wiesel_close,
    ma_vfs_wiesel_read,
    ma_vfs_wiesel_write,
    ma_vfs_wiesel_seek,
    ma_vfs_wiesel_tell,
    ma_vfs_wiesel_info,
}};

// -- AudioManager implementation --

struct AudioManager::Impl {
  ma_engine engine{};
  bool initialized = false;

  float master_volume = 1.0f;
  float sfx_volume = 1.0f;
  float music_volume = 1.0f;

  uint64_t next_id = 1;

  struct ActiveSound {
    ma_sound* sound = nullptr;
    uint64_t id = 0;
    AudioBus bus = AudioBus::SFX;
    float base_volume = 1.0f;
  };

  std::vector<ActiveSound> active_sounds;
  SoundHandle current_music;

  // Reverb (delay node)
  ma_delay_node* delay_node = nullptr;
  bool reverb_active = false;

  float GetBusVolume(AudioBus bus) const {
    float base = master_volume;
    switch (bus) {
      case AudioBus::SFX:
        return base * sfx_volume;
      case AudioBus::Music:
        return base * music_volume;
      default:
        return base;
    }
  }

  ActiveSound* FindSound(uint64_t id) {
    for (auto& s : active_sounds) {
      if (s.id == id) {
        return &s;
      }
    }
    return nullptr;
  }
};

AudioManager::AudioManager() : impl_(std::make_unique<Impl>()) {}

AudioManager::~AudioManager() {
  Shutdown();
}

bool AudioManager::Init() {
  ma_engine_config config = ma_engine_config_init();
  config.pResourceManagerVFS = &g_wiesel_vfs;
  config.listenerCount = 1;

  ma_result result = ma_engine_init(&config, &impl_->engine);
  if (result != MA_SUCCESS) {
    LOG_ERROR("Failed to initialize audio engine: {}", (int)result);
    return false;
  }
  impl_->initialized = true;
  LOG_INFO("Audio engine initialized");
  return true;
}

void AudioManager::Shutdown() {
  if (!impl_ || !impl_->initialized) {
    return;
  }

  StopAll();
  if (impl_->delay_node) {
    ma_delay_node_uninit(impl_->delay_node, nullptr);
    delete impl_->delay_node;
    impl_->delay_node = nullptr;
  }
  ma_engine_uninit(&impl_->engine);
  impl_->initialized = false;
  LOG_INFO("Audio engine shut down");
}

std::string AudioManager::ResolveClipPath(const AssetHandle& handle) {
  if (!handle.IsValid()) {
    return "";
  }
  const auto* meta = Engine::asset_manager().GetMetadata(handle);
  if (!meta || meta->type != AssetType::Audio) {
    return "";
  }
  return meta->virtual_source_path;
}

// --- Asset handle overloads ---

SoundHandle AudioManager::Play(const AssetHandle& clip,
                               const SoundParams& params) {
  std::string path = ResolveClipPath(clip);
  if (path.empty()) {
    return {};
  }
  return Play(path, params);
}

void AudioManager::PlaySound(const AssetHandle& clip, AudioBus bus,
                             float volume) {
  std::string path = ResolveClipPath(clip);
  if (path.empty()) {
    return;
  }
  PlaySound(path, bus, volume);
}

void AudioManager::PlaySound(const std::string& path, AudioBus bus,
                             float volume) {
  SoundParams params;
  params.bus = bus;
  params.volume = volume;
  Play(path, params);
}

void AudioManager::PlaySoundAt(const AssetHandle& clip,
                               const glm::vec3& position, AudioBus bus,
                               float volume, float min_distance,
                               float max_distance) {
  std::string path = ResolveClipPath(clip);
  if (path.empty()) {
    return;
  }
  SoundParams params;
  params.bus = bus;
  params.volume = volume;
  params.spatial_blend = 1.0f;
  params.position = position;
  params.min_distance = min_distance;
  params.max_distance = max_distance;
  Play(path, params);
}

void AudioManager::PlayMusic(const AssetHandle& clip, float volume) {
  std::string path = ResolveClipPath(clip);
  if (path.empty()) {
    return;
  }
  PlayMusic(path, volume);
}

SoundHandle AudioManager::Play(const std::string& path,
                               const SoundParams& params) {
  if (!impl_->initialized) {
    return {};
  }

  auto* sound = new ma_sound;
  ma_uint32 flags = MA_SOUND_FLAG_DECODE;  // decode upfront for low latency
  ma_result result = ma_sound_init_from_file(&impl_->engine, path.c_str(),
                                             flags, nullptr, nullptr, sound);
  if (result != MA_SUCCESS) {
    LOG_ERROR("Failed to load sound '{}': {}", path, (int)result);
    delete sound;
    return {};
  }

  float final_volume = params.volume * impl_->GetBusVolume(params.bus);
  ma_sound_set_volume(sound, final_volume);
  ma_sound_set_looping(sound, params.loop ? MA_TRUE : MA_FALSE);

  if (params.pitch != 1.0f) {
    ma_sound_set_pitch(sound, params.pitch);
  }

  // Spatialization
  if (params.spatial_blend > 0.0f) {
    ma_sound_set_spatialization_enabled(sound, MA_TRUE);
    ma_sound_set_position(sound, params.position.x, params.position.y,
                          params.position.z);
    ma_sound_set_min_distance(sound, params.min_distance);
    ma_sound_set_max_distance(sound, params.max_distance);
    // Blend: for partial spatial_blend we scale the min gain
    // At spatial_blend=0 the sound is full volume everywhere (2D),
    // at spatial_blend=1 it follows distance attenuation normally.
    ma_sound_set_min_gain(sound, 1.0f - params.spatial_blend);
  } else {
    ma_sound_set_spatialization_enabled(sound, MA_FALSE);
  }

  ma_sound_start(sound);

  uint64_t id = impl_->next_id++;
  impl_->active_sounds.push_back({sound, id, params.bus, params.volume});
  return SoundHandle(id);
}

void AudioManager::Stop(SoundHandle handle) {
  if (!handle.IsValid()) {
    return;
  }

  for (auto it = impl_->active_sounds.begin(); it != impl_->active_sounds.end();
       ++it) {
    if (it->id == handle.Id()) {
      ma_sound_stop(it->sound);
      ma_sound_uninit(it->sound);
      delete it->sound;
      impl_->active_sounds.erase(it);
      break;
    }
  }
}

void AudioManager::StopAll() {
  for (auto& s : impl_->active_sounds) {
    ma_sound_stop(s.sound);
    ma_sound_uninit(s.sound);
    delete s.sound;
  }
  impl_->active_sounds.clear();
  impl_->current_music = {};
}

void AudioManager::SetSoundPosition(SoundHandle handle,
                                    const glm::vec3& position) {
  if (!handle.IsValid()) {
    return;
  }
  auto* s = impl_->FindSound(handle.Id());
  if (s) {
    ma_sound_set_position(s->sound, position.x, position.y, position.z);
  }
}

void AudioManager::SetSoundVolume(SoundHandle handle, float volume) {
  if (!handle.IsValid()) {
    return;
  }
  auto* s = impl_->FindSound(handle.Id());
  if (s) {
    s->base_volume = volume;
    ma_sound_set_volume(s->sound, volume * impl_->GetBusVolume(s->bus));
  }
}

void AudioManager::PlayMusic(const std::string& path, float volume) {
  StopMusic();
  SoundParams params;
  params.bus = AudioBus::Music;
  params.volume = volume;
  params.loop = true;
  impl_->current_music = Play(path, params);
}

void AudioManager::StopMusic() {
  if (impl_->current_music.IsValid()) {
    Stop(impl_->current_music);
    impl_->current_music = {};
  }
}

bool AudioManager::IsMusicPlaying() const {
  return impl_->current_music.IsValid();
}

void AudioManager::SetMasterVolume(float volume) {
  impl_->master_volume = std::clamp(volume, 0.0f, 1.0f);
  for (auto& s : impl_->active_sounds) {
    ma_sound_set_volume(s.sound, s.base_volume * impl_->GetBusVolume(s.bus));
  }
}

void AudioManager::SetSFXVolume(float volume) {
  impl_->sfx_volume = std::clamp(volume, 0.0f, 1.0f);
  for (auto& s : impl_->active_sounds) {
    if (s.bus == AudioBus::SFX) {
      ma_sound_set_volume(s.sound,
                          s.base_volume * impl_->GetBusVolume(AudioBus::SFX));
    }
  }
}

void AudioManager::SetMusicVolume(float volume) {
  impl_->music_volume = std::clamp(volume, 0.0f, 1.0f);
  for (auto& s : impl_->active_sounds) {
    if (s.bus == AudioBus::Music) {
      ma_sound_set_volume(s.sound,
                          s.base_volume * impl_->GetBusVolume(AudioBus::Music));
    }
  }
}

float AudioManager::GetMasterVolume() const {
  return impl_->master_volume;
}

float AudioManager::GetSFXVolume() const {
  return impl_->sfx_volume;
}

float AudioManager::GetMusicVolume() const {
  return impl_->music_volume;
}

void AudioManager::SetListenerPosition(const glm::vec3& position) {
  if (!impl_->initialized) {
    return;
  }
  ma_engine_listener_set_position(&impl_->engine, 0, position.x, position.y,
                                  position.z);
}

void AudioManager::SetListenerDirection(const glm::vec3& forward,
                                        const glm::vec3& up) {
  if (!impl_->initialized) {
    return;
  }
  ma_engine_listener_set_direction(&impl_->engine, 0, forward.x, forward.y,
                                   forward.z);
  ma_engine_listener_set_world_up(&impl_->engine, 0, up.x, up.y, up.z);
}

void AudioManager::SetReverb(float delay_ms, float decay, float wet) {
  if (!impl_->initialized) {
    return;
  }

  ma_engine* engine = &impl_->engine;
  ma_uint32 channels = ma_engine_get_channels(engine);
  ma_uint32 sample_rate = ma_engine_get_sample_rate(engine);
  ma_uint32 delay_frames = static_cast<ma_uint32>(
      (delay_ms / 1000.0f) * static_cast<float>(sample_rate));

  if (!impl_->delay_node) {
    impl_->delay_node = new ma_delay_node;
    ma_delay_node_config config =
        ma_delay_node_config_init(channels, sample_rate, delay_frames, decay);
    ma_result result = ma_delay_node_init(ma_engine_get_node_graph(engine),
                                          &config, nullptr, impl_->delay_node);
    if (result != MA_SUCCESS) {
      LOG_ERROR("Failed to create delay node: {}", (int)result);
      delete impl_->delay_node;
      impl_->delay_node = nullptr;
      return;
    }

    // Insert into the graph: endpoint -> delay_node -> endpoint
    ma_node* endpoint = ma_engine_get_endpoint(engine);
    ma_node_attach_output_bus(impl_->delay_node, 0, endpoint, 0);
    ma_node_attach_output_bus(endpoint, 0, impl_->delay_node, 0);
  }

  ma_delay_node_set_decay(impl_->delay_node, decay);
  ma_delay_node_set_wet(impl_->delay_node, wet);
  ma_delay_node_set_dry(impl_->delay_node, 1.0f - wet);
  impl_->reverb_active = true;
}

void AudioManager::ClearReverb() {
  if (!impl_->delay_node) {
    return;
  }

  ma_delay_node_set_wet(impl_->delay_node, 0.0f);
  ma_delay_node_set_dry(impl_->delay_node, 1.0f);
  impl_->reverb_active = false;
}

void AudioManager::Update() {
  if (!impl_->initialized) {
    return;
  }

  for (auto it = impl_->active_sounds.begin();
       it != impl_->active_sounds.end();) {
    if (!ma_sound_is_looping(it->sound) && ma_sound_at_end(it->sound)) {
      ma_sound_uninit(it->sound);
      delete it->sound;
      it = impl_->active_sounds.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace Wiesel