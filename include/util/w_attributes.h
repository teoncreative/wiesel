//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#define USE_NODISCARD
#ifdef USE_NODISCARD
#define WIESEL_NODISCARD [[nodiscard]]
#define WIESEL_NODISCARD_R(reason) [[nodiscard(reason)]]
#else
#define WIESEL_NODISCARD
#define WIESEL_NODISCARD_R
#endif
#undef USE_NODISCARD

#define WIESEL_GETTER_FN WIESEL_NODISCARD_R("Getter functions should not be discarded")
