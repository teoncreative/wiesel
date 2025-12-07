
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
  if (window_size_.width == 0 || window_size_.height == 0) {
    is_minimized_ = true;
  }

  Engine::InitRenderer(std::move(renderer_props));
}

Application::~Application() {
  LOG_DEBUG("Destroying Application");
  for (const auto& item : layers_) {
    item->OnDetach();
  }
  layers_.clear();
  window_ = nullptr;
  Engine::CleanupRenderer();
  Engine::CleanupWindow();
}

void Application::OnEvent(Event& event) {
  EventDispatcher dispatcher(event);

  dispatcher.Dispatch<WindowCloseEvent>(WIESEL_BIND_FN(OnWindowClose));
  dispatcher.Dispatch<WindowResizeEvent>(WIESEL_BIND_FN(OnWindowResize));

  InputManager::OnEvent(event);
  if (event.m_Handled) {
    return;
  }

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
  layer->id_ = layer_counter_++;
  layer->OnAttach();
}

void Application::RemoveLayer(const Ref<Layer>& layer) {
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

      renderer->BeginRender();
      for (const auto& layer : layers_) {
        layer->OnPrePresent();
      }
      if (renderer->BeginPresent()) {
        for (const auto& layer : layers_) {
          layer->OnBeginPresent();
        }
        for (const auto& layer : layers_) {
          layer->OnPresent();
        }
        renderer->EndPresent();
      }
      for (const auto& layer : layers_) {
        layer->OnPostPresent();
      }
    }

    window_->OnUpdate();

    if (window_resized_) {
      window_->GetWindowFramebufferSize(window_size_);
      if (window_size_.width == 0 || window_size_.height == 0) {
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

Ref<AppWindow> Application::GetWindow() {
  return window_;
}

const WindowSize& Application::GetWindowSize() {
  return window_size_;
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