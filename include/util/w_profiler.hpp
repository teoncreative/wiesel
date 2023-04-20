//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "w_pch.hpp"
#include "util/w_logger.hpp"

// todo thread safety

namespace Wiesel {
	struct ProfileData {
		std::string Name;
		std::chrono::microseconds Time;
		int RenderPass;
	};

	class ProfilerInstance {
	public:
		explicit ProfilerInstance(const std::string& name);
		ProfilerInstance(const std::string& name, int renderPass);
		virtual ~ProfilerInstance();

	private:
		std::chrono::time_point<std::chrono::steady_clock> m_StartTime;
		std::string m_Name;
		int m_RenderPass;
	};

	class Profiler {
	public:
		static void BeginSection(const std::string& section);
		static void EndSection(std::ostream& stream);
		static void InsertData(const ProfileData& profileData);
		static void SetActive(bool value);
		static bool IsActive();

	private:
		static bool s_Active;
		static std::vector<ProfileData> s_Data;
		static std::string s_CurrentSection;
	};


	template <size_t N>
	struct ChangeResult
	{
		char Data[N];
	};

	template <size_t N, size_t K>
	constexpr auto CleanupOutputString(const char(&expr)[N], const char(&remove)[K])
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
}


#if WIESEL_PROFILE

#define WIESEL_PROFILE_SCOPE_INTERNAL(name, line) constexpr auto fixedName##line = Wiesel::CleanupOutputString(name, "__cdecl ");\
		Wiesel::ProfilerInstance timer##line(fixedName##line.Data);
#define WIESEL_PROFILE_SCOPE_INTERNAL2(name, pass, line) constexpr auto fixedName##line = Wiesel::CleanupOutputString(name, "__cdecl ");\
		Wiesel::ProfilerInstance timer##line(fixedName##line.Data, pass);

#define WIESEL_PROFILE_SCOPE(name) WIESEL_PROFILE_SCOPE_INTERNAL(name, __LINE__)
#define WIESEL_PROFILE_SCOPE_DRAW(name, pass) WIESEL_PROFILE_SCOPE_INTERNAL2(name, pass, __LINE__)
#define WIESEL_PROFILE_BEGIN_SECTION(name) Wiesel::Profiler::BeginSection(name)
#define WIESEL_PROFILE_END_SECTION(stream) Wiesel::Profiler::EndSection(stream)
#define WIESEL_PROFILE_FUNCTION() WIESEL_PROFILE_SCOPE(WIESEL_FUNC_SIG)

#else
#define WIESEL_PROFILE_SCOPE_INTERNAL(name, line)
#define WIESEL_PROFILE_SCOPE_INTERNAL2(name, pass, line)
#define WIESEL_ACTIVATE_PROFILER()
#define WIESEL_DEACTIVATE_PROFILER()
#define WIESEL_PROFILE_SCOPE(name)
#define WIESEL_PROFILE_SCOPE_DRAW(name, pass)
#define WIESEL_PROFILE_BEGIN_SECTION(name)
#define WIESEL_PROFILE_END_SECTION(stream)
#define WIESEL_PROFILE_FUNCTION()

#endif
