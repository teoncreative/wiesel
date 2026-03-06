//
// Standalone Wiesel Editor Application
//

#include "w_editor.hpp"

#include "layer/w_layerimgui.hpp"
#include "scene/w_scene.hpp"
#include "w_engine.hpp"
#include "w_entrypoint.hpp"

using namespace Wiesel;
using namespace Wiesel::Editor;

class EditorApplication : public Application {
 public:
  EditorApplication()
      : Application({"Wiesel Editor"}, {}) {}

  ~EditorApplication() override = default;

  void Init() override {
    auto scene = std::make_shared<Scene>();

    PushLayer(std::make_shared<ImGuiLayer>());
    PushLayer(std::make_shared<EditorLayer>(*this, scene));
  }
};

Application* Wiesel::CreateApp() {
  // Force editor mode when running standalone
  auto& props =
      const_cast<EngineProperties&>(Engine::GetEngineProperties());
  props.editor_enabled = true;

  return new EditorApplication();
}