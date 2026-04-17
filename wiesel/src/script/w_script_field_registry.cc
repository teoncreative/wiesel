
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "script/w_script_field_registry.h"

#include "mono_wrappers.h"
#include "scene/w_components.h"
#include "scene/w_scene.h"
#include "script/w_scriptmanager.h"
#include "util/w_logger.h"
#include "w_engine.h"

namespace wiesel {

std::unordered_map<std::string, ScriptFieldTypeDesc>&
ScriptFieldTypeRegistry::Registry() {
  static std::unordered_map<std::string, ScriptFieldTypeDesc> registry;
  return registry;
}

void ScriptFieldTypeRegistry::Register(const std::string& mono_type_name,
                                       ScriptFieldTypeDesc desc) {
  Registry()[mono_type_name] = std::move(desc);
}

ScriptFieldTypeDesc* ScriptFieldTypeRegistry::Find(
    const std::string& mono_type_name) {
  auto& reg = Registry();
  auto it = reg.find(mono_type_name);
  if (it != reg.end()) {
    return &it->second;
  }
  return nullptr;
}

bool ScriptFieldTypeRegistry::SerializeField(const std::string& type_name,
                                             MonoObject* object,
                                             MonoClassField* field,
                                             nlohmann::json& out) {
  auto* desc = Find(type_name);
  if (!desc || !desc->Serialize) {
    return false;
  }
  out = desc->Serialize(object, field);
  return true;
}

bool ScriptFieldTypeRegistry::DeserializeField(const std::string& type_name,
                                               MonoObject* object,
                                               MonoClassField* field,
                                               const nlohmann::json& value,
                                               Scene* scene) {
  auto* desc = Find(type_name);
  if (!desc || !desc->Deserialize) {
    return false;
  }
  desc->Deserialize(object, field, value, scene);
  return true;
}

bool ScriptFieldTypeRegistry::SerializeValue(const std::string& type_name,
                                             MonoObject* value,
                                             nlohmann::json& out) {
  auto* desc = Find(type_name);
  if (!desc || !desc->SerializeValue) {
    return false;
  }
  out = desc->SerializeValue(value);
  return true;
}

MonoObject* ScriptFieldTypeRegistry::DeserializeValue(
    const std::string& type_name, const nlohmann::json& value, Scene* scene) {
  auto* desc = Find(type_name);
  if (!desc || !desc->DeserializeValue) {
    return nullptr;
  }
  return desc->DeserializeValue(value, scene);
}

void InitializeScriptFieldTypes() {
  // int
  ScriptFieldTypeRegistry::Register("System.Int32", {
      .Serialize = [](MonoObject* obj, MonoClassField* field) -> nlohmann::json {
        int32_t val = 0;
        mono_field_get_value(obj, field, &val);
        return val;
      },
      .Deserialize = [](MonoObject* obj, MonoClassField* field,
                         const nlohmann::json& val, Scene*) {
        if (val.is_number_integer()) {
          int32_t v = val.get<int32_t>();
          mono_field_set_value(obj, field, &v);
        }
      },
      .Render = nullptr,
      .SerializeValue = [](MonoObject* boxed) -> nlohmann::json {
        return *(int32_t*)mono_object_unbox(boxed);
      },
      .DeserializeValue = [](const nlohmann::json& val,
                              Scene*) -> MonoObject* {
        int32_t v = val.is_number() ? val.get<int32_t>() : 0;
        return mono_value_box(Engine::script_manager().app_domain(),
                              mono_get_int32_class(), &v);
      },
  });

  // float
  ScriptFieldTypeRegistry::Register("System.Single", {
      .Serialize = [](MonoObject* obj, MonoClassField* field) -> nlohmann::json {
        float val = 0.0f;
        mono_field_get_value(obj, field, &val);
        return val;
      },
      .Deserialize = [](MonoObject* obj, MonoClassField* field,
                         const nlohmann::json& val, Scene*) {
        if (val.is_number()) {
          float v = val.get<float>();
          mono_field_set_value(obj, field, &v);
        }
      },
      .Render = nullptr,
      .SerializeValue = [](MonoObject* boxed) -> nlohmann::json {
        return *(float*)mono_object_unbox(boxed);
      },
      .DeserializeValue = [](const nlohmann::json& val,
                              Scene*) -> MonoObject* {
        float v = val.is_number() ? val.get<float>() : 0.0f;
        return mono_value_box(Engine::script_manager().app_domain(),
                              mono_get_single_class(), &v);
      },
  });

  // double
  ScriptFieldTypeRegistry::Register("System.Double", {
      .Serialize = [](MonoObject* obj, MonoClassField* field) -> nlohmann::json {
        double val = 0.0;
        mono_field_get_value(obj, field, &val);
        return val;
      },
      .Deserialize = [](MonoObject* obj, MonoClassField* field,
                         const nlohmann::json& val, Scene*) {
        if (val.is_number()) {
          double v = val.get<double>();
          mono_field_set_value(obj, field, &v);
        }
      },
      .Render = nullptr,
      .SerializeValue = [](MonoObject* boxed) -> nlohmann::json {
        return *(double*)mono_object_unbox(boxed);
      },
      .DeserializeValue = [](const nlohmann::json& val,
                              Scene*) -> MonoObject* {
        double v = val.is_number() ? val.get<double>() : 0.0;
        return mono_value_box(Engine::script_manager().app_domain(),
                              mono_get_double_class(), &v);
      },
  });

  // bool
  ScriptFieldTypeRegistry::Register("System.Boolean", {
      .Serialize = [](MonoObject* obj, MonoClassField* field) -> nlohmann::json {
        MonoBoolean val = 0;
        mono_field_get_value(obj, field, &val);
        return static_cast<bool>(val);
      },
      .Deserialize = [](MonoObject* obj, MonoClassField* field,
                         const nlohmann::json& val, Scene*) {
        if (val.is_boolean()) {
          MonoBoolean v = val.get<bool>() ? 1 : 0;
          mono_field_set_value(obj, field, &v);
        }
      },
      .Render = nullptr,
      .SerializeValue = [](MonoObject* boxed) -> nlohmann::json {
        return static_cast<bool>(*(MonoBoolean*)mono_object_unbox(boxed));
      },
      .DeserializeValue = [](const nlohmann::json& val,
                              Scene*) -> MonoObject* {
        MonoBoolean v = (val.is_boolean() && val.get<bool>()) ? 1 : 0;
        return mono_value_box(Engine::script_manager().app_domain(),
                              mono_get_boolean_class(), &v);
      },
  });

  // string
  ScriptFieldTypeRegistry::Register("System.String", {
      .Serialize = [](MonoObject* obj, MonoClassField* field) -> nlohmann::json {
        MonoString* val = nullptr;
        mono_field_get_value(obj, field, &val);
        if (val) {
          char* cstr = mono_string_to_utf8(val);
          std::string result(cstr);
          mono_free(cstr);
          return result;
        }
        return "";
      },
      .Deserialize = [](MonoObject* obj, MonoClassField* field,
                         const nlohmann::json& val, Scene*) {
        if (val.is_string()) {
          MonoString* str = mono_string_new(
              Engine::script_manager().app_domain(),
              val.get<std::string>().c_str());
          mono_field_set_value(obj, field, str);
        }
      },
      .Render = nullptr,
      .SerializeValue = [](MonoObject* value) -> nlohmann::json {
        if (!value) return "";
        char* cstr = mono_string_to_utf8((MonoString*)value);
        std::string result(cstr);
        mono_free(cstr);
        return result;
      },
      .DeserializeValue = [](const nlohmann::json& val,
                              Scene*) -> MonoObject* {
        std::string str = val.is_string() ? val.get<std::string>() : "";
        return (MonoObject*)mono_string_new(
            Engine::script_manager().app_domain(), str.c_str());
      },
  });

  // Prefab
  ScriptFieldTypeRegistry::Register("WieselEngine.Prefab", {
      .Serialize = [](MonoObject* obj, MonoClassField* field) -> nlohmann::json {
        MonoObject* prefab_obj = nullptr;
        mono_field_get_value(obj, field, &prefab_obj);
        if (!prefab_obj) {
          return nullptr;
        }
        return ScriptFieldTypeRegistry::Find("WieselEngine.Prefab")
            ->SerializeValue(prefab_obj);
      },
      .Deserialize = [](MonoObject* obj, MonoClassField* field,
                         const nlohmann::json& val, Scene* scene) {
        MonoObject* prefab =
            ScriptFieldTypeRegistry::DeserializeValue(
                "WieselEngine.Prefab", val, scene);
        if (prefab) {
          mono_field_set_value(obj, field, prefab);
        }
      },
      .Render = nullptr,
      .SerializeValue = [](MonoObject* prefab_obj) -> nlohmann::json {
        if (!prefab_obj) return nullptr;
        MonoClassField* handle_field = mono_class_get_field_from_name(
            Engine::script_manager().prefab_class(), "handle");
        if (!handle_field) return nullptr;
        MonoString* handle_str = nullptr;
        mono_field_get_value(prefab_obj, handle_field, &handle_str);
        if (!handle_str) return nullptr;
        char* cstr = mono_string_to_utf8(handle_str);
        nlohmann::json pj;
        pj["type"] = "Prefab";
        pj["handle"] = cstr;
        mono_free(cstr);
        return pj;
      },
      .DeserializeValue = [](const nlohmann::json& val,
                              Scene*) -> MonoObject* {
        if (!val.is_object()) return nullptr;
        std::string handle_str = val.value("handle", "");
        if (handle_str.empty()) return nullptr;
        MonoObject* prefab = mono_object_new(
            Engine::script_manager().app_domain(),
            Engine::script_manager().prefab_class());
        mono_runtime_object_init(prefab);
        MonoClassField* handle_field = mono_class_get_field_from_name(
            Engine::script_manager().prefab_class(), "handle");
        if (handle_field) {
          MonoString* mono_val = mono_string_new(
              Engine::script_manager().app_domain(), handle_str.c_str());
          mono_field_set_value(prefab, handle_field, mono_val);
        }
        return prefab;
      },
  });

  // Entity reference
  ScriptFieldTypeRegistry::Register("WieselEngine.Entity", {
      .Serialize = [](MonoObject* obj, MonoClassField* field) -> nlohmann::json {
        MonoObject* entity_obj = nullptr;
        mono_field_get_value(obj, field, &entity_obj);
        return ScriptFieldTypeRegistry::Find("WieselEngine.Entity")
            ->SerializeValue(entity_obj);
      },
      .Deserialize = [](MonoObject* obj, MonoClassField* field,
                         const nlohmann::json& val, Scene* scene) {
        MonoObject* entity_obj = ScriptFieldTypeRegistry::DeserializeValue(
            "WieselEngine.Entity", val, scene);
        if (entity_obj) {
          mono_field_set_value(obj, field, entity_obj);
        }
      },
      .Render = nullptr,
      .SerializeValue = [](MonoObject* entity_obj) -> nlohmann::json {
        if (!entity_obj) return nullptr;
        MonoClassField* id_field = mono_class_get_field_from_name(
            Engine::script_manager().entity_class(), "entityId");
        MonoClassField* scene_field = mono_class_get_field_from_name(
            Engine::script_manager().entity_class(), "scenePtr");
        if (!id_field || !scene_field) return nullptr;
        uint64_t id_val = 0;
        uint64_t scene_ptr = 0;
        mono_field_get_value(entity_obj, id_field, &id_val);
        mono_field_get_value(entity_obj, scene_field, &scene_ptr);
        auto* scene = reinterpret_cast<Scene*>(scene_ptr);
        entt::entity ent = static_cast<entt::entity>(id_val);
        if (scene && scene->HasEntity(ent) &&
            scene->HasComponent<IdComponent>(ent)) {
          nlohmann::json ej;
          ej["type"] = "Entity";
          ej["uuid"] = scene->GetComponent<IdComponent>(ent).Id.ToString();
          return ej;
        }
        return nullptr;
      },
      .DeserializeValue = [](const nlohmann::json& val,
                              Scene* scene) -> MonoObject* {
        if (!val.is_object() || !scene) return nullptr;
        std::string uuid_str = val.value("uuid", "");
        if (uuid_str.empty()) return nullptr;
        urkern::UUID uuid = urkern::UUID::FromString(uuid_str);
        entt::entity ent = scene->FindEntityByUUID(uuid);
        if (ent == entt::null) return nullptr;
        MonoObject* entity_obj = mono_object_new(
            Engine::script_manager().app_domain(),
            Engine::script_manager().entity_class());
        MonoMethod* ctor = mono_class_get_method_from_name(
            Engine::script_manager().entity_class(), ".ctor", 2);
        uint64_t scene_ptr = reinterpret_cast<uint64_t>(scene);
        uint64_t entity_id = static_cast<uint64_t>(ent);
        void* args[2] = {&scene_ptr, &entity_id};
        mono_runtime_invoke(ctor, entity_obj, args, nullptr);
        return entity_obj;
      },
  });

  // AudioClip
  ScriptFieldTypeRegistry::Register("WieselEngine.AudioClip", {
      .Serialize = [](MonoObject* obj, MonoClassField* field) -> nlohmann::json {
        MonoObject* clip_obj = nullptr;
        mono_field_get_value(obj, field, &clip_obj);
        return ScriptFieldTypeRegistry::Find("WieselEngine.AudioClip")
            ->SerializeValue(clip_obj);
      },
      .Deserialize = [](MonoObject* obj, MonoClassField* field,
                         const nlohmann::json& val, Scene* scene) {
        MonoObject* clip = ScriptFieldTypeRegistry::DeserializeValue(
            "WieselEngine.AudioClip", val, scene);
        if (clip) {
          mono_field_set_value(obj, field, clip);
        }
      },
      .Render = nullptr,
      .SerializeValue = [](MonoObject* clip_obj) -> nlohmann::json {
        if (!clip_obj) return nullptr;
        MonoClassField* handle_field = mono_class_get_field_from_name(
            mono_object_get_class(clip_obj), "handle");
        if (!handle_field) return nullptr;
        MonoString* handle_str = nullptr;
        mono_field_get_value(clip_obj, handle_field, &handle_str);
        if (!handle_str) return nullptr;
        char* cstr = mono_string_to_utf8(handle_str);
        nlohmann::json aj;
        aj["type"] = "AudioClip";
        aj["handle"] = cstr;
        mono_free(cstr);
        return aj;
      },
      .DeserializeValue = [](const nlohmann::json& val,
                              Scene*) -> MonoObject* {
        if (!val.is_object()) return nullptr;
        std::string handle_str = val.value("handle", "");
        if (handle_str.empty()) return nullptr;
        MonoClass* clip_class = Engine::script_manager().audio_clip_class();
        if (!clip_class) return nullptr;
        MonoObject* clip = mono_object_new(
            Engine::script_manager().app_domain(), clip_class);
        mono_runtime_object_init(clip);
        MonoClassField* handle_field = mono_class_get_field_from_name(
            clip_class, "handle");
        if (handle_field) {
          MonoString* mono_val = mono_string_new(
              Engine::script_manager().app_domain(), handle_str.c_str());
          mono_field_set_value(clip, handle_field, mono_val);
        }
        return clip;
      },
  });

  // Font
  ScriptFieldTypeRegistry::Register("WieselEngine.Font", {
      .Serialize = [](MonoObject* obj, MonoClassField* field) -> nlohmann::json {
        MonoObject* font_obj = nullptr;
        mono_field_get_value(obj, field, &font_obj);
        return ScriptFieldTypeRegistry::Find("WieselEngine.Font")
            ->SerializeValue(font_obj);
      },
      .Deserialize = [](MonoObject* obj, MonoClassField* field,
                         const nlohmann::json& val, Scene* scene) {
        MonoObject* font = ScriptFieldTypeRegistry::DeserializeValue(
            "WieselEngine.Font", val, scene);
        if (font) {
          mono_field_set_value(obj, field, font);
        }
      },
      .Render = nullptr,
      .SerializeValue = [](MonoObject* font_obj) -> nlohmann::json {
        if (!font_obj) return nullptr;
        MonoClassField* handle_field = mono_class_get_field_from_name(
            Engine::script_manager().font_class(), "handle");
        if (!handle_field) return nullptr;
        MonoString* handle_str = nullptr;
        mono_field_get_value(font_obj, handle_field, &handle_str);
        if (!handle_str) return nullptr;
        char* cstr = mono_string_to_utf8(handle_str);
        nlohmann::json fj;
        fj["type"] = "Font";
        fj["handle"] = cstr;
        mono_free(cstr);
        return fj;
      },
      .DeserializeValue = [](const nlohmann::json& val,
                              Scene*) -> MonoObject* {
        if (!val.is_object()) return nullptr;
        std::string handle_str = val.value("handle", "");
        if (handle_str.empty()) return nullptr;
        MonoClass* font_class = Engine::script_manager().font_class();
        if (!font_class) return nullptr;
        MonoObject* font = mono_object_new(
            Engine::script_manager().app_domain(), font_class);
        mono_runtime_object_init(font);
        MonoClassField* handle_field = mono_class_get_field_from_name(
            font_class, "handle");
        if (handle_field) {
          MonoString* mono_val = mono_string_new(
              Engine::script_manager().app_domain(), handle_str.c_str());
          mono_field_set_value(font, handle_field, mono_val);
        }
        return font;
      },
  });

  LOG_INFO("Script field type registry initialized");
}

}  // namespace wiesel
