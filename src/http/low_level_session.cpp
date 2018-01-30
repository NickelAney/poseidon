// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2018, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "low_level_session.hpp"
#include "exception.hpp"
#include "upgraded_session_base.hpp"
#include "header_option.hpp"
#include "../log.hpp"
#include "../profiler.hpp"
#include "../stream_buffer.hpp"

namespace Poseidon {
namespace Http {

LowLevelSession::LowLevelSession(Move<UniqueFile> socket)
	: TcpSessionBase(STD_MOVE(socket)), ServerReader(), ServerWriter()
{
	//
}
LowLevelSession::~LowLevelSession(){
	//
}

void LowLevelSession::on_connect(){
	PROFILE_ME;

	//
}
void LowLevelSession::on_read_hup(){
	PROFILE_ME;

	// epoll 线程读取不需要锁。
	const AUTO(upgraded_session, m_upgraded_session);
	if(upgraded_session){
		upgraded_session->on_read_hup();
	}
}
void LowLevelSession::on_close(int err_code){
	PROFILE_ME;

	// epoll 线程读取不需要锁。
	const AUTO(upgraded_session, m_upgraded_session);
	if(upgraded_session){
		upgraded_session->on_close(err_code);
	}
}
void LowLevelSession::on_receive(StreamBuffer data){
	PROFILE_ME;

	// epoll 线程读取不需要锁。
	AUTO(upgraded_session, m_upgraded_session);
	if(upgraded_session){
		upgraded_session->on_receive(STD_MOVE(data));
		return;
	}

	ServerReader::put_encoded_data(STD_MOVE(data));

	upgraded_session = m_upgraded_session;
	if(upgraded_session){
		upgraded_session->on_connect();

		StreamBuffer queue;
		queue.swap(ServerReader::get_queue());
		if(!queue.empty()){
			upgraded_session->on_receive(STD_MOVE(queue));
		}
	}
}

void LowLevelSession::on_shutdown_timer(boost::uint64_t now){
	PROFILE_ME;

	// timer 线程读取需要锁。
	const AUTO(upgraded_session, get_upgraded_session());
	if(upgraded_session){
		upgraded_session->on_shutdown_timer(now);
	}

	TcpSessionBase::on_shutdown_timer(now);
}

void LowLevelSession::on_request_headers(RequestHeaders request_headers, boost::uint64_t content_length){
	PROFILE_ME;

	on_low_level_request_headers(STD_MOVE(request_headers), content_length);
}
void LowLevelSession::on_request_entity(boost::uint64_t entity_offset, StreamBuffer entity){
	PROFILE_ME;

	on_low_level_request_entity(entity_offset, STD_MOVE(entity));
}
bool LowLevelSession::on_request_end(boost::uint64_t content_length, OptionalMap headers){
	PROFILE_ME;

	AUTO(upgraded_session, on_low_level_request_end(content_length, STD_MOVE(headers)));
	if(upgraded_session){
		const Mutex::UniqueLock lock(m_upgraded_session_mutex);
		m_upgraded_session = STD_MOVE(upgraded_session);
		return false;
	}
	return true;
}

long LowLevelSession::on_encoded_data_avail(StreamBuffer encoded){
	PROFILE_ME;

	return TcpSessionBase::send(STD_MOVE(encoded));
}

boost::shared_ptr<UpgradedSessionBase> LowLevelSession::get_upgraded_session() const {
	const Mutex::UniqueLock lock(m_upgraded_session_mutex);
	return m_upgraded_session;
}

bool LowLevelSession::send(ResponseHeaders response_headers, StreamBuffer entity){
	PROFILE_ME;

	return ServerWriter::put_response(STD_MOVE(response_headers), STD_MOVE(entity), true);
}
bool LowLevelSession::send(StatusCode status_code){
	PROFILE_ME;

	return send(status_code, OptionalMap(), StreamBuffer());
}
bool LowLevelSession::send(StatusCode status_code, StreamBuffer entity, const HeaderOption &content_type){
	PROFILE_ME;

	OptionalMap headers;
	headers.set(sslit("Content-Type"), content_type.dump().dump_string());
	return send(status_code, STD_MOVE(headers), STD_MOVE(entity));
}
bool LowLevelSession::send(StatusCode status_code, OptionalMap headers, StreamBuffer entity){
	PROFILE_ME;

	ResponseHeaders response_headers;
	response_headers.version = 10001;
	response_headers.status_code = status_code;
	response_headers.reason = get_status_code_desc(status_code).desc_short;
	response_headers.headers = STD_MOVE(headers);
	return send(STD_MOVE(response_headers), STD_MOVE(entity));
}

bool LowLevelSession::send_chunked_header(ResponseHeaders response_headers){
	PROFILE_ME;

	return ServerWriter::put_chunked_header(STD_MOVE(response_headers));
}
bool LowLevelSession::send_chunk(StreamBuffer entity){
	PROFILE_ME;

	return ServerWriter::put_chunk(STD_MOVE(entity));
}
bool LowLevelSession::send_chunked_trailer(OptionalMap headers){
	PROFILE_ME;

	return ServerWriter::put_chunked_trailer(STD_MOVE(headers));
}

bool LowLevelSession::send_default(StatusCode status_code, OptionalMap headers){
	PROFILE_ME;

	AUTO(pair, make_default_response(status_code, STD_MOVE(headers)));
	return ServerWriter::put_response(pair.first, STD_MOVE(pair.second), false); // no need to adjust Content-Length.
}
bool LowLevelSession::send_default_and_shutdown(StatusCode status_code, const OptionalMap &headers) NOEXCEPT
try {
	PROFILE_ME;

	AUTO(pair, make_default_response(status_code, headers));
	pair.first.headers.set(sslit("Connection"), "Close");
	ServerWriter::put_response(pair.first, STD_MOVE(pair.second), false); // no need to adjust Content-Length.
	shutdown_read();
	return shutdown_write();
} catch(std::exception &e){
	LOG_POSEIDON_ERROR("std::exception thrown: what = ", e.what());
	force_shutdown();
	return false;
} catch(...){
	LOG_POSEIDON_ERROR("Unknown exception thrown.");
	force_shutdown();
	return false;
}
bool LowLevelSession::send_default_and_shutdown(StatusCode status_code, Move<OptionalMap> headers) NOEXCEPT
try {
	PROFILE_ME;

	if(has_been_shutdown_write()){
		return false;
	}
	AUTO(pair, make_default_response(status_code, STD_MOVE(headers)));
	pair.first.headers.set(sslit("Connection"), "Close");
	ServerWriter::put_response(pair.first, STD_MOVE(pair.second), false); // no need to adjust Content-Length.
	shutdown_read();
	return shutdown_write();
} catch(std::exception &e){
	LOG_POSEIDON_ERROR("std::exception thrown: what = ", e.what());
	force_shutdown();
	return false;
} catch(...){
	LOG_POSEIDON_ERROR("Unknown exception thrown.");
	force_shutdown();
	return false;
}

}
}
