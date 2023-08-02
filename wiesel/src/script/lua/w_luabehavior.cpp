
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "script/lua/w_luabehavior.hpp"

#include "input/w_input.hpp"
#include "script/lua/w_scriptglue.hpp"
#include "w_application.hpp"

#define HANDLE_FUNCTION_CALL(result)                                         \
  if (!result) {                                                             \
    LOG_ERROR("{} {}", result.errorCode().message(), result.errorMessage()); \
  }

namespace Wiesel {
LuaBehavior::LuaBehavior(Wiesel::Entity entity, const std::string& file)
    : IBehavior("Script" + (!file.empty() ? " (" + file + ")" : ""), entity,
                file) {
  if (file.empty()) {
    m_Unset = true;
    m_Enabled = false;
    return;
  }

  // move binding codes to script glue file!
  lua_State* luaState = luaL_newstate();
  m_LuaState = luaState;

  luaL_openlibs(luaState);

  std::function<void(const char*, lua_State*)> require =
      +[](const char* name, lua_State* state) {
        ScriptGlue::RegisterModule(name, state);
      };
  std::function<luabridge::LuaRef(const char*, lua_State*)> getComponent =
      [this](const char* name, lua_State* state) -> luabridge::LuaRef {
    return ScriptGlue::GetComponentGetter(name)(this->GetEntity(), state);
  };
  std::function<void(const char*)> logDebug = [this](const char* msg) {
    LOG_DEBUG("{}: {}", this->GetName(), msg);
  };
  std::function<void(const char*)> logInfo = [this](const char* msg) {
    LOG_INFO("{}: {}", this->GetName(), msg);
  };
  std::function<void(const char*)> logWarn = [this](const char* msg) {
    LOG_WARN("{}: {}", this->GetName(), msg);
  };
  std::function<void(const char*)> logError = [this](const char* msg) {
    LOG_ERROR("{}: {}", this->GetName(), msg);
  };

  std::function<void(luabridge::LuaRef)> executeAsync =
      [this](luabridge::LuaRef fn) {
        LOG_DEBUG("Execute async functionality is not implemented yet!");
      };

  luabridge::getGlobalNamespace(luaState).addFunction("require", require);
  try {
    int scriptLoadStatus = luaL_dofile(luaState, m_File.c_str());

    // define error reporter for any Lua error
    ScriptGlue::ReportErrors(luaState, scriptLoadStatus);
    if (scriptLoadStatus != 0) {
      throw std::runtime_error("Script failed to load!");
    }

    // binding functions
    m_FnOnLoad = CreateScope<luabridge::LuaRef>(
        luabridge::getGlobal(luaState, "OnLoad"));
    m_FnOnEnable = CreateScope<luabridge::LuaRef>(
        luabridge::getGlobal(luaState, "OnEnable"));
    m_FnOnDisable = CreateScope<luabridge::LuaRef>(
        luabridge::getGlobal(luaState, "OnDisable"));
    m_FnStart =
        CreateScope<luabridge::LuaRef>(luabridge::getGlobal(luaState, "Start"));
    m_FnUpdate = CreateScope<luabridge::LuaRef>(
        luabridge::getGlobal(luaState, "Update"));

    luabridge::getGlobalNamespace(luaState).addFunction("print", logInfo);

    luabridge::getGlobalNamespace(luaState)
        .addFunction("ExecuteAsync", executeAsync)
        .addFunction("LogDebug", logDebug)
        .addFunction("LogInfo", logInfo)
        .addFunction("LogWarn", logWarn)
        .addFunction("LogError", logError)
        .addFunction("GetComponent", getComponent);

    ScriptGlue::ScriptVec3::Link(luaState);
    ScriptGlue::ScriptTransformComponent::Link(luaState);

    // todo cleanup this mess
    std::vector<std::string> variables{};

    lua_getglobal(luaState, "vars");
    lua_pushnil(luaState);  // push the first key
    while (lua_next(luaState, -2) != 0) {
      // get the key (which is now at index -2)
      const char* key = lua_tostring(luaState, -2);

      if (lua_isfunction(luaState, -1) || std::string(key).starts_with("_")) {
        lua_pop(luaState, 1);
        continue;
      }

      if (lua_isnumber(luaState, -1)) {
        variables.push_back(key);
      } else if (lua_istable(luaState, -1)) {
        lua_gettable(luaState, -1);
        lua_pushnil(luaState);  //does not work with or without this line
        while (lua_next(luaState, -2) != 0) {
          const std::string& vkey = lua_tostring(luaState, -2);
          if (vkey == "type" && lua_isstring(luaState, -1)) {
            const std::string& type = lua_tostring(luaState, -1);

            //
          }
          lua_pop(luaState, 1);
        }
      }

      // pop the value, but leave the key on the stack for the next iteration
      lua_pop(luaState, 1);
    }
    lua_pop(luaState, 1);  // pop the namespace from the stack

    {
      luabridge::LuaRef vars = luabridge::getGlobal(luaState, "vars");
      for (const auto& key : variables) {
        auto var = vars[key.c_str()];
        auto value =
            var.isNumber() ? vars[key.c_str()].cast<double>().value() : 0;
        Ref<ExposedVariable<double>> ref =
            CreateReference<LuaExposedVariable<double>>(value, key, this);
        m_ExposedDoubles.push_back(ref);
      }
    }

    // reset user defined namespace
    lua_getglobal(luaState, "vars");
    lua_pushnil(luaState);
    lua_setglobal(luaState, "vars");

    luabridge::Namespace varsNamespace =
        luabridge::getGlobalNamespace(luaState).beginNamespace("vars");
    for (const auto& ref : m_ExposedDoubles) {
      std::function<double()> get = [ref]() {
        return ref->Get();
      };
      std::function<void(double)> set = [ref](double v) {
        ref->Set(v);
      };
      varsNamespace.addProperty(ref->Name.c_str(), get, set);
    }
    varsNamespace.endNamespace();

    if (m_FnOnLoad && m_FnOnLoad->isValid()) {
      HANDLE_FUNCTION_CALL((*m_FnOnLoad)());
    }
    LOG_INFO("{} loaded and bound!", m_Name);
  } catch (std::exception e) {
    LOG_ERROR("Failed to load script {}, what: {}", m_Name, e.what());
    return;
  }
}

LuaBehavior::~LuaBehavior() {
  m_ExposedDoubles.clear();
  m_FnOnLoad = nullptr;
  m_FnOnEnable = nullptr;
  m_FnOnDisable = nullptr;
  m_FnStart = nullptr;
  m_FnUpdate = nullptr;
  if (m_LuaState) {
    lua_close(m_LuaState);
  }
}

void LuaBehavior::OnUpdate(float_t deltaTime) {
  if (m_FnUpdate && m_FnUpdate->isValid()) {
    HANDLE_FUNCTION_CALL((*m_FnUpdate)(deltaTime));
  }
}

void LuaBehavior::OnEvent(Event& event) {
  // todo start event
}

void* LuaBehavior::GetStatePtr() const {
  return m_LuaState;
}

void LuaBehavior::SetEnabled(bool enabled) {
  if (m_Unset || m_Enabled == enabled)
    return;
  m_Enabled = enabled;

  if (m_Enabled && m_FnOnEnable && m_FnOnEnable->isValid()) {
    HANDLE_FUNCTION_CALL((*m_FnOnEnable)());
  } else if (!m_Enabled && m_FnOnDisable && m_FnOnDisable->isValid()) {
    HANDLE_FUNCTION_CALL((*m_FnOnDisable)());
  }
}

template <typename T>
void LuaExposedVariable<T>::RenderImGui() {
  if (ImGui::DragScalar(PrefixLabel(this->Name.c_str()).c_str(),
                        ImGuiDataType_Double, this->GetPtr())) {
    luabridge::setGlobal(m_Behavior->GetState(), this->GetPtr(),
                         this->Name.c_str());
  }
}

}  // namespace Wiesel