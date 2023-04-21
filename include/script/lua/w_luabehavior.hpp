
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
#include <LuaBridge/LuaBridge.h>

#include "behavior/w_behavior.hpp"

namespace Wiesel {

	class LuaBehavior : public IBehavior {
	public:
		LuaBehavior(Entity entity, const std::string& luaFile);
		~LuaBehavior() override;

		void OnUpdate(float_t deltaTime) override;
		void OnEvent(Event& event) override;

	private:
		std::string m_LuaFile;
		lua_State* m_LuaState;
		Scope<luabridge::LuaRef> m_FnStart;
		Scope<luabridge::LuaRef> m_FnUpdate;
	};

}