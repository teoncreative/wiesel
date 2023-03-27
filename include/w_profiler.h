//
// Created by Metehan Gezer on 21.03.2023.
//

#ifndef WIESEL_W_PROFILER_H
#define WIESEL_W_PROFILER_H

//#include <intrin.h>
#include <string>
#include <iostream>
#include <chrono>
#include <vector>

namespace wie {
	struct ProfileData {
		std::string name;
		std::chrono::microseconds time;
		int renderPass;
	};

	template <size_t N>
	struct ChangeResult
	{
		char Data[N];
	};

	void beginProfiler(const std::string& section);
	void endProfiler(std::ostream& stream);
	void insertProfileData(const ProfileData& profileData);
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

	class ProfilerInstance {
	public:
		ProfilerInstance(const std::string& name);
		ProfilerInstance(const std::string& name, int renderPass);
		virtual ~ProfilerInstance();

	private:
		std::chrono::time_point<std::chrono::steady_clock> startTime;
		std::string name;
		int renderPass;
	};
}

#define WIESEL_PROFILE_SCOPE_INTERNAL(name, line) constexpr auto fixedName##line = wie::cleanupString(name, "__cdecl ");\
		wie::ProfilerInstance timer##line(fixedName##line.Data);
#define WIESEL_PROFILE_SCOPE_INTERNAL2(name, pass, line) constexpr auto fixedName##line = wie::cleanupString(name, "__cdecl ");\
		wie::ProfilerInstance timer##line(fixedName##line.Data, pass);

#define WIESEL_ENABLE_PROFILER() wie::setProfilerEnabled(true);
#define WIESEL_DISABLE_PROFILER() wie::setProfilerEnabled(false);

#define WIESEL_PROFILE_SCOPE(name) WIESEL_PROFILE_SCOPE_INTERNAL(name, __LINE__)
#define WIESEL_PROFILE_SCOPE_DRAW(name, pass) WIESEL_PROFILE_SCOPE_INTERNAL2(name, pass, __LINE__)
#define WIESEL_PROFILER_START(name) wie::beginProfiler(name)
#define WIESEL_PROFILER_STOP(stream) wie::endProfiler(stream)

#endif //WIESEL_W_PROFILER_H
