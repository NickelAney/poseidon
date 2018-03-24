// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "config_file.hpp"
#include "profiler.hpp"
#include "log.hpp"
#include "string.hpp"
#include "buffer_streams.hpp"
#include "singletons/filesystem_daemon.hpp"
#include "system_exception.hpp"

namespace Poseidon {

namespace {
	void escape(std::ostream &os, const char *data, std::size_t size){
		PROFILE_ME;

		for(std::size_t i = 0; i < size; ++i){
			const unsigned ch = (unsigned char)data[i];
			switch(ch){
			case '\b':
				os <<'\\' <<'b';
				break;
			case '\f':
				os <<'\\' <<'f';
				break;
			case '\n':
				os <<'\\' <<'n';
				break;
			case '\r':
				os <<'\\' <<'r';
				break;
			case '\t':
				os <<'\\' <<'t';
				break;
			case '\\':
			case ' ':
			case '=':
			case '#':
				os <<'\\' <<(char)ch;
				break;
			default:
				os <<(char)ch;
				break;
			}
		}
	}
	char unescape(std::string &seg, std::istream &is, const char *stops_at){
		PROFILE_ME;

		seg.clear();

		typedef std::istream::traits_type traits;
		traits::int_type next = is.peek();
		bool escaped = false;
		for(; !traits::eq_int_type(next, traits::eof()); next = is.peek()){
			const char ch = traits::to_char_type(is.get());
			if(escaped){
				switch(ch){
				case 'b':
					seg += '\b';
					break;
				case 'f':
					seg += '\f';
					break;
				case 'n':
					seg += '\n';
					break;
				case 'r':
					seg += '\r';
					break;
				case 't':
					seg += '\t';
					break;
				default:
					seg += ch;
					break;
				}
				escaped = false;
			} else if(ch == '\\'){
				escaped = true;
			} else {
				const char *const pos = std::strchr(stops_at, ch);
				if(pos){
					return *pos;
				}
				seg += ch;
			}
		}
		return 0;
	}
}

ConfigFile::ConfigFile()
	: m_contents()
{
	//
}
ConfigFile::ConfigFile(const std::string &path)
	: m_contents()
{
	load(path);
}

bool ConfigFile::empty() const {
	return m_contents.empty();
}
std::size_t ConfigFile::size() const {
	return m_contents.size();
}
void ConfigFile::clear(){
	m_contents.clear();
}

bool ConfigFile::get_raw(std::string &val, const char *key) const {
	PROFILE_ME;

	const AUTO(it, m_contents.find(key));
	if(it == m_contents.end()){
		return false;
	}
	val = it->second;
	return true;
}
const std::string &ConfigFile::get_raw(const char *key) const {
	PROFILE_ME;

	const AUTO(it, m_contents.find(key));
	if(it == m_contents.end()){
		return empty_string();
	}
	return it->second;
}

std::size_t ConfigFile::get_all_raw(boost::container::vector<std::string> &vals, const char *key, bool including_empty) const {
	PROFILE_ME;

	const AUTO(range, m_contents.range(key));
	vals.reserve(vals.size() + static_cast<std::size_t>(std::distance(range.first, range.second)));
	std::size_t total = 0;
	for(AUTO(it, range.first); it != range.second; ++it){
		if(it->second.empty()){
			if(!including_empty){
				continue;
			}
			vals.emplace_back();
		} else {
			vals.emplace_back(it->second);
		}
		++total;
	}
	return total;
}
boost::container::vector<std::string> ConfigFile::get_all_raw(const char *key, bool including_empty) const {
	PROFILE_ME;

	boost::container::vector<std::string> vals;
	get_all_raw(vals, key, including_empty);
	return vals;
}

void ConfigFile::load(const std::string &path){
	PROFILE_ME;
	LOG_POSEIDON(Logger::special_major | Logger::level_info, "Loading config file: ", path);

	AUTO(block, FileSystemDaemon::load(path));
	LOG_POSEIDON(Logger::special_major | Logger::level_info, "Read ", block.size_total, " byte(s) from ", path);

	VALUE_TYPE(m_contents) contents;

	std::size_t line = 0;
	for(;;){
		StreamBuffer buf;
		for(;;){
			const int ch = block.data.get();
			if(ch < 0){
				break;
			}
			if(ch == '\n'){
				break;
			}
			buf.put(ch);
		}
		if(buf.back() == '\r'){
			buf.unput();
		}
		if(buf.empty() && block.data.empty()){
			break;
		}
		++line;

		Buffer_istream is(STD_MOVE(buf));
		std::string key, val;
		const char key_term = unescape(key, is, "=#");
		DEBUG_THROW_UNLESS(is, Exception, sslit("Error parsing escape sequence"));
		key = trim(STD_MOVE(key));
		if(key.empty()){
			continue;
		}
		if(key_term == '='){
			unescape(val, is, "#");
			DEBUG_THROW_UNLESS(is, Exception, sslit("Error parsing escape sequence"));
			val = trim(STD_MOVE(val));
		}
		LOG_POSEIDON(Logger::special_major | Logger::level_debug, "Config: ", std::setw(3), line, " | ", key, " = ", val);
		contents.append(SharedNts(key), STD_MOVE(val));
	}

	m_contents.swap(contents);
}
int ConfigFile::load_nothrow(const std::string &path)
try {
	PROFILE_ME;

	load(path);
	return 0;
} catch(SystemException &e){
	LOG_POSEIDON_ERROR("SystemException thrown while loading config file: path = ", path, ", code = ", e.get_code(), ", what = ", e.what());
	return e.get_code();
} catch(std::exception &e){
	LOG_POSEIDON_ERROR("std::exception thrown while loading config file: path = ", path, ", what = ", e.what());
	return EINVAL;
} catch(...){
	LOG_POSEIDON_ERROR("Unknown exception thrown while loading config file: path = ", path);
	return EINVAL;
}

void ConfigFile::save(const std::string &path){
	PROFILE_ME;
	LOG_POSEIDON(Logger::special_major | Logger::level_info, "Saving config file: ", path);

	Buffer_ostream os;
	for(AUTO(it, m_contents.begin()); it != m_contents.end(); ++it){
		escape(os, it->first.get(), std::strlen(it->first.get()));
		os <<" = ";
		escape(os, it->second.data(), it->second.size());
		os <<std::endl;
	}
	FileSystemDaemon::save(path, STD_MOVE(os.get_buffer()));
}

}
