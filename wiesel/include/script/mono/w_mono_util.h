//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

//
// Created by Metehan Gezer on 28.03.2026.
//

#ifndef WIESEL_PARENT_W_MONO_UTIL_H
#define WIESEL_PARENT_W_MONO_UTIL_H

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/class.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/object.h>

#include <future>
#include <typeindex>

#include "mono_compiler.h"
#include "mono_wrappers.h"

namespace wiesel {

// Wraps mono_runtime_invoke with exception handling.
// Logs to both terminal and developer console.
// Returns the result. Sets *had_exception to true if an exception occurred.
MonoObject* InvokeSafe(MonoMethod* method, MonoObject* obj, void** args,
                       bool* had_exception = nullptr);

}  // namespace wiesel

#endif  //WIESEL_PARENT_W_MONO_UTIL_H
