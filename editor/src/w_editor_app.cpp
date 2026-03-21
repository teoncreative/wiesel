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
      : Application({"Wiesel Editor", {1600, 900}, true}, {}) {}

  ~EditorApplication() override = default;

  void Init() override {
    PushLayer(std::make_shared<ImGuiLayer>());
    PushLayer(std::make_shared<EditorLayer>(*this));
  }
};

Application* Wiesel::CreateApp() {
  // Force editor mode when running standalone
  auto& props =
      const_cast<EngineProperties&>(Engine::properties());
  props.editor_enabled = true;

  return new EditorApplication();
}