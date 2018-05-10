// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "exception.hpp"
#include "log.hpp"

namespace Poseidon {

Exception::Exception(const char *file, std::size_t line, const char *func, Rcnts message)
	: m_file(file), m_line(line), m_func(func), m_message(STD_MOVE(message))
{
	LOG_POSEIDON(Logger::special_major | Logger::level_info, "Constructing Exception: file = ", get_file(), ", line = ", get_line(), ", func = ", get_func(), ", message = ", get_message());
}
Exception::~Exception() NOEXCEPT {
	//
}

const char *Exception::what() const NOEXCEPT {
	return m_message.get();
}

}
