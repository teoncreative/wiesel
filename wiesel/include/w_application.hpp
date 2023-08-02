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
#include "util/w_profiler.hpp"
#include "util/w_utils.hpp"

namespace Wiesel {
class Application {
 public:
  Application(WindowProperties props);
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

  WIESEL_GETTER_FN Ref<AppWindow> GetWindow();
  WIESEL_GETTER_FN const WindowSize& GetWindowSize();
  WIESEL_GETTER_FN Ref<Scene> GetScene();

  void SubmitToMainThread(std::function<void()> fn);

  WIESEL_GETTER_FN static Application* Get();

 private:
  void ExecuteQueue();
  void UpdateKeyboardAxis();

 protected:
  static Application* s_Application;

  std::vector<std::function<void()>> m_MainThreadQueue;
  std::mutex m_MainThreadQueueMutex;

  bool m_IsRunning;
  bool m_IsMinimized;
  bool m_WindowResized;
  WindowSize m_WindowSize;
  // proper layer stack
  std::vector<Ref<Layer>> m_Layers;
  std::vector<Ref<Layer>>
      m_Overlays;  // maybe have another class extending from Layer like OverlayLayer
  Ref<ImGuiLayer> m_ImGuiLayer;
  uint32_t m_LayerCounter;
  Ref<AppWindow> m_Window;
  float_t m_PreviousFrame = 0.0;
  float_t m_DeltaTime = 0.0;
  Ref<Scene> m_Scene;  // move this to somewhere else
};

}  // namespace Wiesel
