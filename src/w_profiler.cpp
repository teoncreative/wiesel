//
// Created by Metehan Gezer on 21.03.2023.
//


#include <sstream>
#include "w_profiler.h"

namespace wie {
	bool enabled = false;
	std::vector<ProfileData> data;
	std::string currentSection;

	ProfilerInstance::ProfilerInstance(const std::string& name) : ProfilerInstance(name, -1){
	}

	ProfilerInstance::ProfilerInstance(const std::string& name, int renderPass) {
		if (!enabled) {
			return;
		}
		this->name = name;
		this->startTime = std::chrono::steady_clock::now();
		this->renderPass = renderPass;
	}

	ProfilerInstance::~ProfilerInstance() {
		if (!enabled) {
			return;
		}
		auto now = std::chrono::steady_clock::now();
		auto elapsed = now - this->startTime;
		auto elapsedMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
		ProfileData profileData;
		profileData.name = name;
		profileData.time = elapsedMicroseconds;
		profileData.renderPass = renderPass;
		wie::insertProfileData(profileData);
	}

	void beginProfiler(const std::string& sectionName) {
		data.clear();
		currentSection = sectionName;
	}

	void endProfiler(std::ostream& out) {
		if (!enabled || data.empty()) {
			return;
		}
		long long sum = 0;
		out << "[PROFILER] Section: " << currentSection << "\n";
		for (auto & v : data) {
			out << "[PROFILER] " << v.name;
			if (v.renderPass >= 0) {
				out << "(pass " << v.renderPass << ")";
			}
			long long count = v.time.count();
			out << ": "  << count << "us.\n";
			sum += count;
		}
		out << "[PROFILER] Total Frame Time: " << sum << "us\n";
	}

	void insertProfileData(const ProfileData& profileData) {
		data.push_back(profileData);
	}

	void setProfilerEnabled(bool value) {
		enabled = value;
		data.clear();
	}
}
