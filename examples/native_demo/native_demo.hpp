
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

#include "w_game_application.hpp"

namespace NativeDemo {

class DemoApplication : public Wiesel::GameApplication {
 public:
  DemoApplication(bool enable_editor);
  ~DemoApplication() override = default;

  void Init() override;

 private:
  bool enable_editor_;
};

}  // namespace LeapLand