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

	void LogDebug(const std::string& s);

	void LogInfo(const std::string& s);

	void LogError(const std::string& s);

	void LogWarn(const std::string& s);
}
