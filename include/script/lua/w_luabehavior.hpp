
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

extern "C" {
#define MAKE_LIB
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
// im considering switching to sol3
#include <LuaBridge/LuaBridge.h>
#include <LuaBridge/detail/FuncTraits.h>
#include <LuaBridge/detail/LuaHelpers.h>
#include <LuaBridge/detail/LuaRef.h>

#include "behavior/w_behavior.hpp"

namespace Wiesel {
  class LuaBehavior : public IBehavior {
  public:
    LuaBehavior(Entity entity, const std::string& file);
    ~LuaBehavior() override;

    void OnUpdate(float_t deltaTime) override;
    void OnEvent(Event& event) override;

    WIESEL_GETTER_FN void* GetStatePtr() const override;
    WIESEL_GETTER_FN lua_State* GetState() {
      return m_LuaState;
    }

    void SetEnabled(bool enabled) override;

  private:
    lua_State* m_LuaState;
    Scope<luabridge::LuaRef> m_FnOnLoad;
    Scope<luabridge::LuaRef> m_FnOnEnable;
    Scope<luabridge::LuaRef> m_FnOnDisable;
    Scope<luabridge::LuaRef> m_FnUpdate;
    Scope<luabridge::LuaRef> m_FnStart;
  };

  template<typename T>
  struct LuaExposedVariable : public ExposedVariable<T> {
    LuaExposedVariable(T value, const std::string& name, LuaBehavior* behavior) : ExposedVariable<T>(value, name),
                                                                                  m_Behavior(behavior) {}
    ~LuaExposedVariable() = default;

    void RenderImGui() override;

    LuaBehavior* m_Behavior;
  };
}
