#include "../precompiled.hpp"
#include "mysql_daemon.hpp"
#include <list>
#include <boost/thread.hpp>
#include <boost/scoped_ptr.hpp>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <mysql/mysql.h>
#include "main_config.hpp"
#include "../mysql/object_base.hpp"
#define POSEIDON_MYSQL_OBJECT_IMPL_
#include "../mysql/object_impl.hpp"
#include "../atomic.hpp"
#include "../exception.hpp"
#include "../log.hpp"
#include "../job_base.hpp"
#include "../profiler.hpp"
#include "../utilities.hpp"
using namespace Poseidon;

namespace {

std::string g_databaseServer			= "tcp://localhost:3306";
std::string g_databaseUsername			= "root";
std::string g_databasePassword			= "root";
std::string g_databaseName				= "test";

std::size_t g_databaseSaveDelay			= 5000;
std::size_t g_databaseMaxReconnDelay	= 60000;

class AsyncLoadCallbackJob : public JobBase {
private:
	MySqlAsyncLoadCallback m_callback;
	boost::shared_ptr<MySqlObjectBase> m_object;

public:
	explicit AsyncLoadCallbackJob(Move<MySqlAsyncLoadCallback> callback,
		boost::shared_ptr<MySqlObjectBase> object)
	{
		callback.swap(m_callback);
		object.swap(m_object);
	}

protected:
	void perform(){
		PROFILE_ME;

		m_callback(STD_MOVE(m_object));
	}
};

struct AsyncSaveItem {
	boost::shared_ptr<const MySqlObjectBase> object;
	unsigned long long timeStamp;

	void swap(AsyncSaveItem &rhs) NOEXCEPT {
		object.swap(rhs.object);
		std::swap(timeStamp, rhs.timeStamp);
	}
};

struct AsyncLoadItem {
	boost::shared_ptr<MySqlObjectBase> object;
	std::string filter;
	MySqlAsyncLoadCallback callback;

	void swap(AsyncLoadItem &rhs) NOEXCEPT {
		object.swap(rhs.object);
		filter.swap(rhs.filter);
		callback.swap(rhs.callback);
	}
};

volatile bool g_running = false;
boost::thread g_thread;

boost::mutex g_mutex;
std::list<AsyncSaveItem> g_saveQueue;
std::list<AsyncSaveItem> g_savePool;
std::list<AsyncLoadItem> g_loadQueue;
std::list<AsyncLoadItem> g_loadPool;
boost::condition_variable g_newObjectAvail;

bool g_connected = false;
volatile std::size_t g_waiting = 0;
boost::condition_variable g_queueEmpty;

bool getMySqlConnection(boost::scoped_ptr<sql::Connection> &connection){
	LOG_POSEIDON_INFO("Connecting to MySQL server...");
	try {
		connection.reset(::get_driver_instance()->connect(
			g_databaseServer, g_databaseUsername, g_databasePassword));
		connection->setSchema(g_databaseName);
		LOG_POSEIDON_INFO("Successfully connected to MySQL server.");
		return true;
	} catch(sql::SQLException &e){
		LOG_POSEIDON_ERROR("Error connecting to MySQL server: code = ", e.getErrorCode(),
			", state = ", e.getSQLState(), ", what = ", e.what());
		return false;
	}
}

void daemonLoop(){
	boost::scoped_ptr<sql::Connection> connection;
	if(!getMySqlConnection(connection)){
		LOG_POSEIDON_FATAL("Failed to connect MySQL server. Bailing out.");
		std::abort();
	} else {
		const boost::mutex::scoped_lock lock(g_mutex);
		g_connected = true;
		g_queueEmpty.notify_all();
	}

	std::size_t reconnectDelay = 0;
	for(;;){
		bool discardConnection = false;

		try {
			if(!connection){
				LOG_POSEIDON_WARN("Lost connection to MySQL server. Reconnecting...");

				if(reconnectDelay == 0){
					reconnectDelay = 1;
				} else {
					LOG_POSEIDON_INFO("Will retry after ", reconnectDelay, " milliseconds.");

					boost::mutex::scoped_lock lock(g_mutex);
					g_newObjectAvail.timed_wait(lock, boost::posix_time::milliseconds(reconnectDelay));

					reconnectDelay <<= 1;
					if(reconnectDelay > g_databaseMaxReconnDelay){
						reconnectDelay = g_databaseMaxReconnDelay;
					}
				}
				if(!getMySqlConnection(connection)){
					if(!atomicLoad(g_running)){
						LOG_POSEIDON_WARN("Shutting down...");
						break;
					}
					continue;
				}
				reconnectDelay = 0;
			}

			AsyncSaveItem asi;
			AsyncLoadItem ali;
			{
				boost::mutex::scoped_lock lock(g_mutex);
				for(;;){
					bool empty = true;
					if(!g_saveQueue.empty()){
						empty = false;
						AUTO_REF(head, g_saveQueue.front());
						if((atomicLoad(g_waiting) == 0) && (head.timeStamp > getMonoClock())){
							goto skip;
						}
						if(MySqlObjectImpl::getContext(*head.object) != &head){
							AsyncSaveItem().swap(head);
						} else {
							asi.swap(head);
						}
						g_savePool.splice(g_savePool.begin(), g_saveQueue, g_saveQueue.begin());
						if(!asi.object){
							goto skip;
						}
						break;
					}
				skip:
					if(!g_loadQueue.empty()){
						empty = false;
						ali.swap(g_loadQueue.front());
						g_loadPool.splice(g_loadPool.begin(), g_loadQueue, g_loadQueue.begin());
						break;
					}
					if(empty){
						g_queueEmpty.notify_all();
						if(!atomicLoad(g_running)){
							break;
						}
					}
					g_newObjectAvail.timed_wait(lock, boost::posix_time::seconds(1));
				}
			}
			if(!asi.object && !ali.object){
				break;
			}
			if(asi.object){
				asi.object->syncSave(connection.get());
				asi.object->enableAutoSaving();
			}
			if(ali.object){
				ali.object->syncLoad(connection.get(), ali.filter.c_str());
				ali.object->enableAutoSaving();

				boost::make_shared<AsyncLoadCallbackJob>(
					STD_MOVE(ali.callback), STD_MOVE(ali.object)
					)->pend();
			}
		} catch(sql::SQLException &e){
			LOG_POSEIDON_ERROR("SQLException thrown in MySQL daemon: code = ", e.getErrorCode(),
				", state = ", e.getSQLState(), ", what = ", e.what());
			discardConnection = true;
		} catch(std::exception &e){
			LOG_POSEIDON_ERROR("std::exception thrown in MySQL daemon: what = ", e.what());
			discardConnection = true;
		} catch(...){
			LOG_POSEIDON_ERROR("Unknown exception thrown in MySQL daemon.");
			discardConnection = true;
		}

		if(discardConnection && connection){
			LOG_POSEIDON_INFO("The connection was left in an indeterminate state. Discard it.");
			connection.reset();
		}
	}

	if(!g_saveQueue.empty()){
		LOG_POSEIDON_FATAL("There are still ", g_saveQueue.size(), " object(s) in MySQL save queue");
		g_saveQueue.clear();
	}
	g_loadQueue.clear();
}

void threadProc(){
	PROFILE_ME;
	Logger::setThreadTag(" D  "); // Database
	LOG_POSEIDON_INFO("MySQL daemon started.");

	daemonLoop();
	::mysql_thread_end();

	LOG_POSEIDON_INFO("MySQL daemon stopped.");
}

}

void MySqlDaemon::start(){
	if(atomicExchange(g_running, true) != false){
		LOG_POSEIDON_FATAL("Only one daemon is allowed at the same time.");
		std::abort();
	}
	LOG_POSEIDON_INFO("Starting MySQL daemon...");

	AUTO_REF(conf, MainConfig::getConfigFile());

	conf.get(g_databaseServer, "database_server");
	LOG_POSEIDON_DEBUG("MySQL server = ", g_databaseServer);

	conf.get(g_databaseUsername, "database_username");
	LOG_POSEIDON_DEBUG("MySQL username = ", g_databaseUsername);

	conf.get(g_databasePassword, "database_password");
	LOG_POSEIDON_DEBUG("MySQL password = ", g_databasePassword);

	conf.get(g_databaseName, "database_name");
	LOG_POSEIDON_DEBUG("MySQL database name = ", g_databaseName);

	conf.get(g_databaseSaveDelay, "database_save_delay");
	LOG_POSEIDON_DEBUG("MySQL save delay = ", g_databaseSaveDelay);

	conf.get(g_databaseMaxReconnDelay, "database_max_reconn_delay");
	LOG_POSEIDON_DEBUG("MySQL max reconnect delay = ", g_databaseMaxReconnDelay);

	boost::thread(threadProc).swap(g_thread);
}
void MySqlDaemon::stop(){
	LOG_POSEIDON_INFO("Stopping MySQL daemon...");

	atomicStore(g_running, false);
	{
		const boost::mutex::scoped_lock lock(g_mutex);
		g_newObjectAvail.notify_all();
	}
	waitForAllAsyncOperations();
	if(g_thread.joinable()){
		g_thread.join();
	}
}

void MySqlDaemon::waitForAllAsyncOperations(){
	atomicAdd(g_waiting, 1);
	try {
		LOG_POSEIDON_INFO("Waiting for all MySQL operations to complete...");

		boost::mutex::scoped_lock lock(g_mutex);
		g_newObjectAvail.notify_all();
		while(!g_connected || !(g_saveQueue.empty() && g_loadQueue.empty())){
			g_queueEmpty.wait(lock);
		}
	} catch(...){
		LOG_POSEIDON_ERROR("Interrupted by exception.");
	}
	atomicSub(g_waiting, 1);
}

void MySqlDaemon::pendForSaving(boost::shared_ptr<const MySqlObjectBase> object){
	const boost::mutex::scoped_lock lock(g_mutex);
	if(g_savePool.empty()){
		g_savePool.push_front(AsyncSaveItem());
	}
	g_saveQueue.splice(g_saveQueue.end(), g_savePool, g_savePool.begin());

	AUTO_REF(asi, g_saveQueue.back());
	asi.object.swap(object);
	asi.timeStamp = getMonoClock() + g_databaseSaveDelay * 1000;
	MySqlObjectImpl::setContext(*asi.object, &asi);

	g_newObjectAvail.notify_all();
}
void MySqlDaemon::pendForLoading(boost::shared_ptr<MySqlObjectBase> object,
	std::string filter, MySqlAsyncLoadCallback callback)
{
	const boost::mutex::scoped_lock lock(g_mutex);
	if(g_loadPool.empty()){
		g_loadPool.push_front(AsyncLoadItem());
	}
	g_loadQueue.splice(g_loadQueue.end(), g_loadPool, g_loadPool.begin());

	AUTO_REF(ali, g_loadQueue.back());
	ali.object.swap(object);
	ali.filter.swap(filter);
	ali.callback.swap(callback);

	g_newObjectAvail.notify_all();
}
