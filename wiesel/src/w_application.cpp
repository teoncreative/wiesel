
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_application.hpp"

#include "input/w_input.hpp"
#include "w_engine.hpp"
#include "window/w_glfwwindow.hpp"

namespace Wiesel {

Application* Application::application_;

Application::Application(const WindowProperties&& window_props, const RendererProperties&& renderer_props) {
  application_ = this;

  layer_counter_ = 0;
  is_running_ = true;
  is_minimized_ = false;

  Engine::InitWindow(std::move(window_props));
  window_ = Engine::GetWindow();
  window_->SetEventHandler(WIESEL_BIND_FN(Application::OnEvent));

  window_->GetWindowFramebufferSize(window_size_);
  if (window_size_.Width == 0 || window_size_.Height == 0) {
    is_minimized_ = true;
  }

  Engine::InitRenderer(std::move(renderer_props));
  scene_ = CreateReference<Scene>();
  imgui_layer_ = CreateReference<ImGuiLayer>();
  PushOverlay(imgui_layer_);
}

Application::~Application() {
  LOG_DEBUG("Destroying Application");
  for (const auto& item : overlays_) {
    item->OnDetach();
  }
  overlays_.clear();
  for (const auto& item : layers_) {
    item->OnDetach();
  }
  layers_.clear();
  scene_ = nullptr;
  window_ = nullptr;
  Engine::CleanupRenderer();
  Engine::CleanupWindow();
}

void Application::OnEvent(Event& event) {
  EventDispatcher dispatcher(event);

  dispatcher.Dispatch<WindowCloseEvent>(WIESEL_BIND_FN(OnWindowClose));
  dispatcher.Dispatch<WindowResizeEvent>(WIESEL_BIND_FN(OnWindowResize));
  dispatcher.Dispatch<KeyPressedEvent>(WIESEL_BIND_FN(OnKeyPressed));
  dispatcher.Dispatch<KeyReleasedEvent>(WIESEL_BIND_FN(OnKeyReleased));
  dispatcher.Dispatch<MouseMovedEvent>(WIESEL_BIND_FN(OnMouseMoved));
  dispatcher.Dispatch<JoystickConnectedEvent>(WIESEL_BIND_FN(OnJoystickConnect));
  dispatcher.Dispatch<JoystickDisconnectedEvent>(WIESEL_BIND_FN(OnJoystickDisconnect));
  dispatcher.Dispatch<JoystickButtonPressedEvent>(WIESEL_BIND_FN(OnJoystickButtonPressed));
  dispatcher.Dispatch<JoystickButtonReleasedEvent>(WIESEL_BIND_FN(OnJoystickButtonReleased));
  dispatcher.Dispatch<JoystickAxisMovedEvent>(WIESEL_BIND_FN(OnJoystickButtonAxisMoved));

  if (event.m_Handled) {
    return;
  }

  scene_->OnEvent(event);
  for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
    const auto& layer = *it;
    if (event.m_Handled) {
      break;
    }

    layer->OnEvent(event);
  }
}

void Application::PushLayer(const Ref<Layer>& layer) {
  layers_.push_back(layer);
  layer->OnAttach();
  layer->id_ = layer_counter_++;
}

void Application::RemoveLayer(const Ref<Layer>& layer) {
  // todo
}

void Application::PushOverlay(const Ref<Layer>& layer) {
  overlays_.push_back(layer);
  layer->OnAttach();
  layer->id_ = layer_counter_++;
}

void Application::RemoveOverlay(const Ref<Layer>& layer) {
  // todo
}

void Application::Run() {
  PROFILE_THREAD("Application Thread");
  previous_frame_ = Time::GetTime();

  Ref<Renderer> renderer = Engine::GetRenderer();
  while (is_running_) {
    PROFILE_FRAME_MARK();
    float time = Time::GetTime();
    delta_time_ = time - previous_frame_;
    previous_frame_ = time;

    fps_timer_ += delta_time_;
    frame_count_++;

    if (fps_timer_ >= 1.0f) {
      fps_ = static_cast<float>(frame_count_) / fps_timer_;
      frame_count_ = 0;
      fps_timer_ = 0.0f;
    }

    ExecuteQueue();

    if (!is_minimized_) {
      for (const auto& layer : layers_) {
        layer->OnUpdate(delta_time_);
      }
      for (const auto& layer : overlays_) {
        layer->OnUpdate(delta_time_);
      }
      scene_->OnUpdate(delta_time_);

      renderer->BeginRender();
      scene_->Render();
      if (renderer->BeginPresent()) {
        imgui_layer_->OnBeginFrame();
        for (const auto& layer : overlays_) {
          layer->OnImGuiRender();
        }
        imgui_layer_->OnEndFrame();
        /*renderer->DrawFullscreen(renderer->GetPresentPipeline(),
                                 {renderer->GetCameraData()->CompositeOutputDescriptor});*/
        renderer->EndPresent();
      }
      for (const auto& layer : overlays_) {
        layer->OnPostRender();
      }
      scene_->ProcessDestroyQueue();
    }

    window_->OnUpdate();

    if (window_resized_) {
      window_->GetWindowFramebufferSize(window_size_);
      if (window_size_.Width == 0 || window_size_.Height == 0) {
        is_minimized_ = true;
      } else {
        is_minimized_ = false;
        renderer->RecreateSwapChain();
      }
      window_resized_ = false;
    }
  }
}

void Application::Close() {
  LOG_INFO("Closing the application!");
  is_running_ = false;
}

bool Application::OnWindowClose(WindowCloseEvent& event) {
  Close();
  return true;
}

bool Application::OnWindowResize(WindowResizeEvent& event) {
  window_resized_ = true;
  return false;
}

// todo handle input logic in InputManager
void Application::UpdateKeyboardAxis() {
  bool right = InputManager::GetKey("Right");
  bool left = InputManager::GetKey("Left");
  bool up = InputManager::GetKey("Up");
  bool down = InputManager::GetKey("Down");

  if (right && !left) {
    InputManager::axis_["Horizontal"] = 1;
  } else if (!right && left) {
    InputManager::axis_["Horizontal"] = -1;
  } else {
    InputManager::axis_["Horizontal"] = 0;
  }

  if (up && !down) {
    InputManager::axis_["Vertical"] = 1;
  } else if (!up && down) {
    InputManager::axis_["Vertical"] = -1;
  } else {
    InputManager::axis_["Vertical"] = 0;
  }
}

bool Application::OnKeyPressed(KeyPressedEvent& event) {
  InputManager::input_mode_ = kInputModeKeyboardAndMouse;
  InputManager::keys_[event.GetKeyCode()].pressed = true;
  UpdateKeyboardAxis();
  return false;
}

bool Application::OnKeyReleased(KeyReleasedEvent& event) {
  InputManager::keys_[event.GetKeyCode()].pressed = false;
  UpdateKeyboardAxis();
  return false;
}

bool Application::OnMouseMoved(MouseMovedEvent& event) {
  InputManager::input_mode_ = kInputModeKeyboardAndMouse;
  InputManager::mouse_x_ = event.GetX();
  InputManager::mouse_y_ = event.GetY();
  // todo mouse delta raw
  if (event.GetCursorMode() == CursorModeRelative) {
    InputManager::axis_["Mouse X"] +=
        InputManager::mouse_axis_sens_x_ *
        (((window_size_.Width / 2.0f) - event.GetX()) / window_size_.Width);
    InputManager::axis_["Mouse Y"] +=
        InputManager::mouse_axis_sens_y_ *
        (((window_size_.Height / 2.0f) - event.GetY()) / window_size_.Width);
    InputManager::axis_["Mouse Y"] = std::clamp(
        InputManager::axis_["Mouse Y"], -InputManager::mouse_axis_limit_y_,
        InputManager::mouse_axis_limit_y_);
  }
  return false;
}

bool Application::OnJoystickConnect(JoystickConnectedEvent& event) {
  return false;
}

bool Application::OnJoystickDisconnect(JoystickDisconnectedEvent& event) {
  return false;
}

bool Application::OnJoystickButtonPressed(JoystickButtonPressedEvent& event) {
  return false;
}

bool Application::OnJoystickButtonReleased(JoystickButtonReleasedEvent& event) {
  return false;
}

bool Application::OnJoystickButtonAxisMoved(JoystickAxisMovedEvent& event) {
  return false;
}

Ref<AppWindow> Application::GetWindow() {
  return window_;
}

const WindowSize& Application::GetWindowSize() {
  return window_size_;
}

Ref<Scene> Application::GetScene() {
  return scene_;
}

void Application::SubmitToMainThread(std::function<void()> fn) {
  std::scoped_lock<std::mutex> lock(main_thread_queue_mutex_);
  main_thread_queue_.emplace_back(fn);
}

void Application::ExecuteQueue() {
  // this has to be inside its own scope or it might have problems
  std::scoped_lock<std::mutex> lock(main_thread_queue_mutex_);
  for (auto& func : main_thread_queue_) {
    func();
  }
  main_thread_queue_.clear();
}

Application* Application::Get() {
  return application_;
}

}  // namespace Wiesel