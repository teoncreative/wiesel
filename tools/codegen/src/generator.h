//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "parser.h"

#include <string>
#include <vector>

namespace wiesel::code_gen {

// Generate a .generated.h file for a single parsed header.
// Returns the generated source code as a string.
std::string GenerateReflection(const ParseResult& parse_result,
                               const std::string& original_include_path);

// Generate the w_reflect_all.generated.h that calls all per-file registrations.
// function_names: list of "RegisterFoo" function names from all generated files.
std::string GenerateReflectAll(
    const std::vector<std::string>& generated_includes,
    const std::vector<std::string>& function_names);

}  // namespace wiesel::code_gen
