
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

		std::function<void(const char*, lua_State*)> require = +[](const char* name, lua_State* state) {
			ScriptGlue::RegisterModule(name, state);
		};
		std::function<luabridge::LuaRef(const char*, lua_State*)> getComponent = [this](const char* name, lua_State* state) -> luabridge::LuaRef {
			return ScriptGlue::GetComponentGetter(name)(this->GetEntity(), state);
		};
		std::function<void(const char*)> log = [this](const char* msg) {
			LOG_INFO("Script {}: {}", this->GetComponent<TagComponent>().Tag, msg);
		};

		luabridge::getGlobalNamespace(luaState)
				.addFunction("require", require);

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
				.addFunction("print", log);

		luabridge::getGlobalNamespace(luaState)
				.addFunction("LogInfo", log)
				.addFunction("GetComponent", getComponent);

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