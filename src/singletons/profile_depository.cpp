// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2018, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "profile_depository.hpp"
#include "main_config.hpp"
#include "../mutex.hpp"
#include "../log.hpp"
#include "../profiler.hpp"

namespace Poseidon {

namespace {
	struct ProfileKey {
		const char *file;
		unsigned long line;
		const char *func;
	};
	struct ProfileCounters {
		unsigned long long samples;
		double total;
		double exclusive;
	};
	struct ProfileKeyComparator {
		bool operator()(const ProfileKey &lhs, const ProfileKey &rhs) const NOEXCEPT {
			int cmp = std::strcmp(lhs.file, rhs.file);
			if(cmp != 0){
				return cmp < 0;
			}
			return lhs.line < rhs.line;
		}
	};
	typedef boost::container::flat_map<ProfileKey, ProfileCounters, ProfileKeyComparator> ProfileMap;

	bool g_enabled = false;

	Mutex g_mutex;
	ProfileMap g_profile;
}

void ProfileDepository::start(){
	LOG_POSEIDON(Logger::special_major | Logger::level_info, "Starting profile depository...");

	g_enabled = MainConfig::get<bool>("profiler_enabled", false);
}
void ProfileDepository::stop(){
	LOG_POSEIDON(Logger::special_major | Logger::level_info, "Stopping profile depository...");

	const Mutex::UniqueLock lock(g_mutex);
	g_profile.clear();
}

bool ProfileDepository::is_enabled() NOEXCEPT {
	return g_enabled;
}

void ProfileDepository::accumulate(const char *file, unsigned long line, const char *func, bool new_sample, double total, double exclusive) NOEXCEPT
try {
	const Mutex::UniqueLock lock(g_mutex);
	const ProfileKey key = { file, line, func };
	AUTO_REF(counters, g_profile[key]);
	counters.samples += new_sample;
	counters.total += total;
	counters.exclusive += exclusive;
} catch(...){
	//
}

void ProfileDepository::snapshot(boost::container::vector<ProfileDepository::SnapshotElement> &ret){
	Profiler::accumulate_all_in_thread();

	const Mutex::UniqueLock lock(g_mutex);
	ret.reserve(ret.size() + g_profile.size());
	for(AUTO(it, g_profile.begin()); it != g_profile.end(); ++it){
		SnapshotElement elem = { };
		elem.file = it->first.file;
		elem.line = it->first.line;
		elem.func = it->first.func;
		elem.samples = it->second.samples;
		elem.total = it->second.total;
		elem.exclusive = it->second.exclusive;
		ret.push_back(STD_MOVE(elem));
	}
}
void ProfileDepository::clear() NOEXCEPT {
	const Mutex::UniqueLock lock(g_mutex);
	g_profile.clear();
}

}
