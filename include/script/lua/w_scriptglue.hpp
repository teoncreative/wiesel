
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

#include "w_luabehavior.hpp"

namespace Wiesel::ScriptGlue {
	class ScriptVec3 {
	public:
		ScriptVec3(glm::vec3& backingVec3, bool* changedPtr) : m_BackingVec3(backingVec3), m_ChangedPtr(changedPtr) {}

		float GetX() const { return m_BackingVec3.x; }
		float GetY() const { return m_BackingVec3.y; }
		float GetZ() const { return m_BackingVec3.z; }
		void SetX(float v) { m_BackingVec3.x = v; if (m_ChangedPtr) *m_ChangedPtr = true; }
		void SetY(float v) { m_BackingVec3.y = v; if (m_ChangedPtr) *m_ChangedPtr = true; }
		void SetZ(float v) { m_BackingVec3.z = v; if (m_ChangedPtr) *m_ChangedPtr = true; }

		static void Link(lua_State* state);

	private:
		glm::vec3& m_BackingVec3;
		bool* m_ChangedPtr;

	};

	struct ScriptTransformComponent {
		ScriptTransformComponent(TransformComponent& transform) : Transform(transform), Position({transform.Position, &transform.IsChanged}), Rotation({transform.Rotation, &transform.IsChanged}), Scale({transform.Scale, &transform.IsChanged}) { }
		~ScriptTransformComponent() = default;

		TransformComponent& Transform;
		ScriptVec3 Position;
		ScriptVec3 Rotation;
		ScriptVec3 Scale;

		glm::vec3 GetForward() { return {Transform.GetForward()}; }
		glm::vec3 GetBackward() { return {Transform.GetBackward()}; }
		glm::vec3 GetLeft() { return {Transform.GetLeft()}; }
		glm::vec3 GetRight() { return {Transform.GetRight()}; }
		glm::vec3 GetUp() { return {Transform.GetUp()}; }
		glm::vec3 GetDown() { return {Transform.GetDown()}; }

		WIESEL_GETTER_FN ScriptVec3 GetPosition() const { return Position; }
		WIESEL_GETTER_FN ScriptVec3 GetRotation() const { return Rotation; }
		WIESEL_GETTER_FN ScriptVec3 GetScale() const { return Scale; }

		void Move(float dx, float dy, float dz) { Transform.Move(dx, dy, dz); }
		void Move2(const glm::vec3& delta) { Move(delta.x, delta.y, delta.z); }
		void SetPosition(float x, float y, float z) { Transform.SetPosition(x, y, z); }
		void SetPosition2(glm::vec3& pos) { SetPosition(pos.x, pos.y, pos.z); }

		void Rotate(float dx, float dy, float dz) { Transform.Move(dx, dy, dz); }
		void Rotate2(const glm::vec3& delta) { Rotate(delta.x, delta.y, delta.z); }
		void SetRotation(float x, float y, float z) { Transform.SetRotation(x, y, z); }
		void SetRotation2(glm::vec3& rot) { SetRotation(rot.x, rot.y, rot.z); }

		void Resize(float dx, float dy, float dz) { Transform.Resize(dx, dy, dz); }
		void Resize2(const glm::vec3& delta) { Resize(delta.x, delta.y, delta.z); }
		void SetScale(float x, float y, float z) { Transform.SetScale(x, y, z); }
		void SetScale2(const glm::vec3& scale) { SetScale(scale.x, scale.y, scale.z); }


		static void Link(lua_State* state);
	};

	void ReportErrors(lua_State *luaState, int status);

	using LuaComponentGetFn = std::function<luabridge::LuaRef(Entity, lua_State*)>;
	using LuaModuleLoaderFn = std::function<void(lua_State*)>;

	LuaComponentGetFn GetComponentGetter(const char* name);

	template<class T, class B>
	void AddGetter(const std::string& name);

	void GenerateComponents();

	void GenerateModules();

	void RegisterModule(const char* name, lua_State* state);

}