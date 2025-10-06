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

#include "w_pch.hpp"
//#ifdef DEBUG
//#define WIESEL_PROFILE 1
//#endif
#include "events/w_appevents.hpp"
#include "events/w_events.hpp"
#include "events/w_keyevents.hpp"
#include "events/w_mouseevents.hpp"
#include "layer/w_layer.hpp"
#include "layer/w_layerimgui.hpp"
#include "rendering/w_renderer.hpp"
#include "scene/w_scene.hpp"
#include "util/w_utils.hpp"

namespace Wiesel {
class Application {
 public:
  Application(const WindowProperties&& window_props, const RendererProperties&& renderer_props);
  virtual ~Application();

  virtual void Init() = 0;

  void Run();
  void Close();

  void OnEvent(Event& event);

  void PushLayer(const Ref<Layer>& layer);
  void RemoveLayer(const Ref<Layer>& layer);

  void PushOverlay(const Ref<Layer>& layer);
  void RemoveOverlay(const Ref<Layer>& layer);

  bool OnWindowClose(WindowCloseEvent& event);
  bool OnWindowResize(WindowResizeEvent& event);
  bool OnKeyPressed(KeyPressedEvent& event);
  bool OnKeyReleased(KeyReleasedEvent& event);
  bool OnMouseMoved(MouseMovedEvent& event);
  bool OnJoystickConnect(JoystickConnectedEvent& event);
  bool OnJoystickDisconnect(JoystickDisconnectedEvent& event);
  bool OnJoystickButtonPressed(JoystickButtonPressedEvent& event);
  bool OnJoystickButtonReleased(JoystickButtonReleasedEvent& event);
  bool OnJoystickButtonAxisMoved(JoystickAxisMovedEvent& event);

  WIESEL_GETTER_FN Ref<AppWindow> GetWindow();
  WIESEL_GETTER_FN float_t GetFPS() const { return fps_; }
  WIESEL_GETTER_FN float_t GetDeltaTime() const { return delta_time_; }
  WIESEL_GETTER_FN const WindowSize& GetWindowSize();
  WIESEL_GETTER_FN Ref<Scene> GetScene();

  void SubmitToMainThread(std::function<void()> fn);

  WIESEL_GETTER_FN static Application* Get();

 private:
  void ExecuteQueue();
  void UpdateKeyboardAxis();

 protected:
  static Application* application_;

  std::vector<std::function<void()>> main_thread_queue_;
  std::mutex main_thread_queue_mutex_;

  bool is_running_;
  bool is_minimized_;
  bool window_resized_;
  WindowSize window_size_;
  // proper layer stack
  std::vector<Ref<Layer>> layers_;
  std::vector<Ref<Layer>> overlays_;  // maybe have another class extending from Layer like OverlayLayer
  Ref<ImGuiLayer> imgui_layer_;
  uint32_t layer_counter_;
  Ref<AppWindow> window_;
  float_t previous_frame_ = 0.0;
  float_t delta_time_ = 0.0;

  float_t fps_timer_ = 0.0f;
  uint32_t frame_count_ = 0;
  float_t fps_ = 0.0f;

  Ref<Scene> scene_;  // move this to somewhere else

};

}  // namespace Wiesel
