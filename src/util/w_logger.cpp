//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "util/w_logger.h"
#include <iostream>

namespace Wiesel {
	// todo use macros and log the caller function
	LogLevel minLogLevel = Debug;

	void SetMinLogLevel(LogLevel level) {
		minLogLevel = level;
	}

	void LogDebug(const std::string& s) {
		if(minLogLevel > Debug)
			return;
		std::cout << "[debug]: " << s << std::endl;
	}

	void LogInfo(const std::string& s) {
		if(minLogLevel > Info)
			return;
		std::cout << "[info ]: " << s << std::endl;
	}

	void LogWarn(const std::string& s) {
		if(minLogLevel > Warn)
			return;
		std::cerr << "[warn ]: " << s << std::endl;
	}

	void LogError(const std::string& s) {
		if(minLogLevel > Error)
			return;
		std::cerr << "[error]: " << s << std::endl;
	}
}
