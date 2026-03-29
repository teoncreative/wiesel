//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "w_pch.h"
//#ifdef DEBUG
//#define WIESEL_PROFILE 1
//#endif
#include "events/w_appevents.h"
#include "events/w_events.h"
#include "events/w_keyevents.h"
#include "events/w_mouseevents.h"
#include "layer/w_layer.h"
#include "layer/w_layerimgui.h"
#include "rendering/w_renderer.h"
#include "scene/w_scene.h"
#include "util/w_utils.h"

namespace Wiesel {
class Application {
 public:
  Application(const WindowProperties&& window_props,
              const RendererProperties&& renderer_props);
  virtual ~Application();

  virtual void Init() = 0;

  void Run();
  void Close();

  void OnEvent(Event& event);

  void PushLayer(const std::shared_ptr<Layer>& layer);
  void RemoveLayer(const std::shared_ptr<Layer>& layer);

  bool OnWindowClose(WindowCloseEvent& event);
  bool OnWindowResize(WindowResizeEvent& event);

  WIESEL_GETTER_FN std::shared_ptr<AppWindow> GetWindow();

  WIESEL_GETTER_FN float_t GetFPS() const { return fps_; }

  WIESEL_GETTER_FN float_t GetDeltaTime() const { return delta_time_; }

  void SetMaxFPS(float_t max_fps) { max_fps_ = max_fps; }

  WIESEL_GETTER_FN float_t GetMaxFPS() const { return max_fps_; }

  void SetIdleMaxFPS(float_t fps) { idle_max_fps_ = fps; }

  void SetIdleTimeout(float_t seconds) { idle_timeout_ = seconds; }

  void SetTimeScale(float_t time_scale) { time_scale_ = time_scale; }

  WIESEL_GETTER_FN float_t GetTimeScale() const { return time_scale_; }

  WIESEL_GETTER_FN const WindowSize& GetWindowSize();

  void SubmitToMainThread(std::function<void()> fn);

 private:
  void ExecuteQueue();
  void UpdateKeyboardAxis();

 protected:
  std::vector<std::function<void()>> main_thread_queue_;
  std::mutex main_thread_queue_mutex_;

  bool is_running_;
  bool is_minimized_;
  bool window_resized_;
  WindowSize window_size_;
  // proper layer stack
  std::vector<std::shared_ptr<Layer>> layers_;
  std::shared_ptr<ImGuiLayer> imgui_layer_;
  uint32_t layer_counter_;
  std::shared_ptr<AppWindow> window_;
  float_t previous_frame_ = 0.0;
  float_t delta_time_ = 0.0;

  float_t fps_timer_ = 0.0f;
  uint32_t frame_count_ = 0;
  float_t fps_ = 0.0f;
  float_t max_fps_ = 0.0f;        // 0 = unlimited
  float_t idle_max_fps_ = 15.0f;  // FPS when idle (no input)
  float_t idle_timeout_ = 3.0f;   // seconds of no input before idle mode
  float_t idle_timer_ = 0.0f;
  bool is_idle_ = false;
  bool had_input_this_frame_ = false;
  float_t time_scale_ = 1.0f;
};

}  // namespace Wiesel
