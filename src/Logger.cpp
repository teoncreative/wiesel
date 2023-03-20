//
// Created by Metehan Gezer on 20.03.2023.
//

#include "Logger.h"
#include <iostream>

namespace Wiesel {
	void logDebug(const std::string& s) {
		std::cout << "[debug]: " << s << std::endl;
	}

	void logInfo(const std::string& s) {
		std::cout << "[info]: " << s << std::endl;
	}

	void logError(const std::string& s) {
		std::cerr << "[error]: " << s << std::endl;
	}

	void logWarn(const std::string& s) {
		std::cerr << "[warn]: " << s << std::endl;
	}
}
