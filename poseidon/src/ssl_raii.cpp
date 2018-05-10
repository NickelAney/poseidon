// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "ssl_raii.hpp"

namespace Poseidon {

void Ssl_deleter::operator()(::SSL *ssl) NOEXCEPT {
	::SSL_free(ssl);
}

void Ssl_ctx_deleter::operator()(::SSL_CTX *ssl_ctx) NOEXCEPT {
	::SSL_CTX_free(ssl_ctx);
}

}
