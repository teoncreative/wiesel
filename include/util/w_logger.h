//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "w_pch.h"

namespace Wiesel {
	typedef enum LogLevel {
		Debug = 0,
		Info = 1,
		Warn = 2,
		Error = 3,
		None = 4
	} LogLevel;

	void SetMinLogLevel(LogLevel level);

	void LogDebug(const std::string& fn, const std::string& s);

	void LogInfo(const std::string& fn, const std::string& s);

	void LogError(const std::string& fn, const std::string& s);

	void LogWarn(const std::string& fn, const std::string& s);


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

#define LOG_DEBUG(msg) Wiesel::LogDebug(WIESEL_FUNC_SIG, msg)
#define LOG_INFO(msg) Wiesel::LogInfo(WIESEL_FUNC_SIG, msg)
#define LOG_ERROR(msg) Wiesel::LogError(WIESEL_FUNC_SIG, msg)
#define LOG_WARN(msg) Wiesel::LogWarn(WIESEL_FUNC_SIG, msg)

}
