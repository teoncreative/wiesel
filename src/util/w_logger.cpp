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

/*
+---------+------------+------------+
|  color  | foreground | background |
|         |    code    |    code    |
+---------+------------+------------+
| black   |     30     |     40     |
| red     |     31     |     41     |
| green   |     32     |     42     |
| yellow  |     33     |     43     |
| blue    |     34     |     44     |
| magenta |     35     |     45     |
| cyan    |     36     |     46     |
| white   |     37     |     47     |
+---------+------------+------------+
reset \x1b[0m
 */

	void LogDebug(const std::string& fn, const std::string& s) {
		if(minLogLevel > Debug)
			return;
		std::cout << "\x1b[44m[debug]\x1b[0m \x1b[35m" << fn << ": \x1b[0m" << s << "\x1b[0m" <<std::endl;
	}

	void LogInfo(const std::string& fn, const std::string& s) {
		if(minLogLevel > Info)
			return;
		std::cout << "\x1b[42m[info ]\x1b[0m \x1b[35m" << fn << ": \x1b[0m" << s << "\x1b[0m" <<std::endl;
	}

	void LogWarn(const std::string& fn, const std::string& s) {
		if(minLogLevel > Warn)
			return;
		std::cout << "\x1b[41m[warn ]\x1b[0m \x1b[35m" << fn << ": \x1b[31m" << s << "\x1b[0m" <<std::endl;
	}

	void LogError(const std::string& fn, const std::string& s) {
		if(minLogLevel > Error)
			return;
		std::cout << "\x1b[41m[error]\x1b[0m \x1b[35m" << fn << ": \x1b[31m" << s << "\x1b[0m" <<std::endl;
	}
}
