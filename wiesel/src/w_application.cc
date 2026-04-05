
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_application.h"

#include <chrono>
#include <thread>

#include "asset/w_asset_manager.h"
#include "audio/w_audio.h"
#include "cursor/w_cursor.h"
#include "input/w_input.h"
#include "w_engine.h"

namespace Wiesel {

Application::Application(const WindowProperties&& window_props,
                         const RendererProperties&& renderer_props) {

  layer_counter_ = 0;
  is_running_ = true;
  is_minimized_ = false;

  Engine::InitWindow(std::move(window_props));
  window_ = Engine::window();
  window_->SetEventHandler(WIESEL_BIND_FN(Application::OnEvent));

  window_->GetWindowFramebufferSize(window_size_);
  if (window_size_.width == 0 || window_size_.height == 0) {
    is_minimized_ = true;
  }

  Engine::InitRenderer(std::move(renderer_props));
}

Application::~Application() {
  LOG_DEBUG("Destroying Application");

  for (const auto& item : layers_ | std::views::reverse) {
    item->OnDetach();
  }
  layers_.clear();

  // Clear assets before renderer, models/textures hold Vulkan objects
  // whose destructors need the device to still be alive.
  Engine::CleanupAssets();

  window_ = nullptr;
  Engine::CleanupRenderer();
  Engine::CleanupWindow();
}

void Application::OnEvent(Event& event) {
  PROFILE_ZONE_SCOPED_N("Application::OnEvent");

  // Track input activity for idle detection
  if (event.IsInCategory(EventCategory::kEventCategoryInput)) {
    had_input_this_frame_ = true;
  }

  EventDispatcher dispatcher(event);

  dispatcher.Dispatch<WindowClosedEvent>(WIESEL_BIND_FN(OnWindowClose));
  dispatcher.Dispatch<WindowResizedEvent>(WIESEL_BIND_FN(OnWindowResize));
  dispatcher.Dispatch<WindowMinimizedEvent>(WIESEL_BIND_FN(OnWindowMinimized));
  dispatcher.Dispatch<WindowRestoredEvent>(WIESEL_BIND_FN(OnWindowRestored));

  for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
    const auto& layer = *it;
    if (event.handled_) {
      break;
    }

    {
      PROFILE_ZONE_SCOPED_N("Layer::OnEvent");
      layer->OnEvent(event);
    }
  }

  if (!event.handled_) {
    PROFILE_ZONE_SCOPED_N("InputManager::OnEvent");
    InputManager::OnEvent(event);
  }
}

void Application::PushLayer(const std::shared_ptr<Layer>& layer) {
  layers_.push_back(layer);
  layer->id_ = layer_counter_++;
  layer->OnAttach();
}

void Application::RemoveLayer(const std::shared_ptr<Layer>& layer) {
  // todo
}

void Application::Run() {
  PROFILE_THREAD("Application Thread");
  previous_frame_ = Time::GetTime();

  std::shared_ptr<Renderer> renderer = Engine::renderer();
  while (is_running_) {
    PROFILE_FRAME_MARK();
    float time = Time::GetTime();
    delta_time_ = time - previous_frame_;
    previous_frame_ = time;

    // Clamp delta to avoid physics/animation explosions after hibernation or debugger pause
    if (delta_time_ > 0.25f) {
      delta_time_ = 0.25f;
    }

    fps_timer_ += delta_time_;
    frame_count_++;

    if (fps_timer_ >= 1.0f) {
      fps_ = static_cast<float>(frame_count_) / fps_timer_;
      frame_count_ = 0;
      fps_timer_ = 0.0f;
    }

    // Idle detection: uses had_input_this_frame_ set by OnEvent
    bool has_input = had_input_this_frame_;
    had_input_this_frame_ = false;

    if (has_input) {
      idle_timer_ = 0.0f;
      if (is_idle_) {
        is_idle_ = false;
        LOG_DEBUG("Idle mode disabled");
      }
    } else {
      idle_timer_ += delta_time_;
      if (idle_timer_ >= idle_timeout_ && !is_idle_) {
        is_idle_ = true;
        LOG_DEBUG("Idle mode enabled ({}fps)", idle_max_fps_);
      }
    }

    // FPS cap: sleep to hit target frame time (idle or active).
    // Done before input polling so we don't add latency between
    // events arriving and the frame that processes them.
    {
      float_t target_fps = is_idle_ ? idle_max_fps_ : max_fps_;
      if (target_fps > 0.0f) {
        float_t target_frame_time = 1.0f / target_fps;
        float_t elapsed = Time::GetTime() - previous_frame_;
        float_t remaining = target_frame_time - elapsed;
        if (remaining > 0.0005f) {
          std::this_thread::sleep_for(std::chrono::microseconds(
              static_cast<int64_t>(remaining * 1000000.0f)));
        }
      }
    }

    // INPUT ORDERING (fragile - do not reorder):
    // 1. InputManager::Update() saves previous_pressed from last frame
    // 2. window_->OnUpdate() polls OS events, dispatches press/release to InputManager
    // 3. Layer updates read IsMouseButtonDown/Up (pressed && !previous_pressed)
    // If Update() runs after OnUpdate(), the single-frame Down/Up signals are lost.
    InputManager::Update();
    window_->OnUpdate();
    if (window_resized_) {
      window_->GetWindowFramebufferSize(window_size_);
      renderer->RecreateSwapChain();
      window_resized_ = false;
    }

    Engine::audio().Update();
    Engine::cursor_manager().Update(delta_time_);
    ExecuteQueue();

    float_t scaled_delta = delta_time_ * time_scale_;
    for (const auto& layer : layers_) {
      layer->OnUpdate(scaled_delta);
    }
    if (!is_minimized_) {
      renderer->BeginRender();
      renderer->stats_.frame_time_ms = delta_time_ * 1000.0f;
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
  }
}

void Application::Close() {
  LOG_INFO("Closing the application!");
  is_running_ = false;
}

bool Application::OnWindowClose(WindowClosedEvent& event) {
  Close();
  return true;
}

bool Application::OnWindowResize(WindowResizedEvent& event) {
  window_resized_ = true;
  return false;
}

bool Application::OnWindowMinimized(WindowMinimizedEvent& event) {
  is_minimized_ = true;
  return false;
}

bool Application::OnWindowRestored(WindowRestoredEvent& event) {
  is_minimized_ = false;
  return false;
}

std::shared_ptr<AppWindow> Application::GetWindow() {
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

}  // namespace Wiesel