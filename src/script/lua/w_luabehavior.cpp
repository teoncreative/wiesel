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
#include "script/lua/w_scriptglue.hpp"
#include "input/w_input.hpp"

namespace Wiesel {

	LuaBehavior::LuaBehavior(Wiesel::Entity entity, const std::string& luaFile) : IBehavior("Lua Script (" + luaFile + ")", entity), m_LuaFile(luaFile) {
		lua_State* luaState = luaL_newstate();

		luaL_openlibs(luaState);

		luabridge::getGlobalNamespace(luaState)
				.addFunctionWithPtr("require", this, &ScriptGlue::StaticRequire);
		int scriptLoadStatus = luaL_dofile(luaState, m_LuaFile.c_str());

		// define error reporter for any Lua error
		ScriptGlue::ReportErrors(luaState, scriptLoadStatus);
		if (scriptLoadStatus != 0) {
			throw std::runtime_error("Script failed to load!");
		}

		// binding functions
		m_FnStart = CreateScope<luabridge::LuaRef>(luabridge::getGlobal(luaState, "Start"));
		m_FnUpdate = CreateScope<luabridge::LuaRef>(luabridge::getGlobal(luaState, "Update"));

		luabridge::getGlobalNamespace(luaState)
				.addFunctionWithPtr("print", this, &ScriptGlue::StaticLogInfo)
				.addFunctionWithPtr("LogInfo", this, &ScriptGlue::StaticLogInfo)
				.addFunctionWithPtr("GetComponent", this, &ScriptGlue::StaticGetComponent)
				.beginNamespace("input")
				.addFunction("GetKey", &InputManager::GetKey)
				.addFunction("GetAxis", &InputManager::GetAxis)
				.addFunction("IsPressed", &InputManager::IsPressed)
				.endNamespace()
				;
		ScriptGlue::ScriptVec3::Link(luaState);
		ScriptGlue::ScriptTransformComponent::Link(luaState);

		(*m_FnStart)(); // todo move this call to scene
	}

	void LuaBehavior::OnUpdate(float_t deltaTime) {
		(*m_FnUpdate)(deltaTime);
	}

	void LuaBehavior::OnEvent(Event& event) {
	}
}