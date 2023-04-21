
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "script/lua/w_scriptglue.hpp"
#include "util/w_logger.hpp"
#include "util/w_utils.hpp"
#include "rendering/w_mesh.hpp"
#include "scene/w_lights.hpp"

namespace Wiesel::ScriptGlue {

	void ReportErrors(lua_State *luaState, int status) {
		if (status == 0) {
			return;
		}
		LOG_ERROR("[SCRIPT ERROR] ", lua_tostring(luaState, -1));
		// remove error message from Lua state
		lua_pop(luaState, 1);
	}

	void ScriptVec3::Link(lua_State* L) {
		luabridge::getGlobalNamespace(L)
				.beginClass<glm::vec3>("RVec3") // read only vec3
					.addProperty("x", &glm::vec3::x, false)
					.addProperty("y", &glm::vec3::y, false)
					.addProperty("z", &glm::vec3::z, false)
				.endClass()
				.beginClass<ScriptVec3>("Vec3") // modifiable vec3
					.addProperty("x", &ScriptVec3::GetX, &ScriptVec3::SetX)
					.addProperty("y", &ScriptVec3::GetY, &ScriptVec3::SetY)
					.addProperty("z", &ScriptVec3::GetZ, &ScriptVec3::SetZ)
				.endClass();
	}

	void ScriptTransformComponent::Link(lua_State* L) {
		// todo scene class
		// todo entity class
		luabridge::getGlobalNamespace(L)
				.beginClass<ScriptTransformComponent>("TransformComponent")
					.addProperty("position", &ScriptTransformComponent::GetPosition)
					.addProperty("rotation", &ScriptTransformComponent::GetRotation)
					.addProperty("scale", &ScriptTransformComponent::GetScale)
					.addFunction("Move", &ScriptTransformComponent::Move)
					.addFunction("SetPosition", &ScriptTransformComponent::SetPosition)
					.addFunction("Rotate", &ScriptTransformComponent::Rotate)
					.addFunction("SetRotation", &ScriptTransformComponent::SetRotation)
					.addFunction("Resize", &ScriptTransformComponent::Resize)
					.addFunction("SetScale", &ScriptTransformComponent::SetScale)
				.endClass();
	}

	std::map<std::string, std::function<luabridge::LuaRef(Entity, lua_State*)>> s_GetterFn = {};

	template<class ScriptWrapper, class Component>
	void AddGetter(const std::string& name) {
		s_GetterFn[name] = [](Entity entity, lua_State* state) -> decltype(auto) {
			auto& transform = entity.GetComponent<Component>();
			luabridge::setGlobal(state, luabridge::RefCountedPtr<ScriptWrapper>(new ScriptWrapper(transform)), "__currentcomponent");
			return luabridge::getGlobal(state, "__currentcomponent");
		};
	}

	void GenerateComponents() {
		AddGetter<ScriptTransformComponent, TransformComponent>("TransformComponent");
	}

	luabridge::LuaRef StaticGetComponent(const std::string& str, lua_State* state) {
		LuaBehavior* pThis = luabridge::getMemberPtr<LuaBehavior>(state);
		return s_GetterFn[str](pThis->GetEntity(), state);
	}

	void StaticLogInfo(const char* msg, lua_State* state) {
		LuaBehavior* pThis = luabridge::getMemberPtr<LuaBehavior>(state);

		LOG_INFO("Script {}: {}", pThis->GetComponent<TagComponent>().Tag, msg);
	}

	void StaticRequire(const std::string& package, lua_State* state) {
		LOG_DEBUG("StaticRequire called from script but not implemented yet!");
		// todo
	}

}