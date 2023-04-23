
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
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
// im considering switching to sol3
#include <LuaBridge/LuaBridge.h>
#include <LuaBridge/detail/LuaHelpers.h>
#include <LuaBridge/detail/FuncTraits.h>

#include "behavior/w_behavior.hpp"

namespace Wiesel {
	template<typename T>
	struct ExposedVariable {
		ExposedVariable(T value, const std::string& name) : Value(value), Name(name) { }
		~ExposedVariable() = default;

		T Value;
		std::string Name;

		void Set(T v) { Value = v; }
		const T& Get() const { return Value; }
		T* GetPtr() { return &Value; }

		void Push(lua_State* state) {
			luabridge::setGlobal(state, &Value, Name.c_str());
		}
	};

	class LuaBehavior : public IBehavior {
	public:
		LuaBehavior(Entity entity, const std::string& file);
		~LuaBehavior() override;

		void OnUpdate(float_t deltaTime) override;
		void OnEvent(Event& event) override;

		void SetEnabled(bool enabled) override;

		WIESEL_GETTER_FN const std::vector<Reference<ExposedVariable<double>>>& GetExposedDoubles() const {
			return m_ExposedDoubles;
		}

		lua_State* GetState() {
			return m_LuaState;
		}
	private:
		lua_State* m_LuaState;
		Scope<luabridge::LuaRef> m_FnOnLoad;
		Scope<luabridge::LuaRef> m_FnOnEnable;
		Scope<luabridge::LuaRef> m_FnOnDisable;
		Scope<luabridge::LuaRef> m_FnUpdate;
		Scope<luabridge::LuaRef> m_FnStart;
		// todo other types like vec3!
		std::vector<Reference<ExposedVariable<double>>> m_ExposedDoubles;
	};

}