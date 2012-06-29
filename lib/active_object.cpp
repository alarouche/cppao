#include "active_object.hpp"
#include <iostream>

#define ACTIVE_OBJECT_CONDITION 0	// !! Slow


active::scheduler active::default_scheduler;

active::scheduler::scheduler() : m_head(nullptr), m_tail(nullptr), m_busy_count(0)
{
}

active::any_object::~any_object()
{
}


// Used by an active object to signal that there are messages to process.
void active::scheduler::activate(ObjectPtr p) throw()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	p->m_next=nullptr;
	if( !m_head )
		m_head = m_tail = p;
	else
		m_tail->m_next = p, m_tail=p;
	++m_busy_count;
#if ACTIVE_OBJECT_CONDITION
	m_ready.notify_one();
#endif
}


// Runs until there are no more messages in the entire pool.
// Returns false if no more items.
bool active::scheduler::run_managed() throw()
{
	std::unique_lock<std::mutex> lock(m_mutex);

	while( m_head )
	{
		ObjectPtr p = m_head;
		m_head = m_head->m_next;
		lock.unlock();
		p->run_some();
		lock.lock();
		--m_busy_count;
	}
	m_tail=nullptr;

	// Can be non-zero if the queues are empty, but other threads are processing.
	// the result of processing could be to add more signalled objects.
	return m_busy_count!=0;
}

active::run::run(int threads, scheduler & sched) : 
	m_scheduler(sched),
	m_threads(threads<1 ? 4 : threads)
{
	m_scheduler.start_work();	// Prevent threads from exiting immediately
	for( auto & t : m_threads )
        t = std::thread( std::bind(&scheduler::run, &sched) );
}

active::run::~run()
{
	m_scheduler.stop_work();
	for( auto & t : m_threads )
		t.join();
}

void active::scheduler::run()
{
	while( run_managed() )
	{
		std::unique_lock<std::mutex> lock(m_mutex);
#if ACTIVE_OBJECT_CONDITION
		m_ready.wait(lock);
#else
		m_ready.wait_for(lock, std::chrono::milliseconds(50) );
#endif
	}
	std::unique_lock<std::mutex> lock(m_mutex);	// Needed on VS11
	m_ready.notify_all();
}

void active::scheduler::run_in_thread()
{
	run();
}


void active::any_object::exception_handler() throw()
{
	try
	{
		throw;
	}
	catch( std::exception & ex )
	{
		std::cerr << "Unprocessed exception during message processing: " << ex.what() << std::endl;
	}
	catch( ... )
	{
		std::cerr << "Unprocessed exception during message processing" << std::endl;
	}
}

void active::scheduler::start_work() throw()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	++m_busy_count;
}

void active::scheduler::stop_work() throw()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if( 0 == --m_busy_count )
	{
		m_ready.notify_one();
	}
}

active::schedule::thread_pool::thread_pool(type & p) : m_pool(&p)
{
}

void active::schedule::thread_pool::set_scheduler(type&p)
{
	m_pool = &p;
}

void active::schedule::thread_pool::activate(const std::shared_ptr<any_object> & sp)
{
	if(m_pool) m_pool->activate(sp.get());
}

void active::schedule::thread_pool::activate(any_object * obj)
{
	if(m_pool) m_pool->activate(obj);
}

void active::schedule::own_thread::activate(any_object * obj)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_object = obj;
	if( m_pool ) m_pool->start_work();
	m_ready.notify_one();
}

void active::schedule::own_thread::activate(const std::shared_ptr<any_object> & sp)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_shared = sp;
	m_object = sp.get();
	if( m_pool ) m_pool->start_work();
	m_ready.notify_one();
}

void active::schedule::own_thread::thread_fn()
{
	std::unique_lock<std::mutex> lock(m_mutex);
	while( !m_shutdown )
	{
		if( m_object )
		{
			any_object * obj = m_object;
			m_object = nullptr;
			lock.unlock();
			obj->run();
			lock.lock();
			if(m_pool) m_pool->stop_work();
			if( m_shared )
			{
				std::weak_ptr<any_object> wp(m_shared);
				std::shared_ptr<any_object> shared;
				shared.swap(m_shared);
				lock.unlock();
				shared.reset();
				if( wp.expired() ) return;	// Object destroyed
				lock.lock();
			}
		}
		if( !m_object && !m_shutdown )
			m_ready.wait(lock);
	}
}

active::schedule::own_thread::own_thread(const own_thread&other) :
	m_pool(other.m_pool),
	m_shutdown(false),
	m_object(nullptr),
	m_thread( std::bind( &own_thread::thread_fn, this ) )
{
}

active::schedule::own_thread::own_thread(type & p) :
	m_pool(&p),
	m_shutdown(false),
	m_object(nullptr),
	m_thread( std::bind( &own_thread::thread_fn, this ) )
{
}

active::schedule::own_thread::~own_thread()
{
	if( std::this_thread::get_id() == m_thread.get_id())
	{
		m_thread.detach();	// Destroyed from within thread
	}
	else
	{
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_shutdown = true;
			m_ready.notify_one();
		}
		m_thread.join();	// Destroyed outside of thread
	}
}

active::queueing::direct_call::direct_call(const allocator_type&)
{
}

bool active::queueing::direct_call::run_some(any_object * o, int n) throw()
{
	return false;
}

bool active::queueing::direct_call::empty() const
{
	return true;
}

active::queueing::mutexed_call::mutexed_call(const allocator_type&)
{
}

active::queueing::mutexed_call::mutexed_call(const mutexed_call&)
{
}

bool active::queueing::mutexed_call::run_some(any_object * o, int n) throw()
{
	return false;
}

bool active::queueing::mutexed_call::empty() const
{
	return true;
}

