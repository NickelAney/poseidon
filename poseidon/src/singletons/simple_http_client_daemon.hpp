// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2018, LH_Mouse. All wrongs reserved.

#ifndef POSEIDON_SINGLETONS_SIMPLE_HTTP_CLIENT_DAEMON_HPP_
#define POSEIDON_SINGLETONS_SIMPLE_HTTP_CLIENT_DAEMON_HPP_

#include "../http/request_headers.hpp"
#include "../http/response_headers.hpp"
#include "../stream_buffer.hpp"
#include "../promise.hpp"

namespace Poseidon {

struct Simple_http_request {
	// 设置 request_headers.uri 为 *完整的* 请求 URI，例如 http://example.com/，可以附带 GET 参数。
	Http::Request_headers request_headers;
	Stream_buffer request_entity;
	// HEAD 请求 *总是不会* 被重定向。
	// GET 请求 *默认会* 被重定向。设置 dont_redirect_get 禁用之。
	// 其它谓词的请求 *默认不会* 被重定向。设置 redirect_non_get 启用之。注意：启用重定向会造成多余的内存分配。
	bool dont_redirect_get;
	bool redirect_non_get;
};

struct Simple_http_response {
	Http::Response_headers response_headers;
	Stream_buffer response_entity;
};

extern template class Promise_container<Simple_http_response>;

class Simple_http_client_daemon {
private:
	Simple_http_client_daemon();

public:
	static void start();
	static void stop();

	// 同步接口。
	static Simple_http_response perform(Simple_http_request request);

	// 异步接口。
	static boost::shared_ptr<const Promise_container<Simple_http_response> > enqueue_for_performing(Simple_http_request request);
};

}

#endif
