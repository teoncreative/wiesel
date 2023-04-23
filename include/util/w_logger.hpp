
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

#include "w_pch.hpp"
#include <fmt/core.h>

namespace Wiesel {

// https://github.com/TheCherno/Hazel/blob/e4b0493999206bd2c3ff9d30fa333bcf81f313c8/Hazel/src/Hazel/Debug/Instrumentor.h#L207
// Resolve which function signature macro will be used. Note that this only
// is resolved when the (pre)compiler starts, so the syntax highlighting
// could mark the wrong one in your editor!
#if defined(__GNUC__) || (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || (defined(__ICC) && (__ICC >= 600)) || defined(__ghs__)
#define WIESEL_FUNC_SIG __PRETTY_FUNCTION__
#elif defined(__DMC__) && (__DMC__ >= 0x810)
	WIESEL_FUNC_SIG
		#define WIESEL_FUNC_SIG __PRETTY_FUNCTION__
	#elif (defined(__FUNCSIG__) || (_MSC_VER))
		#define WIESEL_FUNC_SIG __FUNCSIG__
	#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || (defined(__IBMCPP__) && (__IBMCPP__ >= 500))
		#define WIESEL_FUNC_SIG __FUNCTION__
	#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)
		#define WIESEL_FUNC_SIG __FUNC__
	#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
		#define WIESEL_FUNC_SIG __func__
	#elif defined(__cplusplus) && (__cplusplus >= 201103)
		#define WIESEL_FUNC_SIG __func__
	#else
		#define WIESEL_FUNC_SIG "WIESEL_FUNC_SIG unknown!"
#endif

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_ERROR 3
#define LOG_LEVEL_NONE 4

#define LOG_LEVEL LOG_LEVEL_DEBUG

#define PRINTFN(fmsg, func, msg, args...) fmt::print(fmsg, func, fmt::format(msg, ##args))

#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG(msg, args...) PRINTFN("\x1b[44m[debug]\x1b[0m \x1b[35m{}: \x1b[0m{}\x1b[0m\n", WIESEL_FUNC_SIG, msg, ## args)
#else
#define LOG_DEBUG(msg, args...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_INFO
#define LOG_INFO(msg, args...) PRINTFN("\x1b[42m[info ]\x1b[0m \x1b[35m{}: \x1b[0m{}\x1b[0m\n", WIESEL_FUNC_SIG, msg, ## args)
#else
#define LOG_INFO(msg, args...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define LOG_ERROR(msg, args...) PRINTFN("\x1b[41m[error]\x1b[0m \x1b[35m{}: \x1b[31m{}\x1b[0m\n", WIESEL_FUNC_SIG, msg, ## args)
#else
#define LOG_ERROR(msg, args...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_WARN
#define LOG_WARN(msg, args...) PRINTFN("\x1b[41m[warn ]\x1b[0m \x1b[35m{}: \x1b[31m{}\x1b[0m\n", WIESEL_FUNC_SIG, msg, ## args)
#else
#define LOG_WARN(msg, args...)
#endif

}
