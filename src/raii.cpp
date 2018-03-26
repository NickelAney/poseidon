// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "raii.hpp"
#include <unistd.h>
#include "log.hpp"
#include "errno.hpp"

namespace Poseidon {

void File_closer::operator()(int fd) const NOEXCEPT {
	if(::close(fd) != 0){
		const AUTO(desc, get_error_desc());
		LOG_POSEIDON_WARNING("::close() failed: ", desc);
	}
}

}
