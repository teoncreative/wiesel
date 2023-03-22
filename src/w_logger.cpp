//
// Created by Metehan Gezer on 20.03.2023.
//

#include "w_logger.h"
#include <iostream>

namespace wge {
	LogLevel minLogLevel = LOG_LEVEL_DEBUG;
	void setLogLevel(LogLevel level) {
		minLogLevel = level;
	}

	void logDebug(const std::string& s) {
		if (minLogLevel > LOG_LEVEL_DEBUG)
			return;
		std::cout << "[debug]: " << s << std::endl;
	}

	void logInfo(const std::string& s) {
		if (minLogLevel > LOG_LEVEL_INFO)
			return;
		std::cout << "[info ]: " << s << std::endl;
	}

	void logWarn(const std::string& s) {
		if (minLogLevel > LOG_LEVEL_WARN)
			return;
		std::cerr << "[warn ]: " << s << std::endl;
	}

	void logError(const std::string& s) {
		if (minLogLevel > LOG_LEVEL_ERROR)
			return;
		std::cerr << "[error]: " << s << std::endl;
	}
}
