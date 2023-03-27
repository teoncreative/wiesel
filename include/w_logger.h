//
// Created by Metehan Gezer on 20.03.2023.
//

#ifndef WIESEL_W_LOGGER_H
#define WIESEL_W_LOGGER_H
#include "string"

namespace wie {
	typedef enum LogLevel {
		LOG_LEVEL_DEBUG = 0,
		LOG_LEVEL_INFO = 1,
		LOG_LEVEL_WARN = 2,
		LOG_LEVEL_ERROR = 3,
		LOG_LEVEL_NONE = 4
	} LogLevel;

	void setLogLevel(LogLevel level);

	void logDebug(const std::string& s);

	void logInfo(const std::string& s);

	void logError(const std::string& s);

	void logWarn(const std::string& s);
}

#endif //WIESEL_W_LOGGER_H
