//
// Created by Metehan Gezer on 20.03.2023.
//

#ifndef WIESEL_LOGGER_H
#define WIESEL_LOGGER_H
#include "string"

namespace Wiesel {
	void logDebug(const std::string& s);

	void logInfo(const std::string& s);

	void logError(const std::string& s);

	void logWarn(const std::string& s);
}

#endif //WIESEL_LOGGER_H
