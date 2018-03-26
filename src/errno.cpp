// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "errno.hpp"

namespace Poseidon {

Shared_nts get_error_desc(int err_code) NOEXCEPT {
	char temp[1024];
	const char *desc = ::strerror_r(err_code, temp, sizeof(temp));
	if(desc == temp){
		try {
			return Shared_nts(desc);
		} catch(...){
			desc = "Insufficient memory.";
		}
	}
	// desc 指向一个静态的字符串。
	return Shared_nts::view(desc);
}
std::string get_error_desc_as_string(int err_code){
	std::string ret;
	ret.resize(1024);
	const char *desc = ::strerror_r(err_code, &ret[0], ret.size());
	if(desc == &ret[0]){
		ret.resize(std::strlen(desc));
	} else {
		ret.assign(desc);
	}
	return ret;
}

}
