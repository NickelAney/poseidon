// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2015, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "promise.hpp"
#include "exception.hpp"
#include "../log.hpp"

namespace Poseidon {

namespace {
	typedef Exception BasicException;
}

namespace MySql {
	Promise::Promise()
		: m_satisfied(false)
	{
	}
	Promise::~Promise(){
	}

	bool Promise::isSatisfied() const {
		const Mutex::UniqueLock lock(m_mutex);
		return m_satisfied;
	}
	void Promise::checkAndRethrow() const {
		const Mutex::UniqueLock lock(m_mutex);
		if(!m_satisfied){
			DEBUG_THROW(BasicException, sslit("MySQL promise is not satisfied"));
		}
		if(m_except){
			boost::rethrow_exception(m_except);
		}
	}

	void Promise::setSuccess(){
		const Mutex::UniqueLock lock(m_mutex);
		if(m_satisfied){
			DEBUG_THROW(BasicException, sslit("MySQL promise is already satisfied"));
		}
		m_satisfied = true;
		m_except = boost::exception_ptr();
	}
	void Promise::setException(const boost::exception_ptr &except){
		assert(except);

		const Mutex::UniqueLock lock(m_mutex);
		if(m_satisfied){
			DEBUG_THROW(BasicException, sslit("MySQL promise is already satisfied"));
		}
		m_satisfied = true;
		m_except = except;
	}
}

}