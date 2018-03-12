// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2018, LH_Mouse. All wrongs reserved.

#ifndef POSEIDON_SYSTEM_HTTP_SERVER_HPP_
#define POSEIDON_SYSTEM_HTTP_SERVER_HPP_

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

namespace Poseidon {

class SystemHttpServletBase;

class SystemHttpServer {
private:
	SystemHttpServer();

public:
	static void start();
	static void stop();

	static boost::shared_ptr<const SystemHttpServletBase> get_servlet(const char *uri);
	static void get_all_servlets(boost::container::vector<boost::shared_ptr<const SystemHttpServletBase> > &ret);

	// 返回的 shared_ptr 是该处理程序的唯一持有者。
	static boost::shared_ptr<const SystemHttpServletBase> register_servlet(boost::shared_ptr<SystemHttpServletBase> servlet);
};

}

#endif
