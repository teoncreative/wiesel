//
// Created by Metehan Gezer on 21.03.2023.
//

#ifndef WIESEL_PROFILER_H
#define WIESEL_PROFILER_H

//#include <intrin.h>
#include <string>
#include <iostream>
#include <chrono>
#include <vector>

namespace Wiesel {
	namespace Profiler {
		struct Data {
			std::string name;
			std::chrono::microseconds time;
			int renderPass;
		};

		template <size_t N>
		struct ChangeResult
		{
			char Data[N];
		};

		void begin(const std::string& section);
		void end(std::ostream& stream);
		void insert(const Data& profileData);
		void setProfilerEnabled(bool value);

		template <size_t N, size_t K>
		constexpr auto cleanupString(const char(&expr)[N], const char(&remove)[K])
		{
			ChangeResult<N> result = {};

			size_t srcIndex = 0;
			size_t dstIndex = 0;
			while (srcIndex < N)
			{
				size_t matchIndex = 0;
				while (matchIndex < K - 1 && srcIndex + matchIndex < N - 1 && expr[srcIndex + matchIndex] == remove[matchIndex])
					matchIndex++;
				if (matchIndex == K - 1)
					srcIndex += matchIndex;
				result.Data[dstIndex++] = expr[srcIndex] == '"' ? '\'' : expr[srcIndex];
				srcIndex++;
			}
			return result;
		}

		class Instance {
		public:
			Instance(const std::string& name);
			Instance(const std::string& name, int renderPass);
			virtual ~Instance();

		private:
			std::chrono::time_point<std::chrono::steady_clock> startTime;
			std::string name;
			int renderPass;
		};

	}
}
#define WIESEL_PROFILE_SCOPE_INTERNAL(name, line) constexpr auto fixedName##line = Wiesel::Profiler::cleanupString(name, "__cdecl ");\
		Wiesel::Profiler::Instance timer##line(fixedName##line.Data);
#define WIESEL_PROFILE_SCOPE_INTERNAL2(name, pass, line) constexpr auto fixedName##line = Wiesel::Profiler::cleanupString(name, "__cdecl ");\
		Wiesel::Profiler::Instance timer##line(fixedName##line.Data, pass);

#define WIESEL_ENABLE_PROFILER() Wiesel::Profiler::setProfilerEnabled(true);
#define WIESEL_DISABLE_PROFILER() Wiesel::Profiler::setProfilerEnabled(false);

#define WIESEL_PROFILE_SCOPE(name) WIESEL_PROFILE_SCOPE_INTERNAL(name, __LINE__)
#define WIESEL_PROFILE_SCOPE_DRAW(name, pass) WIESEL_PROFILE_SCOPE_INTERNAL2(name, pass, __LINE__)
#define WIESEL_PROFILER_START(name) Wiesel::Profiler::begin(name)
#define WIESEL_PROFILER_STOP(stream) Wiesel::Profiler::end(stream)

#endif //WIESEL_PROFILER_H
