// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "tcp_server_base.hpp"
#include "tcp_session_base.hpp"
#include "ssl_factories.hpp"
#include "ssl_filter.hpp"
#include "sock_addr.hpp"
#include "ip_port.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include "singletons/main_config.hpp"
#include "singletons/epoll_daemon.hpp"
#include "log.hpp"
#include "system_exception.hpp"
#include "profiler.hpp"

namespace Poseidon {

namespace {
#ifdef POSEIDON_CXX11
	UniqueFile
#else
	Move<UniqueFile>
#endif
		create_tcp_socket(const SockAddr &addr)
	{
#ifdef POSEIDON_CXX11
		UniqueFile tcp;
#else
		static __thread UniqueFile tcp;
#endif
		DEBUG_THROW_UNLESS(tcp.reset(::socket(addr.get_family(), SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP)), SystemException);
		static CONSTEXPR const int TRUE_VALUE = true;
		DEBUG_THROW_UNLESS(::setsockopt(tcp.get(), SOL_SOCKET, SO_REUSEADDR, &TRUE_VALUE, sizeof(TRUE_VALUE)) == 0, SystemException);
		DEBUG_THROW_UNLESS(::bind(tcp.get(), static_cast<const ::sockaddr *>(addr.data()), static_cast<unsigned>(addr.size())) == 0, SystemException);
		DEBUG_THROW_UNLESS(::listen(tcp.get(), SOMAXCONN) == 0, SystemException);
		return STD_MOVE(tcp);
	}
}

TcpServerBase::TcpServerBase(const SockAddr &addr, const char *certificate, const char *private_key)
	: SocketBase(create_tcp_socket(addr))
{
	if(certificate && *certificate){
		m_ssl_factory.reset(new SslServerFactory(certificate, private_key));
	}

	LOG_POSEIDON(Logger::SP_MAJOR | Logger::LV_INFO, "Created TCP server on ", get_local_info(), ", SSL = ", !!m_ssl_factory);
}
TcpServerBase::~TcpServerBase(){
	LOG_POSEIDON(Logger::SP_MAJOR | Logger::LV_INFO, "Destroyed TCP server on ", get_local_info(), ", SSL = ", !!m_ssl_factory);
}

int TcpServerBase::poll_read_and_process(unsigned char *hint_buffer, std::size_t hint_capacity, bool readable){
	PROFILE_ME;

	(void)hint_buffer;
	(void)hint_capacity;
	(void)readable;

	for(unsigned i = 0; i < 16; ++i){
		boost::shared_ptr<TcpSessionBase> session;
		try {
			UniqueFile client;
			if(!client.reset(::accept(get_fd(), NULLPTR, NULLPTR))){
				return errno;
			}
			session = on_client_connect(STD_MOVE(client));
			if(!session){
				LOG_POSEIDON_WARNING("on_client_connect() returns a null pointer.");
				return EWOULDBLOCK;
			}
		} catch(std::exception &e){
			LOG_POSEIDON_ERROR("std::exception thrown: what = ", e.what());
			return EINTR;
		} catch(...){
			LOG_POSEIDON_ERROR("Unknown exception thrown.");
			return EINTR;
		}
		try {
			if(m_ssl_factory){
				boost::scoped_ptr<SslFilter> ssl_filter;
				m_ssl_factory->create_ssl_filter(ssl_filter, session->get_fd());
				session->init_ssl(ssl_filter);
			}
			const AUTO(tcp_request_timeout, MainConfig::get<boost::uint64_t>("tcp_request_timeout", 5000));
			session->set_timeout(tcp_request_timeout);
			EpollDaemon::add_socket(session, true);
			LOG_POSEIDON_INFO("Accepted TCP connection from ", session->get_remote_info());
		} catch(std::exception &e){
			LOG_POSEIDON_ERROR("std::exception thrown: what = ", e.what());
			session->force_shutdown();
			continue;
		}
	}
	return 0;
}

}
