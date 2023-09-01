
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

#include "behavior/w_behavior.hpp"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/class.h>

namespace Wiesel {
class MonoBehavior : public IBehavior {
 public:
  MonoBehavior(Entity entity, const std::string& sourceFile);
  ~MonoBehavior() override;

  void OnUpdate(float_t deltaTime) override;
  void OnEvent(Event& event) override;

  void SetEnabled(bool enabled) override;

 private:
  MonoDomain* m_Domain;
  MonoClass* m_BehaviorClass;
  MonoAssembly* m_Assembly;
  MonoImage* m_Image;
  MonoMethod* m_MethodStart;
  MonoMethod* m_MethodUpdate;
  MonoObject * m_BehaviorObject;
  bool m_CanEnable = false;
  bool m_IsEnabled = true;
};

}  // namespace Wiesel