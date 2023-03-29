//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "util/w_profiler.h"
#include <sstream>

namespace Wiesel {
	bool Profiler::s_Active = false;
	std::vector<ProfileData> Profiler::s_Data;
	std::string Profiler::s_CurrentSection;

	ProfilerInstance::ProfilerInstance(const std::string& name) : ProfilerInstance(name, -1) {
	}

	ProfilerInstance::ProfilerInstance(const std::string& name, int renderPass) {
		this->m_Name = name;
		this->m_StartTime = std::chrono::steady_clock::now();
		this->m_RenderPass = renderPass;
	}

	ProfilerInstance::~ProfilerInstance() {
		auto now = std::chrono::steady_clock::now();
		auto elapsed = now - this->m_StartTime;
		auto elapsedMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
		ProfileData profileData;
		profileData.Name = m_Name;
		profileData.Time = elapsedMicroseconds;
		profileData.RenderPass = m_RenderPass;
		Profiler::InsertData(profileData);
	}

	void Profiler::BeginSection(const std::string& section) {
		if (s_Active) {
			throw std::runtime_error("Cannot begin section while another one was still active!");
		}
		s_Active = true;
		s_Data.clear();
		s_CurrentSection = section;
	}

	void Profiler::EndSection(std::ostream& stream) {
		if(!s_Active) {
			return;
		}
		long long sum = 0;
		stream << "[profiler] Section: " << s_CurrentSection << "\n";
		for(auto& v: s_Data) {
			stream << "[profiler] " << v.Name;
			if(v.RenderPass >= 0) {
				stream << "(pass " << v.RenderPass << ")";
			}
			long long count = v.Time.count();
			stream << ": " << count << "us.\n";
			sum += count;
		}
		stream << "[profiler] Total Time: " << sum << "us\n";
		s_Active = false;
	}

	void Profiler::InsertData(const ProfileData& profileData) {
		s_Data.push_back(profileData);
	}

	void Profiler::SetActive(bool value) {
		s_Active = value;
		s_Data.clear();
	}

	bool Profiler::IsActive() {
		return s_Active;
	}
}
