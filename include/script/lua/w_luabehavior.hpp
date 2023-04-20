
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
#include <LuaBridge/RefCountedPtr.h>
#include "behavior/w_behavior.hpp"

namespace Wiesel {

	class LuaBehavior : public IBehavior {
	public:
		LuaBehavior(Entity entity, const std::string& luaFile);
		virtual ~LuaBehavior() { }

		void OnUpdate(float_t deltaTime);
		void OnEvent(Event& event);

	private:
		std::string m_LuaFile;
		Scope<luabridge::LuaRef> m_FnStart;
		Scope<luabridge::LuaRef> m_FnUpdate;
	};

}