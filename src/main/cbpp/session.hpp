// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2015, LH_Mouse. All wrongs reserved.

#ifndef POSEIDON_CBPP_SESSION_HPP_
#define POSEIDON_CBPP_SESSION_HPP_

#include <cstddef>
#include <boost/type_traits/is_base_of.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/cstdint.hpp>
#include "../tcp_session_base.hpp"
#include "../stream_buffer.hpp"
#include "status_codes.hpp"

namespace Poseidon {

namespace Cbpp {
	class MessageBase;

	class Session : public TcpSessionBase {
	private:
		class RequestJob;
		class ErrorJob;

	private:
		boost::uint64_t m_payloadLen;
		unsigned m_messageId;
		StreamBuffer m_payload;

	public:
		explicit Session(UniqueFile socket);
		~Session();

	private:
		void onReadAvail(const void *data, std::size_t size) OVERRIDE FINAL;

	protected:
		virtual void onRequest(boost::uint16_t messageId, const StreamBuffer &contents) = 0;

	public:
		bool send(boost::uint16_t messageId, StreamBuffer contents, bool fin = false);

		template<class MessageT>
		typename boost::enable_if<boost::is_base_of<MessageBase, MessageT>, bool>::type
			send(const MessageT &contents, bool fin = false)
		{
			return send(MessageT::ID, StreamBuffer(contents), fin);
		}

		bool sendError(boost::uint16_t messageId, StatusCode statusCode, std::string reason, bool fin = false);
		bool sendError(boost::uint16_t messageId, StatusCode statusCode, bool fin = false){
			return sendError(messageId, statusCode, std::string(), fin);
		}
	};
}

}

#endif
