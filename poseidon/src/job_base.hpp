// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2018, LH_Mouse. All wrongs reserved.

#ifndef POSEIDON_JOB_BASE_HPP_
#define POSEIDON_JOB_BASE_HPP_

#include "cxx_util.hpp"
#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>

namespace Poseidon {

class Promise;

class Job_base : NONCOPYABLE {
public:
	virtual ~Job_base();

public:
	// 如果一个任务被推迟执行且 Category 非空，
	// 则所有具有相同 Category 的后续任务都会被推迟，以维持其相对顺序。
	virtual boost::weak_ptr<const void> get_category() const = 0;
	virtual void perform() = 0;
};

extern void enqueue(boost::shared_ptr<Job_base> job, boost::shared_ptr<const bool> withdrawn = boost::shared_ptr<const bool>());

}

#endif
